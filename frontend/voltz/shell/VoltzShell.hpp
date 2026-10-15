#pragma once

#include <QMainWindow>

class QButtonGroup;
class QStackedWidget;

namespace voltz {

class FrontendBridge;

// Janela principal do Voltz Studio. A janela do OBS (OBSBasic) fica oculta como motor
// e pode ser aberta em Ajustes → "Abrir modo clássico".
class VoltzShell : public QMainWindow {
	Q_OBJECT

public:
	explicit VoltzShell(QWidget *parent = nullptr);

protected:
	void closeEvent(QCloseEvent *event) override;
	bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
	QWidget *buildSideNav();
	QWidget *buildPlaceholderPage(const QString &title, const QString &text);
	QWidget *buildSettingsPage();
	void selectPage(int index);

	FrontendBridge *bridge_ = nullptr;
	QStackedWidget *pages_ = nullptr;
	QButtonGroup *navGroup_ = nullptr;
	bool exiting_ = false;
};

} // namespace voltz
