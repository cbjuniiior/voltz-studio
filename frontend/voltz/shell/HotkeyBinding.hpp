#pragma once

#include <obs.h>

#include <QString>

namespace voltz {

// Teclas do atalho `name` a partir do perfil, no formato do OBS ([Hotkeys] <name> = {"bindings":[...]}, o mesmo do
// OBSBasic::InitHotkeys / OBSBasicSettings::SaveHotkeySettings). Nada salvo: Ctrl+Shift+`defaultKey` (ex.
// "OBS_KEY_D"), ou sem tecla se `defaultKey` é nulo. Lista vazia salva = sem atalho. Thread da UI.
void loadHotkeyBinding(obs_hotkey_id id, const char *name, const char *defaultKey);

// Teclas do atalho em texto ("Ctrl + Shift + D"; vários separados por ", "); vazio sem atalho.
QString hotkeyShortcutText(obs_hotkey_id id);

} // namespace voltz
