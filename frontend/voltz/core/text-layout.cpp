#include "text-layout.hpp"
#include "annotation-text.hpp"

#include <algorithm>

namespace voltz {

TextLayout layoutText(const std::string &text, double pixelSize, const GlyphMetrics &metrics)
{
	TextLayout layout;
	const int units = metrics.unitsPerEm();
	if (units <= 0 || pixelSize <= 0.0) {
		return layout;
	}
	const double scale = pixelSize / static_cast<double>(units);
	const double ascender = static_cast<double>(metrics.ascender()) * scale;
	const double descender = static_cast<double>(metrics.descender()) * scale;
	layout.pixelSize = pixelSize;
	layout.lineAdvance = static_cast<double>(metrics.lineHeight()) * scale;
	double x = 0.0;
	double baseline = ascender;
	for (const char32_t codepoint : decodeUtf8(text)) {
		if (codepoint == U'\n') {
			x = 0.0;
			baseline += layout.lineAdvance;
			layout.lines++;
			continue;
		}
		layout.glyphs.push_back(PlacedGlyph{codepoint, x, baseline});
		x += static_cast<double>(metrics.advance(codepoint)) * scale;
		layout.width = std::max(layout.width, x);
	}
	layout.height = baseline - descender;
	layout.caretX = x;
	layout.caretTop = baseline - ascender;
	layout.caretBottom = baseline - descender;
	return layout;
}

} // namespace voltz
