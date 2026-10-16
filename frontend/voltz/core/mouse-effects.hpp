#pragma once

#include "cursor-track.hpp"
#include "mouse-style.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace voltz {

// Destaque do mouse (spec §6.1): contas usadas pela fonte Voltz Mouse (plugin) e pela prévia de Ajustes › Mouse.
// Unidades em pixels da captura; tempos no relógio do libobs (ns).
constexpr int64_t kRippleDurationNs = 450000000;
constexpr size_t kMaxRipples = 16;
constexpr int kRippleMinBase = 24;

// Halo: disco de diâmetro `tamanho`; no anel, espessura = max(2, 12% do diâmetro). innerRadius 0 = disco cheio.
struct HaloGeometry {
	double diameter = 0.0;
	double innerRadius = 0.0;
};
HaloGeometry haloGeometry(const HaloStyle &halo);

// Uma onda de clique, em pixels da captura.
struct Ripple {
	int x = 0;
	int y = 0;
	MouseButton button = MouseButton::Left;
	int64_t startNs = 0;
};

// Anel da onda `ageNs` depois do clique: raio final = max(24, tamanho do halo)·0,9; cresce de 30% a 100% com ease-out
// cúbico em 450 ms; alfa 0,9 → 0; espessura = max(2, 12% do raio final). Invisível com o clique desligado ou vencida.
struct RippleFrame {
	bool visible = false;
	double radius = 0.0;
	double thickness = 0.0;
	double alpha = 0.0;
};
RippleFrame rippleAt(const MouseStyle &style, int64_t ageNs);
RgbColor rippleColor(const ClickStyle &click, MouseButton button);
// Acrescenta no fim; acima de kMaxRipples descarta as mais antigas.
void pushRipple(std::vector<Ripple> &ripples, const Ripple &ripple);
// Tira as ondas com idade >= kRippleDurationNs.
void pruneRipples(std::vector<Ripple> &ripples, int64_t nowNs);

// Holofote: escurece (opacidade `dim`) tudo fora do círculo de raio `radius`, com borda suave de `feather`
// (max(8, 12% do raio)) centrada no raio.
struct SpotlightGeometry {
	double radius = 0.0;
	double feather = 0.0;
	double dim = 0.0;
};
SpotlightGeometry spotlightGeometry(const SpotlightStyle &spotlight);

} // namespace voltz
