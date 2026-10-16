#include "SourcesPanel.hpp"

#include "FrontendBridge.hpp"
#include "Icons.hpp"

#include <widgets/OBSBasic.hpp>

#include <obs-frontend-api.h>
#include <OBSApp.hpp> // QTStr

#include <QAccessible>
#include <QAccessibleWidget>
#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <map>
#include <utility>

#include "moc_SourcesPanel.cpp"

namespace voltz {

namespace {

constexpr int kRowHeight = 34;
constexpr int kCaretSlot = 20;
constexpr int kIndentPerLevel = 22;
constexpr const char *kSceneSignals[] = {"item_add", "item_remove",  "reorder",
					 "refresh",  "item_visible", "item_locked"};

QToolButton *makeIconButton(const QString &icon, const QString &hint, QWidget *parent)
{
	auto *button = new QToolButton(parent);
	button->setObjectName("iconButton");
	button->setIcon(phosphorIcon(icon, palette::kMuted));
	button->setIconSize(QSize(16, 16));
	setButtonHint(button, hint);
	return button;
}

// Acessibilidade da lista de fontes: o QListWidget expõe só as células, sem os widgets das linhas (postos com
// setItemWidget, filhos do viewport); esta interface expõe o viewport, e com ele cada linha e os seus botões.
class SourceListAccessible : public QAccessibleWidget {
public:
	explicit SourceListAccessible(QListWidget *list) : QAccessibleWidget(list, QAccessible::List) {}

	int childCount() const override { return 1; }

	QAccessibleInterface *child(int index) const override
	{
		return index == 0 ? QAccessible::queryAccessibleInterface(list()->viewport()) : nullptr;
	}

	int indexOfChild(const QAccessibleInterface *child) const override
	{
		return child && child->object() == list()->viewport() ? 0 : -1;
	}

private:
	QListWidget *list() const { return static_cast<QListWidget *>(widget()); }
};

constexpr const char *kListObjectName = "sourcesList";

QAccessibleInterface *sourceListAccessibleFactory(const QString &className, QObject *object)
{
	if (className == QLatin1String("QListWidget") && object &&
	    object->objectName() == QLatin1String(kListObjectName)) {
		return new SourceListAccessible(static_cast<QListWidget *>(object));
	}
	return nullptr;
}

bool collectItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	if (!obs_source_removed(obs_sceneitem_get_source(item))) {
		static_cast<std::vector<OBSSceneItem> *>(param)->emplace_back(item);
	}
	return true;
}

} // namespace

SourcesPanel::SourcesPanel(FrontendBridge *bridge, QWidget *parent) : QFrame(parent), bridge_(bridge)
{
	setObjectName("panel");

	auto *title = new QLabel(QTStr("Voltz.Panel.Sources"), this);
	title->setObjectName("panelTitle");
	QToolButton *addButton = makeIconButton(QStringLiteral("plus"), QTStr("Voltz.Sources.Add"), this);

	auto *header = new QHBoxLayout();
	header->setContentsMargins(0, 0, 0, 0);
	header->addWidget(title, 1);
	header->addWidget(addButton);

	static const bool factoryInstalled = []() {
		QAccessible::installFactory(sourceListAccessibleFactory);
		return true;
	}();
	(void)factoryInstalled;
	list_ = new QListWidget(this);
	/* antes de qualquer consulta de acessibilidade: a interface da lista é escolhida pelo nome */
	list_->setObjectName(kListObjectName);
	list_->setContextMenuPolicy(Qt::CustomContextMenu);
	empty_ = new QLabel(QTStr("Voltz.Sources.Empty"), this);
	empty_->setObjectName("muted");
	empty_->setWordWrap(true);

	removeButton_ = makeIconButton(QStringLiteral("trash"), QTStr("Voltz.Sources.Remove"), this);
	upButton_ = makeIconButton(QStringLiteral("caret-up"), QTStr("Voltz.Sources.MoveUp"), this);
	downButton_ = makeIconButton(QStringLiteral("caret-down"), QTStr("Voltz.Sources.MoveDown"), this);
	propertiesButton_ =
		makeIconButton(QStringLiteral("sliders-horizontal"), QTStr("Voltz.Sources.Properties"), this);

	auto *footer = new QHBoxLayout();
	footer->setContentsMargins(0, 0, 0, 0);
	footer->setSpacing(2);
	footer->addWidget(removeButton_);
	footer->addWidget(upButton_);
	footer->addWidget(downButton_);
	footer->addStretch(1);
	footer->addWidget(propertiesButton_);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(10, 10, 10, 8);
	layout->setSpacing(6);
	layout->addLayout(header);
	layout->addWidget(empty_);
	layout->addWidget(list_, 1);
	layout->addLayout(footer);

	connect(addButton, &QToolButton::clicked, this, &SourcesPanel::addSource);
	connect(removeButton_, &QToolButton::clicked, this, &SourcesPanel::removeSelected);
	connect(upButton_, &QToolButton::clicked, this, [this]() { moveSelected(OBS_ORDER_MOVE_UP); });
	connect(downButton_, &QToolButton::clicked, this, [this]() { moveSelected(OBS_ORDER_MOVE_DOWN); });
	connect(propertiesButton_, &QToolButton::clicked, this, &SourcesPanel::openProperties);
	connect(list_, &QListWidget::itemDoubleClicked, this, [this]() { openProperties(); });
	connect(list_, &QListWidget::currentRowChanged, this, [this]() { updateButtons(); });
	connect(list_, &QListWidget::customContextMenuRequested, this, &SourcesPanel::showContextMenu);
	connect(bridge_, &FrontendBridge::currentSceneChanged, this, [this]() { bindCurrentScene(); });
	connect(bridge_, &FrontendBridge::sceneListChanged, this, [this]() { bindCurrentScene(); });

	renameSignal_.Connect(obs_get_signal_handler(), "source_rename", &SourcesPanel::queueRefresh, this);
	/* Fonte removida (inclusive na limpeza da coleção de cenas, que bombeia os eventos da UI): o refresh
	 * solta os sinais e as refs dos grupos antes que o libobs destrua a fonte. */
	removeSignal_.Connect(obs_get_signal_handler(), "source_remove", &SourcesPanel::queueRefresh, this);
	bindCurrentScene();
}

void SourcesPanel::queueRefresh(void *data, calldata_t *)
{
	/* Thread do libobs: só agenda o refresh na thread da UI; uma rajada de sinais vira um refresh só. */
	static_cast<SourcesPanel *>(data)->scheduleRefresh();
}

void SourcesPanel::scheduleRefresh()
{
	if (!refreshQueued_.exchange(true)) {
		QMetaObject::invokeMethod(this, "flushRefresh", Qt::QueuedConnection);
	}
}

void SourcesPanel::flushRefresh()
{
	refreshQueued_ = false;
	refresh();
}

void SourcesPanel::bindCurrentScene()
{
	OBSSourceAutoRelease source = obs_frontend_get_current_scene();

	/* SCENE_LIST_CHANGED também chega aqui: mesma cena viva e já ligada, só redesenha. */
	OBSSource bound = OBSGetStrongRef(sceneSource_);
	if (source && bound.Get() == source.Get() && !sceneSignals_.empty()) {
		refresh();
		return;
	}

	const QString uuid = source ? QString::fromUtf8(obs_source_get_uuid(source)) : QString();
	if (uuid != boundSceneUuid_) {
		/* ids de item são por cena: o mesmo id em outra cena é outro item */
		list_->setCurrentItem(nullptr);
	}
	boundSceneUuid_ = uuid;

	sceneSignals_.clear();
	groupSignals_.clear();
	groupSources_.clear();
	sceneSource_ = OBSGetWeakRef(source);
	if (source) {
		signal_handler_t *handler = obs_source_get_signal_handler(source);
		for (const char *signal : kSceneSignals) {
			sceneSignals_.emplace_back(handler, signal, &SourcesPanel::queueRefresh, this);
		}
	}
	refresh();
}

void SourcesPanel::bindGroupSignals(const std::vector<OBSSceneItem> &groups)
{
	/* Roda na thread da UI (refresh), fora de qualquer emissão de sinal do libobs. Desliga os sinais antes
	 * de soltar as refs: o handler de um grupo removido só é liberado quando a última ref cai. */
	groupSignals_.clear();
	groupSources_.clear();
	for (const OBSSceneItem &group : groups) {
		obs_source_t *source = obs_sceneitem_get_source(group);
		groupSources_.emplace_back(source);
		signal_handler_t *handler = obs_source_get_signal_handler(source);
		for (const char *signal : kSceneSignals) {
			groupSignals_.emplace_back(handler, signal, &SourcesPanel::queueRefresh, this);
		}
	}
}

OBSScene SourcesPanel::currentScene() const
{
	OBSSource source = OBSGetStrongRef(sceneSource_);
	return source ? OBSScene(obs_scene_from_source(source)) : OBSScene();
}

OBSSceneItem SourcesPanel::findItemIn(obs_scene_t *scene, int64_t groupId, int64_t id)
{
	if (!scene) {
		return OBSSceneItem();
	}
	if (groupId < 0) {
		return OBSSceneItem(obs_scene_find_sceneitem_by_id(scene, id));
	}
	obs_sceneitem_t *group = obs_scene_find_sceneitem_by_id(scene, groupId);
	if (!group || !obs_sceneitem_is_group(group)) {
		return OBSSceneItem();
	}
	return OBSSceneItem(obs_scene_find_sceneitem_by_id(obs_sceneitem_group_get_scene(group), id));
}

OBSSceneItem SourcesPanel::findItem(int64_t groupId, int64_t id) const
{
	OBSScene scene = currentScene();
	return findItemIn(scene, groupId, id);
}

const SourceRow *SourcesPanel::selectedRow() const
{
	const int row = list_->currentRow();
	return row >= 0 && static_cast<size_t>(row) < rows_.size() ? &rows_[static_cast<size_t>(row)] : nullptr;
}

OBSSceneItem SourcesPanel::selectedItem() const
{
	const SourceRow *row = selectedRow();
	return row ? findItem(row->groupId, row->id) : OBSSceneItem();
}

void SourcesPanel::refresh()
{
	const SourceRow *previous = selectedRow();
	const int64_t selectedGroup = previous ? previous->groupId : -1;
	const int64_t selectedId = previous ? previous->id : -1;
	const int scroll = list_->verticalScrollBar()->value();

	OBSScene scene = currentScene();
	if (scene && obs_source_removed(obs_scene_get_source(scene))) {
		/* Cena removida (troca de coleção, saída): desliga os sinais enquanto a ref forte acima a mantém
		 * viva; a próxima cena chega por currentSceneChanged. */
		sceneSignals_.clear();
		scene = OBSScene();
	}

	/* Árvore da cena com refs fortes: os itens continuam válidos enquanto as linhas são montadas. */
	std::vector<SourceNode> tree;
	std::vector<OBSSceneItem> groups;
	std::map<std::pair<int64_t, int64_t>, OBSSceneItem> items;
	if (scene) {
		std::vector<OBSSceneItem> top;
		obs_scene_enum_items(scene, collectItem, &top);
		for (const OBSSceneItem &item : top) {
			SourceNode node;
			node.id = obs_sceneitem_get_id(item);
			node.isGroup = obs_sceneitem_is_group(item);
			items[{-1, node.id}] = item;
			if (node.isGroup) {
				OBSDataAutoRelease priv = obs_sceneitem_get_private_settings(item);
				node.collapsed = obs_data_get_bool(priv, "collapsed");
				groups.push_back(item);
				/* fora do enum da cena: enumerar o grupo dentro do callback travaria dois mutexes */
				std::vector<OBSSceneItem> children;
				obs_sceneitem_group_enum_items(item, collectItem, &children);
				for (const OBSSceneItem &child : children) {
					SourceNode childNode;
					childNode.id = obs_sceneitem_get_id(child);
					node.children.push_back(childNode);
					items[{node.id, childNode.id}] = child;
				}
			}
			tree.push_back(std::move(node));
		}
	}
	bindGroupSignals(groups);

	list_->clear();
	rows_ = flattenSourceTree(tree);
	const bool hasGroups = !groups.empty();
	for (const SourceRow &row : rows_) {
		auto *listItem = new QListWidgetItem(list_);
		listItem->setSizeHint(QSize(0, kRowHeight));
		auto found = items.find({row.groupId, row.id});
		if (found != items.end()) {
			list_->setItemWidget(listItem, buildRow(found->second, row, hasGroups));
		}
		if (row.groupId == selectedGroup && row.id == selectedId) {
			list_->setCurrentItem(listItem);
		}
	}

	empty_->setVisible(rows_.empty());
	list_->setVisible(!rows_.empty());
	list_->verticalScrollBar()->setValue(scroll);
	updateButtons();
}

QWidget *SourcesPanel::buildRow(obs_sceneitem_t *item, const SourceRow &row, bool reserveCaret)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const QString sourceName = QString::fromUtf8(obs_source_get_name(source));
	const int64_t groupId = row.groupId;
	const int64_t id = row.id;

	auto *widget = new QWidget(list_);
	widget->setAccessibleName(sourceName);
	auto *layout = new QHBoxLayout(widget);
	/* sem grupos na cena, a margem de antes (6); com grupos, a seta ocupa a sobra */
	layout->setContentsMargins((reserveCaret ? 2 : 6) + row.depth * kIndentPerLevel, 0, 4, 0);
	layout->setSpacing(6);

	if (row.isGroup) {
		auto *caret = new QToolButton(widget);
		caret->setObjectName("iconButton");
		caret->setIcon(phosphorIcon(
			row.collapsed ? QStringLiteral("caret-right") : QStringLiteral("caret-down"), palette::kMuted));
		caret->setIconSize(QSize(12, 12));
		caret->setFixedSize(kCaretSlot, kCaretSlot);
		setButtonHint(caret, QTStr(row.collapsed ? "Voltz.Sources.Expand" : "Voltz.Sources.Collapse"),
			      sourceName);
		connect(caret, &QToolButton::clicked, this, [this, id]() { toggleCollapsed(id); });
		layout->addWidget(caret);
	} else if (reserveCaret) {
		/* alinha o ícone com o dos grupos */
		layout->addSpacing(kCaretSlot);
	}

	auto *icon = new QLabel(widget);
	const QIcon sourceIcon = row.isGroup ? phosphorIcon(QStringLiteral("stack"), palette::kMuted)
					     : OBSBasic::Get()->GetSourceIcon(obs_source_get_id(source));
	icon->setPixmap(sourceIcon.pixmap(16, 16));
	auto *name = new QLabel(sourceName, widget);
	name->setTextInteractionFlags(Qt::NoTextInteraction);

	auto *visible = new QToolButton(widget);
	visible->setObjectName("iconButton");
	visible->setCheckable(true);
	visible->setChecked(obs_sceneitem_visible(item));
	visible->setIcon(obs_sceneitem_visible(item) ? phosphorIcon(QStringLiteral("eye"), palette::kText)
						     : phosphorIcon(QStringLiteral("eye-slash"), palette::kMuted));
	visible->setIconSize(QSize(16, 16));
	setButtonHint(visible, QTStr("Voltz.Sources.ToggleVisible"), sourceName);

	auto *lock = new QToolButton(widget);
	lock->setObjectName("iconButton");
	lock->setCheckable(true);
	lock->setChecked(obs_sceneitem_locked(item));
	lock->setIcon(obs_sceneitem_locked(item)
			      ? phosphorIcon(QStringLiteral("lock-simple"), palette::kAccent, IconWeight::Fill)
			      : phosphorIcon(QStringLiteral("lock-simple-open"), palette::kMuted));
	lock->setIconSize(QSize(16, 16));
	setButtonHint(lock, QTStr("Voltz.Sources.ToggleLock"), sourceName);

	layout->addWidget(icon);
	layout->addWidget(name, 1);
	layout->addWidget(lock);
	layout->addWidget(visible);

	/* Os botões só mudam o estado no OBS; o sinal da cena/grupo dispara refresh() e redesenha a linha. */
	connect(visible, &QToolButton::clicked, this, [this, groupId, id](bool checked) {
		if (OBSSceneItem target = findItem(groupId, id)) {
			obs_sceneitem_set_visible(target, checked);
		}
	});
	connect(lock, &QToolButton::clicked, this, [this, groupId, id](bool checked) {
		if (OBSSceneItem target = findItem(groupId, id)) {
			obs_sceneitem_set_locked(target, checked);
		}
	});
	return widget;
}

void SourcesPanel::toggleCollapsed(int64_t groupId)
{
	OBSSceneItem group = findItem(-1, groupId);
	if (!group || !obs_sceneitem_is_group(group)) {
		return;
	}
	OBSDataAutoRelease priv = obs_sceneitem_get_private_settings(group);
	obs_data_set_bool(priv, "collapsed", !obs_data_get_bool(priv, "collapsed"));
	/* Configuração privada não emite sinal: redesenha. Adiado porque o botão clicado pertence à linha
	 * que o refresh vai destruir. */
	scheduleRefresh();
}

void SourcesPanel::updateButtons()
{
	const SourceRow *row = selectedRow();
	removeButton_->setEnabled(row != nullptr);
	/* grupo não tem propriedades (só filtros, pelo menu de contexto) */
	propertiesButton_->setEnabled(row != nullptr && !row->isGroup);
	upButton_->setEnabled(row != nullptr && canMoveUp(*row));
	downButton_->setEnabled(row != nullptr && canMoveDown(*row));
}

void SourcesPanel::addSource()
{
	/* OBSBasic::AddSourceDialog só abre quando chamado por uma QAction (usa sender()). */
	if (QAction *action = OBSBasic::Get()->findChild<QAction *>(QStringLiteral("actionAddSource"))) {
		action->trigger();
	} else {
		blog(LOG_WARNING, "[voltz] acao actionAddSource nao encontrada");
	}
}

void SourcesPanel::removeSelected()
{
	/* Cena e ids capturados antes do diálogo modal: durante ele a cena atual e a seleção podem mudar. */
	OBSScene scene = currentScene();
	const SourceRow *row = selectedRow();
	if (!scene || !row) {
		return;
	}
	const int64_t groupId = row->groupId;
	const int64_t id = row->id;
	QString name;
	{
		OBSSceneItem item = findItemIn(scene, groupId, id);
		if (!item) {
			return;
		}
		name = QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item)));
	}

	const auto answer = QMessageBox::question(this, QTStr("Voltz.Sources.RemoveConfirm.Title"),
						  QTStr("Voltz.Sources.RemoveConfirm.Text").arg(name.toHtmlEscaped()));
	if (answer != QMessageBox::Yes) {
		return;
	}
	if (OBSSceneItem item = findItemIn(scene, groupId, id)) {
		obs_sceneitem_remove(item);
	}
}

void SourcesPanel::moveSelected(obs_order_movement movement)
{
	if (OBSSceneItem item = selectedItem()) {
		obs_sceneitem_set_order(item, movement);
	}
}

void SourcesPanel::openProperties()
{
	const SourceRow *row = selectedRow();
	if (!row || row->isGroup) {
		return;
	}
	if (OBSSceneItem item = selectedItem()) {
		obs_frontend_open_source_properties(obs_sceneitem_get_source(item));
	}
}

void SourcesPanel::openFilters()
{
	if (OBSSceneItem item = selectedItem()) {
		obs_frontend_open_source_filters(obs_sceneitem_get_source(item));
	}
}

void SourcesPanel::showContextMenu(const QPoint &pos)
{
	if (!list_->itemAt(pos)) {
		return;
	}
	const SourceRow *row = selectedRow();
	QMenu menu(this);
	if (!row || !row->isGroup) {
		menu.addAction(QTStr("Voltz.Sources.Properties"), this, &SourcesPanel::openProperties);
	}
	menu.addAction(QTStr("Voltz.Sources.Filters"), this, &SourcesPanel::openFilters);
	menu.addSeparator();
	menu.addAction(QTStr("Voltz.Sources.Remove"), this, &SourcesPanel::removeSelected);
	menu.exec(list_->viewport()->mapToGlobal(pos));
}

} // namespace voltz
