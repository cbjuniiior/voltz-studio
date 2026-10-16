#include <doctest.h>

#include <voltz/core/atomic-file.hpp>

#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

// Pasta limpa em %TEMP%\voltz-core-tests\<nome> (nome em UTF-8).
fs::path freshDir(const char *name)
{
	const fs::path dir = fs::temp_directory_path() / "voltz-core-tests" / fs::u8path(name);
	std::error_code ec;
	fs::remove_all(dir, ec);
	fs::create_directories(dir);
	return dir;
}

std::string utf8(const fs::path &path)
{
	return path.u8string();
}

} // namespace

TEST_CASE("writeFileAtomically cria o arquivo e não deixa o temporário")
{
	const fs::path dir = freshDir("atomic-new");
	const fs::path file = dir / "projeto.voltz";
	std::string error;
	REQUIRE(voltz::writeFileAtomically(utf8(file), "{\"a\":1}\n", &error));
	CHECK(error.empty());
	std::string back;
	REQUIRE(voltz::readFileText(utf8(file), back));
	CHECK(back == "{\"a\":1}\n");
	CHECK_FALSE(fs::exists(dir / "projeto.voltz.tmp"));
}

TEST_CASE("writeFileAtomically substitui o conteúdo anterior por inteiro")
{
	const fs::path file = freshDir("atomic-replace") / "m.json";
	REQUIRE(voltz::writeFileAtomically(utf8(file), std::string(4096, 'x')));
	REQUIRE(voltz::writeFileAtomically(utf8(file), "curto"));
	std::string back;
	REQUIRE(voltz::readFileText(utf8(file), back));
	CHECK(back == "curto");
}

TEST_CASE("writeFileAtomically aceita caminho com acentos e ponto médio")
{
	const fs::path file = freshDir("2026-10-15 15h30 · Ação") / "marcadores.json";
	REQUIRE(voltz::writeFileAtomically(utf8(file), "[]\n"));
	std::string back;
	REQUIRE(voltz::readFileText(utf8(file), back));
	CHECK(back == "[]\n");
}

TEST_CASE("writeFileAtomically falha sem criar nada quando a pasta não existe")
{
	const fs::path file = freshDir("atomic-missing") / "nao-existe" / "m.json";
	std::string error;
	CHECK_FALSE(voltz::writeFileAtomically(utf8(file), "x", &error));
	CHECK_FALSE(error.empty());
	CHECK_FALSE(fs::exists(file));
}

TEST_CASE("readFileText devolve falso para arquivo inexistente")
{
	std::string out = "intacto";
	CHECK_FALSE(voltz::readFileText(utf8(freshDir("atomic-read") / "nada.txt"), out));
	CHECK(out == "intacto");
}
