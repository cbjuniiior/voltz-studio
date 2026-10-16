#include <doctest.h>

#include <voltz/core/project-naming.hpp>

#include <set>
#include <string>

TEST_CASE("sanitizeProjectName troca caracteres proibidos no Windows por _")
{
	CHECK(voltz::sanitizeProjectName("Aula 03", "Gravação") == "Aula 03");
	CHECK(voltz::sanitizeProjectName("a<b>c:d\"e/f\\g|h?i*j", "x") == "a_b_c_d_e_f_g_h_i_j");
	CHECK(voltz::sanitizeProjectName("tab\there", "x") == "tab_here");
	CHECK(voltz::sanitizeProjectName("Revisão · ç", "x") == "Revisão · ç");
}

TEST_CASE("sanitizeProjectName tira espaços e pontos das pontas e usa o padrão se sobrar nada")
{
	CHECK(voltz::sanitizeProjectName("  Aula final.  ", "x") == "Aula final");
	CHECK(voltz::sanitizeProjectName(" ... ", "Gravação") == "Gravação");
	CHECK(voltz::sanitizeProjectName("", "Gravação") == "Gravação");
}

TEST_CASE("sanitizeProjectName corta no limite de bytes sem quebrar UTF-8")
{
	std::string longName;
	for (int i = 0; i < 100; i++) {
		longName += "ç"; // 2 bytes
	}
	const std::string cut = voltz::sanitizeProjectName(longName, "x", 81);
	CHECK(cut.size() == 80);
	CHECK(cut.substr(78) == "ç");
}

TEST_CASE("projectFolderName segue 'AAAA-MM-DD HHhMM · Nome'")
{
	CHECK(voltz::projectFolderName({2026, 10, 5, 9, 7}, "Aula 03") == "2026-10-05 09h07 · Aula 03");
	CHECK(voltz::projectFolderName({2026, 12, 31, 23, 59}, "X") == "2026-12-31 23h59 · X");
}

TEST_CASE("uniqueFolderName acrescenta (2), (3)… só quando o nome já existe")
{
	const std::set<std::string> taken = {"x", "x (2)"};
	auto exists = [&taken](const std::string &name) {
		return taken.count(name) > 0;
	};
	CHECK(voltz::uniqueFolderName("y", exists) == "y");
	CHECK(voltz::uniqueFolderName("x", exists) == "x (3)");
}
