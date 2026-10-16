#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Retângulo de um monitor em pixels físicos do desktop virtual (pode ter origem negativa).
struct MonitorRect {
	int left = 0;
	int top = 0;
	int width = 0;
	int height = 0;
};

// Monitor capturado e o tamanho em pixels da captura (igual ao do monitor no monitor_capture).
struct CaptureMapping {
	MonitorRect monitor;
	int captureWidth = 0;
	int captureHeight = 0;
};

struct CapturePoint {
	bool inside = false;
	int x = 0;
	int y = 0;
};

// Ponto do desktop (pixels físicos) → pixels da captura; `inside` falso fora do monitor capturado.
CapturePoint mapToCapture(const CaptureMapping &mapping, int desktopX, int desktopY);

enum class RawMouseKind { Move, Down, Up, Wheel };
enum class MouseButton { None, Left, Right, Middle };

// Evento cru vindo do gancho de mouse: tempo no relógio do libobs (ns) e posição física do desktop.
struct RawMouseEvent {
	int64_t tNs = 0;
	RawMouseKind kind = RawMouseKind::Move;
	int x = 0;
	int y = 0;
	MouseButton button = MouseButton::None;
	int wheelDelta = 0; // unidades do Windows (WHEEL_DELTA = 120); positivo = para cima
};

enum class CursorEventType { Move, Down, Up, Scroll, Outside };

// Uma linha do cursor.jsonl (tempo em ms relativos ao t0, posição em pixels da captura).
struct CursorEvent {
	int64_t tMs = 0;
	CursorEventType type = CursorEventType::Move;
	int x = 0;
	int y = 0;
	MouseButton button = MouseButton::None;
	int wheelDelta = 0;
};

// Linha JSON sem "\n": {"t":1234,"x":812,"y":440,"e":"move"} · {"t":5,"x":1,"y":2,"e":"down","b":"l"} ·
// {"t":8,"x":3,"y":4,"e":"scroll","d":-0.5} · {"t":9,"e":"fora"}
std::string cursorLine(const CursorEvent &event);

// Converte eventos crus em linhas do cursor.jsonl. Chamado pela thread de escrita a cada tique (~16 ms) com tudo o
// que o gancho enfileirou: só o último movimento do tique é gravado (60 Hz), um movimento pendente sai antes de
// clique/rolagem, posição repetida não gera linha, sair da captura gera um único "fora" e cliques fora são ignorados.
class CursorSampler {
public:
	CursorSampler(const CaptureMapping &mapping, int64_t t0Ns);

	// Posição no início da gravação (sempre gera "move" ou "fora").
	std::vector<CursorEvent> initial(int64_t tNs, int desktopX, int desktopY);
	std::vector<CursorEvent> process(const std::vector<RawMouseEvent> &batch);

private:
	int64_t relativeMs(int64_t tNs) const;

	CaptureMapping mapping_;
	int64_t t0Ns_ = 0;
	bool hasLast_ = false;
	int lastX_ = 0;
	int lastY_ = 0;
	bool outside_ = false;
};

} // namespace voltz
