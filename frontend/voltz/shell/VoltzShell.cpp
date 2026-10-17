#include "VoltzShell.hpp"

#include "FrontendBridge.hpp"
#include "Icons.hpp"
#include "MouseStyleStore.hpp"
#include "PresetBuilder.hpp"
#include "PresetsPage.hpp"
#include "ProjectsPage.hpp"
#include "RecordPage.hpp"
#include "SettingsPage.hpp"
#include "ShellLocale.hpp"
#include "WindowChrome.hpp"

#include <OBSApp.hpp>
#include <widgets/OBSBasic.hpp>
#include <widgets/OBSQTDisplay.hpp>
#include <voltz/core/voltz-version.hpp>
#include <voltz/project/ProjectRecorder.hpp>
#include <voltz/project/LiveMouse.hpp>
#include <voltz/annotate/AnnotationController.hpp>

#include <obs.h>

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "moc_VoltzShell.cpp"

namespace voltz {

namespace {

VersionInfo currentVersion()
{
	return VersionInfo{"Voltz Studio", VOLTZ_VERSION, obs_get_version_string()};
}

// "Minimizar para a bandeja" do OBS (Configurações → Geral → Bandeja), que também vale para a casca.
bool minimizeToTrayEnabled()
{
	config_t *config = App()->GetUserConfig();
	return QSystemTrayIcon::isSystemTrayAvailable() && config_get_bool(config, "BasicWindow", "SysTrayEnabled") &&
	       config_get_bool(config, "BasicWindow", "SysTrayMinimizeToTray");
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
	setStyleSheet(shellStyleSheet());

	bridge_ = new FrontendBridge(this);
	/* Criado depois da ponte: os callbacks do frontend rodam do último registrado para o primeiro
	 * (OBSStudioAPI::on_event), então o projeto já está atualizado quando a ponte avisa a casca. */
	recorder_ = new ProjectRecorder(this);
	mouseStyles_ = new MouseStyleStore(this);
	recorder_->setMouseStyleStore(mouseStyles_);
	liveMouse_ = new LiveMouse(mouseStyles_, this);
	/* Anotações (4B): só exibe; os traços vêm do fornecedor de teste até o modo desenho do 4C. */
	annotations_ = new AnnotationController(this);
	recorder_->setAnnotationController(annotations_);
	/* A coleção já está carregada (a casca nasce depois do OBSBasic::OBSInit): Tela Voltz nas cenas de antes. */
	ensureScreenGroups();
	connect(bridge_, &FrontendBridge::sceneCollectionChanged, this, []() { ensureScreenGroups(); });

	pages_ = new QStackedWidget(this);
	pages_->addWidget(new RecordPage(bridge_, recorder_, pages_));
	pages_->addWidget(new ProjectsPage(bridge_, recorder_, pages_));
	auto *presets = new PresetsPage(pages_);
	pages_->addWidget(presets);
	pages_->addWidget(new SettingsPage(bridge_, mouseStyles_, pages_));

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
		/* antes de o libobs descarregar o plugin: nada de chamar os procedimentos dele depois daqui; a sobreposição
		 * das anotações é destruída aqui */
		annotations_->shutdown();
		liveMouse_->shutdown();
		exiting_ = true;
		hide();
		deleteLater();
	});

	connect(bridge_, &FrontendBridge::recordingStateChanged, this, [this](bool active, bool paused) {
		setTaskbarState(this, !active  ? TaskbarState::Idle
				      : paused ? TaskbarState::Paused
					       : TaskbarState::Recording);
	});
	/* Com a casca em outra página, o erro só aparece na faixa da página Gravar (sem balão da bandeja,
	 * Plano 2C Task 2): leva o usuário até ela. */
	connect(bridge_, &FrontendBridge::recordingFailed, this, [this](int, const QString &) { showPage(0); });

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
	if (event->type() == QEvent::WindowStateChange) {
		if (isMinimized()) {
			setPreviewsEnabled(false);
			/* Como o OBSBasic::changeEvent/ToggleShowHide: minimizar com "minimizar para a bandeja" esconde a
			 * janela (volta pelo ícone), exceto com diálogo modal aberto. Adiado: esconder dentro da própria
			 * mudança de estado confunde o gerenciador de janelas. */
			if (minimizeToTrayEnabled() && !QApplication::activeModalWidget()) {
				QTimer::singleShot(0, this, [this]() {
					if (isMinimized()) {
						hide();
					}
				});
			}
		} else if (isVisible()) {
			setPreviewsEnabled(true);
		}
	}
	QMainWindow::changeEvent(event);
}

void VoltzShell::toggleFromTray()
{
	if (isVisible() && !isMinimized()) {
		/* Como o OBSBasic::ToggleShowHide: com diálogo modal aberto não esconde (ficaria sem janela visível
		 * por trás); só traz o diálogo para a frente. */
		if (QWidget *modal = QApplication::activeModalWidget()) {
			modal->raise();
			modal->activateWindow();
			return;
		}
		hide();
	} else {
		showFromTray();
	}
}

void VoltzShell::showFromTray()
{
	show();
	setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
	raise();
	activateWindow();
}

void VoltzShell::showEvent(QShowEvent *event)
{
	QMainWindow::showEvent(event);
	setPreviewsEnabled(!isMinimized());
	/* OBSApp::notify já aplicou a afinidade antes de entregar este evento: registra o resultado. */
	logCaptureAffinity(this);
}

void VoltzShell::hideEvent(QHideEvent *event)
{
	QMainWindow::hideEvent(event);
	setPreviewsEnabled(false);
}

void VoltzShell::setPreviewsEnabled(bool enabled)
{
	for (OBSQTDisplay *display : findChildren<OBSQTDisplay *>()) {
		if (obs_display_t *handle = display->GetDisplay()) {
			obs_display_set_enabled(handle, enabled);
		}
	}
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
