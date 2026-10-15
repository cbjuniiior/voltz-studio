#pragma once

namespace voltz {

struct CanvasSize {
	double width = 1920;
	double height = 1080;
};

// Retângulo em pixels do canvas (origem no canto superior esquerdo).
struct LayoutRect {
	double x = 0;
	double y = 0;
	double w = 0;
	double h = 0;
};

LayoutRect fullCanvas(CanvasSize canvas);

// Área quadrada da câmera circular do preset Tutorial: lado = 24% da altura, margem = 3,5% da altura,
// canto inferior direito. Valores arredondados para pixels inteiros.
LayoutRect tutorialCamera(CanvasSize canvas);

// Preset "Lado a lado": divisão em 70% da largura, margem (e vão entre as áreas) = 3% da altura.
LayoutRect sideBySideScreen(CanvasSize canvas);
LayoutRect sideBySideCamera(CanvasSize canvas);

} // namespace voltz
