#include <doctest.h>

#include <voltz/core/annotation-raster.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

using voltz::AnnotationTool;
using voltz::PixelRect;
using voltz::RgbColor;
using voltz::Stroke;
using voltz::StrokePoint;

namespace {

const RgbColor kLime{0xD4, 0xF5, 0x3C};
const RgbColor kRed{0xFF, 0x4D, 0x4D};
const RgbColor kBlue{0x4D, 0xA3, 0xFF};

struct Image {
	Image(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0) {}
	voltz::RasterImage view() { return voltz::RasterImage{pixels.data(), width, height, width * 4}; }
	int width;
	int height;
	std::vector<uint8_t> pixels;
};

Stroke makeStroke(int id, AnnotationTool tool, RgbColor color, int width, const std::vector<StrokePoint> &points)
{
	Stroke stroke;
	stroke.id = id;
	stroke.tool = tool;
	stroke.color = color;
	stroke.width = width;
	voltz::appendStrokePoints(stroke, points);
	return stroke;
}

void redrawAll(Image &image, const std::vector<const Stroke *> &strokes, double scaleX, double scaleY)
{
	voltz::clearRaster(image.view());
	for (const Stroke *stroke : strokes) {
		voltz::rasterizeStroke(image.view(), *stroke, scaleX, scaleY);
	}
}

bool inside(const PixelRect &rect, int x, int y)
{
	return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

} // namespace

TEST_CASE("uniteRects/intersectRects: vazios não contam; sobreposição")
{
	CHECK(PixelRect{}.empty());
	CHECK_FALSE((PixelRect{0, 0, 1, 1}).empty());
	CHECK(voltz::uniteRects(PixelRect{0, 0, 10, 10}, PixelRect{5, -2, 20, 4}) == PixelRect{0, -2, 20, 10});
	CHECK(voltz::uniteRects(PixelRect{}, PixelRect{1, 2, 3, 4}) == PixelRect{1, 2, 3, 4});
	CHECK(voltz::uniteRects(PixelRect{1, 2, 3, 4}, PixelRect{5, 5, 5, 9}) == PixelRect{1, 2, 3, 4});
	CHECK(voltz::uniteRects(PixelRect{}, PixelRect{}).empty());
	CHECK(voltz::intersectRects(PixelRect{0, 0, 10, 10}, PixelRect{5, 5, 20, 20}) == PixelRect{5, 5, 10, 10});
	CHECK(voltz::intersectRects(PixelRect{0, 0, 10, 10}, PixelRect{10, 0, 20, 10}).empty());
	CHECK(voltz::intersectRects(PixelRect{0, 0, 10, 10}, PixelRect{10, 0, 20, 10}) == PixelRect{});
}

TEST_CASE("strokeBounds: cobre todo pixel desenhado (caneta, marca-texto, seta, elipse; escala 1 e 0,25)")
{
	const std::vector<Stroke> strokes = {
		makeStroke(1, AnnotationTool::Pen, kLime, 6, {{20, 20}, {80, 60}, {140, 30}}),
		makeStroke(2, AnnotationTool::Highlighter, kRed, 8, {{30, 200}, {250, 210}}),
		makeStroke(3, AnnotationTool::Arrow, kBlue, 5, {{50, 250}, {350, 120}}),
		makeStroke(4, AnnotationTool::Ellipse, kLime, 4, {{200, 40}, {380, 140}}),
	};
	for (double scale : {1.0, 0.25}) {
		for (const Stroke &stroke : strokes) {
			Image image(400, 300);
			voltz::rasterizeStroke(image.view(), stroke, scale, scale);
			const PixelRect bounds = voltz::strokeBounds(stroke, scale, scale);
			int drawn = 0;
			int outside = 0;
			for (int y = 0; y < image.height; y++) {
				for (int x = 0; x < image.width; x++) {
					const size_t alpha =
						(static_cast<size_t>(y) * static_cast<size_t>(image.width) +
						 static_cast<size_t>(x)) *
							4 +
						3;
					if (image.pixels[alpha] != 0) {
						drawn++;
						if (!inside(bounds, x, y)) {
							outside++;
						}
					}
				}
			}
			CHECK(drawn > 0);
			CHECK(outside == 0);
		}
	}
}

TEST_CASE("rasterizeStrokeClipped: fora do recorte intacto; dentro igual ao desenho inteiro")
{
	const Stroke stroke = makeStroke(1, AnnotationTool::Highlighter, kRed, 10, {{10, 10}, {200, 150}, {390, 290}});
	Image full(400, 300);
	voltz::rasterizeStroke(full.view(), stroke, 1.0, 1.0);
	Image clipped(400, 300);
	const PixelRect clip{100, 80, 260, 200};
	voltz::rasterizeStrokeClipped(clipped.view(), stroke, 1.0, 1.0, clip);
	int mismatches = 0;
	int touchedOutside = 0;
	int drawnInside = 0;
	for (int y = 0; y < 300; y++) {
		for (int x = 0; x < 400; x++) {
			const size_t index = (static_cast<size_t>(y) * 400 + static_cast<size_t>(x)) * 4;
			for (size_t c = 0; c < 4; c++) {
				if (inside(clip, x, y)) {
					mismatches += clipped.pixels[index + c] != full.pixels[index + c] ? 1 : 0;
				} else {
					touchedOutside += clipped.pixels[index + c] != 0 ? 1 : 0;
				}
			}
			drawnInside += inside(clip, x, y) && clipped.pixels[index + 3] != 0 ? 1 : 0;
		}
	}
	CHECK(drawnInside > 0);
	CHECK(mismatches == 0);
	CHECK(touchedOutside == 0);
}

TEST_CASE("redrawRegion depois de acrescentar pontos = redesenhar tudo (caneta, marca-texto, seta, retângulo)")
{
	const double scales[][2] = {{1.0, 1.0}, {0.25, 0.25}, {0.75, 0.5}};
	const AnnotationTool tools[] = {AnnotationTool::Pen, AnnotationTool::Highlighter, AnnotationTool::Arrow,
					AnnotationTool::Rectangle};
	for (const auto &scale : scales) {
		for (AnnotationTool tool : tools) {
			const Stroke below =
				makeStroke(1, AnnotationTool::Highlighter, kRed, 6, {{20, 30}, {300, 200}});
			Stroke stroke = makeStroke(2, tool, kLime, 5, {{40, 40}, {120, 90}});
			const Stroke above = makeStroke(3, AnnotationTool::Pen, kBlue, 3, {{100, 20}, {110, 280}});
			const std::vector<const Stroke *> list{&below, &stroke, &above};
			Image incremental(400, 300);
			redrawAll(incremental, list, scale[0], scale[1]);
			const voltz::StrokeTail tail = voltz::strokeTail(stroke);
			voltz::appendStrokePoints(stroke, {{200, 150}, {260, 90}});
			const PixelRect dirty = voltz::appendDirtyRect(tail, stroke, scale[0], scale[1]);
			CHECK_FALSE(dirty.empty());
			voltz::redrawRegion(incremental.view(), list, scale[0], scale[1], dirty);
			Image full(400, 300);
			redrawAll(full, list, scale[0], scale[1]);
			CHECK(incremental.pixels == full.pixels);
		}
	}
	/* forma que encolhe e muda de direção: a forma antiga (fora da nova) também precisa ser apagada */
	const AnnotationTool shapes[] = {AnnotationTool::Rectangle, AnnotationTool::Arrow, AnnotationTool::Ellipse,
					 AnnotationTool::Line};
	for (const auto &scale : scales) {
		for (AnnotationTool tool : shapes) {
			const Stroke below =
				makeStroke(1, AnnotationTool::Highlighter, kRed, 6, {{20, 30}, {300, 200}});
			Stroke stroke = makeStroke(2, tool, kLime, 5, {{40, 40}, {360, 260}});
			const Stroke above = makeStroke(3, AnnotationTool::Pen, kBlue, 3, {{100, 20}, {110, 280}});
			const std::vector<const Stroke *> list{&below, &stroke, &above};
			Image incremental(400, 300);
			redrawAll(incremental, list, scale[0], scale[1]);
			const voltz::StrokeTail tail = voltz::strokeTail(stroke);
			voltz::appendStrokePoints(stroke, {{100, 80}});
			const PixelRect dirty = voltz::appendDirtyRect(tail, stroke, scale[0], scale[1]);
			CHECK_FALSE(dirty.empty());
			voltz::redrawRegion(incremental.view(), list, scale[0], scale[1], dirty);
			Image full(400, 300);
			redrawAll(full, list, scale[0], scale[1]);
			CHECK(incremental.pixels == full.pixels);
		}
	}
	/* ponto repetido na caneta: nada muda */
	Stroke pen = makeStroke(9, AnnotationTool::Pen, kLime, 4, {{10, 10}, {20, 20}});
	const voltz::StrokeTail tail = voltz::strokeTail(pen);
	voltz::appendStrokePoints(pen, {{20, 20}});
	CHECK(voltz::appendDirtyRect(tail, pen, 1.0, 1.0).empty());
}

TEST_CASE("redrawRegion depois de desfazer = redesenhar tudo sem o traço")
{
	for (double scale : {1.0, 0.25}) {
		const Stroke a = makeStroke(1, AnnotationTool::Highlighter, kRed, 8, {{30, 60}, {370, 80}});
		const Stroke b = makeStroke(2, AnnotationTool::Pen, kLime, 6, {{50, 20}, {120, 200}, {300, 260}});
		const Stroke c = makeStroke(3, AnnotationTool::Arrow, kBlue, 4, {{390, 10}, {150, 150}});
		Image image(400, 300);
		redrawAll(image, {&a, &b, &c}, scale, scale);
		voltz::redrawRegion(image.view(), {&a, &c}, scale, scale, voltz::strokeBounds(b, scale, scale));
		Image expected(400, 300);
		redrawAll(expected, {&a, &c}, scale, scale);
		CHECK(image.pixels == expected.pixels);

		/* marca-texto grosso que atravessa a borda e o canto: a região sai da imagem */
		Stroke edge = makeStroke(4, AnnotationTool::Highlighter, kLime, 48, {{-20, -10}});
		const std::vector<const Stroke *> withEdge{&a, &edge, &c};
		Image incremental(400, 300);
		redrawAll(incremental, withEdge, scale, scale);
		const voltz::StrokeTail tail = voltz::strokeTail(edge);
		voltz::appendStrokePoints(edge, {{395, 295}});
		const PixelRect dirty = voltz::appendDirtyRect(tail, edge, scale, scale);
		CHECK(dirty.left < 0);
		CHECK(dirty.top < 0);
		CHECK((scale < 1.0 || (dirty.right > 400 && dirty.bottom > 300)));
		voltz::redrawRegion(incremental.view(), withEdge, scale, scale, dirty);
		Image full(400, 300);
		redrawAll(full, withEdge, scale, scale);
		CHECK(incremental.pixels == full.pixels);
		voltz::redrawRegion(incremental.view(), {&a, &c}, scale, scale,
				    voltz::strokeBounds(edge, scale, scale));
		CHECK(incremental.pixels == expected.pixels);
	}
}

TEST_CASE("DirtyHistory: união desde a versão conhecida; 0, lacuna e 'tudo' pedem tudo; capacidade")
{
	voltz::DirtyHistory history;
	history.record(2, PixelRect{0, 0, 10, 10}, false);
	history.record(3, PixelRect{20, 20, 30, 30}, false);
	PixelRect out{1, 1, 2, 2};
	CHECK(history.since(1, 3, out));
	CHECK(out == PixelRect{0, 0, 30, 30});
	CHECK(history.since(2, 3, out));
	CHECK(out == PixelRect{20, 20, 30, 30});
	CHECK(history.since(3, 3, out));
	CHECK(out.empty());
	CHECK_FALSE(history.since(0, 3, out));
	CHECK_FALSE(history.since(4, 3, out));
	history.record(4, PixelRect{}, true);
	CHECK_FALSE(history.since(3, 4, out));
	history.record(6, PixelRect{1, 1, 2, 2}, false); /* a versão 5 não foi registrada */
	CHECK_FALSE(history.since(4, 6, out));
	CHECK(history.since(5, 6, out));
	const uint64_t last = 6 + voltz::DirtyHistory::kCapacity;
	for (uint64_t version = 7; version <= last; version++) {
		history.record(version, PixelRect{0, 0, 1, 1}, false);
	}
	CHECK(history.since(6, last, out));
	CHECK(out == PixelRect{0, 0, 1, 1});
	CHECK_FALSE(history.since(5, last, out));
}
