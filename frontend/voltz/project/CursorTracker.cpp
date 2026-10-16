#include "CursorTracker.hpp"

#include <util/base.h>
#include <util/platform.h>
#include <util/threading.h>

#include <chrono>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace voltz {

namespace {

constexpr size_t kMaxQueued = 20000;
constexpr auto kTick = std::chrono::milliseconds(16);
constexpr auto kFlushEvery = std::chrono::seconds(1);
constexpr size_t kFlushBytes = 64 * 1024;

std::atomic<CursorTracker *> gActive{nullptr};

#ifdef _WIN32
FILE *openForWrite(const std::string &utf8Path)
{
	const int size = MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, nullptr, 0);
	if (size <= 0) {
		return nullptr;
	}
	std::wstring wide(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, wide.data(), size);
	return _wfopen(wide.c_str(), L"wb");
}

// Roda na thread do gancho a cada evento de mouse do sistema: precisa voltar em microssegundos. Sem IO, sem log, sem
// lock, sem alocação — só copia para o anel e passa o evento adiante.
LRESULT CALLBACK mouseHook(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION) {
		if (CursorTracker *tracker = gActive.load(std::memory_order_acquire)) {
			const auto *info = reinterpret_cast<const MSLLHOOKSTRUCT *>(lParam);
			RawMouseEvent event;
			event.tNs = static_cast<int64_t>(os_gettime_ns());
			event.x = info->pt.x;
			event.y = info->pt.y;
			bool known = true;
			switch (wParam) {
			case WM_MOUSEMOVE:
				event.kind = RawMouseKind::Move;
				break;
			case WM_LBUTTONDOWN:
				event.kind = RawMouseKind::Down;
				event.button = MouseButton::Left;
				break;
			case WM_LBUTTONUP:
				event.kind = RawMouseKind::Up;
				event.button = MouseButton::Left;
				break;
			case WM_RBUTTONDOWN:
				event.kind = RawMouseKind::Down;
				event.button = MouseButton::Right;
				break;
			case WM_RBUTTONUP:
				event.kind = RawMouseKind::Up;
				event.button = MouseButton::Right;
				break;
			case WM_MBUTTONDOWN:
				event.kind = RawMouseKind::Down;
				event.button = MouseButton::Middle;
				break;
			case WM_MBUTTONUP:
				event.kind = RawMouseKind::Up;
				event.button = MouseButton::Middle;
				break;
			case WM_MOUSEWHEEL:
				event.kind = RawMouseKind::Wheel;
				event.wheelDelta = static_cast<short>(HIWORD(info->mouseData));
				break;
			default: /* rolagem horizontal e botões X: fora do 3A */
				known = false;
				break;
			}
			if (known) {
				tracker->push(event);
			}
		}
	}
	return CallNextHookEx(nullptr, code, wParam, lParam);
}
#endif

} // namespace

bool CursorTracker::start(const std::string &utf8Path, const CaptureMapping &mapping, int64_t t0Ns)
{
#ifdef _WIN32
	if (running_) {
		return false;
	}
	CursorTracker *expected = nullptr;
	if (!gActive.compare_exchange_strong(expected, this)) {
		blog(LOG_WARNING, "[voltz] cursor: outro rastreador ja esta ativo");
		return false;
	}
	file_ = openForWrite(utf8Path);
	if (!file_) {
		gActive.store(nullptr);
		return false;
	}
	mapping_ = mapping;
	t0Ns_ = t0Ns;
	stopping_ = false;
	ring_.assign(kMaxQueued + 1, RawMouseEvent{});
	head_ = 0;
	tail_ = 0;
	lines_ = 0;
	dropped_ = 0;
	unhooked_ = false;
	hookThreadId_ = 0;

	std::promise<bool> ready;
	std::future<bool> installed = ready.get_future();
	hookThread_ = std::thread(&CursorTracker::hookThreadMain, this, std::move(ready));
	if (!installed.get()) {
		hookThread_.join();
		std::fclose(file_);
		file_ = nullptr;
		gActive.store(nullptr);
		return false;
	}
	writerThread_ = std::thread(&CursorTracker::writerThreadMain, this);
	running_ = true;
	blog(LOG_INFO, "[voltz] cursor: gancho do mouse instalado");
	return true;
#else
	(void)utf8Path;
	(void)mapping;
	(void)t0Ns;
	return false;
#endif
}

void CursorTracker::stop()
{
#ifdef _WIN32
	if (!running_) {
		return;
	}
	running_ = false;
	CursorTracker *self = this;
	gActive.compare_exchange_strong(self, nullptr);
	/* WM_QUIT faz o GetMessageW da thread do gancho voltar 0; ela mesma desinstala o gancho. A fila de mensagens já
	 * existe (criada antes de publicar o id), então o envio só falharia com a fila cheia — tenta de novo. */
	const DWORD hookThreadId = static_cast<DWORD>(hookThreadId_.load());
	bool posted = false;
	for (int attempt = 0; attempt < 200 && !posted; attempt++) {
		posted = PostThreadMessageW(hookThreadId, WM_QUIT, 0, 0) != FALSE;
		if (!posted) {
			Sleep(5);
		}
	}
	if (hookThread_.joinable()) {
		hookThread_.join();
	}
	/* Depois do join nenhum callback do gancho pode estar em curso (ele roda na thread que o instalou). */
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stopping_ = true;
	}
	wake_.notify_all();
	if (writerThread_.joinable()) {
		writerThread_.join();
	}
	if (file_) {
		std::fclose(file_);
		file_ = nullptr;
	}
	ring_.clear();
	ring_.shrink_to_fit();
	blog(LOG_INFO, "[voltz] cursor: gancho do mouse removido (%s)", unhooked_.load() ? "ok" : "falhou");
#endif
}

void CursorTracker::push(const RawMouseEvent &event)
{
	const size_t head = head_.load(std::memory_order_relaxed);
	const size_t next = head + 1 == ring_.size() ? 0 : head + 1;
	if (next == tail_.load(std::memory_order_acquire)) {
		dropped_.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	ring_[head] = event;
	head_.store(next, std::memory_order_release);
}

void CursorTracker::drain(std::vector<RawMouseEvent> &out)
{
	size_t tail = tail_.load(std::memory_order_relaxed);
	const size_t head = head_.load(std::memory_order_acquire);
	while (tail != head) {
		out.push_back(ring_[tail]);
		tail = tail + 1 == ring_.size() ? 0 : tail + 1;
	}
	tail_.store(tail, std::memory_order_release);
}

void CursorTracker::hookThreadMain(std::promise<bool> ready)
{
#ifdef _WIN32
	os_set_thread_name("voltz-cursor-hook");
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	MSG msg;
	/* cria a fila de mensagens desta thread antes de publicar o id (o PostThreadMessage do stop precisa dela) */
	PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
	hookThreadId_.store(GetCurrentThreadId());
	HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, mouseHook, GetModuleHandleW(nullptr), 0);
	if (!hook) {
		/* nada instalado: log seguro aqui */
		blog(LOG_WARNING, "[voltz] cursor: SetWindowsHookExW falhou (erro %lu)", GetLastError());
		ready.set_value(false);
		return;
	}
	ready.set_value(true);
	while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	unhooked_.store(UnhookWindowsHookEx(hook) != FALSE);
#else
	ready.set_value(false);
#endif
}

void CursorTracker::writerThreadMain()
{
	os_set_thread_name("voltz-cursor-writer");
	CursorSampler sampler(mapping_, t0Ns_);
	std::string buffer;
	auto append = [&](const std::vector<CursorEvent> &events) {
		for (const CursorEvent &event : events) {
			buffer += cursorLine(event);
			buffer += '\n';
		}
		lines_ += events.size();
	};
#ifdef _WIN32
	POINT point;
	if (GetPhysicalCursorPos(&point)) {
		append(sampler.initial(static_cast<int64_t>(os_gettime_ns()), point.x, point.y));
	}
#endif
	std::vector<RawMouseEvent> batch;
	auto lastFlush = std::chrono::steady_clock::now();
	bool done = false;
	while (!done) {
		{
			std::unique_lock<std::mutex> lock(mutex_);
			(void)wake_.wait_for(lock, kTick, [this]() { return stopping_; });
			done = stopping_;
		}
		/* No último giro o gancho já parou (join no stop): o anel tem tudo o que sobrou. */
		drain(batch);
		append(sampler.process(batch));
		batch.clear();
		const auto now = std::chrono::steady_clock::now();
		if (!buffer.empty() && (done || buffer.size() >= kFlushBytes || now - lastFlush >= kFlushEvery)) {
			std::fwrite(buffer.data(), 1, buffer.size(), file_);
			std::fflush(file_);
			buffer.clear();
			lastFlush = now;
		}
	}
}

} // namespace voltz
