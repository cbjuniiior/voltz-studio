#pragma once

#include <obs.hpp>

#include <QFrame>

class QListWidget;

namespace voltz {

class FrontendBridge;

// Painel "CENAS": lista as cenas na ordem do OBS e troca a cena atual ao clicar. Identifica por UUID.
class ScenesPanel : public QFrame {
	Q_OBJECT

public:
	explicit ScenesPanel(FrontendBridge *bridge, QWidget *parent = nullptr);

private slots:
	void refresh();

private:
	void selectScene(const QString &uuid);

	// Thread do libobs: renomear uma cena não gera evento do frontend, então a lista escuta o sinal global.
	static void handleSourceRename(void *data, calldata_t *params);

	FrontendBridge *bridge_;
	QListWidget *list_ = nullptr;
	bool updating_ = false;
	OBSSignal renameSignal_;
};

} // namespace voltz
