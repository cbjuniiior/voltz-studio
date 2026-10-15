#pragma once

#include <voltz/core/recording-clock.hpp>

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace voltz {

class FrontendBridge;
class ScenesPanel;
class VoltzPreview;

// Tela "Gravar" (layout H2): cenas | prévia | coluna lateral (fontes + áudio), barra inferior com
// Pausar/Gravar/tempo.
class RecordPage : public QWidget {
	Q_OBJECT

public:
	explicit RecordPage(FrontendBridge *bridge, QWidget *parent = nullptr);

private:
	void updateRecordingUi(bool active, bool paused);
	void showRecordingTransition(bool stopping);
	void tickClock();

	FrontendBridge *bridge_;
	ScenesPanel *scenes_ = nullptr;
	VoltzPreview *preview_ = nullptr;
	QVBoxLayout *centerColumn_ = nullptr;
	QVBoxLayout *sideColumn_ = nullptr;
	QPushButton *pauseButton_ = nullptr;
	QPushButton *recordButton_ = nullptr;
	QLabel *timeLabel_ = nullptr;

	QTimer ticker_;
	QElapsedTimer monotonic_;
	RecordingClock recordingClock_;
	bool pendingStart_ = false;
};

} // namespace voltz
