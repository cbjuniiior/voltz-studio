#include "SharedSources.hpp"

#include <OBSApp.hpp>

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

	switch (which) {
	case SharedSource::Screen:
		id = "monitor_capture";
		name = QTStr("Voltz.Source.Screen");
		obs_data_set_string(settings, "monitor_id", firstListValue(id, "monitor_id").c_str());
		obs_data_set_bool(settings, "capture_cursor", true);
		break;
	case SharedSource::Camera:
		id = "dshow_input";
		name = QTStr("Voltz.Source.Camera");
		obs_data_set_string(settings, "video_device_id", firstListValue(id, "video_device_id").c_str());
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
	}

	/* Nome já usado por outra fonte: reaproveita só se for do mesmo tipo; senão cria com sufixo. */
	const char *unversionedId = which == SharedSource::Background ? "color_source" : id;
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
