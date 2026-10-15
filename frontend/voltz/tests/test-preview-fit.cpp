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

TEST_CASE("fitCentered: proporção extrema que arredonda para altura zero retorna tudo zero")
{
	const voltz::FitRect r = voltz::fitCentered(10000, 1, 100, 100);
	CHECK(r.cx == 0);
	CHECK(r.cy == 0);
}

TEST_CASE("fitCentered: altura base zero ou altura da janela zero retornam tamanho zero")
{
	CHECK(voltz::fitCentered(1920, 0, 1280, 720).cx == 0);
	CHECK(voltz::fitCentered(1920, 1080, 1280, 0).cx == 0);
}

TEST_CASE("fitCentered: margem negativa é tratada como zero")
{
	const voltz::FitRect r = voltz::fitCentered(1920, 1080, 1280, 720, -5);
	CHECK(r.x == 0);
	CHECK(r.y == 0);
	CHECK(r.cx == 1280);
	CHECK(r.cy == 720);
}

TEST_CASE("fitCentered: resto ímpar do centramento é truncado")
{
	const voltz::FitRect r = voltz::fitCentered(1920, 1080, 1601, 720);
	CHECK(r.x == 160);
	CHECK(r.cx == 1280);
}

TEST_CASE("fitCentered: margem combinada com letterbox")
{
	const voltz::FitRect r = voltz::fitCentered(1920, 1080, 1620, 740, 10);
	CHECK(r.x == 170);
	CHECK(r.y == 10);
	CHECK(r.cx == 1280);
	CHECK(r.cy == 720);
}

TEST_CASE("fitCentered: margem enorme não estoura o int")
{
	CHECK(voltz::fitCentered(1920, 1080, 1280, 720, 2000000000).cx == 0);
}
