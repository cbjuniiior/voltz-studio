#pragma once

#include <QWidget>

class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace voltz {

class DeviceSelector;
class FrontendBridge;

// Tela "Ajustes": Gravação (pasta, formato, qualidade, codificador automático), Captura (Task 7) e Sobre.
// Aplica cada mudança na hora; trava a seção Gravação enquanto grava.
class SettingsPage : public QWidget {
	Q_OBJECT

public:
	explicit SettingsPage(FrontendBridge *bridge, QWidget *parent = nullptr);

protected:
	void showEvent(QShowEvent *event) override;

private:
	QFrame *buildRecordingSection();
	QFrame *buildCaptureSection();
	QFrame *buildAboutSection();
	QFrame *makeSection(const QString &title, QVBoxLayout **body);
	void loadRecording();
	void setRecordingLocked(bool locked);

	FrontendBridge *bridge_;
	QVBoxLayout *sections_ = nullptr;
	QFrame *recording_ = nullptr;
	QLabel *recordingNotice_ = nullptr;
	QLabel *folder_ = nullptr;
	QPushButton *changeFolder_ = nullptr;
	QComboBox *format_ = nullptr;
	QComboBox *quality_ = nullptr;
	QLabel *encoder_ = nullptr;
	DeviceSelector *monitor_ = nullptr;
	DeviceSelector *camera_ = nullptr;
	DeviceSelector *microphone_ = nullptr;
	QLabel *captureHint_ = nullptr;
	bool loading_ = false;
};

} // namespace voltz
