#include <doctest.h>

#include <voltz/core/mouse-style.hpp>

#include <string>
#include <vector>

using voltz::HaloShape;
using voltz::MouseStyle;
using voltz::NamedMouseStyle;
using voltz::RgbColor;

TEST_CASE("estilo do mouse padrão é o da spec §5.2")
{
	const MouseStyle style;
	CHECK(voltz::mouseStyleToJson(style) ==
	      R"({"halo":{"ativo":true,"cor":"#D4F53C","tamanho":46,"opacidade":0.25,"forma":"cheio"},)"
	      R"("clique":{"ativo":true,"cor":"#D4F53C","cor_direito":"#4DA3FF"},)"
	      R"("holofote":{"ativo":false,"raio":220,"escurecer":0.55}})");
}

TEST_CASE("mouseStyleToJson e mouseStyleFromJson: ida e volta")
{
	MouseStyle style;
	style.halo.active = false;
	style.halo.color = RgbColor{0x12, 0xAB, 0xEF};
	style.halo.size = 80;
	style.halo.opacity = 0.6;
	style.halo.shape = HaloShape::Ring;
	style.click.active = false;
	style.click.color = RgbColor{1, 2, 3};
	style.click.rightColor = RgbColor{4, 5, 6};
	style.spotlight.active = true;
	style.spotlight.radius = 300;
	style.spotlight.dim = 0.7;
	MouseStyle back;
	REQUIRE(voltz::mouseStyleFromJson(voltz::mouseStyleToJson(style), back));
	CHECK_FALSE(back.halo.active);
	CHECK(back.halo.color == style.halo.color);
	CHECK(back.halo.size == 80);
	CHECK(back.halo.opacity == doctest::Approx(0.6));
	CHECK(back.halo.shape == HaloShape::Ring);
	CHECK_FALSE(back.click.active);
	CHECK(back.click.color == style.click.color);
	CHECK(back.click.rightColor == style.click.rightColor);
	CHECK(back.spotlight.active);
	CHECK(back.spotlight.radius == 300);
	CHECK(back.spotlight.dim == doctest::Approx(0.7));
}

TEST_CASE("mouseStyleFromJson tolera ausentes e tipos errados e limita as faixas")
{
	MouseStyle style;
	REQUIRE(voltz::mouseStyleFromJson(
		R"({"halo":{"tamanho":5,"opacidade":2,"cor":"verde","forma":"triangulo"},"holofote":{"raio":5000,"escurecer":0,"ativo":"sim"},"extra":1})",
		style));
	CHECK(style.halo.size == voltz::kHaloSizeMin);
	CHECK(style.halo.opacity == doctest::Approx(voltz::kHaloOpacityMax));
	CHECK(style.halo.color == voltz::HaloStyle{}.color);
	CHECK(style.halo.shape == HaloShape::Filled);
	CHECK(style.spotlight.radius == voltz::kSpotlightRadiusMax);
	CHECK(style.spotlight.dim == doctest::Approx(voltz::kSpotlightDimMin));
	CHECK_FALSE(style.spotlight.active);
	CHECK(style.click.active);

	MouseStyle big;
	big.halo.size = 999;
	big.halo.opacity = -1.0;
	big.spotlight.radius = 10;
	big.spotlight.dim = 1.0;
	big = voltz::clampMouseStyle(big);
	CHECK(big.halo.size == voltz::kHaloSizeMax);
	CHECK(big.halo.opacity == doctest::Approx(voltz::kHaloOpacityMin));
	CHECK(big.spotlight.radius == voltz::kSpotlightRadiusMin);
	CHECK(big.spotlight.dim == doctest::Approx(voltz::kSpotlightDimMax));
}

TEST_CASE("mouseStyleFromJson recusa JSON inválido e devolve o padrão")
{
	MouseStyle style;
	style.halo.size = 99;
	CHECK_FALSE(voltz::mouseStyleFromJson("", style));
	CHECK(style.halo.size == 46);
	style.halo.size = 99;
	CHECK_FALSE(voltz::mouseStyleFromJson("[1,2]", style));
	CHECK(style.halo.size == 46);
	CHECK_FALSE(voltz::mouseStyleFromJson("{", style));
}

TEST_CASE("cor #RRGGBB: lê maiúsculas e minúsculas, escreve maiúsculas, recusa o resto")
{
	RgbColor color;
	CHECK(voltz::parseHexColor("#d4f53c", color));
	CHECK(color.r == 0xD4);
	CHECK(color.g == 0xF5);
	CHECK(color.b == 0x3C);
	CHECK(voltz::formatHexColor(color) == "#D4F53C");
	const RgbColor before = color;
	CHECK_FALSE(voltz::parseHexColor("D4F53C", color));
	CHECK_FALSE(voltz::parseHexColor("#D4F53", color));
	CHECK_FALSE(voltz::parseHexColor("#D4F53G", color));
	CHECK(color == before);
	CHECK(voltz::formatHexColor(RgbColor{0, 10, 255}) == "#000AFF");
}

TEST_CASE("estilos salvos: JSON ida e volta; itens sem nome são ignorados")
{
	std::vector<NamedMouseStyle> styles(2);
	styles[0].name = "Aula";
	styles[0].style.halo.size = 60;
	styles[1].name = "Holofote forte";
	styles[1].style.spotlight.active = true;
	styles[1].style.spotlight.dim = 0.8;
	const std::string json = voltz::savedStylesToJson(styles);
	CHECK(json.rfind(R"([{"nome":"Aula","estilo":{"halo":)", 0) == 0);
	const std::vector<NamedMouseStyle> back = voltz::savedStylesFromJson(json);
	REQUIRE(back.size() == 2);
	CHECK(back[0].name == "Aula");
	CHECK(back[0].style.halo.size == 60);
	CHECK(back[1].name == "Holofote forte");
	CHECK(back[1].style.spotlight.active);
	CHECK(back[1].style.spotlight.dim == doctest::Approx(0.8));
	CHECK(voltz::savedStylesFromJson(R"([{"estilo":{}},{"nome":"  ","estilo":{}},3,{"nome":"Só nome"}])").size() ==
	      1);
	CHECK(voltz::savedStylesFromJson("lixo").empty());
}

TEST_CASE("upsertSavedStyle troca pelo nome, ignora nome vazio e guarda no máximo 20")
{
	std::vector<NamedMouseStyle> styles;
	MouseStyle a;
	a.halo.size = 50;
	MouseStyle b;
	b.halo.size = 70;
	styles = voltz::upsertSavedStyle(styles, "  Aula ", a);
	styles = voltz::upsertSavedStyle(styles, "Outro", a);
	styles = voltz::upsertSavedStyle(styles, "Aula", b);
	REQUIRE(styles.size() == 2);
	CHECK(styles[0].name == "Aula");
	CHECK(styles[0].style.halo.size == 70);
	CHECK(styles[1].name == "Outro");
	CHECK(voltz::upsertSavedStyle(styles, "   ", a).size() == 2);
	for (int i = 0; i < 25; i++) {
		styles = voltz::upsertSavedStyle(styles, "E" + std::to_string(i), a);
	}
	CHECK(styles.size() == voltz::kMaxSavedStyles);
	CHECK(styles.front().name == "E5");
	CHECK(styles.back().name == "E24");
}

TEST_CASE("removeSavedStyle tira só o nome pedido")
{
	std::vector<NamedMouseStyle> styles(2);
	styles[0].name = "A";
	styles[1].name = "B";
	const std::vector<NamedMouseStyle> left = voltz::removeSavedStyle(styles, " A ");
	REQUIRE(left.size() == 1);
	CHECK(left[0].name == "B");
	CHECK(voltz::removeSavedStyle(styles, "Z").size() == 2);
}
