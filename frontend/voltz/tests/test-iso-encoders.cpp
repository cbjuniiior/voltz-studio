#include <doctest.h>

#include <voltz/core/iso-encoders.hpp>

#include <cstdint>
#include <string>
#include <vector>

using voltz::EncoderFamily;
using voltz::EncoderSettings;
using voltz::FrameCounters;
using voltz::IsoEncoderCandidate;
using voltz::RecordingQuality;

namespace {

std::vector<std::string> ids(const std::vector<IsoEncoderCandidate> &chain)
{
	std::vector<std::string> out;
	for (const IsoEncoderCandidate &candidate : chain) {
		out.push_back(candidate.id);
	}
	return out;
}

std::string text(const EncoderSettings &settings, const std::string &key)
{
	for (const auto &item : settings.strings) {
		if (item.first == key) {
			return item.second;
		}
	}
	return "<ausente>";
}

int64_t number(const EncoderSettings &settings, const std::string &key)
{
	for (const auto &item : settings.ints) {
		if (item.first == key) {
			return item.second;
		}
	}
	return -1;
}

IsoEncoderCandidate candidate(const char *id, const char *codec, EncoderFamily family)
{
	return IsoEncoderCandidate{id, codec, family};
}

} // namespace

TEST_CASE("cadeia do ISO só com AMD: HEVC antes de H.264 e sem x264 por padrão")
{
	const std::vector<std::string> available = {"ffmpeg_aac", "obs_x264", "h264_texture_amf", "h265_texture_amf",
						    "av1_texture_amf"};
	const std::vector<IsoEncoderCandidate> chain = voltz::isoEncoderChain(available, false);
	REQUIRE(chain.size() == 2);
	CHECK(ids(chain) == std::vector<std::string>{"h265_texture_amf", "h264_texture_amf"});
	CHECK(chain[0].codec == "hevc");
	CHECK(chain[1].codec == "h264");
	CHECK(chain[0].family == EncoderFamily::Amd);
	CHECK(ids(voltz::isoEncoderChain(available, true)) ==
	      std::vector<std::string>{"h265_texture_amf", "h264_texture_amf", "obs_x264"});
}

TEST_CASE("cadeia do ISO segue AMD, depois NVIDIA, depois Intel")
{
	const std::vector<std::string> available = {"obs_qsv11_v2",       "obs_qsv11_hevc",   "obs_nvenc_h264_tex",
						    "obs_nvenc_hevc_tex", "ffmpeg_nvenc",     "ffmpeg_hevc_nvenc",
						    "h264_texture_amf",   "h265_texture_amf", "obs_x264"};
	CHECK(ids(voltz::isoEncoderChain(available, false)) ==
	      std::vector<std::string>{"h265_texture_amf", "h264_texture_amf", "obs_nvenc_hevc_tex",
				       "obs_nvenc_h264_tex", "obs_qsv11_hevc", "obs_qsv11_v2"});
}

TEST_CASE("NVENC de compatibilidade e QSV v1 entram quando os novos não existem")
{
	CHECK(ids(voltz::isoEncoderChain({"ffmpeg_nvenc", "ffmpeg_hevc_nvenc", "obs_qsv11"}, false)) ==
	      std::vector<std::string>{"ffmpeg_hevc_nvenc", "ffmpeg_nvenc", "obs_qsv11"});
}

TEST_CASE("sem GPU a cadeia fica vazia; x264 só quando permitido")
{
	CHECK(voltz::isoEncoderChain({"obs_x264", "ffmpeg_aac"}, false).empty());
	CHECK(ids(voltz::isoEncoderChain({"obs_x264", "ffmpeg_aac"}, true)) == std::vector<std::string>{"obs_x264"});
	CHECK(voltz::isoEncoderChain({}, true).empty());
}

TEST_CASE("qualidade do ISO é maior (QP menor) que a do programa")
{
	CHECK(voltz::isoQualityLevel(RecordingQuality::High) == 14);
	CHECK(voltz::isoQualityLevel(RecordingQuality::High) < 16);
	CHECK(voltz::isoQualityLevel(RecordingQuality::Compact) == 20);
	CHECK(voltz::isoQualityLevel(RecordingQuality::Compact) < 23);
}

TEST_CASE("configuração AMF: CQP, preset de qualidade, GOP de 2 s; perfil só no H.264")
{
	const EncoderSettings hevc =
		voltz::isoEncoderSettings(candidate("h265_texture_amf", "hevc", EncoderFamily::Amd), 14);
	CHECK(text(hevc, "rate_control") == "CQP");
	CHECK(text(hevc, "preset") == voltz::kIsoAmdPreset);
	CHECK(number(hevc, "cqp") == 14);
	CHECK(number(hevc, "keyint_sec") == 2);
	CHECK(text(hevc, "profile") == "<ausente>");
	const EncoderSettings h264 =
		voltz::isoEncoderSettings(candidate("h264_texture_amf", "h264", EncoderFamily::Amd), 20);
	CHECK(text(h264, "profile") == "high");
	CHECK(number(h264, "cqp") == 20);
}

TEST_CASE("configuração NVENC/QSV em CQP com perfil por codec; x264 em CRF")
{
	const EncoderSettings nvenc =
		voltz::isoEncoderSettings(candidate("obs_nvenc_hevc_tex", "hevc", EncoderFamily::Nvidia), 20);
	CHECK(text(nvenc, "rate_control") == "CQP");
	CHECK(text(nvenc, "profile") == "main");
	CHECK(number(nvenc, "cqp") == 20);
	CHECK(text(nvenc, "preset") == "<ausente>");
	const EncoderSettings qsv =
		voltz::isoEncoderSettings(candidate("obs_qsv11_v2", "h264", EncoderFamily::Intel), 14);
	CHECK(text(qsv, "profile") == "high");
	CHECK(number(qsv, "keyint_sec") == 2);
	const EncoderSettings x264 =
		voltz::isoEncoderSettings(candidate("obs_x264", "h264", EncoderFamily::Software), 14);
	CHECK(text(x264, "rate_control") == "CRF");
	CHECK(number(x264, "crf") == 14);
	CHECK(number(x264, "cqp") == -1);
	CHECK(text(x264, "preset") == "veryfast");
}

TEST_CASE("janela de sobrecarga: excesso do codificador ≥ 5% ou atraso de render ≥ 20%")
{
	const FrameCounters before{1000, 10, 1000, 5};
	// excesso do codificador: Δpulados − Δatrasados sobre Δcodificados
	CHECK(voltz::isoWindowOverloaded(before, FrameCounters{1300, 10, 1300, 20}));       // 15/300 = 5% (ruim)
	CHECK_FALSE(voltz::isoWindowOverloaded(before, FrameCounters{1300, 10, 1300, 19})); // 14/300 = 4,7% (boa)
	// ruído do ambiente: 10% de atraso de render com os pulados iguais aos atrasados → excesso 0 (boa)
	CHECK_FALSE(voltz::isoWindowOverloaded(before, FrameCounters{1300, 40, 1300, 35}));
	// atraso de render: 59/300 = 19,7% (boa) e 60/300 = 20% (ruim), mesmo com os pulados acompanhando
	CHECK_FALSE(voltz::isoWindowOverloaded(before, FrameCounters{1300, 69, 1300, 64}));
	CHECK(voltz::isoWindowOverloaded(before, FrameCounters{1300, 70, 1300, 65}));
	// o excesso soma-se ao atraso: 30 atrasados + 45 pulados → excesso 15 (5%) é ruim
	CHECK(voltz::isoWindowOverloaded(before, FrameCounters{1300, 40, 1300, 50}));
}

TEST_CASE("janela de sobrecarga: curta ou com contador que voltou é ignorada")
{
	const FrameCounters before{1000, 10, 1000, 5};
	CHECK_FALSE(voltz::isoWindowOverloaded(FrameCounters{}, FrameCounters{50, 50, 50, 50})); // 50 < 60 quadros
	CHECK_FALSE(voltz::isoWindowOverloaded(before, FrameCounters{1059, 59, 1059, 59}));      // 59 < 60 quadros
	CHECK(voltz::isoWindowOverloaded(before, FrameCounters{1060, 70, 1060, 5}));    // 60 quadros, 100% atrasados
	CHECK_FALSE(voltz::isoWindowOverloaded(before, FrameCounters{100, 0, 100, 0})); // contadores voltaram
	CHECK_FALSE(voltz::isoWindowOverloaded(before, FrameCounters{1300, 10, 900, 100})); // codificados voltaram
}

TEST_CASE("monitor de sobrecarga: duas janelas ruins seguidas; a primeira só calibra")
{
	const FrameCounters start{1000, 10, 1000, 5};
	const FrameCounters calibration{1150, 90, 1150, 85}; // ruim, mas é a janela de calibração
	// janelas de 150 quadros: ruim = 100 atrasados (66%); boa = nada
	const auto bad = [](const FrameCounters &c) {
		return FrameCounters{c.rendered + 150, c.lagged + 100, c.encoded + 150, c.skipped + 100};
	};
	const auto good = [](const FrameCounters &c) {
		return FrameCounters{c.rendered + 150, c.lagged, c.encoded + 150, c.skipped};
	};

	voltz::IsoOverloadMonitor monitor;
	monitor.reset(start);
	CHECK_FALSE(monitor.feed(calibration));

	// uma janela ruim isolada não dispara; duas seguidas disparam
	const FrameCounters w1 = bad(calibration);
	const FrameCounters w2 = bad(w1);
	CHECK_FALSE(monitor.feed(w1));
	CHECK(monitor.feed(w2));

	// ruim, boa, ruim não dispara (a boa zera a sequência); nova ruim seguida dispara
	monitor.reset(start);
	CHECK_FALSE(monitor.feed(calibration));
	const FrameCounters b1 = bad(calibration);
	const FrameCounters g1 = good(b1);
	const FrameCounters b2 = bad(g1);
	const FrameCounters b3 = bad(b2);
	CHECK_FALSE(monitor.feed(b1));
	CHECK_FALSE(monitor.feed(g1));
	CHECK_FALSE(monitor.feed(b2));
	CHECK(monitor.feed(b3));
}

TEST_CASE("monitor de sobrecarga: janela curta e contador que voltou não contam nem zeram")
{
	const FrameCounters start{1000, 10, 1000, 5};
	const FrameCounters calibration{1150, 10, 1150, 5};
	voltz::IsoOverloadMonitor monitor;
	monitor.reset(start);
	CHECK_FALSE(monitor.feed(calibration));

	const FrameCounters w1{1300, 110, 1300, 105}; // ruim (100/150 atrasados)
	CHECK_FALSE(monitor.feed(w1));
	CHECK_FALSE(monitor.feed(FrameCounters{1330, 140, 1330, 135})); // 30 quadros: ignorada, sem zerar
	CHECK_FALSE(monitor.feed(FrameCounters{100, 0, 100, 0}));       // voltou: rebase, sem zerar
	// a janela seguinte (150 quadros ruins desde o rebase) é a 2ª ruim seguida
	CHECK(monitor.feed(FrameCounters{250, 100, 250, 100}));
}
