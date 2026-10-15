#include "VoltzShell.hpp"

#include "FrontendBridge.hpp"
#include "Icons.hpp"
#include "PresetsPage.hpp"
#include "RecordPage.hpp"
#include "ShellLocale.hpp"
#include "WindowChrome.hpp"

#include <OBSApp.hpp>
#include <widgets/OBSBasic.hpp>
#include <voltz/core/voltz-version.hpp>

#include <obs.h>

#include <QButtonGroup>
#include <QCloseEvent>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "moc_VoltzShell.cpp"

namespace voltz {

namespace {

VersionInfo currentVersion()
{
	return VersionInfo{"Voltz Studio", VOLTZ_VERSION, obs_get_version_string()};
}

QString loadStyleSheet()
{
	QFile file(QStringLiteral(":/voltz/voltz-shell.qss"));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return QString();
	}
	return QString::fromUtf8(file.readAll());
}

} // namespace

VoltzShell::VoltzShell(QWidget *parent) : QMainWindow(parent)
{
	loadShellLocale();
	loadShellFonts();
	setObjectName("voltzShell");
	/* qualificado: dentro de QMainWindow, "windowTitle" sem namespace é QWidget::windowTitle() */
	setWindowTitle(QString::fromStdString(voltz::windowTitle(currentVersion(), "")));
	setMinimumSize(1024, 640);
	resize(1280, 800);
	setStyleSheet(loadStyleSheet());

	bridge_ = new FrontendBridge(this);

	pages_ = new QStackedWidget(this);
	pages_->addWidget(new RecordPage(bridge_, pages_));
	pages_->addWidget(buildPlaceholderPage(QTStr("Voltz.Projects.Title"), QTStr("Voltz.Projects.Soon")));
	auto *presets = new PresetsPage(pages_);
	pages_->addWidget(presets);
	pages_->addWidget(buildSettingsPage());

	auto *central = new QWidget(this);
	auto *layout = new QHBoxLayout(central);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(buildSideNav());
	layout->addWidget(pages_, 1);
	setCentralWidget(central);

	connect(presets, &PresetsPage::presetApplied, this, [this]() { showPage(0); });
	connect(presets, &PresetsPage::firstRunDismissed, this, [this]() { showPage(0); });
	if (PresetsPage::isFirstRun()) {
		showPage(2);
	}

	connect(bridge_, &FrontendBridge::exiting, this, [this]() {
		exiting_ = true;
		hide();
		deleteLater();
	});

	applyDarkTitleBar(this);

	blog(LOG_INFO, "[voltz] shell pronta (%s)",
	     aboutLine(currentVersion(), QTStr("Voltz.About.BasedOn").toStdString()).c_str());
}

QWidget *VoltzShell::buildSideNav()
{
	auto *nav = new QFrame(this);
	nav->setObjectName("sideNav");
	nav->setFixedWidth(76);
	auto *layout = new QVBoxLayout(nav);
	layout->setContentsMargins(8, 12, 8, 12);
	layout->setSpacing(6);

	navGroup_ = new QButtonGroup(this);
	navGroup_->setExclusive(true);

	const QStringList labels = {QTStr("Voltz.Nav.Record"), QTStr("Voltz.Nav.Projects"), QTStr("Voltz.Nav.Presets"),
				    QTStr("Voltz.Nav.Settings")};
	const QStringList icons = {QStringLiteral("record"), QStringLiteral("folder-simple"),
				   QStringLiteral("squares-four"), QStringLiteral("gear-six")};
	for (int i = 0; i < labels.size(); i++) {
		auto *button = new QToolButton(nav);
		button->setText(labels[i]);
		button->setIcon(phosphorToggleIcon(icons[i], palette::kMuted, palette::kAccent));
		button->setIconSize(QSize(22, 22));
		button->setCheckable(true);
		button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		navGroup_->addButton(button, i);
		if (i == labels.size() - 1) {
			layout->addStretch(1);
		}
		layout->addWidget(button);
	}

	connect(navGroup_, &QButtonGroup::idClicked, this, &VoltzShell::selectPage);
	navGroup_->button(0)->setChecked(true);
	return nav;
}

QWidget *VoltzShell::buildPlaceholderPage(const QString &title, const QString &text)
{
	auto *page = new QWidget(this);
	auto *layout = new QVBoxLayout(page);
	layout->setContentsMargins(32, 28, 32, 28);
	layout->setSpacing(8);

	auto *heading = new QLabel(title, page);
	heading->setObjectName("pageTitle");
	auto *body = new QLabel(text, page);
	body->setObjectName("muted");
	body->setWordWrap(true);

	layout->addWidget(heading);
	layout->addWidget(body);
	layout->addStretch(1);
	return page;
}

QWidget *VoltzShell::buildSettingsPage()
{
	QWidget *page = buildPlaceholderPage(
		QTStr("Voltz.Settings.Title"),
		QString::fromStdString(aboutLine(currentVersion(), QTStr("Voltz.About.BasedOn").toStdString())));

	auto *classic = new QPushButton(QTStr("Voltz.Settings.OpenClassic"), page);
	connect(classic, &QPushButton::clicked, this, []() {
		/* SetShowing é slot privado do OBSBasic: religa a prévia e o texto da bandeja junto com o show */
		OBSBasic *main = OBSBasic::Get();
		QMetaObject::invokeMethod(main, "SetShowing", Q_ARG(bool, true));
		main->raise();
		main->activateWindow();
	});

	auto *layout = static_cast<QVBoxLayout *>(page->layout());
	layout->insertWidget(2, classic, 0, Qt::AlignLeft);
	return page;
}

void VoltzShell::selectPage(int index)
{
	pages_->setCurrentIndex(index);
}

void VoltzShell::showPage(int index)
{
	navGroup_->button(index)->setChecked(true);
	selectPage(index);
}

bool VoltzShell::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
	forwardDisplayEvents(this, message);
	return QMainWindow::nativeEvent(eventType, message, result);
}

void VoltzShell::changeEvent(QEvent *event)
{
	/* Trocar tema/paleta do sistema pode devolver a barra de título ao padrão: reaplica. */
	if (event->type() == QEvent::ThemeChange || event->type() == QEvent::PaletteChange) {
		applyDarkTitleBar(this);
	}
	QMainWindow::changeEvent(event);
}

void VoltzShell::closeEvent(QCloseEvent *event)
{
	if (exiting_) {
		event->accept();
		return;
	}

	/* Fechar a casca encerra o app: quem conduz o desligamento é o OBSBasic
	 * (confirmações, salvar cenas, liberar fontes). Ao final ele emite
	 * OBS_FRONTEND_EVENT_EXIT e a casca se destrói antes do obs_shutdown. */
	event->ignore();
	QMetaObject::invokeMethod(OBSBasic::Get(), "close", Qt::QueuedConnection);
}

} // namespace voltz
