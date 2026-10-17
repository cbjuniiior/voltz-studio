#pragma once

#include "annotation-model.hpp"

#include <cstdint>
#include <vector>

namespace voltz {

// Traço na tela agora. Tempos absolutos no relógio do libobs (ns).
struct LiveStroke {
	Stroke stroke;
	int64_t startNs = 0;
	int64_t endNs = -1; // -1 = em andamento
	bool open() const { return endNs < 0; }
};

// Traço que saiu da tela (desfazer, limpar ou "somem após N s") e quando.
struct RemovedStroke {
	LiveStroke live;
	int64_t removedNs = 0;
};

// Traços na tela, na ordem em que começaram (o último fica por cima); no máximo um em andamento, sempre o último. Ids a
// partir de 1, nunca reaproveitados. Sem Qt nem libobs: o dono é o AnnotationController (thread da UI).
class StrokeStore {
public:
	// Começa um traço (espessura limitada) com o primeiro ponto. 0 se já há um em andamento.
	int begin(AnnotationTool tool, const RgbColor &color, int width, const StrokePoint &first, int64_t nowNs);
	// Acrescenta pontos ao traço em andamento (appendStrokePoints). Falso sem traço em andamento ou sem pontos.
	bool extend(const std::vector<StrokePoint> &points);
	// Termina o traço em andamento; nullptr se não há. O ponteiro vale até a próxima mudança.
	const LiveStroke *end(int64_t nowNs);
	// Tira o traço mais recente (em andamento ou não). Vazio se não há traço.
	std::vector<RemovedStroke> undo(int64_t nowNs);
	// Tira todos, na ordem em que estavam.
	std::vector<RemovedStroke> clear(int64_t nowNs);
	// "Somem após": tira os traços terminados com endNs + lifetimeNs <= nowNs, com removedNs = endNs + lifetimeNs (o
	// instante exato, não o do temporizador). lifetimeNs <= 0 = nunca somem.
	std::vector<RemovedStroke> expire(int64_t nowNs, int64_t lifetimeNs);
	// Próximo instante em que algum traço some (-1 se nenhum).
	int64_t nextExpiryNs(int64_t lifetimeNs) const;

	const std::vector<LiveStroke> &visible() const { return visible_; }
	std::vector<Stroke> strokes() const;
	// Id do traço em andamento (0 se nenhum).
	int openId() const;

private:
	std::vector<LiveStroke> visible_;
	int nextId_ = 1;
};

} // namespace voltz
