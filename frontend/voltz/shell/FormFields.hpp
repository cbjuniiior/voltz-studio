#pragma once

class QWidget;

namespace voltz {

// Altura das linhas de formulário dos Ajustes: a de uma caixa de seleção (QComboBox) — 34 px visíveis pelo QSS da casca
// (texto + padding 5 + 5 + borda) mais a margem de cima e de baixo do tema do OBS (--spacing_input). As linhas de slider
// recebem a mesma altura para o espaçamento vertical ficar uniforme.
constexpr int kFormRowHeight = 38;

// Campo de formulário (slider, caixa de seleção) numa página com rolagem: a roda do mouse sobre o campo SEM o foco rola
// a página e não muda o valor; com o foco (depois de clicar no campo) muda como sempre. Põe Qt::StrongFocus (a roda
// nunca dá o foco; só clique e Tab) e instala um filtro de eventos, filho do campo. Thread da UI.
void guardWheel(QWidget *field);

} // namespace voltz
