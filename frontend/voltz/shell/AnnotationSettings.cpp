#include "AnnotationSettings.hpp"

#include "FormFields.hpp"
#include "Icons.hpp"

#include <voltz/annotate/AnnotationController.hpp>
#include <voltz/annotate/DrawModeController.hpp>
#include <voltz/core/annotation-font.hpp>
#include <voltz/core/annotation-raster.hpp>

#include <OBSApp.hpp> // QTStr

#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <iterator>

namespace voltz {

namespace {

constexpr int kFadeChoices[] = {0, 3, 5, 10, 30, 60};

bool isFadeChoice(int seconds)
{
	return std::find(std::begin(kFadeChoices), std::end(kFadeChoices), seconds) != std::end(kFadeChoices);
}

// Texto da prévia, nunca maior que isto (pixels lógicos), para caber.
constexpr int kPreviewTextMax = 40;

struct ToolChoice {
	AnnotationTool tool;
	const char *text;
};

const ToolChoice kToolChoices[] = {
	{AnnotationTool::Pen, "Voltz.Draw.Tool.Pen"},
	{AnnotationTool::Highlighter, "Voltz.Draw.Tool.Highlighter"},
	{AnnotationTool::Line, "Voltz.Draw.Tool.Line"},
	{AnnotationTool::Arrow, "Voltz.Draw.Tool.Arrow"},
	{AnnotationTool::Rectangle, "Voltz.Draw.Tool.Rectangle"},
	{AnnotationTool::Ellipse, "Voltz.Draw.Tool.Ellipse"},
	{AnnotationTool::Text, "Voltz.Draw.Tool.Text"},
};

QColor toQColor(const RgbColor &color)
{
	return QColor(color.r, color.g, color.b);
}

RgbColor fromQColor(const QColor &color)
{
	return RgbColor{static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
			static_cast<uint8_t>(color.blue())};
}

QIcon swatch(const RgbColor &color)
{
	QPixmap pixmap(14, 14);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(QColor(0x3A, 0x3A, 0x42), 1.0));
	painter.setBrush(toQColor(color));
	painter.drawRoundedRect(QRectF(0.5, 0.5, 13.0, 13.0), 3.0, 3.0);
	return QIcon(pixmap);
}

void setWarn(QLabel *label, bool warn)
{
	if (label->property("warn").toBool() != warn) {
		label->setProperty("warn", warn);
		label->style()->unpolish(label);
		label->style()->polish(label);
	}
}

} // namespace

AnnotationPreview::AnnotationPreview(QWidget *parent) : QWidget(parent)
{
	setFixedSize(240, 120);
	setAccessibleName(QTStr("Voltz.Annot.Preview"));
}

void AnnotationPreview::setAnnotationStyle(const AnnotationStyle &style)
{
	style_ = style;
	update();
}

void AnnotationPreview::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(QColor(0x1F, 0x1F, 0x24), 1.0));
	painter.setBrush(QColor(0x09, 0x09, 0x0B));
	painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);

	/* pixels lógicos × devicePixelRatio: a prévia sai nítida a 125/150% */
	const double ratio = devicePixelRatioF();
	QImage image(static_cast<int>(std::lround(width() * ratio)), static_cast<int>(std::lround(height() * ratio)),
		     QImage::Format_ARGB32_Premultiplied); /* B, G, R, A na memória: o RasterImage do núcleo */
	image.fill(Qt::transparent);
	const RasterImage raster{image.bits(), image.width(), image.height(), static_cast<int>(image.bytesPerLine())};

	Stroke stroke;
	stroke.tool = style_.tool == AnnotationTool::Text ? AnnotationTool::Pen : style_.tool;
	stroke.color = style_.color;
	stroke.width = style_.width;
	if (stroke.tool == AnnotationTool::Pen || stroke.tool == AnnotationTool::Highlighter) {
		std::vector<StrokePoint> wave;
		for (int x = 18; x <= 130; x += 4) {
			wave.push_back(StrokePoint{x, static_cast<int>(60 + 22 * std::sin(x / 18.0))});
		}
		appendStrokePoints(stroke, wave);
	} else {
		appendStrokePoints(stroke, {StrokePoint{22, 96}, StrokePoint{126, 24}});
	}
	rasterizeStroke(raster, stroke, ratio, ratio);

	Stroke text;
	text.tool = AnnotationTool::Text;
	text.color = style_.color;
	text.textSize = std::min(style_.textSize, kPreviewTextMax);
	text.textFont = style_.textFont;
	text.text = "Aa";
	text.points = {StrokePoint{150, 60 - text.textSize / 2}};
	rasterizeStroke(raster, text, ratio, ratio);

	image.setDevicePixelRatio(ratio);
	painter.drawImage(QPointF(0.0, 0.0), image);
}

AnnotationSettingsSection::AnnotationSettingsSection(AnnotationController *annotations, DrawModeController *drawMode,
						     QWidget *parent)
	: QFrame(parent),
	  annotations_(annotations),
	  drawMode_(drawMode)
{
	setObjectName("panel");
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(16, 14, 16, 16);
	layout->setSpacing(10);
	auto *title = new QLabel(QTStr("Voltz.Settings.Annotations").toUpper(), this);
	title->setObjectName("panelTitle");
	layout->addWidget(title);
	auto *hint = new QLabel(QTStr("Voltz.Annot.Hint"), this);
	hint->setObjectName("muted");
	hint->setWordWrap(true);
	layout->addWidget(hint);

	tool_ = new QComboBox(this);
	tool_->setAccessibleName(QTStr("Voltz.Annot.Tool"));
	for (const ToolChoice &choice : kToolChoices) {
		tool_->addItem(QTStr(choice.text), static_cast<int>(choice.tool));
	}
	color_ = new QPushButton(this);
	color_->setObjectName("colorButton");
	color_->setAccessibleName(QTStr("Voltz.Annot.PickColor"));
	color_->setIconSize(QSize(14, 14));
	width_ = new QSlider(Qt::Horizontal, this);
	width_->setObjectName("mouseSlider");
	width_->setRange(kStrokeWidthMin, kStrokeWidthMax);
	width_->setAccessibleName(QTStr("Voltz.Annot.Width"));
	width_->setMinimumWidth(160);
	widthValue_ = new QLabel(this);
	textSize_ = new QSlider(Qt::Horizontal, this);
	textSize_->setObjectName("mouseSlider");
	textSize_->setRange(kTextSizeMin, kTextSizeMax);
	textSize_->setAccessibleName(QTStr("Voltz.Annot.TextSize"));
	textSize_->setMinimumWidth(160);
	textSizeValue_ = new QLabel(this);
	font_ = new QComboBox(this);
	font_->setAccessibleName(QTStr("Voltz.Annot.TextFont"));
	font_->addItem(QTStr("Voltz.Annot.Font.Bold"), static_cast<int>(TextFont::Bold));
	font_->addItem(QTStr("Voltz.Annot.Font.Regular"), static_cast<int>(TextFont::Regular));
	fade_ = new QComboBox(this);
	fade_->setAccessibleName(QTStr("Voltz.Annot.Fade"));
	for (int seconds : kFadeChoices) {
		fade_->addItem(seconds > 0 ? QTStr("Voltz.Annot.Fade.Seconds").arg(seconds)
					   : QTStr("Voltz.Annot.Fade.Never"),
			       seconds);
	}

	auto *grid = new QGridLayout();
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setHorizontalSpacing(16);
	grid->setVerticalSpacing(10);
	grid->setColumnStretch(1, 1);
	int row = 0;
	auto addRow = [this, grid, &row](const QString &label, QWidget *field) {
		grid->addWidget(formLabel(label), row, 0, Qt::AlignLeft | Qt::AlignVCenter);
		grid->addWidget(field, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
		row++;
	};
	auto addValueRow = [this, grid, &row](const QString &label, QWidget *field, QLabel *value) {
		value->setObjectName("muted");
		value->setMinimumWidth(56);
		auto *line = new QHBoxLayout();
		line->setSpacing(10);
		line->addWidget(field, 1);
		line->addWidget(value);
		grid->addWidget(formLabel(label), row, 0, Qt::AlignLeft | Qt::AlignVCenter);
		grid->addLayout(line, row, 1);
		grid->setRowMinimumHeight(row, kFormRowHeight); /* a altura das linhas de caixa */
		row++;
	};
	addRow(QTStr("Voltz.Annot.Tool"), tool_);
	addRow(QTStr("Voltz.Annot.Color"), color_);
	addValueRow(QTStr("Voltz.Annot.Width"), width_, widthValue_);
	addValueRow(QTStr("Voltz.Annot.TextSize"), textSize_, textSizeValue_);
	addRow(QTStr("Voltz.Annot.TextFont"), font_);
	addRow(QTStr("Voltz.Annot.Fade"), fade_);
	for (QWidget *field : std::initializer_list<QWidget *>{tool_, width_, textSize_, font_, fade_}) {
		guardWheel(field); /* a roda sem foco rola a página de Ajustes e não muda o estilo */
	}

	preview_ = new AnnotationPreview(this);
	auto *columns = new QHBoxLayout();
	columns->setSpacing(24);
	columns->addLayout(grid, 1);
	columns->addWidget(preview_, 0, Qt::AlignTop);
	layout->addLayout(columns);

	status_ = new QLabel(this);
	status_->setObjectName("annotStatus");
	status_->setWordWrap(true);
	layout->addWidget(status_);
	shortcut_ = new QLabel(this);
	shortcut_->setObjectName("muted");
	shortcut_->setWordWrap(true);
	layout->addWidget(shortcut_);

	connect(tool_, &QComboBox::currentIndexChanged, this, [this]() { commit(); });
	connect(font_, &QComboBox::currentIndexChanged, this, [this]() { commit(); });
	connect(fade_, &QComboBox::currentIndexChanged, this, [this]() { commit(); });
	for (QSlider *slider : {width_, textSize_}) {
		connect(slider, &QSlider::valueChanged, this, [this]() {
			updateValueLabels();
			commit();
		});
	}
	connect(color_, &QPushButton::clicked, this, [this]() { pickColor(); });
	connect(annotations_, &AnnotationController::styleChanged, this, [this]() { reload(); });
	connect(annotations_, &AnnotationController::burnedChanged, this, [this](bool) { reload(); });
	reload();
}

QLabel *AnnotationSettingsSection::formLabel(const QString &text)
{
	auto *label = new QLabel(text, this);
	formLabels_.append(label);
	return label;
}

void AnnotationSettingsSection::pickColor()
{
	const QColor chosen = QColorDialog::getColor(toQColor(colorValue_), this, QTStr("Voltz.Annot.PickColor"));
	if (!chosen.isValid()) {
		return;
	}
	colorValue_ = fromQColor(chosen);
	commit();
}

void AnnotationSettingsSection::updateValueLabels()
{
	widthValue_->setText(QTStr("Voltz.Annot.Pixels").arg(width_->value()));
	textSizeValue_->setText(QTStr("Voltz.Annot.Pixels").arg(textSize_->value()));
}

void AnnotationSettingsSection::selectFade(int seconds)
{
	/* valor de antes (ou do user.ini) fora da lista: entra na lista, em ordem, e só um por vez (sai o anterior) */
	for (int i = fade_->count() - 1; i >= 0; i--) {
		const int value = fade_->itemData(i).toInt();
		if (value != seconds && !isFadeChoice(value)) {
			fade_->removeItem(i);
		}
	}
	int index = fade_->findData(seconds);
	if (index < 0) {
		index = 0;
		while (index < fade_->count() && fade_->itemData(index).toInt() < seconds) {
			index++;
		}
		fade_->insertItem(index, QTStr("Voltz.Annot.Fade.Seconds").arg(seconds), seconds);
	}
	fade_->setCurrentIndex(index);
}

void AnnotationSettingsSection::commit()
{
	if (loading_) {
		return;
	}
	AnnotationStyle style = annotations_->style();
	style.tool = static_cast<AnnotationTool>(tool_->currentData().toInt());
	style.color = colorValue_;
	style.width = width_->value();
	style.textSize = textSize_->value();
	style.textFont = static_cast<TextFont>(font_->currentData().toInt());
	style.fadeSeconds = fade_->currentData().toInt();
	annotations_->setStyle(style); /* styleChanged → reload */
}

void AnnotationSettingsSection::reload()
{
	loading_ = true;
	const AnnotationStyle &style = annotations_->style();
	tool_->setCurrentIndex(tool_->findData(static_cast<int>(style.tool)));
	colorValue_ = style.color;
	color_->setIcon(swatch(colorValue_));
	color_->setText(QString::fromStdString(formatHexColor(colorValue_)));
	width_->setValue(style.width);
	textSize_->setValue(style.textSize);
	font_->setCurrentIndex(font_->findData(static_cast<int>(style.textFont)));
	selectFade(style.fadeSeconds);
	updateValueLabels();
	preview_->setAnnotationStyle(style);
	const bool fontOk = embeddedFontAvailable(TextFont::Regular) && embeddedFontAvailable(TextFont::Bold);
	if (annotations_->burned()) {
		status_->setText(QTStr("Voltz.Annot.Status.Burned"));
	} else if (!fontOk) {
		status_->setText(QTStr("Voltz.Annot.Status.NoFont"));
	} else {
		status_->setText(QTStr("Voltz.Annot.Status.Ok"));
	}
	setWarn(status_, annotations_->burned() || !fontOk);
	shortcut_->setText(QTStr("Voltz.Annot.Shortcut").arg(drawMode_->drawShortcutText()));
	loading_ = false;
}

} // namespace voltz
