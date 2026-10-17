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
// (1 = Tela + Voltz Mouse, do 4A; 2 = Tela + Voltz Anotações + Voltz Mouse, do 4B).
constexpr const char *kScreenMigratedKey = "voltz_tela_migrada";
constexpr const char *kScreenLayersKey = "voltz_tela_camadas";
constexpr int kScreenLayersMouseOnly = 1;
constexpr int kScreenLayersVersion = 2;
// Só testes (com VOLTZ_TEST_LIVE): "1" = os presets criam grupos versão 1 e a atualização não roda (testa a migração
// 1 → 2 sem editar a coleção à mão).
constexpr const char *kTestLayersEnv = "VOLTZ_TEST_SCREEN_LAYERS";

bool testLayersV1()
{
	return live::testModeEnabled() && qEnvironmentVariable(kTestLayersEnv) == QStringLiteral("1");
}

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

// Aponta o "alvo" de uma camada Voltz (Voltz Mouse ou Voltz Anotações) para a Tela.
void syncTarget(obs_source_t *layer, obs_source_t *screen)
{
	const char *uuid = obs_source_get_uuid(screen);
	OBSDataAutoRelease settings = obs_source_get_settings(layer);
	if (strcmp(obs_data_get_string(settings, live::kTargetSetting), uuid) == 0) {
		return;
	}
	OBSDataAutoRelease update = obs_data_create();
	obs_data_set_string(update, live::kTargetSetting, uuid);
	obs_source_update(layer, update);
}

// Põe `screenItem` (item direto da cena) num grupo "Tela Voltz" com as camadas Voltz por cima, de baixo para cima:
// Voltz Anotações e Voltz Mouse (versão de camadas 2). O grupo assume a transformação, a ordem, a visibilidade e a trava
// do item; as camadas ficam em (0,0), escala 1, travadas e com o mesmo recorte — o pixel (x,y) da captura cai no mesmo
// lugar em todas. Devolve o item do grupo (null se não deu: sem plugin ou o libobs recusou).
obs_sceneitem_t *wrapScreenItem(obs_scene_t *scene, obs_sceneitem_t *screenItem)
{
	if (!scene || !screenItem) {
		return nullptr;
	}
	OBSSource mouse = ensureSharedSource(SharedSource::Mouse);
	if (!mouse) {
		return nullptr;
	}
	OBSSource annotations = testLayersV1() ? OBSSource() : ensureSharedSource(SharedSource::Annotations);
	obs_source_t *screen = obs_sceneitem_get_source(screenItem);
	syncTarget(mouse, screen);
	if (annotations) {
		syncTarget(annotations, screen);
	}

	obs_transform_info info = {};
	obs_sceneitem_get_info2(screenItem, &info);
	obs_sceneitem_crop crop = {};
	obs_sceneitem_get_crop(screenItem, &crop);
	const int order = obs_sceneitem_get_order_position(screenItem);
	const bool visible = obs_sceneitem_visible(screenItem);
	const bool locked = obs_sceneitem_locked(screenItem);

	obs_sceneitem_t *annotationsItem = annotations ? obs_scene_add(scene, annotations) : nullptr;
	obs_sceneitem_t *mouseItem = obs_scene_add(scene, mouse);
	if (!mouseItem) {
		if (annotationsItem) {
			obs_sceneitem_remove(annotationsItem);
		}
		return nullptr;
	}
	obs_sceneitem_set_visible(screenItem, true);
	std::vector<obs_sceneitem_t *> items = {screenItem};
	if (annotationsItem) {
		items.push_back(annotationsItem);
	}
	items.push_back(mouseItem);
	for (obs_sceneitem_t *layer : items) {
		placeLayer(layer);
		if (layer != screenItem) {
			obs_sceneitem_set_crop(layer, &crop);
		}
	}

	const QString name = uniqueSourceName(QTStr("Voltz.Source.ScreenGroup"));
	obs_sceneitem_t *group = obs_scene_insert_group(scene, name.toUtf8().constData(), items.data(), items.size());
	if (!group) {
		obs_sceneitem_remove(mouseItem);
		if (annotationsItem) {
			obs_sceneitem_remove(annotationsItem);
		}
		obs_sceneitem_set_info2(screenItem, &info);
		obs_sceneitem_set_visible(screenItem, visible);
		blog(LOG_WARNING, "[voltz] tela voltz: o libobs recusou o grupo; Tela mantida como estava");
		return nullptr;
	}
	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(obs_sceneitem_get_source(group));
	obs_data_set_int(privateSettings, kScreenLayersKey,
			 annotationsItem ? kScreenLayersVersion : kScreenLayersMouseOnly);
	for (obs_sceneitem_t *layer : items) {
		obs_sceneitem_set_locked(layer, true);
	}
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

// Grupos "Tela Voltz" da versão 1 (Tela + Voltz Mouse, do 4A) ganham a Voltz Anotações entre as duas (versão 2), em
// (0,0), escala 1, travada e com o recorte da Tela. Devolve quantos grupos mudaram.
int upgradeScreenGroups(obs_source_t *screen)
{
	struct Found {
		std::vector<OBSSceneItem> groups;
	} found;
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		obs_scene_t *scene = obs_scene_from_source(scenes.sources.array[i]);
		if (!scene) {
			continue;
		}
		/* só coleta (com referência): mexer no grupo durante o enum travaria o lock da cena */
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				if (obs_sceneitem_is_group(item)) {
					OBSDataAutoRelease privateSettings =
						obs_source_get_private_settings(obs_sceneitem_get_source(item));
					if (obs_data_get_int(privateSettings, kScreenLayersKey) ==
					    kScreenLayersMouseOnly) {
						static_cast<Found *>(param)->groups.emplace_back(item);
					}
				}
				return true;
			},
			&found);
	}
	obs_frontend_source_list_free(&scenes);
	if (found.groups.empty()) {
		return 0;
	}
	OBSSource annotations = ensureSharedSource(SharedSource::Annotations);
	if (!annotations) {
		return 0;
	}
	syncTarget(annotations, screen);
	int upgraded = 0;
	for (const OBSSceneItem &group : found.groups) {
		OBSDataAutoRelease privateSettings = obs_source_get_private_settings(obs_sceneitem_get_source(group));
		/* o mesmo grupo pode estar em mais de uma cena (referência): só a primeira passagem acrescenta */
		if (obs_data_get_int(privateSettings, kScreenLayersKey) != kScreenLayersMouseOnly) {
			continue;
		}
		obs_scene_t *parent = obs_sceneitem_get_scene(group);
		obs_scene_t *inner = obs_sceneitem_group_get_scene(group);
		/* Como no OBS: o item nasce na cena-mãe e entra no grupo. Os ids dos itens de um grupo vêm do contador da
		 * cena-mãe (o do grupo fica em 0); obs_scene_add direto no grupo repetiria o id da Tela e a lista de
		 * Fontes, que acha as linhas por (grupo, id), mostraria a camada no lugar da Tela. */
		obs_sceneitem_t *layer = parent && inner ? obs_scene_add(parent, annotations) : nullptr;
		if (!layer) {
			continue;
		}
		obs_sceneitem_defer_group_resize_begin(group);
		obs_sceneitem_group_add_item(group, layer);
		if (obs_sceneitem_get_scene(layer) != inner) {
			obs_sceneitem_defer_group_resize_end(group);
			obs_sceneitem_remove(layer);
			continue;
		}
		placeLayer(layer);
		if (obs_sceneitem_t *screenLayer = obs_scene_find_source(inner, obs_source_get_name(screen))) {
			obs_sceneitem_crop crop = {};
			obs_sceneitem_get_crop(screenLayer, &crop);
			obs_sceneitem_set_crop(layer, &crop);
		}
		obs_sceneitem_set_order_position(layer, 1);
		obs_sceneitem_set_locked(layer, true);
		obs_sceneitem_defer_group_resize_end(group);
		obs_data_set_int(privateSettings, kScreenLayersKey, kScreenLayersVersion);
		upgraded++;
	}
	blog(LOG_INFO, "[voltz] tela voltz: %d grupo(s) com a camada de anotacoes", upgraded);
	return upgraded;
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
		syncTarget(mouse, screen);
	}
	if (OBSSource annotations = findSharedSource(SharedSource::Annotations)) {
		syncTarget(annotations, screen);
	}
	const bool plugin = obs_source_get_display_name(live::kMouseSourceId) != nullptr;
	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(screen);
	int wrapped = 0;
	if (!obs_data_get_bool(privateSettings, kScreenMigratedKey)) {
		if (!plugin) {
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
					if (!obs_sceneitem_is_group(item) &&
					    obs_sceneitem_get_source(item) == search->screen) {
						search->items.emplace_back(OBSScene(search->scene), OBSSceneItem(item));
					}
					return true;
				},
				&found);
		}
		obs_frontend_source_list_free(&scenes);
		for (auto &entry : found.items) {
			if (wrapScreenItem(entry.first, entry.second)) {
				wrapped++;
			}
		}
		obs_data_set_bool(privateSettings, kScreenMigratedKey, true);
		blog(LOG_INFO, "[voltz] tela voltz: %d item(ns) migrado(s)", wrapped);
	}
	/* versão de camadas: independe da migração (coleções migradas no 4A têm grupos versão 1) */
	const bool annotationsPlugin = obs_source_get_display_name(live::kAnnotationsSourceId) != nullptr;
	const int upgraded = plugin && annotationsPlugin && !testLayersV1() ? upgradeScreenGroups(screen) : 0;
	if (wrapped > 0 || upgraded > 0) {
		obs_frontend_save();
	}
}

} // namespace voltz
