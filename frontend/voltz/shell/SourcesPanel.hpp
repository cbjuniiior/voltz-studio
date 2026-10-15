#pragma once

#include <obs.hpp>

#include <QFrame>

#include <vector>

class QLabel;
class QListWidget;
class QToolButton;

namespace voltz {

class FrontendBridge;

// Painel "FONTES": fontes da cena atual com mostrar/ocultar, travar, ordem, adicionar, remover,
// propriedades e filtros.
class SourcesPanel : public QFrame {
	Q_OBJECT

public:
	explicit SourcesPanel(FrontendBridge *bridge, QWidget *parent = nullptr);

private slots:
	void refresh();

private:
	void bindCurrentScene();
	obs_scene_t *currentScene() const;
	obs_sceneitem_t *selectedItem() const;
	QWidget *buildRow(obs_sceneitem_t *item);
	void updateButtons();
	void addSource();
	void removeSelected();
	void moveSelected(obs_order_movement movement);
	void openProperties();
	void openFilters();
	void showContextMenu(const QPoint &pos);

	static void handleSceneSignal(void *data, calldata_t *params);

	FrontendBridge *bridge_;
	OBSWeakSource sceneSource_;
	std::vector<OBSSignal> sceneSignals_;
	OBSSignal renameSignal_;

	QListWidget *list_ = nullptr;
	QLabel *empty_ = nullptr;
	QToolButton *removeButton_ = nullptr;
	QToolButton *upButton_ = nullptr;
	QToolButton *downButton_ = nullptr;
	QToolButton *propertiesButton_ = nullptr;
};

} // namespace voltz
