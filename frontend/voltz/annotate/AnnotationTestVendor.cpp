#include "AnnotationTestVendor.hpp"
#include "AnnotationController.hpp"
#include "DrawToolbar.hpp"

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/live-protocol.hpp>

#include <obs-websocket-api.h>

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QMetaObject>
#include <QPixmap>

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace voltz {

namespace {

std::mutex gMutex;
AnnotationController *gTarget = nullptr; // protegido por gMutex
std::string gState = "{}";               // protegido por gMutex
obs_websocket_vendor gVendor = nullptr;  // só na thread da UI

constexpr const char *kRequests[] = {"InjectStroke",    "ExtendStroke",       "EndStroke",
				     "UndoStroke",      "ClearStrokes",       "ProbeOverlay",
				     "AnnotationState", "SetAnnotationStyle", "ToolbarImage"};

// Entrega `work` na thread da UI sem esperar; falso se o controlador já saiu (EXIT). O mutex é segurado enquanto o evento
// é postado: o stop() (que anula gTarget antes de o controlador morrer) espera; evento de objeto destruído o Qt descarta.
bool post(std::function<void(AnnotationController *)> work)
{
	std::lock_guard<std::mutex> lock(gMutex);
	if (!gTarget) {
		return false;
	}
	AnnotationController *target = gTarget;
	QMetaObject::invokeMethod(target, [target, work]() { work(target); }, Qt::QueuedConnection);
	return true;
}

std::vector<StrokePoint> pointsOf(obs_data_t *request)
{
	std::vector<StrokePoint> points;
	pointsFromJson(obs_data_get_string(request, "pts"), points);
	return points;
}

void injectStroke(obs_data_t *request, obs_data_t *response, void *)
{
	const std::vector<StrokePoint> points = pointsOf(request);
	const std::string toolText = obs_data_get_string(request, "ferr");
	const std::string colorText = obs_data_get_string(request, "cor");
	const int width = static_cast<int>(obs_data_get_int(request, "esp"));
	const bool open = obs_data_get_bool(request, "aberto");
	bool ok = !points.empty();
	if (ok) {
		ok = post([points, toolText, colorText, width, open](AnnotationController *controller) {
			AnnotationTool tool = controller->style().tool;
			toolFromString(toolText, tool);
			RgbColor color = controller->style().color;
			parseHexColor(colorText, color);
			const int strokeWidth = width > 0 ? width : controller->style().width;
			if (!controller->beginStroke(tool, color, strokeWidth, points.front())) {
				return;
			}
			if (points.size() > 1) {
				controller->extendStroke(std::vector<StrokePoint>(points.begin() + 1, points.end()));
			}
			if (!open) {
				controller->endStroke();
			}
		});
	}
	obs_data_set_bool(response, "ok", ok);
}

void extendStroke(obs_data_t *request, obs_data_t *response, void *)
{
	const std::vector<StrokePoint> points = pointsOf(request);
	const bool ok = !points.empty() &&
			post([points](AnnotationController *controller) { controller->extendStroke(points); });
	obs_data_set_bool(response, "ok", ok);
}

void endStroke(obs_data_t *, obs_data_t *response, void *)
{
	obs_data_set_bool(response, "ok", post([](AnnotationController *controller) { controller->endStroke(); }));
}

void undoStroke(obs_data_t *, obs_data_t *response, void *)
{
	obs_data_set_bool(response, "ok", post([](AnnotationController *controller) { controller->undo(); }));
}

void clearStrokes(obs_data_t *, obs_data_t *response, void *)
{
	obs_data_set_bool(response, "ok", post([](AnnotationController *controller) { controller->clear(); }));
}

void probeOverlay(obs_data_t *request, obs_data_t *response, void *)
{
	const std::vector<StrokePoint> points = pointsOf(request);
	obs_data_set_bool(response, "ok",
			  post([points](AnnotationController *controller) { controller->probeOverlay(points); }));
}

void setAnnotationStyle(obs_data_t *request, obs_data_t *response, void *)
{
	AnnotationStyle style;
	const bool parsed = annotationStyleFromJson(obs_data_get_string(request, "estilo"), style);
	const bool ok = parsed && post([style](AnnotationController *controller) { controller->setStyle(style); });
	obs_data_set_bool(response, "ok", ok);
}

void toolbarImage(obs_data_t *, obs_data_t *response, void *)
{
	struct Result {
		std::string png;
		int width = 0;
		int height = 0;
		bool native = true;
	};
	auto result = std::make_shared<Result>();
	auto done = std::make_shared<std::promise<void>>();
	std::future<void> ready = done->get_future();
	bool ok = post([result, done](AnnotationController *controller) {
		/* nunca mostrada: o grab() desenha fora da tela, sem criar janela nativa */
		DrawToolbar toolbar;
		toolbar.setAnnotationStyle(controller->style());
		toolbar.setBurned(controller->burned());
		toolbar.adjustSize();
		const QImage image = toolbar.grab().toImage();
		QByteArray bytes;
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, "PNG");
		result->png = bytes.toBase64().toStdString();
		result->width = image.width();
		result->height = image.height();
		result->native = toolbar.testAttribute(Qt::WA_WState_Created);
		done->set_value();
	});
	if (ok) {
		done.reset();
		ok = ready.wait_for(std::chrono::seconds(3)) == std::future_status::ready && result->width > 0;
	}
	if (ok) {
		obs_data_set_string(response, "png", result->png.c_str());
		obs_data_set_int(response, "largura", result->width);
		obs_data_set_int(response, "altura", result->height);
		obs_data_set_bool(response, "janela_nativa", result->native);
	}
	obs_data_set_bool(response, "ok", ok);
}

void annotationState(obs_data_t *, obs_data_t *response, void *)
{
	/* Republica o estado na thread da UI antes de responder (a captura pode ter mudado sem traço novo) e espera até
	 * 2 s; se o controlador sair antes, o evento é descartado, a promessa é destruída e a espera acaba na hora. */
	auto done = std::make_shared<std::promise<void>>();
	std::future<void> ready = done->get_future();
	if (post([done](AnnotationController *controller) {
		    controller->publishTestState();
		    done->set_value();
	    })) {
		done.reset();
		(void)ready.wait_for(std::chrono::seconds(2));
	}
	std::lock_guard<std::mutex> lock(gMutex);
	obs_data_set_string(response, "estado", gState.c_str());
	obs_data_set_bool(response, "ok", gTarget != nullptr);
}

} // namespace

void AnnotationTestVendor::start(AnnotationController *controller)
{
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gTarget = controller;
	}
	if (!gVendor) {
		gVendor = obs_websocket_register_vendor(live::kTestVendorShell);
	}
	if (!gVendor) {
		blog(LOG_WARNING, "[voltz] obs-websocket ausente: sem pedidos de teste '%s'", live::kTestVendorShell);
		return;
	}
	obs_websocket_vendor_register_request(gVendor, "InjectStroke", injectStroke, nullptr);
	obs_websocket_vendor_register_request(gVendor, "ExtendStroke", extendStroke, nullptr);
	obs_websocket_vendor_register_request(gVendor, "EndStroke", endStroke, nullptr);
	obs_websocket_vendor_register_request(gVendor, "UndoStroke", undoStroke, nullptr);
	obs_websocket_vendor_register_request(gVendor, "ClearStrokes", clearStrokes, nullptr);
	obs_websocket_vendor_register_request(gVendor, "ProbeOverlay", probeOverlay, nullptr);
	obs_websocket_vendor_register_request(gVendor, "AnnotationState", annotationState, nullptr);
	obs_websocket_vendor_register_request(gVendor, "SetAnnotationStyle", setAnnotationStyle, nullptr);
	obs_websocket_vendor_register_request(gVendor, "ToolbarImage", toolbarImage, nullptr);
	blog(LOG_WARNING, "[voltz] modo de teste (%s): pedidos '%s' ligados", live::kTestEnv, live::kTestVendorShell);
}

void AnnotationTestVendor::stop()
{
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gTarget = nullptr;
	}
	if (gVendor) {
		for (const char *request : kRequests) {
			obs_websocket_vendor_unregister_request(gVendor, request);
		}
	}
}

void AnnotationTestVendor::publish(const std::string &stateJson)
{
	std::lock_guard<std::mutex> lock(gMutex);
	gState = stateJson;
}

} // namespace voltz
