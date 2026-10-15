#include "preset-layout.hpp"

#include <cmath>

namespace voltz {

LayoutRect fullCanvas(CanvasSize canvas)
{
	return {0, 0, canvas.width, canvas.height};
}

LayoutRect tutorialCamera(CanvasSize canvas)
{
	const double side = std::round(canvas.height * 0.24);
	const double margin = std::round(canvas.height * 0.035);
	return {canvas.width - margin - side, canvas.height - margin - side, side, side};
}

namespace {

struct Split {
	double margin;
	double halfGap;
	double divider;
};

Split sideBySideSplit(CanvasSize canvas)
{
	const double margin = std::round(canvas.height * 0.03);
	return {margin, std::floor(margin / 2), std::round(canvas.width * 0.70)};
}

} // namespace

LayoutRect sideBySideScreen(CanvasSize canvas)
{
	const Split s = sideBySideSplit(canvas);
	return {s.margin, s.margin, s.divider - s.margin - s.halfGap, canvas.height - 2 * s.margin};
}

LayoutRect sideBySideCamera(CanvasSize canvas)
{
	const Split s = sideBySideSplit(canvas);
	const double x = s.divider + s.halfGap;
	return {x, s.margin, canvas.width - s.margin - x, canvas.height - 2 * s.margin};
}

} // namespace voltz
