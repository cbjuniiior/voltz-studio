#pragma once

#include <cstdint>
#include <vector>

namespace voltz {

// Item de uma cena na ordem em que o libobs enumera (de baixo para cima). Grupos trazem os filhos na
// mesma ordem; `collapsed` é a configuração privada "collapsed" do item do grupo (a mesma do OBS).
struct SourceNode {
	int64_t id = 0;
	bool isGroup = false;
	bool collapsed = false;
	std::vector<SourceNode> children;
};

// Linha do painel de Fontes, de cima para baixo. `groupId` = id do grupo pai (-1 no nível da cena).
// `siblingIndex` 0 = camada de cima entre os irmãos; `siblingCount` inclui a própria linha.
struct SourceRow {
	int64_t id = 0;
	int64_t groupId = -1;
	int depth = 0;
	bool isGroup = false;
	bool collapsed = false;
	int siblingIndex = 0;
	int siblingCount = 0;
};

// Achata a cena em linhas: camada de cima primeiro; filhos logo abaixo do grupo, se ele não estiver recolhido.
std::vector<SourceRow> flattenSourceTree(const std::vector<SourceNode> &bottomToTop);

// Ordem só muda entre irmãos (obs_sceneitem_set_order age dentro da cena ou do grupo do item).
inline bool canMoveUp(const SourceRow &row)
{
	return row.siblingIndex > 0;
}

inline bool canMoveDown(const SourceRow &row)
{
	return row.siblingIndex + 1 < row.siblingCount;
}

} // namespace voltz
