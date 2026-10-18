#include <doctest.h>

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/annotation-text.hpp>
#include <voltz/core/stroke-store.hpp>

#include <algorithm>
#include <string>
#include <vector>

using voltz::AnnotationTool;
using voltz::RgbColor;
using voltz::Stroke;
using voltz::StrokePoint;
using voltz::TextFont;

namespace {

// "Olá" em UTF-8 escrito byte a byte (o arquivo de teste não depende da codificação do compilador).
const std::string kOla = "Ol\xC3\xA1";

} // namespace

TEST_CASE("texto: ferramenta e fonte (nomes, ida e volta); texto não é forma")
{
	AnnotationTool tool = AnnotationTool::Pen;
	CHECK(std::string(voltz::toolToString(AnnotationTool::Text)) == "texto");
	CHECK(voltz::toolFromString("texto", tool));
	CHECK(tool == AnnotationTool::Text);
	CHECK_FALSE(voltz::isShapeTool(AnnotationTool::Text));
	CHECK(voltz::isShapeTool(AnnotationTool::Line));
	CHECK(std::string(voltz::textFontToString(TextFont::Regular)) == "dm_sans");
	CHECK(std::string(voltz::textFontToString(TextFont::Bold)) == "dm_sans_negrito");
	TextFont font = TextFont::Regular;
	CHECK(voltz::textFontFromString("dm_sans_negrito", font));
	CHECK(font == TextFont::Bold);
	CHECK_FALSE(voltz::textFontFromString("arial", font));
	CHECK(font == TextFont::Bold);
	CHECK(voltz::clampTextSize(1) == voltz::kTextSizeMin);
	CHECK(voltz::clampTextSize(999) == voltz::kTextSizeMax);
}

TEST_CASE("strokeToJsonLine/strokeFromJson: texto com tam e fonte; campos de texto só no texto")
{
	Stroke stroke;
	stroke.id = 9;
	stroke.tool = AnnotationTool::Text;
	stroke.points = {{100, 200}};
	stroke.text = kOla + "\nmundo";
	stroke.textSize = 36;
	stroke.textFont = TextFont::Regular;
	stroke.startMs = 10;
	stroke.endMs = 20;
	const std::string line = voltz::strokeToJsonLine(stroke);
	CHECK(line == "{\"id\":9,\"ferr\":\"texto\",\"cor\":\"#D4F53C\",\"esp\":4,\"pts\":[[100,200]],\"texto\":\"" +
			      kOla + "\\nmundo\",\"tam\":36,\"fonte\":\"dm_sans\",\"t_ini\":10,\"t_fim\":20}");
	Stroke parsed;
	REQUIRE(voltz::strokeFromJson(line, parsed));
	CHECK(parsed.tool == AnnotationTool::Text);
	CHECK(parsed.text == stroke.text);
	CHECK(parsed.textSize == 36);
	CHECK(parsed.textFont == TextFont::Regular);
	REQUIRE(parsed.points.size() == 1);
	CHECK(parsed.points[0] == StrokePoint{100, 200});

	/* tolerância: vários pontos = só a âncora; tamanho limitado; fonte desconhecida = negrito; controles saem */
	REQUIRE(voltz::strokeFromJson(
		R"({"id":1,"ferr":"texto","pts":[[1,2],[3,4]],"texto":"a\u0007b\r\nc","tam":5000,"fonte":"x"})",
		parsed));
	CHECK(parsed.points.size() == 1);
	CHECK(parsed.text == "ab\nc");
	CHECK(parsed.textSize == voltz::kTextSizeMax);
	CHECK(parsed.textFont == TextFont::Bold);

	/* noutra ferramenta, "texto"/"tam"/"fonte" não entram nem saem */
	REQUIRE(voltz::strokeFromJson(R"({"id":2,"ferr":"linha","pts":[[0,0],[5,5]],"texto":"x","tam":30})", parsed));
	CHECK(parsed.text.empty());
	CHECK(parsed.textSize == voltz::kTextSizeDefault);
	CHECK(voltz::strokeToJsonLine(parsed).find("texto") == std::string::npos);
}

TEST_CASE("appendStrokePoints: texto guarda só a âncora")
{
	Stroke text;
	text.tool = AnnotationTool::Text;
	voltz::appendStrokePoints(text, {{5, 6}, {7, 8}});
	voltz::appendStrokePoints(text, {{9, 9}});
	REQUIRE(text.points.size() == 1);
	CHECK(text.points[0] == StrokePoint{5, 6});
}

TEST_CASE("AnnotationStyle: tamanho e fonte do texto no JSON, com limites")
{
	voltz::AnnotationStyle style;
	style.tool = AnnotationTool::Text;
	style.textSize = 80;
	style.textFont = TextFont::Regular;
	CHECK(voltz::annotationStyleToJson(style) ==
	      R"({"ferr":"texto","cor":"#D4F53C","esp":4,"somem_apos_s":0,"tam":80,"fonte":"dm_sans"})");
	voltz::AnnotationStyle parsed;
	REQUIRE(voltz::annotationStyleFromJson(voltz::annotationStyleToJson(style), parsed));
	CHECK(parsed.tool == AnnotationTool::Text);
	CHECK(parsed.textSize == 80);
	CHECK(parsed.textFont == TextFont::Regular);
	REQUIRE(voltz::annotationStyleFromJson(R"({"tam":2,"fonte":"comic"})", parsed));
	CHECK(parsed.textSize == voltz::kTextSizeMin);
	CHECK(parsed.textFont == TextFont::Bold);
	/* estilo de antes do 4D (sem tam/fonte): padrão */
	REQUIRE(voltz::annotationStyleFromJson(R"({"ferr":"seta","esp":8})", parsed));
	CHECK(parsed.textSize == voltz::kTextSizeDefault);
	CHECK(parsed.textFont == TextFont::Bold);
}

TEST_CASE("UTF-8: ida e volta; inválidos viram U+FFFD")
{
	const std::u32string codes = voltz::decodeUtf8(kOla + "\xF0\x9F\x98\x80"); // "Olá😀"
	REQUIRE(codes.size() == 4);
	CHECK(codes[2] == U'\u00E1');
	CHECK(codes[3] == U'\U0001F600');
	CHECK(voltz::encodeUtf8(codes) == kOla + "\xF0\x9F\x98\x80");
	CHECK(voltz::decodeUtf8("\x80") == std::u32string(1, voltz::kReplacementChar));         // continuação solta
	CHECK(voltz::decodeUtf8("\xC3") == std::u32string(1, voltz::kReplacementChar));         // curta
	CHECK(voltz::decodeUtf8("\xC0\xAF") == std::u32string(1, voltz::kReplacementChar));     // longa demais
	CHECK(voltz::decodeUtf8("\xED\xA0\x80") == std::u32string(1, voltz::kReplacementChar)); // substituto
	CHECK(voltz::decodeUtf8("\xC3"
				"a") == std::u32string{voltz::kReplacementChar, U'a'}); // curta, segue
	CHECK(voltz::encodeUtf8(std::u32string(1, char32_t(0xD800))) == "\xEF\xBF\xBD");
	CHECK(voltz::textCharCount(kOla) == 3);
}

TEST_CASE("appendTextCodepoint/popTextCodepoint: controles recusados; limites de caracteres e de linhas")
{
	std::string text;
	CHECK(voltz::appendTextCodepoint(text, U'O'));
	CHECK(voltz::appendTextCodepoint(text, U'\u00E1'));
	CHECK(text == "O\xC3\xA1");
	CHECK_FALSE(voltz::appendTextCodepoint(text, U'\b'));
	CHECK_FALSE(voltz::appendTextCodepoint(text, U'\r'));
	CHECK_FALSE(voltz::appendTextCodepoint(text, char32_t(0x1B)));
	CHECK_FALSE(voltz::appendTextCodepoint(text, char32_t(0x7F)));
	CHECK_FALSE(voltz::appendTextCodepoint(text, char32_t(0x85)));
	CHECK(voltz::popTextCodepoint(text));
	CHECK(text == "O");
	CHECK(voltz::popTextCodepoint(text));
	CHECK_FALSE(voltz::popTextCodepoint(text));

	std::string lines;
	for (size_t i = 0; i + 1 < voltz::kMaxTextLines; i++) {
		CHECK(voltz::appendTextCodepoint(lines, U'\n'));
	}
	CHECK_FALSE(voltz::appendTextCodepoint(lines, U'\n')); // a 13ª linha não entra
	CHECK(voltz::appendTextCodepoint(lines, U'x'));

	std::string full;
	for (size_t i = 0; i < voltz::kMaxTextChars; i++) {
		REQUIRE(voltz::appendTextCodepoint(full, U'a'));
	}
	CHECK_FALSE(voltz::appendTextCodepoint(full, U'b'));
	CHECK(voltz::textCharCount(full) == voltz::kMaxTextChars);
}

TEST_CASE("sanitizeAnnotationText: quebras, controles e cortes")
{
	CHECK(voltz::sanitizeAnnotationText("a\r\nb\rc\td") == "a\nb\ncd");
	CHECK(voltz::sanitizeAnnotationText("x\xFFy") == "x\xEF\xBF\xBDy");
	std::string many;
	for (int i = 0; i < 20; i++) {
		many += "l\n";
	}
	const std::string cut = voltz::sanitizeAnnotationText(many);
	CHECK(std::count(cut.begin(), cut.end(), '\n') == static_cast<long>(voltz::kMaxTextLines) - 1);
	CHECK(voltz::textCharCount(voltz::sanitizeAnnotationText(std::string(500, 'z'))) == voltz::kMaxTextChars);
}

TEST_CASE("StrokeStore: texto — beginText vazio, setOpenText, setOpenTextStyle, discardOpen")
{
	voltz::StrokeStore store;
	const int id = store.beginText(RgbColor{1, 2, 3}, 5000, TextFont::Regular, StrokePoint{10, 20}, 100);
	REQUIRE(id == 1);
	const Stroke &open = store.visible().back().stroke;
	CHECK(open.tool == AnnotationTool::Text);
	CHECK(open.text.empty());
	CHECK(open.textSize == voltz::kTextSizeMax);
	CHECK(open.textFont == TextFont::Regular);
	CHECK(store.beginText(RgbColor{}, 48, TextFont::Bold, StrokePoint{}, 110) == 0); // já há um em andamento
	CHECK(store.setOpenText(kOla + "\x07"));
	CHECK(store.visible().back().stroke.text == kOla);
	CHECK(store.setOpenTextStyle(RgbColor{9, 8, 7}, 3, TextFont::Bold));
	CHECK(store.visible().back().stroke.color == RgbColor{9, 8, 7});
	CHECK(store.visible().back().stroke.textSize == voltz::kTextSizeMin);
	CHECK(store.visible().back().stroke.textFont == TextFont::Bold);
	CHECK(store.extend({{1, 1}})); // a âncora não muda
	CHECK(store.visible().back().stroke.points.size() == 1);
	CHECK(store.discardOpen());
	CHECK(store.visible().empty());
	CHECK_FALSE(store.discardOpen());
	CHECK_FALSE(store.setOpenText("x")); // sem texto em andamento

	/* caneta em andamento: setOpenText recusa; depois de terminar, nada mais muda */
	REQUIRE(store.begin(AnnotationTool::Pen, RgbColor{}, 4, StrokePoint{0, 0}, 200) == 2);
	CHECK_FALSE(store.setOpenText("x"));
	CHECK_FALSE(store.setOpenTextStyle(RgbColor{}, 48, TextFont::Bold));
	REQUIRE(store.end(210) != nullptr);
	CHECK_FALSE(store.discardOpen());
	CHECK(store.visible().size() == 1);
}
