#pragma once

#include <obs.hpp>

namespace voltz {

enum class PresetId { Tutorial, ScreenCamera, SideBySide, Opening };

// Cria as fontes compartilhadas que faltarem e as cenas do preset. Cena com o mesmo nome já existente não é
// recriada (o usuário pode tê-la editado). Devolve a cena principal do preset (ref forte) para selecionar.
// Salva a coleção de cenas (obs_frontend_save). Thread da UI.
OBSSource applyPreset(PresetId id);

} // namespace voltz
