#include "target-size.hpp"

namespace voltz {

namespace {
constexpr int kRetryTicks = 60;
} // namespace

TargetSize::~TargetSize()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (target_) {
		obs_weak_source_release(target_);
		target_ = nullptr;
	}
}

void TargetSize::resolve()
{
	if (target_) {
		obs_weak_source_release(target_);
		target_ = nullptr;
	}
	if (uuid_.empty()) {
		return;
	}
	obs_source_t *target = obs_get_source_by_uuid(uuid_.c_str());
	if (target) {
		target_ = obs_source_get_weak_source(target);
		obs_source_release(target);
	}
}

void TargetSize::setUuid(const std::string &uuid)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (uuid == uuid_ && target_) {
		return;
	}
	uuid_ = uuid;
	retryTicks_ = 0;
	resolve();
}

void TargetSize::tick()
{
	obs_source_t *target = nullptr;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!target_ && !uuid_.empty() && ++retryTicks_ >= kRetryTicks) {
			retryTicks_ = 0;
			resolve();
		}
		if (target_) {
			target = obs_weak_source_get_source(target_);
			if (!target) {
				/* a Tela foi removida: solta e volta a procurar pelo UUID */
				obs_weak_source_release(target_);
				target_ = nullptr;
			}
		}
	}
	uint32_t width = 0;
	uint32_t height = 0;
	if (target) {
		width = obs_source_get_width(target);
		height = obs_source_get_height(target);
		obs_source_release(target);
	}
	width_.store(width);
	height_.store(height);
}

} // namespace voltz
