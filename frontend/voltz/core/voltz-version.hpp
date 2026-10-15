#pragma once

#include <string>

namespace voltz {

inline constexpr const char *kProjectUrl = "https://github.com/cbjuniiior/voltz-studio";
inline constexpr const char *kIssuesUrl = "https://github.com/cbjuniiior/voltz-studio/issues";

struct VersionInfo {
	std::string product;        // "Voltz Studio"
	std::string productVersion; // "1.0.0"
	std::string engineVersion;  // obs_get_version_string(), ex.: "32.2.2"
};

// "Voltz Studio 1.0.0 · <basedOnLabel> 32.2.2 (GPL-2.0)"; o rótulo vem traduzido
// (ex.: "baseado no OBS Studio" / "based on OBS Studio").
std::string aboutLine(const VersionInfo &info, const std::string &basedOnLabel);

// "Voltz Studio 1.0.0" ou, com projeto, "Aula 03 — Voltz Studio 1.0.0"
std::string windowTitle(const VersionInfo &info, const std::string &projectName);

} // namespace voltz
