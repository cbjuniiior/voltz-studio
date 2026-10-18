#pragma once

#include "annotation-model.hpp"
#include "text-layout.hpp"

#include <cstdint>
#include <vector>

namespace voltz {

// As duas fontes do texto das anotações (DM Sans Regular e Bold, OFL) embutidas no voltz-core e lidas pelo FreeType
// (o mesmo das deps do OBS). Cada binário que liga o voltz-core (obs-studio e o plugin voltz-sources) tem a sua cópia:
// a sobreposição e a fonte Voltz Anotações desenham o texto com o mesmo código e os mesmos bytes. Sem hinting: o
// desenho só depende do tamanho em pixels, e o layout é proporcional ao tamanho. Thread-safe (um mutex por binário: no
// plugin a thread gráfica desenha enquanto a UI calcula retângulos).

// Falso se a fonte não abriu (aí o texto não é desenhado e os retângulos dele ficam vazios; quem usa avisa no log).
bool embeddedFontAvailable(TextFont font);
// Medidas da fonte para o layoutText (uma fonte que não abriu devolve unitsPerEm 0: layout vazio).
const GlyphMetrics &embeddedFontMetrics(TextFont font);

// Cobertura (0..255) de um caractere, pixels inteiros da imagem.
struct GlyphBitmap {
	int left = 0; // coluna do primeiro pixel
	int top = 0;  // linha do primeiro pixel
	int width = 0;
	int height = 0;
	std::vector<uint8_t> coverage; // width × height, de cima para baixo
};

// Rasteriza `codepoint` com o "em" de `pixelSize` pixels e a origem em (originX, baseline) — pixels da imagem, frações
// valem (o contorno é deslocado antes de rasterizar). Caractere sem desenho (espaço) ou fonte que não abriu → falso.
// Caractere que a fonte não tem: o glifo .notdef (um retângulo).
bool renderGlyph(TextFont font, char32_t codepoint, double pixelSize, double originX, double baseline,
		 GlyphBitmap &out);

// Cursor de digitação de um traço de texto (só a sobreposição o desenha): um traço de caneta vertical no fim do texto,
// na cor dele, em pixels da captura (espessura max(2, tam/16)). Sem pontos se `text` não é um traço de texto.
Stroke textCaretStroke(const Stroke &text);

} // namespace voltz
