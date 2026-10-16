#include <doctest.h>

#include <voltz/core/live-protocol.hpp>
#include <voltz/core/mouse-effects.hpp>

#include <cstdint>
#include <string>
#include <vector>

using voltz::MouseButton;
using voltz::MouseStyle;
using voltz::Ripple;

TEST_CASE("haloGeometry: disco cheio e anel com espessura mínima de 2 px")
{
	voltz::HaloStyle halo;
	voltz::HaloGeometry geometry = voltz::haloGeometry(halo);
	CHECK(geometry.diameter == doctest::Approx(46.0));
	CHECK(geometry.innerRadius == doctest::Approx(0.0));
	halo.shape = voltz::HaloShape::Ring;
	geometry = voltz::haloGeometry(halo);
	CHECK(geometry.innerRadius == doctest::Approx(23.0 - 5.52));
	halo.size = 12;
	CHECK(voltz::haloGeometry(halo).innerRadius == doctest::Approx(4.0));
}

TEST_CASE("rippleAt: a onda cresce com ease-out, some aos 450 ms e tem raio mínimo")
{
	MouseStyle style;
	const voltz::RippleFrame start = voltz::rippleAt(style, 0);
	CHECK(start.visible);
	CHECK(start.radius == doctest::Approx(12.42));
	CHECK(start.thickness == doctest::Approx(4.968));
	CHECK(start.alpha == doctest::Approx(0.9));
	const voltz::RippleFrame half = voltz::rippleAt(style, voltz::kRippleDurationNs / 2);
	CHECK(half.radius == doctest::Approx(37.7775));
	CHECK(half.alpha == doctest::Approx(0.45));
	CHECK_FALSE(voltz::rippleAt(style, voltz::kRippleDurationNs).visible);
	style.halo.size = 12;
	CHECK(voltz::rippleAt(style, voltz::kRippleDurationNs - 1).radius == doctest::Approx(21.6));
}

TEST_CASE("rippleAt: clique desligado não desenha; idade negativa conta como zero")
{
	MouseStyle style;
	style.click.active = false;
	CHECK_FALSE(voltz::rippleAt(style, 0).visible);
	style.click.active = true;
	CHECK(voltz::rippleAt(style, -5000000).radius == doctest::Approx(12.42));
}

TEST_CASE("rippleColor: cor própria só no botão direito")
{
	const voltz::ClickStyle click;
	CHECK(voltz::rippleColor(click, MouseButton::Right) == click.rightColor);
	CHECK(voltz::rippleColor(click, MouseButton::Left) == click.color);
	CHECK(voltz::rippleColor(click, MouseButton::Middle) == click.color);
}

TEST_CASE("pushRipple guarda no máximo 16 e pruneRipples tira as ondas vencidas")
{
	std::vector<Ripple> ripples;
	for (int i = 0; i < 20; i++) {
		voltz::pushRipple(ripples, Ripple{i, 0, MouseButton::Left, static_cast<int64_t>(i) * 1000000});
	}
	REQUIRE(ripples.size() == voltz::kMaxRipples);
	CHECK(ripples.front().x == 4);
	CHECK(ripples.back().x == 19);
	voltz::pruneRipples(ripples, 19 * 1000000 + voltz::kRippleDurationNs - 5000000);
	REQUIRE(ripples.size() == 5);
	CHECK(ripples.front().x == 15);
}

TEST_CASE("spotlightGeometry: borda suave de 12% do raio, no mínimo 8 px")
{
	voltz::SpotlightStyle spotlight;
	const voltz::SpotlightGeometry geometry = voltz::spotlightGeometry(spotlight);
	CHECK(geometry.radius == doctest::Approx(220.0));
	CHECK(geometry.feather == doctest::Approx(26.4));
	CHECK(geometry.dim == doctest::Approx(0.55));
	spotlight.radius = 60;
	CHECK(voltz::spotlightGeometry(spotlight).feather == doctest::Approx(8.0));
}

TEST_CASE("protocolo frontend → plugin: códigos de botão e nomes")
{
	CHECK(voltz::live::buttonCode(MouseButton::Left) == 1);
	CHECK(voltz::live::buttonCode(MouseButton::Right) == 2);
	CHECK(voltz::live::buttonCode(MouseButton::Middle) == 3);
	CHECK(voltz::live::buttonCode(MouseButton::None) == 1);
	CHECK(voltz::live::buttonFromCode(2) == MouseButton::Right);
	CHECK(voltz::live::buttonFromCode(3) == MouseButton::Middle);
	CHECK(voltz::live::buttonFromCode(1) == MouseButton::Left);
	CHECK(voltz::live::buttonFromCode(99) == MouseButton::Left);
	CHECK(std::string(voltz::live::kMouseSourceId) == "voltz_mouse_source");
	CHECK(std::string(voltz::live::kDeclPosition).find(voltz::live::kProcPosition) != std::string::npos);
	CHECK(std::string(voltz::live::kDeclClick).find(voltz::live::kProcClick) != std::string::npos);
	CHECK(std::string(voltz::live::kDeclStyle).find(voltz::live::kProcStyle) != std::string::npos);
	CHECK(std::string(voltz::live::kDeclWanted).find(voltz::live::kProcWanted) != std::string::npos);
}
