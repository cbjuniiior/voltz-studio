#include <doctest.h>

#include <voltz/core/annotation-raster.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

using voltz::AnnotationTool;
using voltz::RgbColor;
using voltz::Stroke;
using voltz::StrokePoint;

namespace {

struct Canvas {
	int width = 0;
	int height = 0;
	std::vector<uint8_t> pixels;

	voltz::RasterImage image() { return voltz::RasterImage{pixels.data(), width, height, width * 4}; }
	// B, G, R, A
	const uint8_t *at(int x, int y) const
	{
		return &pixels[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4];
	}
};

Canvas makeCanvas(int width, int height)
{
	return Canvas{width, height,
		      std::vector<uint8_t>(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0)};
}

Stroke makeStroke(AnnotationTool tool, RgbColor color, int width, std::vector<StrokePoint> points)
{
	Stroke stroke;
	stroke.tool = tool;
	stroke.color = color;
	stroke.width = width;
	stroke.points = std::move(points);
	return stroke;
}

const RgbColor kLime{0xD4, 0xF5, 0x3C};
const RgbColor kRed{0xFF, 0x4D, 0x4D};
const RgbColor kBlue{0x00, 0x00, 0xFF};

} // namespace

TEST_CASE("strokeShape: peças por ferramenta")
{
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Pen, kLime, 4, {{0, 0}, {5, 0}, {9, 3}}), 1, 1)
		      .capsules.size() == 2);
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Pen, kLime, 4, {{3, 3}}), 1, 1).capsules.size() == 1);
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Line, kLime, 4, {{0, 0}, {50, 9}}), 1, 1).capsules.size() ==
	      1);
	const voltz::StrokeShape arrow =
		voltz::strokeShape(makeStroke(AnnotationTool::Arrow, kLime, 4, {{10, 32}, {110, 32}}), 1, 1);
	CHECK(arrow.capsules.size() == 1);
	CHECK(arrow.triangles.size() == 1);
	const voltz::StrokeShape shortArrow =
		voltz::strokeShape(makeStroke(AnnotationTool::Arrow, kLime, 4, {{10, 32}, {15, 32}}), 1, 1);
	CHECK(shortArrow.capsules.empty());
	CHECK(shortArrow.triangles.size() == 1);
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Rectangle, kLime, 4, {{10, 10}, {50, 30}}), 1, 1)
		      .capsules.size() == 4);
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Ellipse, kLime, 4, {{0, 0}, {10, 10}}), 1, 1)
		      .capsules.size() == 24);
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Ellipse, kLime, 4, {{0, 0}, {2000, 1000}}), 1, 1)
		      .capsules.size() == 180);
	const voltz::StrokeShape scaled =
		voltz::strokeShape(makeStroke(AnnotationTool::Line, kLime, 8, {{0, 64}, {255, 64}}), 0.25, 0.25);
	REQUIRE(scaled.capsules.size() == 1);
	CHECK(scaled.capsules[0].radius == doctest::Approx(1.0));
	CHECK(scaled.capsules[0].y0 == doctest::Approx(16.125));
	CHECK(voltz::strokeShape(Stroke{}, 1, 1).capsules.empty());
}

TEST_CASE("rasterizeStroke: linha opaca com borda suavizada de ~1 px")
{
	Canvas canvas = makeCanvas(64, 32);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Line, kLime, 4, {{8, 16}, {56, 16}}), 1, 1);
	const uint8_t *center = canvas.at(32, 16);
	CHECK(center[0] == 0x3C);
	CHECK(center[1] == 0xF5);
	CHECK(center[2] == 0xD4);
	CHECK(center[3] == 255);
	const uint8_t *edge = canvas.at(32, 18); // distância 2 do eixo = metade da cobertura
	CHECK(edge[3] == 128);
	CHECK(edge[1] == 123);
	CHECK(canvas.at(32, 19)[3] == 0);
	CHECK(canvas.at(32, 10)[3] == 0);
}

TEST_CASE("rasterizeStroke: marca-texto a 40% sem escurecer nas junções")
{
	Canvas canvas = makeCanvas(64, 32);
	voltz::rasterizeStroke(canvas.image(),
			       makeStroke(AnnotationTool::Highlighter, kRed, 2, {{8, 16}, {32, 16}, {56, 16}}), 1, 1);
	const uint8_t *joint = canvas.at(32, 16);
	CHECK(joint[3] == 102);
	CHECK(joint[2] == 102);
	CHECK(joint[1] == 31);
	CHECK(canvas.at(20, 16)[3] == 102);
	CHECK(canvas.at(20, 18)[3] == 102); // marca-texto = 3x a espessura (6 px)
	CHECK(voltz::strokeRenderWidth(makeStroke(AnnotationTool::Highlighter, kRed, 2, {})) == doctest::Approx(6.0));
	CHECK(voltz::strokeOpacity(makeStroke(AnnotationTool::Highlighter, kRed, 2, {})) == doctest::Approx(0.4));
	CHECK(voltz::strokeOpacity(makeStroke(AnnotationTool::Pen, kRed, 2, {})) == doctest::Approx(1.0));
}

TEST_CASE("rasterizeStroke: seta com ponta cheia")
{
	Canvas canvas = makeCanvas(128, 64);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Arrow, kLime, 4, {{10, 32}, {110, 32}}), 1,
			       1);
	CHECK(canvas.at(50, 32)[3] == 255);  // haste
	CHECK(canvas.at(100, 36)[3] == 255); // dentro da ponta, fora da haste
	CHECK(canvas.at(100, 42)[3] == 0);
	CHECK(canvas.at(50, 37)[3] == 0);
}

TEST_CASE("rasterizeStroke: retângulo e elipse só no contorno")
{
	Canvas canvas = makeCanvas(128, 64);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Rectangle, kLime, 2, {{10, 10}, {50, 30}}), 1,
			       1);
	CHECK(canvas.at(30, 10)[3] == 255);
	CHECK(canvas.at(50, 20)[3] == 255);
	CHECK(canvas.at(30, 20)[3] == 0);

	Canvas oval = makeCanvas(128, 64);
	voltz::rasterizeStroke(oval.image(), makeStroke(AnnotationTool::Ellipse, kLime, 4, {{0, 0}, {99, 49}}), 1, 1);
	CHECK(oval.at(99, 25)[3] == 255);
	CHECK(oval.at(50, 0)[3] == 255);
	CHECK(oval.at(50, 25)[3] == 0);
}

TEST_CASE("rasterizeStroke: escala da captura para a imagem")
{
	Canvas canvas = makeCanvas(64, 32);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Line, kLime, 8, {{0, 64}, {255, 64}}), 0.25,
			       0.25);
	CHECK(canvas.at(32, 16)[3] == 255);
	CHECK(canvas.at(32, 18)[3] == 0);
}

TEST_CASE("rasterizeStroke: por cima do traço anterior e cortado na borda da imagem")
{
	Canvas canvas = makeCanvas(64, 32);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Line, kRed, 4, {{-100, 16}, {300, 16}}), 1,
			       1);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Line, kBlue, 4, {{32, -50}, {32, 80}}), 1, 1);
	const uint8_t *cross = canvas.at(32, 16);
	CHECK(cross[0] == 255);
	CHECK(cross[2] == 0);
	CHECK(canvas.at(0, 16)[2] == 255);
	CHECK(canvas.at(63, 16)[2] == 255);
	CHECK(canvas.at(32, 0)[0] == 255);
}

TEST_CASE("clearRaster zera a imagem respeitando o stride")
{
	std::vector<uint8_t> pixels(static_cast<size_t>(8) * 4 * 2, 7);
	const voltz::RasterImage image{pixels.data(), 6, 2, 32};
	voltz::clearRaster(image);
	CHECK(pixels[0] == 0);
	CHECK(pixels[23] == 0);
	CHECK(pixels[24] == 7); // fora da largura útil da linha 0
	CHECK(pixels[32] == 0);
	voltz::rasterizeStroke(voltz::RasterImage{}, makeStroke(AnnotationTool::Pen, kLime, 4, {{1, 1}}), 1, 1);
}
