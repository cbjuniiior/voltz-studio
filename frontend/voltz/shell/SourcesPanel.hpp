#pragma once

#include <voltz/core/source-tree.hpp>

#include <obs.hpp>

#include <QFrame>
#include <QString>

#include <atomic>
#include <vector>

class QLabel;
class QListWidget;
class QToolButton;

namespace voltz {

class FrontendBridge;

// Painel "FONTES": fontes da cena atual — grupos com os filhos (expandir/recolher) — com mostrar/ocultar,
// travar, ordem, adicionar, remover, propriedades e filtros.
class SourcesPanel : public QFrame {
	Q_OBJECT

public:
	explicit SourcesPanel(FrontendBridge *bridge, QWidget *parent = nullptr);

private slots:
	void flushRefresh();

private:
	void refresh();
	void bindCurrentScene();
	// Liga os sinais de cada grupo: mudanças nos filhos são emitidas pela fonte do grupo, não pela cena.
	void bindGroupSignals(const std::vector<OBSSceneItem> &groups);
	OBSScene currentScene() const;
	// Item de uma linha na cena dada: groupId < 0 = nível da cena; senão, filho do grupo com esse id.
	static OBSSceneItem findItemIn(obs_scene_t *scene, int64_t groupId, int64_t id);
	OBSSceneItem findItem(int64_t groupId, int64_t id) const;
	const SourceRow *selectedRow() const;
	OBSSceneItem selectedItem() const;
	QWidget *buildRow(obs_sceneitem_t *item, const SourceRow &row, bool reserveCaret);
	void toggleCollapsed(int64_t groupId);
	void scheduleRefresh();
	void updateButtons();
	void addSource();
	void removeSelected();
	void moveSelected(obs_order_movement movement);
	void openProperties();
	void openFilters();
	void showContextMenu(const QPoint &pos);

	// Thread do libobs: agenda um único flushRefresh() na thread da UI por rajada de sinais.
	static void queueRefresh(void *data, calldata_t *params);

	FrontendBridge *bridge_;
	OBSWeakSource sceneSource_;
	// ids de item são por cena (e por grupo): trocar de cena descarta a seleção.
	QString boundSceneUuid_;
	std::atomic<bool> refreshQueued_{false};
	std::vector<OBSSignal> sceneSignals_;
	// Refs fortes das fontes dos grupos ligados: o signal_handler de um grupo removido só é liberado depois
	// que os sinais forem desligados. Declarado antes de groupSignals_ (destruído depois deles).
	std::vector<OBSSource> groupSources_;
	std::vector<OBSSignal> groupSignals_;
	OBSSignal renameSignal_;
	OBSSignal removeSignal_;
	// Uma entrada por linha da lista, na mesma ordem.
	std::vector<SourceRow> rows_;

	QListWidget *list_ = nullptr;
	QLabel *empty_ = nullptr;
	QToolButton *removeButton_ = nullptr;
	QToolButton *upButton_ = nullptr;
	QToolButton *downButton_ = nullptr;
	QToolButton *propertiesButton_ = nullptr;
};

} // namespace voltz
