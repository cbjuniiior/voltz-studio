#pragma once

#include <obs.hpp>

#include <QFrame>

class QLabel;
class QVBoxLayout;

namespace voltz {

// Painel "ÁUDIO": som do sistema (canal 1) e microfone (canal 3) com medidor, volume e mudo.
class AudioPanel : public QFrame {
	Q_OBJECT

public:
	explicit AudioPanel(QWidget *parent = nullptr);

private slots:
	void rebuild();

private:
	void addChannel(uint32_t channel);
	static void handleChannelChange(void *data, calldata_t *params);

	QVBoxLayout *controls_ = nullptr;
	QLabel *empty_ = nullptr;
	OBSSignal channelSignal_;
};

} // namespace voltz
