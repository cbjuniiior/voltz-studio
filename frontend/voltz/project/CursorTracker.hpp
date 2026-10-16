#pragma once

#include <voltz/core/cursor-track.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace voltz {

// Registra o mouse em cursor.jsonl (spec §5.6) enquanto o projeto grava. Um gancho WH_MOUSE_LL numa thread só dele
// (só enfileira, sem lock nem alocação) e uma thread de escrita (60 Hz, conversão para pixels da captura, gravação
// com buffer). Nada roda na thread da UI. Só uma instância ativa por vez. start/stop chamados da thread da UI.
class CursorTracker {
public:
	CursorTracker() = default;
	~CursorTracker() { stop(); }
	CursorTracker(const CursorTracker &) = delete;
	CursorTracker &operator=(const CursorTracker &) = delete;

	// `utf8Path`: caminho do cursor.jsonl; `t0Ns`: início do projeto (os_gettime_ns). Falso se o arquivo não abrir
	// ou o gancho não instalar (nesse caso nada fica instalado).
	bool start(const std::string &utf8Path, const CaptureMapping &mapping, int64_t t0Ns);
	// Desinstala o gancho, grava o que falta e fecha o arquivo. Idempotente.
	void stop();

	bool running() const { return running_; }
	uint64_t linesWritten() const { return lines_.load(); }
	uint64_t droppedEvents() const { return dropped_.load(); }

	// Chamado pelo gancho (thread do gancho, único produtor): copia para o anel; anel cheio = evento descartado.
	void push(const RawMouseEvent &event);

private:
	void hookThreadMain(std::promise<bool> ready);
	void writerThreadMain();
	// Thread de escrita (único consumidor): move tudo o que está no anel para `out`.
	void drain(std::vector<RawMouseEvent> &out);

	FILE *file_ = nullptr;
	CaptureMapping mapping_;
	int64_t t0Ns_ = 0;
	std::thread hookThread_;
	std::thread writerThread_;
	std::atomic<unsigned long> hookThreadId_{0};
	std::atomic<bool> unhooked_{false};
	// Anel SPSC de tamanho fixo (gancho → escrita): head_ só avança no gancho, tail_ só na escrita.
	std::vector<RawMouseEvent> ring_;
	std::atomic<size_t> head_{0};
	std::atomic<size_t> tail_{0};
	// Só para acordar/parar a thread de escrita (o gancho nunca toca neles).
	std::mutex mutex_;
	std::condition_variable wake_;
	bool stopping_ = false;
	bool running_ = false;
	std::atomic<uint64_t> lines_{0};
	std::atomic<uint64_t> dropped_{0};
};

} // namespace voltz
