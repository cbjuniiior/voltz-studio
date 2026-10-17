#pragma once

#include <voltz/core/cursor-track.hpp>
#include <voltz/core/draw-session.hpp>

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace voltz {

// Estado dividido entre a thread de entrada do modo desenho e a thread da UI.
struct DrawShared {
	std::atomic<int64_t> uiHeartbeatNs{0}; // a UI escreve a cada 250 ms; a thread de entrada lê no tique (vigia)
	std::mutex mutex;                      // protege os campos abaixo
	std::vector<DrawCommand> commands;     // thread de entrada → UI (DrawModeController::drain)
	bool wakePending = false;              // já há um drain a caminho da UI
	// Chamado SOB o mutex; anulado no shutdown (depois disso ninguém é acordado).
	void (*wake)(void *context) = nullptr;
	void *wakeContext = nullptr;
	MonitorRect hole; // retângulo da barra (pixels físicos do desktop) que a janela de entrada deixa livre
};

// Thread da UI: pega os comandos que chegaram e limpa o aviso de "acordar".
std::vector<DrawCommand> takeDrawCommands(DrawShared &shared);

struct DrawInputHandle;

// A janela que RECEBE o desenho (spec §6.2, "clicável apenas em modo desenho"). Este arquivo .cpp é o ÚNICO do Voltz
// que pega o mouse ou o primeiro plano (conferido por grep; ver o Plano 4C). Janela Win32 "VoltzDrawInput" numa
// thread própria, só sobre o monitor da Tela: em camadas com alfa 1/255 (invisível, mas recebe o mouse), sempre no topo,
// fora da barra de tarefas, fora da captura quando o Windows suporta, cursor em cruz, sem dono. A thread não faz nada
// além dela: roda a DrawSession (núcleo), que decide todas as saídas — inclusive o vigia (a UI parou de pulsar por 1 s)
// —, e entrega os comandos pela fila do DrawShared sem esperar a UI. Ao sair: solta o mouse, esconde a janela, destrói e
// devolve o primeiro plano a quem o tinha (se ainda era nosso). Recusa com VOLTZ_TEST_LIVE antes de criar a thread.
class DrawInputWindow {
public:
	struct Options {
		CaptureMapping mapping;          // monitor da Tela (pixels físicos) → captura
		bool excludeFromCapture = false; // WDA_EXCLUDEFROMCAPTURE (só com SetDisplayAffinitySupported)
	};

	// Thread da UI. Cria a thread e espera a janela nascer (até 2 s); nullptr = não ligou (motivo no log).
	static std::unique_ptr<DrawInputWindow> start(std::shared_ptr<DrawShared> shared, const Options &options);
	// Pede a saída (Shutdown) e espera a thread até 2 s; se não acabar, ela fica destacada e sai sozinha.
	~DrawInputWindow();
	DrawInputWindow(const DrawInputWindow &) = delete;
	DrawInputWindow &operator=(const DrawInputWindow &) = delete;

	// Assíncronos (postam para a janela; nunca esperam a thread de entrada). Pedir a saída mais de uma vez é inócuo.
	void requestExit(DrawExit reason);
	void holeChanged();
	// Espera a thread terminar (até timeoutMs). Falso = ainda viva.
	bool wait(int timeoutMs);
	// A janela ficou em primeiro plano ao nascer (pelo botão "Desenhar"); pelo atalho, o primeiro clique a ativa.
	bool activated() const;

private:
	DrawInputWindow() = default;

	std::shared_ptr<DrawInputHandle> handle_;
	std::thread thread_;
	std::future<void> finished_;
};

} // namespace voltz
