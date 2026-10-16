#include "disk-space.hpp"

#include <cmath>

namespace voltz {

DiskLevel diskLevel(uint64_t freeBytes)
{
	if (freeBytes < kDiskCriticalBytes) {
		return DiskLevel::Critical;
	}
	if (freeBytes < kDiskLowBytes) {
		return DiskLevel::Low;
	}
	return DiskLevel::Ok;
}

uint64_t estimateBytesPerMinute(const RateInput &input)
{
	double videoBitsPerSecond = 0.0;
	if (input.streamEncoder) {
		videoBitsPerSecond = static_cast<double>(input.streamKbps) * 1000.0;
	} else {
		const double bitsPerPixel = input.compact ? 0.05 : 0.12;
		videoBitsPerSecond =
			static_cast<double>(input.width) * static_cast<double>(input.height) * input.fps * bitsPerPixel;
	}
	const double audioBitsPerSecond =
		static_cast<double>(input.audioTracks) * static_cast<double>(input.audioKbps) * 1000.0;
	const double bytesPerMinute = (videoBitsPerSecond + audioBitsPerSecond) / 8.0 * 60.0;
	return bytesPerMinute > 0.0 ? static_cast<uint64_t>(std::llround(bytesPerMinute)) : 0;
}

uint64_t measuredBytesPerMinute(uint64_t bytes, int64_t elapsedMs)
{
	if (elapsedMs < 10000) {
		return 0;
	}
	return bytes * 60000ULL / static_cast<uint64_t>(elapsedMs);
}

std::string formatRate(uint64_t bytesPerMinute, char decimalSeparator)
{
	if (bytesPerMinute >= kGiB) {
		const long long tenths =
			std::llround(static_cast<double>(bytesPerMinute) / static_cast<double>(kGiB) * 10.0);
		return "~" + std::to_string(tenths / 10) + decimalSeparator + std::to_string(tenths % 10) + " GB/min";
	}
	const long long mebibytes = std::llround(static_cast<double>(bytesPerMinute) / (1024.0 * 1024.0));
	return "~" + std::to_string(mebibytes) + " MB/min";
}

std::string formatFreeSpace(uint64_t bytes, char decimalSeparator)
{
	if (bytes >= 10 * kGiB) {
		return std::to_string(bytes / kGiB) + " GB";
	}
	if (bytes >= kGiB) {
		const uint64_t tenths = bytes * 10 / kGiB;
		return std::to_string(tenths / 10) + decimalSeparator + std::to_string(tenths % 10) + " GB";
	}
	return std::to_string(bytes / (1024ULL * 1024ULL)) + " MB";
}

} // namespace voltz
