#pragma once

#include <voltz/core/recording-clock.hpp>

#include <QElapsedTimer>
#include <QStringList>
#include <QTimer>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

namespace voltz {

class ErrorBanner;
class FrontendBridge;
class ProjectRecorder;
class ScenesPanel;
class VoltzPreview;

// Tela "Gravar" (layout H2): cenas | prévia | coluna lateral (fontes + áudio). Barra inferior (spec §4.2): nome do
// projeto · Marcador · Pausar · Gravar + tempo · estimativa de espaço. Faixas na prévia: erro de gravação e disco.
class RecordPage : public QWidget {
	Q_OBJECT

public:
	RecordPage(FrontendBridge *bridge, ProjectRecorder *recorder, QWidget *parent = nullptr);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void updateRecordingUi(bool active, bool paused);
	void showRecordingTransition(bool stopping);
	void showRecordingError(int code, const QString &lastError);
	void tickClock();
	void showMarkerToast(int number);
	void reloadProjectNames();
	void commitProjectName();
	void refreshDiskInfo();
	void showIsoFailure(const QString &track, const QString &reason);

	FrontendBridge *bridge_;
	ProjectRecorder *recorder_;
	ScenesPanel *scenes_ = nullptr;
	VoltzPreview *preview_ = nullptr;
	ErrorBanner *errorBanner_ = nullptr;
	ErrorBanner *diskBanner_ = nullptr;
	ErrorBanner *isoBanner_ = nullptr;
	QComboBox *projectName_ = nullptr;
	QPushButton *markerButton_ = nullptr;
	QPushButton *pauseButton_ = nullptr;
	QPushButton *recordButton_ = nullptr;
	QLabel *timeLabel_ = nullptr;
	QLabel *markerToast_ = nullptr;
	QLabel *spaceLabel_ = nullptr;

	QTimer ticker_;
	QTimer diskTimer_;
	QTimer markerToastTimer_;
	QElapsedTimer monotonic_;
	RecordingClock recordingClock_;
	bool pendingStart_ = false;
	bool diskBannerDismissed_ = false;
	// Faixas ISO que falharam no projeto atual ("a câmera (motivo)"), na ordem em que falharam.
	QStringList isoFailures_;
};

} // namespace voltz
