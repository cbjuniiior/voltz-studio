#include "mouse-feed.hpp"

#include <voltz/core/live-protocol.hpp>

namespace voltz {

namespace {

void procPosition(void *, calldata_t *params)
{
	MouseFeed::instance().setPosition(static_cast<int>(calldata_int(params, "x")),
					  static_cast<int>(calldata_int(params, "y")), calldata_bool(params, "inside"));
}

void procClick(void *, calldata_t *params)
{
	MouseFeed::instance().addClick(static_cast<int>(calldata_int(params, "x")),
				       static_cast<int>(calldata_int(params, "y")),
				       live::buttonFromCode(static_cast<int>(calldata_int(params, "button"))),
				       static_cast<int64_t>(calldata_int(params, "t_ns")));
}

void procStyle(void *, calldata_t *params)
{
	const char *json = calldata_string(params, "json");
	if (!json || !MouseFeed::instance().setStyleJson(json)) {
		blog(LOG_WARNING, "[voltz-sources] estilo do mouse invalido; mantido o anterior");
	}
}

void procWanted(void *, calldata_t *params)
{
	calldata_set_int(params, "count", MouseFeed::instance().showing());
}

} // namespace

MouseFeed &MouseFeed::instance()
{
	static MouseFeed feed;
	return feed;
}

void MouseFeed::setPosition(int x, int y, bool inside)
{
	std::lock_guard<std::mutex> lock(mutex_);
	x_ = x;
	y_ = y;
	inside_ = inside;
}

void MouseFeed::addClick(int x, int y, MouseButton button, int64_t tNs)
{
	std::lock_guard<std::mutex> lock(mutex_);
	pushRipple(ripples_, Ripple{x, y, button, tNs});
}

bool MouseFeed::setStyleJson(const std::string &json)
{
	MouseStyle style;
	if (!mouseStyleFromJson(json, style)) {
		return false;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	style_ = style;
	return true;
}

MouseSnapshot MouseFeed::snapshot(int64_t nowNs)
{
	std::lock_guard<std::mutex> lock(mutex_);
	pruneRipples(ripples_, nowNs);
	MouseSnapshot snap;
	snap.inside = inside_;
	snap.x = x_;
	snap.y = y_;
	snap.style = style_;
	snap.ripples = ripples_;
	return snap;
}

void registerMouseProcs(proc_handler_t *handler)
{
	proc_handler_add(handler, live::kDeclPosition, procPosition, nullptr);
	proc_handler_add(handler, live::kDeclClick, procClick, nullptr);
	proc_handler_add(handler, live::kDeclStyle, procStyle, nullptr);
	proc_handler_add(handler, live::kDeclWanted, procWanted, nullptr);
}

} // namespace voltz
