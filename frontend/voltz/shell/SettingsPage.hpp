#pragma once

#include <QList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace voltz {

class DeviceSelector;
class ElidedLabel;
class FrontendBridge;
class HotkeySettingsSection;
class MouseSettingsSection;
class MouseStyleStore;

// Tela "Ajustes": Gravação, Captura, Mouse (destaque do mouse), Atalhos e Sobre.
// Aplica cada mudança na hora; trava a seção Gravação enquanto grava.
class SettingsPage : public QWidget {
	Q_OBJECT

public:
	explicit SettingsPage(FrontendBridge *bridge, MouseStyleStore *mouseStyles, QWidget *parent = nullptr);

protected:
	void showEvent(QShowEvent *event) override;

private:
	QFrame *buildRecordingSection();
	QFrame *buildCaptureSection();
	QFrame *buildAboutSection();
	QFrame *makeSection(const QString &title, QVBoxLayout **body);
	QLabel *formLabel(const QString &text, QWidget *parent);
	void alignFormLabels();
	void loadRecording();
	void setRecordingLocked(bool locked);

	FrontendBridge *bridge_;
	QVBoxLayout *sections_ = nullptr;
	QFrame *recording_ = nullptr;
	QLabel *recordingNotice_ = nullptr;
	ElidedLabel *folder_ = nullptr;
	QPushButton *changeFolder_ = nullptr;
	QComboBox *format_ = nullptr;
	QComboBox *quality_ = nullptr;
	QLabel *encoder_ = nullptr;
	QCheckBox *programOnly_ = nullptr;
	DeviceSelector *monitor_ = nullptr;
	DeviceSelector *camera_ = nullptr;
	DeviceSelector *microphone_ = nullptr;
	QLabel *captureHint_ = nullptr;
	QCheckBox *hideFromCapture_ = nullptr;
	MouseSettingsSection *mouse_ = nullptr;
	HotkeySettingsSection *hotkeys_ = nullptr;
	// Rótulos dos formulários de todas as seções: recebem a mesma largura para alinhar as colunas dos campos.
	QList<QLabel *> formLabels_;
	bool loading_ = false;
};

} // namespace voltz
