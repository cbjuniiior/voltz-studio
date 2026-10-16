#pragma once

#include <obs.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace voltz {

// Uma tentativa de encoder de vídeo para o ISO: id registrado no libobs + configurações.
struct IsoEncoderAttempt {
	std::string id;
	OBSData settings;
};

// Por que um ISO não iniciou (vira o "motivo" do evento iso_falhou).
enum class IsoFailure { None, NoImage, Canvas, NoEncoder, Output };

// "sem_imagem" | "canvas" | "sem_codificador" | "saida" ("" para None).
const char *isoFailureCode(IsoFailure failure);

// Faixa ISO (spec §5.3): canvas privado do libobs com uma única fonte no canal 0, na resolução da fonte → encoder de
// vídeo próprio + AAC da faixa 1 (mix) → ffmpeg_muxer (MKV). Vive na thread da UI; os sinais da saída chegam na
// thread do libobs e só mexem em atômicos/condição (e em onFailed).
class IsoTrack {
public:
	explicit IsoTrack(std::string track);
	~IsoTrack();
	IsoTrack(const IsoTrack &) = delete;
	IsoTrack &operator=(const IsoTrack &) = delete;

	// Cria o canvas e tenta cada encoder em ordem até a saída iniciar. Falso: `failure` diz por quê (detalhe no log).
	bool start(obs_source_t *source, const std::string &pathUtf8, const std::vector<IsoEncoderAttempt> &attempts,
		   IsoFailure &failure);
	void setPaused(bool paused);
	// Pede a parada graciosa (não espera). Idempotente.
	void requestStop();
	// Para (se ainda não pediu), espera o sinal "stop" até timeoutMs (depois força), loga e libera tudo.
	void finish(int timeoutMs);

	bool running() const;
	const std::string &track() const { return track_; }
	uint64_t startNs() const { return startNs_.load(); }
	uint32_t width() const { return width_; }
	uint32_t height() const { return height_; }
	double fps() const { return fps_; }
	const std::string &encoderId() const { return encoderId_; }

	// Thread do libobs: a saída parou sem a gente pedir (erro do muxer/encoder, StopOutput do websocket…).
	std::function<void(int code)> onFailed;

private:
	static void handleStart(void *data, calldata_t *params);
	static void handleStop(void *data, calldata_t *params);
	bool createVideo(obs_source_t *source);
	void releaseVideo();
	video_t *videoOutput() const;
	// 1: iniciou; 0: este encoder não serviu (tenta o próximo); -1: a saída nem foi criada.
	int tryStart(const IsoEncoderAttempt &attempt, const std::string &base, const std::string &pathUtf8);
	void release();

	std::string track_;
	OBSCanvasAutoRelease canvas_;
	OBSEncoderAutoRelease video_;
	OBSEncoderAutoRelease audio_;
	OBSOutputAutoRelease output_;
	OBSSignal startSignal_;
	OBSSignal stopSignal_;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	double fps_ = 0.0;
	std::string encoderId_;
	std::atomic<uint64_t> startNs_{0};
	std::atomic<bool> started_{false};
	std::atomic<bool> stopped_{false};
	std::atomic<bool> stopRequested_{false};
	std::mutex mutex_;
	std::condition_variable stopCv_;
};

} // namespace voltz
