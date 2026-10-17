#include "DrawToolbar.hpp"

#include <voltz/project/MonitorLocator.hpp>
#include <voltz/shell/Icons.hpp>

#include <OBSApp.hpp> // QTStr

#include <util/base.h>

#include <QAbstractButton>
#include <QButtonGroup>
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
};

// Cores da barra que não estão na paleta da casca.
const QColor kPanel(0x11, 0x11, 0x14);
const QColor kBorder(0x2A, 0x2A, 0x30);
const QColor kHover(0x1F, 0x1F, 0x24);

// QSS próprio: a barra é uma janela fora do #voltzShell e o tema global do OBS tem regras de QToolButton (altura máxima,
// cores) — tudo leva o prefixo #voltzDrawToolbar. %1 painel · %2 borda/separador/apertado · %3 hover · %4 texto ·
// %5 lima translúcido (marcado) · %6 lima mais forte (marcado e apertado) · %7 aviso.
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
)";

QString rgba(const QColor &color, int alpha)
{
	return QStringLiteral("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
}

QString toolbarStyleSheet()
{
	return QString::fromUtf8(kStyleSheet)
		.arg(kPanel.name(), kBorder.name(), kHover.name(), palette::kText.name(), rgba(palette::kAccent, 40),
		     rgba(palette::kAccent, 80), palette::kWarn.name());
}

QColor toQColor(const RgbColor &color)
{
	return QColor(color.r, color.g, color.b);
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
	row->addWidget(separator());

	for (const RgbColor &color : kQuickColors) {
		row->addWidget(addColor(color));
	}
	moreColors_ = makeButton(panel_, QTStr("Voltz.Draw.MoreColors"));
	moreColors_->setCheckable(true);
	moreColors_->setIcon(voltz::phosphorIcon(QStringLiteral("palette"), voltz::palette::kText));
	row->addWidget(moreColors_);
	row->addWidget(separator());

	for (int width : kWidths) {
		QToolButton *button = makeButton(panel_, QTStr("Voltz.Draw.Width").arg(width));
		button->setCheckable(true);
		button->setIcon(widthIcon(width));
		connect(button, &QToolButton::clicked, this, [this, width]() { emit widthChosen(width); });
		widths_.append(qMakePair(button, width));
		row->addWidget(button);
	}
	row->addWidget(separator());

	QToolButton *undo = makeButton(panel_, QTStr("Voltz.Draw.Undo"));
	undo->setIcon(voltz::phosphorIcon(QStringLiteral("arrow-counter-clockwise"), voltz::palette::kText));
	connect(undo, &QToolButton::clicked, this, [this]() { emit undoClicked(); });
	row->addWidget(undo);
	QToolButton *clear = makeButton(panel_, QTStr("Voltz.Draw.Clear"));
	clear->setIcon(voltz::phosphorIcon(QStringLiteral("trash"), voltz::palette::kText));
	connect(clear, &QToolButton::clicked, this, [this]() { emit clearClicked(); });
	row->addWidget(clear);
	row->addWidget(separator());

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
	row->addWidget(separator());

	exit_ = makeButton(panel_, QString());
	exit_->setIcon(voltz::phosphorIcon(QStringLiteral("x"), voltz::palette::kText));
	connect(exit_, &QToolButton::clicked, this, [this]() { emit exitClicked(); });
	row->addWidget(exit_);
	setExitHint(QStringLiteral("Ctrl + Shift + D"));

	extraColors_ = new QWidget(panel_);
	auto *extraRow = new QHBoxLayout(extraColors_);
	extraRow->setContentsMargins(0, 0, 0, 0);
	extraRow->setSpacing(2);
	for (const RgbColor &color : kMoreColors) {
		extraRow->addWidget(addColor(color));
	}
	extraRow->addStretch(1);
	extraColors_->hide();
	column->addWidget(extraColors_);
	connect(moreColors_, &QToolButton::toggled, this, [this](bool on) {
		extraColors_->setVisible(on);
		resizeAndRecenter();
	});

	burned_ = new QLabel(QTStr("Voltz.Draw.Burned"), panel_);
	burned_->setObjectName("drawBurned");
	burned_->setWordWrap(true);
	burned_->hide();
	column->addWidget(burned_);

	setAnnotationStyle(AnnotationStyle{});
}

QToolButton *DrawToolbar::makeButton(QWidget *parent, const QString &hint)
{
	auto *button = new QToolButton(parent);
	button->setFocusPolicy(Qt::NoFocus);
	button->setIconSize(QSize(20, 20));
	button->setAutoRaise(true);
	if (!hint.isEmpty()) {
		voltz::setButtonHint(button, hint);
	}
	return button;
}

QToolButton *DrawToolbar::addColor(const RgbColor &color)
{
	QToolButton *button =
		makeButton(panel_, QTStr("Voltz.Draw.Color").arg(QString::fromStdString(voltz::formatHexColor(color))));
	button->setIcon(swatchIcon(color, false));
	connect(button, &QToolButton::clicked, this, [this, color]() { emit colorChosen(color); });
	colors_.append(qMakePair(button, color));
	return button;
}

QFrame *DrawToolbar::separator()
{
	auto *line = new QFrame(panel_);
	line->setObjectName("drawSeparator");
	line->setFixedWidth(1);
	return line;
}

void DrawToolbar::setAnnotationStyle(const AnnotationStyle &style)
{
	style_ = style;
	if (QAbstractButton *button = tools_->button(static_cast<int>(style.tool))) {
		button->setChecked(true);
	}
	markColor(style.color);
	for (const auto &entry : widths_) {
		entry.first->setChecked(entry.second == style.width);
	}
	updateFadeText();
}

void DrawToolbar::markColor(const RgbColor &color)
{
	for (const auto &entry : colors_) {
		entry.first->setIcon(swatchIcon(entry.second, entry.second == color));
	}
	/* a cor marcada é uma das 8 extras: abre a linha delas para a marca ficar à vista */
	const bool extra = std::find(std::begin(kMoreColors), std::end(kMoreColors), color) != std::end(kMoreColors);
	if (extra && !moreColors_->isChecked()) {
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
	voltz::setButtonHint(exit_, QTStr("Voltz.Draw.Exit").arg(shortcut));
}

void DrawToolbar::showOnScreen(const QString &deviceName, bool excludeFromCapture)
{
	/* pelo HMONITOR (screenForMonitor): no Qt 6 o QScreen::name() é o nome amigável, não o "\\.\DISPLAYn" */
	QScreen *target = voltz::screenForMonitor(deviceName.toStdString());
	if (!target) {
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
