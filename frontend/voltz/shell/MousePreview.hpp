#pragma once

#include <voltz/core/mouse-effects.hpp>
#include <voltz/core/mouse-style.hpp>

#include <QElapsedTimer>
#include <QPointF>
#include <QTimer>
#include <QWidget>

#include <vector>

namespace voltz {

// Prévia ao vivo de Ajustes › Mouse: um "desktop" de mentira onde o halo segue o mouse quando ele está sobre a prévia
// (senão fica no centro), clicar faz a onda (cor própria no botão direito) e o holofote escurece o resto. Usa as contas
// do núcleo (mouse-effects) na escala 1 px da prévia = 3 px da captura. Só desenha; não lê o mouse fora dela.
class MousePreview : public QWidget {
public:
	explicit MousePreview(QWidget *parent = nullptr);

	void setMouseStyle(const MouseStyle &style);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void leaveEvent(QEvent *event) override;

private:
	double scale() const;
	QPointF cursorPosition() const;
	void tick();

	MouseStyle style_;
	QPointF cursor_;
	bool hovering_ = false;
	std::vector<Ripple> ripples_; // x/y em pixels da prévia
	QElapsedTimer clock_;
	QTimer animation_;
};

} // namespace voltz
