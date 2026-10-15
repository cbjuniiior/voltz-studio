#include <doctest.h>

#include <voltz/core/preview-fit.hpp>

TEST_CASE("fitCentered: mesma proporção ocupa a janela inteira")
{
	const voltz::FitRect r = voltz::fitCentered(1920, 1080, 1280, 720);
	CHECK(r.x == 0);
	CHECK(r.y == 0);
	CHECK(r.cx == 1280);
	CHECK(r.cy == 720);
	CHECK(r.scale == doctest::Approx(1280.0 / 1920.0));
}

TEST_CASE("fitCentered: janela mais larga centraliza na horizontal")
{
	const voltz::FitRect r = voltz::fitCentered(1920, 1080, 1600, 720);
	CHECK(r.x == 160);
	CHECK(r.y == 0);
	CHECK(r.cx == 1280);
	CHECK(r.cy == 720);
}

TEST_CASE("fitCentered: janela mais alta centraliza na vertical")
{
	const voltz::FitRect r = voltz::fitCentered(1920, 1080, 1280, 1000);
	CHECK(r.x == 0);
	CHECK(r.y == 140);
	CHECK(r.cx == 1280);
	CHECK(r.cy == 720);
}

TEST_CASE("fitCentered: margem é respeitada em todas as bordas")
{
	const voltz::FitRect r = voltz::fitCentered(1920, 1080, 1300, 740, 10);
	CHECK(r.x == 10);
	CHECK(r.y == 10);
	CHECK(r.cx == 1280);
	CHECK(r.cy == 720);
}

TEST_CASE("fitCentered: entradas inválidas retornam tamanho zero")
{
	CHECK(voltz::fitCentered(0, 1080, 1280, 720).cx == 0);
	CHECK(voltz::fitCentered(1920, 1080, 0, 720).cx == 0);
	CHECK(voltz::fitCentered(1920, 1080, 15, 15, 10).cx == 0);
}
