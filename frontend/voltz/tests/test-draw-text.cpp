#include <doctest.h>

#include <voltz/core/annotation-text.hpp>
#include <voltz/core/draw-session.hpp>

#include <cstdint>
#include <string>
#include <vector>

using voltz::DrawCommand;
using voltz::DrawCommandType;
using voltz::DrawExit;
using voltz::DrawInput;
using voltz::DrawInputKind;
using voltz::DrawSession;
using voltz::StrokePoint;

namespace {

constexpr int64_t kMs = 1000000;
constexpr int64_t kStart = 1000 * kMs;
// Monitor 1000×500 em (100,50) capturado em 2000×1000 (o mesmo do test-draw-session.cpp).
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

DrawInput click(int64_t tNs, int x, int y, bool textTool)
{
	DrawInput input = at(DrawInputKind::LeftDown, tNs, x, y);
	input.textTool = textTool;
	return input;
}

DrawInput typed(char32_t codepoint, int64_t tNs)
{
	DrawInput input = at(DrawInputKind::Char, tNs);
	input.codepoint = codepoint;
	return input;
}

DrawInput enter(int64_t tNs, bool shift)
{
	DrawInput input = at(DrawInputKind::Enter, tNs);
	input.shift = shift;
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

// "textbegin(200,100) edit:'Oi' commit exit:esc"
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
		case DrawCommandType::TextBegin:
			text += "textbegin";
			break;
		case DrawCommandType::TextEdit:
			text += "edit:'" + command.text + "'";
			break;
		case DrawCommandType::TextCommit:
			text += "commit";
			break;
		case DrawCommandType::TextCancel:
			text += "cancel";
			break;
		}
		for (const StrokePoint &point : command.points) {
			text += "(" + std::to_string(point.x) + "," + std::to_string(point.y) + ")";
		}
	}
	return text;
}

// Começa a digitação em (200,100) da captura (= (200,100) do desktop: (100+50, 50+25)).
DrawSession typingSession()
{
	DrawSession session(kMapping, kStart);
	REQUIRE(describe(session.handle(click(kStart, 200, 100, true))) == "textbegin(200,100)");
	REQUIRE(session.typing());
	return session;
}

} // namespace

TEST_CASE("texto: clique com a ferramenta texto começa a digitação na âncora; cada caractere manda o texto inteiro")
{
	DrawSession session = typingSession();
	CHECK_FALSE(session.drawing());
	CHECK(describe(session.handle(typed(U'O', kStart + 10 * kMs))) == "edit:'O'");
	CHECK(describe(session.handle(typed(U'i', kStart + 20 * kMs))) == "edit:'Oi'");
	CHECK(describe(session.handle(typed(U'\u00E1', kStart + 30 * kMs))) == "edit:'Oi\xC3\xA1'");
	/* arrastar e soltar com a ferramenta texto não desenha nada */
	CHECK(session.handle(at(DrawInputKind::Move, kStart + 40 * kMs, 300, 200)).empty());
	CHECK(session.handle(at(DrawInputKind::LeftUp, kStart + 50 * kMs, 300, 200)).empty());
	CHECK(session.typing());
}

TEST_CASE("texto: Backspace, Shift+Enter (nova linha) e Enter confirma; texto vazio confirma como cancelado")
{
	DrawSession session = typingSession();
	session.handle(typed(U'a', kStart + kMs));
	session.handle(typed(U'b', kStart + 2 * kMs));
	CHECK(describe(session.handle(at(DrawInputKind::Backspace, kStart + 3 * kMs))) == "edit:'a'");
	CHECK(describe(session.handle(enter(kStart + 4 * kMs, true))) == "edit:'a\n'");
	CHECK(describe(session.handle(typed(U'c', kStart + 5 * kMs))) == "edit:'a\nc'");
	CHECK(describe(session.handle(enter(kStart + 6 * kMs, false))) == "commit");
	CHECK_FALSE(session.typing());
	CHECK(session.active());
	/* Enter, Backspace e caracteres sem digitação: nada */
	CHECK(session.handle(enter(kStart + 7 * kMs, false)).empty());
	CHECK(session.handle(typed(U'x', kStart + 8 * kMs)).empty());
	CHECK(session.handle(at(DrawInputKind::Backspace, kStart + 9 * kMs)).empty());

	DrawSession empty = typingSession();
	CHECK(empty.handle(at(DrawInputKind::Backspace, kStart + kMs)).empty()); // nada a apagar
	CHECK(describe(empty.handle(enter(kStart + 2 * kMs, false))) == "cancel");
}

TEST_CASE("texto: Esc cancela só a digitação (sem sair); o Esc seguinte sai")
{
	DrawSession session = typingSession();
	session.handle(typed(U'x', kStart + kMs));
	CHECK(describe(session.handle(at(DrawInputKind::Escape, kStart + 2 * kMs))) == "cancel");
	CHECK(session.active());
	CHECK_FALSE(session.typing());
	CHECK(describe(session.handle(at(DrawInputKind::Escape, kStart + 3 * kMs))) == "exit:esc");
	CHECK_FALSE(session.active());
}

TEST_CASE("texto: Ctrl+Z durante a digitação cancela só o texto; depois desfaz o último traço")
{
	DrawSession session = typingSession();
	session.handle(typed(U'x', kStart + kMs));
	CHECK(describe(session.handle(at(DrawInputKind::Undo, kStart + 2 * kMs))) == "cancel");
	CHECK(describe(session.handle(at(DrawInputKind::Undo, kStart + 3 * kMs))) == "undo");
}

TEST_CASE("texto: Limpar ou desfazer pela UI encerra a digitação; as teclas seguintes não editam e o Esc sai")
{
	DrawSession session = typingSession();
	CHECK(describe(session.handle(typed(U'x', kStart + kMs))) == "edit:'x'");
	/* a UI já tirou o texto (Limpar, desfazer pela barra ou atalho): a sessão o cancela também */
	CHECK(describe(session.endTyping()) == "cancel");
	CHECK_FALSE(session.typing());
	CHECK(session.active());
	CHECK(session.handle(typed(U'y', kStart + 2 * kMs)).empty());
	CHECK(session.handle(at(DrawInputKind::Backspace, kStart + 3 * kMs)).empty());
	CHECK(session.handle(enter(kStart + 4 * kMs, false)).empty());
	CHECK(session.endTyping().empty()); // sem digitação: nada
	/* o próximo texto começa vazio (nada do buffer de antes) */
	CHECK(describe(session.handle(click(kStart + 5 * kMs, 600, 300, true))) == "textbegin(1000,500)");
	CHECK(describe(session.handle(typed(U'z', kStart + 6 * kMs))) == "edit:'z'");
	CHECK(describe(session.endTyping()) == "cancel");
	CHECK(describe(session.handle(at(DrawInputKind::Escape, kStart + 7 * kMs))) == "exit:esc");
	CHECK(session.endTyping().empty()); // depois de sair: nada

	/* durante um traço não mexe nele */
	DrawSession stroke(kMapping, kStart);
	CHECK(describe(stroke.handle(click(kStart, 200, 100, false))) == "begin(200,100)");
	CHECK(stroke.endTyping().empty());
	CHECK(stroke.drawing());
}

TEST_CASE("texto: clicar de novo confirma e começa outro; com outra ferramenta confirma e começa o traço")
{
	DrawSession session = typingSession();
	session.handle(typed(U'A', kStart + kMs));
	CHECK(describe(session.handle(click(kStart + 2 * kMs, 600, 300, true))) == "commit textbegin(1000,500)");
	CHECK(describe(session.handle(click(kStart + 3 * kMs, 700, 300, true))) == "cancel textbegin(1200,500)");
	session.handle(typed(U'B', kStart + 4 * kMs));
	CHECK(describe(session.handle(click(kStart + 5 * kMs, 700, 300, false))) == "commit begin(1200,500)");
	CHECK(session.drawing());
	CHECK_FALSE(session.typing());
	/* com o botão ainda apertado, outro clique (com a ferramenta texto) não começa nada */
	CHECK(session.handle(click(kStart + 6 * kMs, 710, 300, true)).empty());
	/* clique fora do monitor com a ferramenta texto: nada (o texto em andamento continua) */
	DrawSession outside = typingSession();
	outside.handle(typed(U'z', kStart + kMs));
	CHECK(outside.handle(click(kStart + 2 * kMs, 50, 20, true)).empty());
	CHECK(outside.typing());
}

TEST_CASE("texto: toda saída confirma antes (ou cancela se vazio), e o Exit vem por último")
{
	const DrawInputKind exits[] = {DrawInputKind::RightDown, DrawInputKind::Deactivated, DrawInputKind::CaptureLost,
				       DrawInputKind::Close, DrawInputKind::TargetChanged};
	const char *names[] = {"clique_direito", "foco", "foco", "fechar", "alvo"};
	for (int i = 0; i < 5; i++) {
		DrawSession session = typingSession();
		session.handle(typed(U'k', kStart + kMs));
		CHECK(describe(session.handle(at(exits[i], kStart + 2 * kMs))) ==
		      std::string("commit exit:") + names[i]);
	}
	DrawSession empty = typingSession();
	CHECK(describe(empty.requestExit(DrawExit::Hotkey)) == "cancel exit:atalho");
	DrawSession watchdog = typingSession();
	watchdog.handle(typed(U'k', kStart + kMs));
	CHECK(describe(watchdog.handle(tick(kStart + 2000 * kMs, kStart))) == "commit exit:vigia");
}

TEST_CASE("texto: digitar conta como atividade (sem saída por inatividade); controles e limites não editam")
{
	DrawSession session = typingSession();
	int64_t t = kStart;
	for (int i = 0; i < 4; i++) {
		t += 20000 * kMs; // 20 s entre teclas
		CHECK(describe(session.handle(typed(U'a', t))).rfind("edit:", 0) == 0);
		CHECK(session.handle(tick(t + 100 * kMs, t + 100 * kMs)).empty());
	}
	CHECK(session.active());
	CHECK(session.handle(typed(U'\b', t + kMs)).empty());
	CHECK(session.handle(typed(char32_t(0x1B), t + kMs)).empty());
	/* as teclas recusadas também contam como atividade (t + 1 ms) */
	const int64_t idle = t + kMs + voltz::kDrawIdleNs;
	CHECK(describe(session.handle(tick(idle, idle))) == "commit exit:inatividade");

	DrawSession full = typingSession();
	int64_t now = kStart;
	for (size_t i = 0; i < voltz::kMaxTextChars; i++) {
		now += kMs;
		full.handle(typed(U'x', now));
	}
	CHECK(full.handle(typed(U'y', now + kMs)).empty());
	CHECK(describe(full.handle(at(DrawInputKind::Backspace, now + 2 * kMs))).size() ==
	      std::string("edit:''").size() + voltz::kMaxTextChars - 1);
}

TEST_CASE("texto: comandos de traço continuam iguais sem a ferramenta texto")
{
	DrawSession session(kMapping, kStart);
	CHECK(describe(session.handle(click(kStart, 200, 100, false))) == "begin(200,100)");
	CHECK(session.handle(typed(U'a', kStart + kMs)).empty()); // digitar durante o traço não faz nada
	CHECK(describe(session.handle(at(DrawInputKind::LeftUp, kStart + 2 * kMs, 210, 100))) == "extend(220,100) end");
	CHECK(describe(session.handle(at(DrawInputKind::Escape, kStart + 3 * kMs))) == "exit:esc");
}
