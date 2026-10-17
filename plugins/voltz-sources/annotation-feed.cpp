#include "annotation-feed.hpp"

#include <voltz/core/live-protocol.hpp>

#include <algorithm>
#include <cstdio>
#include <utility>

namespace voltz {

namespace {

int idOf(calldata_t *params)
{
	return static_cast<int>(calldata_int(params, "id"));
}

void procBegin(void *, calldata_t *params)
{
	const char *json = calldata_string(params, "json");
	if (!json || !AnnotationFeed::instance().begin(idOf(params), json)) {
		blog(LOG_WARNING, "[voltz-sources] traco invalido ignorado");
	}
}

void procExtend(void *, calldata_t *params)
{
	const char *points = calldata_string(params, "points");
	if (points) {
		AnnotationFeed::instance().extend(idOf(params), points);
	}
}

void procRemove(void *, calldata_t *params)
{
	AnnotationFeed::instance().remove(idOf(params));
}

void procClear(void *, calldata_t *)
{
	AnnotationFeed::instance().clear();
}

void procBurned(void *, calldata_t *params)
{
	AnnotationFeed::instance().setBurned(calldata_bool(params, "burned"));
}

} // namespace

AnnotationFeed &AnnotationFeed::instance()
{
	static AnnotationFeed feed;
	return feed;
}

void AnnotationFeed::changed(const PixelRect &rect, bool full)
{
	version_++;
	history_.record(version_, rect, full);
}

bool AnnotationFeed::begin(int id, const std::string &json)
{
	Stroke stroke;
	if (!strokeFromJson(json, stroke)) {
		return false;
	}
	stroke.id = id;
	const PixelRect area = strokeBounds(stroke, 1.0, 1.0);
	std::lock_guard<std::mutex> lock(mutex_);
	for (Stroke &existing : strokes_) {
		if (existing.id == id) {
			const PixelRect old = strokeBounds(existing, 1.0, 1.0);
			existing = std::move(stroke);
			changed(uniteRects(old, area), false);
			return true;
		}
	}
	strokes_.push_back(std::move(stroke));
	changed(area, false);
	return true;
}

void AnnotationFeed::extend(int id, const std::string &pointsJson)
{
	std::vector<StrokePoint> points;
	if (!pointsFromJson(pointsJson, points) || points.empty()) {
		return;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	for (Stroke &stroke : strokes_) {
		if (stroke.id == id) {
			const StrokeTail tail = strokeTail(stroke);
			appendStrokePoints(stroke, points);
			changed(appendDirtyRect(tail, stroke, 1.0, 1.0), false);
			return;
		}
	}
}

void AnnotationFeed::remove(int id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto it = strokes_.begin(); it != strokes_.end(); ++it) {
		if (it->id == id) {
			const PixelRect area = strokeBounds(*it, 1.0, 1.0);
			strokes_.erase(it);
			changed(area, false);
			return;
		}
	}
}

void AnnotationFeed::clear()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!strokes_.empty()) {
		strokes_.clear();
		changed(PixelRect{}, true);
	}
}

void AnnotationFeed::setBurned(bool burned)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (burned_ != burned) {
		burned_ = burned;
		changed(PixelRect{}, true);
		blog(LOG_INFO, "[voltz-sources] anotacoes %s",
		     burned ? "queimadas: a fonte nao desenha" : "desenhadas pela fonte");
	}
}

bool AnnotationFeed::snapshotIfChanged(uint64_t knownVersion, std::vector<Stroke> &out, uint64_t &version, bool &burned,
				       PixelRect &dirty, bool &full)
{
	std::lock_guard<std::mutex> lock(mutex_);
	version = version_;
	burned = burned_;
	dirty = PixelRect{};
	full = false;
	if (knownVersion == version_) {
		return false;
	}
	full = !history_.since(knownVersion, version_, dirty);
	out = strokes_;
	return true;
}

void AnnotationFeed::recordRaster(int64_t elapsedNs, bool partial)
{
	uint32_t partials = 0;
	uint32_t total = 0;
	int64_t maxNs = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		rasterLastNs_ = elapsedNs;
		rasterMaxNs_ = std::max(rasterMaxNs_, elapsedNs);
		if (partial) {
			partialRasters_++;
		} else {
			fullRasters_++;
		}
		partials = partialRasters_;
		total = partialRasters_ + fullRasters_;
		maxNs = rasterMaxNs_;
	}
	if (total % 600 == 0) {
		blog(LOG_INFO,
		     "[voltz-sources] anotacoes: %u redesenho(s) (%u parcial(is)), ultimo %lld us, max %lld us", total,
		     partials, static_cast<long long>(elapsedNs / 1000), static_cast<long long>(maxNs / 1000));
	}
}

std::string AnnotationFeed::describe()
{
	std::lock_guard<std::mutex> lock(mutex_);
	std::string ids;
	for (const Stroke &stroke : strokes_) {
		if (!ids.empty()) {
			ids += ',';
		}
		ids += std::to_string(stroke.id);
	}
	char buffer[320];
	std::snprintf(buffer, sizeof(buffer),
		      "{\"tracos\":%zu,\"versao\":%llu,\"queimadas\":%s,\"mostradas\":%d,\"raster_us_ultimo\":%lld,"
		      "\"raster_us_max\":%lld,\"parciais\":%u,\"completas\":%u,\"ids\":[",
		      strokes_.size(), static_cast<unsigned long long>(version_), burned_ ? "true" : "false",
		      showing_.load(), static_cast<long long>(rasterLastNs_ / 1000),
		      static_cast<long long>(rasterMaxNs_ / 1000), partialRasters_, fullRasters_);
	return std::string(buffer) + ids + "]}";
}

void registerAnnotationProcs(proc_handler_t *handler)
{
	proc_handler_add(handler, live::kDeclAnnotBegin, procBegin, nullptr);
	proc_handler_add(handler, live::kDeclAnnotExtend, procExtend, nullptr);
	proc_handler_add(handler, live::kDeclAnnotRemove, procRemove, nullptr);
	proc_handler_add(handler, live::kDeclAnnotClear, procClear, nullptr);
	proc_handler_add(handler, live::kDeclAnnotBurned, procBurned, nullptr);
}

} // namespace voltz
