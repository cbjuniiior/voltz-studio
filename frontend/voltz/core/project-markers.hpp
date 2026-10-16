#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Um item do marcadores.json (spec §5.1): { "t_ms": 12345, "nome": "Marcador 1" } — t_ms relativo ao t0.
struct Marker {
	int64_t tMs = 0;
	std::string name;
};

// Lista JSON indentada com "\n" no fim ("[]\n" sem marcadores).
std::string serializeMarkers(const std::vector<Marker> &markers);

// Limpa `out` e lê os itens válidos (t_ms numérico e nome texto). Falso se não for uma lista JSON.
bool parseMarkers(const std::string &json, std::vector<Marker> &out);

} // namespace voltz
