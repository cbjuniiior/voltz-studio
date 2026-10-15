#include "PresetThumbnail.hpp"

#include "Icons.hpp"

#include <voltz/core/preset-layout.hpp>

#include <QPainter>
#include <QPainterPath>

#include "moc_PresetThumbnail.cpp"

namespace voltz {

namespace {

QRectF toWidget(const LayoutRect &r, const QRectF &frame, CanvasSize canvas)
{
	const double sx = frame.width() / canvas.width;
	const double sy = frame.height() / canvas.height;
	return QRectF(frame.x() + r.x * sx, frame.y() + r.y * sy, r.w * sx, r.h * sy);
}

void drawScreen(QPainter &p, const QRectF &rect)
{
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(0x2A, 0x2A, 0x31));
	p.drawRoundedRect(rect, 3, 3);
	p.setBrush(QColor(0x3A, 0x3A, 0x42));
	const QRectF bar(rect.x() + rect.width() * 0.08, rect.y() + rect.height() * 0.14, rect.width() * 0.5,
			 rect.height() * 0.08);
	p.drawRoundedRect(bar, 2, 2);
}

void drawCamera(QPainter &p, const QRectF &rect, bool round)
{
	p.setPen(QPen(palette::kAccent, 1.5));
	p.setBrush(QColor(0xD4, 0xF5, 0x3C, 40));
	if (round) {
		p.drawEllipse(rect);
	} else {
		p.drawRoundedRect(rect, 3, 3);
	}
}

} // namespace

PresetThumbnail::PresetThumbnail(PresetId preset, QWidget *parent) : QWidget(parent), preset_(preset)
{
	setMinimumSize(192, 108);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize PresetThumbnail::sizeHint() const
{
	return QSize(256, 144);
}

void PresetThumbnail::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	const CanvasSize canvas{1920, 1080};
	QRectF frame(0, 0, width(), width() * 9.0 / 16.0);
	if (frame.height() > height()) {
		frame = QRectF(0, 0, height() * 16.0 / 9.0, height());
	}
	frame.moveCenter(rect().center());

	p.setPen(QPen(QColor(0x1F, 0x1F, 0x24), 1));
	p.setBrush(QColor(0x09, 0x09, 0x0B));
	p.drawRoundedRect(frame, 6, 6);

	switch (preset_) {
	case PresetId::Tutorial:
		drawScreen(p, toWidget(fullCanvas(canvas), frame, canvas).adjusted(4, 4, -4, -4));
		drawCamera(p, toWidget(tutorialCamera(canvas), frame, canvas), true);
		break;
	case PresetId::ScreenCamera: {
		QRectF left = frame;
		left.setWidth(frame.width() / 2 - 3);
		QRectF right = left;
		right.moveLeft(frame.x() + frame.width() / 2 + 3);
		drawScreen(p, left.adjusted(4, 4, -2, -4));
		drawCamera(p, right.adjusted(2, 4, -4, -4), false);
		break;
	}
	case PresetId::SideBySide:
		drawScreen(p, toWidget(sideBySideScreen(canvas), frame, canvas));
		drawCamera(p, toWidget(sideBySideCamera(canvas), frame, canvas), false);
		break;
	case PresetId::Opening: {
		p.setPen(Qt::NoPen);
		p.setBrush(palette::kAccent);
		const QRectF title(frame.center().x() - frame.width() * 0.2, frame.center().y() - frame.height() * 0.05,
				   frame.width() * 0.4, frame.height() * 0.1);
		p.drawRoundedRect(title, 3, 3);
		break;
	}
	}
}

} // namespace voltz
