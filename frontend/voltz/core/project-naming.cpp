#include "project-naming.hpp"

#include <cstdio>

namespace voltz {

namespace {

bool isForbidden(unsigned char c)
{
	if (c < 0x20 || c == 0x7F) {
		return true;
	}
	switch (c) {
	case '<':
	case '>':
	case ':':
	case '"':
	case '/':
	case '\\':
	case '|':
	case '?':
	case '*':
		return true;
	default:
		return false;
	}
}

std::string trimEnds(const std::string &text)
{
	const size_t first = text.find_first_not_of(" .");
	if (first == std::string::npos) {
		return {};
	}
	const size_t last = text.find_last_not_of(" .");
	return text.substr(first, last - first + 1);
}

} // namespace

std::string sanitizeProjectName(const std::string &name, const std::string &fallback, size_t maxBytes)
{
	std::string clean;
	clean.reserve(name.size());
	for (const char ch : name) {
		clean += isForbidden(static_cast<unsigned char>(ch)) ? '_' : ch;
	}
	clean = trimEnds(clean);
	if (clean.size() > maxBytes) {
		size_t cut = maxBytes;
		/* não corta no meio de um caractere UTF-8: recua enquanto o byte do corte for de continuação */
		while (cut > 0 && (static_cast<unsigned char>(clean[cut]) & 0xC0) == 0x80) {
			cut--;
		}
		clean = trimEnds(clean.substr(0, cut));
	}
	return clean.empty() ? fallback : clean;
}

std::string projectFolderName(const LocalDateTime &when, const std::string &safeName)
{
	char stamp[32];
	std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %02dh%02d", when.year, when.month, when.day, when.hour,
		      when.minute);
	return std::string(stamp) + " \xC2\xB7 " + safeName;
}

std::string uniqueFolderName(const std::string &base, const std::function<bool(const std::string &)> &exists)
{
	if (!exists(base)) {
		return base;
	}
	for (int n = 2; n < 1000; n++) {
		std::string candidate = base + " (" + std::to_string(n) + ")";
		if (!exists(candidate)) {
			return candidate;
		}
	}
	return base + " (1000)";
}

} // namespace voltz
