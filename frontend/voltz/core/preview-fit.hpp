#pragma once

namespace voltz {

struct FitRect {
	int x = 0;
	int y = 0;
	int cx = 0;
	int cy = 0;
	float scale = 0.0f;
};

// Encaixa uma imagem base (baseCX x baseCY) centralizada numa janela (windowCX x windowCY),
// mantendo a proporção e deixando `margin` px livres em cada borda.
// Retorna um FitRect de tamanho zero se alguma dimensão for inválida.
FitRect fitCentered(int baseCX, int baseCY, int windowCX, int windowCY, int margin = 0);

} // namespace voltz
