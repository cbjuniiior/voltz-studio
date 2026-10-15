#include "time-format.hpp"

#include <cstdio>

namespace voltz {

std::string formatClock(int64_t milliseconds)
{
	if (milliseconds < 0)
		milliseconds = 0;
	const int64_t totalSeconds = milliseconds / 1000;
	const long long hours = static_cast<long long>(totalSeconds / 3600);
	const int minutes = static_cast<int>((totalSeconds / 60) % 60);
	const int seconds = static_cast<int>(totalSeconds % 60);

	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%02lld:%02d:%02d", hours, minutes, seconds);
	return buffer;
}

} // namespace voltz
