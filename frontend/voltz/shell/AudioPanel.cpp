#include "AudioPanel.hpp"

#include <components/VolumeControl.hpp>
#include <widgets/OBSBasic.hpp>

#include <OBSApp.hpp> // QTStr

#include <QLabel>
#include <QLayoutItem>
#include <QVBoxLayout>

#include "moc_AudioPanel.cpp"

namespace voltz {

namespace {

constexpr uint32_t kDesktopChannel = 1;
constexpr uint32_t kMicChannel = 3;

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
	while (QLayoutItem *item = controls_->takeAt(0)) {
		delete item->widget();
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

	auto *control = new VolumeControl(source, this, false);
	control->setGlobalInMixer(true);
	control->enableSlider(true);

	config_t *config = OBSBasic::Get()->Config();
	control->setMeterDecayRate(config_get_double(config, "Audio", "MeterDecayRate"));
	control->setPeakMeterType(config_get_uint(config, "Audio", "PeakMeterType") == 1 ? TRUE_PEAK_METER
											 : SAMPLE_PEAK_METER);
	controls_->addWidget(control);
}

} // namespace voltz
