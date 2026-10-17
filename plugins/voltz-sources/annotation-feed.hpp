#pragma once

#include <voltz/core/annotation-model.hpp>

#include <obs.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace voltz {

// Traços ao vivo das anotações, um estado só no processo. Escrito pelos procedimentos voltz_annot_* (thread da UI do
// frontend) e pelos pedidos de teste (thread do obs-websocket); lido pelas fontes Voltz Anotações na thread gráfica, que
// só copiam os traços quando a versão muda. Trechos curtos sob um mutex; o JSON é lido fora dele.
class AnnotationFeed {
public:
	static AnnotationFeed &instance();

	// Traço novo (ou substitui o de mesmo id) a partir do JSON do núcleo (strokeFromJson). Falso se inválido.
	bool begin(int id, const std::string &json);
	// Acrescenta pontos (pointsFromJson + appendStrokePoints) ao traço `id`.
	void extend(int id, const std::string &pointsJson);
	void remove(int id);
	void clear();
	// "Queimadas" (spec §6.3): a sobreposição aparece na captura, então as fontes não desenham.
	void setBurned(bool burned);
	// Copia os traços para `out` e devolve true se a versão mudou desde `knownVersion` (0 = nunca copiou); `version` e
	// `burned` sempre preenchidos.
	bool snapshotIfChanged(uint64_t knownVersion, std::vector<Stroke> &out, uint64_t &version, bool &burned);
	// {"tracos":N,"versao":V,"queimadas":false,"mostradas":M,"ids":[...]} (pedido de teste AnnotationFeedState).
	std::string describe();

	// Fontes Voltz Anotações sendo mostradas (show/hide do libobs; o destroy desconta o que o hide não descontou).
	void addShowing(int delta) { showing_.fetch_add(delta); }
	int showing() const { return showing_.load(); }

private:
	AnnotationFeed() = default;

	std::mutex mutex_;
	std::vector<Stroke> strokes_;
	uint64_t version_ = 1;
	bool burned_ = false;
	std::atomic<int> showing_{0};
};

// Registra voltz_annot_begin, voltz_annot_extend, voltz_annot_remove, voltz_annot_clear e voltz_annot_burned
// (live-protocol.hpp).
void registerAnnotationProcs(proc_handler_t *handler);

} // namespace voltz
