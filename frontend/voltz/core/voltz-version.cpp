#include "voltz-version.hpp"

namespace voltz {

std::string aboutLine(const VersionInfo &info)
{
	return info.product + " " + info.productVersion + " · baseado no OBS Studio " + info.engineVersion +
	       " (GPL-2.0)";
}

std::string windowTitle(const VersionInfo &info, const std::string &projectName)
{
	const std::string base = info.product + " " + info.productVersion;
	if (projectName.empty()) {
		return base;
	}
	return projectName + " — " + base;
}

} // namespace voltz
