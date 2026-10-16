#pragma once

#include "recording-settings.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace voltz {

// GOP dos ISO: curto para o editor achar quadros-chave perto de qualquer ponto.
constexpr int kIsoKeyframeSeconds = 2;
// Preset do AMF nos ISO (a Task 1 do Plano 3B confirma que "quality" sustenta 3 sessões nesta GPU).
constexpr const char *kIsoAmdPreset = "quality";

enum class EncoderFamily { Amd, Nvidia, Intel, Software };

// Encoder de vídeo candidato para um ISO: id registrado no libobs, codec ("hevc" | "h264") e família.
struct IsoEncoderCandidate {
	std::string id;
	std::string codec;
	EncoderFamily family = EncoderFamily::Software;
};

// Cadeia da spec §5.4 entre os encoders registrados: AMD AMF → NVIDIA NVENC → Intel QSV, em cada família HEVC antes
// de H.264. x264 por último só com allowSoftware (no ISO ele competiria com o programa pela CPU).
std::vector<IsoEncoderCandidate> isoEncoderChain(const std::vector<std::string> &availableIds, bool allowSoftware);

// Nível CQP/CRF do ISO, sempre de qualidade maior que a do programa (o OBS grava "Alta" em 16 e "Arquivo menor" em
// 23): Alta → 14, Arquivo menor → 20.
int isoQualityLevel(RecordingQuality quality);

// Configurações do encoder (chaves dos plugins do libobs).
struct EncoderSettings {
	std::vector<std::pair<std::string, std::string>> strings;
	std::vector<std::pair<std::string, int64_t>> ints;
};

// CQP (CRF no x264) com `qualityLevel`, GOP de kIsoKeyframeSeconds; AMF com kIsoAmdPreset; perfil "high" no H.264 e,
// no HEVC, "main" (NVENC/QSV) ou automático (AMF escolhe main/main10).
EncoderSettings isoEncoderSettings(const IsoEncoderCandidate &candidate, int qualityLevel);

// Regra de sobrecarga (spec §5.4), medida por janela de pulsação (5 s) sobre o programa. O ruído de renderização do
// ambiente (0 a 10% de quadros atrasados, também sem ISO) e a dupla contagem do atraso de render nos "pulados" do
// codificador tornam inúteis os limites ingênuos; por isso a regra separa as duas coisas (spike do Plano 3B):
//  - excesso do codificador = max(0, Δpulados − Δatrasados) sobre Δcodificados (o que o codificador perdeu além do render);
//  - atraso de render = Δatrasados sobre Δrenderizados.
constexpr uint64_t kMinWindowFrames = 60;     // janela com menos quadros renderizados é ignorada
constexpr uint64_t kEncoderExcessPercent = 5; // janela ruim: excesso do codificador ≥ 5%
constexpr uint64_t kRenderLagPercent = 20;    // ... ou atraso de render ≥ 20%
constexpr int kOverloadWindows = 2;           // sobrecarga = esse tanto de janelas ruins seguidas

// Contadores de quadros do programa: renderizados/atrasados (obs_get_total_frames/obs_get_lagged_frames) e
// entregues/pulados pelo codificador (video_output_get_total_frames/get_skipped_frames do vídeo principal).
struct FrameCounters {
	uint64_t rendered = 0;
	uint64_t lagged = 0;
	uint64_t encoded = 0;
	uint64_t skipped = 0;
};

// Janela ruim: desde `before`, excesso do codificador ≥ kEncoderExcessPercent ou atraso de render ≥ kRenderLagPercent.
// Janela com menos de kMinWindowFrames quadros renderizados, ou com contador que voltou (reinício do vídeo) → falso.
bool isoWindowOverloaded(const FrameCounters &before, const FrameCounters &now);

// Estado da regra ao longo da gravação: a primeira janela depois de reset() só calibra; sobrecarga = kOverloadWindows
// janelas ruins seguidas (janela boa zera a sequência; janela ignorada não conta nem zera).
class IsoOverloadMonitor {
public:
	// Início da gravação: descarta o estado e passa a esperar a janela de calibração.
	void reset(const FrameCounters &now);
	// Uma pulsação com os contadores atuais; true quando a sobrecarga se configurou.
	bool feed(const FrameCounters &now);

private:
	FrameCounters baseline_;
	bool calibrating_ = true;
	int badWindows_ = 0;
};

} // namespace voltz
