#include <doctest.h>

#include <voltz/core/disk-space.hpp>

using voltz::DiskLevel;
using voltz::kGiB;

TEST_CASE("diskLevel: abaixo de 5 GB avisa, abaixo de 1 GB para")
{
	CHECK(voltz::diskLevel(6 * kGiB) == DiskLevel::Ok);
	CHECK(voltz::diskLevel(5 * kGiB) == DiskLevel::Ok);
	CHECK(voltz::diskLevel(5 * kGiB - 1) == DiskLevel::Low);
	CHECK(voltz::diskLevel(kGiB) == DiskLevel::Low);
	CHECK(voltz::diskLevel(kGiB - 1) == DiskLevel::Critical);
	CHECK(voltz::diskLevel(0) == DiskLevel::Critical);
}

TEST_CASE("estimateBytesPerMinute: 1080p60 por qualidade e com o codificador da transmissão")
{
	voltz::RateInput high;
	high.width = 1920;
	high.height = 1080;
	high.fps = 60.0;
	high.audioTracks = 3;
	CHECK(voltz::estimateBytesPerMinute(high) == 116294400ULL);

	voltz::RateInput compact = high;
	compact.compact = true;
	CHECK(voltz::estimateBytesPerMinute(compact) == 50976000ULL);

	voltz::RateInput stream = high;
	stream.streamEncoder = true;
	stream.streamKbps = 2500;
	stream.audioTracks = 1;
	CHECK(voltz::estimateBytesPerMinute(stream) == 20190000ULL);
}

TEST_CASE("measuredBytesPerMinute só mede depois de 10 s")
{
	CHECK(voltz::measuredBytesPerMinute(10 * 1024 * 1024, 9999) == 0);
	CHECK(voltz::measuredBytesPerMinute(60000000ULL, 30000) == 120000000ULL);
}

TEST_CASE("formatRate: KB/min abaixo de 0,5 MB, MB/min inteiro abaixo de 1 GB, GB/min com uma casa acima")
{
	CHECK(voltz::formatRate(0, ',') == "~0 KB/min");
	CHECK(voltz::formatRate(307200ULL, ',') == "~300 KB/min");
	CHECK(voltz::formatRate(524287ULL, ',') == "~512 KB/min");
	CHECK(voltz::formatRate(524288ULL, ',') == "~1 MB/min");
	CHECK(voltz::formatRate(116294400ULL, ',') == "~111 MB/min");
	CHECK(voltz::formatRate(50976000ULL, ',') == "~49 MB/min");
	CHECK(voltz::formatRate(kGiB, ',') == "~1,0 GB/min");
	CHECK(voltz::formatRate(1288490188ULL, ',') == "~1,2 GB/min");
	CHECK(voltz::formatRate(1288490188ULL, '.') == "~1.2 GB/min");
}

TEST_CASE("formatFreeSpace arredonda para baixo (nunca promete espaço que não há)")
{
	CHECK(voltz::formatFreeSpace(45848774885ULL, ',') == "42 GB");
	CHECK(voltz::formatFreeSpace(10 * kGiB, ',') == "10 GB");
	CHECK(voltz::formatFreeSpace(4563402752ULL, ',') == "4,2 GB");
	CHECK(voltz::formatFreeSpace(891289600ULL, ',') == "850 MB");
	CHECK(voltz::formatFreeSpace(0, ',') == "0 MB");
}
