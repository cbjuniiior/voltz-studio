#include "project-markers.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace voltz {

namespace {
using Json = nlohmann::ordered_json;
} // namespace

std::string serializeMarkers(const std::vector<Marker> &markers)
{
	Json list = Json::array();
	for (const Marker &marker : markers) {
		Json item = Json::object();
		item["t_ms"] = marker.tMs;
		item["nome"] = marker.name;
		list.push_back(std::move(item));
	}
	return list.dump(2, ' ', false, Json::error_handler_t::replace) + "\n";
}

bool parseMarkers(const std::string &text, std::vector<Marker> &out)
{
	out.clear();
	const Json list = Json::parse(text, nullptr, false);
	if (list.is_discarded() || !list.is_array()) {
		return false;
	}
	for (const Json &item : list) {
		if (!item.is_object()) {
			continue;
		}
		const auto time = item.find("t_ms");
		const auto name = item.find("nome");
		if (time == item.end() || !time->is_number() || name == item.end() || !name->is_string()) {
			continue;
		}
		out.push_back(Marker{time->get<int64_t>(), name->get<std::string>()});
	}
	return true;
}

} // namespace voltz
