#include "MouseSettings.hpp"

#include "FormFields.hpp"
#include "MousePreview.hpp"
#include "MouseStyleStore.hpp"

#include <OBSApp.hpp> // QTStr

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <cmath>
#include <initializer_list>

namespace voltz {

namespace {

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

} // namespace

MouseSettingsSection::MouseSettingsSection(MouseStyleStore *store, QWidget *parent) : QFrame(parent), store_(store)
{
	setObjectName("panel");
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(16, 14, 16, 16);
	layout->setSpacing(10);
	auto *title = new QLabel(QTStr("Voltz.Settings.Mouse").toUpper(), this);
	title->setObjectName("panelTitle");
	layout->addWidget(title);
	auto *hint = new QLabel(QTStr("Voltz.Mouse.Hint"), this);
	hint->setObjectName("muted");
	hint->setWordWrap(true);
	layout->addWidget(hint);

	haloActive_ = new QCheckBox(QTStr("Voltz.Mouse.HaloShow"), this);
	haloColor_ = makeColorButton(QTStr("Voltz.Mouse.HaloColor"));
	haloSize_ = makeSlider(kHaloSizeMin, kHaloSizeMax, QTStr("Voltz.Mouse.HaloSize"));
	haloSizeValue_ = new QLabel(this);
	haloOpacity_ = makeSlider(static_cast<int>(std::lround(kHaloOpacityMin * 100.0)),
				  static_cast<int>(std::lround(kHaloOpacityMax * 100.0)),
				  QTStr("Voltz.Mouse.HaloOpacity"));
	haloOpacityValue_ = new QLabel(this);
	haloShape_ = new QComboBox(this);
	haloShape_->setAccessibleName(QTStr("Voltz.Mouse.Shape"));
	guardWheel(haloShape_);
	haloShape_->addItem(QTStr("Voltz.Mouse.Shape.Filled"), static_cast<int>(HaloShape::Filled));
	haloShape_->addItem(QTStr("Voltz.Mouse.Shape.Ring"), static_cast<int>(HaloShape::Ring));
	clickActive_ = new QCheckBox(QTStr("Voltz.Mouse.ClickShow"), this);
	clickColor_ = makeColorButton(QTStr("Voltz.Mouse.ClickColor"));
	rightColor_ = makeColorButton(QTStr("Voltz.Mouse.RightColor"));
	spotlightActive_ = new QCheckBox(QTStr("Voltz.Mouse.SpotlightShow"), this);
	spotlightRadius_ = makeSlider(kSpotlightRadiusMin, kSpotlightRadiusMax, QTStr("Voltz.Mouse.SpotlightRadius"));
	spotlightRadiusValue_ = new QLabel(this);
	spotlightDim_ = makeSlider(static_cast<int>(std::lround(kSpotlightDimMin * 100.0)),
				   static_cast<int>(std::lround(kSpotlightDimMax * 100.0)),
				   QTStr("Voltz.Mouse.SpotlightDim"));
	spotlightDimValue_ = new QLabel(this);

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
	addRow(QTStr("Voltz.Mouse.Halo"), haloActive_);
	addRow(QTStr("Voltz.Mouse.Color"), haloColor_);
	addValueRow(QTStr("Voltz.Mouse.Size"), haloSize_, haloSizeValue_);
	addValueRow(QTStr("Voltz.Mouse.Opacity"), haloOpacity_, haloOpacityValue_);
	addRow(QTStr("Voltz.Mouse.Shape"), haloShape_);
	addRow(QTStr("Voltz.Mouse.Click"), clickActive_);
	addRow(QTStr("Voltz.Mouse.Color"), clickColor_);
	addRow(QTStr("Voltz.Mouse.RightButton"), rightColor_);
	addRow(QTStr("Voltz.Mouse.Spotlight"), spotlightActive_);
	addValueRow(QTStr("Voltz.Mouse.Radius"), spotlightRadius_, spotlightRadiusValue_);
	addValueRow(QTStr("Voltz.Mouse.Dim"), spotlightDim_, spotlightDimValue_);

	preview_ = new MousePreview(this);
	auto *columns = new QHBoxLayout();
	columns->setSpacing(24);
	columns->addLayout(grid, 1);
	columns->addWidget(preview_, 0, Qt::AlignTop);
	layout->addLayout(columns);

	shortcut_ = new QLabel(this);
	shortcut_->setObjectName("muted");
	shortcut_->setWordWrap(true);
	layout->addWidget(shortcut_);

	saved_ = new QComboBox(this);
	saved_->setAccessibleName(QTStr("Voltz.Mouse.Styles"));
	guardWheel(saved_);
	applySaved_ = new QPushButton(QTStr("Voltz.Mouse.StyleApply"), this);
	auto *saveButton = new QPushButton(QTStr("Voltz.Mouse.StyleSave"), this);
	removeSaved_ = new QPushButton(QTStr("Voltz.Mouse.StyleDelete"), this);
	auto *savedRow = new QHBoxLayout();
	savedRow->setSpacing(8);
	savedRow->addWidget(formLabel(QTStr("Voltz.Mouse.Styles")));
	savedRow->addSpacing(8); // mesma distância rótulo → campo da grade (16)
	savedRow->addWidget(saved_, 1);
	savedRow->addWidget(applySaved_);
	savedRow->addWidget(saveButton);
	savedRow->addWidget(removeSaved_);
	layout->addSpacing(4);
	layout->addLayout(savedRow);

	for (QCheckBox *box : {haloActive_, clickActive_, spotlightActive_}) {
		connect(box, &QCheckBox::toggled, this, [this]() { commit(); });
	}
	for (QSlider *slider : {haloSize_, haloOpacity_, spotlightRadius_, spotlightDim_}) {
		connect(slider, &QSlider::valueChanged, this, [this]() {
			updateValueLabels();
			commit();
		});
	}
	connect(haloShape_, &QComboBox::currentIndexChanged, this, [this]() { commit(); });
	connect(haloColor_, &QPushButton::clicked, this, [this]() { pickColor(haloColorValue_, haloColor_); });
	connect(clickColor_, &QPushButton::clicked, this, [this]() { pickColor(clickColorValue_, clickColor_); });
	connect(rightColor_, &QPushButton::clicked, this, [this]() { pickColor(rightColorValue_, rightColor_); });
	connect(applySaved_, &QPushButton::clicked, this, [this]() {
		if (saved_->isEnabled()) {
			store_->applySaved(saved_->currentText());
		}
	});
	connect(saveButton, &QPushButton::clicked, this, [this]() {
		bool ok = false;
		const QString name = QInputDialog::getText(this, QTStr("Voltz.Mouse.StyleName.Title"),
							   QTStr("Voltz.Mouse.StyleName.Label"), QLineEdit::Normal,
							   saved_->isEnabled() ? saved_->currentText() : QString(),
							   &ok);
		if (ok && !name.trimmed().isEmpty()) {
			store_->saveCurrentAs(name);
			saved_->setCurrentText(name.trimmed());
		}
	});
	connect(removeSaved_, &QPushButton::clicked, this, [this]() {
		if (saved_->isEnabled()) {
			store_->removeSaved(saved_->currentText());
		}
	});
	connect(store_, &MouseStyleStore::styleChanged, this, [this]() { reload(); });
	connect(store_, &MouseStyleStore::savedStylesChanged, this, [this]() { reloadSaved(); });
	reload();
	reloadSaved();
}

QLabel *MouseSettingsSection::formLabel(const QString &text)
{
	auto *label = new QLabel(text, this);
	formLabels_.append(label);
	return label;
}

QSlider *MouseSettingsSection::makeSlider(int minimum, int maximum, const QString &accessibleName)
{
	auto *slider = new QSlider(Qt::Horizontal, this);
	slider->setObjectName("mouseSlider");
	slider->setRange(minimum, maximum);
	slider->setAccessibleName(accessibleName);
	slider->setMinimumWidth(160);
	guardWheel(slider);
	return slider;
}

QPushButton *MouseSettingsSection::makeColorButton(const QString &accessibleName)
{
	auto *button = new QPushButton(this);
	button->setObjectName("colorButton");
	button->setAccessibleName(accessibleName);
	button->setIconSize(QSize(14, 14));
	return button;
}

void MouseSettingsSection::setColorButton(QPushButton *button, const RgbColor &color)
{
	button->setIcon(swatch(color));
	button->setText(QString::fromStdString(formatHexColor(color)));
}

void MouseSettingsSection::pickColor(RgbColor &value, QPushButton *button)
{
	const QColor chosen = QColorDialog::getColor(toQColor(value), this, QTStr("Voltz.Mouse.PickColor"));
	if (!chosen.isValid()) {
		return;
	}
	value = fromQColor(chosen);
	setColorButton(button, value);
	commit();
}

void MouseSettingsSection::updateValueLabels()
{
	haloSizeValue_->setText(QTStr("Voltz.Mouse.Pixels").arg(haloSize_->value()));
	haloOpacityValue_->setText(QTStr("Voltz.Mouse.Percent").arg(haloOpacity_->value()));
	spotlightRadiusValue_->setText(QTStr("Voltz.Mouse.Pixels").arg(spotlightRadius_->value()));
	spotlightDimValue_->setText(QTStr("Voltz.Mouse.Percent").arg(spotlightDim_->value()));
}

void MouseSettingsSection::updateEnabled()
{
	const bool halo = haloActive_->isChecked();
	for (QWidget *widget : std::initializer_list<QWidget *>{haloColor_, haloSize_, haloOpacity_, haloShape_}) {
		widget->setEnabled(halo);
	}
	clickColor_->setEnabled(clickActive_->isChecked());
	rightColor_->setEnabled(clickActive_->isChecked());
	spotlightRadius_->setEnabled(spotlightActive_->isChecked());
	spotlightDim_->setEnabled(spotlightActive_->isChecked());
}

void MouseSettingsSection::commit()
{
	if (loading_) {
		return;
	}
	updateEnabled();
	MouseStyle style = store_->style();
	style.halo.active = haloActive_->isChecked();
	style.halo.color = haloColorValue_;
	style.halo.size = haloSize_->value();
	style.halo.opacity = haloOpacity_->value() / 100.0;
	style.halo.shape = static_cast<HaloShape>(haloShape_->currentData().toInt());
	style.click.active = clickActive_->isChecked();
	style.click.color = clickColorValue_;
	style.click.rightColor = rightColorValue_;
	style.spotlight.active = spotlightActive_->isChecked();
	style.spotlight.radius = spotlightRadius_->value();
	style.spotlight.dim = spotlightDim_->value() / 100.0;
	store_->setStyle(style);
}

void MouseSettingsSection::reload()
{
	loading_ = true;
	const MouseStyle &style = store_->style();
	haloActive_->setChecked(style.halo.active);
	haloColorValue_ = style.halo.color;
	setColorButton(haloColor_, haloColorValue_);
	haloSize_->setValue(style.halo.size);
	haloOpacity_->setValue(static_cast<int>(std::lround(style.halo.opacity * 100.0)));
	haloShape_->setCurrentIndex(haloShape_->findData(static_cast<int>(style.halo.shape)));
	clickActive_->setChecked(style.click.active);
	clickColorValue_ = style.click.color;
	setColorButton(clickColor_, clickColorValue_);
	rightColorValue_ = style.click.rightColor;
	setColorButton(rightColor_, rightColorValue_);
	spotlightActive_->setChecked(style.spotlight.active);
	spotlightRadius_->setValue(style.spotlight.radius);
	spotlightDim_->setValue(static_cast<int>(std::lround(style.spotlight.dim * 100.0)));
	updateValueLabels();
	updateEnabled();
	preview_->setMouseStyle(style);
	const QString keys = store_->spotlightShortcutText();
	shortcut_->setText(QTStr("Voltz.Mouse.Shortcut").arg(keys.isEmpty() ? QTStr("Voltz.Mouse.NoShortcut") : keys));
	loading_ = false;
}

void MouseSettingsSection::reloadSaved()
{
	const QString current = saved_->currentText();
	const QSignalBlocker blocker(saved_);
	saved_->clear();
	const std::vector<NamedMouseStyle> styles = store_->savedStyles();
	for (const NamedMouseStyle &item : styles) {
		saved_->addItem(QString::fromStdString(item.name));
	}
	const bool any = !styles.empty();
	if (!any) {
		saved_->addItem(QTStr("Voltz.Mouse.NoStyles"));
	}
	saved_->setEnabled(any);
	applySaved_->setEnabled(any);
	removeSaved_->setEnabled(any);
	if (any) {
		const int index = saved_->findText(current);
		saved_->setCurrentIndex(index >= 0 ? index : 0);
	}
}

} // namespace voltz
