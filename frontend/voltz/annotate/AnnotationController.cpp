#include "AnnotationController.hpp"
#include "AnnotationTestVendor.hpp"
#include "OverlayWindow.hpp"

#include <voltz/core/annotation-font.hpp>
#include <voltz/core/annotation-text.hpp>
#include <voltz/core/live-protocol.hpp>
#include <voltz/project/MonitorLocator.hpp>
#include <voltz/shell/SharedSources.hpp>

#include <OBSApp.hpp>           // App()
#include <utility/platform.hpp> // SetDisplayAffinitySupported

#include <obs.hpp>
#include <util/config-file.h>
#include <util/platform.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QChar>
#include <QStringList>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

#include "moc_AnnotationController.cpp"

namespace voltz {

namespace {

constexpr int kOverlayPollMs = 1000;
constexpr int64_t kNsPerMs = 1000000;
constexpr int64_t kMaxTimerMs = 3600000;
constexpr int kStyleSaveDelayMs = 400;

int64_t nowNs()
{
	return static_cast<int64_t>(os_gettime_ns());
}

// Procedimento do plugin. Sem o plugin, avisa uma vez: as anotações ficam só na sobreposição.
void callProc(const char *name, calldata_t *params)
{
	if (!proc_handler_call(obs_get_proc_handler(), name, params)) {
		static bool warned = false;
		if (!warned) {
			warned = true;
			blog(LOG_WARNING,
			     "[voltz] plugin voltz-sources ausente: anotacoes sem a fonte Voltz Anotacoes");
		}
	}
}

void sendBegin(const Stroke &stroke)
{
	calldata_t params;
	calldata_init(&params);
	calldata_set_int(&params, "id", stroke.id);
	calldata_set_string(&params, "json", strokeToJsonLine(stroke).c_str());
	callProc(live::kProcAnnotBegin, &params);
	calldata_free(&params);
}

void sendExtend(int id, const std::vector<StrokePoint> &points)
{
	calldata_t params;
	calldata_init(&params);
	calldata_set_int(&params, "id", id);
	calldata_set_string(&params, "points", pointsToJson(points).c_str());
	callProc(live::kProcAnnotExtend, &params);
	calldata_free(&params);
}

void sendRemove(int id)
{
	uint8_t stack[128];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_int(&params, "id", id);
	callProc(live::kProcAnnotRemove, &params);
}

void sendClear()
{
	uint8_t stack[64];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	callProc(live::kProcAnnotClear, &params);
}

void sendBurned(bool burned)
{
	uint8_t stack[128];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_bool(&params, "burned", burned);
	callProc(live::kProcAnnotBurned, &params);
}

bool sameRect(const MonitorRect &a, const MonitorRect &b)
{
	return a.left == b.left && a.top == b.top && a.width == b.width && a.height == b.height;
}

// VOLTZ_TEST_OVERLAY_RECT="x,y,largura,altura" (só com VOLTZ_TEST_LIVE): retângulo da sobreposição no lugar do monitor
// da Tela — os testes o põem fora da área de trabalho.
bool testOverlayRect(MonitorRect &rect)
{
	if (!live::testModeEnabled()) {
		return false;
	}
	const QStringList parts = qEnvironmentVariable(live::kTestOverlayRectEnv).split(QLatin1Char(','));
	if (parts.size() != 4) {
		return false;
	}
	int values[4] = {0, 0, 0, 0};
	for (int i = 0; i < 4; i++) {
		bool ok = false;
		values[i] = parts[i].trimmed().toInt(&ok);
		if (!ok) {
			return false;
		}
	}
	if (values[2] <= 0 || values[3] <= 0) {
		return false;
	}
	rect = MonitorRect{values[0], values[1], values[2], values[3]};
	return true;
}

// Tela compartilhada com tamanho de captura válido (nula sem Tela ou com tamanho 0).
OBSSource sizedScreen(int &captureWidth, int &captureHeight)
{
	OBSSource screen = findSharedSource(SharedSource::Screen);
	if (!screen) {
		return OBSSource();
	}
	captureWidth = static_cast<int>(obs_source_get_width(screen));
	captureHeight = static_cast<int>(obs_source_get_height(screen));
	if (captureWidth <= 0 || captureHeight <= 0) {
		return OBSSource();
	}
	return screen;
}

// Monitor de uma captura de monitor; found = false para outro tipo de fonte ou monitor que não existe mais.
MonitorDescription capturedMonitor(obs_source_t *screen)
{
	if (strcmp(obs_source_get_unversioned_id(screen), "monitor_capture") != 0) {
		return MonitorDescription{};
	}
	OBSDataAutoRelease settings = obs_source_get_settings(screen);
	return findMonitor(obs_data_get_string(settings, "monitor_id"));
}

} // namespace

AnnotationController::AnnotationController(QObject *parent) : QObject(parent)
{
	testMode_ = live::testModeEnabled();
	const char *json = config_get_string(App()->GetUserConfig(), "Voltz", "AnnotationStyle");
	if (json && *json && !annotationStyleFromJson(json, style_)) {
		blog(LOG_WARNING, "[voltz] AnnotationStyle invalido no user.ini; usando o padrao");
	}
	if (style_.fadeSeconds > 0) {
		blog(LOG_INFO, "[voltz] anotacoes: somem apos %d s", style_.fadeSeconds);
	}
	if (!embeddedFontAvailable(TextFont::Regular) || !embeddedFontAvailable(TextFont::Bold)) {
		blog(LOG_WARNING, "[voltz] anotacoes: a fonte do texto nao abriu (o texto nao aparece)");
	}
	const bool forced = testMode_ && qEnvironmentVariableIsSet(live::kTestBurnedEnv);
	/* Antes do Windows 10 2004 o WDA_EXCLUDEFROMCAPTURE vira caixa preta: nem tenta (spec §6.3). */
	burned_ = forced || !SetDisplayAffinitySupported();
	if (burned_) {
		blog(LOG_WARNING, "[voltz] anotacoes: queimadas (%s)",
		     forced ? "teste" : "Windows sem WDA_EXCLUDEFROMCAPTURE");
	}
	sendBurned(burned_);
	expiry_.setSingleShot(true);
	connect(&expiry_, &QTimer::timeout, this, [this]() { expire(); });
	overlayPoll_.setInterval(kOverlayPollMs);
	connect(&overlayPoll_, &QTimer::timeout, this, [this]() { refreshOverlay(false); });
	saveTimer_.setSingleShot(true);
	saveTimer_.setInterval(kStyleSaveDelayMs);
	connect(&saveTimer_, &QTimer::timeout, this, [this]() { saveStyle(); });
	if (testMode_) {
		AnnotationTestVendor::start(this);
	}
	publishTestState();
}

AnnotationController::~AnnotationController()
{
	shutdown();
}

void AnnotationController::shutdown()
{
	if (shutdown_) {
		return;
	}
	shutdown_ = true;
	if (testMode_) {
		AnnotationTestVendor::stop();
	}
	if (saveTimer_.isActive()) {
		saveStyle();
	}
	expiry_.stop();
	overlayPoll_.stop();
	if (overlay_) {
		overlay_->destroy();
		overlay_.reset();
	}
	blog(LOG_INFO, "[voltz] anotacoes: desligadas (sobreposicao destruida)");
}

int AnnotationController::beginStroke(AnnotationTool tool, const RgbColor &color, int width, const StrokePoint &first)
{
	if (shutdown_) {
		return 0;
	}
	if (store_.openId() != 0) {
		endStroke();
	}
	if (tool == AnnotationTool::Text) {
		return beginText(first); /* texto pelo pedido de teste InjectStroke: começa vazio */
	}
	const int id = store_.begin(tool, color, width, first, nowNs());
	if (id == 0) {
		return 0;
	}
	const Stroke &stroke = store_.visible().back().stroke;
	sendBegin(stroke);
	std::optional<PixelRect> dirty;
	double scaleX = 0.0;
	double scaleY = 0.0;
	if (overlayScale(scaleX, scaleY)) {
		dirty = strokeBounds(stroke, scaleX, scaleY);
	}
	refreshOverlay(true, dirty);
	return id;
}

void AnnotationController::extendStroke(const std::vector<StrokePoint> &points)
{
	if (shutdown_ || points.empty() || store_.openId() == 0 || typingText()) {
		return; /* o texto só tem a âncora */
	}
	const StrokeTail tail = strokeTail(store_.visible().back().stroke);
	if (!store_.extend(points)) {
		return;
	}
	sendExtend(store_.openId(), points);
	std::optional<PixelRect> dirty;
	double scaleX = 0.0;
	double scaleY = 0.0;
	if (overlayScale(scaleX, scaleY)) {
		dirty = appendDirtyRect(tail, store_.visible().back().stroke, scaleX, scaleY);
	}
	refreshOverlay(true, dirty);
}

void AnnotationController::endStroke()
{
	if (shutdown_) {
		return;
	}
	if (typingText()) {
		commitText();
		return;
	}
	const LiveStroke *ended = store_.end(nowNs());
	if (!ended) {
		return;
	}
	const LiveStroke copy = *ended;
	emit strokeEnded(copy);
	scheduleExpiry();
	publishTestState();
}

void AnnotationController::undo()
{
	if (shutdown_) {
		return;
	}
	if (typingText()) {
		cancelText();
		return;
	}
	removed(store_.undo(nowNs()), false);
}

void AnnotationController::clear()
{
	if (shutdown_) {
		return;
	}
	const Stroke caretBefore = caret_;
	typing_ = false;
	caret_ = Stroke{};
	removed(store_.clear(nowNs()), true, &caretBefore);
}

int AnnotationController::beginText(const StrokePoint &anchor)
{
	if (shutdown_) {
		return 0;
	}
	if (store_.openId() != 0) {
		endStroke();
	}
	const int id = store_.beginText(style_.color, style_.textSize, style_.textFont, anchor, nowNs());
	if (id == 0) {
		return 0;
	}
	const Stroke &stroke = store_.visible().back().stroke;
	/* vazio: nada vai ao plugin ainda (textChanged manda quando houver algo a mostrar) */
	typing_ = true;
	caret_ = textCaretStroke(stroke);
	refreshOverlay(true, overlayBounds({&caret_}));
	blog(LOG_INFO, "[voltz] anotacoes: texto comecado (tam %d, fonte %s)", stroke.textSize,
	     textFontToString(stroke.textFont));
	return id;
}

bool AnnotationController::typingText() const
{
	return typing_ && store_.openId() != 0 && store_.visible().back().stroke.tool == AnnotationTool::Text;
}

void AnnotationController::textChanged(const Stroke &before, const Stroke &caretBefore)
{
	const Stroke &after = store_.visible().back().stroke;
	if (textIsBlank(after.text)) {
		sendRemove(after.id); /* em branco (apagou tudo): nada no programa; id desconhecido no plugin = nada */
	} else {
		sendBegin(after); /* o plugin troca o traço de mesmo id inteiro */
	}
	caret_ = textCaretStroke(after);
	refreshOverlay(true, overlayBounds({&before, &after, &caretBefore, &caret_}));
}

void AnnotationController::updateText(const std::string &text)
{
	if (shutdown_ || !typingText()) {
		return;
	}
	const Stroke before = store_.visible().back().stroke;
	const Stroke caretBefore = caret_;
	if (!store_.setOpenText(text) || store_.visible().back().stroke.text == before.text) {
		return;
	}
	textChanged(before, caretBefore);
}

void AnnotationController::commitText()
{
	if (shutdown_ || !typingText()) {
		return;
	}
	if (textIsBlank(store_.visible().back().stroke.text)) {
		cancelText(); /* vazio ou só espaços e quebras: nada a confirmar */
		return;
	}
	const Stroke caretBefore = caret_;
	typing_ = false;
	caret_ = Stroke{};
	const LiveStroke *ended = store_.end(nowNs());
	if (!ended) {
		return;
	}
	const LiveStroke copy = *ended;
	emit strokeEnded(copy);
	scheduleExpiry();
	refreshOverlay(true, overlayBounds({&caretBefore}));
	blog(LOG_INFO, "[voltz] anotacoes: texto confirmado (%zu caractere(s))", textCharCount(copy.stroke.text));
}

void AnnotationController::cancelText()
{
	if (shutdown_ || !typingText()) {
		return;
	}
	const Stroke open = store_.visible().back().stroke;
	const Stroke caretBefore = caret_;
	typing_ = false;
	caret_ = Stroke{};
	if (textIsBlank(open.text)) {
		/* nunca apareceu (nem no programa): sai sem registro no diário */
		store_.discardOpen();
		sendRemove(open.id);
		refreshOverlay(true, overlayBounds({&caretBefore}));
	} else {
		removed(store_.undo(nowNs()), false, &caretBefore);
	}
	blog(LOG_INFO, "[voltz] anotacoes: texto cancelado");
}

void AnnotationController::removed(const std::vector<RemovedStroke> &strokes, bool all, const Stroke *extra)
{
	if (strokes.empty()) {
		if (extra && !extra->points.empty()) {
			refreshOverlay(true, overlayBounds({extra}));
		}
		return;
	}
	if (all) {
		sendClear();
	} else {
		for (const RemovedStroke &stroke : strokes) {
			sendRemove(stroke.live.stroke.id);
		}
	}
	std::optional<PixelRect> dirty;
	double scaleX = 0.0;
	double scaleY = 0.0;
	if (overlayScale(scaleX, scaleY)) {
		PixelRect area;
		for (const RemovedStroke &stroke : strokes) {
			area = uniteRects(area, strokeBounds(stroke.live.stroke, scaleX, scaleY));
		}
		if (extra) {
			area = uniteRects(area, strokeBounds(*extra, scaleX, scaleY));
		}
		dirty = area;
	}
	emit strokesRemoved(strokes);
	scheduleExpiry();
	refreshOverlay(true, dirty);
	blog(LOG_INFO, "[voltz] anotacoes: %d traco(s) removido(s), %d na tela", static_cast<int>(strokes.size()),
	     static_cast<int>(store_.visible().size()));
}

int64_t AnnotationController::lifetimeNs() const
{
	return static_cast<int64_t>(style_.fadeSeconds) * 1000 * kNsPerMs;
}

void AnnotationController::scheduleExpiry()
{
	const int64_t next = store_.nextExpiryNs(lifetimeNs());
	if (next < 0 || shutdown_) {
		expiry_.stop();
		return;
	}
	const int64_t waitMs = std::clamp<int64_t>((next - nowNs()) / kNsPerMs + 1, 0, kMaxTimerMs);
	expiry_.start(static_cast<int>(waitMs));
}

void AnnotationController::expire()
{
	if (shutdown_) {
		return;
	}
	removed(store_.expire(nowNs(), lifetimeNs()), false);
	scheduleExpiry();
}

bool AnnotationController::overlayTarget(MonitorRect &rect, int &captureWidth, int &captureHeight) const
{
	OBSSource screen = sizedScreen(captureWidth, captureHeight);
	if (!screen) {
		return false;
	}
	if (testOverlayRect(rect)) {
		return true;
	}
	const MonitorDescription monitor = capturedMonitor(screen);
	if (!monitor.found) {
		return false;
	}
	rect = monitor.rect;
	return true;
}

void AnnotationController::refreshOverlay(bool strokesChanged, const std::optional<PixelRect> &dirty)
{
	if (shutdown_) {
		return;
	}
	MonitorRect rect;
	int captureWidth = 0;
	int captureHeight = 0;
	const bool hasTarget = overlayTarget(rect, captureWidth, captureHeight);
	const int state = hasTarget ? 1 : 0;
	if (state != overlayState_) {
		overlayState_ = state;
		if (hasTarget) {
			blog(LOG_INFO, "[voltz] anotacoes: sobreposicao em (%d,%d %dx%d) para a captura %dx%d",
			     rect.left, rect.top, rect.width, rect.height, captureWidth, captureHeight);
		} else {
			blog(LOG_INFO, "[voltz] anotacoes: sem captura de monitor; sem sobreposicao");
		}
	}
	const std::vector<const Stroke *> strokes = overlayStrokes();
	if (!hasTarget) {
		if (overlay_) {
			overlay_->render({}, 0, 0);
		}
		/* Sem alvo agora (Tela trocada, captura reiniciando com tamanho 0): esquece o alvo, para redesenhar quando ele
		 * voltar, e continua conferindo enquanto houver traço — senão a sobreposição ficaria escondida até o próximo
		 * traço. */
		overlayRect_ = MonitorRect{};
		captureWidth_ = 0;
		captureHeight_ = 0;
		if (strokes.empty()) {
			overlayPoll_.stop();
		} else if (!overlayPoll_.isActive()) {
			overlayPoll_.start();
		}
		publishTestState();
		return;
	}
	const bool moved = !sameRect(rect, overlayRect_) || captureWidth != captureWidth_ ||
			   captureHeight != captureHeight_;
	if (!strokesChanged && !moved) {
		return;
	}
	overlayRect_ = rect;
	captureWidth_ = captureWidth;
	captureHeight_ = captureHeight;
	if (!overlay_ && strokes.empty()) {
		overlayPoll_.stop();
		publishTestState();
		return;
	}
	bool created = false;
	if (!overlay_) {
		auto overlay = std::make_unique<OverlayWindow>();
		if (!overlay->create(rect)) {
			blog(LOG_WARNING, "[voltz] anotacoes: a sobreposicao nao foi criada");
			publishTestState();
			return;
		}
		overlay_ = std::move(overlay);
		created = true;
		if (!burned_ && !overlay_->excludeFromCapture()) {
			markBurned("afinidade recusada");
		}
		blog(LOG_INFO, "[voltz] anotacoes: sobreposicao criada (%s)",
		     burned_ ? "visivel na captura" : "fora da captura");
	} else {
		overlay_->setRect(rect);
	}
	/* só a região que mudou, se o alvo é o mesmo; senão (criada, movida, tamanho novo) a imagem inteira */
	const bool partial = !moved && !created && dirty.has_value();
	overlay_->render(strokes, captureWidth, captureHeight, partial ? &*dirty : nullptr);
	if (strokes.empty()) {
		overlayPoll_.stop();
	} else if (!overlayPoll_.isActive()) {
		overlayPoll_.start();
	}
	publishTestState();
}

bool AnnotationController::overlayScale(double &scaleX, double &scaleY) const
{
	if (!overlay_ || overlayRect_.width <= 0 || overlayRect_.height <= 0 || captureWidth_ <= 0 ||
	    captureHeight_ <= 0) {
		return false;
	}
	scaleX = static_cast<double>(overlayRect_.width) / static_cast<double>(captureWidth_);
	scaleY = static_cast<double>(overlayRect_.height) / static_cast<double>(captureHeight_);
	return true;
}

std::vector<const Stroke *> AnnotationController::strokePointers() const
{
	std::vector<const Stroke *> out;
	out.reserve(store_.visible().size());
	for (const LiveStroke &live : store_.visible()) {
		out.push_back(&live.stroke);
	}
	return out;
}

std::vector<const Stroke *> AnnotationController::overlayStrokes() const
{
	std::vector<const Stroke *> out = strokePointers();
	if (typingText() && !caret_.points.empty()) {
		out.push_back(&caret_); /* por cima de tudo, só na sobreposição */
	}
	return out;
}

std::optional<PixelRect> AnnotationController::overlayBounds(std::initializer_list<const Stroke *> strokes) const
{
	double scaleX = 0.0;
	double scaleY = 0.0;
	if (!overlayScale(scaleX, scaleY)) {
		return std::nullopt;
	}
	PixelRect area;
	for (const Stroke *stroke : strokes) {
		if (stroke) {
			area = uniteRects(area, strokeBounds(*stroke, scaleX, scaleY));
		}
	}
	return area;
}

void AnnotationController::setStyle(const AnnotationStyle &style)
{
	if (shutdown_) {
		return;
	}
	const int fadeBefore = style_.fadeSeconds;
	style_ = clampAnnotationStyle(style);
	saveTimer_.start();
	if (style_.fadeSeconds != fadeBefore) {
		blog(LOG_INFO, "[voltz] anotacoes: somem apos %d s", style_.fadeSeconds);
		scheduleExpiry();
	}
	if (typingText()) {
		/* cor, tamanho e fonte escolhidos na barra durante a digitação valem para o texto em andamento */
		const Stroke before = store_.visible().back().stroke;
		const Stroke caretBefore = caret_;
		if (store_.setOpenTextStyle(style_.color, style_.textSize, style_.textFont)) {
			const Stroke &after = store_.visible().back().stroke;
			if (!(after.color == before.color) || after.textSize != before.textSize ||
			    after.textFont != before.textFont) {
				textChanged(before, caretBefore);
			}
		}
	}
	emit styleChanged();
	publishTestState();
}

void AnnotationController::saveStyle()
{
	saveTimer_.stop();
	config_set_string(App()->GetUserConfig(), "Voltz", "AnnotationStyle", annotationStyleToJson(style_).c_str());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

bool AnnotationController::drawTarget(CaptureMapping &mapping, std::string &deviceName) const
{
	int captureWidth = 0;
	int captureHeight = 0;
	OBSSource screen = sizedScreen(captureWidth, captureHeight);
	if (!screen) {
		return false;
	}
	const MonitorDescription monitor = capturedMonitor(screen);
	if (!monitor.found) {
		return false;
	}
	mapping = CaptureMapping{monitor.rect, captureWidth, captureHeight};
	deviceName = monitor.deviceName;
	return true;
}

void AnnotationController::setDrawStatus(bool active, const char *refusal, const char *exit)
{
	drawActive_ = active;
	drawRefusal_ = refusal ? refusal : "";
	drawExit_ = exit ? exit : "";
	publishTestState();
}

std::string AnnotationController::overlayStatsText() const
{
	const OverlayWindow::Info info = overlay_ ? overlay_->info() : OverlayWindow::Info{};
	const long long average =
		info.partialRenders ? static_cast<long long>(info.partialTotalNs / info.partialRenders / 1000) : 0;
	char text[192];
	std::snprintf(text, sizeof(text), "%u parcial(is) (media %lld us, max %lld us), %u completa(s) (max %lld us)",
		      info.partialRenders, average, static_cast<long long>(info.partialMaxNs / 1000), info.fullRenders,
		      static_cast<long long>(info.fullMaxNs / 1000));
	return text;
}

void AnnotationController::resetOverlayStats()
{
	if (overlay_) {
		overlay_->resetStats();
	}
	publishTestState();
}

void AnnotationController::markBurned(const char *why)
{
	if (burned_) {
		return;
	}
	burned_ = true;
	blog(LOG_WARNING, "[voltz] anotacoes: queimadas (%s)", why);
	sendBurned(true);
	emit burnedChanged(true);
}

void AnnotationController::probeOverlay(const std::vector<StrokePoint> &points)
{
	samples_.clear();
	for (const StrokePoint &point : points) {
		samples_.push_back(overlay_ ? overlay_->pixelHex(point.x, point.y) : std::string());
	}
	publishTestState();
}

void AnnotationController::publishTestState()
{
	if (!testMode_) {
		return;
	}
	QJsonArray ids;
	for (const LiveStroke &live : store_.visible()) {
		ids.append(live.stroke.id);
	}
	const OverlayWindow::Info info = overlay_ ? overlay_->info() : OverlayWindow::Info{};
	QJsonObject overlay;
	overlay.insert(QStringLiteral("criada"), info.exists);
	overlay.insert(QStringLiteral("visivel"), info.visible);
	overlay.insert(QStringLiteral("hwnd"), QString::number(info.hwnd));
	overlay.insert(QStringLiteral("estilo_ex"), QString::number(info.exStyle));
	overlay.insert(QStringLiteral("afinidade"), static_cast<int>(info.affinity));
	overlay.insert(QStringLiteral("retangulo"),
		       QJsonArray{info.rect.left, info.rect.top, info.rect.width, info.rect.height});
	overlay.insert(QStringLiteral("parciais"), static_cast<qint64>(info.partialRenders));
	overlay.insert(QStringLiteral("completas"), static_cast<qint64>(info.fullRenders));
	overlay.insert(QStringLiteral("parcial_us_media"),
		       info.partialRenders ? static_cast<qint64>(info.partialTotalNs / info.partialRenders / 1000)
					   : qint64(0));
	overlay.insert(QStringLiteral("parcial_us_max"), static_cast<qint64>(info.partialMaxNs / 1000));
	overlay.insert(QStringLiteral("completa_us_max"), static_cast<qint64>(info.fullMaxNs / 1000));
	QJsonArray samples;
	for (const std::string &sample : samples_) {
		samples.append(QString::fromStdString(sample));
	}
	OBSSource screen = findSharedSource(SharedSource::Screen);
	const int captureWidth = screen ? static_cast<int>(obs_source_get_width(screen)) : 0;
	const int captureHeight = screen ? static_cast<int>(obs_source_get_height(screen)) : 0;
	QJsonObject state;
	state.insert(QStringLiteral("visiveis"), ids);
	state.insert(QStringLiteral("aberto"), store_.openId());
	state.insert(QStringLiteral("ultimo"),
		     store_.visible().empty()
			     ? QString()
			     : QString::fromStdString(strokeToJsonLine(store_.visible().back().stroke)));
	state.insert(QStringLiteral("queimadas"), burned_);
	state.insert(QStringLiteral("estilo"), QString::fromStdString(annotationStyleToJson(style_)));
	state.insert(QStringLiteral("captura"), QJsonArray{captureWidth, captureHeight});
	state.insert(QStringLiteral("sobreposicao"), overlay);
	state.insert(QStringLiteral("amostras"), samples);
	state.insert(QStringLiteral("digitando"), typingText());
	state.insert(QStringLiteral("cursor_texto"), caret_.points.size() == 2 && typingText()
							     ? QJsonArray{caret_.points[0].x, caret_.points[0].y,
									  caret_.points[1].x, caret_.points[1].y}
							     : QJsonArray{});
	state.insert(QStringLiteral("modo_desenho"), drawActive_);
	state.insert(QStringLiteral("desenho_recusa"), QString::fromStdString(drawRefusal_));
	state.insert(QStringLiteral("desenho_saida"), QString::fromStdString(drawExit_));
	AnnotationTestVendor::publish(QJsonDocument(state).toJson(QJsonDocument::Compact).toStdString());
}

} // namespace voltz
