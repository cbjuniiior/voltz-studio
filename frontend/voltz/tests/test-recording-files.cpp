#include <doctest.h>

#include <voltz/core/recording-files.hpp>

TEST_CASE("isRecordingFile reconhece extensões de vídeo sem diferenciar maiúsculas")
{
	CHECK(voltz::isRecordingFile("2026-10-15 15-30-00.mp4"));
	CHECK(voltz::isRecordingFile("aula.MKV"));
	CHECK(voltz::isRecordingFile("x.mov"));
	CHECK(voltz::isRecordingFile("x.flv"));
	CHECK(voltz::isRecordingFile("x.ts"));
	CHECK(voltz::isRecordingFile("x.avi"));
	CHECK_FALSE(voltz::isRecordingFile("notas.txt"));
	CHECK_FALSE(voltz::isRecordingFile("x.mp4.part"));
	CHECK_FALSE(voltz::isRecordingFile("semextensao"));
	CHECK_FALSE(voltz::isRecordingFile(".mp4"));
}

TEST_CASE("formatFileSize usa base 1024, uma casa decimal e o separador pedido")
{
	CHECK(voltz::formatFileSize(0, ',') == "0 B");
	CHECK(voltz::formatFileSize(1023, ',') == "1023 B");
	CHECK(voltz::formatFileSize(1024, ',') == "1,0 KB");
	CHECK(voltz::formatFileSize(1536, ',') == "1,5 KB");
	CHECK(voltz::formatFileSize(5ULL * 1024 * 1024, ',') == "5,0 MB");
	CHECK(voltz::formatFileSize(1288490188ULL, ',') == "1,2 GB");
	CHECK(voltz::formatFileSize(1288490188ULL, '.') == "1.2 GB");
	CHECK(voltz::formatFileSize(2ULL * 1024 * 1024 * 1024 * 1024, ',') == "2048,0 GB");
}
