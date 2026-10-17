#include "PresetBuilder.hpp"

#include "SharedSources.hpp"

#include <voltz/core/preset-layout.hpp>
#include <voltz/core/live-protocol.hpp>

#include <OBSApp.hpp>
#include <utility/platform.hpp>

#include <obs-frontend-api.h>

#include <QString>

#include <cstring>
#include <utility>
#include <vector>

#include <string>

namespace voltz {

namespace {

CanvasSize canvasSize()
{
	obs_video_info ovi = {};
	if (!obs_get_video_info(&ovi) || !ovi.base_width || !ovi.base_height) {
		return {};
	}
	return {double(ovi.base_width), double(ovi.base_height)};
}

void placeItem(obs_sceneitem_t *item, const LayoutRect &rect, obs_bounds_type bounds, bool cropToBounds)
{
	obs_transform_info info = {};
	vec2_set(&info.pos, float(rect.x), float(rect.y));
	info.rot = 0.0f;
	vec2_set(&info.scale, 1.0f, 1.0f);
	info.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	info.bounds_type = bounds;
	info.bounds_alignment = OBS_ALIGN_CENTER;
	vec2_set(&info.bounds, float(rect.w), float(rect.h));
	info.crop_to_bounds = cropToBounds;
	obs_sceneitem_set_info2(item, &info);
}

// Cena existente com esse nome (não recria) ou nova. `created` diz se foi criada agora.
OBSScene sceneNamed(const QString &name, bool &created)
{
	created = false;
	OBSSourceAutoRelease existing = obs_get_source_by_name(name.toUtf8().constData());
	if (existing) {
		if (obs_scene_t *scene = obs_scene_from_source(existing)) {
			return OBSScene(scene);
		}
	}
	OBSSceneAutoRelease scene = obs_scene_create(name.toUtf8().constData());
	created = true;
	return OBSScene(scene.Get());
}

obs_sceneitem_t *addFull(obs_scene_t *scene, obs_source_t *source, CanvasSize canvas, obs_bounds_type bounds, bool crop)
{
	obs_sceneitem_t *item = obs_scene_add(scene, source);
	if (item) {
		placeItem(item, fullCanvas(canvas), bounds, crop);
	}
	return item;
}

// Configurações privadas (salvas na coleção): na Tela, se a migração já rodou; no grupo, a versão das camadas
// (1 = Voltz Mouse; o Plano 4B sobe para 2 ao acrescentar a Voltz Anotações).
constexpr const char *kScreenMigratedKey = "voltz_tela_migrada";
constexpr const char *kScreenLayersKey = "voltz_tela_camadas";
constexpr int kScreenLayersVersion = 1;

// Nome livre a partir de `base` ("Tela Voltz", "Tela Voltz 2", …), no formato da deduplicação do libobs.
QString uniqueSourceName(const QString &base)
{
	QString name = base;
	for (int suffix = 2;; suffix++) {
		OBSSourceAutoRelease existing = obs_get_source_by_name(name.toUtf8().constData());
		if (!existing) {
			return name;
		}
		name = QStringLiteral("%1 %2").arg(base).arg(suffix);
	}
}

// Camada dentro da Tela Voltz: no canto (0,0), escala 1, sem caixa (quem escala e posiciona é o grupo).
void placeLayer(obs_sceneitem_t *item)
{
	obs_transform_info info = {};
	vec2_set(&info.pos, 0.0f, 0.0f);
	info.rot = 0.0f;
	vec2_set(&info.scale, 1.0f, 1.0f);
	info.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	info.bounds_type = OBS_BOUNDS_NONE;
	info.bounds_alignment = OBS_ALIGN_CENTER;
	vec2_set(&info.bounds, 0.0f, 0.0f);
	info.crop_to_bounds = false;
	obs_sceneitem_set_info2(item, &info);
}

void syncMouseTarget(obs_source_t *mouse, obs_source_t *screen)
{
	const char *uuid = obs_source_get_uuid(screen);
	OBSDataAutoRelease settings = obs_source_get_settings(mouse);
	if (strcmp(obs_data_get_string(settings, live::kTargetSetting), uuid) == 0) {
		return;
	}
	OBSDataAutoRelease update = obs_data_create();
	obs_data_set_string(update, live::kTargetSetting, uuid);
	obs_source_update(mouse, update);
}

// Põe `screenItem` (item direto da cena) num grupo "Tela Voltz" com a Voltz Mouse por cima. O grupo assume a
// transformação, a ordem, a visibilidade e a trava do item; as camadas ficam em (0,0), escala 1, travadas e com o
// mesmo recorte — o pixel (x,y) da captura cai no mesmo lugar nas duas. Devolve o item do grupo (null se não deu: sem
// plugin ou o libobs recusou).
obs_sceneitem_t *wrapScreenItem(obs_scene_t *scene, obs_sceneitem_t *screenItem)
{
	if (!scene || !screenItem) {
		return nullptr;
	}
	OBSSource mouse = ensureSharedSource(SharedSource::Mouse);
	if (!mouse) {
		return nullptr;
	}
	syncMouseTarget(mouse, obs_sceneitem_get_source(screenItem));

	obs_transform_info info = {};
	obs_sceneitem_get_info2(screenItem, &info);
	obs_sceneitem_crop crop = {};
	obs_sceneitem_get_crop(screenItem, &crop);
	const int order = obs_sceneitem_get_order_position(screenItem);
	const bool visible = obs_sceneitem_visible(screenItem);
	const bool locked = obs_sceneitem_locked(screenItem);

	obs_sceneitem_t *mouseItem = obs_scene_add(scene, mouse);
	if (!mouseItem) {
		return nullptr;
	}
	obs_sceneitem_set_visible(screenItem, true);
	placeLayer(screenItem);
	placeLayer(mouseItem);
	obs_sceneitem_set_crop(mouseItem, &crop);

	obs_sceneitem_t *items[] = {screenItem, mouseItem};
	const QString name = uniqueSourceName(QTStr("Voltz.Source.ScreenGroup"));
	obs_sceneitem_t *group = obs_scene_insert_group(scene, name.toUtf8().constData(), items, 2);
	if (!group) {
		obs_sceneitem_remove(mouseItem);
		obs_sceneitem_set_info2(screenItem, &info);
		obs_sceneitem_set_visible(screenItem, visible);
		blog(LOG_WARNING, "[voltz] tela voltz: o libobs recusou o grupo; Tela mantida como estava");
		return nullptr;
	}
	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(obs_sceneitem_get_source(group));
	obs_data_set_int(privateSettings, kScreenLayersKey, kScreenLayersVersion);
	obs_sceneitem_set_locked(screenItem, true);
	obs_sceneitem_set_locked(mouseItem, true);
	obs_sceneitem_set_info2(group, &info);
	obs_sceneitem_set_order_position(group, order);
	obs_sceneitem_set_visible(group, visible);
	obs_sceneitem_set_locked(group, locked);
	return group;
}

// Cenas de preset já nascem com a Tela Voltz: a migração não tem o que fazer nesta coleção.
void markScreenMigrated()
{
	OBSSource screen = findSharedSource(SharedSource::Screen);
	if (!screen || !obs_source_get_display_name(live::kMouseSourceId)) {
		return;
	}
	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(screen);
	obs_data_set_bool(privateSettings, kScreenMigratedKey, true);
}

void addRoundCamera(obs_scene_t *scene, obs_source_t *camera, CanvasSize canvas)
{
	obs_sceneitem_t *cameraItem = obs_scene_add(scene, camera);
	if (!cameraItem) {
		return;
	}
	obs_sceneitem_t *items[] = {cameraItem};
	obs_sceneitem_t *group =
		obs_scene_insert_group(scene, QTStr("Voltz.Source.RoundCamera").toUtf8().constData(), items, 1);
	if (!group) {
		/* Sem grupo: câmera quadrada no lugar do círculo (melhor do que nada). */
		placeItem(cameraItem, tutorialCamera(canvas), OBS_BOUNDS_SCALE_OUTER, true);
		return;
	}
	placeItem(group, tutorialCamera(canvas), OBS_BOUNDS_SCALE_OUTER, true);

	std::string maskPath;
	if (!GetDataFilePath("voltz/mask-circle.png", maskPath)) {
		blog(LOG_WARNING, "[voltz] mascara circular nao encontrada");
		return;
	}
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "type", "mask_alpha_filter.effect");
	obs_data_set_string(settings, "image_path", maskPath.c_str());
	obs_data_set_bool(settings, "stretch", false);
	OBSSourceAutoRelease mask = obs_source_create(
		"mask_filter_v2", QTStr("Voltz.Filter.CircleMask").toUtf8().constData(), settings, nullptr);
	if (mask) {
		obs_source_filter_add(obs_sceneitem_get_source(group), mask);
	}
}

void addTitle(obs_scene_t *scene, CanvasSize canvas)
{
	OBSDataAutoRelease font = obs_data_create();
	obs_data_set_string(font, "face", "Segoe UI");
	obs_data_set_int(font, "size", 120);
	obs_data_set_int(font, "flags", OBS_FONT_BOLD);
	obs_data_set_string(font, "style", "Bold");

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "text", QTStr("Voltz.Source.TitleText").toUtf8().constData());
	obs_data_set_obj(settings, "font", font);
	obs_data_set_string(settings, "align", "center");
	obs_data_set_string(settings, "valign", "center");

	OBSSourceAutoRelease title = obs_source_create(
		"text_gdiplus_v3", QTStr("Voltz.Source.Title").toUtf8().constData(), settings, nullptr);
	if (!title) {
		return;
	}
	obs_sceneitem_t *item = obs_scene_add(scene, title);
	if (!item) {
		return;
	}
	obs_transform_info info = {};
	vec2_set(&info.pos, float(canvas.width / 2), float(canvas.height / 2));
	vec2_set(&info.scale, 1.0f, 1.0f);
	info.alignment = OBS_ALIGN_CENTER;
	info.bounds_type = OBS_BOUNDS_NONE;
	obs_sceneitem_set_info2(item, &info);
}

OBSScene buildTutorial(CanvasSize canvas)
{
	bool created = false;
	OBSScene scene = sceneNamed(QTStr("Voltz.Scene.Tutorial"), created);
	if (created && scene) {
		if (OBSSource screen = ensureSharedSource(SharedSource::Screen)) {
			wrapScreenItem(scene, addFull(scene, screen, canvas, OBS_BOUNDS_SCALE_INNER, false));
		}
		if (OBSSource camera = ensureSharedSource(SharedSource::Camera)) {
			addRoundCamera(scene, camera, canvas);
		}
	}
	return scene;
}

OBSScene buildScreenOnly(CanvasSize canvas)
{
	bool created = false;
	OBSScene scene = sceneNamed(QTStr("Voltz.Scene.ScreenOnly"), created);
	if (created && scene) {
		if (OBSSource screen = ensureSharedSource(SharedSource::Screen)) {
			wrapScreenItem(scene, addFull(scene, screen, canvas, OBS_BOUNDS_SCALE_INNER, false));
		}
	}
	return scene;
}

OBSScene buildCameraOnly(CanvasSize canvas)
{
	bool created = false;
	OBSScene scene = sceneNamed(QTStr("Voltz.Scene.CameraOnly"), created);
	if (created && scene) {
		if (OBSSource camera = ensureSharedSource(SharedSource::Camera)) {
			addFull(scene, camera, canvas, OBS_BOUNDS_SCALE_OUTER, true);
		}
	}
	return scene;
}

OBSScene buildSideBySide(CanvasSize canvas)
{
	bool created = false;
	OBSScene scene = sceneNamed(QTStr("Voltz.Scene.SideBySide"), created);
	if (created && scene) {
		if (OBSSource background = ensureSharedSource(SharedSource::Background)) {
			addFull(scene, background, canvas, OBS_BOUNDS_STRETCH, false);
		}
		if (OBSSource screen = ensureSharedSource(SharedSource::Screen)) {
			if (obs_sceneitem_t *item = obs_scene_add(scene, screen)) {
				placeItem(item, sideBySideScreen(canvas), OBS_BOUNDS_SCALE_INNER, false);
				wrapScreenItem(scene, item);
			}
		}
		if (OBSSource camera = ensureSharedSource(SharedSource::Camera)) {
			if (obs_sceneitem_t *item = obs_scene_add(scene, camera)) {
				placeItem(item, sideBySideCamera(canvas), OBS_BOUNDS_SCALE_OUTER, true);
			}
		}
	}
	return scene;
}

OBSScene buildOpening(CanvasSize canvas)
{
	bool created = false;
	OBSScene scene = sceneNamed(QTStr("Voltz.Scene.Opening"), created);
	if (created && scene) {
		if (OBSSource background = ensureSharedSource(SharedSource::Background)) {
			addFull(scene, background, canvas, OBS_BOUNDS_STRETCH, false);
		}
		addTitle(scene, canvas);
	}
	return scene;
}

} // namespace

OBSSource applyPreset(PresetId id)
{
	const CanvasSize canvas = canvasSize();
	OBSScene main;
	switch (id) {
	case PresetId::Tutorial:
		main = buildTutorial(canvas);
		break;
	case PresetId::ScreenCamera:
		main = buildScreenOnly(canvas);
		buildCameraOnly(canvas);
		break;
	case PresetId::SideBySide:
		main = buildSideBySide(canvas);
		break;
	case PresetId::Opening:
		main = buildOpening(canvas);
		break;
	}
	markScreenMigrated();
	obs_frontend_save();
	blog(LOG_INFO, "[voltz] preset aplicado: %d", int(id));
	return main ? OBSSource(obs_scene_get_source(main)) : OBSSource();
}

void ensureScreenGroups()
{
	OBSSource screen = findSharedSource(SharedSource::Screen);
	if (!screen) {
		return;
	}
	if (OBSSource mouse = findSharedSource(SharedSource::Mouse)) {
		syncMouseTarget(mouse, screen);
	}
	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(screen);
	if (obs_data_get_bool(privateSettings, kScreenMigratedKey)) {
		return;
	}
	if (!obs_source_get_display_name(live::kMouseSourceId)) {
		blog(LOG_WARNING, "[voltz] tela voltz: plugin voltz-sources ausente; migracao adiada");
		return;
	}

	struct Found {
		obs_source_t *screen = nullptr;
		obs_scene_t *scene = nullptr;
		std::vector<std::pair<OBSScene, OBSSceneItem>> items;
	} found;
	found.screen = screen;
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		found.scene = obs_scene_from_source(scenes.sources.array[i]);
		if (!found.scene) {
			continue;
		}
		/* só coleta (com referência): mexer na cena durante o enum travaria o lock dela */
		obs_scene_enum_items(
			found.scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				auto *search = static_cast<Found *>(param);
				if (!obs_sceneitem_is_group(item) && obs_sceneitem_get_source(item) == search->screen) {
					search->items.emplace_back(OBSScene(search->scene), OBSSceneItem(item));
				}
				return true;
			},
			&found);
	}
	obs_frontend_source_list_free(&scenes);

	int wrapped = 0;
	for (auto &entry : found.items) {
		if (wrapScreenItem(entry.first, entry.second)) {
			wrapped++;
		}
	}
	obs_data_set_bool(privateSettings, kScreenMigratedKey, true);
	blog(LOG_INFO, "[voltz] tela voltz: %d item(ns) migrado(s)", wrapped);
	if (wrapped > 0) {
		obs_frontend_save();
	}
}

} // namespace voltz
