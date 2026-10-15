#pragma once

#include <string>

namespace voltz {

// Troca todas as ocorrências de "currentColor" (padrão dos SVGs do Phosphor) pela cor dada, ex. "#D4F53C".
std::string recolorSvg(const std::string &svg, const std::string &color);

} // namespace voltz
