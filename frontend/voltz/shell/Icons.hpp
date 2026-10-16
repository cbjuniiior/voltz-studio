#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

class QAbstractButton;

namespace voltz {

namespace palette {
inline const QColor kText(0xF2, 0xF2, 0xF3);
inline const QColor kMuted(0x82, 0x82, 0x8C);
inline const QColor kAccent(0xD4, 0xF5, 0x3C);
inline const QColor kRec(0xFF, 0x4D, 0x4D);
inline const QColor kWarn(0xF5, 0xC8, 0x3C);
inline const QColor kOnLight(0x09, 0x09, 0x0B);
} // namespace palette

enum class IconWeight { Regular, Fill };

// Ícone Phosphor (":/voltz/icons/...") recolorido. `name` sem sufixo, ex. "microphone".
QIcon phosphorIcon(const QString &name, const QColor &color, IconWeight weight = IconWeight::Regular);

// Ícone de dois estados para botões checáveis: Off = Regular na cor `off`, On = Fill na cor `on`.
QIcon phosphorToggleIcon(const QString &name, const QColor &off, const QColor &on);

// Botão só com ícone: a dica vira também o nome acessível (leitores de tela e UI Automation usam o nome).
// `context` (ex. nome da fonte) entra só no nome acessível: "Mostrar/ocultar — Tela".
void setButtonHint(QAbstractButton *button, const QString &hint, const QString &context = QString());

// Registra a DM Sans (4 pesos) no QFontDatabase. Idempotente.
void loadShellFonts();

} // namespace voltz
