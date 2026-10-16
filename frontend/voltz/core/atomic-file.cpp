#include "atomic-file.hpp"

#include <cstdio>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#endif

namespace voltz {

namespace {

#ifdef _WIN32
std::wstring widen(const std::string &utf8)
{
	if (utf8.empty()) {
		return {};
	}
	const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
	std::wstring wide(static_cast<size_t>(size > 0 ? size : 0), L'\0');
	if (size > 0) {
		MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), size);
	}
	return wide;
}
#endif

FILE *openFile(const std::string &utf8Path, bool write)
{
#ifdef _WIN32
	return _wfopen(widen(utf8Path).c_str(), write ? L"wb" : L"rb");
#else
	return std::fopen(utf8Path.c_str(), write ? "wb" : "rb");
#endif
}

void removeFile(const std::string &utf8Path)
{
#ifdef _WIN32
	_wremove(widen(utf8Path).c_str());
#else
	std::remove(utf8Path.c_str());
#endif
}

void setError(std::string *error, const std::string &message)
{
	if (error) {
		*error = message;
	}
}

} // namespace

bool writeFileAtomically(const std::string &utf8Path, const std::string &content, std::string *error)
{
	const std::string tmp = utf8Path + ".tmp";
	FILE *file = openFile(tmp, true);
	if (!file) {
		setError(error, "nao foi possivel criar " + tmp);
		return false;
	}
	const size_t written = content.empty() ? 0 : std::fwrite(content.data(), 1, content.size(), file);
	bool ok = written == content.size() && std::fflush(file) == 0;
#ifdef _WIN32
	ok = ok && _commit(_fileno(file)) == 0;
#endif
	ok = (std::fclose(file) == 0) && ok;
	if (!ok) {
		removeFile(tmp);
		setError(error, "falha ao gravar " + tmp);
		return false;
	}
#ifdef _WIN32
	if (!MoveFileExW(widen(tmp).c_str(), widen(utf8Path).c_str(),
			 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		const unsigned long code = GetLastError();
		removeFile(tmp);
		setError(error, "falha ao substituir " + utf8Path + " (erro " + std::to_string(code) + ")");
		return false;
	}
#else
	if (std::rename(tmp.c_str(), utf8Path.c_str()) != 0) {
		removeFile(tmp);
		setError(error, "falha ao substituir " + utf8Path);
		return false;
	}
#endif
	return true;
}

bool readFileText(const std::string &utf8Path, std::string &out)
{
	FILE *file = openFile(utf8Path, false);
	if (!file) {
		return false;
	}
	std::string content;
	char buffer[8192];
	size_t count = 0;
	while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
		content.append(buffer, count);
	}
	const bool ok = std::ferror(file) == 0;
	std::fclose(file);
	if (ok) {
		out = std::move(content);
	}
	return ok;
}

} // namespace voltz
