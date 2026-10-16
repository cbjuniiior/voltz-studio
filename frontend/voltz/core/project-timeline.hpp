#pragma once

#include <cstdint>
#include <vector>

namespace voltz {

// Pausa em ms relativos ao t0. endMs < 0: pausa ainda aberta.
struct PauseSpan {
	int64_t startMs = 0;
	int64_t endMs = -1;
};

// Duração líquida: `grossMs` menos o trecho de cada pausa dentro de [0, grossMs]; pausa aberta vai até o fim.
int64_t netDuration(int64_t grossMs, const std::vector<PauseSpan> &pauses);

// Linha do tempo do projeto no relógio do libobs (os_gettime_ns). Tempos em ms relativos ao t0, sem descontar
// pausas (spec §5.5); as pausas ficam registradas para o editor. Não lê relógio: quem chama passa o "agora".
class ProjectTimeline {
public:
	void start(int64_t t0Ns);
	void pause(int64_t nowNs);
	void resume(int64_t nowNs);
	// Fecha uma pausa aberta e congela as durações.
	void stop(int64_t nowNs);

	bool started() const { return started_; }
	bool paused() const { return paused_; }
	bool stopped() const { return stopped_; }
	int64_t t0Ns() const { return t0Ns_; }

	int64_t relativeMs(int64_t nowNs) const;
	int64_t grossMs(int64_t nowNs) const;
	int64_t netMs(int64_t nowNs) const;
	const std::vector<PauseSpan> &pauses() const { return pauses_; }

private:
	bool started_ = false;
	bool paused_ = false;
	bool stopped_ = false;
	int64_t t0Ns_ = 0;
	int64_t stopNs_ = 0;
	std::vector<PauseSpan> pauses_;
};

} // namespace voltz
