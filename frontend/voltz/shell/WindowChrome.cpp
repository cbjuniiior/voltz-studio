#include "WindowChrome.hpp"

#include <widgets/OBSQTDisplay.hpp>

#include <util/base.h>

#include <QFile>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QWidget>

#ifdef _WIN32
#include <Windows.h>
#include <dwmapi.h>
#include <ShObjIdl.h>
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

void setTaskbarState(QWidget *window, TaskbarState state)
{
#ifdef _WIN32
	/* Mesmo caminho do TaskbarOverlaySetStatus do OBS (platform-windows.cpp), mas no HWND da casca:
	 * o do OBS aponta para a OBSBasic oculta, que não tem botão na barra de tarefas. */
	ITaskbarList3 *taskbar = nullptr;
	if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar)))) {
		return;
	}
	if (FAILED(taskbar->HrInit())) {
		taskbar->Release();
		return;
	}
	HWND hwnd = reinterpret_cast<HWND>(window->winId());
	if (state == TaskbarState::Idle) {
		taskbar->SetOverlayIcon(hwnd, nullptr, nullptr);
	} else {
		const QIcon icon(state == TaskbarState::Recording ? QStringLiteral(":/res/images/active.png")
								  : QStringLiteral(":/res/images/paused.png"));
		const int size = GetSystemMetrics(SM_CXSMICON);
		HICON hicon = icon.pixmap(size, size).toImage().toHICON();
		if (hicon) {
			taskbar->SetOverlayIcon(hwnd, hicon, nullptr);
			DestroyIcon(hicon);
		}
	}
	taskbar->Release();
#else
	(void)window;
	(void)state;
#endif
}

void logCaptureAffinity(QWidget *window)
{
#ifdef _WIN32
	DWORD affinity = 0;
	if (GetWindowDisplayAffinity(reinterpret_cast<HWND>(window->winId()), &affinity)) {
		blog(LOG_INFO, "[voltz] afinidade de captura da casca: 0x%lx (%s)",
		     static_cast<unsigned long>(affinity),
		     affinity == WDA_EXCLUDEFROMCAPTURE ? "fora da captura" : "visivel na captura");
	}
#else
	(void)window;
#endif
}

QString shellStyleSheet()
{
	QFile file(QStringLiteral(":/voltz/voltz-shell.qss"));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return QString();
	}
	return QString::fromUtf8(file.readAll());
}

} // namespace voltz
