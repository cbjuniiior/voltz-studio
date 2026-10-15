#include "preview-fit.hpp"

namespace voltz {

FitRect fitCentered(int baseCX, int baseCY, int windowCX, int windowCY, int margin)
{
	if (margin < 0)
		margin = 0;
	if (baseCX <= 0 || baseCY <= 0 || margin > windowCX / 2 || margin > windowCY / 2)
		return {};

	const int availCX = windowCX - 2 * margin;
	const int availCY = windowCY - 2 * margin;
	if (availCX <= 0 || availCY <= 0)
		return {};

	const double baseAspect = double(baseCX) / double(baseCY);
	const double availAspect = double(availCX) / double(availCY);

	FitRect r;
	if (availAspect > baseAspect) {
		r.cy = availCY;
		r.cx = int(double(availCY) * baseAspect + 0.5);
	} else {
		r.cx = availCX;
		r.cy = int(double(availCX) / baseAspect + 0.5);
	}
	if (r.cx <= 0 || r.cy <= 0)
		return {};
	r.scale = float(r.cx) / float(baseCX);
	r.x = margin + (availCX - r.cx) / 2;
	r.y = margin + (availCY - r.cy) / 2;
	return r;
}

} // namespace voltz
