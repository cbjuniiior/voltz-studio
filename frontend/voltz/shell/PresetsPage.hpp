#pragma once

#include "PresetBuilder.hpp"

#include <QWidget>

class QFrame;

namespace voltz {

// Tela "Presets": 4 cartões (miniatura, nome, descrição, Aplicar). Na primeira execução mostra uma faixa de
// boas-vindas com "Pular". Emite presetApplied depois de aplicar (a casca volta para a tela Gravar).
class PresetsPage : public QWidget {
	Q_OBJECT

public:
	explicit PresetsPage(QWidget *parent = nullptr);

	static bool isFirstRun();

signals:
	void presetApplied();
	void firstRunDismissed();

private:
	QWidget *buildCard(PresetId preset, const QString &nameKey, const QString &descKey);
	void apply(PresetId preset);
	void markFirstRunDone();

	QFrame *welcome_ = nullptr;
};

} // namespace voltz
