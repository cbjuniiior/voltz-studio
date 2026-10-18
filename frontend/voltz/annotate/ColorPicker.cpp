#include "ColorPicker.hpp"

#include <QColor>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

#include "moc_ColorPicker.cpp"

namespace voltz {

namespace {

constexpr int kWidth = 216;
constexpr int kSquareHeight = 96;
constexpr int kGap = 8;
constexpr int kHueHeight = 14;

QColor toQColor(const RgbColor &color)
{
	return QColor(color.r, color.g, color.b);
}

} // namespace

ColorPicker::ColorPicker(QWidget *parent) : QWidget(parent)
{
	setObjectName("drawColorPicker");
	setFocusPolicy(Qt::NoFocus);
	setAttribute(Qt::WA_OpaquePaintEvent, false);
	setCursor(Qt::CrossCursor);
	setFixedSize(sizeHint());
}

QSize ColorPicker::sizeHint() const
{
	return QSize(kWidth, kSquareHeight + kGap + kHueHeight);
}

QRectF ColorPicker::squareRect() const
{
	return QRectF(0.0, 0.0, kWidth, kSquareHeight);
}

QRectF ColorPicker::hueRect() const
{
	return QRectF(0.0, kSquareHeight + kGap, kWidth, kHueHeight);
}

void ColorPicker::setColor(const RgbColor &color)
{
	/* a cor que o próprio seletor acabou de escolher: a volta por RGB derivaria a marca (preto puro → saturação 0) */
	if (color == this->color()) {
		return;
	}
	const QColor hsv = toQColor(color).toHsv();
	if (hsv.hsvHueF() >= 0.0) {
		hue_ = hsv.hsvHueF(); /* cinza não tem matiz: fica a anterior */
	}
	saturation_ = hsv.hsvSaturationF();
	value_ = hsv.valueF();
	update();
}

RgbColor ColorPicker::color() const
{
	const QColor rgb =
		QColor::fromHsvF(static_cast<float>(hue_), static_cast<float>(saturation_), static_cast<float>(value_))
			.toRgb();
	return RgbColor{static_cast<uint8_t>(rgb.red()), static_cast<uint8_t>(rgb.green()),
			static_cast<uint8_t>(rgb.blue())};
}

void ColorPicker::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QRectF square = squareRect();
	if (square_.isNull() || squareHue_ != hue_) {
		/* saturação na horizontal (0 → 1), brilho na vertical (1 → 0) */
		square_ = QImage(kWidth, kSquareHeight, QImage::Format_RGB32);
		for (int y = 0; y < kSquareHeight; y++) {
			const double value = 1.0 - static_cast<double>(y) / (kSquareHeight - 1);
			for (int x = 0; x < kWidth; x++) {
				const double saturation = static_cast<double>(x) / (kWidth - 1);
				square_.setPixelColor(x, y,
						      QColor::fromHsvF(static_cast<float>(hue_),
								       static_cast<float>(saturation),
								       static_cast<float>(value)));
			}
		}
		squareHue_ = hue_;
	}
	QPainterPath clip;
	clip.addRoundedRect(square, 6.0, 6.0);
	painter.save();
	painter.setClipPath(clip);
	painter.drawImage(square.topLeft(), square_);
	painter.restore();

	const QRectF hue = hueRect();
	QLinearGradient gradient(hue.topLeft(), hue.topRight());
	for (int i = 0; i <= 6; i++) {
		gradient.setColorAt(i / 6.0, QColor::fromHsvF(static_cast<float>(i % 6) / 6.0f, 1.0f, 1.0f));
	}
	painter.setPen(Qt::NoPen);
	painter.setBrush(gradient);
	painter.drawRoundedRect(hue, kHueHeight / 2.0, kHueHeight / 2.0);

	/* marcas: anel no ponto escolhido do quadrado e na matiz */
	const QPointF mark(square.left() + saturation_ * square.width(),
			   square.top() + (1.0 - value_) * square.height());
	painter.setBrush(Qt::NoBrush);
	painter.setPen(QPen(QColor(0, 0, 0, 160), 3.0));
	painter.drawEllipse(mark, 6.0, 6.0);
	painter.setPen(QPen(Qt::white, 1.5));
	painter.drawEllipse(mark, 6.0, 6.0);
	const QPointF hueMark(hue.left() + hue_ * hue.width(), hue.center().y());
	painter.setPen(QPen(QColor(0, 0, 0, 160), 3.0));
	painter.drawEllipse(hueMark, kHueHeight / 2.0, kHueHeight / 2.0);
	painter.setPen(QPen(Qt::white, 1.5));
	painter.drawEllipse(hueMark, kHueHeight / 2.0, kHueHeight / 2.0);
}

void ColorPicker::pickAt(const QPointF &position)
{
	if (drag_ == Drag::Square) {
		const QRectF square = squareRect();
		saturation_ = std::clamp((position.x() - square.left()) / square.width(), 0.0, 1.0);
		value_ = std::clamp(1.0 - (position.y() - square.top()) / square.height(), 0.0, 1.0);
	} else if (drag_ == Drag::Hue) {
		const QRectF hue = hueRect();
		hue_ = std::clamp((position.x() - hue.left()) / hue.width(), 0.0, 0.9999);
	}
	update();
}

void ColorPicker::mousePressEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton) {
		QWidget::mousePressEvent(event);
		return;
	}
	const QPointF position = event->position();
	if (squareRect().contains(position)) {
		drag_ = Drag::Square;
	} else if (hueRect().adjusted(0.0, -kGap / 2.0, 0.0, 0.0).contains(position)) {
		drag_ = Drag::Hue;
	} else {
		drag_ = Drag::None;
		return;
	}
	pickAt(position);
}

void ColorPicker::mouseMoveEvent(QMouseEvent *event)
{
	if (drag_ != Drag::None) {
		pickAt(event->position());
	}
}

void ColorPicker::mouseReleaseEvent(QMouseEvent *event)
{
	if (drag_ == Drag::None || event->button() != Qt::LeftButton) {
		return;
	}
	pickAt(event->position());
	drag_ = Drag::None;
	emit colorPicked(color());
}

} // namespace voltz
