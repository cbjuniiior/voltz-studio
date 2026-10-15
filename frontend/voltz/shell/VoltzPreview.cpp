#include "VoltzPreview.hpp"

#include <voltz/core/preview-fit.hpp>

#include <obs.hpp>

#include <QColor>

#include "moc_VoltzPreview.cpp"

namespace voltz {

VoltzPreview::VoltzPreview(QWidget *parent) : OBSQTDisplay(parent)
{
	SetDisplayBackgroundColor(QColor(0x09, 0x09, 0x0B));
	connect(this, &OBSQTDisplay::DisplayCreated, this, [this](OBSQTDisplay *) {
		obs_display_add_draw_callback(GetDisplay(), &VoltzPreview::draw, this);
	});
}

VoltzPreview::~VoltzPreview()
{
	if (obs_display_t *display = GetDisplay())
		obs_display_remove_draw_callback(display, &VoltzPreview::draw, this);
}

void VoltzPreview::draw(void *, uint32_t cx, uint32_t cy)
{
	obs_video_info ovi;
	if (!obs_get_video_info(&ovi))
		return;

	const FitRect r = fitCentered(int(ovi.base_width), int(ovi.base_height), int(cx), int(cy));
	if (r.cx <= 0 || r.cy <= 0)
		return;

	gs_viewport_push();
	gs_projection_push();

	gs_ortho(0.0f, float(ovi.base_width), 0.0f, float(ovi.base_height), -100.0f, 100.0f);
	gs_set_viewport(r.x, r.y, r.cx, r.cy);
	obs_render_main_texture_src_color_only();
	gs_load_vertexbuffer(nullptr);

	gs_projection_pop();
	gs_viewport_pop();
}

} // namespace voltz
