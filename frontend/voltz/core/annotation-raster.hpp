#pragma once

#include "annotation-model.hpp"

#include <cstdint>
#include <vector>

namespace voltz {

// Marca-texto: 3x a espessura escolhida, 40% de opacidade.
constexpr double kHighlighterOpacity = 0.4;
constexpr int kHighlighterWidthFactor = 3;

// Espessura desenhada e opacidade da ferramenta.
double strokeRenderWidth(const Stroke &stroke);
double strokeOpacity(const Stroke &stroke);

// Peças de um traço em pixels da imagem: cápsulas (segmento com pontas redondas, raio = metade da espessura) e
// triângulos cheios (ponta da seta).
struct Capsule {
	double x0 = 0.0;
	double y0 = 0.0;
	double x1 = 0.0;
	double y1 = 0.0;
	double radius = 0.0;
};

struct FilledTriangle {
	double x[3] = {0.0, 0.0, 0.0};
	double y[3] = {0.0, 0.0, 0.0};
};

struct StrokeShape {
	std::vector<Capsule> capsules;
	std::vector<FilledTriangle> triangles;
};

// Forma do traço com os pontos multiplicados por (scaleX, scaleY); (1, 1) = pixels da captura. O centro do pixel (x,y)
// é (x+0,5, y+0,5). Caneta/marca-texto: cápsula entre pontos seguidos (um ponto só = bolinha); linha: primeiro → último;
// seta: haste + ponta cheia (comprimento max(12, 4×esp), limitado ao tamanho da seta; largura = comprimento);
// retângulo: 4 lados; elipse: polígono fechado inscrito no retângulo primeiro/último (24 a 180 lados, ~8 px por lado).
StrokeShape strokeShape(const Stroke &stroke, double scaleX, double scaleY);

// Imagem BGRA com alfa pré-multiplicado (bytes B, G, R, A por pixel; linhas de `stride` bytes, de cima para baixo) — o
// layout do DIB de 32 bits do Windows e da textura GS_BGRA do libobs.
struct RasterImage {
	uint8_t *pixels = nullptr;
	int width = 0;
	int height = 0;
	int stride = 0;
};

void clearRaster(const RasterImage &image);

// Desenha por cima (src-over). Dentro do mesmo traço vale a MAIOR cobertura de cada pixel entre as peças, e só então
// mistura: o marca-texto translúcido não escurece nas junções. Bordas suavizadas em ~1 px; o que sai da imagem é cortado.
void rasterizeStroke(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY);

} // namespace voltz
