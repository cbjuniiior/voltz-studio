#include "RecordPage.hpp"

#include "ErrorBanner.hpp"
#include "FrontendBridge.hpp"
#include "Icons.hpp"
#include "ScenesPanel.hpp"
#include "VoltzPreview.hpp"

#include <voltz/core/recording-error.hpp>
#include <voltz/core/time-format.hpp>

#include <OBSApp.hpp> // QTStr

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include "moc_RecordPage.cpp"

namespace voltz {

RecordPage::RecordPage(FrontendBridge *bridge, QWidget *parent) : QWidget(parent), bridge_(bridge)
{
	monotonic_.start();

	scenes_ = new ScenesPanel(bridge_, this);
	scenes_->setFixedWidth(200);
	preview_ = new VoltzPreview(this);

	centerColumn_ = new QVBoxLayout();
	centerColumn_->setContentsMargins(0, 0, 0, 0);
	centerColumn_->setSpacing(12);
	errorBanner_ = new ErrorBanner(this);
	centerColumn_->addWidget(errorBanner_);
	centerColumn_->addWidget(preview_, 1);

	auto *sideHolder = new QWidget(this);
	sideHolder->setFixedWidth(280);
	sideColumn_ = new QVBoxLayout(sideHolder);
	sideColumn_->setContentsMargins(0, 0, 0, 0);
	sideColumn_->setSpacing(12);
	auto *sourcesHint = new QLabel(QStringLiteral("Fontes e áudio chegam no próximo plano."), sideHolder);
	sourcesHint->setObjectName("muted");
	sourcesHint->setWordWrap(true);
	sideColumn_->addWidget(sourcesHint);
	sideColumn_->addStretch(1);

	auto *body = new QHBoxLayout();
	body->setContentsMargins(12, 12, 12, 12);
	body->setSpacing(12);
	body->addWidget(scenes_);
	body->addLayout(centerColumn_, 1);
	body->addWidget(sideHolder);

	pauseButton_ = new QPushButton(this);
	pauseButton_->setIconSize(QSize(16, 16));
	recordButton_ = new QPushButton(this);
	recordButton_->setObjectName("recordButton");
	recordButton_->setIconSize(QSize(16, 16));
	timeLabel_ = new QLabel(QString::fromStdString(formatClock(0)), this);
	timeLabel_->setObjectName("timeLabel");

	auto *bottomBar = new QFrame(this);
	bottomBar->setObjectName("bottomBar");
	bottomBar->setFixedHeight(60);
	auto *bar = new QHBoxLayout(bottomBar);
	bar->setContentsMargins(16, 0, 16, 0);
	bar->setSpacing(12);
	bar->addStretch(1);
	bar->addWidget(pauseButton_);
	bar->addWidget(recordButton_);
	bar->addWidget(timeLabel_);
	bar->addStretch(1);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);
	root->addLayout(body, 1);
	root->addWidget(bottomBar);

	ticker_.setInterval(250);
	connect(&ticker_, &QTimer::timeout, this, &RecordPage::tickClock);

	connect(recordButton_, &QPushButton::clicked, this, [this]() {
		if (bridge_->isRecording()) {
			bridge_->stopRecording();
		} else {
			bridge_->startRecording();
		}
	});
	connect(pauseButton_, &QPushButton::clicked, this,
		[this]() { bridge_->setRecordingPaused(!bridge_->isRecordingPaused()); });

	connect(bridge_, &FrontendBridge::recordingStateChanged, this, &RecordPage::updateRecordingUi);
	connect(bridge_, &FrontendBridge::recordingTransition, this, &RecordPage::showRecordingTransition);
	connect(bridge_, &FrontendBridge::recordingFailed, this, &RecordPage::showRecordingError);

	updateRecordingUi(bridge_->isRecording(), bridge_->isRecordingPaused());
}

void RecordPage::updateRecordingUi(bool active, bool paused)
{
	const int64_t now = monotonic_.elapsed();
	if (active && !recordingClock_.running()) {
		recordingClock_.start(now);
	}
	if (active && paused) {
		recordingClock_.pause(now);
	}
	if (active && !paused) {
		recordingClock_.resume(now);
	}
	if (!active) {
		recordingClock_.stop();
	}

	if (active) {
		ticker_.start();
	} else {
		ticker_.stop();
	}

	pendingStart_ = false;
	recordButton_->setEnabled(true);
	recordButton_->setText(active ? QTStr("Voltz.Record.Stop") : QTStr("Voltz.Record.Start"));
	recordButton_->setIcon(active ? phosphorIcon(QStringLiteral("stop"), palette::kOnLight, IconWeight::Fill)
				      : phosphorIcon(QStringLiteral("record"), palette::kText, IconWeight::Fill));
	recordButton_->setProperty("recording", active);
	recordButton_->style()->unpolish(recordButton_);
	recordButton_->style()->polish(recordButton_);

	const bool canPause = active && bridge_->canPauseRecording();
	pauseButton_->setEnabled(canPause);
	pauseButton_->setToolTip(active && !canPause ? QTStr("Voltz.Record.PauseUnavailable") : QString());
	pauseButton_->setText(active && paused ? QTStr("Voltz.Record.Resume") : QTStr("Voltz.Record.Pause"));
	pauseButton_->setIcon(active && paused
				      ? phosphorIcon(QStringLiteral("play"), palette::kText, IconWeight::Fill)
				      : phosphorIcon(QStringLiteral("pause"), palette::kText, IconWeight::Fill));

	tickClock();
}

void RecordPage::showRecordingError(int code, const QString &lastError)
{
	const RecordingErrorText text = recordingErrorText(classifyRecordingStop(code), !lastError.isEmpty());
	if (text.titleKey.empty()) {
		return;
	}
	QString message = QTStr(text.messageKey.c_str());
	if (text.lastErrorUse == LastErrorUse::FormatArg) {
		message = message.arg(lastError.toHtmlEscaped());
	} else if (text.lastErrorUse == LastErrorUse::Append) {
		message += QStringLiteral("<br><br>") + lastError.toHtmlEscaped();
	}
	blog(LOG_WARNING, "[voltz] erro de gravacao (codigo %d): %s", code, qUtf8Printable(lastError));
	errorBanner_->showError(QTStr(text.titleKey.c_str()), message);
}

void RecordPage::showRecordingTransition(bool stopping)
{
	/* Enquanto inicia/finaliza, um segundo clique chamaria StopRecording com a saída em
	 * "stopping" (parada forçada, pode truncar o arquivo): botões travados até STARTED/STOPPED. */
	recordButton_->setEnabled(false);
	pauseButton_->setEnabled(false);
	recordButton_->setText(stopping ? QTStr("Voltz.Record.Stopping") : QTStr("Voltz.Record.Starting"));

	if (!stopping) {
		/* Falha síncrona ao iniciar (OBSBasic::StartRecording emite STARTING, mas o start da saída
		 * falha e não vem STARTED nem STOPPED): sem isto o botão ficaria preso em "Iniciando…". */
		errorBanner_->hide();
		pendingStart_ = true;
		QTimer::singleShot(0, this, [this]() {
			if (pendingStart_ && !bridge_->isRecording()) {
				updateRecordingUi(false, false);
			}
		});
	}
}

void RecordPage::tickClock()
{
	timeLabel_->setText(QString::fromStdString(formatClock(recordingClock_.elapsedMs(monotonic_.elapsed()))));
}

} // namespace voltz
