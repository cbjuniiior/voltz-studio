#include "draw-session.hpp"
#include "annotation-text.hpp"

#include <algorithm>
#include <utility>

namespace voltz {

namespace {

DrawCommand command(DrawCommandType type, std::vector<StrokePoint> points = {}, DrawExit exit = DrawExit::None)
{
	DrawCommand out;
	out.type = type;
	out.points = std::move(points);
	out.exit = exit;
	return out;
}

DrawCommand textEdit(const std::string &text)
{
	DrawCommand out = command(DrawCommandType::TextEdit);
	out.text = text;
	return out;
}

} // namespace

const char *drawExitName(DrawExit exit)
{
	switch (exit) {
	case DrawExit::Escape:
		return "esc";
	case DrawExit::RightClick:
		return "clique_direito";
	case DrawExit::Hotkey:
		return "atalho";
	case DrawExit::Button:
		return "botao";
	case DrawExit::FocusLost:
		return "foco";
	case DrawExit::Idle:
		return "inatividade";
	case DrawExit::Watchdog:
		return "vigia";
	case DrawExit::Target:
		return "alvo";
	case DrawExit::Close:
		return "fechar";
	case DrawExit::Shutdown:
		return "saida";
	case DrawExit::None:
		break;
	}
	return "";
}

const char *drawRefusalName(DrawRefusal refusal)
{
	switch (refusal) {
	case DrawRefusal::ShuttingDown:
		return "saindo";
	case DrawRefusal::TestMode:
		return "teste";
	case DrawRefusal::Recent:
		return "recente";
	case DrawRefusal::NoTarget:
		return "sem_alvo";
	case DrawRefusal::None:
		break;
	}
	return "";
}

DrawRefusal checkDrawEntry(bool shuttingDown, bool testMode, bool recentFreezeExit, bool hasTarget)
{
	if (shuttingDown) {
		return DrawRefusal::ShuttingDown;
	}
	if (testMode) {
		return DrawRefusal::TestMode;
	}
	if (recentFreezeExit) {
		return DrawRefusal::Recent;
	}
	if (!hasTarget) {
		return DrawRefusal::NoTarget;
	}
	return DrawRefusal::None;
}

DrawSession::DrawSession(const CaptureMapping &mapping, int64_t startNs, int64_t idleNs, int64_t heartbeatNs)
	: mapping_(mapping),
	  lastInputNs_(startNs),
	  idleNs_(idleNs),
	  heartbeatNs_(heartbeatNs)
{
}

StrokePoint DrawSession::clampedPoint(int x, int y) const
{
	const MonitorRect &rect = mapping_.monitor;
	if (rect.width <= 0 || rect.height <= 0) {
		return StrokePoint{};
	}
	const int64_t dx = std::clamp<int64_t>(static_cast<int64_t>(x) - rect.left, 0, rect.width - 1);
	const int64_t dy = std::clamp<int64_t>(static_cast<int64_t>(y) - rect.top, 0, rect.height - 1);
	const CapturePoint point =
		mapToCapture(mapping_, static_cast<int>(rect.left + dx), static_cast<int>(rect.top + dy));
	return StrokePoint{point.x, point.y};
}

void DrawSession::closeText(std::vector<DrawCommand> &out)
{
	if (!typing_) {
		return;
	}
	out.push_back(command(text_.empty() ? DrawCommandType::TextCancel : DrawCommandType::TextCommit));
	typing_ = false;
	text_.clear();
}

std::vector<DrawCommand> DrawSession::finish(DrawExit reason)
{
	std::vector<DrawCommand> out;
	if (drawing_) {
		out.push_back(command(DrawCommandType::End));
		drawing_ = false;
	}
	closeText(out);
	exit_ = reason;
	out.push_back(command(DrawCommandType::Exit, {}, reason));
	return out;
}

std::vector<DrawCommand> DrawSession::requestExit(DrawExit reason)
{
	if (!active() || reason == DrawExit::None) {
		return {};
	}
	return finish(reason);
}

std::vector<DrawCommand> DrawSession::handle(const DrawInput &input)
{
	if (!active()) {
		return {};
	}
	switch (input.kind) {
	case DrawInputKind::Tick:
		/* o vigia vem antes da inatividade: com a UI parada, sair é o que destrava o usuário */
		/* pulso nunca dado (<= 0) sai já, sem depender de o relógio ter passado de 1 s */
		if (input.uiHeartbeatNs <= 0 || input.tNs - input.uiHeartbeatNs >= heartbeatNs_) {
			return finish(DrawExit::Watchdog);
		}
		if (input.tNs - lastInputNs_ >= idleNs_) {
			return finish(DrawExit::Idle);
		}
		return {};
	case DrawInputKind::Escape:
		if (typing_) {
			/* o primeiro Esc só cancela o texto (conta como atividade); o seguinte sai */
			lastInputNs_ = std::max(lastInputNs_, input.tNs);
			typing_ = false;
			text_.clear();
			return {command(DrawCommandType::TextCancel)};
		}
		return finish(DrawExit::Escape);
	case DrawInputKind::RightDown:
		return finish(DrawExit::RightClick);
	case DrawInputKind::Deactivated:
	case DrawInputKind::CaptureLost:
		return finish(DrawExit::FocusLost);
	case DrawInputKind::Close:
		return finish(DrawExit::Close);
	case DrawInputKind::TargetChanged:
		return finish(DrawExit::Target);
	default:
		break;
	}

	lastInputNs_ = std::max(lastInputNs_, input.tNs);
	std::vector<DrawCommand> out;
	switch (input.kind) {
	case DrawInputKind::LeftDown: {
		if (drawing_) {
			break;
		}
		const CapturePoint point = mapToCapture(mapping_, input.x, input.y);
		if (!point.inside) {
			break;
		}
		/* clicar de novo confirma o texto que estava sendo digitado */
		closeText(out);
		if (input.textTool) {
			typing_ = true;
			text_.clear();
			out.push_back(command(DrawCommandType::TextBegin, {StrokePoint{point.x, point.y}}));
			break;
		}
		drawing_ = true;
		last_ = StrokePoint{point.x, point.y};
		out.push_back(command(DrawCommandType::Begin, {last_}));
		break;
	}
	case DrawInputKind::Move:
		if (drawing_) {
			const StrokePoint point = clampedPoint(input.x, input.y);
			if (!(point == last_)) {
				last_ = point;
				out.push_back(command(DrawCommandType::Extend, {point}));
			}
		}
		break;
	case DrawInputKind::LeftUp:
		if (drawing_) {
			const StrokePoint point = clampedPoint(input.x, input.y);
			if (!(point == last_)) {
				last_ = point;
				out.push_back(command(DrawCommandType::Extend, {point}));
			}
			out.push_back(command(DrawCommandType::End));
			drawing_ = false;
		}
		break;
	case DrawInputKind::Undo:
		if (typing_) {
			/* durante a digitação, desfazer tira só o texto em andamento */
			typing_ = false;
			text_.clear();
			out.push_back(command(DrawCommandType::TextCancel));
			break;
		}
		/* desfazer tira o traço em andamento: o botão segue apertado, mas nada mais vai para ele */
		if (drawing_) {
			out.push_back(command(DrawCommandType::End));
			drawing_ = false;
		}
		out.push_back(command(DrawCommandType::Undo));
		break;
	case DrawInputKind::Char:
		if (typing_ && appendTextCodepoint(text_, input.codepoint)) {
			out.push_back(textEdit(text_));
		}
		break;
	case DrawInputKind::Backspace:
		if (typing_ && popTextCodepoint(text_)) {
			out.push_back(textEdit(text_));
		}
		break;
	case DrawInputKind::Enter:
		if (!typing_) {
			break;
		}
		if (input.shift) {
			if (appendTextCodepoint(text_, U'\n')) {
				out.push_back(textEdit(text_));
			}
			break;
		}
		closeText(out);
		break;
	default:
		break; /* Activated, OtherKey: só atividade */
	}
	return out;
}

} // namespace voltz
