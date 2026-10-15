#include <doctest.h>

#include <voltz/core/voltz-version.hpp>

TEST_CASE("aboutLine credita o OBS Studio e a licença")
{
	const voltz::VersionInfo info{"Voltz Studio", "1.0.0", "32.2.2"};
	CHECK(voltz::aboutLine(info) == "Voltz Studio 1.0.0 · baseado no OBS Studio 32.2.2 (GPL-2.0)");
}

TEST_CASE("windowTitle sem projeto mostra produto e versão")
{
	const voltz::VersionInfo info{"Voltz Studio", "1.0.0", "32.2.2"};
	CHECK(voltz::windowTitle(info, "") == "Voltz Studio 1.0.0");
}

TEST_CASE("windowTitle com projeto prefixa o nome do projeto")
{
	const voltz::VersionInfo info{"Voltz Studio", "1.0.0", "32.2.2"};
	CHECK(voltz::windowTitle(info, "Aula 03") == "Aula 03 — Voltz Studio 1.0.0");
}
