#pragma once

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/annotation-raster.hpp>
#include <voltz/core/cursor-track.hpp>
#include <voltz/core/stroke-store.hpp>

#include <QObject>
#include <QTimer>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace voltz {

class OverlayWindow;

// Anotações ao vivo (spec §6.2). Dono do StrokeStore (traços em pixels da captura da Tela compartilhada); repassa cada
// mudança ao plugin voltz-sources (procedimentos voltz_annot_*, desenhados pela fonte Voltz Anotações no programa), à
// sobreposição do monitor da Tela (OverlayWindow: transparente a cliques, fora da captura por WDA_EXCLUDEFROMCAPTURE) e a
// quem grava (ProjectRecorder → anotacoes.jsonl, pelos sinais). "Queimadas" (Windows < 10 2004 ou afinidade recusada,
// spec §6.3): a sobreposição fica sem afinidade (aparece na captura), a fonte não desenha (não duplica) e o manifesto
// marca anotacoes_queimadas. "Somem após N s" pelo estilo (user.ini [Voltz] AnnotationStyle). Plano 4C: os traços
// chegam do modo desenho (DrawModeController) e, com VOLTZ_TEST_LIVE, do fornecedor de teste; a sobreposição redesenha
// só a região que mudou; o estilo escolhido na barra é salvo; drawTarget diz onde o modo desenho pode desenhar.
// Plano 4D: texto (beginText/updateText/commitText/cancelText) — o texto em andamento aparece ao vivo no programa e na
// sobreposição, que desenha também o cursor de digitação (só ela: o cursor não é traço). O conteúdo digitado nunca vai
// para o log. Thread da UI.
class AnnotationController : public QObject {
	Q_OBJECT

public:
	explicit AnnotationController(QObject *parent = nullptr);
	~AnnotationController() override;

	// EXIT do frontend, antes de o libobs descarregar o plugin: grava o estilo pendente, destrói a sobreposição, para os
	// temporizadores e os pedidos de teste; depois disso nada mais é enviado ao plugin.
	void shutdown();

	// Começa um traço (termina antes o que estiver em andamento). Devolve o id (0 se desligado).
	int beginStroke(AnnotationTool tool, const RgbColor &color, int width, const StrokePoint &first);
	void extendStroke(const std::vector<StrokePoint> &points);
	// Termina o traço em andamento; texto em andamento é confirmado (commitText).
	void endStroke();
	// Durante a digitação, desfazer cancela só o texto (cancelText).
	void undo();
	void clear();
	// Texto (Plano 4D). Começa um texto VAZIO na âncora (pixels da captura) com a cor, o tamanho e a fonte do estilo
	// (termina antes o traço em andamento); a sobreposição mostra o cursor. Devolve o id (0 se desligado).
	int beginText(const StrokePoint &anchor);
	// Troca o texto em andamento (limpo e limitado pelo núcleo): o plugin recebe o traço inteiro de novo e a sobreposição
	// redesenha o antes ∪ o depois (com o cursor).
	void updateText(const std::string &text);
	// Confirma o texto em andamento: vira um traço terminado (diário, "somem após"). Vazio = cancelText.
	void commitText();
	// Cancela o texto em andamento: vazio sai sem registro (nunca apareceu); com algo, sai como um desfazer.
	void cancelText();
	bool typingText() const;
	// Ferramenta, cor e espessura do próximo traço, "somem após" e tamanho/fonte do texto (limitados). Valem na hora
	// (styleChanged; o texto em andamento muda de cor, tamanho e fonte); o user.ini é gravado 400 ms depois da última
	// mudança.
	void setStyle(const AnnotationStyle &style);
	// Onde o modo desenho pode desenhar: o monitor da Tela compartilhada (captura de monitor) e o tamanho da captura;
	// nunca o retângulo de teste. `deviceName` = nome GDI do monitor. Falso sem captura de monitor.
	bool drawTarget(CaptureMapping &mapping, std::string &deviceName) const;
	// Estado do modo desenho no AnnotationState (testes): ligado, último motivo de recusa e de saída ("" = nenhum).
	void setDrawStatus(bool active, const char *refusal, const char *exit);
	// "N parcial(is) (media X us, max Y us), C completa(s) (max Z us)" da sobreposição, para o log do modo desenho. Os
	// contadores acumulam desde a criação da sobreposição ou o último resetOverlayStats().
	std::string overlayStatsText() const;
	// Zera as medidas da sobreposição (o modo desenho chama no começo de cada sessão).
	void resetOverlayStats();
	// Só testes: guarda os pixels da sobreposição nesses pontos (pixels da janela) para o estado de teste.
	void probeOverlay(const std::vector<StrokePoint> &points);
	// Só testes (VOLTZ_TEST_LIVE): publica o estado para o pedido AnnotationState (inclui o tamanho atual da captura).
	void publishTestState();

	const std::vector<LiveStroke> &visibleStrokes() const { return store_.visible(); }
	const AnnotationStyle &style() const { return style_; }
	bool burned() const { return burned_; }

signals:
	// Direto (mesma thread): o ProjectRecorder escreve o diário na hora.
	void strokeEnded(const voltz::LiveStroke &stroke);
	void strokesRemoved(const std::vector<voltz::RemovedStroke> &removed);
	void burnedChanged(bool burned);
	void styleChanged();

private:
	// `extra` (cursor que saiu, por exemplo) entra na região redesenhada da sobreposição.
	void removed(const std::vector<RemovedStroke> &strokes, bool all, const Stroke *extra = nullptr);
	void scheduleExpiry();
	void expire();
	int64_t lifetimeNs() const;
	void saveStyle();
	// Recalcula o alvo da sobreposição (monitor da Tela) e redesenha se os traços (`strokesChanged`) ou o alvo mudaram.
	// `dirty` = região do buffer da sobreposição que mudou (nullopt = tudo); só vale se o alvo não mudou.
	void refreshOverlay(bool strokesChanged, const std::optional<PixelRect> &dirty = std::nullopt);
	bool overlayTarget(MonitorRect &rect, int &captureWidth, int &captureHeight) const;
	// Escala captura → buffer da sobreposição de agora; falso sem sobreposição ou sem alvo.
	bool overlayScale(double &scaleX, double &scaleY) const;
	std::vector<const Stroke *> strokePointers() const;
	// Os traços + o cursor de digitação (só na sobreposição).
	std::vector<const Stroke *> overlayStrokes() const;
	// Região do buffer da sobreposição que cobre esses traços (pixels da captura); nullopt sem sobreposição.
	std::optional<PixelRect> overlayBounds(std::initializer_list<const Stroke *> strokes) const;
	// O texto em andamento mudou (texto ou estilo): manda o traço inteiro ao plugin e redesenha antes ∪ depois.
	void textChanged(const Stroke &before, const Stroke &caretBefore);
	void markBurned(const char *why);

	StrokeStore store_;
	AnnotationStyle style_;
	Stroke caret_;        // cursor de digitação (pontos vazios = sem cursor)
	bool typing_ = false; // há um texto em andamento (o último traço do store_)
	std::unique_ptr<OverlayWindow> overlay_;
	MonitorRect overlayRect_;
	int captureWidth_ = 0;
	int captureHeight_ = 0;
	QTimer expiry_;
	QTimer overlayPoll_;
	QTimer saveTimer_;
	std::vector<std::string> samples_;
	std::string drawRefusal_;
	std::string drawExit_;
	bool drawActive_ = false;
	bool burned_ = false;
	bool testMode_ = false;
	bool shutdown_ = false;
	int overlayState_ = -1; // -1 desconhecido, 0 sem alvo, 1 com alvo (só para o log)
};

} // namespace voltz
