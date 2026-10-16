#include <doctest.h>

#include <voltz/core/project-timeline.hpp>

using voltz::PauseSpan;
using voltz::ProjectTimeline;

namespace {
constexpr int64_t ms(int64_t value)
{
	return value * 1000000;
}
} // namespace

TEST_CASE("ProjectTimeline parado marca zero")
{
	ProjectTimeline timeline;
	CHECK_FALSE(timeline.started());
	CHECK(timeline.relativeMs(ms(5000)) == 0);
	CHECK(timeline.grossMs(ms(5000)) == 0);
}

TEST_CASE("ProjectTimeline mede ms a partir do t0 e nunca negativo")
{
	ProjectTimeline timeline;
	timeline.start(ms(5000));
	CHECK(timeline.t0Ns() == ms(5000));
	CHECK(timeline.relativeMs(ms(6234)) == 1234);
	CHECK(timeline.relativeMs(ms(4000)) == 0);
}

TEST_CASE("ProjectTimeline registra a pausa e desconta só na duração líquida")
{
	ProjectTimeline timeline;
	timeline.start(0);
	timeline.pause(ms(61000));
	CHECK(timeline.paused());
	timeline.resume(ms(75000));
	REQUIRE(timeline.pauses().size() == 1);
	CHECK(timeline.pauses()[0].startMs == 61000);
	CHECK(timeline.pauses()[0].endMs == 75000);
	CHECK(timeline.grossMs(ms(100000)) == 100000);
	CHECK(timeline.netMs(ms(100000)) == 86000);
}

TEST_CASE("ProjectTimeline: stop fecha a pausa aberta e congela as durações")
{
	ProjectTimeline timeline;
	timeline.start(0);
	timeline.pause(ms(10000));
	timeline.stop(ms(15000));
	CHECK(timeline.stopped());
	CHECK_FALSE(timeline.paused());
	REQUIRE(timeline.pauses().size() == 1);
	CHECK(timeline.pauses()[0].endMs == 15000);
	CHECK(timeline.grossMs(ms(99999)) == 15000);
	CHECK(timeline.netMs(ms(99999)) == 10000);
}

TEST_CASE("ProjectTimeline ignora pausa dupla, retomada sem pausa e eventos depois do stop")
{
	ProjectTimeline timeline;
	timeline.pause(ms(1));
	timeline.start(0);
	timeline.resume(ms(1000));
	timeline.pause(ms(2000));
	timeline.pause(ms(3000));
	timeline.resume(ms(4000));
	timeline.stop(ms(5000));
	timeline.pause(ms(6000));
	REQUIRE(timeline.pauses().size() == 1);
	CHECK(timeline.pauses()[0].startMs == 2000);
	CHECK(timeline.pauses()[0].endMs == 4000);
	CHECK_FALSE(timeline.paused());
}

TEST_CASE("netDuration recorta pausas no intervalo e conta pausa aberta até o fim")
{
	CHECK(voltz::netDuration(10000, {PauseSpan{2000, -1}}) == 2000);
	CHECK(voltz::netDuration(10000, {PauseSpan{-500, 1000}, PauseSpan{9000, 20000}}) == 8000);
	CHECK(voltz::netDuration(0, {PauseSpan{0, 5}}) == 0);
	CHECK(voltz::netDuration(10000, {}) == 10000);
}
