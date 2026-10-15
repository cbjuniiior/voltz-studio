#pragma once

namespace voltz {

// Acrescenta locale/voltz-en-US.ini e locale/voltz-<idioma>.ini ao lookup de textos do OBS.
// Idempotente: só carrega na primeira chamada. Chamar depois do OBSApp::InitLocale.
void loadShellLocale();

} // namespace voltz
