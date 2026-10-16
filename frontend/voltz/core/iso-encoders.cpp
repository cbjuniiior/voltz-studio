#include "iso-encoders.hpp"

#include <algorithm>
#include <initializer_list>

namespace voltz {

namespace {

// Primeiro id registrado, na ordem de preferência; vazio se nenhum.
std::string firstAvailable(const std::vector<std::string> &ids, std::initializer_list<const char *> preference)
{
	for (const char *id : preference) {
		if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
			return id;
		}
	}
	return {};
}

void add(std::vector<IsoEncoderCandidate> &chain, const std::string &id, const char *codec, EncoderFamily family)
{
	if (!id.empty()) {
		chain.push_back(IsoEncoderCandidate{id, codec, family});
	}
}

bool countersWentBack(const FrameCounters &before, const FrameCounters &now)
{
	return now.rendered < before.rendered || now.lagged < before.lagged || now.encoded < before.encoded ||
	       now.skipped < before.skipped;
}

} // namespace

std::vector<IsoEncoderCandidate> isoEncoderChain(const std::vector<std::string> &availableIds, bool allowSoftware)
{
	std::vector<IsoEncoderCandidate> chain;
	add(chain, firstAvailable(availableIds, {"h265_texture_amf"}), "hevc", EncoderFamily::Amd);
	add(chain, firstAvailable(availableIds, {"h264_texture_amf"}), "h264", EncoderFamily::Amd);
	add(chain, firstAvailable(availableIds, {"obs_nvenc_hevc_tex", "ffmpeg_hevc_nvenc"}), "hevc",
	    EncoderFamily::Nvidia);
	add(chain, firstAvailable(availableIds, {"obs_nvenc_h264_tex", "ffmpeg_nvenc"}), "h264", EncoderFamily::Nvidia);
	add(chain, firstAvailable(availableIds, {"obs_qsv11_hevc"}), "hevc", EncoderFamily::Intel);
	add(chain, firstAvailable(availableIds, {"obs_qsv11_v2", "obs_qsv11"}), "h264", EncoderFamily::Intel);
	if (allowSoftware) {
		add(chain, firstAvailable(availableIds, {"obs_x264"}), "h264", EncoderFamily::Software);
	}
	return chain;
}

int isoQualityLevel(RecordingQuality quality)
{
	return quality == RecordingQuality::Compact ? 20 : 14;
}

EncoderSettings isoEncoderSettings(const IsoEncoderCandidate &candidate, int qualityLevel)
{
	EncoderSettings settings;
	const bool hevc = candidate.codec == "hevc";
	settings.ints.emplace_back("keyint_sec", kIsoKeyframeSeconds);
	switch (candidate.family) {
	case EncoderFamily::Amd:
		settings.strings.emplace_back("rate_control", "CQP");
		settings.strings.emplace_back("preset", kIsoAmdPreset);
		settings.ints.emplace_back("cqp", qualityLevel);
		if (!hevc) {
			settings.strings.emplace_back("profile", "high");
		}
		break;
	case EncoderFamily::Nvidia:
	case EncoderFamily::Intel:
		settings.strings.emplace_back("rate_control", "CQP");
		settings.strings.emplace_back("profile", hevc ? "main" : "high");
		settings.ints.emplace_back("cqp", qualityLevel);
		break;
	case EncoderFamily::Software:
		settings.strings.emplace_back("rate_control", "CRF");
		settings.strings.emplace_back("preset", "veryfast");
		settings.strings.emplace_back("profile", "high");
		settings.ints.emplace_back("crf", qualityLevel);
		break;
	}
	return settings;
}

bool isoWindowOverloaded(const FrameCounters &before, const FrameCounters &now)
{
	if (countersWentBack(before, now)) {
		return false;
	}
	const uint64_t rendered = now.rendered - before.rendered;
	if (rendered < kMinWindowFrames) {
		return false;
	}
	const uint64_t lagged = now.lagged - before.lagged;
	const uint64_t skipped = now.skipped - before.skipped;
	const uint64_t encoded = now.encoded - before.encoded;
	/* o atraso de render também entra nos "pulados": só o que passa dele é do codificador */
	const uint64_t excess = skipped > lagged ? skipped - lagged : 0;
	const bool encoderBad = encoded > 0 && excess * 100 >= encoded * kEncoderExcessPercent;
	const bool renderBad = lagged * 100 >= rendered * kRenderLagPercent;
	return encoderBad || renderBad;
}

void IsoOverloadMonitor::reset(const FrameCounters &now)
{
	baseline_ = now;
	calibrating_ = true;
	badWindows_ = 0;
}

bool IsoOverloadMonitor::feed(const FrameCounters &now)
{
	if (calibrating_) {
		/* Primeira janela só calibra: o início dos encoders costuma atrasar alguns quadros. */
		baseline_ = now;
		calibrating_ = false;
		badWindows_ = 0;
		return false;
	}
	if (countersWentBack(baseline_, now)) {
		baseline_ = now;
		return false;
	}
	if (now.rendered - baseline_.rendered < kMinWindowFrames) {
		return false; /* janela curta: segue acumulando, sem contar nem zerar */
	}
	const bool bad = isoWindowOverloaded(baseline_, now);
	baseline_ = now;
	badWindows_ = bad ? badWindows_ + 1 : 0;
	return badWindows_ >= kOverloadWindows;
}

} // namespace voltz
