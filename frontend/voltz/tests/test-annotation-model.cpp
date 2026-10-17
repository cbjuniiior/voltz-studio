#include <doctest.h>

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/live-protocol.hpp>

#include <string>
#include <vector>

using voltz::AnnotationTool;
using voltz::RgbColor;
using voltz::Stroke;
using voltz::StrokePoint;

TEST_CASE("toolToString/toolFromString: as seis ferramentas e texto desconhecido")
{
	const AnnotationTool tools[] = {AnnotationTool::Pen,   AnnotationTool::Highlighter, AnnotationTool::Line,
					AnnotationTool::Arrow, AnnotationTool::Rectangle,   AnnotationTool::Ellipse};
	const char *names[] = {"caneta", "marca_texto", "linha", "seta", "retangulo", "elipse"};
	for (int i = 0; i < 6; i++) {
		CHECK(std::string(voltz::toolToString(tools[i])) == names[i]);
		AnnotationTool parsed = AnnotationTool::Pen;
		CHECK(voltz::toolFromString(names[i], parsed));
		CHECK(parsed == tools[i]);
	}
	AnnotationTool unchanged = AnnotationTool::Arrow;
	CHECK_FALSE(voltz::toolFromString("texto", unchanged));
	CHECK(unchanged == AnnotationTool::Arrow);
	CHECK_FALSE(voltz::isShapeTool(AnnotationTool::Pen));
	CHECK_FALSE(voltz::isShapeTool(AnnotationTool::Highlighter));
	CHECK(voltz::isShapeTool(AnnotationTool::Ellipse));
}

TEST_CASE("strokeToJsonLine: formato da spec, sem t_fim/t_limpo enquanto -1")
{
	Stroke stroke;
	stroke.id = 7;
	stroke.tool = AnnotationTool::Arrow;
	stroke.width = 4;
	stroke.points = {{10, 20}, {30, 40}};
	stroke.startMs = 1200;
	CHECK(voltz::strokeToJsonLine(stroke) ==
	      R"({"id":7,"ferr":"seta","cor":"#D4F53C","esp":4,"pts":[[10,20],[30,40]],"t_ini":1200})");
	stroke.endMs = 1850;
	stroke.clearedMs = 9000;
	CHECK(voltz::strokeToJsonLine(stroke) ==
	      R"({"id":7,"ferr":"seta","cor":"#D4F53C","esp":4,"pts":[[10,20],[30,40]],"t_ini":1200,"t_fim":1850,"t_limpo":9000})");
}

TEST_CASE("strokeFromJson: ida e volta, leitura tolerante e recusas")
{
	Stroke stroke;
	stroke.id = 3;
	stroke.tool = AnnotationTool::Highlighter;
	stroke.color = RgbColor{0xFF, 0x4D, 0x4D};
	stroke.width = 8;
	stroke.points = {{1, 2}, {3, 4}, {5, 6}};
	stroke.startMs = 10;
	stroke.endMs = 20;
	Stroke parsed;
	REQUIRE(voltz::strokeFromJson(voltz::strokeToJsonLine(stroke), parsed));
	CHECK(parsed.id == 3);
	CHECK(parsed.tool == AnnotationTool::Highlighter);
	CHECK(parsed.color == stroke.color);
	CHECK(parsed.width == 8);
	REQUIRE(parsed.points.size() == 3);
	CHECK(parsed.points[2] == StrokePoint{5, 6});
	CHECK(parsed.startMs == 10);
	CHECK(parsed.endMs == 20);
	CHECK(parsed.clearedMs == -1);

	Stroke loose;
	REQUIRE(voltz::strokeFromJson(
		R"({"id":9,"ferr":"linha","cor":"azul","esp":500,"pts":[[1.6,2],"x",[3]],"extra":true})", loose));
	CHECK(loose.color == RgbColor{0xD4, 0xF5, 0x3C});
	CHECK(loose.width == voltz::kStrokeWidthMax);
	REQUIRE(loose.points.size() == 1);
	CHECK(loose.points[0] == StrokePoint{2, 2});
	CHECK(loose.startMs == 0);

	Stroke untouched;
	untouched.id = 42;
	CHECK_FALSE(voltz::strokeFromJson("nao e json", untouched));
	CHECK_FALSE(voltz::strokeFromJson(R"({"ferr":"seta"})", untouched));
	CHECK_FALSE(voltz::strokeFromJson(R"({"id":1,"ferr":"texto"})", untouched));
	CHECK_FALSE(voltz::strokeFromJson("[1,2]", untouched));
	CHECK(untouched.id == 42);
}

TEST_CASE("appendStrokePoints: caneta acumula sem repetir; formas guardam primeiro e último")
{
	Stroke pen;
	voltz::appendStrokePoints(pen, {{1, 1}, {1, 1}, {2, 2}});
	voltz::appendStrokePoints(pen, {{2, 2}, {3, 3}});
	REQUIRE(pen.points.size() == 3);
	CHECK(pen.points[2] == StrokePoint{3, 3});

	Stroke arrow;
	arrow.tool = AnnotationTool::Arrow;
	voltz::appendStrokePoints(arrow, {{10, 10}});
	CHECK(arrow.points.size() == 1);
	voltz::appendStrokePoints(arrow, {{20, 20}, {30, 35}});
	voltz::appendStrokePoints(arrow, {{40, 45}});
	REQUIRE(arrow.points.size() == 2);
	CHECK(arrow.points[0] == StrokePoint{10, 10});
	CHECK(arrow.points[1] == StrokePoint{40, 45});

	Stroke full;
	std::vector<StrokePoint> many;
	for (int i = 0; i < static_cast<int>(voltz::kMaxStrokePoints) + 5; i++) {
		many.push_back(StrokePoint{i, 0});
	}
	voltz::appendStrokePoints(full, many);
	CHECK(full.points.size() == voltz::kMaxStrokePoints);
}

TEST_CASE("pointsToJson/pointsFromJson")
{
	CHECK(voltz::pointsToJson({{1, 2}, {-3, 4}}) == "[[1,2],[-3,4]]");
	CHECK(voltz::pointsToJson({}) == "[]");
	std::vector<StrokePoint> points;
	REQUIRE(voltz::pointsFromJson("[[5,6],[7,8]]", points));
	REQUIRE(points.size() == 2);
	CHECK(points[1] == StrokePoint{7, 8});
	CHECK_FALSE(voltz::pointsFromJson("{\"x\":1}", points));
	CHECK_FALSE(voltz::pointsFromJson("lixo", points));
	CHECK(points.size() == 2);
}

TEST_CASE("AnnotationStyle: padrão, limites e JSON")
{
	const voltz::AnnotationStyle style;
	CHECK(voltz::annotationStyleToJson(style) == R"({"ferr":"caneta","cor":"#D4F53C","esp":4,"somem_apos_s":0})");
	voltz::AnnotationStyle parsed;
	REQUIRE(voltz::annotationStyleFromJson(R"({"ferr":"seta","cor":"#4DA3FF","esp":0,"somem_apos_s":999})",
					       parsed));
	CHECK(parsed.tool == AnnotationTool::Arrow);
	CHECK(parsed.color == RgbColor{0x4D, 0xA3, 0xFF});
	CHECK(parsed.width == voltz::kStrokeWidthMin);
	CHECK(parsed.fadeSeconds == voltz::kFadeSecondsMax);
	REQUIRE(voltz::annotationStyleFromJson(R"({"somem_apos_s":3})", parsed));
	CHECK(parsed.tool == AnnotationTool::Pen);
	CHECK(parsed.fadeSeconds == 3);
	CHECK_FALSE(voltz::annotationStyleFromJson("[]", parsed));
	CHECK(parsed.fadeSeconds == 0);
}

TEST_CASE("live-protocol: procedimentos das anotações declarados com o próprio nome")
{
	const char *pairs[][2] = {{voltz::live::kProcAnnotBegin, voltz::live::kDeclAnnotBegin},
				  {voltz::live::kProcAnnotExtend, voltz::live::kDeclAnnotExtend},
				  {voltz::live::kProcAnnotRemove, voltz::live::kDeclAnnotRemove},
				  {voltz::live::kProcAnnotClear, voltz::live::kDeclAnnotClear},
				  {voltz::live::kProcAnnotBurned, voltz::live::kDeclAnnotBurned}};
	for (const auto &pair : pairs) {
		CHECK(std::string(pair[1]).find(std::string(" ") + pair[0] + "(") != std::string::npos);
	}
	CHECK(std::string(voltz::live::kAnnotationsSourceId) == "voltz_annotations_source");
}
