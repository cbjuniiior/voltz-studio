#pragma once

#include <voltz/core/cursor-track.hpp>

#include <string>

namespace voltz {

struct MonitorDescription {
	bool found = false;
	std::string id;         // monitor_id da fonte
	MonitorRect rect;       // pixels físicos do desktop virtual
	double scale = 1.0;     // DPI efetivo / 96
	std::string deviceName; // nome GDI ("\\.\DISPLAY1"): o mesmo de QScreen::name() no Windows
};

// Monitor de uma fonte monitor_capture pelo "monitor_id" (mesma busca do plugin win-capture: DeviceID da interface
// do monitor e, se não achar, o nome GDI "\\.\DISPLAYn"). found = false se o monitor não existe mais.
MonitorDescription findMonitor(const std::string &monitorId);

} // namespace voltz
