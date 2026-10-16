#include "LiveMouse.hpp"
#include "MonitorLocator.hpp"

#include <voltz/core/live-protocol.hpp>
#include <voltz/core/mouse-style.hpp>
#include <voltz/shell/MouseStyleStore.hpp>
#include <voltz/shell/SharedSources.hpp>

#include <obs.hpp>
#include <util/platform.h>
#include <util/threading.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace voltz {

namespace {

constexpr int kPollMs = 500;
constexpr unsigned long kTickMs = 16;
constexpr int64_t kPositionIntervalNs = 16000000;

int queryWanted()
{
	uint8_t stack[128];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	if (!proc_handler_call(obs_get_proc_handler(), live::kProcWanted, &params)) {
		return 0;
	}
	return static_cast<int>(calldata_int(&params, "count"));
}

void sendPosition(const CapturePoint &point)
{
	uint8_t stack[256];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_int(&params, "x", point.x);
	calldata_set_int(&params, "y", point.y);
	calldata_set_bool(&params, "inside", point.inside);
	proc_handler_call(obs_get_proc_handler(), live::kProcPosition, &params);
}

void sendClick(const CapturePoint &point, MouseButton button, int64_t tNs)
{
	uint8_t stack[256];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_int(&params, "x", point.x);
	calldata_set_int(&params, "y", point.y);
	calldata_set_int(&params, "button", live::buttonCode(button));
	calldata_set_int(&params, "t_ns", tNs);
	proc_handler_call(obs_get_proc_handler(), live::kProcClick, &params);
}

void sendStyle(const std::string &json)
{
	calldata_t params;
	calldata_init(&params);
	calldata_set_string(&params, "json", json.c_str());
	if (!proc_handler_call(obs_get_proc_handler(), live::kProcStyle, &params)) {
		blog(LOG_WARNING, "[voltz] plugin voltz-sources ausente: estilo do mouse nao enviado");
	}
	calldata_free(&params);
}

#ifdef _WIN32
const wchar_t *kWindowClass = L"VoltzLiveMouse";

bool registerWindowClass()
{
	static bool registered = false;
	if (!registered) {
		WNDCLASSEXW windowClass = {};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = DefWindowProcW;
		windowClass.hInstance = GetModuleHandleW(nullptr);
		windowClass.lpszClassName = kWindowClass;
		registered = RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	}
	return registered;
}

// Botão que desceu neste pacote de Raw Input. O Raw Input fala em botões físicos: com SM_SWAPBUTTON o físico
// esquerdo é o botão secundário (direito).
MouseButton pressedButton(USHORT flags, bool swapped)
{
	if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) {
		return swapped ? MouseButton::Right : MouseButton::Left;
	}
	if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
		return swapped ? MouseButton::Left : MouseButton::Right;
	}
	if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
		return MouseButton::Middle;
	}
	return MouseButton::None;
}
#endif

} // namespace

LiveMouse::LiveMouse(MouseStyleStore *styles, QObject *parent) : QObject(parent), styles_(styles)
{
	testMode_ = live::testModeEnabled();
	if (testMode_) {
		blog(LOG_WARNING, "[voltz] mouse ao vivo: modo de teste (%s), o mouse real nao e lido", live::kTestEnv);
	}
	connect(styles_, &MouseStyleStore::styleChanged, this, [this]() { pushStyle(); });
	pushStyle();
	poll_.setInterval(kPollMs);
	connect(&poll_, &QTimer::timeout, this, [this]() { poll(); });
	poll_.start();
}

LiveMouse::~LiveMouse()
{
	shutdown();
}

void LiveMouse::shutdown()
{
	shutdown_ = true;
	poll_.stop();
	stop();
}

void LiveMouse::pushStyle()
{
	if (!shutdown_) {
		sendStyle(mouseStyleToJson(styles_->style()));
	}
}

void LiveMouse::poll()
{
	if (shutdown_) {
		return;
	}
	const int wanted = queryWanted();
	if (testMode_) {
		return;
	}
	if (wanted <= 0) {
		stop();
		return;
	}
	refreshMapping();
	if (!running_) {
		start();
	}
}

void LiveMouse::refreshMapping()
{
	CaptureMapping mapping;
	OBSSource screen = findSharedSource(SharedSource::Screen);
	if (screen && strcmp(obs_source_get_unversioned_id(screen), "monitor_capture") == 0) {
		OBSDataAutoRelease settings = obs_source_get_settings(screen);
		const MonitorDescription monitor = findMonitor(obs_data_get_string(settings, "monitor_id"));
		if (monitor.found) {
			mapping = CaptureMapping{monitor.rect, monitor.rect.width, monitor.rect.height};
		}
	}
	const int state = mapping.captureWidth > 0 ? 1 : 0;
	if (state != mappingState_) {
		mappingState_ = state;
		if (state == 1) {
			blog(LOG_INFO, "[voltz] mouse ao vivo: captura de monitor %dx%d", mapping.captureWidth,
			     mapping.captureHeight);
		} else {
			blog(LOG_INFO, "[voltz] mouse ao vivo: sem captura de monitor compartilhada (fora)");
		}
	}
	std::lock_guard<std::mutex> lock(mutex_);
	mapping_ = mapping;
}

void LiveMouse::start()
{
#ifdef _WIN32
	std::promise<void> ready;
	std::future<void> started = ready.get_future();
	thread_ = std::thread(&LiveMouse::threadMain, this, std::move(ready));
	started.wait();
	running_ = true;
#endif
}

void LiveMouse::stop()
{
#ifdef _WIN32
	if (!running_) {
		return;
	}
	running_ = false;
	/* WM_QUIT faz o laço da thread terminar; a fila já existe (criada antes de publicar o id). */
	const DWORD threadId = static_cast<DWORD>(threadId_.load());
	for (int attempt = 0; attempt < 200; attempt++) {
		if (PostThreadMessageW(threadId, WM_QUIT, 0, 0)) {
			break;
		}
		Sleep(5);
	}
	if (thread_.joinable()) {
		thread_.join();
	}
	sendPosition(CapturePoint{}); // some o halo
	blog(LOG_INFO, "[voltz] mouse ao vivo: desligado");
#endif
}

CapturePoint LiveMouse::currentPoint()
{
#ifdef _WIN32
	POINT cursor;
	if (!GetPhysicalCursorPos(&cursor)) {
		return CapturePoint{};
	}
	std::lock_guard<std::mutex> lock(mutex_);
	return mapToCapture(mapping_, cursor.x, cursor.y);
#else
	return CapturePoint{};
#endif
}

void LiveMouse::threadMain(std::promise<void> ready)
{
#ifdef _WIN32
	os_set_thread_name("voltz-mouse-live");
	MSG msg;
	/* cria a fila de mensagens desta thread antes de publicar o id (o PostThreadMessage do stop precisa dela) */
	PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
	threadId_.store(GetCurrentThreadId());
	HWND window = nullptr;
	if (registerWindowClass()) {
		window = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
					 GetModuleHandleW(nullptr), nullptr);
	}
	bool rawInput = false;
	if (window) {
		RAWINPUTDEVICE device = {};
		device.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
		device.usUsage = 0x02;     // HID_USAGE_GENERIC_MOUSE
		device.dwFlags = RIDEV_INPUTSINK;
		device.hwndTarget = window;
		rawInput = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
	}
	const bool swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
	blog(LOG_INFO, "[voltz] mouse ao vivo: ligado (cliques %s)", rawInput ? "por raw input" : "indisponiveis");
	ready.set_value();

	CapturePoint last;
	bool sent = false;
	int64_t lastSendNs = 0;
	bool quit = false;
	while (!quit) {
		MsgWaitForMultipleObjects(0, nullptr, FALSE, kTickMs, QS_ALLINPUT);
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				quit = true;
				break;
			}
			if (msg.message == WM_INPUT) {
				RAWINPUT raw;
				UINT size = sizeof(raw);
				if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg.lParam), RID_INPUT, &raw, &size,
						    sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1) &&
				    raw.header.dwType == RIM_TYPEMOUSE) {
					const MouseButton button = pressedButton(raw.data.mouse.usButtonFlags, swapped);
					if (button != MouseButton::None) {
						const CapturePoint point = currentPoint();
						if (point.inside) {
							sendClick(point, button, static_cast<int64_t>(os_gettime_ns()));
						}
					}
				}
			}
			DispatchMessageW(&msg); // WM_INPUT: o DefWindowProc libera o pacote
		}
		const int64_t now = static_cast<int64_t>(os_gettime_ns());
		if (quit || now - lastSendNs < kPositionIntervalNs) {
			continue;
		}
		lastSendNs = now;
		const CapturePoint point = currentPoint();
		if (!sent || point.inside != last.inside || point.x != last.x || point.y != last.y) {
			sendPosition(point);
			last = point;
			sent = true;
		}
	}
	if (rawInput) {
		RAWINPUTDEVICE device = {};
		device.usUsagePage = 0x01;
		device.usUsage = 0x02;
		device.dwFlags = RIDEV_REMOVE;
		device.hwndTarget = nullptr;
		RegisterRawInputDevices(&device, 1, sizeof(device));
	}
	if (window) {
		DestroyWindow(window);
	}
#else
	ready.set_value();
#endif
}

} // namespace voltz
