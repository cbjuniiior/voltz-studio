#include "mouse-style.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace voltz {

namespace {

using Json = nlohmann::ordered_json;

int hexValue(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

const Json *child(const Json &object, const char *key)
{
	const auto it = object.find(key);
	return it != object.end() && it->is_object() ? &*it : nullptr;
}

void readBool(const Json &object, const char *key, bool &out)
{
	const auto it = object.find(key);
	if (it != object.end() && it->is_boolean()) {
		out = it->get<bool>();
	}
}

void readInt(const Json &object, const char *key, int &out)
{
	const auto it = object.find(key);
	if (it == object.end() || !it->is_number()) {
		return;
	}
	const double value = it->get<double>();
	/* fora do intervalo do int: vira um extremo e o clamp resolve */
	if (value > 1e9) {
		out = 1000000000;
	} else if (value < -1e9) {
		out = -1000000000;
	} else {
		out = static_cast<int>(std::lround(value));
	}
}

void readDouble(const Json &object, const char *key, double &out)
{
	const auto it = object.find(key);
	if (it != object.end() && it->is_number()) {
		out = it->get<double>();
	}
}

void readColor(const Json &object, const char *key, RgbColor &out)
{
	const auto it = object.find(key);
	if (it != object.end() && it->is_string()) {
		parseHexColor(it->get<std::string>(), out);
	}
}

// Os controles usam porcentagem inteira: 0,25 continua 0.25 no JSON.
double round2(double value)
{
	return std::round(value * 100.0) / 100.0;
}

std::string dump(const Json &json)
{
	return json.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string trimmed(const std::string &text)
{
	const size_t first = text.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return std::string();
	}
	const size_t last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}

Json styleJson(const MouseStyle &style)
{
	Json halo = Json::object();
	halo["ativo"] = style.halo.active;
	halo["cor"] = formatHexColor(style.halo.color);
	halo["tamanho"] = style.halo.size;
	halo["opacidade"] = round2(style.halo.opacity);
	halo["forma"] = style.halo.shape == HaloShape::Ring ? "anel" : "cheio";

	Json click = Json::object();
	click["ativo"] = style.click.active;
	click["cor"] = formatHexColor(style.click.color);
	click["cor_direito"] = formatHexColor(style.click.rightColor);

	Json spotlight = Json::object();
	spotlight["ativo"] = style.spotlight.active;
	spotlight["raio"] = style.spotlight.radius;
	spotlight["escurecer"] = round2(style.spotlight.dim);

	Json root = Json::object();
	root["halo"] = std::move(halo);
	root["clique"] = std::move(click);
	root["holofote"] = std::move(spotlight);
	return root;
}

MouseStyle styleFromObject(const Json &root)
{
	MouseStyle style;
	if (const Json *halo = child(root, "halo")) {
		readBool(*halo, "ativo", style.halo.active);
		readColor(*halo, "cor", style.halo.color);
		readInt(*halo, "tamanho", style.halo.size);
		readDouble(*halo, "opacidade", style.halo.opacity);
		const auto shape = halo->find("forma");
		if (shape != halo->end() && shape->is_string() && shape->get<std::string>() == "anel") {
			style.halo.shape = HaloShape::Ring;
		}
	}
	if (const Json *click = child(root, "clique")) {
		readBool(*click, "ativo", style.click.active);
		readColor(*click, "cor", style.click.color);
		readColor(*click, "cor_direito", style.click.rightColor);
	}
	if (const Json *spotlight = child(root, "holofote")) {
		readBool(*spotlight, "ativo", style.spotlight.active);
		readInt(*spotlight, "raio", style.spotlight.radius);
		readDouble(*spotlight, "escurecer", style.spotlight.dim);
	}
	return clampMouseStyle(style);
}

} // namespace

bool parseHexColor(const std::string &text, RgbColor &out)
{
	if (text.size() != 7 || text[0] != '#') {
		return false;
	}
	int digits[6];
	for (size_t i = 0; i < 6; i++) {
		digits[i] = hexValue(text[i + 1]);
		if (digits[i] < 0) {
			return false;
		}
	}
	out.r = static_cast<uint8_t>(digits[0] * 16 + digits[1]);
	out.g = static_cast<uint8_t>(digits[2] * 16 + digits[3]);
	out.b = static_cast<uint8_t>(digits[4] * 16 + digits[5]);
	return true;
}

std::string formatHexColor(const RgbColor &color)
{
	char text[8];
	std::snprintf(text, sizeof(text), "#%02X%02X%02X", static_cast<unsigned>(color.r),
		      static_cast<unsigned>(color.g), static_cast<unsigned>(color.b));
	return text;
}

MouseStyle clampMouseStyle(MouseStyle style)
{
	style.halo.size = std::clamp(style.halo.size, kHaloSizeMin, kHaloSizeMax);
	style.halo.opacity = std::clamp(style.halo.opacity, kHaloOpacityMin, kHaloOpacityMax);
	style.spotlight.radius = std::clamp(style.spotlight.radius, kSpotlightRadiusMin, kSpotlightRadiusMax);
	style.spotlight.dim = std::clamp(style.spotlight.dim, kSpotlightDimMin, kSpotlightDimMax);
	return style;
}

std::string mouseStyleToJson(const MouseStyle &style)
{
	return dump(styleJson(style));
}

bool mouseStyleFromJson(const std::string &json, MouseStyle &out)
{
	const Json root = Json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		out = MouseStyle{};
		return false;
	}
	out = styleFromObject(root);
	return true;
}

std::string savedStylesToJson(const std::vector<NamedMouseStyle> &styles)
{
	Json list = Json::array();
	for (const NamedMouseStyle &item : styles) {
		Json entry = Json::object();
		entry["nome"] = item.name;
		entry["estilo"] = styleJson(item.style);
		list.push_back(std::move(entry));
	}
	return dump(list);
}

std::vector<NamedMouseStyle> savedStylesFromJson(const std::string &json)
{
	std::vector<NamedMouseStyle> result;
	const Json root = Json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_array()) {
		return result;
	}
	for (const Json &entry : root) {
		if (!entry.is_object()) {
			continue;
		}
		const auto name = entry.find("nome");
		if (name == entry.end() || !name->is_string()) {
			continue;
		}
		NamedMouseStyle item;
		item.name = trimmed(name->get<std::string>());
		if (item.name.empty()) {
			continue;
		}
		if (const Json *style = child(entry, "estilo")) {
			item.style = styleFromObject(*style);
		}
		result.push_back(std::move(item));
		if (result.size() == kMaxSavedStyles) {
			break;
		}
	}
	return result;
}

std::vector<NamedMouseStyle> upsertSavedStyle(std::vector<NamedMouseStyle> styles, const std::string &name,
					      const MouseStyle &style)
{
	const std::string clean = trimmed(name);
	if (clean.empty()) {
		return styles;
	}
	for (NamedMouseStyle &item : styles) {
		if (item.name == clean) {
			item.style = clampMouseStyle(style);
			return styles;
		}
	}
	styles.push_back(NamedMouseStyle{clean, clampMouseStyle(style)});
	while (styles.size() > kMaxSavedStyles) {
		styles.erase(styles.begin());
	}
	return styles;
}

std::vector<NamedMouseStyle> removeSavedStyle(std::vector<NamedMouseStyle> styles, const std::string &name)
{
	const std::string clean = trimmed(name);
	styles.erase(std::remove_if(styles.begin(), styles.end(),
				    [&clean](const NamedMouseStyle &item) { return item.name == clean; }),
		     styles.end());
	return styles;
}

} // namespace voltz
