#include "annotation-source.hpp"
#include "annotation-feed.hpp"
#include "target-size.hpp"

#include <voltz/core/annotation-raster.hpp>
#include <voltz/core/live-protocol.hpp>

#include <obs-module.h>
#include <util/platform.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace voltz {

namespace {

struct AnnotationSource {
	TargetSize target;
	// Se esta fonte está contada em AnnotationFeed::showing (ver o mesmo campo da Voltz Mouse).
	std::atomic<bool> counted{false};
	// Só na thread gráfica (render; destroy dentro de obs_enter_graphics):
	gs_texture_t *texture = nullptr;
	uint32_t textureWidth = 0;
	uint32_t textureHeight = 0;
	uint64_t builtVersion = 0;
	bool hasStrokes = false;
	std::vector<uint8_t> pixels;
	std::vector<Stroke> strokes;
};

void annotationsShow(void *data)
{
	if (!static_cast<AnnotationSource *>(data)->counted.exchange(true)) {
		AnnotationFeed::instance().addShowing(1);
	}
}

void annotationsHide(void *data)
{
	if (static_cast<AnnotationSource *>(data)->counted.exchange(false)) {
		AnnotationFeed::instance().addShowing(-1);
	}
}

const char *annotationsName(void *)
{
	return obs_module_text("VoltzAnnotations");
}

void annotationsUpdate(void *data, obs_data_t *settings)
{
	static_cast<AnnotationSource *>(data)->target.setUuid(obs_data_get_string(settings, live::kTargetSetting));
}

void *annotationsCreate(obs_data_t *settings, obs_source_t *)
{
	auto *ctx = new AnnotationSource();
	annotationsUpdate(ctx, settings);
	return ctx;
}

void annotationsDestroy(void *data)
{
	auto *ctx = static_cast<AnnotationSource *>(data);
	annotationsHide(ctx);
	if (ctx->texture) {
		obs_enter_graphics();
		gs_texture_destroy(ctx->texture);
		obs_leave_graphics();
	}
	delete ctx;
}

void annotationsDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, live::kTargetSetting, "");
}

uint32_t annotationsWidth(void *data)
{
	return static_cast<AnnotationSource *>(data)->target.width();
}

uint32_t annotationsHeight(void *data)
{
	return static_cast<AnnotationSource *>(data)->target.height();
}

void annotationsTick(void *data, float)
{
	static_cast<AnnotationSource *>(data)->target.tick();
}

// Refaz a imagem (CPU, núcleo) e manda para a textura. Só quando a versão do feed ou o tamanho muda; com a textura do
// mesmo tamanho e uma região conhecida, redesenha só a região (o envio continua inteiro: textura dinâmica do D3D11 é
// mapeada com WRITE_DISCARD).
void rebuild(AnnotationSource *ctx, uint32_t width, uint32_t height, uint64_t version, bool full,
	     const PixelRect &dirty)
{
	const int64_t start = static_cast<int64_t>(os_gettime_ns());
	const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
	const bool partial = !full && ctx->texture && ctx->textureWidth == width && ctx->textureHeight == height &&
			     ctx->pixels.size() == bytes;
	/* região fora da imagem = nada mudou nela: avança a versão sem reenviar a textura */
	const PixelRect area =
		intersectRects(dirty, PixelRect{0, 0, static_cast<int>(width), static_cast<int>(height)});
	if (partial && area.empty()) {
		ctx->hasStrokes = !ctx->strokes.empty();
		ctx->builtVersion = version;
		return;
	}
	ctx->pixels.resize(bytes);
	const RasterImage image{ctx->pixels.data(), static_cast<int>(width), static_cast<int>(height),
				static_cast<int>(width) * 4};
	std::vector<const Stroke *> strokes;
	strokes.reserve(ctx->strokes.size());
	for (const Stroke &stroke : ctx->strokes) {
		strokes.push_back(&stroke);
	}
	if (partial) {
		redrawRegion(image, strokes, 1.0, 1.0, area);
	} else {
		clearRaster(image);
		for (const Stroke *stroke : strokes) {
			rasterizeStroke(image, *stroke, 1.0, 1.0);
		}
	}
	if (!ctx->texture || ctx->textureWidth != width || ctx->textureHeight != height) {
		if (ctx->texture) {
			gs_texture_destroy(ctx->texture);
		}
		ctx->texture = gs_texture_create(width, height, GS_BGRA, 1, nullptr, GS_DYNAMIC);
		ctx->textureWidth = width;
		ctx->textureHeight = height;
	}
	if (ctx->texture) {
		gs_texture_set_image(ctx->texture, ctx->pixels.data(), width * 4, false);
	}
	ctx->hasStrokes = !ctx->strokes.empty();
	ctx->builtVersion = version;
	AnnotationFeed::instance().recordRaster(static_cast<int64_t>(os_gettime_ns()) - start, partial);
}

void annotationsRender(void *data, gs_effect_t *effect)
{
	auto *ctx = static_cast<AnnotationSource *>(data);
	const uint32_t width = ctx->target.width();
	const uint32_t height = ctx->target.height();
	if (!width || !height || !effect) {
		return;
	}
	const bool resized = width != ctx->textureWidth || height != ctx->textureHeight;
	uint64_t version = 0;
	bool burned = false;
	PixelRect dirty;
	bool full = true;
	const bool changed = AnnotationFeed::instance().snapshotIfChanged(resized ? 0 : ctx->builtVersion, ctx->strokes,
									  version, burned, dirty, full);
	if (burned) {
		return;
	}
	if (changed || resized) {
		if (ctx->strokes.empty() && !ctx->texture) {
			/* nada a desenhar e nada alocado: não cria textura à toa */
			ctx->hasStrokes = false;
			ctx->builtVersion = version;
			return;
		}
		rebuild(ctx, width, height, version, full || resized, dirty);
	}
	if (!ctx->hasStrokes || !ctx->texture) {
		return;
	}
	/* Fonte sem OBS_SOURCE_SRGB: a imagem é BGRA pré-multiplicada em espaço gama (a mesma da sobreposição). O render
	 * principal liga o framebuffer sRGB; desliga aqui para a mistura ficar em gama, igual ao UpdateLayeredWindow. */
	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(false);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), ctx->texture);
	gs_draw_sprite(ctx->texture, 0, width, height);
	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(previous);
}

} // namespace

void registerAnnotationsSource()
{
	obs_source_info info = {};
	info.id = live::kAnnotationsSourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	/* sem OBS_SOURCE_CUSTOM_DRAW: o libobs chama o render dentro da técnica "Draw" do efeito padrão */
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CAP_DISABLED;
	info.get_name = annotationsName;
	info.create = annotationsCreate;
	info.destroy = annotationsDestroy;
	info.update = annotationsUpdate;
	info.get_defaults = annotationsDefaults;
	info.get_width = annotationsWidth;
	info.get_height = annotationsHeight;
	info.video_tick = annotationsTick;
	info.video_render = annotationsRender;
	info.show = annotationsShow;
	info.hide = annotationsHide;
	info.icon_type = OBS_ICON_TYPE_CUSTOM;
	obs_register_source(&info);
}

} // namespace voltz
