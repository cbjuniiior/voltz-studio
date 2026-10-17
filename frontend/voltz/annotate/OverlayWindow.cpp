#include "OverlayWindow.hpp"

#include <voltz/core/annotation-raster.hpp>

#include <util/base.h>
#include <util/platform.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace voltz {

#ifdef _WIN32
namespace {

const wchar_t *kOverlayClass = L"VoltzAnnotationOverlay";
// Transparente a cliques (TRANSPARENT + LAYERED), nunca ativa (NOACTIVATE), fora da barra de tarefas e do Alt+Tab
// (TOOLWINDOW), sempre no topo (TOPMOST). Obrigatórios em toda a vida da janela no Plano 4B.
constexpr LONG_PTR kClickThroughStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW |
					WS_EX_TOPMOST;

LRESULT CALLBACK overlayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_NCHITTEST:
		return HTTRANSPARENT; /* o clique segue para a janela de baixo */
	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;
	case WM_DPICHANGED:
		return 0; /* o retângulo é sempre o do monitor, em pixels físicos */
	case WM_CLOSE:
		return 0; /* só o Voltz destrói a sobreposição */
	default:
		break;
	}
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool registerOverlayClass()
{
	static bool registered = false;
	if (!registered) {
		WNDCLASSEXW windowClass = {};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = overlayProc;
		windowClass.hInstance = GetModuleHandleW(nullptr);
		windowClass.lpszClassName = kOverlayClass;
		registered = RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	}
	return registered;
}

} // namespace
#endif

OverlayWindow::~OverlayWindow()
{
	destroy();
}

bool OverlayWindow::create(const MonitorRect &rect)
{
#ifdef _WIN32
	destroy();
	if (rect.width <= 0 || rect.height <= 0 || !registerOverlayClass()) {
		return false;
	}
	HWND hwnd = CreateWindowExW(static_cast<DWORD>(kClickThroughStyle), kOverlayClass, L"Voltz Anotações", WS_POPUP,
				    rect.left, rect.top, rect.width, rect.height, nullptr, nullptr,
				    GetModuleHandleW(nullptr), nullptr);
	if (!hwnd) {
		blog(LOG_WARNING, "[voltz] anotacoes: CreateWindowEx falhou (%lu)", GetLastError());
		return false;
	}
	hwnd_ = hwnd;
	rect_ = rect;
	visible_ = false;
	return true;
#else
	(void)rect;
	return false;
#endif
}

void OverlayWindow::destroy()
{
#ifdef _WIN32
	if (hwnd_) {
		DestroyWindow(static_cast<HWND>(hwnd_));
		hwnd_ = nullptr;
	}
	releaseBuffer();
	visible_ = false;
#endif
}

bool OverlayWindow::excludeFromCapture()
{
#ifdef _WIN32
	HWND hwnd = static_cast<HWND>(hwnd_);
	if (!hwnd || !SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)) {
		return false;
	}
	DWORD affinity = 0;
	return GetWindowDisplayAffinity(hwnd, &affinity) && affinity == WDA_EXCLUDEFROMCAPTURE;
#else
	return false;
#endif
}

void OverlayWindow::setRect(const MonitorRect &rect)
{
	rect_ = rect;
}

void OverlayWindow::ensureClickThrough()
{
#ifdef _WIN32
	HWND hwnd = static_cast<HWND>(hwnd_);
	const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
	if ((style & kClickThroughStyle) != kClickThroughStyle) {
		SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style | kClickThroughStyle);
		blog(LOG_WARNING, "[voltz] anotacoes: estilo da sobreposicao refeito (transparente a cliques)");
	}
	if ((style & WS_EX_TOPMOST) == 0) {
		/* o bit no estilo não muda a ordem Z: só o SetWindowPos com HWND_TOPMOST devolve o "sempre no topo" */
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		blog(LOG_WARNING, "[voltz] anotacoes: sobreposicao de volta ao topo");
	}
#endif
}

bool OverlayWindow::ensureBuffer(int width, int height)
{
#ifdef _WIN32
	if (bits_ && width == bufferWidth_ && height == bufferHeight_) {
		return true;
	}
	releaseBuffer();
	if (width <= 0 || height <= 0) {
		return false;
	}
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height; /* de cima para baixo, como o RasterImage */
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	HDC memoryDc = CreateCompatibleDC(nullptr);
	if (!memoryDc) {
		return false;
	}
	void *bits = nullptr;
	HBITMAP bitmap = CreateDIBSection(memoryDc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
	if (!bitmap || !bits) {
		if (bitmap) {
			DeleteObject(bitmap);
		}
		DeleteDC(memoryDc);
		return false;
	}
	previousBitmap_ = SelectObject(memoryDc, bitmap);
	memoryDc_ = memoryDc;
	bitmap_ = bitmap;
	bits_ = static_cast<uint8_t *>(bits);
	bufferWidth_ = width;
	bufferHeight_ = height;
	return true;
#else
	(void)width;
	(void)height;
	return false;
#endif
}

void OverlayWindow::releaseBuffer()
{
#ifdef _WIN32
	if (memoryDc_) {
		SelectObject(static_cast<HDC>(memoryDc_), static_cast<HGDIOBJ>(previousBitmap_));
		DeleteDC(static_cast<HDC>(memoryDc_));
	}
	if (bitmap_) {
		DeleteObject(static_cast<HGDIOBJ>(bitmap_));
	}
#endif
	memoryDc_ = nullptr;
	bitmap_ = nullptr;
	previousBitmap_ = nullptr;
	bits_ = nullptr;
	bufferWidth_ = 0;
	bufferHeight_ = 0;
}

void OverlayWindow::hide()
{
#ifdef _WIN32
	if (bits_) {
		clearRaster(RasterImage{bits_, bufferWidth_, bufferHeight_, bufferWidth_ * 4});
	}
	if (visible_ && hwnd_) {
		ShowWindow(static_cast<HWND>(hwnd_), SW_HIDE);
	}
	visible_ = false;
#endif
}

void OverlayWindow::render(const std::vector<const Stroke *> &strokes, int captureWidth, int captureHeight,
			   const PixelRect *dirty)
{
#ifdef _WIN32
	HWND hwnd = static_cast<HWND>(hwnd_);
	if (!hwnd) {
		return;
	}
	ensureClickThrough();
	if (strokes.empty() || captureWidth <= 0 || captureHeight <= 0) {
		hide();
		return;
	}
	const bool sameBuffer = bits_ && bufferWidth_ == rect_.width && bufferHeight_ == rect_.height;
	const bool partial = dirty && sameBuffer && visible_ && !needsFull_;
	if (!ensureBuffer(rect_.width, rect_.height)) {
		blog(LOG_WARNING, "[voltz] anotacoes: sem memoria para a sobreposicao %dx%d", rect_.width,
		     rect_.height);
		hide();
		return;
	}
	PixelRect area{0, 0, bufferWidth_, bufferHeight_};
	if (partial) {
		area = intersectRects(*dirty, area);
		if (area.empty()) {
			return; /* nada mudou na imagem */
		}
	}
	const int64_t start = static_cast<int64_t>(os_gettime_ns());
	const RasterImage image{bits_, bufferWidth_, bufferHeight_, bufferWidth_ * 4};
	const double scaleX = static_cast<double>(bufferWidth_) / static_cast<double>(captureWidth);
	const double scaleY = static_cast<double>(bufferHeight_) / static_cast<double>(captureHeight);
	if (partial) {
		redrawRegion(image, strokes, scaleX, scaleY, area);
	} else {
		clearRaster(image);
		for (const Stroke *stroke : strokes) {
			rasterizeStroke(image, *stroke, scaleX, scaleY);
		}
	}
	GdiFlush();
	POINT position{rect_.left, rect_.top};
	SIZE size{bufferWidth_, bufferHeight_};
	POINT source{0, 0};
	BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
	RECT dirtyRect{area.left, area.top, area.right, area.bottom};
	UPDATELAYEREDWINDOWINFO update = {};
	update.cbSize = sizeof(update);
	update.pptDst = &position;
	update.psize = &size;
	update.hdcSrc = static_cast<HDC>(memoryDc_);
	update.pptSrc = &source;
	update.pblend = &blend;
	update.dwFlags = ULW_ALPHA;
	update.prcDirty = partial ? &dirtyRect : nullptr;
	if (!UpdateLayeredWindowIndirect(hwnd, &update)) {
		blog(LOG_WARNING, "[voltz] anotacoes: UpdateLayeredWindowIndirect falhou (%lu)", GetLastError());
		needsFull_ = true; /* a janela pode ter ficado com a imagem velha: o próximo desenho publica tudo */
		return;
	}
	needsFull_ = false;
	const int64_t elapsed = static_cast<int64_t>(os_gettime_ns()) - start;
	if (partial) {
		partialRenders_++;
		partialTotalNs_ += elapsed;
		partialMaxNs_ = std::max(partialMaxNs_, elapsed);
	} else {
		fullRenders_++;
		fullMaxNs_ = std::max(fullMaxNs_, elapsed);
	}
	if (!visible_) {
		/* mostra SEM ativar: o foco do usuário não muda */
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
		visible_ = true;
	}
#else
	(void)strokes;
	(void)captureWidth;
	(void)captureHeight;
	(void)dirty;
#endif
}

std::string OverlayWindow::pixelHex(int x, int y) const
{
	if (!bits_ || x < 0 || y < 0 || x >= bufferWidth_ || y >= bufferHeight_) {
		return {};
	}
	const uint8_t *pixel =
		bits_ + (static_cast<size_t>(y) * static_cast<size_t>(bufferWidth_) + static_cast<size_t>(x)) * 4;
	char text[16];
	std::snprintf(text, sizeof(text), "#%02X%02X%02X%02X", pixel[3], pixel[2], pixel[1], pixel[0]);
	return text;
}

void OverlayWindow::resetStats()
{
	partialRenders_ = 0;
	fullRenders_ = 0;
	partialTotalNs_ = 0;
	partialMaxNs_ = 0;
	fullMaxNs_ = 0;
}

OverlayWindow::Info OverlayWindow::info() const
{
	Info info;
	info.rect = rect_;
	info.partialRenders = partialRenders_;
	info.fullRenders = fullRenders_;
	info.partialTotalNs = partialTotalNs_;
	info.partialMaxNs = partialMaxNs_;
	info.fullMaxNs = fullMaxNs_;
#ifdef _WIN32
	HWND hwnd = static_cast<HWND>(hwnd_);
	if (!hwnd) {
		return info;
	}
	info.exists = IsWindow(hwnd) != FALSE;
	info.visible = IsWindowVisible(hwnd) != FALSE;
	info.hwnd = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(hwnd));
	info.exStyle = static_cast<int64_t>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
	DWORD affinity = 0;
	if (GetWindowDisplayAffinity(hwnd, &affinity)) {
		info.affinity = static_cast<uint32_t>(affinity);
	}
#endif
	return info;
}

} // namespace voltz
