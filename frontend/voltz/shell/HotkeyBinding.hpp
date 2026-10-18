#pragma once

#include <voltz/core/hotkey-bindings.hpp>

#include <obs.h>

#include <QString>

#include <vector>

namespace voltz {

// Atalhos do Voltz no libobs e no perfil (Plano 4D: um lugar só para o marcador, o holofote e o modo desenho). Formato
// do OBS: perfil [Hotkeys] <nome> = {"bindings":[...]} (o do OBSBasic::InitHotkeys e do OBSBasicSettings, que continua
// editando os mesmos atalhos no modo clássico). Thread da UI.

// Carrega as teclas do atalho `name` (VoltzStudio.*) do perfil. Nada salvo: as teclas padrão da tabela do núcleo
// (voltzHotkeys). Lista vazia salva = sem atalho. JSON inválido = padrão, com aviso no log.
void loadHotkeyBinding(obs_hotkey_id id, const char *name);
// Grava `bindings` no atalho (vale na hora) e no perfil (config_save_safe); log "[voltz] atalhos: <nome> = <teclas>".
void saveHotkeyBinding(obs_hotkey_id id, const char *name, const std::vector<KeyCombo> &bindings);
// As teclas do atalho agora (obs_enum_hotkey_bindings), na ordem do libobs.
std::vector<KeyCombo> currentHotkeyBindings(obs_hotkey_id id);
// O atalho registrado com esse nome (OBS_INVALID_HOTKEY_ID se não há: atalho de depuração sem a variável).
obs_hotkey_id findHotkey(const char *name);
// Todas as teclas de todos os atalhos registrados (do OBS e do Voltz), com o nome de cada um (para os conflitos).
std::vector<NamedBinding> allHotkeyBindings();
// Descrição do atalho registrado com esse nome ("" se não há).
QString hotkeyDescription(const char *name);

KeyCombo fromObsCombination(obs_key_combination_t combination);
obs_key_combination_t toObsCombination(const KeyCombo &combo);
// "Ctrl + Shift + D" (texto do OBS, no idioma e no teclado do usuário); vazio para combinação vazia.
QString keyComboText(const KeyCombo &combo);
// Teclas do atalho em texto ("Ctrl + Shift + D"; vários separados por ", "); vazio sem atalho.
QString hotkeyShortcutText(obs_hotkey_id id);

} // namespace voltz
