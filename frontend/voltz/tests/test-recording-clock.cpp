#include <doctest.h>

#include <voltz/core/recording-clock.hpp>

TEST_CASE("RecordingClock parado marca zero")
{
	voltz::RecordingClock clock;
	CHECK_FALSE(clock.running());
	CHECK(clock.elapsedMs(5000) == 0);
}

TEST_CASE("RecordingClock conta a partir do start")
{
	voltz::RecordingClock clock;
	clock.start(1000);
	CHECK(clock.running());
	CHECK(clock.elapsedMs(4500) == 3500);
}

TEST_CASE("RecordingClock congela durante a pausa e desconta ao retomar")
{
	voltz::RecordingClock clock;
	clock.start(0);
	clock.pause(10000);
	CHECK(clock.paused());
	CHECK(clock.elapsedMs(15000) == 10000);
	clock.resume(16000);
	CHECK_FALSE(clock.paused());
	CHECK(clock.elapsedMs(20000) == 14000);
}

TEST_CASE("RecordingClock ignora pausa dupla e retomada sem pausa")
{
	voltz::RecordingClock clock;
	clock.start(0);
	clock.resume(1000);
	clock.pause(2000);
	clock.pause(3000);
	clock.resume(5000);
	CHECK(clock.elapsedMs(6000) == 3000);
}

TEST_CASE("RecordingClock: stop zera e start reinicia")
{
	voltz::RecordingClock clock;
	clock.start(0);
	clock.pause(1000);
	clock.stop();
	CHECK_FALSE(clock.running());
	CHECK_FALSE(clock.paused());
	CHECK(clock.elapsedMs(9000) == 0);
	clock.start(10000);
	CHECK(clock.elapsedMs(10500) == 500);
}

TEST_CASE("RecordingClock nunca retorna negativo")
{
	voltz::RecordingClock clock;
	clock.start(5000);
	CHECK(clock.elapsedMs(1000) == 0);
}
