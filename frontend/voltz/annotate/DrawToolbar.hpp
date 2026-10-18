#pragma once

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/cursor-track.hpp>

#include <QList>
#include <QPair>
#include <QString>
#include <QWidget>

class QButtonGroup;
class QFrame;
class QLabel;
class QScreen;
class QToolButton;

namespace voltz {

class ColorPicker;

// Barra do modo desenho (spec §6.2): caneta, marca-texto, linha, seta, retângulo, elipse, texto · 4 cores rápidas + 8 em
// "mais cores" + cor personalizada (seletor completo, sem diálogo) · 4 espessuras (ou, com o texto, 4 tamanhos e
// negrito) · desfazer · limpar · "traços somem após" (nunca → 3 → 5 → 10 s) · sair · linha de dica (a dica do botão sob
// o mouse; as dicas flutuantes do Qt não aparecem numa janela que nunca fica ativa). Janela Qt própria (sem pai: nada de
// dono de outra thread), sempre no topo, que NUNCA pega o foco (Qt::WindowDoesNotAcceptFocus + WS_EX_NOACTIVATE): a
// janela de entrada do desenho continua ativa e o Esc continua chegando nela. Com a propriedade isOBSProjectorWindow o
// OBSApp::notify não mexe na afinidade dela; fora da captura por WDA_EXCLUDEFROMCAPTURE quando o Windows suporta. Aviso
// visível quando as anotações estão "queimadas" (spec §6.3). A janela nativa só nasce no primeiro showOnScreen — o
// grab() do pedido de teste ToolbarImage não cria janela. Título nativo "VoltzDrawToolbar" (os testes a procuram para
// garantir que ela nunca aparece num teste automático). Thread da UI.
class DrawToolbar : public QWidget {
	Q_OBJECT

public:
	explicit DrawToolbar(QWidget *parent = nullptr);

	// Marca ferramenta, cor, espessura, tamanho/fonte do texto e "somem após" (não emite sinais).
	void setAnnotationStyle(const AnnotationStyle &style);
	void setBurned(bool burned);
	// Texto do atalho de desenhar na dica do botão Sair e na linha de dica.
	void setExitHint(const QString &shortcut);
	// Abre (ou fecha) "mais cores" com o seletor completo (o botão de cor personalizada faz o mesmo).
	void showColorPicker(bool show);
	// Mostra no alto e ao centro do monitor de nome GDI `deviceName` ("\\.\DISPLAYn", MonitorDescription::deviceName;
	// a QScreen sai do screenForMonitor, não do QScreen::name(), que no Qt 6 é o nome amigável), sem ativar. Monitor não
	// encontrado → tela principal.
	void showOnScreen(const QString &deviceName, bool excludeFromCapture);
	// Põe de novo por cima (a janela de entrada acabou de ir para o topo), sem ativar.
	void raiseWithoutActivating();
	// Retângulo da janela em pixels físicos do desktop (o buraco na janela de entrada); vazio se não está visível.
	MonitorRect nativeRect() const;

signals:
	void toolChosen(voltz::AnnotationTool tool);
	void colorChosen(voltz::RgbColor color);
	void widthChosen(int width);
	void textSizeChosen(int size);
	void textFontChosen(voltz::TextFont font);
	void fadeChosen(int seconds);
	void undoClicked();
	void clearClicked();
	void exitClicked();
	void nativeRectChanged();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void moveEvent(QMoveEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	QToolButton *makeButton(QWidget *parent, const QString &hint);
	QToolButton *addColor(QWidget *parent, const RgbColor &color);
	QFrame *separator(QWidget *parent);
	void markColor(const RgbColor &color);
	void updateFadeText();
	void updateHint();
	// adjustSize e, se visível, de volta ao alto e ao centro do monitor em que está (aviso de "queimadas", "mais
	// cores", troca entre espessuras e tamanhos do texto).
	void resizeAndRecenter();
	void placeOn(QScreen *screen);

	QFrame *panel_ = nullptr;
	QButtonGroup *tools_ = nullptr;
	QList<QPair<QToolButton *, RgbColor>> colors_;
	QList<QPair<QToolButton *, int>> widths_;
	QList<QPair<QToolButton *, int>> textSizes_;
	QWidget *widthGroup_ = nullptr;
	QWidget *textGroup_ = nullptr;
	QToolButton *bold_ = nullptr;
	QToolButton *moreColors_ = nullptr;
	QWidget *extraColors_ = nullptr;
	QToolButton *customColor_ = nullptr;
	ColorPicker *picker_ = nullptr;
	QToolButton *fade_ = nullptr;
	QToolButton *exit_ = nullptr;
	QLabel *burned_ = nullptr;
	QLabel *hint_ = nullptr;
	QString hovered_; // dica do botão sob o mouse ("" = a dica padrão)
	QString shortcut_;
	AnnotationStyle style_;
};

} // namespace voltz
