#include "hotkey-bindings.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace voltz {

namespace {

using Json = nlohmann::ordered_json;

// "OBS_KEY_F12" → 12 com prefixo "OBS_KEY_F"; 0 se não for prefixo + número.
int numberAfter(const std::string &key, const char *prefix)
{
	const std::string start(prefix);
	if (key.size() <= start.size() || key.compare(0, start.size(), start) != 0) {
		return 0;
	}
	const std::string digits = key.substr(start.size());
	if (digits.size() > 3 || !std::all_of(digits.begin(), digits.end(), [](char c) {
		    return std::isdigit(static_cast<unsigned char>(c)) != 0;
	    })) {
		return 0;
	}
	return std::atoi(digits.c_str());
}

bool readFlag(const Json &object, const char *key)
{
	const auto it = object.find(key);
	return it != object.end() && it->is_boolean() && it->get<bool>();
}

} // namespace

bool KeyCombo::hasKey() const
{
	return !key.empty() && key != "OBS_KEY_NONE";
}

bool operator==(const KeyCombo &a, const KeyCombo &b)
{
	return a.shift == b.shift && a.control == b.control && a.alt == b.alt && a.command == b.command &&
	       (a.hasKey() ? a.key : std::string()) == (b.hasKey() ? b.key : std::string());
}

const std::vector<VoltzHotkey> &voltzHotkeys()
{
	static const std::vector<VoltzHotkey> hotkeys = {
		{"VoltzStudio.Draw", "OBS_KEY_D", "Voltz.Hotkey.Draw", false},
		{"VoltzStudio.UndoAnnotation", nullptr, "Voltz.Hotkey.UndoAnnotation", false},
		{"VoltzStudio.ClearAnnotations", nullptr, "Voltz.Hotkey.ClearAnnotations", false},
		{"VoltzStudio.AddMarker", "OBS_KEY_M", "Voltz.Hotkey.AddMarker", false},
		{"VoltzStudio.ToggleSpotlight", "OBS_KEY_H", "Voltz.Hotkey.ToggleSpotlight", false},
		{"VoltzStudio.DebugFreezeUi", "OBS_KEY_F12", "Voltz.Hotkey.DebugFreezeUi", true},
	};
	return hotkeys;
}

const VoltzHotkey *findVoltzHotkey(const std::string &name)
{
	for (const VoltzHotkey &hotkey : voltzHotkeys()) {
		if (name == hotkey.name) {
			return &hotkey;
		}
	}
	return nullptr;
}

std::vector<KeyCombo> defaultHotkeyBindings(const std::string &name)
{
	const VoltzHotkey *hotkey = findVoltzHotkey(name);
	if (!hotkey || !hotkey->defaultKey) {
		return {};
	}
	KeyCombo combo;
	combo.control = true;
	combo.shift = true;
	combo.key = hotkey->defaultKey;
	return {combo};
}

bool hotkeyBindingsFromJson(const std::string &json, std::vector<KeyCombo> &out)
{
	out.clear();
	const Json root = Json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		return false;
	}
	const auto list = root.find("bindings");
	if (list == root.end() || !list->is_array()) {
		return false;
	}
	for (const Json &item : *list) {
		if (!item.is_object()) {
			continue;
		}
		KeyCombo combo;
		combo.shift = readFlag(item, "shift");
		combo.control = readFlag(item, "control");
		combo.alt = readFlag(item, "alt");
		combo.command = readFlag(item, "command");
		const auto key = item.find("key");
		if (key != item.end() && key->is_string()) {
			combo.key = key->get<std::string>();
		}
		if (!combo.empty()) {
			out.push_back(combo);
		}
	}
	return true;
}

std::string hotkeyBindingsToJson(const std::vector<KeyCombo> &bindings)
{
	Json list = Json::array();
	for (const KeyCombo &combo : bindings) {
		if (combo.empty()) {
			continue;
		}
		Json item = Json::object();
		if (combo.shift) {
			item["shift"] = true;
		}
		if (combo.control) {
			item["control"] = true;
		}
		if (combo.alt) {
			item["alt"] = true;
		}
		if (combo.command) {
			item["command"] = true;
		}
		item["key"] = combo.hasKey() ? combo.key : std::string("OBS_KEY_NONE");
		list.push_back(item);
	}
	Json root = Json::object();
	root["bindings"] = list;
	return root.dump();
}

HotkeyComboCheck checkVoltzHotkeyCombo(const KeyCombo &combo)
{
	if (combo.empty()) {
		return HotkeyComboCheck::Empty;
	}
	if (!combo.hasKey()) {
		return HotkeyComboCheck::NeedsKey;
	}
	if (combo.control || combo.alt || combo.command) {
		return HotkeyComboCheck::Ok;
	}
	const int function = numberAfter(combo.key, "OBS_KEY_F");
	const int mouse = numberAfter(combo.key, "OBS_KEY_MOUSE");
	if ((function >= 1 && function <= 24) || mouse >= 4) {
		return HotkeyComboCheck::Ok;
	}
	return HotkeyComboCheck::NeedsModifier;
}

std::vector<KeyCombo> replaceFirstBinding(const std::vector<KeyCombo> &bindings, const KeyCombo &combo)
{
	std::vector<KeyCombo> out;
	if (!combo.empty()) {
		out.push_back(combo);
	}
	for (size_t i = 1; i < bindings.size(); i++) {
		if (!bindings[i].empty() && !(bindings[i] == combo)) {
			out.push_back(bindings[i]);
		}
	}
	return out;
}

std::vector<std::string> hotkeyConflicts(const std::string &name, const std::vector<KeyCombo> &bindings,
					 const std::vector<NamedBinding> &all)
{
	std::vector<std::string> out;
	for (const NamedBinding &other : all) {
		if (other.name == name || other.combo.empty() ||
		    std::find(out.begin(), out.end(), other.name) != out.end()) {
			continue;
		}
		for (const KeyCombo &combo : bindings) {
			if (!combo.empty() && combo == other.combo) {
				out.push_back(other.name);
				break;
			}
		}
	}
	return out;
}

} // namespace voltz
