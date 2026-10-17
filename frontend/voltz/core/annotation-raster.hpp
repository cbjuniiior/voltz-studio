#pragma once

#include "annotation-model.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
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

// Retângulo de pixels [left, right) × [top, bottom); vazio quando left >= right ou top >= bottom.
struct PixelRect {
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;
	bool empty() const { return left >= right || top >= bottom; }
};

inline bool operator==(const PixelRect &a, const PixelRect &b)
{
	return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

// União (o menor retângulo que contém os dois; vazio não conta) e interseção (vazia = PixelRect{}).
PixelRect uniteRects(const PixelRect &a, const PixelRect &b);
PixelRect intersectRects(const PixelRect &a, const PixelRect &b);

// Todo pixel que rasterizeStroke pode tocar (pixels da imagem, com a escala dada), sem cortar na imagem. Vazio se o
// traço não desenha nada.
PixelRect strokeBounds(const Stroke &stroke, double scaleX, double scaleY);

// rasterizeStroke só dentro de `clip` (cortado na imagem): fora dele nada muda; dentro, o mesmo resultado.
void rasterizeStrokeClipped(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY,
			    const PixelRect &clip);

// Limpa `region` (cortada na imagem) e redesenha nela, na ordem, os traços que a tocam: o mesmo que limpar e redesenhar
// a imagem inteira, só que dentro de `region` (Plano 4C: sobreposição e fonte redesenham só o que mudou).
void redrawRegion(const RasterImage &image, const std::vector<const Stroke *> &strokes, double scaleX, double scaleY,
		  const PixelRect &region);

// O que appendDirtyRect precisa do traço ANTES de acrescentar pontos: quantos pontos tinha e, nas formas, os pontos.
struct StrokeTail {
	size_t count = 0;
	std::vector<StrokePoint> shapePoints;
};

StrokeTail strokeTail(const Stroke &stroke);

// Região que muda quando `after` é o traço de `before` com pontos acrescentados (appendStrokePoints): caneta e
// marca-texto, só as peças novas (do último ponto antigo em diante; vazio se nada entrou); formas, a forma antiga ∪ a nova.
PixelRect appendDirtyRect(const StrokeTail &before, const Stroke &after, double scaleX, double scaleY);

// Regiões que mudaram, por versão (AnnotationFeed do plugin): quem copia os traços de vez em quando pergunta o que mudou
// desde a versão que já tem. Guarda as últimas kCapacity versões.
class DirtyHistory {
public:
	static constexpr size_t kCapacity = 64;

	// `version` cresce de 1 em 1; `full` = mudou tudo (limpar, queimadas).
	void record(uint64_t version, const PixelRect &rect, bool full);
	// União das regiões das versões em (known, current]. Falso = refazer a imagem inteira (known 0 ou à frente,
	// versão que já saiu do histórico ou não foi registrada, mudança "tudo").
	bool since(uint64_t known, uint64_t current, PixelRect &out) const;

private:
	struct Entry {
		uint64_t version = 0;
		PixelRect rect;
		bool full = false;
	};
	std::deque<Entry> entries_;
};

} // namespace voltz
