#pragma once

#include <QFrame>

class QLabel;

namespace voltz {

// Faixa vermelha dispensável com título + mensagem (aceita HTML simples, como os textos do OBS).
class ErrorBanner : public QFrame {
	Q_OBJECT

public:
	explicit ErrorBanner(QWidget *parent = nullptr);

	void showError(const QString &title, const QString &message);

private:
	QLabel *title_ = nullptr;
	QLabel *message_ = nullptr;
};

} // namespace voltz
