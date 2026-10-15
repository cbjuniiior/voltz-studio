#include <doctest.h>

#include <voltz/core/svg-recolor.hpp>

TEST_CASE("recolorSvg troca todas as ocorrências de currentColor")
{
	const std::string svg = R"(<svg fill="currentColor"><path stroke="currentColor" d="M0 0"/></svg>)";
	CHECK(voltz::recolorSvg(svg, "#D4F53C") == R"(<svg fill="#D4F53C"><path stroke="#D4F53C" d="M0 0"/></svg>)");
}

TEST_CASE("recolorSvg sem currentColor devolve o texto igual")
{
	const std::string svg = R"(<svg fill="#000"/>)";
	CHECK(voltz::recolorSvg(svg, "#FFFFFF") == svg);
}

TEST_CASE("recolorSvg com entrada vazia devolve vazio")
{
	CHECK(voltz::recolorSvg("", "#FFFFFF").empty());
}
