#include "MonitorLocator.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellscalingapi.h>
#endif

namespace voltz {

#ifdef _WIN32
namespace {

struct MonitorSearch {
	std::string id;
	bool byGdiName = false;
	HMONITOR handle = nullptr;
	RECT rect = {};
};

// Mesma busca de plugins/win-capture/duplicator-monitor-capture.c (enum_monitor / enum_monitor_fallback).
BOOL CALLBACK matchMonitor(HMONITOR handle, HDC, LPRECT rect, LPARAM param)
{
	auto *search = reinterpret_cast<MonitorSearch *>(param);
	MONITORINFOEXA info = {};
	info.cbSize = sizeof(info);
	if (!GetMonitorInfoA(handle, reinterpret_cast<LPMONITORINFO>(&info))) {
		return TRUE;
	}
	bool match = false;
	if (search->byGdiName) {
		match = search->id == info.szDevice;
	} else {
		DISPLAY_DEVICEA device = {};
		device.cb = sizeof(device);
		if (EnumDisplayDevicesA(info.szDevice, 0, &device, EDD_GET_DEVICE_INTERFACE_NAME)) {
			match = search->id == device.DeviceID;
		}
	}
	if (match) {
		search->handle = handle;
		search->rect = *rect;
	}
	return match ? FALSE : TRUE;
}

} // namespace
#endif

MonitorDescription findMonitor(const std::string &monitorId)
{
	MonitorDescription result;
#ifdef _WIN32
	if (monitorId.empty()) {
		return result;
	}
	MonitorSearch search;
	search.id = monitorId;
	EnumDisplayMonitors(nullptr, nullptr, matchMonitor, reinterpret_cast<LPARAM>(&search));
	if (!search.handle) {
		search.byGdiName = true;
		EnumDisplayMonitors(nullptr, nullptr, matchMonitor, reinterpret_cast<LPARAM>(&search));
	}
	if (!search.handle) {
		return result;
	}
	result.found = true;
	result.id = monitorId;
	result.rect = MonitorRect{static_cast<int>(search.rect.left), static_cast<int>(search.rect.top),
				  static_cast<int>(search.rect.right - search.rect.left),
				  static_cast<int>(search.rect.bottom - search.rect.top)};
	UINT dpiX = 96;
	UINT dpiY = 96;
	if (SUCCEEDED(GetDpiForMonitor(search.handle, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
		result.scale = static_cast<double>(dpiX) / 96.0;
	}
#else
	(void)monitorId;
#endif
	return result;
}

} // namespace voltz
