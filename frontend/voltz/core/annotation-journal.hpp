#pragma once

#include "stroke-store.hpp"

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace voltz {

constexpr const char *kAnnotationsJournalUpdateKey = "t_limpo";

// anotacoes.jsonl durante a gravação (spec §5.1/§6.2). O arquivo só cresce (sobrevive a uma queda): uma linha completa
// quando o traço termina e, quando ele sai da tela, a linha curta {"id":7,"t_limpo":9000}. No fim (e ao recuperar o
// projeto) consolidateAnnotations junta tudo no formato da spec: uma linha completa por traço, com t_limpo.
// Tempos em ms relativos a t0 (sem descontar pausas, spec §5.5), nunca negativos: o que já estava na tela no início
// vale 0. Linhas sem "\n". Texto em branco (textIsBlank: nunca apareceu) não tem linha.
class AnnotationJournal {
public:
	explicit AnnotationJournal(int64_t t0Ns) : t0Ns_(t0Ns) {}

	// Início da gravação: linha completa de cada traço já terminado (t_ini = t_fim = 0 se for de antes). Os em
	// andamento entram quando terminarem.
	std::vector<std::string> initialLines(const std::vector<LiveStroke> &visible);
	// O traço terminou: linha completa, uma vez por id ("" se em andamento ou já escrito).
	std::string strokeFinished(const LiveStroke &live);
	// O traço saiu da tela: linha curta; se ainda não tinha linha (em andamento), linha completa com t_fim = t_limpo.
	std::string strokeRemoved(const RemovedStroke &removed);
	// Fim da gravação: linha completa dos traços em andamento que ainda não têm linha (t_fim = endNs).
	std::vector<std::string> finishOpen(const std::vector<LiveStroke> &visible, int64_t endNs);
	// Traços com linha completa.
	size_t strokeCount() const { return written_.size(); }

private:
	int64_t relativeMs(int64_t ns) const;
	std::string fullLine(const LiveStroke &live, int64_t endNs, int64_t clearedNs);

	int64_t t0Ns_ = 0;
	std::set<int> written_;
};

// Junta o diário no formato da spec: cada linha completa (com "ferr") na ordem em que apareceu, com o "t_limpo" da linha
// curta de mesmo id; campos desconhecidos ficam; linhas inválidas, curtas de id desconhecido e repetidas saem. Cada
// linha termina com "\n". Idempotente.
std::string consolidateAnnotations(const std::string &jsonl);

} // namespace voltz
