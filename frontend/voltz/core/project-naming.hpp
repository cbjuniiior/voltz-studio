#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace voltz {

struct LocalDateTime {
	int year = 0;
	int month = 0;
	int day = 0;
	int hour = 0;
	int minute = 0;
};

// Nome seguro para pasta no Windows: troca <>:"/\|?* e caracteres de controle por "_", tira espaços e pontos das
// pontas e corta em `maxBytes` bytes sem quebrar UTF-8. Vazio vira `fallback`. Nomes reservados (CON, NUL…) não são
// tratados porque a pasta sempre começa pela data.
std::string sanitizeProjectName(const std::string &name, const std::string &fallback, size_t maxBytes = 80);

// "2026-10-15 15h30 · Aula 03" (spec §5.1). `safeName` já saneado.
std::string projectFolderName(const LocalDateTime &when, const std::string &safeName);

// `base` se `exists(base)` for falso; senão "base (2)", "base (3)"… (até 999).
std::string uniqueFolderName(const std::string &base, const std::function<bool(const std::string &)> &exists);

} // namespace voltz
