#include "mouse-effects.hpp"

#include <algorithm>
#include <cstddef>

namespace voltz {

HaloGeometry haloGeometry(const HaloStyle &halo)
{
	HaloGeometry geometry;
	geometry.diameter = static_cast<double>(halo.size);
	if (halo.shape == HaloShape::Ring) {
		const double thickness = std::max(2.0, geometry.diameter * 0.12);
		geometry.innerRadius = std::max(0.0, geometry.diameter / 2.0 - thickness);
	}
	return geometry;
}

RippleFrame rippleAt(const MouseStyle &style, int64_t ageNs)
{
	RippleFrame frame;
	if (!style.click.active) {
		return frame;
	}
	const int64_t age = std::max<int64_t>(0, ageNs);
	if (age >= kRippleDurationNs) {
		return frame;
	}
	const double t = static_cast<double>(age) / static_cast<double>(kRippleDurationNs);
	const double remaining = 1.0 - t;
	const double ease = 1.0 - remaining * remaining * remaining;
	const double endRadius = static_cast<double>(std::max(kRippleMinBase, style.halo.size)) * 0.9;
	frame.visible = true;
	frame.radius = endRadius * (0.3 + 0.7 * ease);
	frame.thickness = std::max(2.0, endRadius * 0.12);
	frame.alpha = 0.9 * remaining;
	return frame;
}

RgbColor rippleColor(const ClickStyle &click, MouseButton button)
{
	return button == MouseButton::Right ? click.rightColor : click.color;
}

void pushRipple(std::vector<Ripple> &ripples, const Ripple &ripple)
{
	ripples.push_back(ripple);
	if (ripples.size() > kMaxRipples) {
		ripples.erase(ripples.begin(),
			      ripples.begin() + static_cast<std::ptrdiff_t>(ripples.size() - kMaxRipples));
	}
}

void pruneRipples(std::vector<Ripple> &ripples, int64_t nowNs)
{
	ripples.erase(
		std::remove_if(ripples.begin(), ripples.end(),
			       [nowNs](const Ripple &ripple) { return nowNs - ripple.startNs >= kRippleDurationNs; }),
		ripples.end());
}

SpotlightGeometry spotlightGeometry(const SpotlightStyle &spotlight)
{
	SpotlightGeometry geometry;
	geometry.radius = static_cast<double>(spotlight.radius);
	geometry.feather = std::max(8.0, geometry.radius * 0.12);
	geometry.dim = spotlight.dim;
	return geometry;
}

} // namespace voltz
