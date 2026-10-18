#pragma once

#include "mouse-style.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Desenho na tela (spec §6.2). Ferramentas do Plano 4B; o texto entra no 4D.
enum class AnnotationTool { Pen, Highlighter, Line, Arrow, Rectangle, Ellipse, Text };

// "caneta" | "marca_texto" | "linha" | "seta" | "retangulo" | "elipse" | "texto" (campo "ferr" do anotacoes.jsonl).
const char *toolToString(AnnotationTool tool);
// Falso (e `out` intacto) para texto desconhecido.
bool toolFromString(const std::string &text, AnnotationTool &out);
// Linha, seta, retângulo e elipse: só o primeiro e o último ponto contam. (O texto guarda só o primeiro.)
bool isShapeTool(AnnotationTool tool);

// Fonte do texto: as duas embutidas no voltz-core (DM Sans, OFL), as mesmas na sobreposição e na fonte do plugin.
// "dm_sans" | "dm_sans_negrito" (campo "fonte").
enum class TextFont { Regular, Bold };
const char *textFontToString(TextFont font);
// Falso (e `out` intacto) para nome desconhecido.
bool textFontFromString(const std::string &text, TextFont &out);

// Pixel da captura da Tela (as coordenadas de tela.mkv e do cursor.jsonl).
struct StrokePoint {
	int x = 0;
	int y = 0;
};

inline bool operator==(const StrokePoint &a, const StrokePoint &b)
{
	return a.x == b.x && a.y == b.y;
}

constexpr int kStrokeWidthMin = 1;
constexpr int kStrokeWidthMax = 48;
constexpr size_t kMaxStrokePoints = 20000;
constexpr int kFadeSecondsMax = 60;
// Tamanho do texto: altura do "em" em pixels da captura (como o font-size do CSS).
constexpr int kTextSizeMin = 12;
constexpr int kTextSizeMax = 200;
constexpr int kTextSizeDefault = 48;

int clampStrokeWidth(int width);
int clampTextSize(int size);

// Um traço. Tempos em ms relativos ao t0 do projeto (spec §5.5); -1 = ainda não aconteceu. Texto (Plano 4D): `points`
// = só a âncora (canto de cima à esquerda da caixa do texto, pixels da captura), `text` em UTF-8 (linhas separadas por
// '\n'; annotation-text.hpp), `textSize` e `textFont`; `width` não conta.
struct Stroke {
	int id = 0;
	AnnotationTool tool = AnnotationTool::Pen;
	RgbColor color{0xD4, 0xF5, 0x3C};
	int width = 4;
	std::vector<StrokePoint> points;
	std::string text;
	int textSize = kTextSizeDefault;
	TextFont textFont = TextFont::Bold;
	int64_t startMs = 0;
	int64_t endMs = -1;
	int64_t clearedMs = -1;
};

// Acrescenta pontos. Caneta/marca-texto: no fim, sem repetir o último e até kMaxStrokePoints. Formas: o primeiro ponto
// fica e o segundo passa a ser o último recebido (no máximo 2 pontos). Texto: só o primeiro ponto (a âncora) conta.
void appendStrokePoints(Stroke &stroke, const std::vector<StrokePoint> &points);

// [[x,y],[x,y]] compacto.
std::string pointsToJson(const std::vector<StrokePoint> &points);
// Falso (e `out` intacto) se não for uma lista; itens que não são [número, número] são ignorados.
bool pointsFromJson(const std::string &json, std::vector<StrokePoint> &out);

// Linha do anotacoes.jsonl, sem "\n" (spec §6.2):
// {"id":7,"ferr":"seta","cor":"#D4F53C","esp":4,"pts":[[x,y],...],"t_ini":1200,"t_fim":1850,"t_limpo":9000}
// t_fim e t_limpo ficam de fora quando -1. Texto: "texto", "tam" e "fonte" logo depois de "pts":
// {"id":9,"ferr":"texto","cor":"#D4F53C","esp":4,"pts":[[x,y]],"texto":"Olá","tam":48,"fonte":"dm_sans_negrito",...}
std::string strokeToJsonLine(const Stroke &stroke);
// Leitura tolerante: precisa de objeto com "id" inteiro e "ferr" conhecido; cor inválida = lima; espessura limitada;
// tempos ausentes = 0 (t_ini) e -1 (t_fim, t_limpo). Texto: "texto" limpo (sanitizeAnnotationText), "tam" limitado,
// "fonte" desconhecida = negrito; nas outras ferramentas esses campos são ignorados. Falso deixa `out` intacto.
bool strokeFromJson(const std::string &json, Stroke &out);

// Estilo das anotações (user.ini [Voltz] AnnotationStyle): ferramenta, cor e espessura do próximo traço, "traços somem
// após N s" (0 = nunca) e tamanho/fonte do texto.
// {"ferr":"caneta","cor":"#D4F53C","esp":4,"somem_apos_s":0,"tam":48,"fonte":"dm_sans_negrito"}
struct AnnotationStyle {
	AnnotationTool tool = AnnotationTool::Pen;
	RgbColor color{0xD4, 0xF5, 0x3C};
	int width = 4;
	int fadeSeconds = 0;
	int textSize = kTextSizeDefault;
	TextFont textFont = TextFont::Bold;
};

AnnotationStyle clampAnnotationStyle(AnnotationStyle style);
std::string annotationStyleToJson(const AnnotationStyle &style);
// `out` recebe o padrão + o que vier válido (limitado). Falso só para JSON inválido ou que não é objeto (aí `out` fica
// no padrão).
bool annotationStyleFromJson(const std::string &json, AnnotationStyle &out);

} // namespace voltz
