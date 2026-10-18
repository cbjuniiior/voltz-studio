#include <doctest.h>

#include <voltz/core/text-layout.hpp>

#include <string>

using doctest::Approx;
using voltz::TextLayout;

namespace {

// Fonte falsa: em de 1000 unidades, ascender 800, descender -200, linhas a cada 1200; "W" 900, espaço 250, o resto 500.
class FakeMetrics : public voltz::GlyphMetrics {
public:
	int unitsPerEm() const override { return 1000; }
	int ascender() const override { return 800; }
	int descender() const override { return -200; }
	int lineHeight() const override { return 1200; }
	int advance(char32_t codepoint) const override
	{
		if (codepoint == U'W') {
			return 900;
		}
		return codepoint == U' ' ? 250 : 500;
	}
};

const FakeMetrics kMetrics;

} // namespace

TEST_CASE("layoutText: uma linha — origens, largura, altura e cursor")
{
	const TextLayout layout = voltz::layoutText("AW b", 10.0, kMetrics); // escala 0,01 px por unidade
	REQUIRE(layout.glyphs.size() == 4);
	CHECK(layout.pixelSize == Approx(10.0));
	CHECK(layout.glyphs[0].x == Approx(0.0));
	CHECK(layout.glyphs[1].x == Approx(5.0));
	CHECK(layout.glyphs[2].x == Approx(14.0));
	CHECK(layout.glyphs[3].x == Approx(16.5));
	CHECK(layout.glyphs[3].codepoint == U'b');
	for (const voltz::PlacedGlyph &glyph : layout.glyphs) {
		CHECK(glyph.baseline == Approx(8.0));
	}
	CHECK(layout.lines == 1);
	CHECK(layout.width == Approx(21.5));
	CHECK(layout.height == Approx(10.0));
	CHECK(layout.lineAdvance == Approx(12.0));
	CHECK(layout.caretX == Approx(21.5));
	CHECK(layout.caretTop == Approx(0.0));
	CHECK(layout.caretBottom == Approx(10.0));
}

TEST_CASE("layoutText: várias linhas, linha vazia e '\\n' no fim (cursor na linha nova)")
{
	const TextLayout layout = voltz::layoutText("ab\n\nW\n", 100.0, kMetrics);
	REQUIRE(layout.glyphs.size() == 3);
	CHECK(layout.lines == 4);
	CHECK(layout.glyphs[1].x == Approx(50.0));
	CHECK(layout.glyphs[1].baseline == Approx(80.0));
	CHECK(layout.glyphs[2].x == Approx(0.0));
	CHECK(layout.glyphs[2].baseline == Approx(80.0 + 2 * 120.0));
	CHECK(layout.width == Approx(100.0)); // "ab" = 50 + 50; "W" = 90
	CHECK(layout.height == Approx(3 * 120.0 + 100.0));
	CHECK(layout.caretX == Approx(0.0));
	CHECK(layout.caretTop == Approx(3 * 120.0));
	CHECK(layout.caretBottom == Approx(3 * 120.0 + 100.0));
}

TEST_CASE("layoutText: tudo proporcional ao tamanho")
{
	const TextLayout small = voltz::layoutText("Wa\nb", 24.0, kMetrics);
	const TextLayout big = voltz::layoutText("Wa\nb", 48.0, kMetrics);
	REQUIRE(small.glyphs.size() == big.glyphs.size());
	for (size_t i = 0; i < small.glyphs.size(); i++) {
		CHECK(big.glyphs[i].x == Approx(2.0 * small.glyphs[i].x));
		CHECK(big.glyphs[i].baseline == Approx(2.0 * small.glyphs[i].baseline));
	}
	CHECK(big.width == Approx(2.0 * small.width));
	CHECK(big.height == Approx(2.0 * small.height));
	CHECK(big.caretX == Approx(2.0 * small.caretX));
}

TEST_CASE("layoutText: texto vazio é uma caixa de uma linha com o cursor na âncora; tamanho inválido = vazio")
{
	const TextLayout empty = voltz::layoutText("", 50.0, kMetrics);
	CHECK(empty.glyphs.empty());
	CHECK(empty.lines == 1);
	CHECK(empty.width == Approx(0.0));
	CHECK(empty.height == Approx(50.0));
	CHECK(empty.caretX == Approx(0.0));
	CHECK(empty.caretTop == Approx(0.0));
	CHECK(empty.caretBottom == Approx(50.0));
	const TextLayout invalid = voltz::layoutText("abc", 0.0, kMetrics);
	CHECK(invalid.glyphs.empty());
	CHECK(invalid.pixelSize == Approx(0.0));
}

TEST_CASE("layoutText: UTF-8 — acento é um caractere; byte inválido vira U+FFFD")
{
	const TextLayout layout = voltz::layoutText("\xC3\xA1\xFF", 10.0, kMetrics); // "á" + byte inválido
	REQUIRE(layout.glyphs.size() == 2);
	CHECK(layout.glyphs[0].codepoint == U'á');
	CHECK(layout.glyphs[1].codepoint == U'�');
	CHECK(layout.glyphs[1].x == Approx(5.0));
	CHECK(layout.width == Approx(10.0));
}
