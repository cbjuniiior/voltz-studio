#include "IsoTrack.hpp"

#include <util/platform.h>

#include <chrono>
#include <utility>

namespace voltz {

namespace {

constexpr int kForceStopWaitMs = 2000;
constexpr int64_t kAudioKbps = 160;

} // namespace

const char *isoFailureCode(IsoFailure failure)
{
	switch (failure) {
	case IsoFailure::NoImage:
		return "sem_imagem";
	case IsoFailure::Canvas:
		return "canvas";
	case IsoFailure::NoEncoder:
		return "sem_codificador";
	case IsoFailure::Output:
		return "saida";
	case IsoFailure::None:
		break;
	}
	return "";
}

IsoTrack::IsoTrack(std::string track) : track_(std::move(track)) {}

IsoTrack::~IsoTrack()
{
	finish(3000);
}

bool IsoTrack::running() const
{
	return started_.load() && !stopped_.load();
}

video_t *IsoTrack::videoOutput() const
{
	return canvas_ ? obs_canvas_get_video(canvas_) : nullptr;
}

bool IsoTrack::createVideo(obs_source_t *source)
{
	obs_video_info ovi = {};
	if (!obs_get_video_info(&ovi)) {
		return false;
	}
	ovi.base_width = width_;
	ovi.base_height = height_;
	ovi.output_width = width_;
	ovi.output_height = height_;
	/* O libobs força o fps do canvas principal em todo mix auxiliar (obs.c, obs_init_video_mix). */
	fps_ = ovi.fps_den ? static_cast<double>(ovi.fps_num) / static_cast<double>(ovi.fps_den) : 0.0;
	const std::string name = "voltz-iso-" + track_;
	/* Privado: fora da lista de canvases (não é salvo nem aparece no obs-websocket). Sem ACTIVATE: a fonte só é
	 * "mostrada" aqui (AUX_VIEW); quem a ativa é a cena do programa. Sem MIX_AUDIO. */
	canvas_ = obs_canvas_create_private(name.c_str(), &ovi, EPHEMERAL);
	if (!canvas_ || !obs_canvas_get_video(canvas_)) {
		canvas_ = nullptr;
		return false;
	}
	obs_canvas_set_channel(canvas_, 0, source);
	return true;
}

void IsoTrack::releaseVideo()
{
	if (!canvas_) {
		return;
	}
	obs_canvas_set_channel(canvas_, 0, nullptr);
	/* Única referência (canvas privado): obs_canvas_destroy tira o mix da lista e o libera na hora — por isso só
	 * depois de a saída e os encoders (que apontam para o video_t do mix) terem sido soltos. Não usar
	 * obs_canvas_remove: o sinal global "canvas_remove" faria o OBSBasic enfileirar RemoveCanvas com uma ref. */
	canvas_ = nullptr;
}

bool IsoTrack::start(obs_source_t *source, const std::string &pathUtf8, const std::vector<IsoEncoderAttempt> &attempts,
		     IsoFailure &failure)
{
	failure = IsoFailure::None;
	const char *sourceName = obs_source_get_name(source);
	/* NV12/P010 pedem lados pares: fonte ímpar perde 1 px na borda. */
	width_ = obs_source_get_width(source) & ~1u;
	height_ = obs_source_get_height(source) & ~1u;
	if (width_ < 2 || height_ < 2) {
		blog(LOG_WARNING, "[voltz] iso %s: fonte '%s' sem imagem (%ux%u)", track_.c_str(), sourceName, width_,
		     height_);
		failure = IsoFailure::NoImage;
		return false;
	}
	if (!createVideo(source)) {
		blog(LOG_ERROR, "[voltz] iso %s: canvas privado %ux%u nao foi criado", track_.c_str(), width_, height_);
		failure = IsoFailure::Canvas;
		release();
		return false;
	}

	const std::string base = "voltz-iso-" + track_;
	OBSDataAutoRelease audioSettings = obs_data_create();
	obs_data_set_int(audioSettings, "bitrate", kAudioKbps);
	/* Faixa 1 do Voltz (mix): o ffmpeg_muxer exige áudio (OBS_OUTPUT_AV) e o editor ganha uma referência de
	 * sincronia. */
	audio_ = obs_audio_encoder_create("ffmpeg_aac", (base + "-aac").c_str(), audioSettings, 0, nullptr);
	if (!audio_) {
		blog(LOG_ERROR, "[voltz] iso %s: encoder AAC nao foi criado", track_.c_str());
		failure = IsoFailure::Output;
		release();
		return false;
	}
	obs_encoder_set_audio(audio_, obs_get_audio());

	bool outputFailed = false;
	for (const IsoEncoderAttempt &attempt : attempts) {
		const int result = tryStart(attempt, base, pathUtf8);
		if (result > 0) {
			blog(LOG_INFO, "[voltz] iso %s: gravando '%s' de '%s' (%ux%u, %s)", track_.c_str(),
			     pathUtf8.c_str(), sourceName, width_, height_, encoderId_.c_str());
			return true;
		}
		if (result < 0) {
			outputFailed = true;
			break;
		}
	}
	failure = outputFailed ? IsoFailure::Output : IsoFailure::NoEncoder;
	blog(LOG_WARNING, "[voltz] iso %s: nenhum codificador iniciou (%zu opcao(oes))", track_.c_str(),
	     attempts.size());
	release();
	return false;
}

int IsoTrack::tryStart(const IsoEncoderAttempt &attempt, const std::string &base, const std::string &pathUtf8)
{
	OBSDataAutoRelease outputSettings = obs_data_create();
	obs_data_set_string(outputSettings, "path", pathUtf8.c_str());
	output_ = obs_output_create("ffmpeg_muxer", base.c_str(), outputSettings, nullptr);
	if (!output_) {
		blog(LOG_ERROR, "[voltz] iso %s: saida ffmpeg_muxer nao foi criada", track_.c_str());
		return -1;
	}
	video_ = obs_video_encoder_create(attempt.id.c_str(), (base + "-video").c_str(), attempt.settings, nullptr);
	if (!video_) {
		blog(LOG_INFO, "[voltz] iso %s: encoder '%s' nao registrado", track_.c_str(), attempt.id.c_str());
		output_ = nullptr;
		return 0;
	}
	obs_encoder_set_video(video_, videoOutput());
	obs_output_set_video_encoder(output_, video_);
	obs_output_set_audio_encoder(output_, audio_, 0);
	signal_handler_t *handler = obs_output_get_signal_handler(output_);
	startSignal_.Connect(handler, "start", &IsoTrack::handleStart, this);
	stopSignal_.Connect(handler, "stop", &IsoTrack::handleStop, this);
	startNs_ = 0;
	stopped_ = false;
	stopRequested_ = false;
	/* Falha de encoder (ex. sessão de GPU esgotada) é síncrona e não emite "stop" (obs-output.c). */
	if (obs_output_start(output_)) {
		started_ = true;
		encoderId_ = attempt.id;
		return 1;
	}
	const char *error = obs_output_get_last_error(output_);
	blog(LOG_WARNING, "[voltz] iso %s: '%s' nao iniciou (%s); tentando a proxima opcao", track_.c_str(),
	     attempt.id.c_str(), error && *error ? error : "sem detalhe");
	startSignal_.Disconnect();
	stopSignal_.Disconnect();
	output_ = nullptr;
	video_ = nullptr;
	return 0;
}

void IsoTrack::setPaused(bool paused)
{
	if (running() && !obs_output_pause(output_, paused)) {
		blog(LOG_WARNING, "[voltz] iso %s: %s falhou", track_.c_str(), paused ? "pausar" : "retomar");
	}
}

void IsoTrack::requestStop()
{
	if (!running() || stopRequested_.exchange(true)) {
		return;
	}
	obs_output_stop(output_);
}

void IsoTrack::finish(int timeoutMs)
{
	if (started_.load()) {
		requestStop();
		std::unique_lock<std::mutex> lock(mutex_);
		if (!stopCv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
				      [this]() { return stopped_.load(); })) {
			lock.unlock();
			blog(LOG_WARNING, "[voltz] iso %s: a parada passou de %d ms; forcando", track_.c_str(),
			     timeoutMs);
			obs_output_force_stop(output_);
			lock.lock();
			stopCv_.wait_for(lock, std::chrono::milliseconds(kForceStopWaitMs),
					 [this]() { return stopped_.load(); });
		}
		lock.unlock();
		video_t *video = videoOutput();
		blog(LOG_INFO, "[voltz] iso %s: %d quadro(s) na saida; codificador pulou %u de %u (%s)", track_.c_str(),
		     obs_output_get_total_frames(output_), video ? video_output_get_skipped_frames(video) : 0u,
		     video ? video_output_get_total_frames(video) : 0u, encoderId_.c_str());
	}
	release();
}

void IsoTrack::release()
{
	startSignal_.Disconnect();
	stopSignal_.Disconnect();
	/* Ordem obrigatória: saída (obs_output_destroy espera o fim da captura e solta os encoders) → encoders → canvas. */
	output_ = nullptr;
	video_ = nullptr;
	audio_ = nullptr;
	releaseVideo();
	started_ = false;
}

void IsoTrack::handleStart(void *data, calldata_t *)
{
	static_cast<IsoTrack *>(data)->startNs_.store(os_gettime_ns());
}

void IsoTrack::handleStop(void *data, calldata_t *params)
{
	auto *self = static_cast<IsoTrack *>(data);
	const int code = static_cast<int>(calldata_int(params, "code"));
	/* Primeiro marca parada (running() já falso quando a casca contar os ISO), depois avisa. O IsoTrack não some no
	 * meio: release() desconecta o sinal, e signal_handler_disconnect espera este callback terminar. */
	{
		std::lock_guard<std::mutex> lock(self->mutex_);
		self->stopped_.store(true);
	}
	self->stopCv_.notify_all();
	if (!self->stopRequested_.load() && self->onFailed) {
		self->onFailed(code);
	}
}

} // namespace voltz
