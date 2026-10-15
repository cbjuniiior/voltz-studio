#include "recording-settings.hpp"

#include <algorithm>

namespace voltz {

std::string obsRecQuality(RecordingQuality quality)
{
	return quality == RecordingQuality::Compact ? "Small" : "HQ";
}

RecordingQuality qualityFromObs(const std::string &value)
{
	return value == "Small" ? RecordingQuality::Compact : RecordingQuality::High;
}

std::string obsRecFormat(RecordingFormat format)
{
	switch (format) {
	case RecordingFormat::Mkv:
		return "mkv";
	case RecordingFormat::Mov:
		return "hybrid_mov";
	case RecordingFormat::Mp4:
		break;
	}
	return "hybrid_mp4";
}

RecordingFormat formatFromObs(const std::string &value)
{
	if (value == "mkv") {
		return RecordingFormat::Mkv;
	}
	if (value == "mov" || value == "hybrid_mov" || value == "fragmented_mov") {
		return RecordingFormat::Mov;
	}
	return RecordingFormat::Mp4;
}

std::string pickRecEncoder(const std::vector<std::string> &availableEncoderIds)
{
	auto has = [&availableEncoderIds](const char *id) {
		return std::find(availableEncoderIds.begin(), availableEncoderIds.end(), id) !=
		       availableEncoderIds.end();
	};
	if (has("h264_texture_amf")) {
		return "amd";
	}
	if (has("ffmpeg_nvenc")) {
		return "nvenc";
	}
	if (has("obs_qsv11")) {
		return "qsv";
	}
	return "x264";
}

} // namespace voltz
