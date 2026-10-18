#include "annotation-journal.hpp"
#include "annotation-text.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <map>
#include <utility>

namespace voltz {

namespace {

// Texto ainda em branco (cancelado por limpar, ou aberto no fim da gravação): nada apareceu, nada a registrar.
bool blankText(const LiveStroke &live)
{
	return live.stroke.tool == AnnotationTool::Text && textIsBlank(live.stroke.text);
}

} // namespace

int64_t AnnotationJournal::relativeMs(int64_t ns) const
{
	const int64_t delta = ns - t0Ns_;
	return delta > 0 ? delta / 1000000 : 0;
}

std::string AnnotationJournal::fullLine(const LiveStroke &live, int64_t endNs, int64_t clearedNs)
{
	Stroke stroke = live.stroke;
	stroke.startMs = relativeMs(live.startNs);
	stroke.endMs = endNs >= 0 ? std::max(stroke.startMs, relativeMs(endNs)) : -1;
	stroke.clearedMs = clearedNs >= 0 ? std::max(std::max(stroke.startMs, stroke.endMs), relativeMs(clearedNs))
					  : -1;
	written_.insert(stroke.id);
	return strokeToJsonLine(stroke);
}

std::vector<std::string> AnnotationJournal::initialLines(const std::vector<LiveStroke> &visible)
{
	std::vector<std::string> lines;
	for (const LiveStroke &live : visible) {
		if (!live.open() && written_.count(live.stroke.id) == 0 && !blankText(live)) {
			lines.push_back(fullLine(live, live.endNs, -1));
		}
	}
	return lines;
}

std::string AnnotationJournal::strokeFinished(const LiveStroke &live)
{
	if (live.open() || written_.count(live.stroke.id) != 0 || blankText(live)) {
		return {};
	}
	return fullLine(live, live.endNs, -1);
}

std::string AnnotationJournal::strokeRemoved(const RemovedStroke &removed)
{
	const LiveStroke &live = removed.live;
	if (written_.count(live.stroke.id) == 0) {
		if (blankText(live)) {
			return {};
		}
		return fullLine(live, live.open() ? removed.removedNs : live.endNs, removed.removedNs);
	}
	char buffer[96];
	std::snprintf(buffer, sizeof(buffer), "{\"id\":%d,\"%s\":%lld}", live.stroke.id, kAnnotationsJournalUpdateKey,
		      static_cast<long long>(relativeMs(removed.removedNs)));
	return buffer;
}

std::vector<std::string> AnnotationJournal::finishOpen(const std::vector<LiveStroke> &visible, int64_t endNs)
{
	std::vector<std::string> lines;
	for (const LiveStroke &live : visible) {
		if (live.open() && written_.count(live.stroke.id) == 0 && !blankText(live)) {
			lines.push_back(fullLine(live, endNs, -1));
		}
	}
	return lines;
}

std::string consolidateAnnotations(const std::string &jsonl)
{
	using Json = nlohmann::ordered_json;
	std::vector<Json> strokes;
	std::map<int64_t, size_t> index;
	size_t start = 0;
	while (start < jsonl.size()) {
		size_t end = jsonl.find('\n', start);
		if (end == std::string::npos) {
			end = jsonl.size();
		}
		std::string line = jsonl.substr(start, end - start);
		start = end + 1;
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (line.empty()) {
			continue;
		}
		Json item = Json::parse(line, nullptr, false);
		if (item.is_discarded() || !item.is_object()) {
			continue;
		}
		const auto id = item.find("id");
		if (id == item.end() || !id->is_number_integer()) {
			continue;
		}
		const int64_t key = id->get<int64_t>();
		if (item.contains("ferr")) {
			if (index.emplace(key, strokes.size()).second) {
				strokes.push_back(std::move(item));
			}
			continue;
		}
		const auto cleared = item.find(kAnnotationsJournalUpdateKey);
		const auto found = index.find(key);
		if (cleared != item.end() && found != index.end()) {
			strokes[found->second][kAnnotationsJournalUpdateKey] = *cleared;
		}
	}
	std::string out;
	for (const Json &stroke : strokes) {
		out += stroke.dump(-1, ' ', false, Json::error_handler_t::replace);
		out += '\n';
	}
	return out;
}

} // namespace voltz
