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
// Margem negativa é tratada como 0. Retorna um FitRect todo zerado se alguma dimensão for inválida
// ou se o resultado tiver largura ou altura zero.
FitRect fitCentered(int baseCX, int baseCY, int windowCX, int windowCY, int margin = 0);

} // namespace voltz
