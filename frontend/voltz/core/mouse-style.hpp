#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

struct RgbColor {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
};

inline bool operator==(const RgbColor &a, const RgbColor &b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b;
}

// "#RRGGBB" (maiúsculas ou minúsculas) → cor. Falso (e `out` intacto) em qualquer outro formato.
bool parseHexColor(const std::string &text, RgbColor &out);
// Cor → "#RRGGBB" em maiúsculas.
std::string formatHexColor(const RgbColor &color);

enum class HaloShape { Filled, Ring };

// Limites de clampMouseStyle (os mesmos dos controles de Ajustes › Mouse).
constexpr int kHaloSizeMin = 12;
constexpr int kHaloSizeMax = 240;
constexpr double kHaloOpacityMin = 0.05;
constexpr double kHaloOpacityMax = 1.0;
constexpr int kSpotlightRadiusMin = 60;
constexpr int kSpotlightRadiusMax = 1000;
constexpr double kSpotlightDimMin = 0.1;
constexpr double kSpotlightDimMax = 0.9;
constexpr size_t kMaxSavedStyles = 20;

// Spec §5.2/§6.1. Tamanhos em pixels da captura (os do cursor.jsonl).
struct HaloStyle {
	bool active = true;
	RgbColor color{0xD4, 0xF5, 0x3C};
	int size = 46; // diâmetro
	double opacity = 0.25;
	HaloShape shape = HaloShape::Filled;
};

struct ClickStyle {
	bool active = true;
	RgbColor color{0xD4, 0xF5, 0x3C};
	RgbColor rightColor{0x4D, 0xA3, 0xFF};
};

struct SpotlightStyle {
	bool active = false;
	int radius = 220;
	double dim = 0.55; // "escurecer": opacidade do preto fora do círculo
};

struct MouseStyle {
	HaloStyle halo;
	ClickStyle click;
	SpotlightStyle spotlight;
};

MouseStyle clampMouseStyle(MouseStyle style);

// JSON compacto no formato do bloco "mouse" do projeto.voltz (spec §5.2):
// {"halo":{"ativo","cor","tamanho","opacidade","forma":"cheio|anel"},"clique":{"ativo","cor","cor_direito"},
//  "holofote":{"ativo","raio","escurecer"}}
std::string mouseStyleToJson(const MouseStyle &style);
// Leitura tolerante: `out` recebe o padrão + o que vier válido (já limitado). Falso só para JSON inválido ou que não é
// objeto (aí `out` fica no padrão).
bool mouseStyleFromJson(const std::string &json, MouseStyle &out);

struct NamedMouseStyle {
	std::string name;
	MouseStyle style;
};

// [{"nome":"Aula","estilo":{...}}]
std::string savedStylesToJson(const std::vector<NamedMouseStyle> &styles);
// Itens sem nome (ou só espaços) e itens que não são objeto são ignorados; JSON inválido → lista vazia.
std::vector<NamedMouseStyle> savedStylesFromJson(const std::string &json);
// Nome (sem espaços nas pontas) já salvo → troca o estilo; senão acrescenta no fim e, acima de kMaxSavedStyles, tira o
// mais antigo. Nome vazio → lista igual.
std::vector<NamedMouseStyle> upsertSavedStyle(std::vector<NamedMouseStyle> styles, const std::string &name,
					      const MouseStyle &style);
std::vector<NamedMouseStyle> removeSavedStyle(std::vector<NamedMouseStyle> styles, const std::string &name);

} // namespace voltz
