#include "DrawModeController.hpp"
#include "AnnotationController.hpp"
#include "DrawInputWindow.hpp"
#include "DrawToolbar.hpp"

#include <voltz/core/live-protocol.hpp>
#include <voltz/shell/HotkeyBinding.hpp>

#include <OBSApp.hpp>           // QTStr
#include <utility/platform.hpp> // SetDisplayAffinitySupported

#include <util/platform.h>

#include <QSignalBlocker>
#include <QThread>

#include <initializer_list>
#include <mutex>
#include <string>
#include <vector>

#include "moc_DrawModeController.cpp"

namespace voltz {

namespace {

constexpr const char *kDrawHotkeyName = "VoltzStudio.Draw";
constexpr const char *kUndoHotkeyName = "VoltzStudio.UndoAnnotation";
constexpr const char *kClearHotkeyName = "VoltzStudio.ClearAnnotations";
constexpr const char *kFreezeHotkeyName = "VoltzStudio.DebugFreezeUi";
constexpr int kHeartbeatMs = 250;
constexpr int kTargetCheckBeats = 4; // confere a Tela/monitor a cada ~1 s
constexpr unsigned long kFreezeMs = 8000;
constexpr int64_t kFocusLostClickNs = 500LL * 1000000LL; // um clique (apertar e soltar) cabe nisso

int64_t nowNs()
{
	return static_cast<int64_t>(os_gettime_ns());
}

bool sameMapping(const CaptureMapping &a, const CaptureMapping &b)
{
	return a.monitor.left == b.monitor.left && a.monitor.top == b.monitor.top &&
	       a.monitor.width == b.monitor.width && a.monitor.height == b.monitor.height &&
	       a.captureWidth == b.captureWidth && a.captureHeight == b.captureHeight;
}

} // namespace

DrawModeController::DrawModeController(AnnotationController *annotations, QObject *parent)
	: QObject(parent),
	  annotations_(annotations)
{
	heartbeat_.setInterval(kHeartbeatMs);
	connect(&heartbeat_, &QTimer::timeout, this, [this]() { beat(); });
	/* A barra reflete o estilo de verdade (também o que ela mesma escolheu: clicar duas vezes na mesma espessura não a
	 * deixa desmarcada). */
	connect(annotations_, &AnnotationController::styleChanged, this, [this]() {
		if (toolbar_) {
			toolbar_->setAnnotationStyle(annotations_->style());
		}
		if (shared_) {
			shared_->textTool.store(annotations_->style().tool == AnnotationTool::Text);
		}
	});
	connect(annotations_, &AnnotationController::burnedChanged, this, [this](bool burned) {
		if (toolbar_) {
			toolbar_->setBurned(burned);
		}
	});
	/* A casca nasce depois do OBSBasic::OBSInit: perfil e atalhos do OBS já estão carregados. */
	registerHotkeys();
	obs_frontend_add_event_callback(&DrawModeController::handleEvent, this);
	eventsRegistered_ = true;
	blog(LOG_INFO, "[voltz] modo desenho: desligado ao abrir (so liga pelo atalho ou pelo botao Desenhar)");
}

DrawModeController::~DrawModeController()
{
	/* Normalmente o EXIT já chamou o shutdown. Se não (casca destruída sem o EXIT), os irmãos mais velhos na casca
	 * (LiveMouse, AnnotationController) já morreram: nenhum sinal sai daqui (o activeChanged chegaria ao LiveMouse pela
	 * casca) e o finish não toca nas anotações (QPointer nulo). */
	const QSignalBlocker blocker(this);
	shutdown();
}

void DrawModeController::shutdown()
{
	if (shutdown_) {
		return;
	}
	shutdown_ = true; /* daqui em diante nada liga o modo (checkDrawEntry recusa "saindo") */
	if (input_) {
		input_->requestExit(DrawExit::Shutdown);
		/* O destrutor espera a thread de entrada até 2 s (nada, se o start já esgotou o prazo): pior caso ~2 s para a
		 * UI. Sem o mutex do DrawShared preso: no fim ela o pega (bloqueante) para entregar o Exit. */
		input_.reset();
		drain(); /* o End do traço em andamento e o Exit chegam aqui → finish */
	}
	if (shared_) {
		finish(DrawExit::Shutdown); /* a thread não entregou o Exit a tempo (ficou destacada e sai sozinha) */
	}
	unregisterHotkeys();
	if (eventsRegistered_) {
		/* seguro durante o EXIT: OBSStudioAPI::on_event percorre os callbacks de trás para frente */
		obs_frontend_remove_event_callback(&DrawModeController::handleEvent, this);
		eventsRegistered_ = false;
	}
	delete toolbar_;
	toolbar_ = nullptr;
}

void DrawModeController::toggle(DrawTrigger trigger)
{
	if (shutdown_) {
		return;
	}
	if (input_) {
		input_->requestExit(trigger == DrawTrigger::Hotkey ? DrawExit::Hotkey : DrawExit::Button);
		return;
	}
	enter(trigger);
}

void DrawModeController::enter(DrawTrigger trigger)
{
	const bool testMode = live::testModeEnabled();
	const bool shuttingDown = shutdown_ || !annotations_;
	/* O atalho ou o clique em "Desenhar" que ficou na fila durante uma trava da UI chega logo depois do drain que tratou
	 * a saída (vigia): queria desligar o modo, não religá-lo. */
	const bool recent = !shuttingDown && recentFreezeExit();
	CaptureMapping mapping;
	std::string device;
	/* com VOLTZ_TEST_LIVE nem procura o alvo: a recusa vem antes de qualquer janela */
	const bool hasTarget = !shuttingDown && !testMode && !recent && annotations_->drawTarget(mapping, device);
	const DrawRefusal refusal = checkDrawEntry(shuttingDown, testMode, recent, hasTarget);
	if (refusal != DrawRefusal::None) {
		blog(LOG_INFO, "[voltz] modo desenho: recusado (%s)", drawRefusalName(refusal));
		if (annotations_) {
			annotations_->setDrawStatus(false, drawRefusalName(refusal), "");
		}
		if (refusal == DrawRefusal::TestMode) {
			emit refused(QTStr("Voltz.Draw.Refused.Test"));
		} else if (refusal == DrawRefusal::Recent) {
			emit refused(QTStr("Voltz.Draw.Refused.Recent"));
		} else if (refusal == DrawRefusal::NoTarget) {
			emit refused(QTStr("Voltz.Draw.Refused.NoTarget"));
		}
		return;
	}
	/* As medidas de redesenho do log "desligado (...)" são só desta sessão. */
	annotations_->resetOverlayStats();
	/* Barra e janela de entrada fora da gravação sempre que o Windows suporta — mesmo com as anotações "queimadas" por
	 * outro motivo (afinidade recusada na sobreposição, VOLTZ_TEST_ANNOT_BURNED): a barra nunca é conteúdo do vídeo. */
	const bool exclude = SetDisplayAffinitySupported();
	shared_ = std::make_shared<DrawShared>();
	shared_->uiHeartbeatNs.store(nowNs());
	shared_->textTool.store(annotations_->style().tool == AnnotationTool::Text);
	{
		std::lock_guard<std::mutex> lock(shared_->mutex);
		shared_->wake = &DrawModeController::wake;
		shared_->wakeContext = this;
	}
	mapping_ = mapping;
	beats_ = 0;
	heartbeat_.start();
	if (!toolbar_) {
		createToolbar();
	}
	toolbar_->setAnnotationStyle(annotations_->style());
	toolbar_->setBurned(annotations_->burned());
	toolbar_->setExitHint(drawShortcutText());
	/* nome GDI ("\\.\DISPLAYn") do drawTarget, não QScreen::name() */
	toolbar_->showOnScreen(QString::fromStdString(device), exclude);
	{
		/* Qt/Win32 fora do mutex; sob ele, só a atribuição */
		const MonitorRect hole = toolbar_->nativeRect();
		std::lock_guard<std::mutex> lock(shared_->mutex);
		shared_->hole = hole;
	}
	DrawInputWindow::Options options;
	options.mapping = mapping;
	options.excludeFromCapture = exclude;
	/* pulso fresco logo antes de a thread de entrada nascer (o vigia dela sai com o pulso velho de 1 s) */
	shared_->uiHeartbeatNs.store(nowNs());
	input_ = DrawInputWindow::start(shared_, options);
	if (input_) {
		/* o start pode ter esperado a janela nascer (até 2 s, UI parada): pulso fresco antes do primeiro tique */
		shared_->uiHeartbeatNs.store(nowNs());
	}
	if (!input_) {
		blog(LOG_WARNING, "[voltz] modo desenho: recusado (janela)");
		heartbeat_.stop();
		{
			std::lock_guard<std::mutex> lock(shared_->mutex);
			shared_->wake = nullptr;
		}
		shared_.reset();
		toolbar_->hide();
		annotations_->setDrawStatus(false, "janela", "");
		emit refused(QTStr("Voltz.Draw.Refused.Window"));
		return;
	}
	/* a janela de entrada acabou de ir para o topo: a barra volta por cima (e tem o buraco na região dela) */
	toolbar_->raiseWithoutActivating();
	annotations_->setDrawStatus(true, "", "");
	blog(LOG_INFO,
	     "[voltz] modo desenho: ligado (%s; monitor %dx%d em %d,%d; captura %dx%d; ativa: %s; fora da captura: %s)",
	     trigger == DrawTrigger::Hotkey ? "atalho" : "botao", mapping.monitor.width, mapping.monitor.height,
	     mapping.monitor.left, mapping.monitor.top, mapping.captureWidth, mapping.captureHeight,
	     input_->activated() ? "sim" : "nao (o primeiro clique ativa)", exclude ? "sim" : "nao");
	emit activeChanged(true);
}

void DrawModeController::createToolbar()
{
	toolbar_ = new DrawToolbar(nullptr);
	connect(toolbar_, &DrawToolbar::toolChosen, this, [this](AnnotationTool tool) {
		AnnotationStyle style = annotations_->style();
		style.tool = tool;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::colorChosen, this, [this](RgbColor color) {
		AnnotationStyle style = annotations_->style();
		style.color = color;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::widthChosen, this, [this](int width) {
		AnnotationStyle style = annotations_->style();
		style.width = width;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::textSizeChosen, this, [this](int size) {
		AnnotationStyle style = annotations_->style();
		style.textSize = size;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::textFontChosen, this, [this](TextFont font) {
		AnnotationStyle style = annotations_->style();
		style.textFont = font;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::fadeChosen, this, [this](int seconds) {
		AnnotationStyle style = annotations_->style();
		style.fadeSeconds = seconds;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::undoClicked, this, [this]() { undoAnnotation(); });
	connect(toolbar_, &DrawToolbar::clearClicked, this, [this]() { clearAnnotations(); });
	connect(toolbar_, &DrawToolbar::exitClicked, this, [this]() {
		if (input_) {
			input_->requestExit(DrawExit::Button);
		}
	});
	connect(toolbar_, &DrawToolbar::nativeRectChanged, this, [this]() { updateHole(); });
}

void DrawModeController::undoAnnotation()
{
	if (shutdown_ || !annotations_) {
		return;
	}
	annotations_->undo();
	/* desfazer durante a digitação tirou o texto: a sessão para de digitar (senão as teclas seguiriam para um texto que
	 * não existe mais e o Esc só o "cancelaria" em vez de sair) */
	if (input_) {
		input_->endTyping();
	}
}

void DrawModeController::clearAnnotations()
{
	if (shutdown_ || !annotations_) {
		return;
	}
	annotations_->clear();
	/* Limpar leva o texto em andamento junto: a sessão para de digitar (ver undoAnnotation) */
	if (input_) {
		input_->endTyping();
	}
}

void DrawModeController::updateHole()
{
	if (!shared_ || !toolbar_) {
		return;
	}
	{
		/* Qt/Win32 fora do mutex; sob ele, só a atribuição */
		const MonitorRect hole = toolbar_->nativeRect();
		std::lock_guard<std::mutex> lock(shared_->mutex);
		shared_->hole = hole;
	}
	if (input_) {
		input_->holeChanged();
	}
}

void DrawModeController::beat()
{
	if (!shared_) {
		return;
	}
	shared_->uiHeartbeatNs.store(nowNs());
	if (++beats_ % kTargetCheckBeats != 0 || !input_) {
		return;
	}
	CaptureMapping mapping;
	std::string device;
	if (!annotations_ || !annotations_->drawTarget(mapping, device) || !sameMapping(mapping, mapping_)) {
		blog(LOG_INFO, "[voltz] modo desenho: a Tela ou o monitor mudou");
		input_->requestExit(DrawExit::Target);
	}
}

void DrawModeController::wake(void *context)
{
	/* Thread de entrada, SOB o mutex do DrawShared: o finish anula `wake` sob o mesmo mutex antes de este objeto
	 * morrer, e evento postado para objeto já destruído o Qt descarta. */
	auto *self = static_cast<DrawModeController *>(context);
	QMetaObject::invokeMethod(self, [self]() { self->drain(); }, Qt::QueuedConnection);
}

void DrawModeController::drain()
{
	if (!shared_) {
		return;
	}
	const std::vector<DrawCommand> commands = takeDrawCommands(*shared_);
	if (!annotations_) {
		/* destrutor sem o EXIT: o AnnotationController já morreu; dos comandos, só a saída importa */
		for (const DrawCommand &command : commands) {
			if (command.type == DrawCommandType::Exit) {
				finish(command.exit);
				return;
			}
		}
		return;
	}
	const AnnotationStyle style = annotations_->style();
	std::vector<StrokePoint> pending;
	auto flush = [this, &pending]() {
		if (!pending.empty()) {
			annotations_->extendStroke(pending);
			pending.clear();
		}
	};
	for (const DrawCommand &command : commands) {
		switch (command.type) {
		case DrawCommandType::Begin:
			flush();
			if (!command.points.empty()) {
				annotations_->beginStroke(style.tool, style.color, style.width, command.points.front());
				/* o traço pode ter acabado de mostrar a sobreposição (HWND_TOPMOST, por cima de tudo, inclusive da
				 * barra): a barra volta para cima, sem ativar */
				if (toolbar_) {
					toolbar_->raiseWithoutActivating();
				}
			}
			break;
		case DrawCommandType::Extend:
			pending.insert(pending.end(), command.points.begin(), command.points.end());
			break;
		case DrawCommandType::End:
			flush();
			annotations_->endStroke();
			break;
		case DrawCommandType::Undo:
			flush();
			annotations_->undo();
			break;
		case DrawCommandType::TextBegin:
			flush();
			if (!command.points.empty()) {
				annotations_->beginText(command.points.front());
				if (toolbar_) {
					toolbar_->raiseWithoutActivating();
				}
			}
			break;
		case DrawCommandType::TextEdit:
			flush();
			annotations_->updateText(command.text);
			break;
		case DrawCommandType::TextCommit:
			flush();
			annotations_->commitText();
			break;
		case DrawCommandType::TextCancel:
			flush();
			annotations_->cancelText();
			break;
		case DrawCommandType::Exit:
			flush();
			finish(command.exit);
			return;
		}
	}
	flush();
}

void DrawModeController::finish(DrawExit reason)
{
	const int64_t now = nowNs();
	/* Saída durante uma trava da UI: o vigia, ou qualquer outra tratada aqui com o último pulso (a UI pulsa a cada
	 * 250 ms) mais velho que o prazo do vigia — o atalho/botão que ficou na fila não religa o modo (recentFreezeExit). */
	const int64_t lastBeat = shared_ ? shared_->uiHeartbeatNs.load() : 0;
	exitInFreeze_ = reason == DrawExit::Watchdog || (lastBeat != 0 && now - lastBeat > kDrawHeartbeatNs);
	exitNs_ = now;
	exitReason_ = reason;
	/* Nenhum traço fica aberto (a sessão sempre manda o End/TextCommit antes do Exit; isto cobre a thread que não o
	 * entregou — e confirma o texto em andamento). Sem traço aberto, não faz nada. */
	if (annotations_) {
		annotations_->endStroke();
	}
	heartbeat_.stop();
	if (shared_) {
		std::lock_guard<std::mutex> lock(shared_->mutex);
		shared_->wake = nullptr;
	}
	/* Fora do mutex: a thread de entrada já saiu ou sai agora; o destrutor espera até 2 s (e ela, no fim, pega o mutex
	 * para entregar o que restou). */
	input_.reset();
	shared_.reset();
	if (toolbar_) {
		toolbar_->hide();
	}
	std::string stats = "indisponivel";
	if (annotations_) {
		annotations_->setDrawStatus(false, "", drawExitName(reason));
		stats = annotations_->overlayStatsText();
	}
	blog(LOG_INFO, "[voltz] modo desenho: desligado (%s); sobreposicao: %s", drawExitName(reason), stats.c_str());
	if (exitInFreeze_ && !shutdown_) {
		blog(LOG_INFO,
		     "[voltz] modo desenho: saida durante uma trava da UI; atalho e botao nao religam por %lld ms",
		     static_cast<long long>(kDrawReentryBlockNs / 1000000));
	}
	emit activeChanged(false);
}

bool DrawModeController::lostFocusRecently() const
{
	return exitReason_ == DrawExit::FocusLost && exitNs_ != 0 && nowNs() - exitNs_ < kFocusLostClickNs;
}

bool DrawModeController::recentFreezeExit() const
{
	return exitInFreeze_ && exitNs_ != 0 && nowNs() - exitNs_ < kDrawReentryBlockNs;
}

QString DrawModeController::drawShortcutText() const
{
	const QString text = voltz::hotkeyShortcutText(drawHotkey_);
	return text.isEmpty() ? QTStr("Voltz.Record.Draw.NoShortcut") : text;
}

void DrawModeController::registerHotkeys()
{
	drawHotkey_ = obs_hotkey_register_frontend(kDrawHotkeyName, QTStr("Voltz.Hotkey.Draw").toUtf8().constData(),
						   &DrawModeController::handleDrawHotkey, this);
	undoHotkey_ = obs_hotkey_register_frontend(kUndoHotkeyName,
						   QTStr("Voltz.Hotkey.UndoAnnotation").toUtf8().constData(),
						   &DrawModeController::handleUndoHotkey, this);
	clearHotkey_ = obs_hotkey_register_frontend(kClearHotkeyName,
						    QTStr("Voltz.Hotkey.ClearAnnotations").toUtf8().constData(),
						    &DrawModeController::handleClearHotkey, this);
	if (qEnvironmentVariableIsSet(live::kDebugFreezeEnv)) {
		freezeHotkey_ = obs_hotkey_register_frontend(kFreezeHotkeyName,
							     QTStr("Voltz.Hotkey.DebugFreezeUi").toUtf8().constData(),
							     &DrawModeController::handleFreezeHotkey, this);
		blog(LOG_WARNING, "[voltz] depuracao (%s): atalho %s ligado", live::kDebugFreezeEnv, kFreezeHotkeyName);
	}
	loadHotkeyBindings();
}

void DrawModeController::loadHotkeyBindings()
{
	/* Padrões na tabela do núcleo (hotkey-bindings): Ctrl+Shift+D e Ctrl+Shift+F12; desfazer e limpar sem tecla —
	 * atalhos do OBS não consomem a tecla (Ctrl+Shift+Z iria também para o app em uso). Dentro do modo desenho, Ctrl+Z
	 * desfaz pela janela de entrada. */
	voltz::loadHotkeyBinding(drawHotkey_, kDrawHotkeyName);
	voltz::loadHotkeyBinding(undoHotkey_, kUndoHotkeyName);
	voltz::loadHotkeyBinding(clearHotkey_, kClearHotkeyName);
	voltz::loadHotkeyBinding(freezeHotkey_, kFreezeHotkeyName);
}

void DrawModeController::unregisterHotkeys()
{
	for (obs_hotkey_id *id : {&drawHotkey_, &undoHotkey_, &clearHotkey_, &freezeHotkey_}) {
		if (*id != OBS_INVALID_HOTKEY_ID) {
			obs_hotkey_unregister(*id);
			*id = OBS_INVALID_HOTKEY_ID;
		}
	}
}

void DrawModeController::handleEvent(enum obs_frontend_event event, void *data)
{
	auto *self = static_cast<DrawModeController *>(data);
	switch (event) {
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		self->loadHotkeyBindings();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		/* Criado depois da FrontendBridge, do ProjectRecorder e do MouseStyleStore, este callback roda ANTES dos
		 * deles (OBSStudioAPI::on_event vai de trás para frente): o modo sai e o traço em andamento termina (e vai
		 * para o diário do projeto) antes de qualquer outro atalho ser solto ou de o projeto fechar. O
		 * exiting da ponte chama de novo (sem efeito). */
		self->shutdown();
		break;
	default:
		break;
	}
}

// Teclado: rota do OBSBasic (thread da UI). obs-websocket TriggerHotkeyByName: thread do websocket com o lock dos
// atalhos preso. Os quatro só enfileiram.
void DrawModeController::handleDrawHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<DrawModeController *>(data);
	QMetaObject::invokeMethod(self, [self]() { self->toggle(DrawTrigger::Hotkey); }, Qt::QueuedConnection);
}

void DrawModeController::handleUndoHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<DrawModeController *>(data);
	QMetaObject::invokeMethod(self, [self]() { self->undoAnnotation(); }, Qt::QueuedConnection);
}

void DrawModeController::handleClearHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<DrawModeController *>(data);
	QMetaObject::invokeMethod(self, [self]() { self->clearAnnotations(); }, Qt::QueuedConnection);
}

void DrawModeController::handleFreezeHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<DrawModeController *>(data);
	QMetaObject::invokeMethod(
		self,
		[]() {
			blog(LOG_WARNING, "[voltz] depuracao: thread da UI travada por %lu ms (%s)", kFreezeMs,
			     live::kDebugFreezeEnv);
			QThread::msleep(kFreezeMs);
			blog(LOG_INFO, "[voltz] depuracao: thread da UI de volta");
		},
		Qt::QueuedConnection);
}

} // namespace voltz
