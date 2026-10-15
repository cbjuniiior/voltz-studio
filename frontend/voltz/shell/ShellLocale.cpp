#include "ShellLocale.hpp"

#include <OBSApp.hpp>
#include <utility/platform.hpp>

#include <util/text-lookup.h>

#include <string>

namespace voltz {

namespace {

bool addLocaleFile(const std::string &language)
{
	std::string path;
	const std::string relative = "locale/voltz-" + language + ".ini";
	if (!GetDataFilePath(relative.c_str(), path)) {
		return false;
	}
	return text_lookup_add(App()->GetTextLookup(), path.c_str());
}

} // namespace

void loadShellLocale()
{
	static bool loaded = false;
	if (loaded) {
		return;
	}
	loaded = true;

	if (!addLocaleFile("en-US")) {
		blog(LOG_WARNING, "[voltz] locale/voltz-en-US.ini nao encontrado");
	}
	const std::string language = App()->GetLocale();
	if (language != "en-US" && !addLocaleFile(language)) {
		blog(LOG_INFO, "[voltz] sem traducao da casca para '%s'; usando ingles", language.c_str());
	}
}

} // namespace voltz
