#include "PresetBuilder.hpp"

#include "SharedSources.hpp"

#include <voltz/core/preset-layout.hpp>

#include <OBSApp.hpp>
#include <utility/platform.hpp>

#include <obs-frontend-api.h>

#include <QString>

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
			addFull(scene, screen, canvas, OBS_BOUNDS_SCALE_INNER, false);
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
			addFull(scene, screen, canvas, OBS_BOUNDS_SCALE_INNER, false);
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
	obs_frontend_save();
	blog(LOG_INFO, "[voltz] preset aplicado: %d", int(id));
	return main ? OBSSource(obs_scene_get_source(main)) : OBSSource();
}

} // namespace voltz
