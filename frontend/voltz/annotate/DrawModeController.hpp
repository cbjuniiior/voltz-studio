#pragma once

#include <voltz/core/cursor-track.hpp>
#include <voltz/core/draw-session.hpp>

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

namespace voltz {

class AnnotationController;
class DrawInputWindow;
class DrawToolbar;
struct DrawShared;

// Quem pediu o modo desenho: as duas únicas portas de entrada (ação explícita do usuário).
enum class DrawTrigger { Hotkey, Button };

// Modo desenho (spec §6.2) na thread da UI. Liga SÓ por ação do usuário — o atalho do OBS "VoltzStudio.Draw" (padrão
// Ctrl+Shift+D) ou o botão "Desenhar" —, nunca sozinho (não é salvo; desligado em toda abertura) e nunca com
// VOLTZ_TEST_LIVE. Ligado: pulso da UI a cada 250 ms (o vigia da thread de entrada sai se ele parar por 1 s), barra
// (DrawToolbar) no alto do monitor da Tela e a janela de entrada (DrawInputWindow, thread própria) sobre esse monitor.
// As saídas garantidas são decididas na thread de entrada (DrawSession); daqui saem o mesmo atalho, o botão Sair da
// barra, a troca da Tela/monitor (conferida a cada 1 s) e a saída do app. Drena os comandos de traço para o
// AnnotationController. Também registra "VoltzStudio.UndoAnnotation" e "VoltzStudio.ClearAnnotations" (sem tecla
// padrão) e, só com VOLTZ_DEBUG_FREEZE_UI, "VoltzStudio.DebugFreezeUi".
class DrawModeController : public QObject {
	Q_OBJECT

public:
	explicit DrawModeController(AnnotationController *annotations, QObject *parent = nullptr);
	~DrawModeController() override;

	// EXIT do frontend, ANTES do AnnotationController::shutdown: sai do modo (o traço em andamento termina), apaga a
	// barra e solta os atalhos. Idempotente (o próprio EXIT chama primeiro; a casca chama de novo).
	void shutdown();
	// Atalho ou botão: liga (se nada recusar) ou pede a saída.
	void toggle(DrawTrigger trigger);
	bool active() const { return input_ != nullptr; }
	// Teclas do atalho de desenhar ("Ctrl + Shift + D") ou "sem atalho".
	QString drawShortcutText() const;

signals:
	void activeChanged(bool active);
	// Mensagem para o usuário quando o modo não ligou (vazia nunca).
	void refused(const QString &message);

private:
	void enter(DrawTrigger trigger);
	void createToolbar();
	void beat();
	void drain();
	void finish(DrawExit reason);
	void updateHole();
	void registerHotkeys();
	void loadHotkeyBindings();
	void unregisterHotkeys();
	static void wake(void *context);
	static void handleEvent(enum obs_frontend_event event, void *data);
	static void handleDrawHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void handleUndoHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void handleClearHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void handleFreezeHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

	AnnotationController *annotations_;
	std::shared_ptr<DrawShared> shared_;
	std::unique_ptr<DrawInputWindow> input_;
	DrawToolbar *toolbar_ = nullptr; // sem pai (janela própria): apagada no shutdown
	CaptureMapping mapping_;
	QTimer heartbeat_;
	int beats_ = 0;
	obs_hotkey_id drawHotkey_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id undoHotkey_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id clearHotkey_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id freezeHotkey_ = OBS_INVALID_HOTKEY_ID;
	bool eventsRegistered_ = false;
	bool shutdown_ = false;
};

} // namespace voltz
