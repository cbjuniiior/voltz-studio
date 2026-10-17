#include "annotation-feed.hpp"

#include <voltz/core/live-protocol.hpp>

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

bool AnnotationFeed::begin(int id, const std::string &json)
{
	Stroke stroke;
	if (!strokeFromJson(json, stroke)) {
		return false;
	}
	stroke.id = id;
	std::lock_guard<std::mutex> lock(mutex_);
	for (Stroke &existing : strokes_) {
		if (existing.id == id) {
			existing = std::move(stroke);
			version_++;
			return true;
		}
	}
	strokes_.push_back(std::move(stroke));
	version_++;
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
			appendStrokePoints(stroke, points);
			version_++;
			return;
		}
	}
}

void AnnotationFeed::remove(int id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto it = strokes_.begin(); it != strokes_.end(); ++it) {
		if (it->id == id) {
			strokes_.erase(it);
			version_++;
			return;
		}
	}
}

void AnnotationFeed::clear()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!strokes_.empty()) {
		strokes_.clear();
		version_++;
	}
}

void AnnotationFeed::setBurned(bool burned)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (burned_ != burned) {
		burned_ = burned;
		version_++;
		blog(LOG_INFO, "[voltz-sources] anotacoes %s",
		     burned ? "queimadas: a fonte nao desenha" : "desenhadas pela fonte");
	}
}

bool AnnotationFeed::snapshotIfChanged(uint64_t knownVersion, std::vector<Stroke> &out, uint64_t &version, bool &burned)
{
	std::lock_guard<std::mutex> lock(mutex_);
	version = version_;
	burned = burned_;
	if (knownVersion == version_) {
		return false;
	}
	out = strokes_;
	return true;
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
	char buffer[160];
	std::snprintf(buffer, sizeof(buffer),
		      "{\"tracos\":%zu,\"versao\":%llu,\"queimadas\":%s,\"mostradas\":%d,\"ids\":[", strokes_.size(),
		      static_cast<unsigned long long>(version_), burned_ ? "true" : "false", showing_.load());
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
