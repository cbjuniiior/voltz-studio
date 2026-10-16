#include "AudioPanel.hpp"

#include <components/VolumeControl.hpp>
#include <widgets/OBSBasic.hpp>

#include <OBSApp.hpp> // QTStr

#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

#include "moc_AudioPanel.cpp"

namespace voltz {

namespace {

constexpr uint32_t kDesktopChannel = 1;
constexpr uint32_t kMicChannel = 3;

// Troca o nome padrão do OBS ("Áudio do desktop", "Mic/Aux"...) pelo nome da casca. Nunca mexe num nome
// escolhido pelo usuário nem cria nome duplicado.
void applyFriendlyName(obs_source_t *source, const char *obsDefaultKey, const char *obsDefaultEnUs,
		       const char *voltzKey)
{
	const char *current = obs_source_get_name(source);
	if (!current) {
		return;
	}
	const QString name = QString::fromUtf8(current);
	if (name != QString::fromUtf8(Str(obsDefaultKey)) && name != QLatin1String(obsDefaultEnUs)) {
		return;
	}
	const QString target = QTStr(voltzKey);
	if (target.isEmpty() || target == name) {
		return;
	}
	const QByteArray targetUtf8 = target.toUtf8();
	OBSSourceAutoRelease existing = obs_get_source_by_name(targetUtf8.constData());
	if (existing) {
		return;
	}
	obs_source_set_name(source, targetUtf8.constData());
	blog(LOG_INFO, "[voltz] fonte de audio renomeada: '%s' -> '%s'", qUtf8Printable(name), targetUtf8.constData());
}

// Classes de estilo do OBS ficam na propriedade dinâmica "class" (Idian::Utils::addClass).
bool hasStyleClass(const QWidget *widget, const char *styleClass)
{
	return widget->property("class")
		.toString()
		.split(QLatin1Char(' '), Qt::SkipEmptyParts)
		.contains(QLatin1String(styleClass));
}

} // namespace

AudioPanel::AudioPanel(QWidget *parent) : QFrame(parent)
{
	setObjectName("panel");

	auto *title = new QLabel(QTStr("Voltz.Panel.Audio"), this);
	title->setObjectName("panelTitle");
	empty_ = new QLabel(QTStr("Voltz.Audio.Empty"), this);
	empty_->setObjectName("muted");
	empty_->setWordWrap(true);

	controls_ = new QVBoxLayout();
	controls_->setContentsMargins(0, 0, 0, 0);
	controls_->setSpacing(6);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(10, 10, 10, 10);
	layout->setSpacing(8);
	layout->addWidget(title);
	layout->addWidget(empty_);
	layout->addLayout(controls_);
	layout->addStretch(1);

	channelSignal_.Connect(obs_get_signal_handler(), "channel_change", &AudioPanel::handleChannelChange, this);
	rebuild();
}

void AudioPanel::handleChannelChange(void *data, calldata_t *params)
{
	const long long channel = calldata_int(params, "channel");
	if (channel != kDesktopChannel && channel != kMicChannel) {
		return;
	}
	QMetaObject::invokeMethod(static_cast<AudioPanel *>(data), "rebuild", Qt::QueuedConnection);
}

void AudioPanel::rebuild()
{
	/* deleteLater: um menu de contexto do VolumeControl pode estar aberto (loop aninhado) agora. */
	while (QLayoutItem *item = controls_->takeAt(0)) {
		if (QWidget *widget = item->widget()) {
			widget->hide();
			widget->deleteLater();
		}
		delete item;
	}
	addChannel(kDesktopChannel);
	addChannel(kMicChannel);
	empty_->setVisible(controls_->count() == 0);
}

void AudioPanel::addChannel(uint32_t channel)
{
	OBSSourceAutoRelease source = obs_get_output_source(channel);
	if (!source) {
		return;
	}

	if (channel == kDesktopChannel) {
		applyFriendlyName(source, "Basic.DesktopDevice1", "Desktop Audio", "Voltz.Audio.Desktop");
	} else if (channel == kMicChannel) {
		applyFriendlyName(source, "Basic.AuxDevice1", "Mic/Aux", "Voltz.Audio.Mic");
	}

	auto *control = new VolumeControl(source, this, false);
	control->setGlobalInMixer(true);
	control->enableSlider(true);

	config_t *config = OBSBasic::Get()->Config();
	control->setMeterDecayRate(config_get_double(config, "Audio", "MeterDecayRate"));
	control->setPeakMeterType(config_get_uint(config, "Audio", "PeakMeterType") == 1 ? TRUE_PEAK_METER
											 : SAMPLE_PEAK_METER);
	/* O rótulo de categoria do mixer ("Global"/"Ativo") não informa nada na casca. */
	for (QLabel *label : control->findChildren<QLabel *>()) {
		if (hasStyleClass(label, "mixer-category")) {
			label->hide();
		}
	}
	/* O OBS nomeia só o botão de mudo (VolControl.Mute); o de monitor fica sem nome para leitores de tela. */
	const QString name = QString::fromUtf8(obs_source_get_name(source));
	for (QPushButton *button : control->findChildren<QPushButton *>()) {
		if (hasStyleClass(button, "btn-monitor")) {
			button->setAccessibleName(QTStr("Voltz.Audio.Monitor") + QStringLiteral(" — ") + name);
		}
	}
	controls_->addWidget(control);
}

} // namespace voltz
