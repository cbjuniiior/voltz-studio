#pragma once

#include <cstdint>
#include <string>

namespace voltz {

constexpr uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;
constexpr uint64_t kDiskLowBytes = 5 * kGiB;      // spec §8: faixa de aviso
constexpr uint64_t kDiskCriticalBytes = 1 * kGiB; // spec §8: para a gravação

enum class DiskLevel { Ok, Low, Critical };

DiskLevel diskLevel(uint64_t freeBytes);

struct RateInput {
	int width = 1920;
	int height = 1080;
	double fps = 60.0;
	bool compact = false;       // qualidade "Arquivo menor" (Small)
	bool streamEncoder = false; // qualidade "Stream": usa o bitrate da transmissão
	int streamKbps = 0;
	int audioTracks = 1;
	int audioKbps = 192;
};

// Estimativa de bytes por minuto antes de gravar (bits/pixel: Alta 0,12; Arquivo menor 0,05).
uint64_t estimateBytesPerMinute(const RateInput &input);

// Medida real: bytes do arquivo / duração líquida. 0 antes de 10 s (medida instável).
uint64_t measuredBytesPerMinute(uint64_t bytes, int64_t elapsedMs);

// "~111 MB/min", "~1,2 GB/min" ou, abaixo de 0,5 MB/min, "~300 KB/min" (base 1024).
std::string formatRate(uint64_t bytesPerMinute, char decimalSeparator);

// "42 GB" (≥ 10 GB), "4,2 GB" (1–10 GB) ou "850 MB"; sempre arredondado para baixo.
std::string formatFreeSpace(uint64_t bytes, char decimalSeparator);

} // namespace voltz
