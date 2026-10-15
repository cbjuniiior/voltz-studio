#pragma once

#include <string>
#include <vector>

namespace voltz {

enum class RecordingQuality { High, Compact };
enum class RecordingFormat { Mp4, Mkv, Mov };

// SimpleOutput/RecQuality: High = "HQ", Compact = "Small". Qualquer outro valor do OBS ("Stream",
// "Lossless", vazio) é apresentado como High.
std::string obsRecQuality(RecordingQuality quality);
RecordingQuality qualityFromObs(const std::string &value);

// SimpleOutput/RecFormat2: Mp4 = "hybrid_mp4" (resistente a falhas), Mkv = "mkv", Mov = "hybrid_mov".
std::string obsRecFormat(RecordingFormat format);
RecordingFormat formatFromObs(const std::string &value);

// SimpleOutput/RecEncoder a partir dos ids de encoder registrados no libobs:
// "h264_texture_amf" → "amd", "ffmpeg_nvenc" → "nvenc", "obs_qsv11" → "qsv", senão "x264".
std::string pickRecEncoder(const std::vector<std::string> &availableEncoderIds);

} // namespace voltz
