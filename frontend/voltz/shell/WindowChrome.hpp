#pragma once

class QWidget;

namespace voltz {

// Barra de título nativa escura (Windows 10 20H1+/11: modo escuro; Windows 11: cor da barra e do texto).
// No-op fora do Windows.
void applyDarkTitleBar(QWidget *window);

// Repassa WM_MOVE/WM_DISPLAYCHANGE aos OBSQTDisplay filhos de `window` (cor/HDR acompanham o monitor),
// como o OBSBasic::nativeEvent faz. Chamar de dentro do nativeEvent da janela.
void forwardDisplayEvents(QWidget *window, void *message);

} // namespace voltz
