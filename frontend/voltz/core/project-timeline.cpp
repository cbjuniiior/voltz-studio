#include "project-timeline.hpp"

#include <algorithm>

namespace voltz {

int64_t netDuration(int64_t grossMs, const std::vector<PauseSpan> &pauses)
{
	if (grossMs <= 0) {
		return 0;
	}
	int64_t paused = 0;
	for (const PauseSpan &pause : pauses) {
		const int64_t start = std::clamp<int64_t>(pause.startMs, 0, grossMs);
		const int64_t end = std::clamp<int64_t>(pause.endMs < 0 ? grossMs : pause.endMs, 0, grossMs);
		if (end > start) {
			paused += end - start;
		}
	}
	return std::max<int64_t>(0, grossMs - paused);
}

void ProjectTimeline::start(int64_t t0Ns)
{
	started_ = true;
	paused_ = false;
	stopped_ = false;
	t0Ns_ = t0Ns;
	stopNs_ = 0;
	pauses_.clear();
}

void ProjectTimeline::pause(int64_t nowNs)
{
	if (!started_ || stopped_ || paused_) {
		return;
	}
	paused_ = true;
	pauses_.push_back(PauseSpan{relativeMs(nowNs), -1});
}

void ProjectTimeline::resume(int64_t nowNs)
{
	if (!started_ || stopped_ || !paused_) {
		return;
	}
	paused_ = false;
	pauses_.back().endMs = std::max(pauses_.back().startMs, relativeMs(nowNs));
}

void ProjectTimeline::stop(int64_t nowNs)
{
	if (!started_ || stopped_) {
		return;
	}
	if (paused_) {
		resume(nowNs);
	}
	stopped_ = true;
	stopNs_ = nowNs;
}

int64_t ProjectTimeline::relativeMs(int64_t nowNs) const
{
	if (!started_) {
		return 0;
	}
	const int64_t delta = nowNs - t0Ns_;
	return delta > 0 ? delta / 1000000 : 0;
}

int64_t ProjectTimeline::grossMs(int64_t nowNs) const
{
	return relativeMs(stopped_ ? stopNs_ : nowNs);
}

int64_t ProjectTimeline::netMs(int64_t nowNs) const
{
	return netDuration(grossMs(nowNs), pauses_);
}

} // namespace voltz
