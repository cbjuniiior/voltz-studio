#include "RecordPage.hpp"

#include "FrontendBridge.hpp"
#include "VoltzPreview.hpp"

#include <voltz/core/time-format.hpp>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include "moc_RecordPage.cpp"

namespace voltz {

namespace {

QFrame *makePanel(const QString &title, QWidget *content, QWidget *parent)
{
	auto *panel = new QFrame(parent);
	panel->setObjectName("panel");
	auto *layout = new QVBoxLayout(panel);
	layout->setContentsMargins(10, 10, 10, 10);
	layout->setSpacing(8);
	auto *label = new QLabel(title, panel);
	label->setObjectName("panelTitle");
	layout->addWidget(label);
	layout->addWidget(content, 1);
	return panel;
}

} // namespace

RecordPage::RecordPage(FrontendBridge *bridge, QWidget *parent) : QWidget(parent), bridge_(bridge)
{
	monotonic_.start();

	scenes_ = new QListWidget(this);
	preview_ = new VoltzPreview(this);

	auto *sourcesHint = new QLabel(QStringLiteral("Fontes e áudio chegam no próximo plano."), this);
	sourcesHint->setObjectName("muted");
	sourcesHint->setWordWrap(true);
	sourcesHint->setAlignment(Qt::AlignTop | Qt::AlignLeft);

	QFrame *scenesPanel = makePanel(QStringLiteral("CENAS"), scenes_, this);
	scenesPanel->setFixedWidth(200);
	QFrame *sidePanel = makePanel(QStringLiteral("FONTES"), sourcesHint, this);
	sidePanel->setFixedWidth(260);

	auto *body = new QHBoxLayout();
	body->setContentsMargins(12, 12, 12, 12);
	body->setSpacing(12);
	body->addWidget(scenesPanel);
	body->addWidget(preview_, 1);
	body->addWidget(sidePanel);

	pauseButton_ = new QPushButton(this);
	recordButton_ = new QPushButton(this);
	recordButton_->setObjectName("recordButton");
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

	connect(scenes_, &QListWidget::currentTextChanged, this, [this](const QString &name) {
		if (!updatingList_ && !name.isEmpty()) {
			bridge_->setCurrentScene(name);
		}
	});
	connect(recordButton_, &QPushButton::clicked, this, [this]() {
		if (bridge_->isRecording()) {
			bridge_->stopRecording();
		} else {
			bridge_->startRecording();
		}
	});
	connect(pauseButton_, &QPushButton::clicked, this,
		[this]() { bridge_->setRecordingPaused(!bridge_->isRecordingPaused()); });

	connect(bridge_, &FrontendBridge::sceneListChanged, this, &RecordPage::refreshScenes);
	connect(bridge_, &FrontendBridge::currentSceneChanged, this, &RecordPage::selectScene);
	connect(bridge_, &FrontendBridge::recordingStateChanged, this, &RecordPage::updateRecordingUi);
	connect(bridge_, &FrontendBridge::recordingTransition, this, &RecordPage::showRecordingTransition);

	refreshScenes();
	updateRecordingUi(bridge_->isRecording(), bridge_->isRecordingPaused());
}

void RecordPage::refreshScenes()
{
	updatingList_ = true;
	scenes_->clear();
	scenes_->addItems(bridge_->sceneNames());
	updatingList_ = false;
	selectScene(bridge_->currentSceneName());
}

void RecordPage::selectScene(const QString &name)
{
	updatingList_ = true;
	const QList<QListWidgetItem *> matches = scenes_->findItems(name, Qt::MatchExactly);
	if (!matches.isEmpty()) {
		scenes_->setCurrentItem(matches.first());
	}
	updatingList_ = false;
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
	recordButton_->setText(active ? QStringLiteral("■  Parar") : QStringLiteral("●  Gravar"));
	recordButton_->setProperty("recording", active);
	recordButton_->style()->unpolish(recordButton_);
	recordButton_->style()->polish(recordButton_);

	pauseButton_->setEnabled(active);
	pauseButton_->setText(active && paused ? QStringLiteral("▶  Retomar") : QStringLiteral("⏸  Pausar"));

	tickClock();
}

void RecordPage::showRecordingTransition(bool stopping)
{
	/* Enquanto inicia/finaliza, um segundo clique chamaria StopRecording com a saída em
	 * "stopping" (parada forçada, pode truncar o arquivo): botões travados até STARTED/STOPPED. */
	recordButton_->setEnabled(false);
	pauseButton_->setEnabled(false);
	recordButton_->setText(stopping ? QStringLiteral("Finalizando…") : QStringLiteral("Iniciando…"));

	if (!stopping) {
		/* Falha síncrona ao iniciar (OBSBasic::StartRecording emite STARTING, mas o start da saída
		 * falha e não vem STARTED nem STOPPED): sem isto o botão ficaria preso em "Iniciando…". */
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
