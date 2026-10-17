#pragma once

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/annotation-raster.hpp>
#include <voltz/core/cursor-track.hpp>
#include <voltz/core/stroke-store.hpp>

#include <QObject>
#include <QTimer>

#include <cstdint>
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
// Thread da UI.
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
	void endStroke();
	void undo();
	void clear();
	// Ferramenta, cor e espessura do próximo traço e "somem após" (limitados). Valem na hora (styleChanged); o user.ini
	// é gravado 400 ms depois da última mudança.
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
	void removed(const std::vector<RemovedStroke> &strokes, bool all);
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
	void markBurned(const char *why);

	StrokeStore store_;
	AnnotationStyle style_;
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
