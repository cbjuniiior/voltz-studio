#include "SettingsPage.hpp"

#include "DeviceSelector.hpp"
#include "FrontendBridge.hpp"
#include "RecordingConfig.hpp"
#include "SharedSources.hpp"

#include <voltz/core/voltz-version.hpp>

#include <OBSApp.hpp>
#include <widgets/OBSBasic.hpp>

#include <obs.h>

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

#include "moc_SettingsPage.cpp"

namespace voltz {

namespace {

QString encoderLabel(const std::string &encoder)
{
	const std::string key = "Voltz.Settings.Encoder." + encoder;
	const QString name = QTStr(key.c_str());
	return QTStr("Voltz.Settings.Encoder.Auto")
		.arg(name == QString::fromStdString(key) ? QString::fromStdString(encoder) : name);
}

} // namespace

SettingsPage::SettingsPage(FrontendBridge *bridge, QWidget *parent) : QWidget(parent), bridge_(bridge)
{
	auto *heading = new QLabel(QTStr("Voltz.Settings.Title"), this);
	heading->setObjectName("pageTitle");

	auto *content = new QWidget(this);
	sections_ = new QVBoxLayout(content);
	sections_->setContentsMargins(32, 28, 32, 28);
	sections_->setSpacing(16);
	sections_->addWidget(heading);
	sections_->addWidget(buildRecordingSection());
	sections_->addWidget(buildCaptureSection());
	sections_->addWidget(buildAboutSection());
	sections_->addStretch(1);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(content);
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->addWidget(scroll);

	connect(bridge_, &FrontendBridge::recordingStateChanged, this,
		[this](bool active, bool) { setRecordingLocked(active); });
	loadRecording();
}

QFrame *SettingsPage::makeSection(const QString &title, QVBoxLayout **body)
{
	auto *section = new QFrame(this);
	section->setObjectName("panel");
	auto *layout = new QVBoxLayout(section);
	layout->setContentsMargins(16, 14, 16, 16);
	layout->setSpacing(10);
	auto *label = new QLabel(title.toUpper(), section);
	label->setObjectName("panelTitle");
	layout->addWidget(label);
	*body = layout;
	return section;
}

QFrame *SettingsPage::buildRecordingSection()
{
	QVBoxLayout *body = nullptr;
	recording_ = makeSection(QTStr("Voltz.Settings.Recording"), &body);

	recordingNotice_ = new QLabel(recording_);
	recordingNotice_->setObjectName("muted");
	recordingNotice_->setWordWrap(true);
	recordingNotice_->hide();
	body->addWidget(recordingNotice_);

	folder_ = new QLabel(recording_);
	folder_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	changeFolder_ = new QPushButton(QTStr("Voltz.Settings.ChangeFolder"), recording_);
	auto *folderRow = new QHBoxLayout();
	folderRow->addWidget(folder_, 1);
	folderRow->addWidget(changeFolder_);

	format_ = new QComboBox(recording_);
	format_->addItem(QTStr("Voltz.Settings.Format.Mp4"), int(RecordingFormat::Mp4));
	format_->addItem(QTStr("Voltz.Settings.Format.Mkv"), int(RecordingFormat::Mkv));
	format_->addItem(QTStr("Voltz.Settings.Format.Mov"), int(RecordingFormat::Mov));

	quality_ = new QComboBox(recording_);
	quality_->addItem(QTStr("Voltz.Settings.Quality.High"), int(RecordingQuality::High));
	quality_->addItem(QTStr("Voltz.Settings.Quality.Compact"), int(RecordingQuality::Compact));

	encoder_ = new QLabel(recording_);
	encoder_->setObjectName("muted");

	auto *form = new QFormLayout();
	form->setLabelAlignment(Qt::AlignLeft);
	form->setHorizontalSpacing(16);
	form->setVerticalSpacing(10);
	form->addRow(QTStr("Voltz.Settings.Folder"), folderRow);
	form->addRow(QTStr("Voltz.Settings.Format"), format_);
	form->addRow(QTStr("Voltz.Settings.Quality"), quality_);
	form->addRow(QTStr("Voltz.Settings.Encoder"), encoder_);
	body->addLayout(form);

	connect(changeFolder_, &QPushButton::clicked, this, [this]() {
		const QString chosen = QFileDialog::getExistingDirectory(this, QTStr("Voltz.Settings.Folder"),
									 RecordingConfig::folder());
		if (!chosen.isEmpty()) {
			RecordingConfig::setFolder(chosen);
			loadRecording();
		}
	});
	connect(format_, &QComboBox::currentIndexChanged, this, [this]() {
		if (!loading_) {
			RecordingConfig::setFormat(RecordingFormat(format_->currentData().toInt()));
		}
	});
	connect(quality_, &QComboBox::currentIndexChanged, this, [this]() {
		if (!loading_) {
			RecordingConfig::setQuality(RecordingQuality(quality_->currentData().toInt()));
			loadRecording();
		}
	});
	return recording_;
}

QFrame *SettingsPage::buildCaptureSection()
{
	QVBoxLayout *body = nullptr;
	QFrame *section = makeSection(QTStr("Voltz.Settings.Capture"), &body);

	monitor_ = new DeviceSelector([]() { return findSharedSource(SharedSource::Screen); }, "monitor_id", section);
	camera_ =
		new DeviceSelector([]() { return findSharedSource(SharedSource::Camera); }, "video_device_id", section);
	microphone_ = new DeviceSelector(
		[]() {
			OBSSourceAutoRelease mic = obs_get_output_source(3);
			return OBSSource(mic.Get());
		},
		"device_id", section);

	captureHint_ = new QLabel(QTStr("Voltz.Settings.NoSource"), section);
	captureHint_->setObjectName("muted");
	captureHint_->setWordWrap(true);

	auto *form = new QFormLayout();
	form->setHorizontalSpacing(16);
	form->setVerticalSpacing(10);
	form->addRow(QTStr("Voltz.Settings.Monitor"), monitor_);
	form->addRow(QTStr("Voltz.Settings.Camera"), camera_);
	form->addRow(QTStr("Voltz.Settings.Microphone"), microphone_);
	body->addLayout(form);
	body->addWidget(captureHint_);
	return section;
}

QFrame *SettingsPage::buildAboutSection()
{
	QVBoxLayout *body = nullptr;
	QFrame *section = makeSection(QTStr("Voltz.Settings.About"), &body);

	const VersionInfo info{"Voltz Studio", VOLTZ_VERSION, obs_get_version_string()};
	auto *about = new QLabel(QString::fromStdString(aboutLine(info, QTStr("Voltz.About.BasedOn").toStdString())),
				 section);
	about->setObjectName("muted");
	about->setWordWrap(true);

	auto *classic = new QPushButton(QTStr("Voltz.Settings.OpenClassic"), section);
	connect(classic, &QPushButton::clicked, this, []() {
		OBSBasic *main = OBSBasic::Get();
		QMetaObject::invokeMethod(main, "SetShowing", Q_ARG(bool, true));
		main->raise();
		main->activateWindow();
	});
	auto *logs = new QPushButton(QTStr("Voltz.Settings.OpenLogs"), section);
	connect(logs, &QPushButton::clicked, this, []() {
		char path[512];
		if (GetAppConfigPath(path, sizeof(path), VOLTZ_CONFIG_DIR "/logs") > 0) {
			QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromUtf8(path)));
		}
	});

	auto *buttons = new QHBoxLayout();
	buttons->addWidget(classic);
	buttons->addWidget(logs);
	buttons->addStretch(1);
	body->addWidget(about);
	body->addLayout(buttons);
	return section;
}

void SettingsPage::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	loadRecording();
	monitor_->reload();
	camera_->reload();
	microphone_->reload();
	captureHint_->setVisible(!monitor_->isEnabled() || !camera_->isEnabled());
}

void SettingsPage::loadRecording()
{
	loading_ = true;
	folder_->setText(QDir::toNativeSeparators(RecordingConfig::folder()));
	format_->setCurrentIndex(format_->findData(int(RecordingConfig::format())));
	quality_->setCurrentIndex(quality_->findData(int(RecordingConfig::quality())));
	encoder_->setText(encoderLabel(RecordingConfig::currentEncoder()));
	loading_ = false;

	setRecordingLocked(bridge_->isRecording());
}

void SettingsPage::setRecordingLocked(bool locked)
{
	const bool editable = RecordingConfig::isSimpleMode() && !locked;
	changeFolder_->setEnabled(editable);
	format_->setEnabled(editable);
	quality_->setEnabled(editable);
	if (locked) {
		recordingNotice_->setText(QTStr("Voltz.Settings.LockedWhileRecording"));
		recordingNotice_->show();
	} else if (!RecordingConfig::isSimpleMode()) {
		recordingNotice_->setText(QTStr("Voltz.Settings.AdvancedMode"));
		recordingNotice_->show();
	} else {
		recordingNotice_->hide();
	}
}

} // namespace voltz
