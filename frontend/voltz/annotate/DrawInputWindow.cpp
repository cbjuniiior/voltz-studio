#include "DrawInputWindow.hpp"

#include <voltz/core/live-protocol.hpp>

#include <util/base.h>
#include <util/platform.h>
#include <util/threading.h>

#include <chrono>
#include <iterator>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#endif

namespace voltz {

struct DrawInputHandle {
	std::atomic<void *> hwnd{nullptr}; // HWND da janela de entrada (nulo antes de nascer e depois de destruída)
	std::atomic<bool> cancel{false}; // start() desistiu ou o dono foi destruído: a thread sai (conferido no tique)
	std::atomic<bool> activated{false};
};

std::vector<DrawCommand> takeDrawCommands(DrawShared &shared)
{
	std::lock_guard<std::mutex> lock(shared.mutex);
	shared.wakePending = false;
	std::vector<DrawCommand> out;
	out.swap(shared.commands);
	return out;
}

#ifdef _WIN32
namespace {

constexpr UINT_PTR kTickTimer = 1;
constexpr UINT kTickMs = 100;
// Sem WM_TIMER por 2 tiques (a fila nunca esvazia com o mouse em movimento): o laço roda o tique à mão.
constexpr int64_t kLateTickNs = 2LL * kTickMs * 1000000LL;
const wchar_t *kInputClass = L"VoltzDrawInput";

// Mensagens registradas (não WM_APP + n): um pedido postado para um HWND já reciclado não mexe em janela alheia.
UINT registeredMessage(const wchar_t *name, UINT fallback)
{
	const UINT message = RegisterWindowMessageW(name);
	return message != 0 ? message : fallback;
}

UINT exitMessage() // wParam = DrawExit
{
	static const UINT message = registeredMessage(L"VoltzDrawInput.Exit", WM_APP + 1);
	return message;
}

UINT holeMessage()
{
	static const UINT message = registeredMessage(L"VoltzDrawInput.Hole", WM_APP + 2);
	return message;
}

int64_t nowNs()
{
	return static_cast<int64_t>(os_gettime_ns());
}

// wParam do exitMessage() → motivo. Fora de Escape..Shutdown (inclusive None, que a DrawSession ignoraria) é erro de
// programação: na dúvida, sai como na saída do app (Shutdown) e registra — a sessão nunca fica ligada por isso.
DrawExit exitFromMessage(WPARAM wParam)
{
	if (wParam >= static_cast<WPARAM>(DrawExit::Escape) && wParam <= static_cast<WPARAM>(DrawExit::Shutdown)) {
		return static_cast<DrawExit>(static_cast<int>(wParam));
	}
	blog(LOG_WARNING, "[voltz] modo desenho: pedido de saida com motivo invalido (%llu); saindo como 'saida'",
	     static_cast<unsigned long long>(wParam));
	return DrawExit::Shutdown;
}

// Tudo que a janela precisa, só na thread de entrada.
struct InputState {
	InputState(std::shared_ptr<DrawShared> sharedState, std::shared_ptr<DrawInputHandle> handleState,
		   const CaptureMapping &mapping)
		: shared(std::move(sharedState)),
		  handle(std::move(handleState)),
		  session(mapping, nowNs()),
		  rect(mapping.monitor)
	{
	}

	std::shared_ptr<DrawShared> shared;
	std::shared_ptr<DrawInputHandle> handle;
	DrawSession session;
	MonitorRect rect;
	HWND hwnd = nullptr;
	HWND previousForeground = nullptr;
	bool capturing = false;
	bool releasing = false;
	bool foregroundAtExit = false;
	// Comandos ainda não entregues à UI (o mutex estava ocupado). Em ordem; nunca se perdem.
	std::vector<DrawCommand> backlog;
	bool holeDirty = false; // o buraco não foi lido (mutex ocupado): o tique tenta de novo
	int64_t lastTickNs = 0;
};

void releaseMouse(InputState &state)
{
	if (!state.capturing) {
		return;
	}
	state.releasing = true;
	ReleaseCapture();
	state.releasing = false;
	state.capturing = false;
}

// Passa o backlog para a fila da UI. Com a janela viva, só com try_lock: a thread de entrada NUNCA espera um mutex que a
// UI (talvez travada) segura — se falhar, tenta de novo no próximo evento ou tique. `blocking` só depois de destruir a
// janela (nada mais a proteger), para o Exit sempre chegar. Falso = ficou algo no backlog.
bool flushBacklog(InputState &state, bool blocking)
{
	if (state.backlog.empty()) {
		return true;
	}
	std::unique_lock<std::mutex> lock(state.shared->mutex, std::defer_lock);
	if (blocking) {
		lock.lock();
	} else if (!lock.try_lock()) {
		return false;
	}
	std::vector<DrawCommand> &queue = state.shared->commands;
	queue.insert(queue.end(), std::make_move_iterator(state.backlog.begin()),
		     std::make_move_iterator(state.backlog.end()));
	state.backlog.clear();
	if (!state.shared->wakePending && state.shared->wake) {
		state.shared->wakePending = true;
		state.shared->wake(state.shared->wakeContext);
	}
	return true;
}

// Entrega os comandos à UI sem esperar. Se a sessão acabou (Exit é sempre o último): solta o mouse, esconde a janela e
// encerra o laço desta thread — tudo aqui, ANTES de tocar no mutex e sem depender da UI. Também reenvia o backlog.
void deliver(InputState &state, std::vector<DrawCommand> commands)
{
	if (!commands.empty()) {
		if (commands.back().type == DrawCommandType::Exit && state.hwnd) {
			state.foregroundAtExit = GetForegroundWindow() == state.hwnd;
			releaseMouse(state);
			ShowWindow(state.hwnd, SW_HIDE);
			PostQuitMessage(0);
		}
		state.backlog.insert(state.backlog.end(), std::make_move_iterator(commands.begin()),
				     std::make_move_iterator(commands.end()));
	}
	flushBacklog(state, false);
}

DrawInput eventAt(DrawInputKind kind)
{
	DrawInput input;
	input.kind = kind;
	input.tNs = nowNs();
	return input;
}

// Coordenadas do cliente (pixels físicos; a janela começa no canto do monitor) → desktop.
DrawInput pointerAt(DrawInputKind kind, const InputState &state, LPARAM lParam)
{
	DrawInput input = eventAt(kind);
	input.x = state.rect.left + GET_X_LPARAM(lParam);
	input.y = state.rect.top + GET_Y_LPARAM(lParam);
	return input;
}

// Região da janela = o monitor inteiro menos o retângulo da barra (o clique ali vai para a barra, que é de outra
// thread: HTTRANSPARENT não serviria). Só com try_lock; ocupado = holeDirty e o tique tenta de novo.
void applyHole(InputState &state)
{
	MonitorRect hole;
	{
		std::unique_lock<std::mutex> lock(state.shared->mutex, std::try_to_lock);
		if (!lock.owns_lock()) {
			state.holeDirty = true;
			return;
		}
		hole = state.shared->hole;
	}
	state.holeDirty = false;
	HRGN region = CreateRectRgn(0, 0, state.rect.width, state.rect.height);
	if (!region) {
		return;
	}
	if (hole.width > 0 && hole.height > 0) {
		const int left = hole.left - state.rect.left;
		const int top = hole.top - state.rect.top;
		HRGN cut = CreateRectRgn(left, top, left + hole.width, top + hole.height);
		if (cut) {
			CombineRgn(region, region, cut, RGN_DIFF);
			DeleteObject(cut);
		}
	}
	if (!SetWindowRgn(state.hwnd, region, TRUE)) {
		DeleteObject(region); /* deu certo = a região passa a ser do sistema */
	}
}

// Tique (~100 ms): pelo WM_TIMER ou, se ele atrasar, pelo próprio laço de mensagens. Vigia, inatividade, cancel do
// dono e as tentativas de novo do backlog e do buraco.
void runTick(InputState &state)
{
	state.lastTickNs = nowNs();
	if (state.holeDirty) {
		applyHole(state);
	}
	/* o dono desistiu ou foi destruído sem que o pedido de saída chegasse: sai já */
	if (state.handle->cancel.load()) {
		deliver(state, state.session.requestExit(DrawExit::Shutdown));
		return;
	}
	DrawInput input = eventAt(DrawInputKind::Tick);
	input.uiHeartbeatNs = state.shared->uiHeartbeatNs.load();
	deliver(state, state.session.handle(input));
}

LRESULT CALLBACK inputProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto *state = reinterpret_cast<InputState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (!state) {
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}
	if (message == exitMessage()) {
		deliver(*state, state->session.requestExit(exitFromMessage(wParam)));
		return 0;
	}
	if (message == holeMessage()) {
		applyHole(*state);
		return 0;
	}
	switch (message) {
	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT) {
			SetCursor(LoadCursorW(nullptr, IDC_CROSS));
			return TRUE;
		}
		break;
	case WM_LBUTTONDOWN:
		/* o mouse fica com esta janela só entre apertar e soltar (arrastar para fora do monitor continua o traço) */
		if (!state->capturing && state->session.active()) {
			SetCapture(hwnd);
			state->capturing = true;
		}
		deliver(*state, state->session.handle(pointerAt(DrawInputKind::LeftDown, *state, lParam)));
		return 0;
	case WM_MOUSEMOVE:
		deliver(*state, state->session.handle(pointerAt(DrawInputKind::Move, *state, lParam)));
		return 0;
	case WM_LBUTTONUP:
		deliver(*state, state->session.handle(pointerAt(DrawInputKind::LeftUp, *state, lParam)));
		releaseMouse(*state);
		return 0;
	case WM_RBUTTONDOWN:
		deliver(*state, state->session.handle(pointerAt(DrawInputKind::RightDown, *state, lParam)));
		return 0;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN: {
		DrawInputKind kind = DrawInputKind::OtherKey;
		if (wParam == VK_ESCAPE) {
			kind = DrawInputKind::Escape;
		} else if (wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
			kind = DrawInputKind::Undo;
		}
		deliver(*state, state->session.handle(eventAt(kind)));
		if (message == WM_SYSKEYDOWN) {
			break; /* Alt+F4: o DefWindowProc manda WM_CLOSE, que também sai */
		}
		return 0;
	}
	case WM_ACTIVATE:
		deliver(*state,
			state->session.handle(eventAt(LOWORD(wParam) == WA_INACTIVE ? DrawInputKind::Deactivated
										    : DrawInputKind::Activated)));
		break; /* o DefWindowProc põe o foco do teclado na janela ativada: Esc e Ctrl+Z chegam aqui */
	case WM_CANCELMODE:
		deliver(*state, state->session.handle(eventAt(DrawInputKind::CaptureLost)));
		break; /* o DefWindowProc solta a captura */
	case WM_CAPTURECHANGED:
		if (!state->releasing && reinterpret_cast<HWND>(lParam) != hwnd) {
			state->capturing = false;
			deliver(*state, state->session.handle(eventAt(DrawInputKind::CaptureLost)));
		}
		return 0;
	case WM_TIMER:
		if (wParam == kTickTimer) {
			runTick(*state);
			return 0;
		}
		break;
	case WM_DISPLAYCHANGE:
		deliver(*state, state->session.handle(eventAt(DrawInputKind::TargetChanged)));
		return 0;
	case WM_CLOSE:
		deliver(*state, state->session.handle(eventAt(DrawInputKind::Close)));
		return 0;
	case WM_ENDSESSION:
		if (wParam) {
			deliver(*state, state->session.requestExit(DrawExit::Shutdown));
		}
		return 0;
	case WM_DPICHANGED:
		return 0; /* o retângulo é sempre o do monitor, em pixels físicos */
	default:
		break;
	}
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool registerInputClass()
{
	static const bool registered = []() {
		WNDCLASSEXW windowClass = {};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = inputProc;
		windowClass.hInstance = GetModuleHandleW(nullptr);
		windowClass.lpszClassName = kInputClass;
		return RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	}();
	return registered;
}

// A janela é deste processo? (para não devolver o primeiro plano a uma janela nossa que travou)
bool ownedByThisProcess(HWND window)
{
	DWORD processId = 0;
	GetWindowThreadProcessId(window, &processId);
	return processId == GetCurrentProcessId();
}

void inputThreadMain(std::shared_ptr<DrawShared> shared, DrawInputWindow::Options options,
		     std::shared_ptr<DrawInputHandle> handle, std::promise<bool> ready, std::promise<void> done)
{
	os_set_thread_name("voltz-draw-input");
	/* pixels físicos em qualquer escala, como o resto do Voltz (MonitorLocator, sobreposição) */
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	InputState state(std::move(shared), handle, options.mapping);
	const MonitorRect rect = state.rect;
	state.previousForeground = GetForegroundWindow();
	HWND hwnd = nullptr;
	if (registerInputClass()) {
		/* Sem os bits de "transparente a cliques" e "não ativar", de propósito: é a janela que recebe o desenho. Sem
		 * dono: uma dona de outra thread ligaria as filas de entrada das duas threads. */
		hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kInputClass, L"Voltz Desenho",
				       WS_POPUP, rect.left, rect.top, rect.width, rect.height, nullptr, nullptr,
				       GetModuleHandleW(nullptr), nullptr);
	}
	if (!hwnd) {
		blog(LOG_WARNING, "[voltz] modo desenho: CreateWindowEx falhou (%lu)", GetLastError());
		ready.set_value(false);
		done.set_value();
		return;
	}
	/* Sem o tique não há vigia nem inatividade: sem ele a janela nem aparece. */
	if (!SetTimer(hwnd, kTickTimer, kTickMs, nullptr)) {
		blog(LOG_WARNING, "[voltz] modo desenho: SetTimer falhou (%lu)", GetLastError());
		DestroyWindow(hwnd);
		ready.set_value(false);
		done.set_value();
		return;
	}
	state.hwnd = hwnd;
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
	/* alfa 1/255: invisível, mas recebe o mouse (um pixel de alfa 0 deixaria o clique passar) */
	SetLayeredWindowAttributes(hwnd, 0, 1, LWA_ALPHA);
	if (options.excludeFromCapture) {
		SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
	}
	applyHole(state);
	/* Grava o hwnd e SÓ DEPOIS lê o cancel (o destrutor marca o cancel antes de ler o hwnd): um dos dois vê o outro.
	 * Cancelado aqui = o dono já desistiu: a janela nem aparece nem pega o primeiro plano. */
	handle->hwnd.store(hwnd);
	if (handle->cancel.load()) {
		ready.set_value(false);
		deliver(state, state.session.requestExit(DrawExit::Shutdown)); /* posta o WM_QUIT: o laço sai já */
	} else {
		ShowWindow(hwnd, SW_SHOW);
		/* Só consegue se o Voltz já está em primeiro plano (botão "Desenhar"); pelo atalho, o primeiro clique ativa. */
		const bool foreground = SetForegroundWindow(hwnd) != FALSE || GetForegroundWindow() == hwnd;
		handle->activated.store(foreground);
		ready.set_value(true);
	}

	state.lastTickNs = nowNs();
	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
		DispatchMessageW(&msg);
		/* WM_TIMER só sai com a fila vazia: com o mouse sempre em movimento, o tique (vigia) roda daqui */
		if (nowNs() - state.lastTickNs >= kLateTickNs) {
			runTick(state);
		}
	}

	/* Fim, tudo nesta thread (nada espera a UI até a janela sumir). */
	if (state.session.active()) {
		deliver(state, state.session.requestExit(DrawExit::Shutdown));
	}
	releaseMouse(state);
	KillTimer(hwnd, kTickTimer);
	handle->hwnd.store(nullptr);
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
	DestroyWindow(hwnd);
	state.hwnd = nullptr;
	/* Devolve o primeiro plano a quem o tinha, só se ainda era nosso na saída (Esc, atalho, vigia...). Na perda de
	 * ativação o usuário já foi para outra janela: não mexe. Pelo vigia, a UI do Voltz parou: o teclado não volta
	 * para uma janela deste processo. */
	const bool uiFrozen = state.session.exitReason() == DrawExit::Watchdog;
	if (state.foregroundAtExit && state.previousForeground && IsWindow(state.previousForeground) &&
	    !(uiFrozen && ownedByThisProcess(state.previousForeground))) {
		SetForegroundWindow(state.previousForeground);
	}
	/* Sem janela a proteger: agora pode esperar o mutex, para o Exit sempre chegar à UI. */
	flushBacklog(state, true);
	done.set_value();
}

} // namespace
#endif

std::unique_ptr<DrawInputWindow> DrawInputWindow::start(std::shared_ptr<DrawShared> shared, const Options &options)
{
	/* Defesa em profundidade: o DrawModeController já recusa com VOLTZ_TEST_LIVE; aqui nem a thread nasce. */
	if (live::testModeEnabled()) {
		blog(LOG_WARNING, "[voltz] modo desenho: janela de entrada recusada (%s)", live::kTestEnv);
		return nullptr;
	}
#ifdef _WIN32
	const CaptureMapping &mapping = options.mapping;
	if (!shared || mapping.monitor.width <= 0 || mapping.monitor.height <= 0 || mapping.captureWidth <= 0 ||
	    mapping.captureHeight <= 0) {
		return nullptr;
	}
	std::unique_ptr<DrawInputWindow> window(new DrawInputWindow());
	window->handle_ = std::make_shared<DrawInputHandle>();
	std::promise<bool> ready;
	std::future<bool> started = ready.get_future();
	std::promise<void> done;
	window->finished_ = done.get_future();
	try {
		window->thread_ = std::thread(inputThreadMain, std::move(shared), options, window->handle_,
					      std::move(ready), std::move(done));
	} catch (const std::system_error &error) {
		blog(LOG_WARNING, "[voltz] modo desenho: a thread de entrada nao nasceu (%s)", error.what());
		return nullptr; /* sem thread: o destrutor não espera nada */
	}
	if (started.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
		blog(LOG_WARNING, "[voltz] modo desenho: a janela de entrada nao nasceu em 2 s");
		window->handle_->cancel.store(true);
		window->waitTimedOut_ = true; /* a UI já esperou 2 s: o destrutor não espera de novo */
		return nullptr;
	}
	if (!started.get()) {
		blog(LOG_WARNING, "[voltz] modo desenho: a janela de entrada nao nasceu");
		return nullptr; /* o destrutor pede a saída e espera a thread, que já está acabando */
	}
	return window;
#else
	(void)shared;
	(void)options;
	return nullptr;
#endif
}

DrawInputWindow::~DrawInputWindow()
{
	if (!handle_) {
		return;
	}
	/* O cancel vem ANTES de ler o hwnd: se a janela ainda não tinha nascido, a thread o vê logo depois de criá-la (e
	 * a cada tique); se já tinha, o pedido de saída chega a ela. */
	handle_->cancel.store(true);
	requestExit(DrawExit::Shutdown);
	/* Pior caso para a UI ~2 s no total: se um wait()/start() anterior já esgotou o tempo, não espera de novo. */
	if (!wait(waitTimedOut_ ? 0 : 2000) && thread_.joinable()) {
		blog(LOG_WARNING, "[voltz] modo desenho: a thread de entrada nao terminou a tempo (destacada)");
		thread_.detach();
	}
}

void DrawInputWindow::requestExit(DrawExit reason)
{
#ifdef _WIN32
	void *hwnd = handle_ ? handle_->hwnd.load() : nullptr;
	if (hwnd) {
		PostMessageW(static_cast<HWND>(hwnd), exitMessage(), static_cast<WPARAM>(reason), 0);
	}
#else
	(void)reason;
#endif
}

void DrawInputWindow::holeChanged()
{
#ifdef _WIN32
	void *hwnd = handle_ ? handle_->hwnd.load() : nullptr;
	if (hwnd) {
		PostMessageW(static_cast<HWND>(hwnd), holeMessage(), 0, 0);
	}
#endif
}

bool DrawInputWindow::wait(int timeoutMs)
{
	if (!thread_.joinable()) {
		return true;
	}
	/* join só depois do aviso de fim (nunca um join sem prazo na thread da UI) */
	if (!finished_.valid() ||
	    finished_.wait_for(std::chrono::milliseconds(timeoutMs)) != std::future_status::ready) {
		if (timeoutMs > 0) {
			waitTimedOut_ = true; /* a UI já esperou: o destrutor não espera de novo */
		}
		return false;
	}
	thread_.join();
	return true;
}

bool DrawInputWindow::activated() const
{
	return handle_ && handle_->activated.load();
}

} // namespace voltz
