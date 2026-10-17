#pragma once

#include <obs.hpp>

namespace voltz {

// Mouse = fonte "Voltz Mouse" (plugin voltz-sources) com o ajuste "alvo" = UUID da Tela; só existe com o plugin.
enum class SharedSource { Screen, Camera, Background, Mouse };

// Devolve a fonte compartilhada (ref forte) — pelo UUID salvo em user.ini [Voltz]; se não existir mais, null.
OBSSource findSharedSource(SharedSource which);

// Devolve a fonte compartilhada, criando-a se preciso (Tela = monitor_capture do primeiro monitor; Câmera =
// dshow_input do primeiro dispositivo; Fundo = color_source_v3 #09090B do tamanho do canvas; Mouse = Voltz Mouse
// apontando para a Tela) e salvando o UUID. Só testes: com VOLTZ_TEST_PRESET_STANDINS, Tela e Câmera viram fontes de
// cor (2560×1440 #2A2A30 e 1280×720 #3A4A5A) — nenhuma captura de tela nem webcam é criada.
OBSSource ensureSharedSource(SharedSource which);

} // namespace voltz
