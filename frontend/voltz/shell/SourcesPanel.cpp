#include "SourcesPanel.hpp"

#include "FrontendBridge.hpp"
#include "Icons.hpp"

#include <widgets/OBSBasic.hpp>

#include <obs-frontend-api.h>
#include <OBSApp.hpp> // QTStr

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <vector>

#include "moc_SourcesPanel.cpp"

namespace voltz {

namespace {

constexpr int kItemIdRole = Qt::UserRole;

QToolButton *makeIconButton(const QString &icon, const QString &tooltip, QWidget *parent)
{
	auto *button = new QToolButton(parent);
	button->setObjectName("iconButton");
	button->setIcon(phosphorIcon(icon, palette::kMuted));
	button->setIconSize(QSize(16, 16));
	button->setToolTip(tooltip);
	return button;
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

	list_ = new QListWidget(this);
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

	renameSignal_.Connect(obs_get_signal_handler(), "source_rename", &SourcesPanel::handleSceneSignal, this);
	bindCurrentScene();
}

void SourcesPanel::handleSceneSignal(void *data, calldata_t *)
{
	/* Thread do libobs: só agenda o refresh na thread da UI. */
	QMetaObject::invokeMethod(static_cast<SourcesPanel *>(data), "refresh", Qt::QueuedConnection);
}

void SourcesPanel::bindCurrentScene()
{
	sceneSignals_.clear();
	OBSSourceAutoRelease source = obs_frontend_get_current_scene();
	sceneSource_ = OBSGetWeakRef(source);
	if (source) {
		signal_handler_t *handler = obs_source_get_signal_handler(source);
		for (const char *signal :
		     {"item_add", "item_remove", "reorder", "refresh", "item_visible", "item_locked"}) {
			sceneSignals_.emplace_back(handler, signal, &SourcesPanel::handleSceneSignal, this);
		}
	}
	refresh();
}

obs_scene_t *SourcesPanel::currentScene() const
{
	OBSSource source = OBSGetStrongRef(sceneSource_);
	return source ? obs_scene_from_source(source) : nullptr;
}

obs_sceneitem_t *SourcesPanel::selectedItem() const
{
	obs_scene_t *scene = currentScene();
	QListWidgetItem *row = list_->currentItem();
	if (!scene || !row) {
		return nullptr;
	}
	return obs_scene_find_sceneitem_by_id(scene, row->data(kItemIdRole).toLongLong());
}

void SourcesPanel::refresh()
{
	const qint64 selectedId = list_->currentItem() ? list_->currentItem()->data(kItemIdRole).toLongLong() : -1;
	list_->clear();

	std::vector<obs_sceneitem_t *> items;
	if (obs_scene_t *scene = currentScene()) {
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				static_cast<std::vector<obs_sceneitem_t *> *>(param)->push_back(item);
				return true;
			},
			&items);
	}

	/* enum vai de baixo para cima; a lista mostra a camada de cima primeiro */
	for (auto it = items.rbegin(); it != items.rend(); ++it) {
		auto *row = new QListWidgetItem(list_);
		const qint64 id = obs_sceneitem_get_id(*it);
		row->setData(kItemIdRole, id);
		row->setSizeHint(QSize(0, 34));
		list_->setItemWidget(row, buildRow(*it));
		if (id == selectedId) {
			list_->setCurrentItem(row);
		}
	}

	empty_->setVisible(items.empty());
	list_->setVisible(!items.empty());
	updateButtons();
}

QWidget *SourcesPanel::buildRow(obs_sceneitem_t *item)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const qint64 id = obs_sceneitem_get_id(item);

	auto *row = new QWidget(list_);
	auto *layout = new QHBoxLayout(row);
	layout->setContentsMargins(6, 0, 4, 0);
	layout->setSpacing(6);

	auto *icon = new QLabel(row);
	icon->setPixmap(OBSBasic::Get()->GetSourceIcon(obs_source_get_id(source)).pixmap(16, 16));
	auto *name = new QLabel(QString::fromUtf8(obs_source_get_name(source)), row);
	name->setTextInteractionFlags(Qt::NoTextInteraction);

	auto *visible = new QToolButton(row);
	visible->setObjectName("iconButton");
	visible->setCheckable(true);
	visible->setChecked(obs_sceneitem_visible(item));
	visible->setIcon(obs_sceneitem_visible(item) ? phosphorIcon(QStringLiteral("eye"), palette::kText)
						     : phosphorIcon(QStringLiteral("eye-slash"), palette::kMuted));
	visible->setIconSize(QSize(16, 16));
	visible->setToolTip(QTStr("Voltz.Sources.ToggleVisible"));

	auto *lock = new QToolButton(row);
	lock->setObjectName("iconButton");
	lock->setCheckable(true);
	lock->setChecked(obs_sceneitem_locked(item));
	lock->setIcon(obs_sceneitem_locked(item)
			      ? phosphorIcon(QStringLiteral("lock-simple"), palette::kAccent, IconWeight::Fill)
			      : phosphorIcon(QStringLiteral("lock-simple-open"), palette::kMuted));
	lock->setIconSize(QSize(16, 16));
	lock->setToolTip(QTStr("Voltz.Sources.ToggleLock"));

	layout->addWidget(icon);
	layout->addWidget(name, 1);
	layout->addWidget(lock);
	layout->addWidget(visible);

	/* Os botões só mudam o estado no OBS; o sinal da cena dispara refresh() e redesenha a linha. */
	connect(visible, &QToolButton::clicked, this, [this, id](bool checked) {
		if (obs_scene_t *scene = currentScene()) {
			if (obs_sceneitem_t *target = obs_scene_find_sceneitem_by_id(scene, id)) {
				obs_sceneitem_set_visible(target, checked);
			}
		}
	});
	connect(lock, &QToolButton::clicked, this, [this, id](bool checked) {
		if (obs_scene_t *scene = currentScene()) {
			if (obs_sceneitem_t *target = obs_scene_find_sceneitem_by_id(scene, id)) {
				obs_sceneitem_set_locked(target, checked);
			}
		}
	});
	return row;
}

void SourcesPanel::updateButtons()
{
	const int row = list_->currentRow();
	const bool hasSelection = row >= 0;
	removeButton_->setEnabled(hasSelection);
	propertiesButton_->setEnabled(hasSelection);
	upButton_->setEnabled(hasSelection && row > 0);
	downButton_->setEnabled(hasSelection && row < list_->count() - 1);
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
	obs_sceneitem_t *item = selectedItem();
	if (!item) {
		return;
	}
	const QString name = QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item)));
	const auto answer = QMessageBox::question(this, QTStr("Voltz.Sources.RemoveConfirm.Title"),
						  QTStr("Voltz.Sources.RemoveConfirm.Text").arg(name));
	if (answer != QMessageBox::Yes) {
		return;
	}
	if (obs_sceneitem_t *again = selectedItem()) {
		obs_sceneitem_remove(again);
	}
}

void SourcesPanel::moveSelected(obs_order_movement movement)
{
	if (obs_sceneitem_t *item = selectedItem()) {
		obs_sceneitem_set_order(item, movement);
	}
}

void SourcesPanel::openProperties()
{
	if (obs_sceneitem_t *item = selectedItem()) {
		obs_frontend_open_source_properties(obs_sceneitem_get_source(item));
	}
}

void SourcesPanel::openFilters()
{
	if (obs_sceneitem_t *item = selectedItem()) {
		obs_frontend_open_source_filters(obs_sceneitem_get_source(item));
	}
}

void SourcesPanel::showContextMenu(const QPoint &pos)
{
	if (!list_->itemAt(pos)) {
		return;
	}
	QMenu menu(this);
	menu.addAction(QTStr("Voltz.Sources.Properties"), this, &SourcesPanel::openProperties);
	menu.addAction(QTStr("Voltz.Sources.Filters"), this, &SourcesPanel::openFilters);
	menu.addSeparator();
	menu.addAction(QTStr("Voltz.Sources.Remove"), this, &SourcesPanel::removeSelected);
	menu.exec(list_->viewport()->mapToGlobal(pos));
}

} // namespace voltz
