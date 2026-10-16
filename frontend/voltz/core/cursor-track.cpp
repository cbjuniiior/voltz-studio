#include "cursor-track.hpp"

#include <cstdio>

namespace voltz {

CapturePoint mapToCapture(const CaptureMapping &mapping, int desktopX, int desktopY)
{
	CapturePoint point;
	const MonitorRect &rect = mapping.monitor;
	if (rect.width <= 0 || rect.height <= 0 || mapping.captureWidth <= 0 || mapping.captureHeight <= 0) {
		return point;
	}
	const int64_t dx = static_cast<int64_t>(desktopX) - rect.left;
	const int64_t dy = static_cast<int64_t>(desktopY) - rect.top;
	if (dx < 0 || dy < 0 || dx >= rect.width || dy >= rect.height) {
		return point;
	}
	point.inside = true;
	point.x = static_cast<int>(dx * mapping.captureWidth / rect.width);
	point.y = static_cast<int>(dy * mapping.captureHeight / rect.height);
	return point;
}

namespace {

const char *eventName(CursorEventType type)
{
	switch (type) {
	case CursorEventType::Move:
		return "move";
	case CursorEventType::Down:
		return "down";
	case CursorEventType::Up:
		return "up";
	case CursorEventType::Scroll:
		return "scroll";
	case CursorEventType::Outside:
		break;
	}
	return "fora";
}

char buttonCode(MouseButton button)
{
	switch (button) {
	case MouseButton::Right:
		return 'r';
	case MouseButton::Middle:
		return 'm';
	case MouseButton::Left:
	case MouseButton::None:
		break;
	}
	return 'l';
}

// 120 unidades = 1 "notch", arredondado ao milésimo, sem printf de ponto flutuante (independe da locale C).
std::string formatNotches(int wheelDelta)
{
	const int64_t scaled = static_cast<int64_t>(wheelDelta) * 1000;
	const int64_t thousandths = (scaled + (scaled >= 0 ? 60 : -60)) / 120;
	const bool negative = thousandths < 0;
	const int64_t magnitude = negative ? -thousandths : thousandths;
	std::string text = std::to_string(magnitude / 1000);
	const int64_t fraction = magnitude % 1000;
	if (fraction != 0) {
		char digits[8];
		std::snprintf(digits, sizeof(digits), "%03lld", static_cast<long long>(fraction));
		std::string decimals = digits;
		while (!decimals.empty() && decimals.back() == '0') {
			decimals.pop_back();
		}
		text += "." + decimals;
	}
	return negative ? "-" + text : text;
}

CursorEventType typeFor(RawMouseKind kind)
{
	switch (kind) {
	case RawMouseKind::Down:
		return CursorEventType::Down;
	case RawMouseKind::Up:
		return CursorEventType::Up;
	case RawMouseKind::Wheel:
		return CursorEventType::Scroll;
	case RawMouseKind::Move:
		break;
	}
	return CursorEventType::Move;
}

} // namespace

std::string cursorLine(const CursorEvent &event)
{
	char buffer[128];
	if (event.type == CursorEventType::Outside) {
		std::snprintf(buffer, sizeof(buffer), "{\"t\":%lld,\"e\":\"fora\"}", static_cast<long long>(event.tMs));
		return buffer;
	}
	std::snprintf(buffer, sizeof(buffer), "{\"t\":%lld,\"x\":%d,\"y\":%d,\"e\":\"%s\"",
		      static_cast<long long>(event.tMs), event.x, event.y, eventName(event.type));
	std::string line = buffer;
	if (event.type == CursorEventType::Down || event.type == CursorEventType::Up) {
		line += ",\"b\":\"";
		line += buttonCode(event.button);
		line += '"';
	} else if (event.type == CursorEventType::Scroll) {
		line += ",\"d\":";
		line += formatNotches(event.wheelDelta);
	}
	line += '}';
	return line;
}

CursorSampler::CursorSampler(const CaptureMapping &mapping, int64_t t0Ns) : mapping_(mapping), t0Ns_(t0Ns) {}

int64_t CursorSampler::relativeMs(int64_t tNs) const
{
	const int64_t delta = tNs - t0Ns_;
	return delta > 0 ? delta / 1000000 : 0;
}

std::vector<CursorEvent> CursorSampler::initial(int64_t tNs, int desktopX, int desktopY)
{
	RawMouseEvent raw;
	raw.tNs = tNs;
	raw.kind = RawMouseKind::Move;
	raw.x = desktopX;
	raw.y = desktopY;
	return process({raw});
}

std::vector<CursorEvent> CursorSampler::process(const std::vector<RawMouseEvent> &batch)
{
	std::vector<CursorEvent> out;
	bool hasPending = false;
	CursorEvent pending;
	auto flushPending = [&]() {
		if (hasPending && (!hasLast_ || pending.x != lastX_ || pending.y != lastY_)) {
			out.push_back(pending);
			hasLast_ = true;
			lastX_ = pending.x;
			lastY_ = pending.y;
		}
		hasPending = false;
	};

	for (const RawMouseEvent &raw : batch) {
		const CapturePoint point = mapToCapture(mapping_, raw.x, raw.y);
		const int64_t t = relativeMs(raw.tNs);
		if (!point.inside) {
			flushPending();
			if (!outside_) {
				CursorEvent outsideEvent;
				outsideEvent.tMs = t;
				outsideEvent.type = CursorEventType::Outside;
				out.push_back(outsideEvent);
				outside_ = true;
				hasLast_ = false;
			}
			continue;
		}
		outside_ = false;
		if (raw.kind == RawMouseKind::Move) {
			pending = CursorEvent{t, CursorEventType::Move, point.x, point.y};
			hasPending = true;
			continue;
		}
		flushPending();
		out.push_back(CursorEvent{t, typeFor(raw.kind), point.x, point.y, raw.button, raw.wheelDelta});
		hasLast_ = true;
		lastX_ = point.x;
		lastY_ = point.y;
	}
	flushPending();
	return out;
}

} // namespace voltz
