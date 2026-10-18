#include "HotkeyBinding.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>
#include <util/config-file.h>
#include <util/dstr.h>

#include <QStringList>

#include <cstring>
#include <string>

namespace voltz {

namespace {

std::vector<obs_key_combination_t> toObsCombinations(const std::vector<KeyCombo> &bindings)
{
	std::vector<obs_key_combination_t> out;
	for (const KeyCombo &combo : bindings) {
		if (!combo.empty()) {
			out.push_back(toObsCombination(combo));
		}
	}
	return out;
}

} // namespace

KeyCombo fromObsCombination(obs_key_combination_t combination)
{
	KeyCombo combo;
	combo.shift = (combination.modifiers & INTERACT_SHIFT_KEY) != 0;
	combo.control = (combination.modifiers & INTERACT_CONTROL_KEY) != 0;
	combo.alt = (combination.modifiers & INTERACT_ALT_KEY) != 0;
	combo.command = (combination.modifiers & INTERACT_COMMAND_KEY) != 0;
	if (combination.key != OBS_KEY_NONE) {
		const char *name = obs_key_to_name(combination.key);
		combo.key = name ? name : "";
	}
	return combo;
}

obs_key_combination_t toObsCombination(const KeyCombo &combo)
{
	obs_key_combination_t combination = {};
	combination.modifiers = (combo.shift ? INTERACT_SHIFT_KEY : 0u) | (combo.control ? INTERACT_CONTROL_KEY : 0u) |
				(combo.alt ? INTERACT_ALT_KEY : 0u) | (combo.command ? INTERACT_COMMAND_KEY : 0u);
	combination.key = combo.hasKey() ? obs_key_from_name(combo.key.c_str()) : OBS_KEY_NONE;
	return combination;
}

void loadHotkeyBinding(obs_hotkey_id id, const char *name)
{
	if (id == OBS_INVALID_HOTKEY_ID || !name) {
		return;
	}
	config_t *profile = obs_frontend_get_profile_config();
	const char *json = profile ? config_get_string(profile, "Hotkeys", name) : nullptr;
	std::vector<KeyCombo> bindings;
	if (!json || !*json) {
		bindings = defaultHotkeyBindings(name);
	} else if (!hotkeyBindingsFromJson(json, bindings)) {
		blog(LOG_WARNING, "[voltz] atalhos: %s invalido no perfil; usando o padrao", name);
		bindings = defaultHotkeyBindings(name);
	}
	std::vector<obs_key_combination_t> combinations = toObsCombinations(bindings);
	obs_hotkey_load_bindings(id, combinations.data(), combinations.size());
}

void saveHotkeyBinding(obs_hotkey_id id, const char *name, const std::vector<KeyCombo> &bindings)
{
	if (id == OBS_INVALID_HOTKEY_ID || !name) {
		return;
	}
	std::vector<obs_key_combination_t> combinations = toObsCombinations(bindings);
	obs_hotkey_load_bindings(id, combinations.data(), combinations.size());
	const std::string json = hotkeyBindingsToJson(bindings);
	config_t *profile = obs_frontend_get_profile_config();
	if (profile) {
		config_set_string(profile, "Hotkeys", name, json.c_str());
		config_save_safe(profile, "tmp", nullptr);
	}
	blog(LOG_INFO, "[voltz] atalhos: %s = %s", name, json.c_str());
}

std::vector<KeyCombo> currentHotkeyBindings(obs_hotkey_id id)
{
	struct Search {
		obs_hotkey_id id;
		std::vector<KeyCombo> combos;
	} search{id, {}};
	if (id == OBS_INVALID_HOTKEY_ID) {
		return {};
	}
	obs_enum_hotkey_bindings(
		[](void *data, size_t, obs_hotkey_binding_t *binding) {
			auto *found = static_cast<Search *>(data);
			if (obs_hotkey_binding_get_hotkey_id(binding) == found->id) {
				found->combos.push_back(
					fromObsCombination(obs_hotkey_binding_get_key_combination(binding)));
			}
			return true;
		},
		&search);
	return search.combos;
}

obs_hotkey_id findHotkey(const char *name)
{
	struct Search {
		const char *name;
		obs_hotkey_id id;
	} search{name, OBS_INVALID_HOTKEY_ID};
	if (!name) {
		return OBS_INVALID_HOTKEY_ID;
	}
	obs_enum_hotkeys(
		[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey) {
			auto *found = static_cast<Search *>(data);
			const char *current = obs_hotkey_get_name(hotkey);
			if (current && std::strcmp(current, found->name) == 0) {
				found->id = id;
				return false;
			}
			return true;
		},
		&search);
	return search.id;
}

std::vector<NamedBinding> allHotkeyBindings()
{
	std::vector<NamedBinding> all;
	obs_enum_hotkey_bindings(
		[](void *data, size_t, obs_hotkey_binding_t *binding) {
			auto *out = static_cast<std::vector<NamedBinding> *>(data);
			const obs_hotkey_t *hotkey = obs_hotkey_binding_get_hotkey(binding);
			const char *name = hotkey ? obs_hotkey_get_name(hotkey) : nullptr;
			out->push_back(NamedBinding{
				name ? name : "", fromObsCombination(obs_hotkey_binding_get_key_combination(binding))});
			return true;
		},
		&all);
	return all;
}

QString hotkeyDescription(const char *name)
{
	struct Search {
		const char *name;
		QString description;
	} search{name, {}};
	if (!name) {
		return {};
	}
	obs_enum_hotkeys(
		[](void *data, obs_hotkey_id, obs_hotkey_t *hotkey) {
			auto *found = static_cast<Search *>(data);
			const char *current = obs_hotkey_get_name(hotkey);
			if (current && std::strcmp(current, found->name) == 0) {
				const char *description = obs_hotkey_get_description(hotkey);
				found->description = QString::fromUtf8(description ? description : current);
				return false;
			}
			return true;
		},
		&search);
	return search.description;
}

QString keyComboText(const KeyCombo &combo)
{
	if (combo.empty()) {
		return {};
	}
	struct dstr text = {};
	obs_key_combination_to_str(toObsCombination(combo), &text);
	const QString out = QString::fromUtf8(text.array ? text.array : "");
	dstr_free(&text);
	return out;
}

QString hotkeyShortcutText(obs_hotkey_id id)
{
	QStringList keys;
	for (const KeyCombo &combo : currentHotkeyBindings(id)) {
		const QString text = keyComboText(combo);
		if (!text.isEmpty()) {
			keys.append(text);
		}
	}
	return keys.join(QStringLiteral(", "));
}

} // namespace voltz
