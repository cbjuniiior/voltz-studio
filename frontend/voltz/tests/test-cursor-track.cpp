#include <doctest.h>

#include <voltz/core/cursor-track.hpp>

#include <vector>

using voltz::CaptureMapping;
using voltz::CapturePoint;
using voltz::CursorEvent;
using voltz::CursorEventType;
using voltz::CursorSampler;
using voltz::MonitorRect;
using voltz::MouseButton;
using voltz::RawMouseEvent;
using voltz::RawMouseKind;
using voltz::cursorLine;
using voltz::mapToCapture;

namespace {

constexpr int64_t kMs = 1000000; // ns por ms

CaptureMapping identity(int left, int top, int width, int height)
{
	return CaptureMapping{MonitorRect{left, top, width, height}, width, height};
}

RawMouseEvent move(int64_t ms, int x, int y)
{
	RawMouseEvent event;
	event.tNs = ms * kMs;
	event.kind = RawMouseKind::Move;
	event.x = x;
	event.y = y;
	return event;
}

RawMouseEvent button(int64_t ms, RawMouseKind kind, MouseButton which, int x, int y)
{
	RawMouseEvent event = move(ms, x, y);
	event.kind = kind;
	event.button = which;
	return event;
}

RawMouseEvent wheel(int64_t ms, int x, int y, int delta)
{
	RawMouseEvent event = move(ms, x, y);
	event.kind = RawMouseKind::Wheel;
	event.wheelDelta = delta;
	return event;
}

} // namespace

TEST_CASE("mapToCapture: monitor único mapeia 1:1 e fora da área é 'fora'")
{
	const CaptureMapping m = identity(0, 0, 1920, 1080);
	CapturePoint p = mapToCapture(m, 0, 0);
	CHECK(p.inside);
	CHECK(p.x == 0);
	CHECK(p.y == 0);
	p = mapToCapture(m, 1919, 1079);
	CHECK(p.inside);
	CHECK(p.x == 1919);
	CHECK(p.y == 1079);
	CHECK_FALSE(mapToCapture(m, 1920, 5).inside);
	CHECK_FALSE(mapToCapture(m, -1, 5).inside);
	CHECK_FALSE(mapToCapture(m, 5, 1080).inside);
}

TEST_CASE("mapToCapture: monitor à esquerda do principal (origem negativa)")
{
	const CaptureMapping m = identity(-2560, 0, 2560, 1440);
	CapturePoint p = mapToCapture(m, -2560, 0);
	CHECK(p.inside);
	CHECK(p.x == 0);
	CHECK(p.y == 0);
	p = mapToCapture(m, -1, 1439);
	CHECK(p.inside);
	CHECK(p.x == 2559);
	CHECK(p.y == 1439);
	CHECK_FALSE(mapToCapture(m, 0, 0).inside);
}

TEST_CASE("mapToCapture: monitor deslocado para cima (y negativo)")
{
	const CaptureMapping m = identity(1920, -300, 1280, 1024);
	CapturePoint p = mapToCapture(m, 1920, -300);
	CHECK(p.inside);
	CHECK(p.x == 0);
	CHECK(p.y == 0);
	p = mapToCapture(m, 2000, 700);
	CHECK(p.inside);
	CHECK(p.x == 80);
	CHECK(p.y == 1000);
	CHECK_FALSE(mapToCapture(m, 2000, 724).inside);
}

TEST_CASE("mapToCapture: DPI 125% e 150% - gancho e retângulo em pixels físicos (PMv2) mapeiam 1:1")
{
	// 125%: 2560x1440 físicos (2048x1152 lógicos). 150% à esquerda: 2880x1620 físicos.
	const CaptureMapping m125 = identity(0, 0, 2560, 1440);
	CapturePoint p = mapToCapture(m125, 2559, 1439);
	CHECK(p.inside);
	CHECK(p.x == 2559);
	CHECK(p.y == 1439);
	const CaptureMapping m150 = identity(-2880, 0, 2880, 1620);
	p = mapToCapture(m150, -1440, 810);
	CHECK(p.inside);
	CHECK(p.x == 1440);
	CHECK(p.y == 810);
}

TEST_CASE("mapToCapture: captura em resolução menor escala as coordenadas")
{
	const CaptureMapping m{MonitorRect{0, 0, 2560, 1440}, 1920, 1080};
	CapturePoint p = mapToCapture(m, 2559, 1439);
	CHECK(p.x == 1919);
	CHECK(p.y == 1079);
	p = mapToCapture(m, 1280, 720);
	CHECK(p.x == 960);
	CHECK(p.y == 540);
}

TEST_CASE("cursorLine segue o formato do cursor.jsonl (spec §5.6)")
{
	CHECK(cursorLine(CursorEvent{1234, CursorEventType::Move, 812, 440}) ==
	      R"({"t":1234,"x":812,"y":440,"e":"move"})");
	CHECK(cursorLine(CursorEvent{5, CursorEventType::Down, 1, 2, MouseButton::Left}) ==
	      R"({"t":5,"x":1,"y":2,"e":"down","b":"l"})");
	CHECK(cursorLine(CursorEvent{6, CursorEventType::Up, 1, 2, MouseButton::Right}) ==
	      R"({"t":6,"x":1,"y":2,"e":"up","b":"r"})");
	CHECK(cursorLine(CursorEvent{7, CursorEventType::Down, 1, 2, MouseButton::Middle}) ==
	      R"({"t":7,"x":1,"y":2,"e":"down","b":"m"})");
	CHECK(cursorLine(CursorEvent{8, CursorEventType::Scroll, 3, 4, MouseButton::None, 120}) ==
	      R"({"t":8,"x":3,"y":4,"e":"scroll","d":1})");
	CHECK(cursorLine(CursorEvent{8, CursorEventType::Scroll, 3, 4, MouseButton::None, -240}) ==
	      R"({"t":8,"x":3,"y":4,"e":"scroll","d":-2})");
	CHECK(cursorLine(CursorEvent{8, CursorEventType::Scroll, 3, 4, MouseButton::None, 60}) ==
	      R"({"t":8,"x":3,"y":4,"e":"scroll","d":0.5})");
	CHECK(cursorLine(CursorEvent{8, CursorEventType::Scroll, 3, 4, MouseButton::None, -8}) ==
	      R"({"t":8,"x":3,"y":4,"e":"scroll","d":-0.067})");
	CHECK(cursorLine(CursorEvent{9, CursorEventType::Outside}) == R"({"t":9,"e":"fora"})");
}

TEST_CASE("CursorSampler::initial registra a posição inicial (ou 'fora')")
{
	CursorSampler inside(identity(0, 0, 1920, 1080), 1000 * kMs);
	std::vector<CursorEvent> events = inside.initial(1500 * kMs, 100, 200);
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == CursorEventType::Move);
	CHECK(events[0].tMs == 500);
	CHECK(events[0].x == 100);
	CHECK(events[0].y == 200);

	CursorSampler outside(identity(0, 0, 1920, 1080), 0);
	events = outside.initial(0, 5000, 5000);
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == CursorEventType::Outside);
}

TEST_CASE("CursorSampler junta os movimentos de um tique no último")
{
	CursorSampler sampler(identity(0, 0, 1920, 1080), 0);
	const std::vector<CursorEvent> events = sampler.process({move(1, 10, 10), move(5, 20, 20), move(9, 30, 30)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].tMs == 9);
	CHECK(events[0].x == 30);
	CHECK(events[0].y == 30);
}

TEST_CASE("CursorSampler: clique e rolagem saem em ordem, com o movimento pendente antes")
{
	CursorSampler sampler(identity(0, 0, 1920, 1080), 0);
	std::vector<CursorEvent> events =
		sampler.process({move(1, 100, 100), button(2, RawMouseKind::Down, MouseButton::Left, 100, 100),
				 button(3, RawMouseKind::Up, MouseButton::Left, 100, 100), move(4, 110, 100)});
	REQUIRE(events.size() == 4);
	CHECK(events[0].type == CursorEventType::Move);
	CHECK(events[1].type == CursorEventType::Down);
	CHECK(events[1].button == MouseButton::Left);
	CHECK(events[1].tMs == 2);
	CHECK(events[2].type == CursorEventType::Up);
	CHECK(events[3].type == CursorEventType::Move);
	CHECK(events[3].x == 110);

	events = sampler.process({wheel(10, 110, 100, -120)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == CursorEventType::Scroll);
	CHECK(events[0].wheelDelta == -120);
}

TEST_CASE("CursorSampler: parado não repete; sair gera um 'fora'; voltar registra de novo")
{
	CursorSampler sampler(identity(0, 0, 1920, 1080), 0);
	CHECK(sampler.process({move(1, 50, 50)}).size() == 1);
	CHECK(sampler.process({move(2, 50, 50)}).empty());
	std::vector<CursorEvent> events = sampler.process(
		{move(3, 5000, 50), move(4, 6000, 50), button(5, RawMouseKind::Down, MouseButton::Left, 6000, 50)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == CursorEventType::Outside);
	CHECK(events[0].tMs == 3);
	events = sampler.process({move(6, 50, 50)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == CursorEventType::Move);
}

TEST_CASE("CursorSampler: tempos em ms relativos ao t0, nunca negativos")
{
	CursorSampler sampler(identity(0, 0, 1920, 1080), 10000 * kMs);
	std::vector<CursorEvent> events = sampler.process({move(10500, 1, 1)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].tMs == 500);
	events = sampler.process({move(9000, 2, 2)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].tMs == 0);
}
