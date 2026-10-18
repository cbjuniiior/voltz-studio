#pragma once

#include <voltz/core/annotation-model.hpp>

#include <QFrame>
#include <QList>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

namespace voltz {

class AnnotationController;
class DrawModeController;

// Prévia do estilo das anotações com o rasterizador do núcleo (o mesmo da sobreposição e da fonte do plugin): um traço
// da ferramenta escolhida e "Aa" na fonte do texto, sobre o fundo da casca.
class AnnotationPreview : public QWidget {
public:
	explicit AnnotationPreview(QWidget *parent = nullptr);
	void setAnnotationStyle(const AnnotationStyle &style);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	AnnotationStyle style_;
};

// Ajustes › Anotações (spec §4.3, Plano 4D): o estilo com que o modo desenho começa — ferramenta, cor (QColorDialog: fora
// do modo desenho, um diálogo modal não atrapalha), espessura, tamanho e fonte do texto, "traços somem após" — com
// prévia; o estado das anotações (fora da gravação da tela ou "queimadas", e a fonte do texto) e o atalho de desenhar.
// Cada mudança vale na hora (AnnotationController::setStyle, a mesma da barra; o user.ini é gravado 400 ms depois). Os
// rótulos ficam em formLabels() para o SettingsPage alinhar as colunas. Thread da UI.
class AnnotationSettingsSection : public QFrame {
public:
	AnnotationSettingsSection(AnnotationController *annotations, DrawModeController *drawMode,
				  QWidget *parent = nullptr);

	const QList<QLabel *> &formLabels() const { return formLabels_; }
	// Relê o estilo, o estado e o atalho.
	void reload();

private:
	QLabel *formLabel(const QString &text);
	void pickColor();
	void commit();
	void updateValueLabels();
	void selectFade(int seconds);

	AnnotationController *annotations_;
	DrawModeController *drawMode_;
	QComboBox *tool_ = nullptr;
	QPushButton *color_ = nullptr;
	QSlider *width_ = nullptr;
	QLabel *widthValue_ = nullptr;
	QSlider *textSize_ = nullptr;
	QLabel *textSizeValue_ = nullptr;
	QComboBox *font_ = nullptr;
	QComboBox *fade_ = nullptr;
	AnnotationPreview *preview_ = nullptr;
	QLabel *status_ = nullptr;
	QLabel *shortcut_ = nullptr;
	RgbColor colorValue_;
	QList<QLabel *> formLabels_;
	bool loading_ = false;
};

} // namespace voltz
