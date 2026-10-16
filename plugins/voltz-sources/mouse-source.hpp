#pragma once

namespace voltz {

// Registra a fonte "voltz_mouse_source" (Voltz Mouse): tamanho = tamanho da captura do ajuste "alvo" (UUID);
// desenha holofote, ondas de clique e halo a partir do MouseFeed. Fica fora do "Adicionar fonte"
// (OBS_SOURCE_CAP_DISABLED): quem cria é o preset/migração do Voltz.
void registerMouseSource();

} // namespace voltz
