#include "RecordPage.hpp"

#include "AudioPanel.hpp"
#include "ErrorBanner.hpp"
#include "FrontendBridge.hpp"
#include "Icons.hpp"
#include "RecordingConfig.hpp"
#include "ScenesPanel.hpp"
#include "SourcesPanel.hpp"
#include "VoltzPreview.hpp"

#include <voltz/core/disk-space.hpp>
#include <voltz/core/recording-error.hpp>
#include <voltz/core/time-format.hpp>
#include <voltz/project/ProjectLibrary.hpp>
#include <voltz/project/ProjectRecorder.hpp>
#include <voltz/annotate/DrawModeController.hpp>

#include <OBSApp.hpp> // QTStr

#include <QComboBox>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

#include "moc_RecordPage.cpp"

namespace voltz {

namespace {

constexpr int kDiskIdleMs = 30000;
constexpr int kDiskRecordingMs = 5000;
constexpr int kMarkerToastMs = 2500;

char decimalSeparator()
{
	return QLocale().decimalPoint().at(0).toLatin1();
}

QString freeSpaceText(quint64 bytes)
{
	return QString::fromStdString(formatFreeSpace(bytes, decimalSeparator()));
}

} // namespace

RecordPage::RecordPage(FrontendBridge *bridge, ProjectRecorder *recorder, DrawModeController *drawMode, QWidget *parent)
	: QWidget(parent),
	  bridge_(bridge),
	  recorder_(recorder),
	  drawMode_(drawMode)
{
	monotonic_.start();

	scenes_ = new ScenesPanel(bridge_, this);
	scenes_->setFixedWidth(200);
	preview_ = new VoltzPreview(this);

	auto *centerColumn = new QVBoxLayout();
	centerColumn->setContentsMargins(0, 0, 0, 0);
	centerColumn->setSpacing(12);
	errorBanner_ = new ErrorBanner(this);
	diskBanner_ = new ErrorBanner(this);
	diskBanner_->setWarning(true);
	isoBanner_ = new ErrorBanner(this);
	isoBanner_->setWarning(true);
	centerColumn->addWidget(errorBanner_);
	centerColumn->addWidget(diskBanner_);
	centerColumn->addWidget(isoBanner_);
	centerColumn->addWidget(preview_, 1);

	auto *sideHolder = new QWidget(this);
	sideHolder->setFixedWidth(280);
	auto *sideColumn = new QVBoxLayout(sideHolder);
	sideColumn->setContentsMargins(0, 0, 0, 0);
	sideColumn->setSpacing(12);
	sideColumn->addWidget(new SourcesPanel(bridge_, sideHolder), 3);
	sideColumn->addWidget(new AudioPanel(sideHolder), 2);

	auto *body = new QHBoxLayout();
	body->setContentsMargins(12, 12, 12, 12);
	body->setSpacing(12);
	body->addWidget(scenes_);
	body->addLayout(centerColumn, 1);
	body->addWidget(sideHolder);

	projectName_ = new QComboBox(this);
	projectName_->setObjectName("projectName");
	projectName_->setEditable(true);
	projectName_->setInsertPolicy(QComboBox::NoInsert);
	projectName_->setMinimumWidth(220);
	projectName_->setMaxVisibleItems(8);
	projectName_->setToolTip(QTStr("Voltz.Record.ProjectName.Hint"));
	projectName_->setAccessibleName(QTStr("Voltz.Record.ProjectName"));
	projectName_->lineEdit()->setPlaceholderText(QTStr("Voltz.Project.DefaultName"));

	drawButton_ = new QPushButton(QTStr("Voltz.Record.Draw"), this);
	drawButton_->setObjectName("drawButton");
	drawButton_->setCheckable(true);
	drawButton_->setIcon(phosphorToggleIcon(QStringLiteral("pencil-simple"), palette::kText, palette::kAccent));
	drawButton_->setIconSize(QSize(16, 16));
	markerButton_ = new QPushButton(QTStr("Voltz.Record.Marker"), this);
	markerButton_->setIcon(phosphorIcon(QStringLiteral("flag"), palette::kText));
	markerButton_->setIconSize(QSize(16, 16));
	markerButton_->setToolTip(QTStr("Voltz.Record.Marker.Hint"));
	pauseButton_ = new QPushButton(this);
	pauseButton_->setIconSize(QSize(16, 16));
	recordButton_ = new QPushButton(this);
	recordButton_->setObjectName("recordButton");
	recordButton_->setIconSize(QSize(16, 16));
	timeLabel_ = new QLabel(QString::fromStdString(formatClock(0)), this);
	timeLabel_->setObjectName("timeLabel");
	markerToast_ = new QLabel(this);
	markerToast_->setObjectName("markerToast");
	markerToast_->setMinimumWidth(180);
	auto *spaceIcon = new QLabel(this);
	spaceIcon->setPixmap(phosphorIcon(QStringLiteral("hard-drives"), palette::kMuted).pixmap(16, 16));
	spaceLabel_ = new QLabel(this);
	spaceLabel_->setObjectName("spaceLabel");

	auto *bottomBar = new QFrame(this);
	bottomBar->setObjectName("bottomBar");
	bottomBar->setFixedHeight(60);
	auto *bar = new QHBoxLayout(bottomBar);
	bar->setContentsMargins(16, 0, 16, 0);
	bar->setSpacing(12);
	bar->addWidget(projectName_);
	bar->addStretch(1);
	bar->addWidget(drawButton_);
	bar->addWidget(markerButton_);
	bar->addWidget(pauseButton_);
	bar->addWidget(recordButton_);
	bar->addWidget(timeLabel_);
	bar->addWidget(markerToast_);
	bar->addStretch(1);
	bar->addWidget(spaceIcon);
	bar->addWidget(spaceLabel_);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);
	root->addLayout(body, 1);
	root->addWidget(bottomBar);

	ticker_.setInterval(250);
	connect(&ticker_, &QTimer::timeout, this, &RecordPage::tickClock);
	diskTimer_.setInterval(kDiskIdleMs);
	connect(&diskTimer_, &QTimer::timeout, this, &RecordPage::refreshDiskInfo);
	diskTimer_.start();
	markerToastTimer_.setSingleShot(true);
	markerToastTimer_.setInterval(kMarkerToastMs);
	connect(&markerToastTimer_, &QTimer::timeout, markerToast_, &QLabel::clear);

	connect(recordButton_, &QPushButton::clicked, this, [this]() {
		if (bridge_->isRecording()) {
			bridge_->stopRecording();
		} else {
			commitProjectName();
			bridge_->startRecording();
		}
	});
	connect(pauseButton_, &QPushButton::clicked, this,
		[this]() { bridge_->setRecordingPaused(!bridge_->isRecordingPaused()); });
	connect(markerButton_, &QPushButton::clicked, this, [this]() { recorder_->addMarker(); });
	connect(drawButton_, &QPushButton::clicked, this, [this]() {
		drawMode_->toggle(DrawTrigger::Button);
		/* o botão reflete o estado de verdade (ligou, foi recusado ou vai sair) */
		const QSignalBlocker blocker(drawButton_);
		drawButton_->setChecked(drawMode_->active());
	});
	connect(drawMode_, &DrawModeController::activeChanged, this, [this](bool active) {
		const QSignalBlocker blocker(drawButton_);
		drawButton_->setChecked(active);
	});
	connect(drawMode_, &DrawModeController::refused, this, [this](const QString &message) { showToast(message); });
	refreshDrawHint();
	connect(projectName_->lineEdit(), &QLineEdit::editingFinished, this, &RecordPage::commitProjectName);
	connect(projectName_, &QComboBox::activated, this, [this](int) { commitProjectName(); });

	connect(bridge_, &FrontendBridge::recordingStateChanged, this, &RecordPage::updateRecordingUi);
	connect(bridge_, &FrontendBridge::recordingTransition, this, &RecordPage::showRecordingTransition);
	connect(bridge_, &FrontendBridge::recordingFailed, this, &RecordPage::showRecordingError);

	connect(recorder_, &ProjectRecorder::markerAdded, this,
		[this](int number, qint64) { showMarkerToast(number); });
	connect(recorder_, &ProjectRecorder::projectStarted, this, [this](const QString &, const QString &name) {
		const QSignalBlocker blocker(projectName_);
		projectName_->setEditText(name);
		isoFailures_.clear();
		isoBanner_->hide();
	});
	connect(recorder_, &ProjectRecorder::isoFailed, this, &RecordPage::showIsoFailure);
	connect(recorder_, &ProjectRecorder::projectFinished, this, [this](const QString &, const QString &) {
		reloadProjectNames();
		refreshDiskInfo();
	});
	connect(recorder_, &ProjectRecorder::stoppedForLowDisk, this, [this](quint64 freeBytes) {
		errorBanner_->showError(QTStr("Voltz.Disk.Stopped.Title"),
					QTStr("Voltz.Disk.Stopped.Text").arg(freeSpaceText(freeBytes)));
	});
	connect(recorder_, &ProjectRecorder::manifestWriteFailed, this, [this](const QString &backupPath) {
		errorBanner_->showError(QTStr("Voltz.Project.ManifestFailed.Title"),
					QTStr("Voltz.Project.ManifestFailed.Text")
						.arg(QDir::toNativeSeparators(backupPath).toHtmlEscaped()));
	});
	connect(diskBanner_, &ErrorBanner::dismissed, this, [this]() { diskBannerDismissed_ = true; });

	reloadProjectNames();
	updateRecordingUi(bridge_->isRecording(), bridge_->isRecordingPaused());
	refreshDiskInfo();
}

void RecordPage::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	if (!recorder_->isActive()) {
		reloadProjectNames();
	}
	refreshDiskInfo();
	refreshDrawHint();
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

	markerButton_->setEnabled(active && !paused && recorder_->isActive());
	projectName_->setEnabled(!active);
	diskTimer_.setInterval(active ? kDiskRecordingMs : kDiskIdleMs);

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
	markerButton_->setEnabled(false);
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

void RecordPage::showMarkerToast(int number)
{
	/* Mostra o tempo do cronômetro (líquido, como o usuário vê), não o t_ms bruto do marcadores.json. */
	const QString clock = QString::fromStdString(formatClock(recordingClock_.elapsedMs(monotonic_.elapsed())));
	showToast(QTStr("Voltz.Record.MarkerAdded").arg(QTStr("Voltz.Marker.Name").arg(number), clock));
}

void RecordPage::showToast(const QString &text)
{
	markerToast_->setText(text);
	markerToastTimer_.start();
}

void RecordPage::refreshDrawHint()
{
	drawButton_->setToolTip(QTStr("Voltz.Record.Draw.Hint").arg(drawMode_->drawShortcutText()));
}

void RecordPage::reloadProjectNames()
{
	const QString current = recorder_->isActive() ? recorder_->currentProjectName() : recorder_->nextProjectName();
	const QSignalBlocker blocker(projectName_);
	projectName_->clear();
	projectName_->addItems(ProjectLibrary::recentNames(RecordingConfig::outputFolder()));
	projectName_->setEditText(current);
}

void RecordPage::commitProjectName()
{
	if (recorder_->isActive()) {
		return;
	}
	recorder_->setNextProjectName(projectName_->currentText());
	if (projectName_->currentText().trimmed().isEmpty()) {
		const QSignalBlocker blocker(projectName_);
		projectName_->setEditText(recorder_->nextProjectName());
	}
}

void RecordPage::refreshDiskInfo()
{
	const QString folder = RecordingConfig::outputFolder();
	uint64_t rate = recorder_->measuredBytesPerMinute();
	if (rate == 0) {
		rate = RecordingConfig::estimatedBytesPerMinute();
	}
	const QString rateText = QString::fromStdString(formatRate(rate, decimalSeparator()));
	quint64 free = 0;
	const bool known = ProjectRecorder::freeBytes(folder, free);
	spaceLabel_->setText(known ? QTStr("Voltz.Record.Space").arg(rateText, freeSpaceText(free)) : rateText);
	spaceLabel_->setToolTip(QTStr("Voltz.Record.Space.Hint").arg(QDir::toNativeSeparators(folder)));

	const DiskLevel level = known ? diskLevel(free) : DiskLevel::Ok;
	if (level == DiskLevel::Ok) {
		diskBannerDismissed_ = false;
		diskBanner_->hide();
	} else if (!diskBannerDismissed_) {
		diskBanner_->showError(QTStr("Voltz.Disk.Low.Title"),
				       QTStr("Voltz.Disk.Low.Text").arg(freeSpaceText(free)));
	}
}

void RecordPage::showIsoFailure(const QString &track, const QString &reason)
{
	const QString item = QTStr("Voltz.Iso.Failed.Item")
				     .arg(QTStr(qUtf8Printable(QStringLiteral("Voltz.Iso.Track.") + track)),
					  QTStr(qUtf8Printable(QStringLiteral("Voltz.Iso.Reason.") + reason)));
	if (!isoFailures_.contains(item)) {
		isoFailures_.append(item);
	}
	/* spec §5.4: sem nenhum ISO gravando, "gravando só a versão montada". */
	const QString title = recorder_->runningIsoCount() == 0 ? QTStr("Voltz.Iso.Off.Title")
								: QTStr("Voltz.Iso.Partial.Title");
	isoBanner_->showError(
		title, QTStr("Voltz.Iso.Failed.Text").arg(isoFailures_.join(QStringLiteral(", ")).toHtmlEscaped()));
	blog(LOG_INFO, "[voltz] aviso de ISO: %s (%s)", qUtf8Printable(track), qUtf8Printable(reason));
}

} // namespace voltz
