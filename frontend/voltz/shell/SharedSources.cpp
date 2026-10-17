#include "SharedSources.hpp"

#include <OBSApp.hpp>
#include <voltz/core/live-protocol.hpp>

#include <util/config-file.h>

#include <cstring>
#include <string>

#include <QString>

namespace voltz {

namespace {

const char *configKey(SharedSource which)
{
	switch (which) {
	case SharedSource::Screen:
		return "ScreenSourceUuid";
	case SharedSource::Camera:
		return "CameraSourceUuid";
	case SharedSource::Background:
		return "BackgroundSourceUuid";
	case SharedSource::Mouse:
		return "MouseSourceUuid";
	}
	return "";
}

// Primeiro valor habilitado de uma propriedade de lista (ex.: monitor_id, video_device_id), ou "" se não houver.
std::string firstListValue(const char *sourceId, const char *property)
{
	obs_properties_t *props = obs_get_source_properties(sourceId);
	if (!props) {
		return {};
	}
	std::string value;
	if (obs_property_t *p = obs_properties_get(props, property)) {
		const size_t count = obs_property_list_item_count(p);
		for (size_t i = 0; i < count; i++) {
			const char *candidate = obs_property_list_item_string(p, i);
			if (!obs_property_list_item_disabled(p, i) && candidate && *candidate) {
				value = candidate;
				break;
			}
		}
	}
	obs_properties_destroy(props);
	return value;
}

OBSSource createShared(SharedSource which)
{
	OBSDataAutoRelease settings = obs_data_create();
	const char *id = nullptr;
	QString name;
	/* Só testes: presets sem captura de monitor nem webcam reais (fontes de cor nos tamanhos típicos). */
	const bool standins = qEnvironmentVariableIsSet("VOLTZ_TEST_PRESET_STANDINS");

	switch (which) {
	case SharedSource::Screen:
		name = QTStr("Voltz.Source.Screen");
		if (standins) {
			id = "color_source_v3";
			obs_data_set_int(settings, "color", 0xFF302A2A); // ABGR de #2A2A30
			obs_data_set_int(settings, "width", 2560);
			obs_data_set_int(settings, "height", 1440);
		} else {
			id = "monitor_capture";
			obs_data_set_string(settings, "monitor_id", firstListValue(id, "monitor_id").c_str());
			obs_data_set_bool(settings, "capture_cursor", true);
		}
		break;
	case SharedSource::Camera:
		name = QTStr("Voltz.Source.Camera");
		if (standins) {
			id = "color_source_v3";
			obs_data_set_int(settings, "color", 0xFF5A4A3A); // ABGR de #3A4A5A
			obs_data_set_int(settings, "width", 1280);
			obs_data_set_int(settings, "height", 720);
		} else {
			id = "dshow_input";
			obs_data_set_string(settings, "video_device_id", firstListValue(id, "video_device_id").c_str());
		}
		break;
	case SharedSource::Background: {
		id = "color_source_v3";
		name = QTStr("Voltz.Source.Background");
		obs_video_info ovi = {};
		obs_get_video_info(&ovi);
		obs_data_set_int(settings, "color", 0xFF0B0909); // ABGR de #09090B
		obs_data_set_int(settings, "width", ovi.base_width ? ovi.base_width : 1920);
		obs_data_set_int(settings, "height", ovi.base_height ? ovi.base_height : 1080);
		break;
	}
	case SharedSource::Mouse: {
		if (!obs_source_get_display_name(live::kMouseSourceId)) {
			blog(LOG_WARNING, "[voltz] plugin voltz-sources ausente: sem a fonte Voltz Mouse");
			return {};
		}
		id = live::kMouseSourceId;
		name = QTStr("Voltz.Source.Mouse");
		OBSSource screen = findSharedSource(SharedSource::Screen);
		obs_data_set_string(settings, live::kTargetSetting, screen ? obs_source_get_uuid(screen) : "");
		break;
	}
	}

	/* Nome já usado por outra fonte: reaproveita só se for do mesmo tipo; senão cria com sufixo. */
	const char *unversionedId = strcmp(id, "color_source_v3") == 0 ? "color_source" : id;
	OBSSourceAutoRelease existing = obs_get_source_by_name(name.toUtf8().constData());
	if (existing && strcmp(obs_source_get_unversioned_id(existing), unversionedId) == 0) {
		return OBSSource(existing.Get()); // OBSRef(T) faz addref; o AutoRelease solta o dele
	}
	if (existing) {
		name += QStringLiteral(" (Voltz)");
	}
	OBSSourceAutoRelease created = obs_source_create(id, name.toUtf8().constData(), settings, nullptr);
	return OBSSource(created.Get());
}

} // namespace

OBSSource findSharedSource(SharedSource which)
{
	const char *uuid = config_get_string(App()->GetUserConfig(), "Voltz", configKey(which));
	if (!uuid || !*uuid) {
		return {};
	}
	OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid);
	return OBSSource(source.Get());
}

OBSSource ensureSharedSource(SharedSource which)
{
	if (OBSSource found = findSharedSource(which)) {
		return found;
	}
	OBSSource created = createShared(which);
	if (created) {
		config_set_string(App()->GetUserConfig(), "Voltz", configKey(which), obs_source_get_uuid(created));
		config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
	}
	return created;
}

} // namespace voltz
