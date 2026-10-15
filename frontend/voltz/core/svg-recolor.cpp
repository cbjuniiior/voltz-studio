#include "svg-recolor.hpp"

namespace voltz {

std::string recolorSvg(const std::string &svg, const std::string &color)
{
	static const std::string token = "currentColor";
	std::string out;
	out.reserve(svg.size());
	size_t pos = 0;
	while (true) {
		const size_t found = svg.find(token, pos);
		if (found == std::string::npos) {
			out.append(svg, pos, std::string::npos);
			break;
		}
		out.append(svg, pos, found - pos);
		out.append(color);
		pos = found + token.size();
	}
	return out;
}

} // namespace voltz
