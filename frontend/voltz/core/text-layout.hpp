#pragma once

#include <string>
#include <vector>

namespace voltz {

// Medidas de uma fonte em unidades da fonte — só o que o layout precisa. A de verdade é a da fonte embutida
// (annotation-font: DM Sans pelo FreeType); os testes usam uma falsa.
class GlyphMetrics {
public:
	virtual ~GlyphMetrics() = default;
	virtual int unitsPerEm() const = 0;
	virtual int ascender() const = 0;                  // > 0: acima da linha de base
	virtual int descender() const = 0;                 // <= 0: abaixo da linha de base
	virtual int lineHeight() const = 0;                // distância entre duas linhas de base
	virtual int advance(char32_t codepoint) const = 0; // avanço horizontal do caractere (sem kerning)
};

// Um caractere posicionado: origem (na linha de base) em pixels, relativa à âncora do texto.
struct PlacedGlyph {
	char32_t codepoint = 0;
	double x = 0.0;
	double baseline = 0.0;
};

// Texto montado, em pixels relativos à âncora (canto de cima à esquerda da caixa; y cresce para baixo).
struct TextLayout {
	std::vector<PlacedGlyph> glyphs; // sem os '\n'
	double pixelSize = 0.0;
	double lineAdvance = 0.0; // de uma linha de base à seguinte
	int lines = 1;
	double width = 0.0;  // a linha mais larga
	double height = 0.0; // do topo da primeira linha (âncora) ao pé da última (ascender - descender na última)
	// Cursor de digitação: depois do último caractere, na última linha (do topo ao pé dela).
	double caretX = 0.0;
	double caretTop = 0.0;
	double caretBottom = 0.0;
};

// Monta `text` (UTF-8, linhas separadas por '\n') com o "em" de `pixelSize` pixels: a primeira linha de base fica
// `ascender` abaixo da âncora e cada linha seguinte `lineHeight` abaixo da anterior; cada caractere avança o seu
// `advance`. Sem quebra automática, sem kerning, sem arredondar (quem desenha arredonda). Métricas inválidas
// (unitsPerEm <= 0) ou pixelSize <= 0 → layout vazio com pixelSize 0.
TextLayout layoutText(const std::string &text, double pixelSize, const GlyphMetrics &metrics);

} // namespace voltz
