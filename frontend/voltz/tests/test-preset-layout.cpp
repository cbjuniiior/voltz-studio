#include <doctest.h>

#include <voltz/core/preset-layout.hpp>

using voltz::CanvasSize;
using voltz::LayoutRect;

namespace {
void checkRect(const LayoutRect &r, double x, double y, double w, double h)
{
	CHECK(r.x == doctest::Approx(x));
	CHECK(r.y == doctest::Approx(y));
	CHECK(r.w == doctest::Approx(w));
	CHECK(r.h == doctest::Approx(h));
}
} // namespace

TEST_CASE("fullCanvas cobre o canvas inteiro")
{
	checkRect(voltz::fullCanvas(CanvasSize{1920, 1080}), 0, 0, 1920, 1080);
}

TEST_CASE("tutorialCamera: quadrado de 24% da altura no canto inferior direito, margem de 3,5%")
{
	checkRect(voltz::tutorialCamera(CanvasSize{1920, 1080}), 1623, 783, 259, 259);
	checkRect(voltz::tutorialCamera(CanvasSize{1280, 720}), 1082, 522, 173, 173);
}

TEST_CASE("sideBySide: tela à esquerda até 70% da largura, câmera à direita, margem de 3% da altura")
{
	checkRect(voltz::sideBySideScreen(CanvasSize{1920, 1080}), 32, 32, 1296, 1016);
	checkRect(voltz::sideBySideCamera(CanvasSize{1920, 1080}), 1360, 32, 528, 1016);
	checkRect(voltz::sideBySideScreen(CanvasSize{1280, 720}), 22, 22, 863, 676);
	checkRect(voltz::sideBySideCamera(CanvasSize{1280, 720}), 907, 22, 351, 676);
}

TEST_CASE("sideBySide: tela e câmera não se sobrepõem e ficam dentro do canvas")
{
	const CanvasSize canvas{1920, 1080};
	const LayoutRect screen = voltz::sideBySideScreen(canvas);
	const LayoutRect camera = voltz::sideBySideCamera(canvas);
	CHECK(screen.x + screen.w < camera.x);
	CHECK(camera.x + camera.w <= canvas.width);
	CHECK(screen.y + screen.h <= canvas.height);
}
