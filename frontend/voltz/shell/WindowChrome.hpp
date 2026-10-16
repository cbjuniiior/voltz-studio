#pragma once

class QWidget;

namespace voltz {

// Barra de título nativa escura (Windows 10 20H1+/11: modo escuro; Windows 11: cor da barra e do texto).
// No-op fora do Windows.
void applyDarkTitleBar(QWidget *window);

// Repassa WM_MOVE/WM_DISPLAYCHANGE aos OBSQTDisplay filhos de `window` (cor/HDR acompanham o monitor),
// como o OBSBasic::nativeEvent faz. Chamar de dentro do nativeEvent da janela.
void forwardDisplayEvents(QWidget *window, void *message);

enum class TaskbarState { Idle, Recording, Paused };

// Ícone de estado sobre o botão da janela na barra de tarefas (mesmos ícones do OBS: active/paused).
// No-op fora do Windows.
void setTaskbarState(QWidget *window, TaskbarState state);

// Loga a afinidade de exibição da janela (0x11 = WDA_EXCLUDEFROMCAPTURE: fora da captura de tela).
void logCaptureAffinity(QWidget *window);

} // namespace voltz
