#include "mouse-source.hpp"
#include "mouse-feed.hpp"

#include <voltz/core/live-protocol.hpp>
#include <voltz/core/mouse-effects.hpp>

#include <obs-module.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <util/platform.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace voltz {

namespace {

constexpr int kTargetRetryTicks = 60;

struct MouseSource {
	obs_source_t *self = nullptr;
	gs_effect_t *effect = nullptr;
	gs_eparam_t *color = nullptr;
	gs_eparam_t *sizePx = nullptr;
	gs_eparam_t *innerPx = nullptr;
	gs_eparam_t *centerPx = nullptr;
	gs_eparam_t *radiusPx = nullptr;
	gs_eparam_t *featherPx = nullptr;
	// Alvo: escrito no update (thread de quem atualiza) e lido no tick (thread gráfica).
	std::mutex targetMutex;
	std::string targetUuid;
	obs_weak_source_t *target = nullptr;
	int retryTicks = 0;
	std::atomic<uint32_t> width{0};
	std::atomic<uint32_t> height{0};
};

// Com targetMutex preso. A Tela pode carregar depois desta fonte: o tick tenta de novo a cada kTargetRetryTicks.
void resolveTarget(MouseSource *ctx)
{
	if (ctx->target) {
		obs_weak_source_release(ctx->target);
		ctx->target = nullptr;
	}
	if (ctx->targetUuid.empty()) {
		return;
	}
	obs_source_t *target = obs_get_source_by_uuid(ctx->targetUuid.c_str());
	if (target) {
		ctx->target = obs_source_get_weak_source(target);
		obs_source_release(target);
	}
}

const char *mouseName(void *)
{
	return obs_module_text("VoltzMouse");
}

void mouseUpdate(void *data, obs_data_t *settings)
{
	auto *ctx = static_cast<MouseSource *>(data);
	const std::string uuid = obs_data_get_string(settings, live::kTargetSetting);
	std::lock_guard<std::mutex> lock(ctx->targetMutex);
	if (uuid == ctx->targetUuid && ctx->target) {
		return;
	}
	ctx->targetUuid = uuid;
	ctx->retryTicks = 0;
	resolveTarget(ctx);
}

void *mouseCreate(obs_data_t *settings, obs_source_t *source)
{
	auto *ctx = new MouseSource();
	ctx->self = source;
	char *path = obs_module_file("voltz-mouse.effect");
	obs_enter_graphics();
	ctx->effect = path ? gs_effect_create_from_file(path, nullptr) : nullptr;
	if (ctx->effect) {
		ctx->color = gs_effect_get_param_by_name(ctx->effect, "color");
		ctx->sizePx = gs_effect_get_param_by_name(ctx->effect, "size_px");
		ctx->innerPx = gs_effect_get_param_by_name(ctx->effect, "inner_px");
		ctx->centerPx = gs_effect_get_param_by_name(ctx->effect, "center_px");
		ctx->radiusPx = gs_effect_get_param_by_name(ctx->effect, "radius_px");
		ctx->featherPx = gs_effect_get_param_by_name(ctx->effect, "feather_px");
	}
	obs_leave_graphics();
	bfree(path);
	if (!ctx->effect) {
		blog(LOG_WARNING, "[voltz-sources] voltz-mouse.effect nao carregou: a fonte nao desenha");
	}
	mouseUpdate(ctx, settings);
	return ctx;
}

void mouseDestroy(void *data)
{
	auto *ctx = static_cast<MouseSource *>(data);
	if (ctx->effect) {
		obs_enter_graphics();
		gs_effect_destroy(ctx->effect);
		obs_leave_graphics();
	}
	{
		std::lock_guard<std::mutex> lock(ctx->targetMutex);
		if (ctx->target) {
			obs_weak_source_release(ctx->target);
			ctx->target = nullptr;
		}
	}
	delete ctx;
}

void mouseDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, live::kTargetSetting, "");
}

uint32_t mouseWidth(void *data)
{
	return static_cast<MouseSource *>(data)->width.load();
}

uint32_t mouseHeight(void *data)
{
	return static_cast<MouseSource *>(data)->height.load();
}

void mouseTick(void *data, float)
{
	auto *ctx = static_cast<MouseSource *>(data);
	obs_source_t *target = nullptr;
	{
		std::lock_guard<std::mutex> lock(ctx->targetMutex);
		if (!ctx->target && !ctx->targetUuid.empty() && ++ctx->retryTicks >= kTargetRetryTicks) {
			ctx->retryTicks = 0;
			resolveTarget(ctx);
		}
		if (ctx->target) {
			target = obs_weak_source_get_source(ctx->target);
			if (!target) {
				/* a Tela foi removida: solta e volta a procurar pelo UUID */
				obs_weak_source_release(ctx->target);
				ctx->target = nullptr;
			}
		}
	}
	uint32_t width = 0;
	uint32_t height = 0;
	if (target) {
		width = obs_source_get_width(target);
		height = obs_source_get_height(target);
		obs_source_release(target);
	}
	ctx->width.store(width);
	ctx->height.store(height);
}

// Cor do núcleo → vetor linear (o desenho roda com gs_enable_framebuffer_srgb, como o color_source com transparência).
struct vec4 linearColor(const RgbColor &rgb, double alpha)
{
	const uint32_t packed = 0xFF000000u | (static_cast<uint32_t>(rgb.b) << 16) |
				(static_cast<uint32_t>(rgb.g) << 8) | static_cast<uint32_t>(rgb.r);
	struct vec4 color;
	vec4_from_rgba_srgb(&color, packed);
	color.w = static_cast<float>(alpha);
	return color;
}

void drawCircle(MouseSource *ctx, double centerX, double centerY, double diameter, double innerRadius,
		const RgbColor &rgb, double alpha)
{
	if (diameter < 1.0 || alpha <= 0.0) {
		return;
	}
	const struct vec4 color = linearColor(rgb, alpha);
	struct vec2 size;
	vec2_set(&size, static_cast<float>(diameter), static_cast<float>(diameter));
	gs_effect_set_vec4(ctx->color, &color);
	gs_effect_set_vec2(ctx->sizePx, &size);
	gs_effect_set_float(ctx->innerPx, static_cast<float>(std::max(0.0, innerRadius)));
	gs_matrix_push();
	gs_matrix_translate3f(static_cast<float>(centerX - diameter / 2.0),
			      static_cast<float>(centerY - diameter / 2.0), 0.0f);
	while (gs_effect_loop(ctx->effect, "Circle")) {
		gs_draw_quadf(nullptr, 0, static_cast<float>(diameter), static_cast<float>(diameter));
	}
	gs_matrix_pop();
}

void drawSpotlight(MouseSource *ctx, const MouseSnapshot &snap, uint32_t width, uint32_t height)
{
	const SpotlightGeometry geometry = spotlightGeometry(snap.style.spotlight);
	struct vec4 color;
	vec4_set(&color, 0.0f, 0.0f, 0.0f, static_cast<float>(geometry.dim));
	struct vec2 size;
	vec2_set(&size, static_cast<float>(width), static_cast<float>(height));
	struct vec2 center;
	vec2_set(&center, static_cast<float>(snap.x) + 0.5f, static_cast<float>(snap.y) + 0.5f);
	gs_effect_set_vec4(ctx->color, &color);
	gs_effect_set_vec2(ctx->sizePx, &size);
	gs_effect_set_vec2(ctx->centerPx, &center);
	gs_effect_set_float(ctx->radiusPx, static_cast<float>(geometry.radius));
	gs_effect_set_float(ctx->featherPx, static_cast<float>(geometry.feather));
	while (gs_effect_loop(ctx->effect, "Spotlight")) {
		gs_draw_quadf(nullptr, 0, static_cast<float>(width), static_cast<float>(height));
	}
}

void mouseRender(void *data, gs_effect_t *)
{
	auto *ctx = static_cast<MouseSource *>(data);
	const uint32_t width = ctx->width.load();
	const uint32_t height = ctx->height.load();
	if (!ctx->effect || !width || !height) {
		return;
	}
	const int64_t now = static_cast<int64_t>(os_gettime_ns());
	const MouseSnapshot snap = MouseFeed::instance().snapshot(now);
	if (!snap.inside && snap.ripples.empty()) {
		return;
	}
	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);
	if (snap.inside && snap.style.spotlight.active) {
		drawSpotlight(ctx, snap, width, height);
	}
	for (const Ripple &ripple : snap.ripples) {
		const RippleFrame frame = rippleAt(snap.style, now - ripple.startNs);
		if (frame.visible) {
			drawCircle(ctx, ripple.x + 0.5, ripple.y + 0.5, frame.radius * 2.0,
				   frame.radius - frame.thickness, rippleColor(snap.style.click, ripple.button),
				   frame.alpha);
		}
	}
	if (snap.inside && snap.style.halo.active) {
		const HaloGeometry halo = haloGeometry(snap.style.halo);
		drawCircle(ctx, snap.x + 0.5, snap.y + 0.5, halo.diameter, halo.innerRadius, snap.style.halo.color,
			   snap.style.halo.opacity);
	}
	gs_enable_framebuffer_srgb(previous);
}

} // namespace

void registerMouseSource()
{
	obs_source_info info = {};
	info.id = live::kMouseSourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_SRGB | OBS_SOURCE_CAP_DISABLED;
	info.get_name = mouseName;
	info.create = mouseCreate;
	info.destroy = mouseDestroy;
	info.update = mouseUpdate;
	info.get_defaults = mouseDefaults;
	info.get_width = mouseWidth;
	info.get_height = mouseHeight;
	info.video_tick = mouseTick;
	info.video_render = mouseRender;
	info.show = [](void *) {
		MouseFeed::instance().addShowing(1);
	};
	info.hide = [](void *) {
		MouseFeed::instance().addShowing(-1);
	};
	info.icon_type = OBS_ICON_TYPE_CUSTOM;
	obs_register_source(&info);
}

} // namespace voltz
