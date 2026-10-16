#pragma once

#include <QFrame>

class QLabel;

namespace voltz {

// Faixa dispensável com título + mensagem (aceita HTML simples, como os textos do OBS). Vermelha (erro) por padrão;
// setWarning(true) a deixa âmbar (aviso).
class ErrorBanner : public QFrame {
	Q_OBJECT

public:
	explicit ErrorBanner(QWidget *parent = nullptr);

	void showError(const QString &title, const QString &message);
	void setWarning(bool warning);

signals:
	// O usuário fechou a faixa pelo X.
	void dismissed();

private:
	QLabel *icon_ = nullptr;
	QLabel *title_ = nullptr;
	QLabel *message_ = nullptr;
};

} // namespace voltz
