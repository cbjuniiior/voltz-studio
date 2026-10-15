#pragma once

#include <obs.hpp>

namespace voltz {

enum class SharedSource { Screen, Camera, Background };

// Devolve a fonte compartilhada (ref forte) — pelo UUID salvo em user.ini [Voltz]; se não existir mais, null.
OBSSource findSharedSource(SharedSource which);

// Devolve a fonte compartilhada, criando-a se preciso (Tela = monitor_capture do primeiro monitor; Câmera =
// dshow_input do primeiro dispositivo; Fundo = color_source_v3 #09090B do tamanho do canvas) e salvando o UUID.
OBSSource ensureSharedSource(SharedSource which);

} // namespace voltz
