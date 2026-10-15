#pragma once

#include <voltz/core/recording-clock.hpp>

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

namespace voltz {

class FrontendBridge;
class VoltzPreview;

// Tela "Gravar": cenas | prévia | painel lateral, e barra inferior com Pausar/Gravar/tempo.
class RecordPage : public QWidget {
	Q_OBJECT

public:
	explicit RecordPage(FrontendBridge *bridge, QWidget *parent = nullptr);

private:
	void refreshScenes();
	void selectScene(const QString &name);
	void updateRecordingUi(bool active, bool paused);
	void tickClock();

	FrontendBridge *bridge_;
	QListWidget *scenes_ = nullptr;
	VoltzPreview *preview_ = nullptr;
	QPushButton *pauseButton_ = nullptr;
	QPushButton *recordButton_ = nullptr;
	QLabel *timeLabel_ = nullptr;

	QTimer ticker_;
	QElapsedTimer monotonic_;
	RecordingClock recordingClock_;
	bool updatingList_ = false;
};

} // namespace voltz
