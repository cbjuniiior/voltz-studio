#include "stroke-store.hpp"

#include <algorithm>
#include <utility>

namespace voltz {

int StrokeStore::begin(AnnotationTool tool, const RgbColor &color, int width, const StrokePoint &first, int64_t nowNs)
{
	if (openId() != 0) {
		return 0;
	}
	LiveStroke live;
	live.stroke.id = nextId_++;
	live.stroke.tool = tool;
	live.stroke.color = color;
	live.stroke.width = clampStrokeWidth(width);
	appendStrokePoints(live.stroke, {first});
	live.startNs = nowNs;
	visible_.push_back(std::move(live));
	return visible_.back().stroke.id;
}

bool StrokeStore::extend(const std::vector<StrokePoint> &points)
{
	if (points.empty() || visible_.empty() || !visible_.back().open()) {
		return false;
	}
	appendStrokePoints(visible_.back().stroke, points);
	return true;
}

const LiveStroke *StrokeStore::end(int64_t nowNs)
{
	if (visible_.empty() || !visible_.back().open()) {
		return nullptr;
	}
	LiveStroke &live = visible_.back();
	live.endNs = std::max(nowNs, live.startNs);
	return &live;
}

std::vector<RemovedStroke> StrokeStore::undo(int64_t nowNs)
{
	if (visible_.empty()) {
		return {};
	}
	RemovedStroke removed{visible_.back(), nowNs};
	visible_.pop_back();
	return {removed};
}

std::vector<RemovedStroke> StrokeStore::clear(int64_t nowNs)
{
	std::vector<RemovedStroke> removed;
	removed.reserve(visible_.size());
	for (const LiveStroke &live : visible_) {
		removed.push_back(RemovedStroke{live, nowNs});
	}
	visible_.clear();
	return removed;
}

std::vector<RemovedStroke> StrokeStore::expire(int64_t nowNs, int64_t lifetimeNs)
{
	if (lifetimeNs <= 0) {
		return {};
	}
	std::vector<RemovedStroke> removed;
	std::vector<LiveStroke> kept;
	kept.reserve(visible_.size());
	for (LiveStroke &live : visible_) {
		if (!live.open() && live.endNs + lifetimeNs <= nowNs) {
			removed.push_back(RemovedStroke{live, live.endNs + lifetimeNs});
		} else {
			kept.push_back(std::move(live));
		}
	}
	visible_ = std::move(kept);
	return removed;
}

int64_t StrokeStore::nextExpiryNs(int64_t lifetimeNs) const
{
	if (lifetimeNs <= 0) {
		return -1;
	}
	int64_t next = -1;
	for (const LiveStroke &live : visible_) {
		if (!live.open() && (next < 0 || live.endNs + lifetimeNs < next)) {
			next = live.endNs + lifetimeNs;
		}
	}
	return next;
}

std::vector<Stroke> StrokeStore::strokes() const
{
	std::vector<Stroke> out;
	out.reserve(visible_.size());
	for (const LiveStroke &live : visible_) {
		out.push_back(live.stroke);
	}
	return out;
}

int StrokeStore::openId() const
{
	return !visible_.empty() && visible_.back().open() ? visible_.back().stroke.id : 0;
}

} // namespace voltz
