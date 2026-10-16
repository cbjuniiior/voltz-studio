#pragma once

#include <voltz/core/mouse-effects.hpp>
#include <voltz/core/mouse-style.hpp>

#include <obs.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace voltz {

// Retrato do estado ao vivo para desenhar um quadro.
struct MouseSnapshot {
	bool inside = false;
	int x = 0;
	int y = 0;
	MouseStyle style;
	std::vector<Ripple> ripples;
};

// Estado ao vivo do mouse, um só no processo. Escrito pelos procedimentos (thread do LiveMouse: posição e cliques;
// thread da UI: estilo) e pelos pedidos de teste (thread do obs-websocket); lido pelas fontes Voltz Mouse na thread
// de render. Trechos curtos sob um mutex; `showing` é atômico.
class MouseFeed {
public:
	static MouseFeed &instance();

	void setPosition(int x, int y, bool inside);
	void addClick(int x, int y, MouseButton button, int64_t tNs);
	// JSON do núcleo (mouseStyleFromJson). Falso (estilo anterior mantido) se inválido.
	bool setStyleJson(const std::string &json);
	// Copia o estado e descarta as ondas vencidas em `nowNs`.
	MouseSnapshot snapshot(int64_t nowNs);

	// Fontes Voltz Mouse sendo mostradas (show/hide do libobs): o frontend só lê o mouse quando > 0.
	void addShowing(int delta) { showing_.fetch_add(delta); }
	int showing() const { return showing_.load(); }

private:
	MouseFeed() = default;

	std::mutex mutex_;
	bool inside_ = false;
	int x_ = 0;
	int y_ = 0;
	MouseStyle style_;
	std::vector<Ripple> ripples_;
	std::atomic<int> showing_{0};
};

// Registra voltz_mouse_position, voltz_mouse_click, voltz_mouse_style e voltz_mouse_wanted (live-protocol.hpp).
void registerMouseProcs(proc_handler_t *handler);

} // namespace voltz
