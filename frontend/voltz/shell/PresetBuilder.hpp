#pragma once

#include <obs.hpp>

namespace voltz {

enum class PresetId { Tutorial, ScreenCamera, SideBySide, Opening };

// Cria as fontes compartilhadas que faltarem e as cenas do preset. Cena com o mesmo nome já existente não é
// recriada (o usuário pode tê-la editado). Devolve a cena principal do preset (ref forte) para selecionar.
// Salva a coleção de cenas (obs_frontend_save). Thread da UI.
OBSSource applyPreset(PresetId id);

// Tela Voltz (spec §4.4) nas cenas que já existiam: na primeira vez em cada coleção, cada item DIRETO da Tela
// compartilhada vira um grupo "Tela Voltz" com a camada Voltz Mouse (o grupo herda transformação, ordem, visibilidade e
// trava). Marca a Tela (voltz_tela_migrada, nas configurações privadas salvas na coleção) para não repetir — quem
// desagrupar não vê o grupo voltar. Acerta também o "alvo" da Voltz Mouse. Sem o plugin voltz-sources, adia. Thread
// da UI.
void ensureScreenGroups();

} // namespace voltz
