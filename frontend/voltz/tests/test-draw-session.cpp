#include <doctest.h>

#include <voltz/core/draw-session.hpp>

#include <cstdint>
#include <string>
#include <vector>

using voltz::DrawCommand;
using voltz::DrawCommandType;
using voltz::DrawExit;
using voltz::DrawInput;
using voltz::DrawInputKind;
using voltz::DrawRefusal;
using voltz::DrawSession;
using voltz::StrokePoint;

namespace {

constexpr int64_t kMs = 1000000;
constexpr int64_t kStart = 1000 * kMs;
// Monitor 1000×500 em (100,50) capturado em 2000×1000: cada pixel do monitor vale 2 da captura.
const voltz::CaptureMapping kMapping{voltz::MonitorRect{100, 50, 1000, 500}, 2000, 1000};

DrawInput at(DrawInputKind kind, int64_t tNs, int x = 0, int y = 0)
{
	DrawInput input;
	input.kind = kind;
	input.tNs = tNs;
	input.x = x;
	input.y = y;
	input.uiHeartbeatNs = tNs;
	return input;
}

DrawInput tick(int64_t tNs, int64_t heartbeatNs)
{
	DrawInput input;
	input.kind = DrawInputKind::Tick;
	input.tNs = tNs;
	input.uiHeartbeatNs = heartbeatNs;
	return input;
}

// "begin(1000,500) extend(1020,510) end exit:esc"
std::string describe(const std::vector<DrawCommand> &commands)
{
	std::string text;
	for (const DrawCommand &command : commands) {
		if (!text.empty()) {
			text += ' ';
		}
		switch (command.type) {
		case DrawCommandType::Begin:
			text += "begin";
			break;
		case DrawCommandType::Extend:
			text += "extend";
			break;
		case DrawCommandType::End:
			text += "end";
			break;
		case DrawCommandType::Undo:
			text += "undo";
			break;
		case DrawCommandType::Exit:
			text += std::string("exit:") + voltz::drawExitName(command.exit);
			break;
		}
		for (const StrokePoint &point : command.points) {
			text += "(" + std::to_string(point.x) + "," + std::to_string(point.y) + ")";
		}
	}
	return text;
}

} // namespace

TEST_CASE("checkDrawEntry: saindo vence teste, que vence sem alvo; nomes das recusas")
{
	CHECK(voltz::checkDrawEntry(false, false, true) == DrawRefusal::None);
	CHECK(voltz::checkDrawEntry(true, true, false) == DrawRefusal::ShuttingDown);
	CHECK(voltz::checkDrawEntry(false, true, true) == DrawRefusal::TestMode);
	CHECK(voltz::checkDrawEntry(false, true, false) == DrawRefusal::TestMode);
	CHECK(voltz::checkDrawEntry(false, false, false) == DrawRefusal::NoTarget);
	CHECK(std::string(voltz::drawRefusalName(DrawRefusal::None)).empty());
	CHECK(std::string(voltz::drawRefusalName(DrawRefusal::ShuttingDown)) == "saindo");
	CHECK(std::string(voltz::drawRefusalName(DrawRefusal::TestMode)) == "teste");
	CHECK(std::string(voltz::drawRefusalName(DrawRefusal::NoTarget)) == "sem_alvo");
}

TEST_CASE("drawExitName: um nome para cada saída")
{
	const DrawExit exits[] = {DrawExit::Escape,    DrawExit::RightClick, DrawExit::Hotkey,   DrawExit::Button,
				  DrawExit::FocusLost, DrawExit::Idle,       DrawExit::Watchdog, DrawExit::Target,
				  DrawExit::Close,     DrawExit::Shutdown};
	const char *names[] = {"esc",         "clique_direito", "atalho", "botao",  "foco",
			       "inatividade", "vigia",          "alvo",   "fechar", "saida"};
	for (int i = 0; i < 10; i++) {
		CHECK(std::string(voltz::drawExitName(exits[i])) == names[i]);
	}
	CHECK(std::string(voltz::drawExitName(DrawExit::None)).empty());
}

TEST_CASE("DrawSession: arrastar com o botão esquerdo vira Begin, Extend e End em pixels da captura")
{
	DrawSession session(kMapping, kStart);
	CHECK(describe(session.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300))) == "begin(1000,500)");
	CHECK(session.drawing());
	CHECK(describe(session.handle(at(DrawInputKind::Move, kStart + 2 * kMs, 610, 305))) == "extend(1020,510)");
	CHECK(describe(session.handle(at(DrawInputKind::LeftUp, kStart + 3 * kMs, 620, 310))) ==
	      "extend(1040,520) end");
	CHECK_FALSE(session.drawing());
	CHECK(session.active());
	CHECK(session.exitReason() == DrawExit::None);
}

TEST_CASE("DrawSession: mover sem botão não desenha; ponto repetido não repete; arrastar para fora fica na borda")
{
	DrawSession session(kMapping, kStart);
	CHECK(session.handle(at(DrawInputKind::Move, kStart + 1 * kMs, 600, 300)).empty());
	CHECK(describe(session.handle(at(DrawInputKind::LeftDown, kStart + 2 * kMs, 600, 300))) == "begin(1000,500)");
	CHECK(session.handle(at(DrawInputKind::Move, kStart + 3 * kMs, 600, 300)).empty());
	CHECK(describe(session.handle(at(DrawInputKind::Move, kStart + 4 * kMs, 5000, -40))) == "extend(1998,0)");
	CHECK(describe(session.handle(at(DrawInputKind::LeftUp, kStart + 5 * kMs, 5000, -40))) == "end");
}

TEST_CASE("DrawSession: apertar fora do monitor não começa traço")
{
	DrawSession session(kMapping, kStart);
	CHECK(session.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 50, 300)).empty());
	CHECK(session.handle(at(DrawInputKind::LeftDown, kStart + 2 * kMs, 1100, 300)).empty());
	CHECK_FALSE(session.drawing());
	CHECK(session.handle(at(DrawInputKind::Move, kStart + 3 * kMs, 600, 300)).empty());
	CHECK(session.handle(at(DrawInputKind::LeftUp, kStart + 4 * kMs, 600, 300)).empty());
}

TEST_CASE("DrawSession: Esc termina o traço e sai; depois disso tudo é ignorado")
{
	DrawSession session(kMapping, kStart);
	session.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300));
	CHECK(describe(session.handle(at(DrawInputKind::Escape, kStart + 2 * kMs))) == "end exit:esc");
	CHECK_FALSE(session.active());
	CHECK_FALSE(session.drawing());
	CHECK(session.exitReason() == DrawExit::Escape);
	CHECK(session.handle(at(DrawInputKind::LeftDown, kStart + 3 * kMs, 600, 300)).empty());
	CHECK(session.handle(tick(kStart + 60000 * kMs, 0)).empty());
	CHECK(session.requestExit(DrawExit::Hotkey).empty());
	CHECK(session.exitReason() == DrawExit::Escape);
}

TEST_CASE("DrawSession: clique direito, perda de foco, captura perdida, Alt+F4 e troca de monitores saem")
{
	const DrawInputKind kinds[] = {DrawInputKind::RightDown, DrawInputKind::Deactivated, DrawInputKind::CaptureLost,
				       DrawInputKind::Close, DrawInputKind::TargetChanged};
	const char *expected[] = {"exit:clique_direito", "exit:foco", "exit:foco", "exit:fechar", "exit:alvo"};
	for (int i = 0; i < 5; i++) {
		DrawSession idle(kMapping, kStart);
		CHECK(describe(idle.handle(at(kinds[i], kStart + 1 * kMs, 600, 300))) == expected[i]);
		CHECK_FALSE(idle.active());
		DrawSession drawing(kMapping, kStart);
		drawing.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300));
		CHECK(describe(drawing.handle(at(kinds[i], kStart + 2 * kMs, 600, 300))) ==
		      std::string("end ") + expected[i]);
	}
}

TEST_CASE("DrawSession: 30 s sem entrada saem; qualquer entrada reinicia a conta")
{
	DrawSession fresh(kMapping, kStart);
	CHECK(fresh.handle(tick(kStart + 29999 * kMs, kStart + 29999 * kMs)).empty());
	CHECK(describe(fresh.handle(tick(kStart + 30000 * kMs, kStart + 30000 * kMs))) == "exit:inatividade");

	DrawSession busy(kMapping, kStart);
	busy.handle(at(DrawInputKind::OtherKey, kStart + 10000 * kMs));
	busy.handle(at(DrawInputKind::Activated, kStart + 20000 * kMs));
	CHECK(busy.handle(tick(kStart + 49999 * kMs, kStart + 49999 * kMs)).empty());
	busy.handle(at(DrawInputKind::Move, kStart + 49999 * kMs, 600, 300));
	CHECK(busy.handle(tick(kStart + 79998 * kMs, kStart + 79998 * kMs)).empty());
	CHECK(describe(busy.handle(tick(kStart + 79999 * kMs, kStart + 79999 * kMs))) == "exit:inatividade");
}

TEST_CASE("DrawSession: vigia sai com a UI 1 s sem pulsar, mesmo com entrada recente, e com pulso nunca dado")
{
	DrawSession session(kMapping, kStart);
	session.handle(at(DrawInputKind::Move, kStart + 5000 * kMs, 600, 300));
	CHECK(session.handle(tick(kStart + 5000 * kMs, kStart + 4001 * kMs)).empty());
	CHECK(describe(session.handle(tick(kStart + 5000 * kMs, kStart + 4000 * kMs))) == "exit:vigia");

	DrawSession never(kMapping, kStart);
	CHECK(describe(never.handle(tick(kStart + 100 * kMs, 0))) == "exit:vigia");

	DrawSession both(kMapping, kStart);
	both.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300));
	CHECK(describe(both.handle(tick(kStart + 40000 * kMs, kStart))) == "end exit:vigia");
}

TEST_CASE("DrawSession: saída pedida de fora termina o traço; pedir de novo não repete")
{
	DrawSession session(kMapping, kStart);
	session.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300));
	CHECK(describe(session.requestExit(DrawExit::Hotkey)) == "end exit:atalho");
	CHECK(session.requestExit(DrawExit::Shutdown).empty());
	CHECK(session.exitReason() == DrawExit::Hotkey);

	DrawSession other(kMapping, kStart);
	CHECK(other.requestExit(DrawExit::None).empty());
	CHECK(other.active());
	CHECK(describe(other.requestExit(DrawExit::Button)) == "exit:botao");
}

TEST_CASE("DrawSession: Ctrl+Z desfaz (termina antes o traço em andamento, que não continua)")
{
	DrawSession session(kMapping, kStart);
	session.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300));
	CHECK(describe(session.handle(at(DrawInputKind::Undo, kStart + 2 * kMs))) == "end undo");
	CHECK(session.handle(at(DrawInputKind::Move, kStart + 3 * kMs, 700, 300)).empty());
	CHECK(session.handle(at(DrawInputKind::LeftUp, kStart + 4 * kMs, 700, 300)).empty());
	CHECK(describe(session.handle(at(DrawInputKind::Undo, kStart + 5 * kMs))) == "undo");
	CHECK(session.active());
}
