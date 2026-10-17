#include "MousePreview.hpp"

#include <OBSApp.hpp> // QTStr

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QRadialGradient>

#include <algorithm>
#include <cstdint>

namespace voltz {

namespace {

constexpr double kPreviewCaptureHeight = 540.0; // a prévia mostra 540 px de altura da captura (1 px = 3 px)

QColor toQColor(const RgbColor &color, double alpha)
{
	QColor result(color.r, color.g, color.b);
	result.setAlphaF(static_cast<float>(std::clamp(alpha, 0.0, 1.0)));
	return result;
}

// Anel: traço centrado entre o raio interno e o externo.
void drawRing(QPainter &painter, const QPointF &center, double outer, double inner, const QColor &color)
{
	const double thickness = std::max(1.0, outer - inner);
	painter.setPen(QPen(color, thickness));
	painter.setBrush(Qt::NoBrush);
	const double radius = outer - thickness / 2.0;
	painter.drawEllipse(center, radius, radius);
}

} // namespace

MousePreview::MousePreview(QWidget *parent) : QWidget(parent)
{
	setObjectName("mousePreview");
	setAccessibleName(QTStr("Voltz.Mouse.Preview"));
	setFixedSize(320, 180);
	setMouseTracking(true);
	clock_.start();
	animation_.setInterval(16);
	connect(&animation_, &QTimer::timeout, this, [this]() { tick(); });
}

void MousePreview::setMouseStyle(const MouseStyle &style)
{
	style_ = style;
	update();
}

double MousePreview::scale() const
{
	return static_cast<double>(height()) / kPreviewCaptureHeight;
}

QPointF MousePreview::cursorPosition() const
{
	return hovering_ ? cursor_ : QPointF(width() / 2.0, height() / 2.0);
}

void MousePreview::tick()
{
	pruneRipples(ripples_, clock_.nsecsElapsed());
	if (ripples_.empty()) {
		animation_.stop();
	}
	update();
}

void MousePreview::mouseMoveEvent(QMouseEvent *event)
{
	cursor_ = event->position();
	hovering_ = true;
	update();
}

void MousePreview::mousePressEvent(QMouseEvent *event)
{
	const MouseButton button = event->button() == Qt::RightButton    ? MouseButton::Right
				   : event->button() == Qt::MiddleButton ? MouseButton::Middle
									 : MouseButton::Left;
	const QPointF at = event->position();
	pushRipple(ripples_, Ripple{static_cast<int>(at.x()), static_cast<int>(at.y()), button, clock_.nsecsElapsed()});
	animation_.start();
	update();
}

void MousePreview::leaveEvent(QEvent *event)
{
	QWidget::leaveEvent(event);
	hovering_ = false;
	update();
}

void MousePreview::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QRectF area = rect();
	/* "desktop" de mentira: fundo e duas janelas, para o holofote ter o que escurecer */
	painter.fillRect(area, QColor(0x18, 0x18, 0x1C));
	painter.fillRect(QRectF(area.width() * 0.07, area.height() * 0.12, area.width() * 0.52, area.height() * 0.66),
			 QColor(0x26, 0x26, 0x2C));
	painter.fillRect(QRectF(area.width() * 0.64, area.height() * 0.28, area.width() * 0.29, area.height() * 0.5),
			 QColor(0x2E, 0x2E, 0x35));

	const double s = scale();
	const QPointF cursor = cursorPosition();
	if (style_.spotlight.active) {
		const SpotlightGeometry spot = spotlightGeometry(style_.spotlight);
		const double outer = (spot.radius + spot.feather / 2.0) * s;
		const double inner = std::max(0.0, (spot.radius - spot.feather / 2.0) * s);
		QRadialGradient gradient(cursor, outer);
		gradient.setColorAt(0.0, QColor(0, 0, 0, 0));
		gradient.setColorAt(outer > 0.0 ? inner / outer : 0.0, QColor(0, 0, 0, 0));
		gradient.setColorAt(1.0, QColor(0, 0, 0, static_cast<int>(spot.dim * 255.0 + 0.5)));
		painter.fillRect(area, gradient);
	}
	const int64_t now = clock_.nsecsElapsed();
	for (const Ripple &ripple : ripples_) {
		const RippleFrame frame = rippleAt(style_, now - ripple.startNs);
		if (frame.visible) {
			drawRing(painter, QPointF(ripple.x, ripple.y), frame.radius * s,
				 (frame.radius - frame.thickness) * s,
				 toQColor(rippleColor(style_.click, ripple.button), frame.alpha));
		}
	}
	if (style_.halo.active) {
		const HaloGeometry halo = haloGeometry(style_.halo);
		const QColor color = toQColor(style_.halo.color, style_.halo.opacity);
		const double outer = halo.diameter / 2.0 * s;
		if (halo.innerRadius > 0.0) {
			drawRing(painter, cursor, outer, halo.innerRadius * s, color);
		} else {
			painter.setPen(Qt::NoPen);
			painter.setBrush(color);
			painter.drawEllipse(cursor, outer, outer);
		}
	}
	painter.setPen(QPen(QColor(0x1F, 0x1F, 0x24), 1.0));
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(area.adjusted(0.5, 0.5, -0.5, -0.5));
}

} // namespace voltz
