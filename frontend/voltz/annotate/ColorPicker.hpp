#pragma once

#include <voltz/core/mouse-style.hpp>

#include <QImage>
#include <QWidget>

namespace voltz {

// Seletor de cor completo da barra do modo desenho (Plano 4D): quadrado saturação × brilho e faixa de matiz, desenhados
// à mão. Sem diálogo — um QColorDialog é modal, pegaria o foco e a janela de entrada sairia do modo desenho (perda de
// ativação) — e sem foco nunca (a barra nunca fica ativa). Apertar e arrastar escolhe; colorPicked ao soltar (uma
// mudança de estilo por escolha). Thread da UI.
class ColorPicker : public QWidget {
	Q_OBJECT

public:
	explicit ColorPicker(QWidget *parent = nullptr);

	// Marca a cor (sem sinal).
	void setColor(const RgbColor &color);
	RgbColor color() const;
	QSize sizeHint() const override;

signals:
	void colorPicked(voltz::RgbColor color);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	enum class Drag { None, Square, Hue };

	QRectF squareRect() const;
	QRectF hueRect() const;
	void pickAt(const QPointF &position);

	double hue_ = 0.2;
	double saturation_ = 0.75;
	double value_ = 0.96;
	Drag drag_ = Drag::None;
	QImage square_; // cache do quadrado para a matiz squareHue_
	double squareHue_ = -1.0;
};

} // namespace voltz
