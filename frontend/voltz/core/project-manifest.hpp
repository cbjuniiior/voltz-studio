#pragma once

#include "project-timeline.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace voltz {

constexpr int kManifestVersion = 1;
constexpr const char *kManifestFileName = "projeto.voltz";
constexpr const char *kProgramBaseName = "programa";
constexpr const char *kMarkersFileName = "marcadores.json";
constexpr const char *kCursorFileName = "cursor.jsonl";
constexpr const char *kScreenIsoFileName = "tela.mkv";
constexpr const char *kCameraIsoFileName = "camera.mkv";
// "faixa" dos eventos e chave em "arquivos"
constexpr const char *kScreenTrack = "tela";
constexpr const char *kCameraTrack = "camera";

enum class ProjectStatus { Recording, Ok, Recovered, Failed };

// "gravando" | "ok" | "recuperado" | "falhou" (spec §5.2). Texto desconhecido vira Failed.
const char *statusToString(ProjectStatus status);
ProjectStatus statusFromString(const std::string &text);

// arquivos.programa
struct ProgramFile {
	bool present = false;
	std::string file;
	int width = 0;
	int height = 0;
	double fps = 0.0;
	int64_t offsetMs = 0;
	int audioTracks = 0;
};

// Origem de uma captura: { "tipo": "monitor", "id": monitor_id, "retangulo": [x, y, l, a], "escala": 1.25 }
struct CaptureOrigin {
	std::string type;
	std::string id;
	int rect[4] = {0, 0, 0, 0};
	double scale = 1.0;
};

// arquivos.cursor (3A): coordenadas do cursor.jsonl em pixels da captura descrita em `origin`.
struct CursorFile {
	bool present = false;
	std::string file;
	int width = 0;
	int height = 0;
	CaptureOrigin origin;
};

// arquivos.tela / arquivos.camera (3B): ISO de uma fonte de papel Tela/Câmera (spec §5.3).
struct IsoFile {
	bool present = false;
	std::string file;
	int width = 0;
	int height = 0;
	double fps = 0.0;
	int64_t offsetMs = 0;   // primeiro quadro em relação ao t0_ns (spec §5.5)
	std::string encoder;    // "codificador": id do encoder no libobs (ex. "h265_texture_amf")
	bool hasOrigin = false; // só na tela, quando é captura de monitor
	CaptureOrigin origin;   // mesmo formato de arquivos.cursor.origem
};

// eventos[]: { "t_ms", "tipo", "faixa"?, "codigo"?, "motivo"? }
struct ManifestEvent {
	int64_t tMs = 0;
	std::string type;
	std::string track;
	int code = 0;
	std::string reason;
};

struct ProjectManifest {
	int version = kManifestVersion;
	std::string app;
	std::string engine;
	std::string name;
	std::string createdAt;
	ProjectStatus status = ProjectStatus::Recording;
	int64_t t0Ns = 0;
	int64_t durationMs = 0;
	std::vector<PauseSpan> pauses;
	ProgramFile program;
	IsoFile screen;
	IsoFile camera;
	CursorFile cursor;
	std::vector<ManifestEvent> events;
	// JSON lido do disco. Ao reescrever, os campos que esta versão não conhece (no nível de cima e dentro de cada
	// arquivos.*) voltam intactos; arquivo ausente no modelo sai de "arquivos".
	std::string preservedJson;
};

// JSON indentado (2 espaços), UTF-8, com "\n" no fim.
std::string serializeManifest(const ProjectManifest &manifest);

struct ManifestParse {
	bool ok = false;
	bool newerVersion = false;
	std::string error;
	ProjectManifest manifest;
};

// Leitura tolerante: campos ausentes ou de tipo errado ficam no padrão; só recusa JSON inválido, não-objeto ou
// "formato" diferente de "voltz-projeto". Versão maior que kManifestVersion é lida e sinalizada.
ManifestParse parseManifest(const std::string &json);

// Duração líquida do manifesto (duracao_ms menos as pausas).
int64_t manifestNetDurationMs(const ProjectManifest &manifest);

// Fecha um manifesto que ficou "gravando" (o app caiu): duração = maior entre a última pulsação (duracao_ms) e
// `fileElapsedMs`; pausa aberta termina no fim; status "recuperado"; evento "recuperado" no fim.
void recoverManifest(ProjectManifest &manifest, int64_t fileElapsedMs);

// Recuperação: tira do manifesto os ISO (tela/camera) cujo arquivo não serve (`usable` falso: não existe ou está
// vazio) e registra, para cada um, o evento iso_falhou (t_ms 0, faixa, motivo "arquivo_ausente"). Devolve quantos tirou.
int dropUnusableIsoFiles(ProjectManifest &manifest, const std::function<bool(const std::string &file)> &usable);

} // namespace voltz
