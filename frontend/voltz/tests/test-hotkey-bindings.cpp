#include <doctest.h>

#include <voltz/core/hotkey-bindings.hpp>

#include <set>
#include <string>
#include <vector>

using voltz::HotkeyComboCheck;
using voltz::KeyCombo;
using voltz::NamedBinding;

namespace {

KeyCombo combo(const char *key, bool control = false, bool shift = false, bool alt = false, bool command = false)
{
	KeyCombo out;
	out.key = key;
	out.control = control;
	out.shift = shift;
	out.alt = alt;
	out.command = command;
	return out;
}

} // namespace

TEST_CASE("atalhos do Voltz: tabela com nomes únicos e os padrões de antes (Ctrl+Shift+D/M/H; desfazer e limpar sem)")
{
	std::set<std::string> names;
	for (const voltz::VoltzHotkey &hotkey : voltz::voltzHotkeys()) {
		CHECK(std::string(hotkey.name).rfind("VoltzStudio.", 0) == 0);
		CHECK(std::string(hotkey.localeKey).rfind("Voltz.Hotkey.", 0) == 0);
		names.insert(hotkey.name);
	}
	CHECK(names.size() == voltz::voltzHotkeys().size());
	CHECK(voltz::voltzHotkeys().size() == 6);
	CHECK(std::string(voltz::voltzHotkeys().front().name) == "VoltzStudio.Draw");
	const std::vector<KeyCombo> draw = voltz::defaultHotkeyBindings("VoltzStudio.Draw");
	REQUIRE(draw.size() == 1);
	CHECK(draw[0] == combo("OBS_KEY_D", true, true));
	CHECK(voltz::defaultHotkeyBindings("VoltzStudio.AddMarker")[0].key == "OBS_KEY_M");
	CHECK(voltz::defaultHotkeyBindings("VoltzStudio.ToggleSpotlight")[0].key == "OBS_KEY_H");
	CHECK(voltz::defaultHotkeyBindings("VoltzStudio.DebugFreezeUi")[0].key == "OBS_KEY_F12");
	CHECK(voltz::defaultHotkeyBindings("VoltzStudio.UndoAnnotation").empty());
	CHECK(voltz::defaultHotkeyBindings("VoltzStudio.ClearAnnotations").empty());
	CHECK(voltz::defaultHotkeyBindings("OBSBasic.StartRecording").empty());
	REQUIRE(voltz::findVoltzHotkey("VoltzStudio.DebugFreezeUi") != nullptr);
	CHECK(voltz::findVoltzHotkey("VoltzStudio.DebugFreezeUi")->debug);
	CHECK_FALSE(voltz::findVoltzHotkey("VoltzStudio.Draw")->debug);
	CHECK(voltz::findVoltzHotkey("VoltzStudio.Nada") == nullptr);
}

TEST_CASE("hotkeyBindingsFromJson/ToJson: formato do OBS, ida e volta, tolerância")
{
	std::vector<KeyCombo> bindings;
	REQUIRE(voltz::hotkeyBindingsFromJson(
		R"({"bindings":[{"control":true,"shift":true,"key":"OBS_KEY_D"},{"alt":true,"command":true,"key":"OBS_KEY_F5"}]})",
		bindings));
	REQUIRE(bindings.size() == 2);
	CHECK(bindings[0] == combo("OBS_KEY_D", true, true));
	CHECK(bindings[1] == combo("OBS_KEY_F5", false, false, true, true));
	CHECK(voltz::hotkeyBindingsToJson(bindings) ==
	      R"({"bindings":[{"shift":true,"control":true,"key":"OBS_KEY_D"},{"alt":true,"command":true,"key":"OBS_KEY_F5"}]})");
	std::vector<KeyCombo> again;
	REQUIRE(voltz::hotkeyBindingsFromJson(voltz::hotkeyBindingsToJson(bindings), again));
	CHECK(again == bindings);

	/* lista vazia = sem atalho (válido); itens vazios ou que não são objeto saem; só modificador fica */
	REQUIRE(voltz::hotkeyBindingsFromJson(R"({"bindings":[]})", again));
	CHECK(again.empty());
	REQUIRE(voltz::hotkeyBindingsFromJson(
		R"({"bindings":[1,{},{"key":"OBS_KEY_NONE"},{"control":"sim","shift":true}]})", again));
	REQUIRE(again.size() == 1);
	CHECK(again[0] == combo("", false, true));
	CHECK(voltz::hotkeyBindingsToJson(again) == R"({"bindings":[{"shift":true,"key":"OBS_KEY_NONE"}]})");
	CHECK(voltz::hotkeyBindingsToJson({}) == R"({"bindings":[]})");

	CHECK_FALSE(voltz::hotkeyBindingsFromJson("lixo", again));
	CHECK_FALSE(voltz::hotkeyBindingsFromJson("[]", again));
	CHECK_FALSE(voltz::hotkeyBindingsFromJson(R"({"teclas":[]})", again));
	CHECK(again.empty());
}

TEST_CASE("checkVoltzHotkeyCombo: precisa de tecla e de Ctrl, Alt ou Windows; F1–F24 e mouse lateral sozinhos")
{
	CHECK(voltz::checkVoltzHotkeyCombo(KeyCombo{}) == HotkeyComboCheck::Empty);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("", true, true)) == HotkeyComboCheck::NeedsKey);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_NONE", true)) == HotkeyComboCheck::NeedsKey);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_D")) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_D", false, true)) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_ESCAPE")) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_MOUSE3")) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_D", true)) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_D", false, false, true)) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_D", false, false, false, true)) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_F1")) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_F24", false, true)) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_F25")) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_FN")) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_MOUSE4")) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_MOUSE29")) == HotkeyComboCheck::Ok);
}

TEST_CASE("replaceFirstBinding: troca a primeira, mantém as outras (sem repetir), vazia tira só a primeira")
{
	const KeyCombo a = combo("OBS_KEY_A", true);
	const KeyCombo b = combo("OBS_KEY_B", true);
	const KeyCombo c = combo("OBS_KEY_C", true);
	CHECK(voltz::replaceFirstBinding({}, a) == std::vector<KeyCombo>{a});
	CHECK(voltz::replaceFirstBinding({a, b}, c) == std::vector<KeyCombo>{c, b});
	CHECK(voltz::replaceFirstBinding({a, b}, b) == std::vector<KeyCombo>{b});
	CHECK(voltz::replaceFirstBinding({a, b}, KeyCombo{}) == std::vector<KeyCombo>{b});
	CHECK(voltz::replaceFirstBinding({a}, KeyCombo{}).empty());
}

TEST_CASE("hotkeyConflicts: mesma combinação em outro atalho; o próprio e as vazias não contam")
{
	const KeyCombo d = combo("OBS_KEY_D", true, true);
	const KeyCombo r = combo("OBS_KEY_R", true);
	const std::vector<NamedBinding> all = {{"VoltzStudio.Draw", d},
					       {"OBSBasic.StartRecording", r},
					       {"OBSBasic.Screenshot", d},
					       {"OBSBasic.Screenshot", d},
					       {"OBSBasic.Vazio", KeyCombo{}}};
	CHECK(voltz::hotkeyConflicts("VoltzStudio.Draw", {d}, all) == std::vector<std::string>{"OBSBasic.Screenshot"});
	CHECK(voltz::hotkeyConflicts("VoltzStudio.AddMarker", {r, d}, all) ==
	      std::vector<std::string>{"VoltzStudio.Draw", "OBSBasic.StartRecording", "OBSBasic.Screenshot"});
	CHECK(voltz::hotkeyConflicts("VoltzStudio.Draw", {KeyCombo{}}, all).empty());
	CHECK(voltz::hotkeyConflicts("VoltzStudio.Draw", {}, all).empty());
}

TEST_CASE("KeyCombo: OBS_KEY_NONE vale como sem tecla; igualdade compara modificadores e tecla")
{
	CHECK_FALSE(combo("OBS_KEY_NONE").hasKey());
	CHECK(combo("OBS_KEY_NONE").empty());
	CHECK(combo("OBS_KEY_NONE", true) == combo("", true));
	CHECK_FALSE(combo("OBS_KEY_D", true) == combo("OBS_KEY_D", true, true));
	CHECK_FALSE(combo("OBS_KEY_D", true) == combo("OBS_KEY_E", true));
	CHECK(combo("OBS_KEY_D", true).hasModifier());
}
