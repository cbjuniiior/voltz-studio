#pragma once

#include <string>

namespace voltz {

enum class RecordingStopKind { Success, Unsupported, NoSpace, EncodeError, Generic };

// Como usar o texto "last_error" que a saída de gravação devolve ao parar.
enum class LastErrorUse { None, FormatArg, Append };

struct RecordingErrorText {
	std::string titleKey; // vazio quando não houve erro
	std::string messageKey;
	LastErrorUse lastErrorUse = LastErrorUse::None;
};

// Códigos OBS_OUTPUT_* (libobs/obs-defs.h): 0 sucesso, -6 sem suporte, -7 sem espaço, -8 erro de codificação;
// qualquer outro é erro genérico.
RecordingStopKind classifyRecordingStop(int code);

// Mesmas chaves de texto que OBSBasic::RecordingStop usa, para a casca mostrar o erro igual ao OBS.
RecordingErrorText recordingErrorText(RecordingStopKind kind, bool hasLastError);

} // namespace voltz
