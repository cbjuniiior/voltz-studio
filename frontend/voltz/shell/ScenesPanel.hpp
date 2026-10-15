#pragma once

#include <QFrame>

class QListWidget;

namespace voltz {

class FrontendBridge;

// Painel "CENAS": lista as cenas na ordem do OBS e troca a cena atual ao clicar. Identifica por UUID.
class ScenesPanel : public QFrame {
	Q_OBJECT

public:
	explicit ScenesPanel(FrontendBridge *bridge, QWidget *parent = nullptr);

private:
	void refresh();
	void selectScene(const QString &uuid);

	FrontendBridge *bridge_;
	QListWidget *list_ = nullptr;
	bool updating_ = false;
};

} // namespace voltz
