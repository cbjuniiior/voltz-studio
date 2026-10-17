#include "AnnotationController.hpp"
#include "AnnotationTestVendor.hpp"
#include "OverlayWindow.hpp"

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
#include <cstring>
#include <utility>

#include "moc_AnnotationController.cpp"

namespace voltz {

namespace {

constexpr int kOverlayPollMs = 1000;
constexpr int64_t kNsPerMs = 1000000;
constexpr int64_t kMaxTimerMs = 3600000;

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
	const int id = store_.begin(tool, color, width, first, nowNs());
	if (id == 0) {
		return 0;
	}
	sendBegin(store_.visible().back().stroke);
	refreshOverlay(true);
	return id;
}

void AnnotationController::extendStroke(const std::vector<StrokePoint> &points)
{
	if (shutdown_ || !store_.extend(points)) {
		return;
	}
	sendExtend(store_.openId(), points);
	refreshOverlay(true);
}

void AnnotationController::endStroke()
{
	if (shutdown_) {
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
	if (!shutdown_) {
		removed(store_.undo(nowNs()), false);
	}
}

void AnnotationController::clear()
{
	if (!shutdown_) {
		removed(store_.clear(nowNs()), true);
	}
}

void AnnotationController::removed(const std::vector<RemovedStroke> &strokes, bool all)
{
	if (strokes.empty()) {
		return;
	}
	if (all) {
		sendClear();
	} else {
		for (const RemovedStroke &stroke : strokes) {
			sendRemove(stroke.live.stroke.id);
		}
	}
	emit strokesRemoved(strokes);
	scheduleExpiry();
	refreshOverlay(true);
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
	OBSSource screen = findSharedSource(SharedSource::Screen);
	if (!screen) {
		return false;
	}
	captureWidth = static_cast<int>(obs_source_get_width(screen));
	captureHeight = static_cast<int>(obs_source_get_height(screen));
	if (captureWidth <= 0 || captureHeight <= 0) {
		return false;
	}
	if (testOverlayRect(rect)) {
		return true;
	}
	if (strcmp(obs_source_get_unversioned_id(screen), "monitor_capture") != 0) {
		return false;
	}
	OBSDataAutoRelease settings = obs_source_get_settings(screen);
	const MonitorDescription monitor = findMonitor(obs_data_get_string(settings, "monitor_id"));
	if (!monitor.found) {
		return false;
	}
	rect = monitor.rect;
	return true;
}

void AnnotationController::refreshOverlay(bool strokesChanged)
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
	const std::vector<Stroke> strokes = store_.strokes();
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
	if (!overlay_) {
		auto overlay = std::make_unique<OverlayWindow>();
		if (!overlay->create(rect)) {
			blog(LOG_WARNING, "[voltz] anotacoes: a sobreposicao nao foi criada");
			publishTestState();
			return;
		}
		overlay_ = std::move(overlay);
		if (!burned_ && !overlay_->excludeFromCapture()) {
			markBurned("afinidade recusada");
		}
		blog(LOG_INFO, "[voltz] anotacoes: sobreposicao criada (%s)",
		     burned_ ? "visivel na captura" : "fora da captura");
	} else {
		overlay_->setRect(rect);
	}
	overlay_->render(strokes, captureWidth, captureHeight);
	if (strokes.empty()) {
		overlayPoll_.stop();
	} else if (!overlayPoll_.isActive()) {
		overlayPoll_.start();
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
	state.insert(QStringLiteral("modo_desenho"), false);
	AnnotationTestVendor::publish(QJsonDocument(state).toJson(QJsonDocument::Compact).toStdString());
}

} // namespace voltz
