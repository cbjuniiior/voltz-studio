#include "recording-files.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace voltz {

bool isRecordingFile(const std::string &fileName)
{
	const size_t dot = fileName.find_last_of('.');
	if (dot == std::string::npos || dot == 0 || dot + 1 >= fileName.size()) {
		return false;
	}
	std::string ext = fileName.substr(dot + 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
	static const char *const kExtensions[] = {"mp4", "mkv", "mov", "flv", "ts", "avi"};
	return std::any_of(std::begin(kExtensions), std::end(kExtensions),
			   [&ext](const char *candidate) { return ext == candidate; });
}

std::string formatFileSize(uint64_t bytes, char decimalSeparator)
{
	if (bytes < 1024) {
		return std::to_string(bytes) + " B";
	}
	static const char *const kUnits[] = {"KB", "MB", "GB"};
	double value = double(bytes) / 1024.0;
	int unit = 0;
	while (value >= 1024.0 && unit < 2) {
		value /= 1024.0;
		unit++;
	}
	char buffer[48];
	std::snprintf(buffer, sizeof(buffer), "%.1f", value);
	std::string text = buffer;
	std::replace(text.begin(), text.end(), '.', decimalSeparator);
	return text + " " + kUnits[unit];
}

} // namespace voltz
