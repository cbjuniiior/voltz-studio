#include "recording-clock.hpp"

namespace voltz {

void RecordingClock::start(int64_t nowMs)
{
	running_ = true;
	paused_ = false;
	startMs_ = nowMs;
	pausedTotalMs_ = 0;
	pausedAtMs_ = 0;
}

void RecordingClock::pause(int64_t nowMs)
{
	if (!running_ || paused_) {
		return;
	}
	paused_ = true;
	pausedAtMs_ = nowMs;
}

void RecordingClock::resume(int64_t nowMs)
{
	if (!running_ || !paused_) {
		return;
	}
	pausedTotalMs_ += nowMs - pausedAtMs_;
	paused_ = false;
}

void RecordingClock::stop()
{
	running_ = false;
	paused_ = false;
}

int64_t RecordingClock::elapsedMs(int64_t nowMs) const
{
	if (!running_) {
		return 0;
	}
	const int64_t end = paused_ ? pausedAtMs_ : nowMs;
	const int64_t elapsed = end - startMs_ - pausedTotalMs_;
	return elapsed > 0 ? elapsed : 0;
}

} // namespace voltz
