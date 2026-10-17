#include "HotkeyBinding.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>
#include <util/config-file.h>
#include <util/dstr.h>

#include <QStringList>

namespace voltz {

void loadHotkeyBinding(obs_hotkey_id id, const char *name, const char *defaultKey)
{
	if (id == OBS_INVALID_HOTKEY_ID) {
		return;
	}
	config_t *profile = obs_frontend_get_profile_config();
	const char *json = profile ? config_get_string(profile, "Hotkeys", name) : nullptr;
	OBSDataArrayAutoRelease bindings;
	if (json && *json) {
		OBSDataAutoRelease data = obs_data_create_from_json(json);
		if (data) {
			bindings = obs_data_get_array(data, "bindings");
		}
	} else {
		bindings = obs_data_array_create();
		if (defaultKey) {
			OBSDataAutoRelease combo = obs_data_create();
			obs_data_set_bool(combo, "control", true);
			obs_data_set_bool(combo, "shift", true);
			obs_data_set_string(combo, "key", defaultKey);
			obs_data_array_push_back(bindings, combo);
		}
	}
	obs_hotkey_load(id, bindings);
}

QString hotkeyShortcutText(obs_hotkey_id id)
{
	struct Search {
		obs_hotkey_id id;
		QStringList keys;
	} search{id, {}};
	if (id == OBS_INVALID_HOTKEY_ID) {
		return QString();
	}
	obs_enum_hotkey_bindings(
		[](void *data, size_t, obs_hotkey_binding_t *binding) {
			auto *found = static_cast<Search *>(data);
			if (obs_hotkey_binding_get_hotkey_id(binding) == found->id) {
				struct dstr text = {};
				obs_key_combination_to_str(obs_hotkey_binding_get_key_combination(binding), &text);
				if (text.array && *text.array) {
					found->keys.append(QString::fromUtf8(text.array));
				}
				dstr_free(&text);
			}
			return true;
		},
		&search);
	return search.keys.join(QStringLiteral(", "));
}

} // namespace voltz
