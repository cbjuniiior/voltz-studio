#include <doctest.h>

#include <voltz/core/recording-settings.hpp>

using voltz::RecordingFormat;
using voltz::RecordingQuality;

TEST_CASE("qualidade ↔ SimpleOutput/RecQuality")
{
	CHECK(voltz::obsRecQuality(RecordingQuality::High) == "HQ");
	CHECK(voltz::obsRecQuality(RecordingQuality::Compact) == "Small");
	CHECK(voltz::qualityFromObs("Small") == RecordingQuality::Compact);
	CHECK(voltz::qualityFromObs("HQ") == RecordingQuality::High);
	CHECK(voltz::qualityFromObs("Stream") == RecordingQuality::High);
	CHECK(voltz::qualityFromObs("Lossless") == RecordingQuality::High);
	CHECK(voltz::qualityFromObs("") == RecordingQuality::High);
}

TEST_CASE("formato ↔ SimpleOutput/RecFormat2")
{
	CHECK(voltz::obsRecFormat(RecordingFormat::Mp4) == "hybrid_mp4");
	CHECK(voltz::obsRecFormat(RecordingFormat::Mkv) == "mkv");
	CHECK(voltz::obsRecFormat(RecordingFormat::Mov) == "hybrid_mov");
	CHECK(voltz::formatFromObs("mkv") == RecordingFormat::Mkv);
	CHECK(voltz::formatFromObs("mov") == RecordingFormat::Mov);
	CHECK(voltz::formatFromObs("hybrid_mov") == RecordingFormat::Mov);
	CHECK(voltz::formatFromObs("fragmented_mov") == RecordingFormat::Mov);
	CHECK(voltz::formatFromObs("hybrid_mp4") == RecordingFormat::Mp4);
	CHECK(voltz::formatFromObs("mp4") == RecordingFormat::Mp4);
	CHECK(voltz::formatFromObs("flv") == RecordingFormat::Mp4);
}

TEST_CASE("pickRecEncoder prefere AMD, depois NVIDIA, depois Intel, depois x264")
{
	CHECK(voltz::pickRecEncoder({"obs_x264", "h264_texture_amf", "ffmpeg_nvenc", "obs_qsv11"}) == "amd");
	CHECK(voltz::pickRecEncoder({"obs_x264", "ffmpeg_nvenc", "obs_qsv11"}) == "nvenc");
	CHECK(voltz::pickRecEncoder({"obs_x264", "obs_qsv11"}) == "qsv");
	CHECK(voltz::pickRecEncoder({"obs_x264"}) == "x264");
	CHECK(voltz::pickRecEncoder({}) == "x264");
}
