#include "ProjectsPage.hpp"

#include "FrontendBridge.hpp"
#include "Icons.hpp"

#include <voltz/core/recording-files.hpp>

#include <OBSApp.hpp>
#include <widgets/OBSBasic.hpp>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "moc_ProjectsPage.cpp"

namespace voltz {

namespace {

constexpr int kPathRole = Qt::UserRole;
constexpr int kSortRole = Qt::UserRole + 1;

class SortableItem : public QTreeWidgetItem {
public:
	using QTreeWidgetItem::QTreeWidgetItem;
	bool operator<(const QTreeWidgetItem &other) const override
	{
		const int column = treeWidget() ? treeWidget()->sortColumn() : 0;
		const QVariant a = data(column, kSortRole);
		const QVariant b = other.data(column, kSortRole);
		if (a.isValid() && b.isValid()) {
			return a.toLongLong() < b.toLongLong();
		}
		return QTreeWidgetItem::operator<(other);
	}
};

} // namespace

ProjectsPage::ProjectsPage(FrontendBridge *bridge, QWidget *parent) : QWidget(parent)
{
	auto *heading = new QLabel(QTStr("Voltz.Projects.Title"), this);
	heading->setObjectName("pageTitle");
	folderLabel_ = new QLabel(this);
	folderLabel_->setObjectName("muted");
	folderLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

	auto *openFolder = new QPushButton(QTStr("Voltz.Projects.OpenFolder"), this);
	openFolder->setIcon(phosphorIcon(QStringLiteral("folder-simple"), palette::kText));
	auto *refreshButton = new QPushButton(QTStr("Voltz.Projects.Refresh"), this);

	auto *header = new QHBoxLayout();
	header->addWidget(folderLabel_, 1);
	header->addWidget(refreshButton);
	header->addWidget(openFolder);

	list_ = new QTreeWidget(this);
	list_->setObjectName("projectsList");
	list_->setColumnCount(3);
	list_->setHeaderLabels({QTStr("Voltz.Projects.Column.Name"), QTStr("Voltz.Projects.Column.Date"),
				QTStr("Voltz.Projects.Column.Size")});
	list_->setRootIsDecorated(false);
	list_->setUniformRowHeights(true);
	list_->setSortingEnabled(true);
	list_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	list_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	list_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

	empty_ = new QLabel(QTStr("Voltz.Projects.Empty"), this);
	empty_->setObjectName("muted");
	empty_->setAlignment(Qt::AlignCenter);

	openButton_ = new QPushButton(QTStr("Voltz.Projects.Open"), this);
	showButton_ = new QPushButton(QTStr("Voltz.Projects.ShowInFolder"), this);
	trashButton_ = new QPushButton(QTStr("Voltz.Projects.Delete"), this);
	trashButton_->setIcon(phosphorIcon(QStringLiteral("trash"), palette::kText));

	auto *actions = new QHBoxLayout();
	actions->addWidget(openButton_);
	actions->addWidget(showButton_);
	actions->addStretch(1);
	actions->addWidget(trashButton_);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(32, 28, 32, 28);
	root->setSpacing(10);
	root->addWidget(heading);
	root->addLayout(header);
	root->addWidget(empty_, 1);
	root->addWidget(list_, 1);
	root->addLayout(actions);

	connect(refreshButton, &QPushButton::clicked, this, &ProjectsPage::refresh);
	connect(openFolder, &QPushButton::clicked, this,
		[this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(folder_)); });
	connect(openButton_, &QPushButton::clicked, this, &ProjectsPage::openSelected);
	connect(showButton_, &QPushButton::clicked, this, &ProjectsPage::showSelectedInFolder);
	connect(trashButton_, &QPushButton::clicked, this, &ProjectsPage::trashSelected);
	connect(list_, &QTreeWidget::itemDoubleClicked, this, [this]() { openSelected(); });
	connect(list_, &QTreeWidget::itemSelectionChanged, this, &ProjectsPage::updateButtons);
	connect(bridge, &FrontendBridge::recordingStateChanged, this, [this](bool active, bool) {
		if (!active && isVisible()) {
			refresh();
		}
	});

	refresh();
}

void ProjectsPage::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	refresh();
}

void ProjectsPage::refresh()
{
	const char *outputPath = OBSBasic::Get()->GetCurrentOutputPath();
	folder_ = QString::fromUtf8(outputPath ? outputPath : "");
	folderLabel_->setText(QTStr("Voltz.Projects.Folder").arg(QDir::toNativeSeparators(folder_)));

	const QString selected = selectedPath();
	list_->setSortingEnabled(false);
	list_->clear();

	const QChar separator = QLocale().decimalPoint().at(0);
	const QFileInfoList files = QDir(folder_).entryInfoList(QDir::Files, QDir::Time);
	for (const QFileInfo &file : files) {
		if (!isRecordingFile(file.fileName().toStdString())) {
			continue;
		}
		auto *item = new SortableItem(list_);
		item->setText(0, file.fileName());
		item->setIcon(0, phosphorIcon(QStringLiteral("film-strip"), palette::kMuted));
		item->setData(0, kPathRole, file.absoluteFilePath());
		const QDateTime modified = file.lastModified();
		item->setText(1, QLocale().toString(modified, QLocale::ShortFormat));
		item->setData(1, kSortRole, modified.toMSecsSinceEpoch());
		item->setText(2, QString::fromStdString(formatFileSize(uint64_t(file.size()), separator.toLatin1())));
		item->setData(2, kSortRole, file.size());
		item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
		if (file.absoluteFilePath() == selected) {
			list_->setCurrentItem(item);
		}
	}
	list_->setSortingEnabled(true);
	list_->sortByColumn(1, Qt::DescendingOrder);

	const bool hasFiles = list_->topLevelItemCount() > 0;
	empty_->setVisible(!hasFiles);
	list_->setVisible(hasFiles);
	updateButtons();
}

QString ProjectsPage::selectedPath() const
{
	const QList<QTreeWidgetItem *> items = list_->selectedItems();
	return items.isEmpty() ? QString() : items.first()->data(0, kPathRole).toString();
}

void ProjectsPage::updateButtons()
{
	const bool hasSelection = !selectedPath().isEmpty();
	openButton_->setEnabled(hasSelection);
	showButton_->setEnabled(hasSelection);
	trashButton_->setEnabled(hasSelection);
}

void ProjectsPage::openSelected()
{
	const QString path = selectedPath();
	if (!path.isEmpty()) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
	}
}

void ProjectsPage::showSelectedInFolder()
{
	const QString path = selectedPath();
	if (path.isEmpty()) {
		return;
	}
#ifdef _WIN32
	QProcess::startDetached(QStringLiteral("explorer.exe"),
				{QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#else
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void ProjectsPage::trashSelected()
{
	const QString path = selectedPath();
	if (path.isEmpty()) {
		return;
	}
	const QString name = QFileInfo(path).fileName();
	if (QMessageBox::question(this, QTStr("Voltz.Projects.DeleteConfirm.Title"),
				  QTStr("Voltz.Projects.DeleteConfirm.Text").arg(name.toHtmlEscaped())) !=
	    QMessageBox::Yes) {
		return;
	}
	if (!QFile::moveToTrash(path)) {
		QMessageBox::warning(this, QTStr("Voltz.Projects.DeleteConfirm.Title"),
				     QTStr("Voltz.Projects.DeleteFailed"));
	}
	refresh();
}

} // namespace voltz
