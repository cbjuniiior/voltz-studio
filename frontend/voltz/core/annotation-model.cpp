#include "annotation-model.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace voltz {

namespace {

using Json = nlohmann::ordered_json;

struct ToolName {
	AnnotationTool tool;
	const char *name;
};

constexpr ToolName kToolNames[] = {
	{AnnotationTool::Pen, "caneta"},          {AnnotationTool::Highlighter, "marca_texto"},
	{AnnotationTool::Line, "linha"},          {AnnotationTool::Arrow, "seta"},
	{AnnotationTool::Rectangle, "retangulo"}, {AnnotationTool::Ellipse, "elipse"},
};

int toIntClamped(int64_t value)
{
	return static_cast<int>(std::clamp<int64_t>(value, -1000000000LL, 1000000000LL));
}

int64_t roundedNumber(const Json &value)
{
	if (value.is_number_integer()) {
		return value.get<int64_t>();
	}
	const double number = value.get<double>();
	if (number > 1e15) {
		return 1000000000000000LL;
	}
	if (number < -1e15) {
		return -1000000000000000LL;
	}
	return static_cast<int64_t>(std::llround(number));
}

// Número JSON em `key` → int64 (arredondado); falso se ausente ou não numérico.
bool readInt64(const Json &object, const char *key, int64_t &out)
{
	const auto it = object.find(key);
	if (it == object.end() || !it->is_number()) {
		return false;
	}
	out = roundedNumber(*it);
	return true;
}

Json pointsArray(const std::vector<StrokePoint> &points)
{
	Json array = Json::array();
	for (const StrokePoint &point : points) {
		array.push_back(Json::array({point.x, point.y}));
	}
	return array;
}

void readPoints(const Json &array, std::vector<StrokePoint> &out)
{
	for (const Json &item : array) {
		if (out.size() >= kMaxStrokePoints) {
			break;
		}
		if (!item.is_array() || item.size() < 2 || !item[0].is_number() || !item[1].is_number()) {
			continue;
		}
		out.push_back(StrokePoint{toIntClamped(roundedNumber(item[0])), toIntClamped(roundedNumber(item[1]))});
	}
}

} // namespace

const char *toolToString(AnnotationTool tool)
{
	for (const ToolName &entry : kToolNames) {
		if (entry.tool == tool) {
			return entry.name;
		}
	}
	return "caneta";
}

bool toolFromString(const std::string &text, AnnotationTool &out)
{
	for (const ToolName &entry : kToolNames) {
		if (text == entry.name) {
			out = entry.tool;
			return true;
		}
	}
	return false;
}

bool isShapeTool(AnnotationTool tool)
{
	return tool != AnnotationTool::Pen && tool != AnnotationTool::Highlighter;
}

int clampStrokeWidth(int width)
{
	return std::clamp(width, kStrokeWidthMin, kStrokeWidthMax);
}

void appendStrokePoints(Stroke &stroke, const std::vector<StrokePoint> &points)
{
	if (points.empty()) {
		return;
	}
	if (isShapeTool(stroke.tool)) {
		if (stroke.points.empty()) {
			stroke.points.push_back(points.front());
			if (points.size() == 1) {
				return;
			}
		}
		stroke.points.resize(1);
		stroke.points.push_back(points.back());
		return;
	}
	for (const StrokePoint &point : points) {
		if (stroke.points.size() >= kMaxStrokePoints) {
			break;
		}
		if (!stroke.points.empty() && stroke.points.back() == point) {
			continue;
		}
		stroke.points.push_back(point);
	}
}

std::string pointsToJson(const std::vector<StrokePoint> &points)
{
	return pointsArray(points).dump();
}

bool pointsFromJson(const std::string &json, std::vector<StrokePoint> &out)
{
	const Json array = Json::parse(json, nullptr, false);
	if (array.is_discarded() || !array.is_array()) {
		return false;
	}
	out.clear();
	readPoints(array, out);
	return true;
}

std::string strokeToJsonLine(const Stroke &stroke)
{
	Json json = Json::object();
	json["id"] = stroke.id;
	json["ferr"] = toolToString(stroke.tool);
	json["cor"] = formatHexColor(stroke.color);
	json["esp"] = stroke.width;
	json["pts"] = pointsArray(stroke.points);
	json["t_ini"] = stroke.startMs;
	if (stroke.endMs >= 0) {
		json["t_fim"] = stroke.endMs;
	}
	if (stroke.clearedMs >= 0) {
		json["t_limpo"] = stroke.clearedMs;
	}
	return json.dump(-1, ' ', false, Json::error_handler_t::replace);
}

bool strokeFromJson(const std::string &json, Stroke &out)
{
	const Json root = Json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		return false;
	}
	const auto idIt = root.find("id");
	if (idIt == root.end() || !idIt->is_number_integer()) {
		return false;
	}
	const auto toolIt = root.find("ferr");
	AnnotationTool tool = AnnotationTool::Pen;
	if (toolIt == root.end() || !toolIt->is_string() || !toolFromString(toolIt->get<std::string>(), tool)) {
		return false;
	}
	Stroke stroke;
	stroke.id = toIntClamped(idIt->get<int64_t>());
	stroke.tool = tool;
	const auto colorIt = root.find("cor");
	if (colorIt != root.end() && colorIt->is_string()) {
		parseHexColor(colorIt->get<std::string>(), stroke.color);
	}
	int64_t value = 0;
	if (readInt64(root, "esp", value)) {
		stroke.width = clampStrokeWidth(toIntClamped(value));
	}
	const auto pointsIt = root.find("pts");
	if (pointsIt != root.end() && pointsIt->is_array()) {
		readPoints(*pointsIt, stroke.points);
	}
	if (readInt64(root, "t_ini", value)) {
		stroke.startMs = value;
	}
	if (readInt64(root, "t_fim", value)) {
		stroke.endMs = value;
	}
	if (readInt64(root, "t_limpo", value)) {
		stroke.clearedMs = value;
	}
	out = std::move(stroke);
	return true;
}

AnnotationStyle clampAnnotationStyle(AnnotationStyle style)
{
	style.width = clampStrokeWidth(style.width);
	style.fadeSeconds = std::clamp(style.fadeSeconds, 0, kFadeSecondsMax);
	return style;
}

std::string annotationStyleToJson(const AnnotationStyle &style)
{
	Json json = Json::object();
	json["ferr"] = toolToString(style.tool);
	json["cor"] = formatHexColor(style.color);
	json["esp"] = style.width;
	json["somem_apos_s"] = style.fadeSeconds;
	return json.dump();
}

bool annotationStyleFromJson(const std::string &json, AnnotationStyle &out)
{
	out = AnnotationStyle{};
	const Json root = Json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		return false;
	}
	const auto toolIt = root.find("ferr");
	if (toolIt != root.end() && toolIt->is_string()) {
		toolFromString(toolIt->get<std::string>(), out.tool);
	}
	const auto colorIt = root.find("cor");
	if (colorIt != root.end() && colorIt->is_string()) {
		parseHexColor(colorIt->get<std::string>(), out.color);
	}
	int64_t value = 0;
	if (readInt64(root, "esp", value)) {
		out.width = toIntClamped(value);
	}
	if (readInt64(root, "somem_apos_s", value)) {
		out.fadeSeconds = toIntClamped(value);
	}
	out = clampAnnotationStyle(out);
	return true;
}

} // namespace voltz
