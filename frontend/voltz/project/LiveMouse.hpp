#pragma once

#include <voltz/core/cursor-track.hpp>

#include <QObject>
#include <QTimer>

#include <atomic>
#include <future>
#include <mutex>
#include <thread>

namespace voltz {

class MouseStyleStore;

// Alimenta as fontes Voltz Mouse (plugin voltz-sources) com a posição e os cliques do mouse em pixels da captura de
// monitor compartilhada (a Tela do preset) — a mesma conversão do cursor.jsonl. Pergunta ao plugin a cada 500 ms se
// alguma Voltz Mouse está sendo mostrada e só então liga uma thread própria, sem gancho: posição por
// GetPhysicalCursorPos a cada 16 ms e cliques por Raw Input (RIDEV_INPUTSINK) numa janela só de mensagens
// (HWND_MESSAGE: invisível, não recebe nem bloqueia entrada). Também empurra o estilo para o plugin. Com
// VOLTZ_TEST_LIVE a thread nunca liga (os testes injetam o mouse pelo obs-websocket). Thread da UI, menos threadMain.
class LiveMouse : public QObject {
public:
	explicit LiveMouse(MouseStyleStore *styles, QObject *parent = nullptr);
	~LiveMouse() override;

	// Para a consulta e a thread. Chamar no EXIT do frontend, antes de o libobs descarregar o plugin.
	void shutdown();
	bool running() const { return running_; }

private:
	void poll();
	void pushStyle();
	void refreshMapping();
	void start();
	void stop();
	void threadMain(std::promise<void> ready);
	CapturePoint currentPoint();

	MouseStyleStore *styles_;
	QTimer poll_;
	std::thread thread_;
	std::atomic<unsigned long> threadId_{0};
	std::mutex mutex_;
	CaptureMapping mapping_; // protegido por mutex_ (a UI escreve, a thread lê)
	int mappingState_ = -1;  // -1 desconhecido, 0 sem captura de monitor, 1 com captura (só para o log)
	bool running_ = false;
	bool testMode_ = false;
	bool shutdown_ = false;
};

} // namespace voltz
