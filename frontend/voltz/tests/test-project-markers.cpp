#include <doctest.h>

#include <voltz/core/project-markers.hpp>

#include <string>
#include <vector>

TEST_CASE("serializeMarkers sem marcadores grava lista vazia")
{
	CHECK(voltz::serializeMarkers({}) == "[]\n");
}

TEST_CASE("marcadores: ida e volta mantém ordem, tempo e nome com acento")
{
	const std::vector<voltz::Marker> markers = {{12345, "Marcador 1"}, {67890, "Introdução"}};
	const std::string json = voltz::serializeMarkers(markers);
	CHECK(json.find("\"t_ms\": 12345") != std::string::npos);
	CHECK(json.find("\"nome\": \"Marcador 1\"") != std::string::npos);
	std::vector<voltz::Marker> back;
	REQUIRE(voltz::parseMarkers(json, back));
	REQUIRE(back.size() == 2);
	CHECK(back[0].tMs == 12345);
	CHECK(back[1].tMs == 67890);
	CHECK(back[1].name == "Introdução");
}

TEST_CASE("parseMarkers ignora itens inválidos e recusa o que não é lista")
{
	std::vector<voltz::Marker> out;
	REQUIRE(voltz::parseMarkers(R"([{"t_ms":5,"nome":"A"},{"x":1},7,{"t_ms":9}])", out));
	REQUIRE(out.size() == 1);
	CHECK(out[0].name == "A");
	CHECK_FALSE(voltz::parseMarkers("{}", out));
	CHECK_FALSE(voltz::parseMarkers("nada", out));
}
