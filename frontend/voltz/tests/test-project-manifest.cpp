#include <doctest.h>

#include <voltz/core/project-manifest.hpp>

#include <string>

using voltz::IsoFile;
using voltz::ManifestEvent;
using voltz::PauseSpan;
using voltz::ProjectManifest;
using voltz::ProjectStatus;

namespace {

ProjectManifest sample()
{
	ProjectManifest m;
	m.app = "Voltz Studio 1.0.0";
	m.engine = "libobs 32.2.2";
	m.name = "Aula 03 · Revisão";
	m.createdAt = "2026-10-15T15:30:12-03:00";
	m.status = ProjectStatus::Recording;
	m.t0Ns = 123456789000;
	m.durationMs = 90000;
	m.pauses = {PauseSpan{61000, 75000}, PauseSpan{80000, -1}};
	m.program.present = true;
	m.program.file = "programa.mp4";
	m.program.width = 1920;
	m.program.height = 1080;
	m.program.fps = 60;
	m.program.audioTracks = 3;
	m.cursor.present = true;
	m.cursor.file = "cursor.jsonl";
	m.cursor.width = 2560;
	m.cursor.height = 1440;
	m.cursor.origin.type = "monitor";
	m.cursor.origin.id = "\\\\?\\DISPLAY#ABC";
	m.cursor.origin.rect[0] = -2560;
	m.cursor.origin.rect[1] = 0;
	m.cursor.origin.rect[2] = 2560;
	m.cursor.origin.rect[3] = 1440;
	m.cursor.origin.scale = 1.25;
	m.events = {ManifestEvent{90500, "camera_desconectada", "", 0}, ManifestEvent{1200, "erro_gravacao", "", -4}};
	return m;
}

bool contains(const std::string &text, const std::string &part)
{
	return text.find(part) != std::string::npos;
}

size_t count(const std::string &text, const std::string &part)
{
	size_t found = 0;
	for (size_t pos = text.find(part); pos != std::string::npos; pos = text.find(part, pos + part.size())) {
		found++;
	}
	return found;
}

ProjectManifest withIso()
{
	ProjectManifest m = sample();
	m.screen.present = true;
	m.screen.file = "tela.mkv";
	m.screen.width = 2560;
	m.screen.height = 1440;
	m.screen.fps = 60;
	m.screen.offsetMs = 3;
	m.screen.encoder = "h265_texture_amf";
	m.screen.hasOrigin = true;
	m.screen.origin.type = "monitor";
	m.screen.origin.id = "\\\\.\\DISPLAY1";
	m.screen.origin.rect[0] = 0;
	m.screen.origin.rect[1] = 0;
	m.screen.origin.rect[2] = 2560;
	m.screen.origin.rect[3] = 1440;
	m.screen.origin.scale = 1.25;
	m.camera.present = true;
	m.camera.file = "camera.mkv";
	m.camera.width = 1920;
	m.camera.height = 1080;
	m.camera.fps = 60;
	m.camera.offsetMs = 5;
	m.camera.encoder = "h265_texture_amf";
	return m;
}

} // namespace

TEST_CASE("serializeManifest escreve o formato v1 da spec, na ordem da spec")
{
	const std::string json = voltz::serializeManifest(sample());
	CHECK(contains(json, "\"formato\": \"voltz-projeto\""));
	CHECK(contains(json, "\"versao\": 1"));
	CHECK(contains(json, "\"nome\": \"Aula 03 · Revisão\""));
	CHECK(contains(json, "\"status\": \"gravando\""));
	CHECK(contains(json, "\"relogio\": \"obs_ns\""));
	CHECK(contains(json, "\"t0_ns\": 123456789000"));
	CHECK(contains(json, "\"duracao_liquida_ms\": 66000"));
	CHECK(contains(json, "\"arquivo\": \"programa.mp4\""));
	CHECK(contains(json, "\"fps\": 60,"));
	CHECK(contains(json, "\"faixas_audio\": 3"));
	CHECK(contains(json, "\"fim_ms\": null"));
	CHECK(contains(json, "\"escala\": 1.25"));
	CHECK(contains(json, "\"codigo\": -4"));
	CHECK_FALSE(contains(json, "\"tela\""));
	CHECK_FALSE(contains(json, "\"camera\""));
	CHECK(json.find("\"formato\"") < json.find("\"versao\""));
	CHECK(json.find("\"versao\"") < json.find("\"arquivos\""));
	CHECK(json.find("\"arquivos\"") < json.find("\"eventos\""));
}

TEST_CASE("parseManifest lê de volta tudo o que serializeManifest escreveu")
{
	const ProjectManifest original = sample();
	const voltz::ManifestParse parsed = voltz::parseManifest(voltz::serializeManifest(original));
	REQUIRE(parsed.ok);
	CHECK_FALSE(parsed.newerVersion);
	const ProjectManifest &m = parsed.manifest;
	CHECK(m.version == 1);
	CHECK(m.app == original.app);
	CHECK(m.engine == original.engine);
	CHECK(m.name == original.name);
	CHECK(m.createdAt == original.createdAt);
	CHECK(m.status == ProjectStatus::Recording);
	CHECK(m.t0Ns == original.t0Ns);
	CHECK(m.durationMs == 90000);
	REQUIRE(m.pauses.size() == 2);
	CHECK(m.pauses[0].startMs == 61000);
	CHECK(m.pauses[0].endMs == 75000);
	CHECK(m.pauses[1].endMs == -1);
	CHECK(m.program.present);
	CHECK(m.program.file == "programa.mp4");
	CHECK(m.program.width == 1920);
	CHECK(m.program.height == 1080);
	CHECK(m.program.fps == doctest::Approx(60.0));
	CHECK(m.program.audioTracks == 3);
	CHECK(m.cursor.present);
	CHECK(m.cursor.width == 2560);
	CHECK(m.cursor.origin.type == "monitor");
	CHECK(m.cursor.origin.id == original.cursor.origin.id);
	CHECK(m.cursor.origin.rect[0] == -2560);
	CHECK(m.cursor.origin.rect[3] == 1440);
	CHECK(m.cursor.origin.scale == doctest::Approx(1.25));
	REQUIRE(m.events.size() == 2);
	CHECK(m.events[0].type == "camera_desconectada");
	CHECK(m.events[0].tMs == 90500);
	CHECK(m.events[1].code == -4);
}

TEST_CASE("fps fracionário sai com até 3 casas e volta igual")
{
	ProjectManifest m = sample();
	m.program.fps = 29.97002997;
	const std::string json = voltz::serializeManifest(m);
	CHECK(contains(json, "\"fps\": 29.97,"));
	const voltz::ManifestParse parsed = voltz::parseManifest(json);
	REQUIRE(parsed.ok);
	CHECK(parsed.manifest.program.fps == doctest::Approx(29.97));
}

TEST_CASE("parseManifest tolera campos ausentes e tipos errados")
{
	const voltz::ManifestParse parsed = voltz::parseManifest(
		R"({"formato":"voltz-projeto","nome":"X","duracao_ms":"muito","pausas":[{"inicio_ms":5},3],"arquivos":{"programa":"?"}})");
	REQUIRE(parsed.ok);
	CHECK(parsed.manifest.version == 1);
	CHECK(parsed.manifest.name == "X");
	CHECK(parsed.manifest.durationMs == 0);
	REQUIRE(parsed.manifest.pauses.size() == 1);
	CHECK(parsed.manifest.pauses[0].startMs == 5);
	CHECK(parsed.manifest.pauses[0].endMs == -1);
	CHECK_FALSE(parsed.manifest.program.present);
	CHECK_FALSE(parsed.manifest.cursor.present);
	CHECK(parsed.manifest.status == ProjectStatus::Failed);
}

TEST_CASE("parseManifest rejeita JSON inválido e outro formato")
{
	CHECK_FALSE(voltz::parseManifest("").ok);
	CHECK_FALSE(voltz::parseManifest("{").ok);
	CHECK_FALSE(voltz::parseManifest("[]").ok);
	CHECK_FALSE(voltz::parseManifest(R"({"formato":"outro","versao":1})").ok);
}

TEST_CASE("versão mais nova é lida e campos desconhecidos sobrevivem à reescrita")
{
	const std::string future = R"({"formato":"voltz-projeto","versao":2,"nome":"Antigo","status":"ok",
		"arquivos":{"programa":{"arquivo":"programa.mp4"},"tela":{"arquivo":"tela.mkv","largura":2560}},
		"mouse":{"halo":{"ativo":true}},"novidade":[1,2,3]})";
	voltz::ManifestParse parsed = voltz::parseManifest(future);
	REQUIRE(parsed.ok);
	CHECK(parsed.newerVersion);
	CHECK(parsed.manifest.version == 2);
	CHECK(parsed.manifest.status == ProjectStatus::Ok);
	parsed.manifest.name = "Novo";
	const std::string rewritten = voltz::serializeManifest(parsed.manifest);
	CHECK(contains(rewritten, "\"nome\": \"Novo\""));
	CHECK(contains(rewritten, "\"tela.mkv\""));
	CHECK(contains(rewritten, "\"halo\""));
	CHECK(contains(rewritten, "\"novidade\""));
	CHECK(contains(rewritten, "\"versao\": 2"));
}

TEST_CASE("status usa os textos da spec; desconhecido vira falhou")
{
	CHECK(std::string(voltz::statusToString(ProjectStatus::Recording)) == "gravando");
	CHECK(std::string(voltz::statusToString(ProjectStatus::Ok)) == "ok");
	CHECK(std::string(voltz::statusToString(ProjectStatus::Recovered)) == "recuperado");
	CHECK(std::string(voltz::statusToString(ProjectStatus::Failed)) == "falhou");
	CHECK(voltz::statusFromString("gravando") == ProjectStatus::Recording);
	CHECK(voltz::statusFromString("recuperado") == ProjectStatus::Recovered);
	CHECK(voltz::statusFromString("xyz") == ProjectStatus::Failed);
}

TEST_CASE("recoverManifest usa o maior entre pulsação e arquivo e fecha a pausa aberta")
{
	ProjectManifest m;
	m.status = ProjectStatus::Recording;
	m.durationMs = 30000;
	m.pauses = {PauseSpan{20000, -1}};
	voltz::recoverManifest(m, 42000);
	CHECK(m.status == ProjectStatus::Recovered);
	CHECK(m.durationMs == 42000);
	CHECK(m.pauses[0].endMs == 42000);
	REQUIRE_FALSE(m.events.empty());
	CHECK(m.events.back().type == "recuperado");
	CHECK(m.events.back().tMs == 42000);
	CHECK(voltz::manifestNetDurationMs(m) == 20000);

	ProjectManifest heartbeatOnly;
	heartbeatOnly.durationMs = 5000;
	voltz::recoverManifest(heartbeatOnly, -10);
	CHECK(heartbeatOnly.durationMs == 5000);
}

TEST_CASE("tela e câmera saem com arquivo, tamanho, fps, offset, codificador e origem só na tela")
{
	const std::string json = voltz::serializeManifest(withIso());
	CHECK(contains(json, "\"arquivo\": \"tela.mkv\""));
	CHECK(contains(json, "\"arquivo\": \"camera.mkv\""));
	CHECK(contains(json, "\"offset_ms\": 3"));
	CHECK(contains(json, "\"offset_ms\": 5"));
	CHECK(contains(json, "\"codificador\": \"h265_texture_amf\""));
	CHECK(count(json, "\"origem\"") == 2); // tela + cursor; a câmera não tem
	CHECK(json.find("\"programa\"") < json.find("\"tela\""));
	CHECK(json.find("\"tela\"") < json.find("\"camera\""));
	CHECK(json.find("\"camera\"") < json.find("\"cursor\""));
}

TEST_CASE("parseManifest lê tela e câmera de volta")
{
	const voltz::ManifestParse parsed = voltz::parseManifest(voltz::serializeManifest(withIso()));
	REQUIRE(parsed.ok);
	const IsoFile &screen = parsed.manifest.screen;
	CHECK(screen.present);
	CHECK(screen.file == "tela.mkv");
	CHECK(screen.width == 2560);
	CHECK(screen.height == 1440);
	CHECK(screen.fps == doctest::Approx(60.0));
	CHECK(screen.offsetMs == 3);
	CHECK(screen.encoder == "h265_texture_amf");
	CHECK(screen.hasOrigin);
	CHECK(screen.origin.type == "monitor");
	CHECK(screen.origin.id == "\\\\.\\DISPLAY1");
	CHECK(screen.origin.rect[2] == 2560);
	CHECK(screen.origin.scale == doctest::Approx(1.25));
	const IsoFile &camera = parsed.manifest.camera;
	CHECK(camera.present);
	CHECK(camera.offsetMs == 5);
	CHECK_FALSE(camera.hasOrigin);
}

TEST_CASE("evento com motivo vai e volta; sem motivo não escreve a chave")
{
	ProjectManifest m = sample();
	m.events.push_back(ManifestEvent{0, "iso_falhou", "camera", 0, "sem_imagem"});
	const std::string json = voltz::serializeManifest(m);
	CHECK(contains(json, "\"faixa\": \"camera\""));
	CHECK(contains(json, "\"motivo\": \"sem_imagem\""));
	CHECK(count(json, "\"motivo\"") == 1);
	const voltz::ManifestParse parsed = voltz::parseManifest(json);
	REQUIRE(parsed.ok);
	REQUIRE(parsed.manifest.events.size() == 3);
	CHECK(parsed.manifest.events[2].track == "camera");
	CHECK(parsed.manifest.events[2].reason == "sem_imagem");
	CHECK(parsed.manifest.events[0].reason.empty());
}

TEST_CASE("campos desconhecidos dentro de arquivos.* sobrevivem; arquivo que saiu do modelo some do JSON")
{
	voltz::ManifestParse parsed = voltz::parseManifest(R"({"formato":"voltz-projeto","versao":1,"nome":"X",
		"status":"ok","arquivos":{"programa":{"arquivo":"programa.mp4","hdr":true},
		"tela":{"arquivo":"tela.mkv","largura":2560,"nota":"x"},"camera":{"arquivo":"camera.mkv"}}})");
	REQUIRE(parsed.ok);
	CHECK(parsed.manifest.screen.present);
	CHECK(parsed.manifest.camera.present);
	parsed.manifest.name = "Y";
	parsed.manifest.camera.present = false;
	const std::string rewritten = voltz::serializeManifest(parsed.manifest);
	CHECK(contains(rewritten, "\"nome\": \"Y\""));
	CHECK(contains(rewritten, "\"hdr\": true"));
	CHECK(contains(rewritten, "\"nota\": \"x\""));
	CHECK(contains(rewritten, "\"tela.mkv\""));
	CHECK_FALSE(contains(rewritten, "camera.mkv"));
}

TEST_CASE("dropUnusableIsoFiles tira o ISO sem arquivo e registra iso_falhou com motivo")
{
	ProjectManifest m = withIso();
	m.events.clear();
	const int dropped =
		voltz::dropUnusableIsoFiles(m, [](const std::string &file) { return file == "camera.mkv"; });
	CHECK(dropped == 1);
	CHECK_FALSE(m.screen.present);
	CHECK(m.camera.present);
	REQUIRE(m.events.size() == 1);
	CHECK(m.events[0].type == "iso_falhou");
	CHECK(m.events[0].track == "tela");
	CHECK(m.events[0].reason == "arquivo_ausente");
	CHECK(m.events[0].tMs == 0);
	CHECK(voltz::dropUnusableIsoFiles(m, [](const std::string &) { return true; }) == 0);
}
