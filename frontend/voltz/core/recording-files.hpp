#pragma once

#include <cstdint>
#include <string>

namespace voltz {

// true para nomes com extensão de vídeo que o OBS grava (mp4, mkv, mov, flv, ts, avi), sem diferenciar
// maiúsculas. Nomes sem base (".mp4") não contam.
bool isRecordingFile(const std::string &fileName);

// "0 B", "1023 B", "1,5 KB", "12,3 MB", "1,2 GB" (base 1024, uma casa decimal, para no GB).
std::string formatFileSize(uint64_t bytes, char decimalSeparator);

} // namespace voltz
