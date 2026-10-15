#pragma once

#include <cstdint>
#include <string>

namespace voltz {

// 3725000 -> "01:02:05". Horas sem limite de dígitos (mínimo 2). Negativo vira "00:00:00".
std::string formatClock(int64_t milliseconds);

} // namespace voltz
