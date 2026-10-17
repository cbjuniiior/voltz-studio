#include "annotation-raster.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace voltz {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kEllipseMinSides = 24;
constexpr int kEllipseMaxSides = 180;
constexpr double kEllipseSideLength = 8.0;
constexpr double kArrowHeadMin = 12.0;
constexpr double kArrowHeadPerWidth = 4.0;

struct Point {
	double x = 0.0;
	double y = 0.0;
};

Point scaledPoint(const StrokePoint &point, double scaleX, double scaleY)
{
	return Point{(point.x + 0.5) * scaleX, (point.y + 0.5) * scaleY};
}

void addCapsule(StrokeShape &shape, const Point &a, const Point &b, double radius)
{
	shape.capsules.push_back(Capsule{a.x, a.y, b.x, b.y, radius});
}

void addClosedPolygon(StrokeShape &shape, const std::vector<Point> &corners, double radius)
{
	for (size_t i = 0; i < corners.size(); i++) {
		addCapsule(shape, corners[i], corners[(i + 1) % corners.size()], radius);
	}
}

double capsuleDistance(double px, double py, const Capsule &capsule)
{
	const double dx = capsule.x1 - capsule.x0;
	const double dy = capsule.y1 - capsule.y0;
	const double lengthSq = dx * dx + dy * dy;
	double t = 0.0;
	if (lengthSq > 0.0) {
		t = std::clamp(((px - capsule.x0) * dx + (py - capsule.y0) * dy) / lengthSq, 0.0, 1.0);
	}
	const double ex = px - (capsule.x0 + t * dx);
	const double ey = py - (capsule.y0 + t * dy);
	return std::sqrt(ex * ex + ey * ey);
}

// Distância do ponto à borda do triângulo, positiva por dentro (qualquer orientação dos vértices).
double triangleDepth(double px, double py, const FilledTriangle &triangle)
{
	const double area = (triangle.x[1] - triangle.x[0]) * (triangle.y[2] - triangle.y[0]) -
			    (triangle.y[1] - triangle.y[0]) * (triangle.x[2] - triangle.x[0]);
	if (std::abs(area) < 1e-9) {
		return -1e9;
	}
	const double sign = area > 0.0 ? 1.0 : -1.0;
	double depth = 1e9;
	for (int i = 0; i < 3; i++) {
		const int j = (i + 1) % 3;
		const double ex = triangle.x[j] - triangle.x[i];
		const double ey = triangle.y[j] - triangle.y[i];
		const double length = std::sqrt(ex * ex + ey * ey);
		if (length < 1e-9) {
			continue;
		}
		depth = std::min(depth, (ex * (py - triangle.y[i]) - ey * (px - triangle.x[i])) * sign / length);
	}
	return depth;
}

// [low, high] em pixels inteiros [from, to) dentro de [limitLow, limitHigh).
void pixelSpan(double low, double high, int limitLow, int limitHigh, int &from, int &to)
{
	from = static_cast<int>(
		std::clamp(std::floor(low), static_cast<double>(limitLow), static_cast<double>(limitHigh)));
	to = static_cast<int>(
		std::clamp(std::ceil(high) + 1.0, static_cast<double>(limitLow), static_cast<double>(limitHigh)));
}

uint8_t blendChannel(int source, int destination, int alpha)
{
	return static_cast<uint8_t>((source * alpha + destination * (255 - alpha) + 127) / 255);
}

} // namespace

double strokeRenderWidth(const Stroke &stroke)
{
	const double width = static_cast<double>(clampStrokeWidth(stroke.width));
	return stroke.tool == AnnotationTool::Highlighter ? width * kHighlighterWidthFactor : width;
}

double strokeOpacity(const Stroke &stroke)
{
	return stroke.tool == AnnotationTool::Highlighter ? kHighlighterOpacity : 1.0;
}

StrokeShape strokeShape(const Stroke &stroke, double scaleX, double scaleY)
{
	StrokeShape shape;
	if (stroke.points.empty() || scaleX <= 0.0 || scaleY <= 0.0) {
		return shape;
	}
	const double scale = (scaleX + scaleY) / 2.0;
	const double radius = strokeRenderWidth(stroke) * scale / 2.0;
	const Point first = scaledPoint(stroke.points.front(), scaleX, scaleY);
	const Point last = scaledPoint(stroke.points.back(), scaleX, scaleY);
	switch (stroke.tool) {
	case AnnotationTool::Pen:
	case AnnotationTool::Highlighter:
		if (stroke.points.size() == 1) {
			addCapsule(shape, first, first, radius);
		}
		for (size_t i = 1; i < stroke.points.size(); i++) {
			addCapsule(shape, scaledPoint(stroke.points[i - 1], scaleX, scaleY),
				   scaledPoint(stroke.points[i], scaleX, scaleY), radius);
		}
		break;
	case AnnotationTool::Line:
		addCapsule(shape, first, last, radius);
		break;
	case AnnotationTool::Arrow: {
		const double dx = last.x - first.x;
		const double dy = last.y - first.y;
		const double length = std::sqrt(dx * dx + dy * dy);
		if (length < 1e-6) {
			addCapsule(shape, first, first, radius);
			break;
		}
		const double head = std::min(
			length, std::max(kArrowHeadMin, kArrowHeadPerWidth * clampStrokeWidth(stroke.width)) * scale);
		const double ux = dx / length;
		const double uy = dy / length;
		const Point base{last.x - ux * head, last.y - uy * head};
		if (length > head) {
			addCapsule(shape, first, base, radius);
		}
		const double half = head * 0.5;
		FilledTriangle tip;
		tip.x[0] = last.x;
		tip.y[0] = last.y;
		tip.x[1] = base.x - uy * half;
		tip.y[1] = base.y + ux * half;
		tip.x[2] = base.x + uy * half;
		tip.y[2] = base.y - ux * half;
		shape.triangles.push_back(tip);
		break;
	}
	case AnnotationTool::Rectangle:
		addClosedPolygon(shape, {first, Point{last.x, first.y}, last, Point{first.x, last.y}}, radius);
		break;
	case AnnotationTool::Ellipse: {
		const double centerX = (first.x + last.x) / 2.0;
		const double centerY = (first.y + last.y) / 2.0;
		const double radiusX = std::abs(last.x - first.x) / 2.0;
		const double radiusY = std::abs(last.y - first.y) / 2.0;
		if (radiusX < 0.5 && radiusY < 0.5) {
			addCapsule(shape, first, first, radius);
			break;
		}
		const double perimeter = 2.0 * kPi * std::sqrt((radiusX * radiusX + radiusY * radiusY) / 2.0);
		const int sides = std::clamp(static_cast<int>(std::ceil(perimeter / kEllipseSideLength)),
					     kEllipseMinSides, kEllipseMaxSides);
		std::vector<Point> corners;
		corners.reserve(static_cast<size_t>(sides));
		for (int i = 0; i < sides; i++) {
			const double angle = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(sides);
			corners.push_back(
				Point{centerX + radiusX * std::cos(angle), centerY + radiusY * std::sin(angle)});
		}
		addClosedPolygon(shape, corners, radius);
		break;
	}
	}
	return shape;
}

void clearRaster(const RasterImage &image)
{
	if (!image.pixels || image.width <= 0 || image.height <= 0) {
		return;
	}
	for (int y = 0; y < image.height; y++) {
		std::fill_n(image.pixels + static_cast<size_t>(y) * static_cast<size_t>(image.stride),
			    static_cast<size_t>(image.width) * 4, uint8_t{0});
	}
}

void rasterizeStroke(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY)
{
	if (!image.pixels || image.width <= 0 || image.height <= 0) {
		return;
	}
	const StrokeShape shape = strokeShape(stroke, scaleX, scaleY);
	if (shape.capsules.empty() && shape.triangles.empty()) {
		return;
	}

	double minX = 1e300;
	double minY = 1e300;
	double maxX = -1e300;
	double maxY = -1e300;
	auto grow = [&](double x, double y, double margin) {
		minX = std::min(minX, x - margin);
		maxX = std::max(maxX, x + margin);
		minY = std::min(minY, y - margin);
		maxY = std::max(maxY, y + margin);
	};
	for (const Capsule &capsule : shape.capsules) {
		grow(capsule.x0, capsule.y0, capsule.radius + 1.0);
		grow(capsule.x1, capsule.y1, capsule.radius + 1.0);
	}
	for (const FilledTriangle &triangle : shape.triangles) {
		for (int i = 0; i < 3; i++) {
			grow(triangle.x[i], triangle.y[i], 1.0);
		}
	}
	int left = 0;
	int right = 0;
	int top = 0;
	int bottom = 0;
	pixelSpan(minX, maxX, 0, image.width, left, right);
	pixelSpan(minY, maxY, 0, image.height, top, bottom);
	if (left >= right || top >= bottom) {
		return;
	}

	/* cobertura do traço inteiro (0..255): a maior entre as peças em cada pixel */
	const int maskWidth = right - left;
	std::vector<uint8_t> mask(static_cast<size_t>(maskWidth) * static_cast<size_t>(bottom - top), 0);
	auto cover = [&](int x, int y, double coverage) {
		if (coverage <= 0.0) {
			return;
		}
		const uint8_t value = static_cast<uint8_t>(std::lround(std::min(coverage, 1.0) * 255.0));
		uint8_t &cell = mask[static_cast<size_t>(y - top) * static_cast<size_t>(maskWidth) +
				     static_cast<size_t>(x - left)];
		cell = std::max(cell, value);
	};
	for (const Capsule &capsule : shape.capsules) {
		int x0 = 0;
		int x1 = 0;
		int y0 = 0;
		int y1 = 0;
		pixelSpan(std::min(capsule.x0, capsule.x1) - capsule.radius - 1.0,
			  std::max(capsule.x0, capsule.x1) + capsule.radius + 1.0, left, right, x0, x1);
		pixelSpan(std::min(capsule.y0, capsule.y1) - capsule.radius - 1.0,
			  std::max(capsule.y0, capsule.y1) + capsule.radius + 1.0, top, bottom, y0, y1);
		for (int y = y0; y < y1; y++) {
			for (int x = x0; x < x1; x++) {
				cover(x, y, capsule.radius - capsuleDistance(x + 0.5, y + 0.5, capsule) + 0.5);
			}
		}
	}
	for (const FilledTriangle &triangle : shape.triangles) {
		int x0 = 0;
		int x1 = 0;
		int y0 = 0;
		int y1 = 0;
		pixelSpan(std::min({triangle.x[0], triangle.x[1], triangle.x[2]}) - 1.0,
			  std::max({triangle.x[0], triangle.x[1], triangle.x[2]}) + 1.0, left, right, x0, x1);
		pixelSpan(std::min({triangle.y[0], triangle.y[1], triangle.y[2]}) - 1.0,
			  std::max({triangle.y[0], triangle.y[1], triangle.y[2]}) + 1.0, top, bottom, y0, y1);
		for (int y = y0; y < y1; y++) {
			for (int x = x0; x < x1; x++) {
				cover(x, y, triangleDepth(x + 0.5, y + 0.5, triangle) + 0.5);
			}
		}
	}

	const double opacity = strokeOpacity(stroke);
	const int blue = stroke.color.b;
	const int green = stroke.color.g;
	const int red = stroke.color.r;
	for (int y = top; y < bottom; y++) {
		uint8_t *row = image.pixels + static_cast<size_t>(y) * static_cast<size_t>(image.stride);
		const uint8_t *maskRow = mask.data() + static_cast<size_t>(y - top) * static_cast<size_t>(maskWidth);
		for (int x = left; x < right; x++) {
			const int value = maskRow[x - left];
			if (value == 0) {
				continue;
			}
			const int alpha = static_cast<int>(std::lround(value * opacity));
			if (alpha <= 0) {
				continue;
			}
			uint8_t *pixel = row + static_cast<size_t>(x) * 4;
			pixel[0] = blendChannel(blue, pixel[0], alpha);
			pixel[1] = blendChannel(green, pixel[1], alpha);
			pixel[2] = blendChannel(red, pixel[2], alpha);
			pixel[3] = static_cast<uint8_t>(alpha + (pixel[3] * (255 - alpha) + 127) / 255);
		}
	}
}

} // namespace voltz
