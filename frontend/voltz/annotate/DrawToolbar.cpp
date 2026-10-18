#include "DrawToolbar.hpp"
#include "ColorPicker.hpp"

#include <voltz/project/MonitorLocator.hpp>
#include <voltz/shell/Icons.hpp>

#include <OBSApp.hpp> // QTStr

#include <util/base.h>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMoveEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QResizeEvent>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <iterator>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "moc_DrawToolbar.cpp"

namespace voltz {

namespace {

constexpr int kTopMargin = 12;
constexpr int kFadeSteps[] = {0, 3, 5, 10};
constexpr int kWidths[] = {2, 4, 8, 16};
constexpr int kTextSizes[] = {24, 36, 48, 72};
const RgbColor kQuickColors[] = {{0xD4, 0xF5, 0x3C}, {0xFF, 0x4D, 0x4D}, {0x4D, 0xA3, 0xFF}, {0xFF, 0xE1, 0x4D}};
const RgbColor kMoreColors[] = {{0xFF, 0xFF, 0xFF}, {0x09, 0x09, 0x0B}, {0xFF, 0x9F, 0x43}, {0x2E, 0xD5, 0x73},
				{0xA5, 0x5E, 0xEA}, {0xFF, 0x6B, 0x9D}, {0x22, 0xD3, 0xEE}, {0x9C, 0xA3, 0xAF}};

struct ToolEntry {
	AnnotationTool tool;
	const char *icon;
	const char *text;
};

const ToolEntry kTools[] = {
	{AnnotationTool::Pen, "pen", "Voltz.Draw.Tool.Pen"},
	{AnnotationTool::Highlighter, "highlighter-circle", "Voltz.Draw.Tool.Highlighter"},
	{AnnotationTool::Line, "line-segment", "Voltz.Draw.Tool.Line"},
	{AnnotationTool::Arrow, "arrow-up-right", "Voltz.Draw.Tool.Arrow"},
	{AnnotationTool::Rectangle, "rectangle", "Voltz.Draw.Tool.Rectangle"},
	{AnnotationTool::Ellipse, "circle", "Voltz.Draw.Tool.Ellipse"},
	{AnnotationTool::Text, "text-t", "Voltz.Draw.Tool.Text"},
};

// Cores da barra que não estão na paleta da casca.
const QColor kPanel(0x11, 0x11, 0x14);
const QColor kBorder(0x2A, 0x2A, 0x30);
const QColor kHover(0x1F, 0x1F, 0x24);

// QSS próprio: a barra é uma janela fora do #voltzShell e o tema global do OBS tem regras de QToolButton (altura máxima,
// cores) — tudo leva o prefixo #voltzDrawToolbar. %1 painel · %2 borda/separador/apertado · %3 hover · %4 texto ·
// %5 lima translúcido (marcado) · %6 lima mais forte (marcado e apertado) · %7 aviso · %8 texto secundário.
const char *kStyleSheet = R"(
#voltzDrawToolbar QFrame#drawPanel {
	background: %1;
	border: 1px solid %2;
	border-radius: 12px;
}
#voltzDrawToolbar QToolButton {
	background: transparent;
	color: %4;
	border: none;
	border-radius: 8px;
	padding: 0;
	margin: 0;
	min-width: 32px;
	min-height: 32px;
	max-height: 32px;
	font-family: "DM Sans", "Segoe UI";
	font-size: 12px;
}
#voltzDrawToolbar QToolButton:hover {
	background: %3;
}
#voltzDrawToolbar QToolButton:pressed {
	background: %2;
}
#voltzDrawToolbar QToolButton:checked {
	background: %5;
}
#voltzDrawToolbar QToolButton:checked:pressed {
	background: %6;
}
#voltzDrawToolbar QToolButton#fadeButton {
	padding: 0 8px;
}
#voltzDrawToolbar QFrame#drawSeparator {
	background: %2;
	border: none;
	margin: 6px 4px;
}
#voltzDrawToolbar QLabel#drawBurned {
	background: transparent;
	color: %7;
	font-family: "DM Sans", "Segoe UI";
	font-size: 12px;
}
#voltzDrawToolbar QLabel#drawHint {
	background: transparent;
	color: %8;
	font-family: "DM Sans", "Segoe UI";
	font-size: 11px;
	padding: 0 4px;
}
)";

QString rgba(const QColor &color, int alpha)
{
	return QStringLiteral("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
}

QString toolbarStyleSheet()
{
	return QString::fromUtf8(kStyleSheet)
		.arg(kPanel.name(), kBorder.name(), kHover.name(), palette::kText.name(), rgba(palette::kAccent, 40),
		     rgba(palette::kAccent, 80), palette::kWarn.name(), palette::kMuted.name());
}

QColor toQColor(const RgbColor &color)
{
	return QColor(color.r, color.g, color.b);
}

bool inPalette(const RgbColor &color)
{
	return std::find(std::begin(kQuickColors), std::end(kQuickColors), color) != std::end(kQuickColors) ||
	       std::find(std::begin(kMoreColors), std::end(kMoreColors), color) != std::end(kMoreColors);
}

// Bolinha da cor (20 px lógicos desenhados a 2×); a escolhida ganha um anel lima por fora.
QIcon swatchIcon(const RgbColor &color, bool selected)
{
	QPixmap pixmap(40, 40);
	pixmap.setDevicePixelRatio(2.0);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	if (selected) {
		painter.setPen(QPen(palette::kAccent, 2.0));
		painter.setBrush(Qt::NoBrush);
		painter.drawEllipse(QRectF(1.0, 1.0, 18.0, 18.0));
	}
	painter.setPen(QPen(kBorder.lighter(140), 1.0));
	painter.setBrush(toQColor(color));
	painter.drawEllipse(QRectF(4.5, 4.5, 11.0, 11.0));
	return QIcon(pixmap);
}

// Traço horizontal da espessura (2 → 1,5 · 4 → 3 · 8 → 4,5 · 16 → 6 px lógicos): não se confunde com as bolinhas de cor.
QIcon widthIcon(int width)
{
	QPixmap pixmap(40, 40);
	pixmap.setDevicePixelRatio(2.0);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	const double thickness = std::clamp(1.5 * std::log2(static_cast<double>(std::max(width, 1))), 1.5, 6.5);
	painter.setPen(QPen(palette::kText, thickness, Qt::SolidLine, Qt::RoundCap));
	painter.drawLine(QPointF(4.0, 10.0), QPointF(16.0, 10.0));
	return QIcon(pixmap);
}

// "A" do tamanho do texto: 8, 11, 14 e 17 px lógicos (posição na lista, não o tamanho real).
QIcon textSizeIcon(int index)
{
	QPixmap pixmap(40, 40);
	pixmap.setDevicePixelRatio(2.0);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	QFont font(QStringLiteral("DM Sans"));
	font.setPixelSize(8 + 3 * index);
	font.setBold(true);
	painter.setFont(font);
	painter.setPen(palette::kText);
	painter.drawText(QRectF(0.0, 0.0, 20.0, 20.0), Qt::AlignCenter, QStringLiteral("A"));
	return QIcon(pixmap);
}

} // namespace

DrawToolbar::DrawToolbar(QWidget *parent)
	: QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus |
				  Qt::NoDropShadowWindowHint)
{
	setObjectName("voltzDrawToolbar");
	/* os testes procuram este título: a barra nunca pode estar visível num teste automático */
	setWindowTitle(QStringLiteral("VoltzDrawToolbar"));
	setAccessibleName(QTStr("Voltz.Draw.Toolbar"));
	setAttribute(Qt::WA_ShowWithoutActivating);
	setAttribute(Qt::WA_TranslucentBackground);
	setFocusPolicy(Qt::NoFocus);
	setStyleSheet(toolbarStyleSheet());

	panel_ = new QFrame(this);
	panel_->setObjectName("drawPanel");
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->addWidget(panel_);
	auto *column = new QVBoxLayout(panel_);
	column->setContentsMargins(8, 6, 8, 6);
	column->setSpacing(4);
	auto *row = new QHBoxLayout();
	row->setSpacing(2);
	column->addLayout(row);

	tools_ = new QButtonGroup(this);
	tools_->setExclusive(true);
	for (const ToolEntry &entry : kTools) {
		QToolButton *button = makeButton(panel_, QTStr(entry.text));
		button->setCheckable(true);
		button->setIcon(voltz::phosphorToggleIcon(QString::fromLatin1(entry.icon), voltz::palette::kText,
							  voltz::palette::kAccent));
		tools_->addButton(button, static_cast<int>(entry.tool));
		row->addWidget(button);
	}
	connect(tools_, &QButtonGroup::idClicked, this,
		[this](int id) { emit toolChosen(static_cast<AnnotationTool>(id)); });
	row->addWidget(separator(panel_));

	for (const RgbColor &color : kQuickColors) {
		row->addWidget(addColor(panel_, color));
	}
	moreColors_ = makeButton(panel_, QTStr("Voltz.Draw.MoreColors"));
	moreColors_->setCheckable(true);
	moreColors_->setIcon(voltz::phosphorIcon(QStringLiteral("palette"), voltz::palette::kText));
	row->addWidget(moreColors_);
	row->addWidget(separator(panel_));

	/* espessuras (traços) ou tamanhos + negrito (texto): um grupo de cada vez, conforme a ferramenta */
	widthGroup_ = new QWidget(panel_);
	auto *widthRow = new QHBoxLayout(widthGroup_);
	widthRow->setContentsMargins(0, 0, 0, 0);
	widthRow->setSpacing(2);
	for (int width : kWidths) {
		QToolButton *button = makeButton(widthGroup_, QTStr("Voltz.Draw.Width").arg(width));
		button->setCheckable(true);
		button->setIcon(widthIcon(width));
		connect(button, &QToolButton::clicked, this, [this, width]() { emit widthChosen(width); });
		widths_.append(qMakePair(button, width));
		widthRow->addWidget(button);
	}
	row->addWidget(widthGroup_);
	textGroup_ = new QWidget(panel_);
	auto *textRow = new QHBoxLayout(textGroup_);
	textRow->setContentsMargins(0, 0, 0, 0);
	textRow->setSpacing(2);
	for (int i = 0; i < static_cast<int>(std::size(kTextSizes)); i++) {
		const int size = kTextSizes[i];
		QToolButton *button = makeButton(textGroup_, QTStr("Voltz.Draw.TextSize").arg(size));
		button->setCheckable(true);
		button->setIcon(textSizeIcon(i));
		connect(button, &QToolButton::clicked, this, [this, size]() { emit textSizeChosen(size); });
		textSizes_.append(qMakePair(button, size));
		textRow->addWidget(button);
	}
	bold_ = makeButton(textGroup_, QTStr("Voltz.Draw.Bold"));
	bold_->setCheckable(true);
	bold_->setIcon(
		voltz::phosphorToggleIcon(QStringLiteral("text-b"), voltz::palette::kText, voltz::palette::kAccent));
	connect(bold_, &QToolButton::clicked, this,
		[this](bool on) { emit textFontChosen(on ? TextFont::Bold : TextFont::Regular); });
	textRow->addWidget(bold_);
	textGroup_->hide();
	row->addWidget(textGroup_);
	row->addWidget(separator(panel_));

	QToolButton *undo = makeButton(panel_, QTStr("Voltz.Draw.Undo"));
	undo->setIcon(voltz::phosphorIcon(QStringLiteral("arrow-counter-clockwise"), voltz::palette::kText));
	connect(undo, &QToolButton::clicked, this, [this]() { emit undoClicked(); });
	row->addWidget(undo);
	QToolButton *clear = makeButton(panel_, QTStr("Voltz.Draw.Clear"));
	clear->setIcon(voltz::phosphorIcon(QStringLiteral("trash"), voltz::palette::kText));
	connect(clear, &QToolButton::clicked, this, [this]() { emit clearClicked(); });
	row->addWidget(clear);
	row->addWidget(separator(panel_));

	fade_ = makeButton(panel_, QString());
	fade_->setObjectName("fadeButton");
	fade_->setIcon(voltz::phosphorIcon(QStringLiteral("timer"), voltz::palette::kText));
	fade_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	connect(fade_, &QToolButton::clicked, this, [this]() {
		/* nunca → 3 s → 5 s → 10 s → nunca (valor fora da lista volta ao começo) */
		const int *current = std::find(std::begin(kFadeSteps), std::end(kFadeSteps), style_.fadeSeconds);
		const int *next = (current == std::end(kFadeSteps) || current + 1 == std::end(kFadeSteps))
					  ? std::begin(kFadeSteps)
					  : current + 1;
		emit fadeChosen(*next);
	});
	row->addWidget(fade_);
	row->addWidget(separator(panel_));

	exit_ = makeButton(panel_, QString());
	exit_->setIcon(voltz::phosphorIcon(QStringLiteral("x"), voltz::palette::kText));
	connect(exit_, &QToolButton::clicked, this, [this]() { emit exitClicked(); });
	row->addWidget(exit_);

	/* "mais cores": as 8 cores, a cor personalizada e, aberto por ela, o seletor completo */
	extraColors_ = new QWidget(panel_);
	auto *extraColumn = new QVBoxLayout(extraColors_);
	extraColumn->setContentsMargins(0, 0, 0, 0);
	extraColumn->setSpacing(6);
	auto *extraRow = new QHBoxLayout();
	extraRow->setSpacing(2);
	for (const RgbColor &color : kMoreColors) {
		extraRow->addWidget(addColor(extraColors_, color));
	}
	customColor_ = makeButton(extraColors_, QTStr("Voltz.Draw.CustomColor"));
	customColor_->setCheckable(true);
	extraRow->addWidget(customColor_);
	extraRow->addStretch(1);
	extraColumn->addLayout(extraRow);
	picker_ = new ColorPicker(extraColors_);
	picker_->hide();
	extraColumn->addWidget(picker_, 0, Qt::AlignLeft);
	extraColors_->hide();
	column->addWidget(extraColors_);
	connect(moreColors_, &QToolButton::toggled, this, [this](bool on) {
		extraColors_->setVisible(on);
		resizeAndRecenter();
	});
	connect(customColor_, &QToolButton::toggled, this, [this](bool on) {
		picker_->setVisible(on);
		resizeAndRecenter();
	});
	connect(picker_, &ColorPicker::colorPicked, this, [this](RgbColor color) { emit colorChosen(color); });

	burned_ = new QLabel(QTStr("Voltz.Draw.Burned"), panel_);
	burned_->setObjectName("drawBurned");
	burned_->setWordWrap(true);
	burned_->hide();
	column->addWidget(burned_);

	/* linha de dica: o que o botão sob o mouse faz ou, sem botão, como sair (ou digitar) */
	hint_ = new QLabel(panel_);
	hint_->setObjectName("drawHint");
	hint_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	column->addWidget(hint_);

	setExitHint(QStringLiteral("Ctrl + Shift + D"));
	setAnnotationStyle(AnnotationStyle{});
}

QToolButton *DrawToolbar::makeButton(QWidget *parent, const QString &hint)
{
	auto *button = new QToolButton(parent);
	button->setFocusPolicy(Qt::NoFocus);
	button->setIconSize(QSize(20, 20));
	button->setAutoRaise(true);
	button->installEventFilter(this); /* a dica vai para a linha de dica */
	if (!hint.isEmpty()) {
		voltz::setButtonHint(button, hint);
	}
	return button;
}

QToolButton *DrawToolbar::addColor(QWidget *parent, const RgbColor &color)
{
	QToolButton *button =
		makeButton(parent, QTStr("Voltz.Draw.Color").arg(QString::fromStdString(voltz::formatHexColor(color))));
	button->setIcon(swatchIcon(color, false));
	connect(button, &QToolButton::clicked, this, [this, color]() { emit colorChosen(color); });
	colors_.append(qMakePair(button, color));
	return button;
}

QFrame *DrawToolbar::separator(QWidget *parent)
{
	auto *line = new QFrame(parent);
	line->setObjectName("drawSeparator");
	line->setFixedWidth(1);
	return line;
}

bool DrawToolbar::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
		auto *button = qobject_cast<QToolButton *>(watched);
		hovered_ = (event->type() == QEvent::Enter && button) ? button->toolTip() : QString();
		updateHint();
	}
	return QWidget::eventFilter(watched, event);
}

void DrawToolbar::setAnnotationStyle(const AnnotationStyle &style)
{
	style_ = style;
	if (QAbstractButton *button = tools_->button(static_cast<int>(style.tool))) {
		button->setChecked(true);
	}
	markColor(style.color);
	picker_->setColor(style.color);
	for (const auto &entry : widths_) {
		entry.first->setChecked(entry.second == style.width);
	}
	for (const auto &entry : textSizes_) {
		entry.first->setChecked(entry.second == style.textSize);
	}
	bold_->setChecked(style.textFont == TextFont::Bold);
	const bool text = style.tool == AnnotationTool::Text;
	if (textGroup_->isHidden() == text) {
		widthGroup_->setVisible(!text);
		textGroup_->setVisible(text);
		/* o botão sob o mouse pode ter sumido com o grupo (sem Leave): a dica dele não fica (updateHint logo abaixo) */
		hovered_.clear();
		resizeAndRecenter();
	}
	updateFadeText();
	updateHint();
}

void DrawToolbar::markColor(const RgbColor &color)
{
	for (const auto &entry : colors_) {
		entry.first->setIcon(swatchIcon(entry.second, entry.second == color));
	}
	/* cor fora da paleta: o botão da cor personalizada mostra a cor, marcada; senão, o conta-gotas */
	const bool custom = !inPalette(color);
	customColor_->setIcon(custom ? swatchIcon(color, true)
				     : voltz::phosphorIcon(QStringLiteral("eyedropper"), voltz::palette::kText));
	/* a cor marcada é uma das 8 extras (ou a personalizada): abre a linha delas para a marca ficar à vista */
	const bool extra = std::find(std::begin(kMoreColors), std::end(kMoreColors), color) != std::end(kMoreColors);
	if ((extra || custom) && !moreColors_->isChecked()) {
		moreColors_->setChecked(true);
	}
}

void DrawToolbar::updateFadeText()
{
	const QString value = style_.fadeSeconds > 0 ? QTStr("Voltz.Draw.Fade.Seconds").arg(style_.fadeSeconds)
						     : QTStr("Voltz.Draw.Fade.Never");
	fade_->setText(value);
	voltz::setButtonHint(fade_, QTStr("Voltz.Draw.Fade").arg(value));
}

void DrawToolbar::updateHint()
{
	if (!hovered_.isEmpty()) {
		hint_->setText(hovered_);
	} else if (style_.tool == AnnotationTool::Text) {
		hint_->setText(QTStr("Voltz.Draw.Hint.Text"));
	} else {
		hint_->setText(QTStr("Voltz.Draw.Hint.Exit").arg(shortcut_));
	}
}

void DrawToolbar::showColorPicker(bool show)
{
	moreColors_->setChecked(show || moreColors_->isChecked());
	customColor_->setChecked(show);
}

void DrawToolbar::setBurned(bool burned)
{
	burned_->setVisible(burned);
	resizeAndRecenter();
}

void DrawToolbar::resizeAndRecenter()
{
	adjustSize();
	if (isVisible()) {
		placeOn(windowHandle() ? windowHandle()->screen() : screen());
	}
}

void DrawToolbar::placeOn(QScreen *target)
{
	if (target) {
		const QRect area = target->geometry();
		move(area.x() + (area.width() - width()) / 2, area.y() + kTopMargin);
	}
}

void DrawToolbar::setExitHint(const QString &shortcut)
{
	shortcut_ = shortcut;
	voltz::setButtonHint(exit_, QTStr("Voltz.Draw.Exit").arg(shortcut));
	updateHint();
}

void DrawToolbar::showOnScreen(const QString &deviceName, bool excludeFromCapture)
{
	/* pelo HMONITOR (screenForMonitor): no Qt 6 o QScreen::name() é o nome amigável, não o "\\.\DISPLAYn" */
	QScreen *target = voltz::screenForMonitor(deviceName.toStdString());
	if (!target) {
		blog(LOG_WARNING,
		     "[voltz] barra do modo desenho: monitor '%s' nao encontrado entre as telas do Qt; vai para a "
		     "tela principal",
		     deviceName.toUtf8().constData());
		target = QGuiApplication::primaryScreen();
	}
	adjustSize();
	if (!testAttribute(Qt::WA_WState_Created)) {
		(void)winId(); /* cria a janela nativa agora: a propriedade abaixo tem de existir antes do primeiro show */
	}
	/* o OBSApp::notify não reaplica nela o "esconder da captura" do usuário (WDA_NONE com a opção desligada) */
	windowHandle()->setProperty("isOBSProjectorWindow", true);
#ifdef _WIN32
	/* sem ativar e fora da captura ANTES do primeiro show: nenhum quadro ativo nem capturado */
	const auto applyNativeStyle = [this]() {
		HWND hwnd = reinterpret_cast<HWND>(winId());
		const LONG_PTR wanted = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
		const LONG_PTR current = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
		if ((current & wanted) != wanted) {
			SetWindowLongPtrW(hwnd, GWL_EXSTYLE, current | wanted);
		}
	};
	applyNativeStyle();
	if (!SetWindowDisplayAffinity(reinterpret_cast<HWND>(winId()),
				      excludeFromCapture ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE)) {
		blog(LOG_WARNING, "[voltz] barra do modo desenho: SetWindowDisplayAffinity falhou (%lu)",
		     GetLastError());
	}
#else
	(void)excludeFromCapture;
#endif
	if (target) {
		windowHandle()->setScreen(target);
		placeOn(target);
	}
	show();
#ifdef _WIN32
	applyNativeStyle(); /* o Qt pode refazer o estilo estendido no show: confere de novo */
#endif
	raiseWithoutActivating();
	emit nativeRectChanged();
}

void DrawToolbar::raiseWithoutActivating()
{
#ifdef _WIN32
	if (testAttribute(Qt::WA_WState_Created) && isVisible()) {
		SetWindowPos(reinterpret_cast<HWND>(winId()), HWND_TOPMOST, 0, 0, 0, 0,
			     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}
#endif
}

MonitorRect DrawToolbar::nativeRect() const
{
	MonitorRect rect;
#ifdef _WIN32
	if (!testAttribute(Qt::WA_WState_Created) || !isVisible()) {
		return rect;
	}
	RECT window;
	if (GetWindowRect(reinterpret_cast<HWND>(winId()), &window)) {
		rect = MonitorRect{static_cast<int>(window.left), static_cast<int>(window.top),
				   static_cast<int>(window.right - window.left),
				   static_cast<int>(window.bottom - window.top)};
	}
#endif
	return rect;
}

void DrawToolbar::moveEvent(QMoveEvent *event)
{
	QWidget::moveEvent(event);
	if (isVisible()) {
		emit nativeRectChanged();
	}
}

void DrawToolbar::paintEvent(QPaintEvent *event)
{
	/* alfa 1/255 em toda a janela (invisível): os cantos arredondados do painel não deixam o clique atravessar a barra
	 * (e o buraco da janela de entrada) até o app de baixo, o que tiraria o Voltz do modo desenho */
	QPainter painter(this);
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.fillRect(rect(), QColor(0, 0, 0, 1));
	painter.end();
	QWidget::paintEvent(event);
}

void DrawToolbar::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if (isVisible()) {
		emit nativeRectChanged();
	}
}

} // namespace voltz
