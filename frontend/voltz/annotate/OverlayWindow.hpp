#pragma once

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/cursor-track.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Sobreposição das anotações sobre um monitor (spec §6.2). Janela Win32 PURA — não é QWidget: o OBSApp::notify
// reaplicaria nela a afinidade "esconder janelas do OBS" do usuário (WDA_NONE com a opção desligada). WS_POPUP em
// camadas, sempre no topo e SEMPRE transparente a cliques: WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
// WS_EX_TOOLWINDOW | WS_EX_TOPMOST, WM_NCHITTEST = HTTRANSPARENT, WM_MOUSEACTIVATE = MA_NOACTIVATE. NÃO existe método que
// tire o WS_EX_TRANSPARENT (o modo desenho do Plano 4C é outro código, com saídas garantidas); cada render reconfere os
// bits obrigatórios. Desenha com o rasterizador do núcleo num DIB BGRA pré-multiplicado e publica com
// UpdateLayeredWindow. Thread da UI (as mensagens da janela chegam pelo laço de eventos do Qt).
class OverlayWindow {
public:
	struct Info {
		bool exists = false;
		bool visible = false;
		uint64_t hwnd = 0;
		int64_t exStyle = 0;
		uint32_t affinity = 0;
		MonitorRect rect;
	};

	OverlayWindow() = default;
	~OverlayWindow();
	OverlayWindow(const OverlayWindow &) = delete;
	OverlayWindow &operator=(const OverlayWindow &) = delete;

	// Cria escondida sobre `rect` (pixels físicos do desktop). Falso se a janela não nasceu.
	bool create(const MonitorRect &rect);
	void destroy();
	// WDA_EXCLUDEFROMCAPTURE, conferido por GetWindowDisplayAffinity. Falso = a janela aparece na captura. Só chamar com
	// SetDisplayAffinitySupported() (antes do Windows 10 2004 a flag vira caixa preta).
	bool excludeFromCapture();
	// Novo retângulo (vale no próximo render).
	void setRect(const MonitorRect &rect);
	// Desenha `strokes` (pixels da captura captureWidth×captureHeight, esticados para o retângulo) e mostra sem ativar;
	// sem traços, limpa e esconde.
	void render(const std::vector<Stroke> &strokes, int captureWidth, int captureHeight);
	// Pixel do último desenho como "#AARRGGBB" (pré-multiplicado); "" fora do buffer. Só para testes.
	std::string pixelHex(int x, int y) const;
	Info info() const;

private:
	void ensureClickThrough();
	bool ensureBuffer(int width, int height);
	void releaseBuffer();
	void hide();

	void *hwnd_ = nullptr; // HWND
	void *memoryDc_ = nullptr;
	void *bitmap_ = nullptr;
	void *previousBitmap_ = nullptr;
	uint8_t *bits_ = nullptr;
	int bufferWidth_ = 0;
	int bufferHeight_ = 0;
	MonitorRect rect_;
	bool visible_ = false;
};

} // namespace voltz
