#include "WindowChrome.hpp"

#include <widgets/OBSQTDisplay.hpp>

#include <QWidget>

#ifdef _WIN32
#include <Windows.h>
#include <dwmapi.h>
#endif

namespace voltz {

void applyDarkTitleBar(QWidget *window)
{
#ifdef _WIN32
	HWND hwnd = reinterpret_cast<HWND>(window->winId());
	const BOOL dark = TRUE;
	DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
	const COLORREF caption = RGB(0x11, 0x11, 0x14);
	DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));
	const COLORREF text = RGB(0xF2, 0xF2, 0xF3);
	DwmSetWindowAttribute(hwnd, 36 /* DWMWA_TEXT_COLOR */, &text, sizeof(text));
#else
	(void)window;
#endif
}

void forwardDisplayEvents(QWidget *window, void *message)
{
#ifdef _WIN32
	const MSG &msg = *static_cast<MSG *>(message);
	if (msg.message == WM_MOVE) {
		for (OBSQTDisplay *const display : window->findChildren<OBSQTDisplay *>()) {
			display->OnMove();
		}
	} else if (msg.message == WM_DISPLAYCHANGE) {
		for (OBSQTDisplay *const display : window->findChildren<OBSQTDisplay *>()) {
			display->OnDisplayChange();
		}
	}
#else
	(void)window;
	(void)message;
#endif
}

} // namespace voltz
