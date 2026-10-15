#include <doctest.h>

#include <voltz/core/time-format.hpp>

TEST_CASE("formatClock formata HH:MM:SS")
{
	CHECK(voltz::formatClock(0) == "00:00:00");
	CHECK(voltz::formatClock(999) == "00:00:00");
	CHECK(voltz::formatClock(65000) == "00:01:05");
	CHECK(voltz::formatClock(3725000) == "01:02:05");
}

TEST_CASE("formatClock aceita mais de 99 horas e trata negativos como zero")
{
	CHECK(voltz::formatClock(int64_t(100) * 3600 * 1000) == "100:00:00");
	CHECK(voltz::formatClock(-5000) == "00:00:00");
}

TEST_CASE("formatClock nas viradas de hora")
{
	CHECK(voltz::formatClock(3599999) == "00:59:59");
	CHECK(voltz::formatClock(3600000) == "01:00:00");
}
