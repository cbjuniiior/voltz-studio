#pragma once

#include <voltz/core/cursor-track.hpp>

#include <string>

class QScreen;

namespace voltz {

struct MonitorDescription {
	bool found = false;
	std::string id;         // monitor_id da fonte
	MonitorRect rect;       // pixels físicos do desktop virtual
	double scale = 1.0;     // DPI efetivo / 96
	std::string deviceName; // nome GDI ("\\.\DISPLAY1"); NÃO é o QScreen::name() (no Qt 6 é o nome amigável)
};

// Monitor de uma fonte monitor_capture pelo "monitor_id" (mesma busca do plugin win-capture: DeviceID da interface
// do monitor e, se não achar, o nome GDI "\\.\DISPLAYn"). found = false se o monitor não existe mais.
MonitorDescription findMonitor(const std::string &monitorId);

// QScreen do monitor de nome GDI `deviceName` (MonitorDescription::deviceName), pelo HMONITOR de cada tela
// (QNativeInterface::QWindowsScreen::handle() + GetMonitorInfo). Não compara QScreen::name(): no Qt 6 do Windows ele é o
// nome amigável do monitor ("DELL U2720Q"), não o "\\.\DISPLAYn". nullptr se nenhuma tela corresponde. Thread da UI.
QScreen *screenForMonitor(const std::string &deviceName);

} // namespace voltz
