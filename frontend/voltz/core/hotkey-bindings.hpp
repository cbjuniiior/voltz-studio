#pragma once

#include <string>
#include <vector>

namespace voltz {

// Uma combinação de teclas no formato do OBS (perfil [Hotkeys] <nome> = {"bindings":[...]}, o mesmo do obs_hotkey_save
// e do OBSBasicSettings): modificadores ligados e o nome da tecla ("OBS_KEY_D", "OBS_KEY_F12", "OBS_KEY_MOUSE4").
struct KeyCombo {
	bool shift = false;
	bool control = false;
	bool alt = false;
	bool command = false; // tecla Windows
	std::string key;      // "" ou "OBS_KEY_NONE" = só modificadores

	bool hasKey() const;
	bool hasModifier() const { return shift || control || alt || command; }
	bool empty() const { return !hasKey() && !hasModifier(); }
};

bool operator==(const KeyCombo &a, const KeyCombo &b);

// Atalhos do Voltz (obs_hotkey_register_frontend): nome, tecla padrão (sempre Ctrl+Shift+tecla) ou nenhuma, texto do
// locale e se só existe em depuração (VOLTZ_DEBUG_FREEZE_UI). Na ordem de Ajustes › Atalhos.
struct VoltzHotkey {
	const char *name;
	const char *defaultKey; // nullptr = sem tecla padrão
	const char *localeKey;
	bool debug;
};

const std::vector<VoltzHotkey> &voltzHotkeys();
// nullptr se `name` não é um atalho do Voltz.
const VoltzHotkey *findVoltzHotkey(const std::string &name);
// Teclas padrão (Ctrl+Shift+defaultKey) ou nenhuma (sem padrão ou nome desconhecido).
std::vector<KeyCombo> defaultHotkeyBindings(const std::string &name);

// {"bindings":[{"control":true,"shift":true,"key":"OBS_KEY_D"},...]} → combinações (itens vazios ou que não são objeto
// saem). Falso para JSON inválido, que não é objeto ou sem a lista "bindings" (aí `out` fica vazio).
bool hotkeyBindingsFromJson(const std::string &json, std::vector<KeyCombo> &out);
// Combinações → o mesmo formato (só os modificadores ligados, na ordem shift, control, alt, command; depois "key").
std::string hotkeyBindingsToJson(const std::vector<KeyCombo> &bindings);

enum class HotkeyComboCheck { Ok, Empty, NeedsKey, NeedsModifier };

// Pode ser atalho do Voltz? Precisa de uma tecla (não só modificadores) e de Ctrl, Alt ou Windows; sem eles (ou só com
// Shift) só F1–F24 e os botões laterais do mouse (MOUSE4 em diante). Os atalhos do OBS não consomem a tecla: "D"
// sozinho (ou Shift+D) ligaria o modo desenho no meio de qualquer digitação. Vazia = Empty (tirar o atalho é válido).
HotkeyComboCheck checkVoltzHotkeyCombo(const KeyCombo &combo);

// Troca a primeira combinação pela nova (ou a acrescenta, se não há nenhuma); as outras ficam — vieram das
// Configurações do OBS (modo clássico), que aceitam várias. Nova vazia = tira só a primeira.
std::vector<KeyCombo> replaceFirstBinding(const std::vector<KeyCombo> &bindings, const KeyCombo &combo);

struct NamedBinding {
	std::string name; // nome do atalho no OBS ("VoltzStudio.Draw", "OBSBasic.StartRecording", ...)
	KeyCombo combo;
};

// Nomes dos OUTROS atalhos (não `name`) que usam alguma das combinações não vazias de `bindings`, sem repetir, na ordem
// em que aparecem em `all`.
std::vector<std::string> hotkeyConflicts(const std::string &name, const std::vector<KeyCombo> &bindings,
					 const std::vector<NamedBinding> &all);

} // namespace voltz
