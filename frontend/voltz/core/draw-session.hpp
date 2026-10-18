#pragma once

#include "annotation-model.hpp"
#include "cursor-track.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Por que o modo desenho acabou (log "[voltz] modo desenho: desligado (<nome>)" e estado de teste).
enum class DrawExit { None, Escape, RightClick, Hotkey, Button, FocusLost, Idle, Watchdog, Target, Close, Shutdown };

// "esc" | "clique_direito" | "atalho" | "botao" | "foco" | "inatividade" | "vigia" | "alvo" | "fechar" | "saida";
// "" para None.
const char *drawExitName(DrawExit exit);

// Por que o modo desenho não ligou.
enum class DrawRefusal { None, ShuttingDown, TestMode, Recent, NoTarget };

// "saindo" | "teste" | "recente" | "sem_alvo"; "" para None.
const char *drawRefusalName(DrawRefusal refusal);

// O modo desenho só liga por ação do usuário e nunca: com o app saindo, com VOLTZ_TEST_LIVE (teste automático não
// consegue ligá-lo), logo depois de uma saída durante uma trava da UI (`recentFreezeExit`: o atalho ou o clique em
// "Desenhar" que ficou na fila durante a trava queria desligar, não religar) ou sem monitor capturado onde desenhar —
// nessa ordem.
DrawRefusal checkDrawEntry(bool shuttingDown, bool testMode, bool recentFreezeExit, bool hasTarget);

constexpr int64_t kDrawIdleNs = 30LL * 1000000000LL; // 30 s sem entrada do usuário
constexpr int64_t kDrawHeartbeatNs = 1000000000LL;   // thread da UI sem pulsar por 1 s
// Depois de uma saída durante uma trava da UI (vigia, ou qualquer saída tratada com a UI parada há mais de
// kDrawHeartbeatNs), o atalho e o botão não religam o modo por este tempo (DrawRefusal::Recent).
constexpr int64_t kDrawReentryBlockNs = 500LL * 1000000LL;

enum class DrawInputKind {
	Move,          // o mouse moveu (x, y)
	LeftDown,      // botão esquerdo desceu (x, y)
	LeftUp,        // botão esquerdo subiu (x, y)
	RightDown,     // botão direito desceu
	Escape,        // Esc
	Undo,          // Ctrl+Z
	OtherKey,      // outra tecla (só conta como atividade)
	Char,          // caractere digitado (codepoint; WM_CHAR, já com acentos e substitutos juntados)
	Backspace,     // apagar o último caractere
	Enter,         // Enter (shift = Shift+Enter: nova linha)
	Activated,     // a janela de entrada ficou ativa
	Deactivated,   // perdeu a ativação (Alt+Tab, tecla Windows, clique em outra janela ou monitor)
	CaptureLost,   // alguém tirou a captura do mouse (WM_CANCELMODE, WM_CAPTURECHANGED)
	Close,         // WM_CLOSE (Alt+F4)
	TargetChanged, // os monitores mudaram (WM_DISPLAYCHANGE)
	Tick,          // tique da thread de entrada (~100 ms), com o último pulso da UI
};

struct DrawInput {
	DrawInputKind kind = DrawInputKind::Tick;
	int64_t tNs = 0; // relógio do libobs (os_gettime_ns)
	int x = 0;       // pixels físicos do desktop (eventos de mouse)
	int y = 0;
	int64_t uiHeartbeatNs = 0; // Tick: último pulso da thread da UI (0 = nunca pulsou)
	bool textTool = false;     // LeftDown: a ferramenta escolhida é o texto (o clique começa a digitação)
	char32_t codepoint = 0;    // Char
	bool shift = false;        // Enter: Shift apertado
};

// Begin/Extend/End/Undo: traço; TextBegin (1 ponto: a âncora), TextEdit (o texto inteiro, UTF-8), TextCommit (o texto
// fica), TextCancel (o texto sai): digitação (Plano 4D); Exit: fim da sessão (sempre o último).
enum class DrawCommandType { Begin, Extend, End, Undo, Exit, TextBegin, TextEdit, TextCommit, TextCancel };

struct DrawCommand {
	DrawCommandType type = DrawCommandType::End;
	std::vector<StrokePoint> points; // Begin/TextBegin: 1 ponto; Extend: os pontos novos (pixels da captura)
	DrawExit exit = DrawExit::None;  // Exit: o motivo
	std::string text;                // TextEdit: o texto inteiro até agora
};

// Uma sessão do modo desenho, na thread de entrada: traduz a entrada em comandos de traço e decide TODAS as saídas
// garantidas (Esc, clique direito, perda de ativação/captura, Alt+F4, troca de monitores, 30 s parado, vigia da UI).
// Sem Qt nem Win32; o tempo vem em cada entrada (relógio injetável nos testes). Toda saída termina antes o traço em
// andamento e confirma o texto que está sendo digitado (vazio = cancelado); depois de sair, ignora tudo.
// Texto (Plano 4D): com a ferramenta texto, o clique começa a digitação na âncora; caracteres, Backspace e Shift+Enter
// editam (limites do annotation-text); Enter confirma; Esc e Ctrl+Z cancelam só o texto (o Esc seguinte sai); clicar
// de novo confirma e começa outro texto (ou o traço, se a ferramenta mudou). Digitar conta como atividade.
class DrawSession {
public:
	DrawSession(const CaptureMapping &mapping, int64_t startNs, int64_t idleNs = kDrawIdleNs,
		    int64_t heartbeatNs = kDrawHeartbeatNs);

	std::vector<DrawCommand> handle(const DrawInput &input);
	// Saída pedida de fora (o mesmo atalho, botão Sair da barra, saída do app). Vazio se já saiu ou `reason` é None.
	std::vector<DrawCommand> requestExit(DrawExit reason);

	bool active() const { return exit_ == DrawExit::None; }
	bool drawing() const { return drawing_; }
	bool typing() const { return typing_; }
	DrawExit exitReason() const { return exit_; }

private:
	std::vector<DrawCommand> finish(DrawExit reason);
	// Fecha a digitação: TextCommit (texto com algo) ou TextCancel (vazio) em `out`. Nada sem digitação.
	void closeText(std::vector<DrawCommand> &out);
	// Ponto do desktop preso ao monitor e convertido para a captura (arrastar para fora fica na borda).
	StrokePoint clampedPoint(int x, int y) const;

	CaptureMapping mapping_;
	int64_t lastInputNs_ = 0;
	int64_t idleNs_ = kDrawIdleNs;
	int64_t heartbeatNs_ = kDrawHeartbeatNs;
	bool drawing_ = false;
	bool typing_ = false;
	std::string text_;
	StrokePoint last_;
	DrawExit exit_ = DrawExit::None;
};

} // namespace voltz
