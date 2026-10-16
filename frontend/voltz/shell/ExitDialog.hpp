#pragma once

class QMessageBox;
class QPushButton;
class QWidget;

namespace voltz {

// Pergunta de saída com o visual da casca. `recording`: gravação ativa ("Parar a gravação e sair?");
// senão fala das outras saídas ativas. Devolve true para sair. Enter = Cancelar.
bool confirmExitWhileActive(QWidget *parent, bool recording);

// Visual da casca numa QMessageBox do OBS (ex. aviso de encerramento inesperado, que abre antes da casca
// existir): DM Sans, QSS da casca, ícone Phosphor e barra de título escura. `primary` (opcional) fica em lima.
void applyShellStyle(QMessageBox *box, QPushButton *primary);

} // namespace voltz
