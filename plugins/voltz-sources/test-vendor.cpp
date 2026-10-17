#include "test-vendor.hpp"
#include "annotation-feed.hpp"
#include "mouse-feed.hpp"

#include <voltz/core/live-protocol.hpp>
#include <voltz/core/mouse-style.hpp>

#include <obs-websocket-api.h>
#include <util/platform.h>

namespace voltz {

namespace {

void injectMouse(obs_data_t *request, obs_data_t *response, void *)
{
	obs_data_set_default_bool(request, "inside", true);
	const int x = static_cast<int>(obs_data_get_int(request, "x"));
	const int y = static_cast<int>(obs_data_get_int(request, "y"));
	MouseFeed::instance().setPosition(x, y, obs_data_get_bool(request, "inside"));
	const char *click = obs_data_get_string(request, "click");
	if (click && *click) {
		const MouseButton button = click[0] == 'r'   ? MouseButton::Right
					   : click[0] == 'm' ? MouseButton::Middle
							     : MouseButton::Left;
		MouseFeed::instance().addClick(x, y, button, static_cast<int64_t>(os_gettime_ns()));
	}
	obs_data_set_bool(response, "ok", true);
}

void mouseState(obs_data_t *, obs_data_t *response, void *)
{
	const MouseSnapshot state = MouseFeed::instance().snapshot(static_cast<int64_t>(os_gettime_ns()));
	obs_data_set_int(response, "x", state.x);
	obs_data_set_int(response, "y", state.y);
	obs_data_set_bool(response, "inside", state.inside);
	obs_data_set_int(response, "ripples", static_cast<long long>(state.ripples.size()));
	obs_data_set_int(response, "showing", MouseFeed::instance().showing());
	obs_data_set_string(response, "style", mouseStyleToJson(state.style).c_str());
}

void setStyle(obs_data_t *request, obs_data_t *response, void *)
{
	obs_data_set_bool(response, "ok", MouseFeed::instance().setStyleJson(obs_data_get_string(request, "style")));
}

// Cria uma fonte Voltz `id` com alvo = UUID de `targetName` e a põe por cima na cena `sceneName`.
void addTargetedSource(obs_data_t *request, obs_data_t *response, const char *id, const char *defaultName)
{
	obs_source_t *sceneSource = obs_get_source_by_name(obs_data_get_string(request, "sceneName"));
	obs_source_t *target = obs_get_source_by_name(obs_data_get_string(request, "targetName"));
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	bool ok = false;
	if (scene && target) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, live::kTargetSetting, obs_source_get_uuid(target));
		const char *name = obs_data_get_string(request, "name");
		obs_source_t *source = obs_source_create(id, *name ? name : defaultName, settings, nullptr);
		if (source) {
			ok = obs_scene_add(scene, source) != nullptr;
			obs_source_release(source);
		}
		obs_data_release(settings);
	}
	obs_source_release(target);
	obs_source_release(sceneSource);
	obs_data_set_bool(response, "ok", ok);
}

void addMouseSource(obs_data_t *request, obs_data_t *response, void *)
{
	addTargetedSource(request, response, live::kMouseSourceId, "Mouse teste");
}

void addAnnotationSource(obs_data_t *request, obs_data_t *response, void *)
{
	/* nome diferente da cena "Anotacoes teste" dos testes: a busca por nome acharia esta fonte no lugar da cena */
	addTargetedSource(request, response, live::kAnnotationsSourceId, "Voltz Anotacoes teste");
}

void annotationFeedState(obs_data_t *, obs_data_t *response, void *)
{
	obs_data_set_string(response, "estado", AnnotationFeed::instance().describe().c_str());
	obs_data_set_bool(response, "ok", true);
}

void feedStroke(obs_data_t *request, obs_data_t *response, void *)
{
	const bool ok = AnnotationFeed::instance().begin(static_cast<int>(obs_data_get_int(request, "id")),
							 obs_data_get_string(request, "json"));
	obs_data_set_bool(response, "ok", ok);
}

void feedClear(obs_data_t *, obs_data_t *response, void *)
{
	AnnotationFeed::instance().clear();
	obs_data_set_bool(response, "ok", true);
}

} // namespace

void registerTestVendor()
{
	obs_websocket_vendor vendor = obs_websocket_register_vendor(live::kTestVendor);
	if (!vendor) {
		blog(LOG_WARNING, "[voltz-sources] obs-websocket ausente: sem pedidos de teste");
		return;
	}
	obs_websocket_vendor_register_request(vendor, "InjectMouse", injectMouse, nullptr);
	obs_websocket_vendor_register_request(vendor, "MouseState", mouseState, nullptr);
	obs_websocket_vendor_register_request(vendor, "SetStyle", setStyle, nullptr);
	obs_websocket_vendor_register_request(vendor, "AddMouseSource", addMouseSource, nullptr);
	obs_websocket_vendor_register_request(vendor, "AddAnnotationSource", addAnnotationSource, nullptr);
	obs_websocket_vendor_register_request(vendor, "AnnotationFeedState", annotationFeedState, nullptr);
	obs_websocket_vendor_register_request(vendor, "FeedStroke", feedStroke, nullptr);
	obs_websocket_vendor_register_request(vendor, "FeedClear", feedClear, nullptr);
	blog(LOG_WARNING, "[voltz-sources] modo de teste (%s): pedidos '%s' ligados", live::kTestEnv,
	     live::kTestVendor);
}

} // namespace voltz
