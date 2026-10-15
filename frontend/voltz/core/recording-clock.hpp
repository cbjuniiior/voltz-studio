#pragma once

#include <cstdint>

namespace voltz {

// Relógio de gravação que desconta pausas. Todos os tempos em ms de um relógio monotônico
// fornecido por quem chama (não lê o relógio do sistema — testável).
class RecordingClock {
public:
	void start(int64_t nowMs);
	void pause(int64_t nowMs);
	void resume(int64_t nowMs);
	void stop();

	bool running() const { return running_; }
	bool paused() const { return paused_; }

	int64_t elapsedMs(int64_t nowMs) const;

private:
	bool running_ = false;
	bool paused_ = false;
	int64_t startMs_ = 0;
	int64_t pausedTotalMs_ = 0;
	int64_t pausedAtMs_ = 0;
};

} // namespace voltz
