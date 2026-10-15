#include "ScenesPanel.hpp"

#include "FrontendBridge.hpp"

#include <OBSApp.hpp> // QTStr

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

#include "moc_ScenesPanel.cpp"

namespace voltz {

ScenesPanel::ScenesPanel(FrontendBridge *bridge, QWidget *parent) : QFrame(parent), bridge_(bridge)
{
	setObjectName("panel");

	auto *title = new QLabel(QTStr("Voltz.Panel.Scenes"), this);
	title->setObjectName("panelTitle");
	list_ = new QListWidget(this);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(10, 10, 10, 10);
	layout->setSpacing(8);
	layout->addWidget(title);
	layout->addWidget(list_, 1);

	connect(list_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
		if (!updating_ && current) {
			bridge_->setCurrentSceneByUuid(current->data(Qt::UserRole).toString());
		}
	});
	connect(bridge_, &FrontendBridge::sceneListChanged, this, &ScenesPanel::refresh);
	connect(bridge_, &FrontendBridge::currentSceneChanged, this, &ScenesPanel::selectScene);

	refresh();
}

void ScenesPanel::refresh()
{
	updating_ = true;
	list_->clear();
	for (const SceneEntry &scene : bridge_->scenes()) {
		auto *item = new QListWidgetItem(scene.name, list_);
		item->setData(Qt::UserRole, scene.uuid);
	}
	updating_ = false;
	selectScene(bridge_->currentSceneUuid());
}

void ScenesPanel::selectScene(const QString &uuid)
{
	updating_ = true;
	QListWidgetItem *match = nullptr;
	for (int i = 0; i < list_->count(); i++) {
		if (list_->item(i)->data(Qt::UserRole).toString() == uuid) {
			match = list_->item(i);
			break;
		}
	}
	if (match) {
		list_->setCurrentItem(match);
	} else {
		list_->clearSelection();
		list_->setCurrentItem(nullptr);
	}
	updating_ = false;
}

} // namespace voltz
