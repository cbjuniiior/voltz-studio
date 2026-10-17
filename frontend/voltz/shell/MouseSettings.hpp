#pragma once

#include <voltz/core/mouse-style.hpp>

#include <QFrame>
#include <QList>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

namespace voltz {

class MousePreview;
class MouseStyleStore;

// Ajustes › Mouse (spec §6.1): halo (cor, tamanho, opacidade, forma), onda do clique (cor, cor do botão direito),
// holofote (raio, escurecimento, atalho), prévia ao vivo e estilos salvos. Cada mudança vale na hora
// (MouseStyleStore). Os rótulos do formulário ficam em formLabels() para o SettingsPage alinhar as colunas.
class MouseSettingsSection : public QFrame {
public:
	MouseSettingsSection(MouseStyleStore *store, QWidget *parent = nullptr);

	const QList<QLabel *> &formLabels() const { return formLabels_; }
	// Relê o estilo, o atalho e a lista de estilos salvos (o atalho pode ter mudado em Configurações).
	void reload();

private:
	QLabel *formLabel(const QString &text);
	QSlider *makeSlider(int minimum, int maximum, const QString &accessibleName);
	QPushButton *makeColorButton(const QString &accessibleName);
	void setColorButton(QPushButton *button, const RgbColor &color);
	void pickColor(RgbColor &value, QPushButton *button);
	void updateValueLabels();
	void updateEnabled();
	void commit();
	void reloadSaved();

	MouseStyleStore *store_;
	MousePreview *preview_ = nullptr;
	QCheckBox *haloActive_ = nullptr;
	QPushButton *haloColor_ = nullptr;
	QSlider *haloSize_ = nullptr;
	QLabel *haloSizeValue_ = nullptr;
	QSlider *haloOpacity_ = nullptr;
	QLabel *haloOpacityValue_ = nullptr;
	QComboBox *haloShape_ = nullptr;
	QCheckBox *clickActive_ = nullptr;
	QPushButton *clickColor_ = nullptr;
	QPushButton *rightColor_ = nullptr;
	QCheckBox *spotlightActive_ = nullptr;
	QSlider *spotlightRadius_ = nullptr;
	QLabel *spotlightRadiusValue_ = nullptr;
	QSlider *spotlightDim_ = nullptr;
	QLabel *spotlightDimValue_ = nullptr;
	QLabel *shortcut_ = nullptr;
	QComboBox *saved_ = nullptr;
	QPushButton *applySaved_ = nullptr;
	QPushButton *removeSaved_ = nullptr;
	RgbColor haloColorValue_;
	RgbColor clickColorValue_;
	RgbColor rightColorValue_;
	QList<QLabel *> formLabels_;
	bool loading_ = false;
};

} // namespace voltz
