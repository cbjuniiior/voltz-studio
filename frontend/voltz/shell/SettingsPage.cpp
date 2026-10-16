#include "SettingsPage.hpp"

#include "DeviceSelector.hpp"
#include "FrontendBridge.hpp"
#include "RecordingConfig.hpp"
#include "SharedSources.hpp"

#include <voltz/core/voltz-version.hpp>

#include <OBSApp.hpp>
#include <widgets/OBSBasic.hpp>
#include <utility/platform.hpp>

#include <obs.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFontMetrics>
#include <QGridLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QGuiApplication>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>

#include "moc_SettingsPage.cpp"

namespace voltz {

namespace {

QString encoderName(const std::string &encoder)
{
	const std::string key = "Voltz.Settings.Encoder." + encoder;
	const QString name = QTStr(key.c_str());
	return name == QString::fromStdString(key) ? QString::fromStdString(encoder) : name;
}

// "(automático)" só quando o codificador é o que o Voltz escolheria; x264 pode ser só o padrão do OBS.
// Com qualidade "Stream" o OBS grava com o codificador da transmissão (SimpleOutput::LoadRecordingPreset),
// então o nome mostrado vem de SimpleOutput/StreamEncoder.
QString encoderLabel()
{
	if (RecordingConfig::recordsWithStreamEncoder()) {
		return encoderName(RecordingConfig::streamEncoder());
	}
	const std::string current = RecordingConfig::currentEncoder();
	if (current == RecordingConfig::automaticEncoder()) {
		return QTStr("Voltz.Settings.Encoder.Auto").arg(encoderName(current));
	}
	return encoderName(current);
}

// Grade "rótulo | campo" com o rótulo centrado na altura do campo. O QFormLayout deixava o rótulo no topo da
// linha (~5 px acima do centro dos combos).
class FormGrid {
public:
	FormGrid() : grid_(new QGridLayout())
	{
		grid_->setContentsMargins(0, 0, 0, 0);
		grid_->setHorizontalSpacing(16);
		grid_->setVerticalSpacing(10);
		grid_->setColumnStretch(1, 1);
	}

	void addRow(QLabel *label, QWidget *field)
	{
		grid_->addWidget(label, rows_, 0, Qt::AlignLeft | Qt::AlignVCenter);
		grid_->addWidget(field, rows_, 1, Qt::AlignVCenter);
		rows_++;
	}

	void addRow(QLabel *label, QLayout *field)
	{
		grid_->addWidget(label, rows_, 0, Qt::AlignLeft | Qt::AlignVCenter);
		grid_->addLayout(field, rows_, 1);
		rows_++;
	}

	QGridLayout *layout() const { return grid_; }

private:
	QGridLayout *grid_;
	int rows_ = 0;
};

} // namespace

// Rótulo de uma linha que corta o texto no meio ("C:\Users\…\Gravações") conforme a largura disponível.
// Guarda o texto completo (também na dica) e refaz o corte a cada redimensionamento.
class ElidedLabel : public QLabel {
public:
	explicit ElidedLabel(QWidget *parent) : QLabel(parent)
	{
		// Caminho longo não pode alargar a página (gerava rolagem horizontal): o rótulo encolhe.
		setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	}

	void setFullText(const QString &text)
	{
		fullText_ = text;
		setToolTip(text);
		updateElided();
	}

protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QLabel::resizeEvent(event);
		updateElided();
	}

	void changeEvent(QEvent *event) override
	{
		QLabel::changeEvent(event);
		if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
			updateElided();
		}
	}

private:
	void updateElided()
	{
		const QString elided = fontMetrics().elidedText(fullText_, Qt::ElideMiddle, contentsRect().width());
		if (elided != text()) {
			setText(elided);
		}
	}

	QString fullText_;
};

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

QLabel *SettingsPage::formLabel(const QString &text, QWidget *parent)
{
	auto *label = new QLabel(text, parent);
	formLabels_.append(label);
	return label;
}

// Cada seção tem seu QFormLayout; sem isto a coluna dos campos começa em x diferente em cada uma.
// Roda no showEvent porque a fonte do rótulo vem do QSS da casca, aplicado só depois de entrar na hierarquia.
void SettingsPage::alignFormLabels()
{
	int width = 0;
	for (QLabel *label : formLabels_) {
		label->ensurePolished();
		width = std::max(width, label->sizeHint().width());
	}
	for (QLabel *label : formLabels_) {
		label->setMinimumWidth(width);
	}
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

	folder_ = new ElidedLabel(recording_);
	changeFolder_ = new QPushButton(QTStr("Voltz.Settings.ChangeFolder"), recording_);
	auto *folderRow = new QHBoxLayout();
	folderRow->setSpacing(12); // respiro entre o caminho e o botão "Alterar…"
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

	FormGrid form;
	form.addRow(formLabel(QTStr("Voltz.Settings.Folder"), recording_), folderRow);
	form.addRow(formLabel(QTStr("Voltz.Settings.Format"), recording_), format_);
	form.addRow(formLabel(QTStr("Voltz.Settings.Quality"), recording_), quality_);
	form.addRow(formLabel(QTStr("Voltz.Settings.Encoder"), recording_), encoder_);
	body->addLayout(form.layout());

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

	FormGrid form;
	form.addRow(formLabel(QTStr("Voltz.Settings.Monitor"), section), monitor_);
	form.addRow(formLabel(QTStr("Voltz.Settings.Camera"), section), camera_);
	form.addRow(formLabel(QTStr("Voltz.Settings.Microphone"), section), microphone_);
	body->addLayout(form.layout());
	body->addWidget(captureHint_);

	hideFromCapture_ = new QCheckBox(QTStr("Voltz.Settings.HideFromCapture"), section);
	hideFromCapture_->setEnabled(SetDisplayAffinitySupported());
	auto *hideHint = new QLabel(SetDisplayAffinitySupported() ? QTStr("Voltz.Settings.HideFromCapture.Hint")
								  : QTStr("Voltz.Settings.HideFromCapture.Unsupported"),
				    section);
	hideHint->setObjectName("muted");
	hideHint->setWordWrap(true);
	body->addSpacing(4);
	body->addWidget(hideFromCapture_);
	body->addWidget(hideHint);

	connect(hideFromCapture_, &QCheckBox::toggled, this, [](bool hide) {
		/* Mesma chave e mesmo efeito de Configurações → Geral do OBS: grava e reaplica em todas as janelas. */
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "HideOBSWindowsFromCapture", hide);
		config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
		for (QWindow *window : QGuiApplication::allWindows()) {
			if (window->isVisible()) {
				OBSBasic::Get()->SetDisplayAffinity(window);
			}
		}
		blog(LOG_INFO, "[voltz] esconder da captura: %s", hide ? "ligado" : "desligado");
	});
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
	alignFormLabels();
	loadRecording();
	monitor_->reload();
	camera_->reload();
	microphone_->reload();
	{
		/* Pode ter mudado pelas Configurações do OBS: relê sem disparar o toggled. */
		const QSignalBlocker blocker(hideFromCapture_);
		hideFromCapture_->setChecked(
			config_get_bool(App()->GetUserConfig(), "BasicWindow", "HideOBSWindowsFromCapture"));
	}
	captureHint_->setVisible(!monitor_->isEnabled() || !camera_->isEnabled());
}

void SettingsPage::loadRecording()
{
	loading_ = true;
	folder_->setFullText(QDir::toNativeSeparators(RecordingConfig::folder()));
	format_->setCurrentIndex(format_->findData(int(RecordingConfig::format())));
	quality_->setCurrentIndex(quality_->findData(int(RecordingConfig::quality())));
	encoder_->setText(encoderLabel());
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
