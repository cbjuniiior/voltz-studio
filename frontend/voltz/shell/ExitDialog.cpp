#include "ExitDialog.hpp"

#include "Icons.hpp"
#include "WindowChrome.hpp"

#include <OBSApp.hpp> // QTStr

#include <obs.h>

#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace voltz {

namespace {

// Pai do diálogo: a janela pedida, se visível; senão a casca visível (a OBSBasic fica oculta e um diálogo
// centrado nela apareceria num lugar qualquer).
QWidget *resolveParent(QWidget *requested)
{
	if (requested && requested->isVisible()) {
		return requested->window();
	}
	for (QWidget *widget : QApplication::topLevelWidgets()) {
		if (widget->objectName() == QLatin1String("voltzShell") && widget->isVisible()) {
			return widget;
		}
	}
	return requested;
}

} // namespace

bool confirmExitWhileActive(QWidget *parent, bool recording)
{
	QDialog dialog(resolveParent(parent));
	/* objectName + QSS próprios: as regras "#voltzShell ..." valem mesmo sem a casca como ancestral. */
	dialog.setObjectName("voltzShell");
	dialog.setStyleSheet(shellStyleSheet());
	dialog.setWindowTitle(QTStr("Voltz.Exit.WindowTitle"));
	dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
	dialog.setMinimumWidth(480);

	auto *icon = new QLabel(&dialog);
	icon->setPixmap(phosphorIcon(QStringLiteral("warning-circle"), palette::kRec, IconWeight::Fill).pixmap(28, 28));

	auto *title = new QLabel(QTStr(recording ? "Voltz.Exit.Recording.Title" : "Voltz.Exit.Active.Title"), &dialog);
	title->setObjectName("dialogTitle");
	auto *text = new QLabel(QTStr(recording ? "Voltz.Exit.Recording.Text" : "Voltz.Exit.Active.Text"), &dialog);
	text->setObjectName("muted");
	text->setWordWrap(true);

	auto *texts = new QVBoxLayout();
	texts->setSpacing(6);
	texts->addWidget(title);
	texts->addWidget(text);

	auto *top = new QHBoxLayout();
	top->setSpacing(14);
	top->addWidget(icon, 0, Qt::AlignTop);
	top->addLayout(texts, 1);

	auto *cancel = new QPushButton(QTStr("Voltz.Exit.Cancel"), &dialog);
	auto *quit = new QPushButton(QTStr(recording ? "Voltz.Exit.StopAndQuit" : "Voltz.Exit.Quit"), &dialog);
	quit->setObjectName("dangerButton");
	/* Enter cancela: sair sem querer encerraria a gravação. */
	quit->setAutoDefault(false);
	cancel->setDefault(true);
	QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
	QObject::connect(quit, &QPushButton::clicked, &dialog, &QDialog::accept);

	auto *buttons = new QHBoxLayout();
	buttons->setSpacing(8);
	buttons->addStretch(1);
	buttons->addWidget(cancel);
	buttons->addWidget(quit);

	auto *layout = new QVBoxLayout(&dialog);
	layout->setContentsMargins(22, 20, 22, 18);
	layout->setSpacing(18);
	layout->addLayout(top);
	layout->addLayout(buttons);

	applyDarkTitleBar(&dialog);
	const bool accepted = dialog.exec() == QDialog::Accepted;
	blog(LOG_INFO, "[voltz] confirmacao de saida: %s", accepted ? "sair" : "cancelar");
	return accepted;
}

void applyShellStyle(QMessageBox *box, QPushButton *primary)
{
	loadShellFonts();
	/* objectName antes do QSS: setStyleSheet pole os filhos agora e não repole quando o nome muda depois. */
	if (primary) {
		primary->setObjectName("primaryButton");
	}
	box->setObjectName("voltzShell");
	box->setStyleSheet(shellStyleSheet());
	box->setIconPixmap(
		phosphorIcon(QStringLiteral("warning-circle"), palette::kRec, IconWeight::Fill).pixmap(28, 28));
	applyDarkTitleBar(box);
}

} // namespace voltz
