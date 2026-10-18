#include <doctest.h>

#include <voltz/core/annotation-font.hpp>
#include <voltz/core/annotation-raster.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using voltz::AnnotationTool;
using voltz::PixelRect;
using voltz::RgbColor;
using voltz::Stroke;
using voltz::StrokePoint;
using voltz::TextFont;

namespace {

const RgbColor kLime{0xD4, 0xF5, 0x3C};
const RgbColor kBlue{0x4D, 0xA3, 0xFF};

struct Image {
	Image(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0) {}
	voltz::RasterImage view() { return voltz::RasterImage{pixels.data(), width, height, width * 4}; }
	uint8_t alpha(int x, int y) const
	{
		return pixels[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4 + 3];
	}
	int inked() const
	{
		int count = 0;
		for (size_t i = 3; i < pixels.size(); i += 4) {
			count += pixels[i] != 0 ? 1 : 0;
		}
		return count;
	}
	int width;
	int height;
	std::vector<uint8_t> pixels;
};

Stroke makeText(int id, const std::string &text, StrokePoint anchor, int size, TextFont font = TextFont::Bold,
		RgbColor color = kLime)
{
	Stroke stroke;
	stroke.id = id;
	stroke.tool = AnnotationTool::Text;
	stroke.color = color;
	stroke.points = {anchor};
	stroke.text = text;
	stroke.textSize = size;
	stroke.textFont = font;
	return stroke;
}

void redrawAll(Image &image, const std::vector<const Stroke *> &strokes, double scale)
{
	voltz::clearRaster(image.view());
	for (const Stroke *stroke : strokes) {
		voltz::rasterizeStroke(image.view(), *stroke, scale, scale);
	}
}

} // namespace

TEST_CASE("fontes embutidas: as duas abrem; medidas da DM Sans")
{
	for (const TextFont font : {TextFont::Regular, TextFont::Bold}) {
		REQUIRE(voltz::embeddedFontAvailable(font));
		const voltz::GlyphMetrics &metrics = voltz::embeddedFontMetrics(font);
		CHECK(metrics.unitsPerEm() == 1000);
		CHECK(metrics.ascender() > 700);
		CHECK(metrics.descender() < -150);
		CHECK(metrics.lineHeight() >= metrics.ascender() - metrics.descender());
		CHECK(metrics.advance(U'W') > metrics.advance(U'i'));
		CHECK(metrics.advance(U'á') == metrics.advance(U'a')); // "á" existe e avança como "a"
	}
	CHECK(voltz::embeddedFontMetrics(TextFont::Bold).advance(U'a') >
	      voltz::embeddedFontMetrics(TextFont::Regular).advance(U'a'));
}

TEST_CASE("renderGlyph: 'A' tem tinta acima da linha de base; espaço não desenha; a fração desloca")
{
	voltz::GlyphBitmap glyph;
	REQUIRE(voltz::renderGlyph(TextFont::Bold, U'A', 40.0, 100.0, 200.0, glyph));
	CHECK(glyph.width > 10);
	CHECK(glyph.height > 20);
	CHECK(glyph.top >= 200 - 40);
	CHECK(glyph.top + glyph.height <= 201);
	CHECK(glyph.left >= 99);
	int full = 0;
	for (const uint8_t value : glyph.coverage) {
		full += value == 255 ? 1 : 0;
	}
	CHECK(full > 50);
	CHECK_FALSE(voltz::renderGlyph(TextFont::Bold, U' ', 40.0, 100.0, 200.0, glyph));
	voltz::GlyphBitmap whole;
	voltz::GlyphBitmap half;
	REQUIRE(voltz::renderGlyph(TextFont::Regular, U'l', 40.0, 10.0, 50.0, whole));
	REQUIRE(voltz::renderGlyph(TextFont::Regular, U'l', 40.0, 10.5, 50.0, half));
	CHECK(whole.coverage != half.coverage); // meio pixel para a direita muda a suavização
	CHECK_FALSE(voltz::renderGlyph(TextFont::Bold, U'A', 0.0, 0.0, 0.0, glyph));
}

TEST_CASE("texto: strokeBounds cobre todo pixel desenhado (escala 1 e 0,25); texto vazio não desenha")
{
	const Stroke text = makeText(1, "Ola\xC3\xA1 gj\nQ\xC3\x87y", {40, 30}, 64);
	for (const double scale : {1.0, 0.25}) {
		Image image(800, 400);
		voltz::rasterizeStroke(image.view(), text, scale, scale);
		const PixelRect bounds = voltz::strokeBounds(text, scale, scale);
		REQUIRE_FALSE(bounds.empty());
		int outside = 0;
		for (int y = 0; y < image.height; y++) {
			for (int x = 0; x < image.width; x++) {
				if (image.alpha(x, y) != 0 &&
				    !(x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)) {
					outside++;
				}
			}
		}
		CHECK(outside == 0);
		CHECK(image.inked() > (scale == 1.0 ? 2000 : 100));
	}
	const Stroke empty = makeText(2, "", {10, 10}, 48);
	CHECK(voltz::strokeBounds(empty, 1.0, 1.0).empty());
	Image blank(100, 100);
	voltz::rasterizeStroke(blank.view(), empty, 1.0, 1.0);
	CHECK(blank.inked() == 0);
}

TEST_CASE("texto: rasterizeStrokeClipped e redrawRegion iguais ao desenho inteiro (byte a byte)")
{
	const Stroke text = makeText(1, "Voltz\nStudio", {100, 80}, 72);
	Stroke pen;
	pen.id = 2;
	pen.tool = AnnotationTool::Pen;
	pen.color = kBlue;
	pen.width = 8;
	voltz::appendStrokePoints(pen, {{60, 60}, {400, 300}, {420, 90}});
	const std::vector<const Stroke *> strokes{&text, &pen};

	Image whole(640, 360);
	redrawAll(whole, strokes, 1.0);
	REQUIRE(whole.inked() > 0);

	/* recorte: fora dele intacto, dentro igual */
	Image clipped(640, 360);
	const PixelRect clip{150, 100, 300, 200};
	voltz::rasterizeStrokeClipped(clipped.view(), text, 1.0, 1.0, clip);
	Image textOnly(640, 360);
	voltz::rasterizeStroke(textOnly.view(), text, 1.0, 1.0);
	for (int y = 0; y < 360; y++) {
		for (int x = 0; x < 640; x++) {
			const bool in = x >= clip.left && x < clip.right && y >= clip.top && y < clip.bottom;
			if (!(in ? clipped.alpha(x, y) == textOnly.alpha(x, y) : clipped.alpha(x, y) == 0)) {
				FAIL("recorte diferente em (" << x << "," << y << ")");
			}
		}
	}

	/* o texto mudou (digitação): redesenhar a região antiga ∪ nova = redesenhar tudo */
	Stroke typed = text;
	typed.text = "Voltz!\nStudio 4D";
	Image partial = whole;
	const PixelRect region =
		voltz::uniteRects(voltz::strokeBounds(text, 1.0, 1.0), voltz::strokeBounds(typed, 1.0, 1.0));
	voltz::redrawRegion(partial.view(), {&typed, &pen}, 1.0, 1.0, region);
	Image expected(640, 360);
	redrawAll(expected, {&typed, &pen}, 1.0);
	CHECK(partial.pixels == expected.pixels);
}

TEST_CASE("texto: miolo da letra opaco na cor; negrito tem mais tinta que regular")
{
	Image regular(400, 120);
	Image bold(400, 120);
	voltz::rasterizeStroke(regular.view(), makeText(1, "HHHH", {10, 10}, 80, TextFont::Regular), 1.0, 1.0);
	voltz::rasterizeStroke(bold.view(), makeText(1, "HHHH", {10, 10}, 80, TextFont::Bold), 1.0, 1.0);
	CHECK(bold.inked() > regular.inked());
	int opaque = 0;
	int wrongColor = 0;
	for (size_t i = 0; i < bold.pixels.size(); i += 4) {
		if (bold.pixels[i + 3] == 255) {
			opaque++;
			wrongColor += (bold.pixels[i] != kLime.b || bold.pixels[i + 1] != kLime.g ||
				       bold.pixels[i + 2] != kLime.r)
					      ? 1
					      : 0;
		}
	}
	CHECK(opaque > 500);
	CHECK(wrongColor == 0);
}

TEST_CASE("textCaretStroke: no fim da última linha, na cor do texto; vazio na âncora")
{
	const Stroke text = makeText(5, "ab\ncd", {100, 50}, 48, TextFont::Bold, kBlue);
	const Stroke caret = voltz::textCaretStroke(text);
	CHECK(caret.tool == AnnotationTool::Pen);
	CHECK(caret.color == kBlue);
	CHECK(caret.width == 3);
	REQUIRE(caret.points.size() == 2);
	CHECK(caret.points[0].x == caret.points[1].x);
	CHECK(caret.points[0].x > 100 + 30); // depois de "cd"
	CHECK(caret.points[0].y > 50 + 48);  // segunda linha
	CHECK(caret.points[1].y > caret.points[0].y + 30);

	const Stroke empty = voltz::textCaretStroke(makeText(6, "", {10, 20}, 48));
	REQUIRE(empty.points.size() == 2);
	CHECK(empty.points[0].x <= 12);
	CHECK(empty.points[0].y == 20);
	Stroke pen;
	CHECK(voltz::textCaretStroke(pen).points.empty());
}
