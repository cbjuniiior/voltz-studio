#pragma once

#include <QMainWindow>

class QButtonGroup;
class QStackedWidget;

namespace voltz {

class FrontendBridge;
class LiveMouse;
class MouseStyleStore;
class ProjectRecorder;

// Janela principal do Voltz Studio. A janela do OBS (OBSBasic) fica oculta como motor
// e pode ser aberta em Ajustes → "Abrir modo clássico".
class VoltzShell : public QMainWindow {
	Q_OBJECT

public:
	explicit VoltzShell(QWidget *parent = nullptr);

	// Bandeja ("Mostrar/Ocultar" e clique no ícone): visível → esconde; oculta ou minimizada → mostra.
	void toggleFromTray();
	void showFromTray();

protected:
	void changeEvent(QEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void showEvent(QShowEvent *event) override;
	bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
	QWidget *buildSideNav();
	void selectPage(int index);
	// Marca o botão da navegação e troca a página (para trocas que não vêm de clique).
	void showPage(int index);
	// Liga/desliga o desenho das prévias (OBSQTDisplay), como o OBS faz ao esconder/minimizar a janela.
	void setPreviewsEnabled(bool enabled);

	FrontendBridge *bridge_ = nullptr;
	ProjectRecorder *recorder_ = nullptr;
	MouseStyleStore *mouseStyles_ = nullptr;
	LiveMouse *liveMouse_ = nullptr;
	QStackedWidget *pages_ = nullptr;
	QButtonGroup *navGroup_ = nullptr;
	bool exiting_ = false;
};

} // namespace voltz
