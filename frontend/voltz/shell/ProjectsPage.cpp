#include "ProjectsPage.hpp"

#include "FrontendBridge.hpp"
#include "Icons.hpp"
#include "RecordingConfig.hpp"

#include <voltz/core/project-manifest.hpp>
#include <voltz/core/recording-files.hpp>
#include <voltz/core/time-format.hpp>
#include <voltz/project/ProjectLibrary.hpp>
#include <voltz/project/ProjectRecorder.hpp>

#include <OBSApp.hpp>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
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
constexpr int kProjectRole = Qt::UserRole + 2;
constexpr int kProgramRole = Qt::UserRole + 3;
constexpr int kActiveRole = Qt::UserRole + 4;
constexpr int kRecoverableRole = Qt::UserRole + 5;
constexpr int kNewerRole = Qt::UserRole + 6;

enum Column { ColName = 0, ColDate, ColDuration, ColSize, ColStatus };

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

QString statusText(const ProjectEntry &entry, bool active)
{
	if (!entry.isProject) {
		return QString();
	}
	if (active) {
		return QTStr("Voltz.Projects.Status.Recording");
	}
	if (entry.newerFormat) {
		return QStringLiteral("—");
	}
	switch (entry.status) {
	case ProjectStatus::Ok:
		return QTStr("Voltz.Projects.Status.Ok");
	case ProjectStatus::Recording:
		return QTStr("Voltz.Projects.Status.Recoverable");
	case ProjectStatus::Recovered:
		return QTStr("Voltz.Projects.Status.Recovered");
	case ProjectStatus::Failed:
		break;
	}
	return QTStr("Voltz.Projects.Status.Failed");
}

} // namespace

ProjectsPage::ProjectsPage(FrontendBridge *bridge, ProjectRecorder *recorder, QWidget *parent)
	: QWidget(parent),
	  recorder_(recorder)
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

	recoverBanner_ = new QFrame(this);
	recoverBanner_->setObjectName("recoverBanner");
	recoverText_ = new QLabel(recoverBanner_);
	recoverText_->setWordWrap(true);
	auto *recoverAllButton = new QPushButton(QTStr("Voltz.Projects.RecoverAll"), recoverBanner_);
	recoverAllButton->setObjectName("primaryButton");
	auto *bannerLayout = new QHBoxLayout(recoverBanner_);
	bannerLayout->setContentsMargins(14, 10, 10, 10);
	bannerLayout->setSpacing(12);
	bannerLayout->addWidget(recoverText_, 1);
	bannerLayout->addWidget(recoverAllButton);
	recoverBanner_->hide();

	list_ = new QTreeWidget(this);
	list_->setObjectName("projectsList");
	list_->setColumnCount(5);
	list_->setHeaderLabels({QTStr("Voltz.Projects.Column.Name"), QTStr("Voltz.Projects.Column.Date"),
				QTStr("Voltz.Projects.Column.Duration"), QTStr("Voltz.Projects.Column.Size"),
				QTStr("Voltz.Projects.Column.Status")});
	list_->setRootIsDecorated(false);
	list_->setUniformRowHeights(true);
	list_->setSortingEnabled(true);
	list_->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);
	for (int column = ColDate; column <= ColStatus; column++) {
		list_->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
	}
	list_->headerItem()->setTextAlignment(ColDuration, Qt::AlignRight | Qt::AlignVCenter);
	list_->headerItem()->setTextAlignment(ColSize, Qt::AlignRight | Qt::AlignVCenter);

	empty_ = new QLabel(QTStr("Voltz.Projects.Empty"), this);
	empty_->setObjectName("muted");
	empty_->setAlignment(Qt::AlignCenter);

	openButton_ = new QPushButton(QTStr("Voltz.Projects.Open"), this);
	showButton_ = new QPushButton(QTStr("Voltz.Projects.ShowInFolder"), this);
	renameButton_ = new QPushButton(QTStr("Voltz.Projects.Rename"), this);
	recoverButton_ = new QPushButton(QTStr("Voltz.Projects.Recover"), this);
	trashButton_ = new QPushButton(QTStr("Voltz.Projects.Delete"), this);
	trashButton_->setIcon(phosphorIcon(QStringLiteral("trash"), palette::kText));

	auto *actions = new QHBoxLayout();
	actions->addWidget(openButton_);
	actions->addWidget(showButton_);
	actions->addWidget(renameButton_);
	actions->addWidget(recoverButton_);
	actions->addStretch(1);
	actions->addWidget(trashButton_);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(32, 28, 32, 28);
	root->setSpacing(10);
	root->addWidget(heading);
	root->addLayout(header);
	root->addWidget(recoverBanner_);
	root->addWidget(empty_, 1);
	root->addWidget(list_, 1);
	root->addLayout(actions);

	connect(refreshButton, &QPushButton::clicked, this, &ProjectsPage::refresh);
	connect(openFolder, &QPushButton::clicked, this,
		[this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(folder_)); });
	connect(openButton_, &QPushButton::clicked, this, &ProjectsPage::openSelected);
	connect(showButton_, &QPushButton::clicked, this, &ProjectsPage::showSelectedInFolder);
	connect(renameButton_, &QPushButton::clicked, this, &ProjectsPage::renameSelected);
	connect(recoverButton_, &QPushButton::clicked, this, &ProjectsPage::recoverSelected);
	connect(recoverAllButton, &QPushButton::clicked, this, &ProjectsPage::recoverAll);
	connect(trashButton_, &QPushButton::clicked, this, &ProjectsPage::trashSelected);
	connect(list_, &QTreeWidget::itemDoubleClicked, this, [this]() { openSelected(); });
	connect(list_, &QTreeWidget::itemSelectionChanged, this, &ProjectsPage::updateButtons);
	connect(recorder_, &ProjectRecorder::projectStarted, this, [this](const QString &, const QString &) {
		if (isVisible()) {
			refresh();
		}
	});
	connect(recorder_, &ProjectRecorder::projectFinished, this, [this](const QString &, const QString &) {
		if (isVisible()) {
			refresh();
		}
	});

	/* Preserva a recarga do Plano 2C: vídeo solto (gravação sem projeto) também aparece ao terminar. */
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
	folder_ = RecordingConfig::outputFolder();
	folderLabel_->setText(QTStr("Voltz.Projects.Folder").arg(QDir::toNativeSeparators(folder_)));

	QTreeWidgetItem *previous = selectedItem();
	const QString selected = previous ? previous->data(ColName, kPathRole).toString() : QString();
	const QDir activeDir(recorder_->isActive() ? recorder_->currentProjectDir() : QString());
	const bool hasActive = recorder_->isActive();

	list_->setSortingEnabled(false);
	list_->clear();
	const char separator = QLocale().decimalPoint().at(0).toLatin1();
	int recoverable = 0;
	for (const ProjectEntry &entry : ProjectLibrary::scan(folder_)) {
		const bool active = hasActive && entry.isProject && QDir(entry.path) == activeDir;
		const bool canRecover = entry.isProject && !active && !entry.newerFormat &&
					entry.status == ProjectStatus::Recording;
		auto *item = new SortableItem(list_);
		item->setText(ColName, entry.name);
		item->setIcon(ColName, phosphorIcon(entry.isProject ? QStringLiteral("folder-simple")
								    : QStringLiteral("film-strip"),
						    palette::kMuted));
		item->setData(ColName, kPathRole, entry.path);
		item->setData(ColName, kProjectRole, entry.isProject);
		item->setData(ColName, kProgramRole, entry.programFile);
		item->setData(ColName, kActiveRole, active);
		item->setData(ColName, kRecoverableRole, canRecover);
		item->setData(ColName, kNewerRole, entry.newerFormat);
		if (entry.newerFormat) {
			item->setToolTip(ColName, QTStr("Voltz.Projects.NewerFormat"));
		}
		item->setText(ColDate, QLocale().toString(entry.date.toLocalTime(), QLocale::ShortFormat));
		item->setData(ColDate, kSortRole, entry.date.toMSecsSinceEpoch());
		/* Formato mais novo: duração e status não são confiáveis para esta versão (mostra "—" nos dois). */
		const qint64 durationMs = entry.newerFormat ? -1 : entry.durationMs;
		item->setText(ColDuration,
			      durationMs >= 0 ? QString::fromStdString(formatClock(durationMs)) : QStringLiteral("—"));
		item->setData(ColDuration, kSortRole, durationMs);
		item->setTextAlignment(ColDuration, Qt::AlignRight | Qt::AlignVCenter);
		item->setText(ColSize, QString::fromStdString(formatFileSize(entry.sizeBytes, separator)));
		item->setData(ColSize, kSortRole, static_cast<qlonglong>(entry.sizeBytes));
		item->setTextAlignment(ColSize, Qt::AlignRight | Qt::AlignVCenter);
		item->setText(ColStatus, statusText(entry, active));
		if (canRecover) {
			recoverable++;
			item->setIcon(ColStatus, phosphorIcon(QStringLiteral("warning-circle"), palette::kAccent));
		} else if (entry.isProject && !active && !entry.newerFormat && entry.status == ProjectStatus::Failed) {
			item->setIcon(ColStatus, phosphorIcon(QStringLiteral("warning-circle"), palette::kRec));
		}
		if (entry.path == selected) {
			list_->setCurrentItem(item);
		}
	}
	list_->setSortingEnabled(true);
	list_->sortByColumn(ColDate, Qt::DescendingOrder);

	const bool hasItems = list_->topLevelItemCount() > 0;
	empty_->setVisible(!hasItems);
	list_->setVisible(hasItems);
	recoverText_->setText(recoverable == 1 ? QTStr("Voltz.Projects.RecoverBanner.One")
					       : QTStr("Voltz.Projects.RecoverBanner.Many").arg(recoverable));
	recoverBanner_->setVisible(recoverable > 0);
	updateButtons();
}

QTreeWidgetItem *ProjectsPage::selectedItem() const
{
	const QList<QTreeWidgetItem *> items = list_->selectedItems();
	return items.isEmpty() ? nullptr : items.first();
}

void ProjectsPage::updateButtons()
{
	QTreeWidgetItem *item = selectedItem();
	const bool has = item != nullptr;
	const bool project = has && item->data(ColName, kProjectRole).toBool();
	const bool active = has && item->data(ColName, kActiveRole).toBool();
	const bool newer = has && item->data(ColName, kNewerRole).toBool();
	openButton_->setEnabled(has);
	showButton_->setEnabled(has);
	renameButton_->setEnabled(project && !active && !newer);
	recoverButton_->setEnabled(has && item->data(ColName, kRecoverableRole).toBool());
	trashButton_->setEnabled(has && !active);
}

void ProjectsPage::openSelected()
{
	QTreeWidgetItem *item = selectedItem();
	if (!item) {
		return;
	}
	const QString program = item->data(ColName, kProgramRole).toString();
	const QString target =
		!program.isEmpty() && QFileInfo::exists(program) ? program : item->data(ColName, kPathRole).toString();
	QDesktopServices::openUrl(QUrl::fromLocalFile(target));
}

void ProjectsPage::showSelectedInFolder()
{
	QTreeWidgetItem *item = selectedItem();
	if (!item) {
		return;
	}
	const QString path = item->data(ColName, kPathRole).toString();
	if (item->data(ColName, kProjectRole).toBool()) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
		return;
	}
#ifdef _WIN32
	QProcess::startDetached(QStringLiteral("explorer.exe"),
				{QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#else
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void ProjectsPage::renameSelected()
{
	QTreeWidgetItem *item = selectedItem();
	if (!item || !item->data(ColName, kProjectRole).toBool() || item->data(ColName, kActiveRole).toBool()) {
		return;
	}
	/* O diálogo abre um laço de eventos: se a gravação começar/parar (atalho, websocket), refresh() recria a lista
	 * e `item` deixa de existir. Tudo o que vem do item é lido antes. */
	const QString current = item->text(ColName);
	const QString path = item->data(ColName, kPathRole).toString();
	bool ok = false;
	const QString name = QInputDialog::getText(this, QTStr("Voltz.Projects.Rename.Title"),
						   QTStr("Voltz.Projects.Rename.Label"), QLineEdit::Normal, current,
						   &ok)
				     .trimmed();
	if (!ok || name.isEmpty() || name == current) {
		return;
	}
	if (!ProjectLibrary::rename(path, name)) {
		QMessageBox::warning(this, QTStr("Voltz.Projects.Rename.Title"), QTStr("Voltz.Projects.RenameFailed"));
	}
	refresh();
}

void ProjectsPage::recoverSelected()
{
	QTreeWidgetItem *item = selectedItem();
	if (!item || !item->data(ColName, kRecoverableRole).toBool()) {
		return;
	}
	const QString name = item->text(ColName);
	if (!ProjectLibrary::recover(item->data(ColName, kPathRole).toString())) {
		QMessageBox::warning(this, QTStr("Voltz.Projects.Recover"),
				     QTStr("Voltz.Projects.RecoverFailed").arg(name.toHtmlEscaped()));
	}
	refresh();
}

void ProjectsPage::recoverAll()
{
	QStringList failed;
	for (int i = 0; i < list_->topLevelItemCount(); i++) {
		QTreeWidgetItem *item = list_->topLevelItem(i);
		if (!item->data(ColName, kRecoverableRole).toBool()) {
			continue;
		}
		if (!ProjectLibrary::recover(item->data(ColName, kPathRole).toString())) {
			failed.append(item->text(ColName));
		}
	}
	refresh();
	if (!failed.isEmpty()) {
		QMessageBox::warning(
			this, QTStr("Voltz.Projects.Recover"),
			QTStr("Voltz.Projects.RecoverFailed").arg(failed.join(QStringLiteral(", ")).toHtmlEscaped()));
	}
}

void ProjectsPage::trashSelected()
{
	QTreeWidgetItem *item = selectedItem();
	if (!item || item->data(ColName, kActiveRole).toBool()) {
		return;
	}
	const QString path = item->data(ColName, kPathRole).toString();
	const QString name = item->text(ColName).toHtmlEscaped();
	const QString question = item->data(ColName, kProjectRole).toBool()
					 ? QTStr("Voltz.Projects.DeleteProjectConfirm.Text").arg(name)
					 : QTStr("Voltz.Projects.DeleteConfirm.Text").arg(name);
	if (QMessageBox::question(this, QTStr("Voltz.Projects.DeleteConfirm.Title"), question) != QMessageBox::Yes) {
		return;
	}
	/* QFile::moveToTrash usa o IFileOperation no Windows e aceita pasta: o projeto vai inteiro para a lixeira. */
	if (!QFile::moveToTrash(path)) {
		QMessageBox::warning(this, QTStr("Voltz.Projects.DeleteConfirm.Title"),
				     QTStr("Voltz.Projects.DeleteFailed"));
	}
	refresh();
}

} // namespace voltz
