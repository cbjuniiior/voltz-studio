# Voltz Studio — Plano 2B: Presets, Projetos e Ajustes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Substituir as três telas provisórias da casca por telas reais: **Presets** (4 presets que criam 5 cenas
de fábrica, oferecidos na primeira execução), **Projetos** (gravações da pasta de saída: abrir, mostrar na pasta,
mover para a lixeira) e **Ajustes** (pasta/formato/qualidade da gravação com codificador automático, monitor,
câmera e microfone, sobre/modo clássico/logs).

**Architecture:** Lógica pura nova em `frontend/voltz/core/` com testes (geometria dos presets, arquivos de
gravação, mapeamento de configurações). Na casca, `PresetBuilder` monta fontes/cenas pela API do libobs
(o `OBSBasic` adota cenas novas sozinho via sinal `source_create`), `RecordingConfig` lê/escreve o perfil do OBS
(`OBSBasic::Get()->Config()`, modo de saída Simples) e as páginas só exibem/comandam. Fontes compartilhadas
(Tela, Câmera, Fundo) são identificadas por UUID salvo na configuração do usuário (seção `Voltz`).

**Tech Stack:** C++17, Qt 6.11, libobs 32.2.2, frontend API, doctest. Teste de UI por **UI Automation** do
Windows (PowerShell) e obs-websocket (`tools/voltz-ws.ps1`).

**Spec:** `docs/superpowers/specs/2026-10-15-voltz-studio-base-gravacao-design.md` §4.3–4.4 ·
**Backlog:** `docs/voltz/backlog-plano-2.md`. **Fica para o 2C:** tema `.ovt` do modo clássico, bandeja,
confirmação de saída própria, diálogo de falha, acabamento visual do Áudio, grupos no painel de Fontes.

**Desvio consciente da spec §4.4:** o grupo "Tela Voltz" (captura + anotações + mouse) só nasce no Plano 4, quando
existirem as fontes de anotação/mouse. Aqui a fonte "Tela" entra direto nas cenas. A câmera circular do Tutorial
usa um **grupo** ("Câmera redonda") com filtro de máscara — assim só ela fica redonda (filtros são por fonte; o
grupo se redimensiona sozinho quando a câmera liga — `obs-scene.c` `source_size_changed` → `resize_group`).

---

## Convenções

**Diretório de trabalho:** a raiz do repositório (`voltz-studio`), branch `voltz`. Leia o `CLAUDE.md` antes.

```bash
CMAKE="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
CTEST="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe"
```

- **CONFIGURAR:** `"$CMAKE" --preset voltz-windows-x64`
- **COMPILAR APP:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target obs-studio --parallel -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
- **COMPILAR TUDO:** `"$CMAKE" --build --preset voltz-windows-x64 --parallel -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
- **COMPILAR TESTES:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target voltz-core-tests -- -v:minimal -nologo -nodeReuse:false`
- **RODAR TESTES:** `"$CTEST" --test-dir build_x64/frontend/voltz/tests -C RelWithDebInfo --output-on-failure`
- **SMOKE:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1`

> **Regras fixas:** builds longos em segundo plano, nunca dois ao mesmo tempo (`MSB4018`/`ZERO_CHECK` no
> `build_x86` → rode de novo); arquivos novos exigem CONFIGURAR. **Nunca** encerre processos que você não iniciou
> (só pelo PID que lançou; nunca `taskkill /IM`). **Nunca** capture a tela inteira — só o retângulo da janela do app
> (PrintWindow). `clang-format -i` (VS Build Tools `VC/Tools/Llvm/bin/clang-format.exe`) nos `.cpp/.hpp` novos ou
> alterados em `frontend/voltz/`. `QTStr` vem de `<OBSApp.hpp>`. Toda alteração em C/C++ do OBS fica em
> `#ifdef VOLTZ_STUDIO` (este plano não prevê nenhuma).

**Commits:** terminar com `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

---

## Mapa de arquivos

**Novos**
| Arquivo | Responsabilidade |
|---|---|
| `frontend/voltz/core/preset-layout.hpp/.cpp` | retângulos dos presets em função do canvas |
| `frontend/voltz/core/recording-files.hpp/.cpp` | o que é arquivo de gravação; tamanho legível |
| `frontend/voltz/core/recording-settings.hpp/.cpp` | qualidade/formato ↔ chaves do OBS; escolha do codificador |
| `frontend/voltz/tests/test-preset-layout.cpp`, `test-recording-files.cpp`, `test-recording-settings.cpp` | testes |
| `frontend/data/voltz/mask-circle.png` | máscara circular (alfa) da câmera do Tutorial |
| `frontend/voltz/shell/SharedSources.hpp/.cpp` | fontes compartilhadas por UUID (Tela, Câmera, Fundo) |
| `frontend/voltz/shell/PresetBuilder.hpp/.cpp` | aplica um preset (fontes + cenas + transformações) |
| `frontend/voltz/shell/PresetThumbnail.hpp/.cpp` | miniatura esquemática do preset |
| `frontend/voltz/shell/PresetsPage.hpp/.cpp` | tela Presets + primeira execução |
| `frontend/voltz/shell/ProjectsPage.hpp/.cpp` | tela Projetos |
| `frontend/voltz/shell/RecordingConfig.hpp/.cpp` | leitura/escrita das configurações de gravação no perfil |
| `frontend/voltz/shell/DeviceSelector.hpp/.cpp` | combobox de dispositivo a partir das propriedades de uma fonte |
| `frontend/voltz/shell/SettingsPage.hpp/.cpp` | tela Ajustes |
| `tools/voltz-uia.ps1` | clica botões da casca por UI Automation (testes) |

**Modificados:** `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`,
`frontend/data/locale/voltz-en-US.ini`, `voltz-pt-BR.ini`, `frontend/cmake/feature-voltz.cmake`,
`frontend/voltz/shell/VoltzShell.hpp/.cpp`, `frontend/voltz/shell/voltz-shell.qss`.

---

### Task 1: Núcleo — layouts dos presets, arquivos de gravação e configurações

**Files:**
- Create: `frontend/voltz/core/preset-layout.hpp/.cpp`, `recording-files.hpp/.cpp`, `recording-settings.hpp/.cpp`
- Create: `frontend/voltz/tests/test-preset-layout.cpp`, `test-recording-files.cpp`, `test-recording-settings.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

- [ ] **Step 1: Testes (falham)**

`frontend/voltz/tests/test-preset-layout.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/preset-layout.hpp>

using voltz::CanvasSize;
using voltz::LayoutRect;

namespace {
void checkRect(const LayoutRect &r, double x, double y, double w, double h)
{
	CHECK(r.x == doctest::Approx(x));
	CHECK(r.y == doctest::Approx(y));
	CHECK(r.w == doctest::Approx(w));
	CHECK(r.h == doctest::Approx(h));
}
} // namespace

TEST_CASE("fullCanvas cobre o canvas inteiro")
{
	checkRect(voltz::fullCanvas(CanvasSize{1920, 1080}), 0, 0, 1920, 1080);
}

TEST_CASE("tutorialCamera: quadrado de 24% da altura no canto inferior direito, margem de 3,5%")
{
	checkRect(voltz::tutorialCamera(CanvasSize{1920, 1080}), 1623, 783, 259, 259);
	checkRect(voltz::tutorialCamera(CanvasSize{1280, 720}), 1082, 522, 173, 173);
}

TEST_CASE("sideBySide: tela à esquerda até 70% da largura, câmera à direita, margem de 3% da altura")
{
	checkRect(voltz::sideBySideScreen(CanvasSize{1920, 1080}), 32, 32, 1296, 1016);
	checkRect(voltz::sideBySideCamera(CanvasSize{1920, 1080}), 1360, 32, 528, 1016);
	checkRect(voltz::sideBySideScreen(CanvasSize{1280, 720}), 22, 22, 863, 676);
	checkRect(voltz::sideBySideCamera(CanvasSize{1280, 720}), 907, 22, 351, 676);
}

TEST_CASE("sideBySide: tela e câmera não se sobrepõem e ficam dentro do canvas")
{
	const CanvasSize canvas{1920, 1080};
	const LayoutRect screen = voltz::sideBySideScreen(canvas);
	const LayoutRect camera = voltz::sideBySideCamera(canvas);
	CHECK(screen.x + screen.w < camera.x);
	CHECK(camera.x + camera.w <= canvas.width);
	CHECK(screen.y + screen.h <= canvas.height);
}
```

`frontend/voltz/tests/test-recording-files.cpp`:
```cpp
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
```

`frontend/voltz/tests/test-recording-settings.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/recording-settings.hpp>

using voltz::RecordingFormat;
using voltz::RecordingQuality;

TEST_CASE("qualidade ↔ SimpleOutput/RecQuality")
{
	CHECK(voltz::obsRecQuality(RecordingQuality::High) == "HQ");
	CHECK(voltz::obsRecQuality(RecordingQuality::Compact) == "Small");
	CHECK(voltz::qualityFromObs("Small") == RecordingQuality::Compact);
	CHECK(voltz::qualityFromObs("HQ") == RecordingQuality::High);
	CHECK(voltz::qualityFromObs("Stream") == RecordingQuality::High);
	CHECK(voltz::qualityFromObs("Lossless") == RecordingQuality::High);
	CHECK(voltz::qualityFromObs("") == RecordingQuality::High);
}

TEST_CASE("formato ↔ SimpleOutput/RecFormat2")
{
	CHECK(voltz::obsRecFormat(RecordingFormat::Mp4) == "hybrid_mp4");
	CHECK(voltz::obsRecFormat(RecordingFormat::Mkv) == "mkv");
	CHECK(voltz::obsRecFormat(RecordingFormat::Mov) == "hybrid_mov");
	CHECK(voltz::formatFromObs("mkv") == RecordingFormat::Mkv);
	CHECK(voltz::formatFromObs("mov") == RecordingFormat::Mov);
	CHECK(voltz::formatFromObs("hybrid_mov") == RecordingFormat::Mov);
	CHECK(voltz::formatFromObs("fragmented_mov") == RecordingFormat::Mov);
	CHECK(voltz::formatFromObs("hybrid_mp4") == RecordingFormat::Mp4);
	CHECK(voltz::formatFromObs("mp4") == RecordingFormat::Mp4);
	CHECK(voltz::formatFromObs("flv") == RecordingFormat::Mp4);
}

TEST_CASE("pickRecEncoder prefere AMD, depois NVIDIA, depois Intel, depois x264")
{
	CHECK(voltz::pickRecEncoder({"obs_x264", "h264_texture_amf", "ffmpeg_nvenc", "obs_qsv11"}) == "amd");
	CHECK(voltz::pickRecEncoder({"obs_x264", "ffmpeg_nvenc", "obs_qsv11"}) == "nvenc");
	CHECK(voltz::pickRecEncoder({"obs_x264", "obs_qsv11"}) == "qsv");
	CHECK(voltz::pickRecEncoder({"obs_x264"}) == "x264");
	CHECK(voltz::pickRecEncoder({}) == "x264");
}
```

Em `frontend/voltz/tests/CMakeLists.txt`, acrescentar à lista `PRIVATE` (ordem alfabética):
`test-preset-layout.cpp`, `test-recording-files.cpp`, `test-recording-settings.cpp`.

- [ ] **Step 2: Headers + CMake**

`frontend/voltz/core/preset-layout.hpp`:
```cpp
#pragma once

namespace voltz {

struct CanvasSize {
	double width = 1920;
	double height = 1080;
};

// Retângulo em pixels do canvas (origem no canto superior esquerdo).
struct LayoutRect {
	double x = 0;
	double y = 0;
	double w = 0;
	double h = 0;
};

LayoutRect fullCanvas(CanvasSize canvas);

// Área quadrada da câmera circular do preset Tutorial: lado = 24% da altura, margem = 3,5% da altura,
// canto inferior direito. Valores arredondados para pixels inteiros.
LayoutRect tutorialCamera(CanvasSize canvas);

// Preset "Lado a lado": divisão em 70% da largura, margem (e vão entre as áreas) = 3% da altura.
LayoutRect sideBySideScreen(CanvasSize canvas);
LayoutRect sideBySideCamera(CanvasSize canvas);

} // namespace voltz
```

`frontend/voltz/core/recording-files.hpp`:
```cpp
#pragma once

#include <cstdint>
#include <string>

namespace voltz {

// true para nomes com extensão de vídeo que o OBS grava (mp4, mkv, mov, flv, ts, avi), sem diferenciar
// maiúsculas. Nomes sem base (".mp4") não contam.
bool isRecordingFile(const std::string &fileName);

// "0 B", "1023 B", "1,5 KB", "12,3 MB", "1,2 GB" (base 1024, uma casa decimal, para no GB).
std::string formatFileSize(uint64_t bytes, char decimalSeparator);

} // namespace voltz
```

`frontend/voltz/core/recording-settings.hpp`:
```cpp
#pragma once

#include <string>
#include <vector>

namespace voltz {

enum class RecordingQuality { High, Compact };
enum class RecordingFormat { Mp4, Mkv, Mov };

// SimpleOutput/RecQuality: High = "HQ", Compact = "Small". Qualquer outro valor do OBS ("Stream",
// "Lossless", vazio) é apresentado como High.
std::string obsRecQuality(RecordingQuality quality);
RecordingQuality qualityFromObs(const std::string &value);

// SimpleOutput/RecFormat2: Mp4 = "hybrid_mp4" (resistente a falhas), Mkv = "mkv", Mov = "hybrid_mov".
std::string obsRecFormat(RecordingFormat format);
RecordingFormat formatFromObs(const std::string &value);

// SimpleOutput/RecEncoder a partir dos ids de encoder registrados no libobs:
// "h264_texture_amf" → "amd", "ffmpeg_nvenc" → "nvenc", "obs_qsv11" → "qsv", senão "x264".
std::string pickRecEncoder(const std::vector<std::string> &availableEncoderIds);

} // namespace voltz
```

Em `frontend/voltz/core/CMakeLists.txt`, acrescentar (ordem alfabética) em `PRIVATE`: `preset-layout.cpp`,
`recording-files.cpp`, `recording-settings.cpp`; em `PUBLIC`: os três `.hpp`. Criar os `.cpp` só com o include:
```bash
for f in preset-layout recording-files recording-settings; do printf '#include "%s.hpp"\n' "$f" > "frontend/voltz/core/$f.cpp"; done
```

- [ ] **Step 3: Configurar e compilar os testes — devem FALHAR (link)**

Run: CONFIGURAR, COMPILAR TESTES → `LNK2019` para `fullCanvas`, `tutorialCamera`, `isRecordingFile`, `pickRecEncoder` etc.

- [ ] **Step 4: Implementar**

`frontend/voltz/core/preset-layout.cpp`:
```cpp
#include "preset-layout.hpp"

#include <cmath>

namespace voltz {

LayoutRect fullCanvas(CanvasSize canvas)
{
	return {0, 0, canvas.width, canvas.height};
}

LayoutRect tutorialCamera(CanvasSize canvas)
{
	const double side = std::round(canvas.height * 0.24);
	const double margin = std::round(canvas.height * 0.035);
	return {canvas.width - margin - side, canvas.height - margin - side, side, side};
}

namespace {

struct Split {
	double margin;
	double halfGap;
	double divider;
};

Split sideBySideSplit(CanvasSize canvas)
{
	const double margin = std::round(canvas.height * 0.03);
	return {margin, std::floor(margin / 2), std::round(canvas.width * 0.70)};
}

} // namespace

LayoutRect sideBySideScreen(CanvasSize canvas)
{
	const Split s = sideBySideSplit(canvas);
	return {s.margin, s.margin, s.divider - s.margin - s.halfGap, canvas.height - 2 * s.margin};
}

LayoutRect sideBySideCamera(CanvasSize canvas)
{
	const Split s = sideBySideSplit(canvas);
	const double x = s.divider + s.halfGap;
	return {x, s.margin, canvas.width - s.margin - x, canvas.height - 2 * s.margin};
}

} // namespace voltz
```

`frontend/voltz/core/recording-files.cpp`:
```cpp
#include "recording-files.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace voltz {

bool isRecordingFile(const std::string &fileName)
{
	const size_t dot = fileName.find_last_of('.');
	if (dot == std::string::npos || dot == 0 || dot + 1 >= fileName.size()) {
		return false;
	}
	std::string ext = fileName.substr(dot + 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
	static const char *const kExtensions[] = {"mp4", "mkv", "mov", "flv", "ts", "avi"};
	return std::any_of(std::begin(kExtensions), std::end(kExtensions),
			   [&ext](const char *candidate) { return ext == candidate; });
}

std::string formatFileSize(uint64_t bytes, char decimalSeparator)
{
	if (bytes < 1024) {
		return std::to_string(bytes) + " B";
	}
	static const char *const kUnits[] = {"KB", "MB", "GB"};
	double value = double(bytes) / 1024.0;
	int unit = 0;
	while (value >= 1024.0 && unit < 2) {
		value /= 1024.0;
		unit++;
	}
	char buffer[48];
	std::snprintf(buffer, sizeof(buffer), "%.1f", value);
	std::string text = buffer;
	std::replace(text.begin(), text.end(), '.', decimalSeparator);
	return text + " " + kUnits[unit];
}

} // namespace voltz
```

`frontend/voltz/core/recording-settings.cpp`:
```cpp
#include "recording-settings.hpp"

#include <algorithm>

namespace voltz {

std::string obsRecQuality(RecordingQuality quality)
{
	return quality == RecordingQuality::Compact ? "Small" : "HQ";
}

RecordingQuality qualityFromObs(const std::string &value)
{
	return value == "Small" ? RecordingQuality::Compact : RecordingQuality::High;
}

std::string obsRecFormat(RecordingFormat format)
{
	switch (format) {
	case RecordingFormat::Mkv:
		return "mkv";
	case RecordingFormat::Mov:
		return "hybrid_mov";
	case RecordingFormat::Mp4:
		break;
	}
	return "hybrid_mp4";
}

RecordingFormat formatFromObs(const std::string &value)
{
	if (value == "mkv") {
		return RecordingFormat::Mkv;
	}
	if (value == "mov" || value == "hybrid_mov" || value == "fragmented_mov") {
		return RecordingFormat::Mov;
	}
	return RecordingFormat::Mp4;
}

std::string pickRecEncoder(const std::vector<std::string> &availableEncoderIds)
{
	auto has = [&availableEncoderIds](const char *id) {
		return std::find(availableEncoderIds.begin(), availableEncoderIds.end(), id) !=
		       availableEncoderIds.end();
	};
	if (has("h264_texture_amf")) {
		return "amd";
	}
	if (has("ffmpeg_nvenc")) {
		return "nvenc";
	}
	if (has("obs_qsv11")) {
		return "qsv";
	}
	return "x264";
}

} // namespace voltz
```

- [ ] **Step 5: Testes passam + app compila**

Run: COMPILAR TESTES, RODAR TESTES, COMPILAR APP. Expected: `100% tests passed`; o executável de testes direto
reporta **43** casos (34 anteriores + 4 de preset-layout + 2 de recording-files + 3 de recording-settings),
todos passando. App: `exit=0`.

- [ ] **Step 6: Commit**

```bash
git add frontend/voltz/core frontend/voltz/tests
git commit -m "feat(voltz): nucleo dos presets (geometria), arquivos de gravacao e mapeamento de configuracoes

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Textos, máscara circular e ferramenta de UI Automation

**Files:**
- Modify: `frontend/data/locale/voltz-en-US.ini`, `voltz-pt-BR.ini`
- Create: `frontend/data/voltz/mask-circle.png`, `tools/voltz-uia.ps1`

- [ ] **Step 1: Textos (acrescentar ao fim dos dois arquivos, UTF-8 sem BOM)**

`voltz-pt-BR.ini`:
```ini
Voltz.Presets.Subtitle="Escolha um ponto de partida. Dá para ajustar tudo depois."
Voltz.Presets.FirstRun="Bem-vindo ao Voltz Studio! Escolha um preset para começar ou pule e monte do seu jeito."
Voltz.Presets.Skip="Pular"
Voltz.Presets.Apply="Aplicar"
Voltz.Preset.Tutorial.Name="Tutorial"
Voltz.Preset.Tutorial.Desc="Tela cheia com a sua câmera em círculo no canto."
Voltz.Preset.ScreenCamera.Name="Só tela / Só câmera"
Voltz.Preset.ScreenCamera.Desc="Duas cenas: a tela inteira e a câmera em tela cheia."
Voltz.Preset.SideBySide.Name="Lado a lado"
Voltz.Preset.SideBySide.Desc="Tela à esquerda e câmera à direita, sobre o fundo da marca."
Voltz.Preset.Opening.Name="Abertura / Pausa"
Voltz.Preset.Opening.Desc="Fundo da marca com um título editável (\"Já volto\", \"Aula 03\")."
Voltz.Scene.Tutorial="Tutorial"
Voltz.Scene.ScreenOnly="Só tela"
Voltz.Scene.CameraOnly="Só câmera"
Voltz.Scene.SideBySide="Lado a lado"
Voltz.Scene.Opening="Abertura"
Voltz.Source.Screen="Tela"
Voltz.Source.Camera="Câmera"
Voltz.Source.RoundCamera="Câmera redonda"
Voltz.Source.Background="Fundo"
Voltz.Source.Title="Título"
Voltz.Source.TitleText="Já volto"
Voltz.Filter.CircleMask="Máscara redonda"
Voltz.Projects.Empty="Nenhuma gravação ainda. Elas aparecem aqui assim que você gravar."
Voltz.Projects.Folder="Pasta: %1"
Voltz.Projects.OpenFolder="Abrir pasta"
Voltz.Projects.Refresh="Atualizar"
Voltz.Projects.Open="Abrir"
Voltz.Projects.ShowInFolder="Mostrar na pasta"
Voltz.Projects.Delete="Mover para a lixeira"
Voltz.Projects.DeleteConfirm.Title="Mover para a lixeira"
Voltz.Projects.DeleteConfirm.Text="Mover \"%1\" para a lixeira?"
Voltz.Projects.DeleteFailed="Não foi possível mover o arquivo para a lixeira."
Voltz.Projects.Column.Name="Nome"
Voltz.Projects.Column.Date="Data"
Voltz.Projects.Column.Size="Tamanho"
Voltz.Settings.Recording="Gravação"
Voltz.Settings.Folder="Pasta das gravações"
Voltz.Settings.ChangeFolder="Alterar…"
Voltz.Settings.Format="Formato"
Voltz.Settings.Format.Mp4="MP4 (recomendado)"
Voltz.Settings.Format.Mkv="MKV"
Voltz.Settings.Format.Mov="MOV"
Voltz.Settings.Quality="Qualidade"
Voltz.Settings.Quality.High="Alta"
Voltz.Settings.Quality.Compact="Arquivo menor"
Voltz.Settings.Encoder="Codificador"
Voltz.Settings.Encoder.Auto="%1 (automático)"
Voltz.Settings.Encoder.amd="AMD"
Voltz.Settings.Encoder.nvenc="NVIDIA"
Voltz.Settings.Encoder.qsv="Intel"
Voltz.Settings.Encoder.x264="Processador (x264)"
Voltz.Settings.AdvancedMode="O modo de saída avançado está ativo. Ajuste a gravação pelo modo clássico."
Voltz.Settings.LockedWhileRecording="As opções de gravação ficam bloqueadas enquanto você grava."
Voltz.Settings.Capture="Captura"
Voltz.Settings.Monitor="Monitor"
Voltz.Settings.Camera="Câmera"
Voltz.Settings.Microphone="Microfone"
Voltz.Settings.NoSource="Aplique um preset para escolher este dispositivo."
Voltz.Settings.About="Sobre"
Voltz.Settings.OpenLogs="Abrir pasta de logs"
```

`voltz-en-US.ini` (mesmas chaves):
```ini
Voltz.Presets.Subtitle="Pick a starting point. You can tweak everything later."
Voltz.Presets.FirstRun="Welcome to Voltz Studio! Pick a preset to get started, or skip and build your own."
Voltz.Presets.Skip="Skip"
Voltz.Presets.Apply="Apply"
Voltz.Preset.Tutorial.Name="Tutorial"
Voltz.Preset.Tutorial.Desc="Full screen with your camera in a circle in the corner."
Voltz.Preset.ScreenCamera.Name="Screen only / Camera only"
Voltz.Preset.ScreenCamera.Desc="Two scenes: the whole screen and the camera in full screen."
Voltz.Preset.SideBySide.Name="Side by side"
Voltz.Preset.SideBySide.Desc="Screen on the left and camera on the right, over the brand background."
Voltz.Preset.Opening.Name="Opening / Break"
Voltz.Preset.Opening.Desc="Brand background with an editable title (\"Be right back\", \"Lesson 03\")."
Voltz.Scene.Tutorial="Tutorial"
Voltz.Scene.ScreenOnly="Screen only"
Voltz.Scene.CameraOnly="Camera only"
Voltz.Scene.SideBySide="Side by side"
Voltz.Scene.Opening="Opening"
Voltz.Source.Screen="Screen"
Voltz.Source.Camera="Camera"
Voltz.Source.RoundCamera="Round camera"
Voltz.Source.Background="Background"
Voltz.Source.Title="Title"
Voltz.Source.TitleText="Be right back"
Voltz.Filter.CircleMask="Round mask"
Voltz.Projects.Empty="No recordings yet. They show up here as soon as you record."
Voltz.Projects.Folder="Folder: %1"
Voltz.Projects.OpenFolder="Open folder"
Voltz.Projects.Refresh="Refresh"
Voltz.Projects.Open="Open"
Voltz.Projects.ShowInFolder="Show in folder"
Voltz.Projects.Delete="Move to trash"
Voltz.Projects.DeleteConfirm.Title="Move to trash"
Voltz.Projects.DeleteConfirm.Text="Move \"%1\" to the trash?"
Voltz.Projects.DeleteFailed="Could not move the file to the trash."
Voltz.Projects.Column.Name="Name"
Voltz.Projects.Column.Date="Date"
Voltz.Projects.Column.Size="Size"
Voltz.Settings.Recording="Recording"
Voltz.Settings.Folder="Recordings folder"
Voltz.Settings.ChangeFolder="Change…"
Voltz.Settings.Format="Format"
Voltz.Settings.Format.Mp4="MP4 (recommended)"
Voltz.Settings.Format.Mkv="MKV"
Voltz.Settings.Format.Mov="MOV"
Voltz.Settings.Quality="Quality"
Voltz.Settings.Quality.High="High"
Voltz.Settings.Quality.Compact="Smaller file"
Voltz.Settings.Encoder="Encoder"
Voltz.Settings.Encoder.Auto="%1 (automatic)"
Voltz.Settings.Encoder.amd="AMD"
Voltz.Settings.Encoder.nvenc="NVIDIA"
Voltz.Settings.Encoder.qsv="Intel"
Voltz.Settings.Encoder.x264="CPU (x264)"
Voltz.Settings.AdvancedMode="Advanced output mode is active. Adjust recording in classic mode."
Voltz.Settings.LockedWhileRecording="Recording options are locked while you record."
Voltz.Settings.Capture="Capture"
Voltz.Settings.Monitor="Monitor"
Voltz.Settings.Camera="Camera"
Voltz.Settings.Microphone="Microphone"
Voltz.Settings.NoSource="Apply a preset to choose this device."
Voltz.Settings.About="About"
Voltz.Settings.OpenLogs="Open logs folder"
```

Verificar paridade: `diff <(cut -d= -f1 frontend/data/locale/voltz-en-US.ini | sort) <(cut -d= -f1 frontend/data/locale/voltz-pt-BR.ini | sort)` → sem saída.

- [ ] **Step 2: Máscara circular (512×512, círculo branco opaco com borda suave sobre fundo transparente)**

Gerar com PowerShell (System.Drawing):
```powershell
Add-Type -AssemblyName System.Drawing
$dir = Join-Path (Get-Location) "frontend\data\voltz"
New-Item -ItemType Directory -Force $dir | Out-Null
$bmp = New-Object System.Drawing.Bitmap 512, 512, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::Transparent)
$g.FillEllipse([System.Drawing.Brushes]::White, 1, 1, 510, 510)
$g.Dispose()
$bmp.Save((Join-Path $dir "mask-circle.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
(Get-Item (Join-Path $dir "mask-circle.png")).Length
```
Expected: arquivo de alguns KB. Após CONFIGURAR + build ele aparece em
`build_x64/rundir/RelWithDebInfo/data/obs-studio/voltz/mask-circle.png`.

- [ ] **Step 3: `tools/voltz-uia.ps1` — clicar botões da casca por UI Automation**

Criar um script PowerShell 5.1 que:
- recebe `-ProcessId <int>` (obrigatório), `-Name <string>` (texto do botão/elemento, obrigatório), `-TimeoutSeconds` (padrão 10);
- carrega `UIAutomationClient`/`UIAutomationTypes`, encontra a janela principal do processo
  (`AutomationElement.RootElement.FindFirst(TreeScope.Children, ProcessIdProperty == pid)`), procura um descendente
  com `NameProperty == -Name` (aguardando até o timeout) e o aciona com `InvokePattern` (ou, se não houver,
  `SelectionItemPattern.Select()`/`TogglePattern.Toggle()`);
- imprime `OK <Name>` (exit 0) ou `FALHOU <motivo>` (exit 1).
Testar: abrir o app (`--portable --multi`, config limpa), `tools/voltz-uia.ps1 -ProcessId <pid> -Name "Ajustes"` deve
trocar para a página Ajustes (verificar com captura **só da janela**). Se os widgets do Qt não expuserem o nome via
UIA, reporte o motivo; nesse caso as tasks seguintes verificam apenas por websocket/captura.

- [ ] **Step 4: Commit**

```bash
git add frontend/data/locale/voltz-en-US.ini frontend/data/locale/voltz-pt-BR.ini frontend/data/voltz/mask-circle.png tools/voltz-uia.ps1
git commit -m "feat(voltz): textos de presets/projetos/ajustes, mascara circular e helper de UI Automation

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Fontes compartilhadas e `PresetBuilder`

**Files:**
- Create: `frontend/voltz/shell/SharedSources.hpp/.cpp`, `frontend/voltz/shell/PresetBuilder.hpp/.cpp`
- Modify: `frontend/cmake/feature-voltz.cmake`

As fontes compartilhadas são criadas uma vez e reaproveitadas por todos os presets. O UUID de cada uma fica em
`user.ini` (`[Voltz] ScreenSourceUuid`, `CameraSourceUuid`, `BackgroundSourceUuid`) para Ajustes encontrá-las
mesmo se o usuário renomear ou trocar o idioma.

- [ ] **Step 1: `SharedSources`**

`frontend/voltz/shell/SharedSources.hpp`:
```cpp
#pragma once

#include <obs.hpp>

namespace voltz {

enum class SharedSource { Screen, Camera, Background };

// Devolve a fonte compartilhada (ref forte) — pelo UUID salvo em user.ini [Voltz]; se não existir mais, null.
OBSSource findSharedSource(SharedSource which);

// Devolve a fonte compartilhada, criando-a se preciso (Tela = monitor_capture do primeiro monitor; Câmera =
// dshow_input do primeiro dispositivo; Fundo = color_source_v3 #09090B do tamanho do canvas) e salvando o UUID.
OBSSource ensureSharedSource(SharedSource which);

} // namespace voltz
```

`frontend/voltz/shell/SharedSources.cpp`:
```cpp
#include "SharedSources.hpp"

#include <OBSApp.hpp>

#include <util/config-file.h>

#include <QString>

namespace voltz {

namespace {

const char *configKey(SharedSource which)
{
	switch (which) {
	case SharedSource::Screen:
		return "ScreenSourceUuid";
	case SharedSource::Camera:
		return "CameraSourceUuid";
	case SharedSource::Background:
		return "BackgroundSourceUuid";
	}
	return "";
}

// Primeiro valor habilitado de uma propriedade de lista (ex.: monitor_id, video_device_id), ou "" se não houver.
std::string firstListValue(const char *sourceId, const char *property)
{
	obs_properties_t *props = obs_get_source_properties(sourceId);
	if (!props) {
		return {};
	}
	std::string value;
	if (obs_property_t *p = obs_properties_get(props, property)) {
		const size_t count = obs_property_list_item_count(p);
		for (size_t i = 0; i < count; i++) {
			const char *candidate = obs_property_list_item_string(p, i);
			if (!obs_property_list_item_disabled(p, i) && candidate && *candidate) {
				value = candidate;
				break;
			}
		}
	}
	obs_properties_destroy(props);
	return value;
}

OBSSource createShared(SharedSource which)
{
	OBSDataAutoRelease settings = obs_data_create();
	const char *id = nullptr;
	QString name;

	switch (which) {
	case SharedSource::Screen:
		id = "monitor_capture";
		name = QTStr("Voltz.Source.Screen");
		obs_data_set_string(settings, "monitor_id", firstListValue(id, "monitor_id").c_str());
		obs_data_set_bool(settings, "capture_cursor", true);
		break;
	case SharedSource::Camera:
		id = "dshow_input";
		name = QTStr("Voltz.Source.Camera");
		obs_data_set_string(settings, "video_device_id", firstListValue(id, "video_device_id").c_str());
		break;
	case SharedSource::Background: {
		id = "color_source_v3";
		name = QTStr("Voltz.Source.Background");
		obs_video_info ovi = {};
		obs_get_video_info(&ovi);
		obs_data_set_int(settings, "color", 0xFF0B0909); // ABGR de #09090B
		obs_data_set_int(settings, "width", ovi.base_width ? ovi.base_width : 1920);
		obs_data_set_int(settings, "height", ovi.base_height ? ovi.base_height : 1080);
		break;
	}
	}

	/* Nome já usado por outra fonte: reaproveita só se for do mesmo tipo; senão cria com sufixo. */
	const char *unversionedId = which == SharedSource::Background ? "color_source" : id;
	OBSSourceAutoRelease existing = obs_get_source_by_name(name.toUtf8().constData());
	if (existing && strcmp(obs_source_get_unversioned_id(existing), unversionedId) == 0) {
		return OBSSource(existing.Get()); // OBSRef(T) faz addref; o AutoRelease solta o dele
	}
	if (existing) {
		name += QStringLiteral(" (Voltz)");
	}
	OBSSourceAutoRelease created = obs_source_create(id, name.toUtf8().constData(), settings, nullptr);
	return OBSSource(created.Get());
}

} // namespace

OBSSource findSharedSource(SharedSource which)
{
	const char *uuid = config_get_string(App()->GetUserConfig(), "Voltz", configKey(which));
	if (!uuid || !*uuid) {
		return {};
	}
	OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid);
	return OBSSource(source.Get());
}

OBSSource ensureSharedSource(SharedSource which)
{
	if (OBSSource found = findSharedSource(which)) {
		return found;
	}
	OBSSource created = createShared(which);
	if (created) {
		config_set_string(App()->GetUserConfig(), "Voltz", configKey(which), obs_source_get_uuid(created));
		config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
	}
	return created;
}

} // namespace voltz
```

> Observação ao implementador: inclua `<cstring>` (strcmp). `OBSSource(raw)` faz addref (construtor de `OBSRef`).

- [ ] **Step 2: `PresetBuilder`**

`frontend/voltz/shell/PresetBuilder.hpp`:
```cpp
#pragma once

#include <obs.hpp>

namespace voltz {

enum class PresetId { Tutorial, ScreenCamera, SideBySide, Opening };

// Cria as fontes compartilhadas que faltarem e as cenas do preset. Cena com o mesmo nome já existente não é
// recriada (o usuário pode tê-la editado). Devolve a cena principal do preset (ref forte) para selecionar.
// Salva a coleção de cenas (obs_frontend_save). Thread da UI.
OBSSource applyPreset(PresetId id);

} // namespace voltz
```

`frontend/voltz/shell/PresetBuilder.cpp`:
```cpp
#include "PresetBuilder.hpp"

#include "SharedSources.hpp"

#include <voltz/core/preset-layout.hpp>

#include <OBSApp.hpp>
#include <utility/platform.hpp>

#include <obs-frontend-api.h>

#include <QString>

#include <string>

namespace voltz {

namespace {

CanvasSize canvasSize()
{
	obs_video_info ovi = {};
	if (!obs_get_video_info(&ovi) || !ovi.base_width || !ovi.base_height) {
		return {};
	}
	return {double(ovi.base_width), double(ovi.base_height)};
}

void placeItem(obs_sceneitem_t *item, const LayoutRect &rect, obs_bounds_type bounds, bool cropToBounds)
{
	obs_transform_info info = {};
	vec2_set(&info.pos, float(rect.x), float(rect.y));
	info.rot = 0.0f;
	vec2_set(&info.scale, 1.0f, 1.0f);
	info.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	info.bounds_type = bounds;
	info.bounds_alignment = OBS_ALIGN_CENTER;
	vec2_set(&info.bounds, float(rect.w), float(rect.h));
	info.crop_to_bounds = cropToBounds;
	obs_sceneitem_set_info2(item, &info);
}

// Cena existente com esse nome (não recria) ou nova. `created` diz se foi criada agora.
OBSScene sceneNamed(const QString &name, bool &created)
{
	created = false;
	OBSSourceAutoRelease existing = obs_get_source_by_name(name.toUtf8().constData());
	if (existing) {
		if (obs_scene_t *scene = obs_scene_from_source(existing)) {
			return OBSScene(scene);
		}
	}
	OBSSceneAutoRelease scene = obs_scene_create(name.toUtf8().constData());
	created = true;
	return OBSScene(scene.Get());
}

obs_sceneitem_t *addFull(obs_scene_t *scene, obs_source_t *source, CanvasSize canvas, obs_bounds_type bounds,
			 bool crop)
{
	obs_sceneitem_t *item = obs_scene_add(scene, source);
	if (item) {
		placeItem(item, fullCanvas(canvas), bounds, crop);
	}
	return item;
}

void addRoundCamera(obs_scene_t *scene, obs_source_t *camera, CanvasSize canvas)
{
	obs_sceneitem_t *cameraItem = obs_scene_add(scene, camera);
	if (!cameraItem) {
		return;
	}
	obs_sceneitem_t *items[] = {cameraItem};
	obs_sceneitem_t *group =
		obs_scene_insert_group(scene, QTStr("Voltz.Source.RoundCamera").toUtf8().constData(), items, 1);
	if (!group) {
		/* Sem grupo: câmera quadrada no lugar do círculo (melhor do que nada). */
		placeItem(cameraItem, tutorialCamera(canvas), OBS_BOUNDS_SCALE_OUTER, true);
		return;
	}
	placeItem(group, tutorialCamera(canvas), OBS_BOUNDS_SCALE_OUTER, true);

	std::string maskPath;
	if (!GetDataFilePath("voltz/mask-circle.png", maskPath)) {
		blog(LOG_WARNING, "[voltz] mascara circular nao encontrada");
		return;
	}
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "type", "mask_alpha_filter.effect");
	obs_data_set_string(settings, "image_path", maskPath.c_str());
	obs_data_set_bool(settings, "stretch", false);
	OBSSourceAutoRelease mask = obs_source_create("mask_filter_v2",
						      QTStr("Voltz.Filter.CircleMask").toUtf8().constData(), settings,
						      nullptr);
	if (mask) {
		obs_source_filter_add(obs_sceneitem_get_source(group), mask);
	}
}

void addTitle(obs_scene_t *scene, CanvasSize canvas)
{
	OBSDataAutoRelease font = obs_data_create();
	obs_data_set_string(font, "face", "Segoe UI");
	obs_data_set_int(font, "size", 120);
	obs_data_set_int(font, "flags", OBS_FONT_BOLD);
	obs_data_set_string(font, "style", "Bold");

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "text", QTStr("Voltz.Source.TitleText").toUtf8().constData());
	obs_data_set_obj(settings, "font", font);
	obs_data_set_string(settings, "align", "center");
	obs_data_set_string(settings, "valign", "center");

	OBSSourceAutoRelease title = obs_source_create(
		"text_gdiplus_v3", QTStr("Voltz.Source.Title").toUtf8().constData(), settings, nullptr);
	if (!title) {
		return;
	}
	obs_sceneitem_t *item = obs_scene_add(scene, title);
	if (!item) {
		return;
	}
	obs_transform_info info = {};
	vec2_set(&info.pos, float(canvas.width / 2), float(canvas.height / 2));
	vec2_set(&info.scale, 1.0f, 1.0f);
	info.alignment = OBS_ALIGN_CENTER;
	info.bounds_type = OBS_BOUNDS_NONE;
	obs_sceneitem_set_info2(item, &info);
}

OBSScene buildTutorial(CanvasSize canvas)
{
	bool created = false;
	OBSScene scene = sceneNamed(QTStr("Voltz.Scene.Tutorial"), created);
	if (created && scene) {
		if (OBSSource screen = ensureSharedSource(SharedSource::Screen)) {
			addFull(scene, screen, canvas, OBS_BOUNDS_SCALE_INNER, false);
		}
		if (OBSSource camera = ensureSharedSource(SharedSource::Camera)) {
			addRoundCamera(scene, camera, canvas);
		}
	}
	return scene;
}

OBSScene buildScreenOnly(CanvasSize canvas)
{
	bool created = false;
	OBSScene scene = sceneNamed(QTStr("Voltz.Scene.ScreenOnly"), created);
	if (created && scene) {
		if (OBSSource screen = ensureSharedSource(SharedSource::Screen)) {
			addFull(scene, screen, canvas, OBS_BOUNDS_SCALE_INNER, false);
		}
	}
	return scene;
}

OBSScene buildCameraOnly(CanvasSize canvas)
{
	bool created = false;
	OBSScene scene = sceneNamed(QTStr("Voltz.Scene.CameraOnly"), created);
	if (created && scene) {
		if (OBSSource camera = ensureSharedSource(SharedSource::Camera)) {
			addFull(scene, camera, canvas, OBS_BOUNDS_SCALE_OUTER, true);
		}
	}
	return scene;
}

OBSScene buildSideBySide(CanvasSize canvas)
{
	bool created = false;
	OBSScene scene = sceneNamed(QTStr("Voltz.Scene.SideBySide"), created);
	if (created && scene) {
		if (OBSSource background = ensureSharedSource(SharedSource::Background)) {
			addFull(scene, background, canvas, OBS_BOUNDS_STRETCH, false);
		}
		if (OBSSource screen = ensureSharedSource(SharedSource::Screen)) {
			if (obs_sceneitem_t *item = obs_scene_add(scene, screen)) {
				placeItem(item, sideBySideScreen(canvas), OBS_BOUNDS_SCALE_INNER, false);
			}
		}
		if (OBSSource camera = ensureSharedSource(SharedSource::Camera)) {
			if (obs_sceneitem_t *item = obs_scene_add(scene, camera)) {
				placeItem(item, sideBySideCamera(canvas), OBS_BOUNDS_SCALE_OUTER, true);
			}
		}
	}
	return scene;
}

OBSScene buildOpening(CanvasSize canvas)
{
	bool created = false;
	OBSScene scene = sceneNamed(QTStr("Voltz.Scene.Opening"), created);
	if (created && scene) {
		if (OBSSource background = ensureSharedSource(SharedSource::Background)) {
			addFull(scene, background, canvas, OBS_BOUNDS_STRETCH, false);
		}
		addTitle(scene, canvas);
	}
	return scene;
}

} // namespace

OBSSource applyPreset(PresetId id)
{
	const CanvasSize canvas = canvasSize();
	OBSScene main;
	switch (id) {
	case PresetId::Tutorial:
		main = buildTutorial(canvas);
		break;
	case PresetId::ScreenCamera:
		main = buildScreenOnly(canvas);
		buildCameraOnly(canvas);
		break;
	case PresetId::SideBySide:
		main = buildSideBySide(canvas);
		break;
	case PresetId::Opening:
		main = buildOpening(canvas);
		break;
	}
	obs_frontend_save();
	blog(LOG_INFO, "[voltz] preset aplicado: %d", int(id));
	return main ? OBSSource(obs_scene_get_source(main)) : OBSSource();
}

} // namespace voltz
```

> Observações ao implementador:
> - `obs_scene_from_source` devolve ponteiro emprestado; `OBSScene(scene)` faz `obs_scene_get_ref` (OBSSafeRef).
>   `OBSSceneAutoRelease` e `OBSSourceAutoRelease` existem em `obs.hpp` (linhas ~51).
> - Texto usa o id versionado `text_gdiplus_v3` (o id sem versão é obsoleto — visto no Plano 2A).
> - Criar a cena dispara `source_create` → o OBSBasic a adiciona à lista e salva sozinho. Não chame APIs do
>   OBSBasic para isso.

Em `frontend/cmake/feature-voltz.cmake`: acrescentar `PresetBuilder.cpp/.hpp` e `SharedSources.cpp/.hpp` (ordem
alfabética).

- [ ] **Step 3: Compilar e smoke**

Run: CONFIGURAR, COMPILAR APP, SMOKE → `exit=0`, `SMOKE OK`. (O builder ainda não é chamado; a verificação
visual acontece na Task 4.)

- [ ] **Step 4: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): fontes compartilhadas por UUID e PresetBuilder (5 cenas de fabrica, camera circular por grupo + mascara)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Tela Presets + primeira execução

**Files:**
- Create: `frontend/voltz/shell/PresetThumbnail.hpp/.cpp`, `frontend/voltz/shell/PresetsPage.hpp/.cpp`
- Modify: `frontend/voltz/shell/VoltzShell.hpp/.cpp`, `frontend/voltz/shell/voltz-shell.qss`, `frontend/cmake/feature-voltz.cmake`

- [ ] **Step 1: Miniatura**

`frontend/voltz/shell/PresetThumbnail.hpp`:
```cpp
#pragma once

#include "PresetBuilder.hpp"

#include <QWidget>

namespace voltz {

// Desenho esquemático do preset (16:9), usando a mesma geometria do PresetBuilder.
class PresetThumbnail : public QWidget {
	Q_OBJECT

public:
	explicit PresetThumbnail(PresetId preset, QWidget *parent = nullptr);
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	PresetId preset_;
};

} // namespace voltz
```

`frontend/voltz/shell/PresetThumbnail.cpp`:
```cpp
#include "PresetThumbnail.hpp"

#include "Icons.hpp"

#include <voltz/core/preset-layout.hpp>

#include <QPainter>
#include <QPainterPath>

#include "moc_PresetThumbnail.cpp"

namespace voltz {

namespace {

QRectF toWidget(const LayoutRect &r, const QRectF &frame, CanvasSize canvas)
{
	const double sx = frame.width() / canvas.width;
	const double sy = frame.height() / canvas.height;
	return QRectF(frame.x() + r.x * sx, frame.y() + r.y * sy, r.w * sx, r.h * sy);
}

void drawScreen(QPainter &p, const QRectF &rect)
{
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(0x2A, 0x2A, 0x31));
	p.drawRoundedRect(rect, 3, 3);
	p.setBrush(QColor(0x3A, 0x3A, 0x42));
	const QRectF bar(rect.x() + rect.width() * 0.08, rect.y() + rect.height() * 0.14, rect.width() * 0.5,
			 rect.height() * 0.08);
	p.drawRoundedRect(bar, 2, 2);
}

void drawCamera(QPainter &p, const QRectF &rect, bool round)
{
	p.setPen(QPen(palette::kAccent, 1.5));
	p.setBrush(QColor(0xD4, 0xF5, 0x3C, 40));
	if (round) {
		p.drawEllipse(rect);
	} else {
		p.drawRoundedRect(rect, 3, 3);
	}
}

} // namespace

PresetThumbnail::PresetThumbnail(PresetId preset, QWidget *parent) : QWidget(parent), preset_(preset)
{
	setMinimumSize(192, 108);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize PresetThumbnail::sizeHint() const
{
	return QSize(256, 144);
}

void PresetThumbnail::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	const CanvasSize canvas{1920, 1080};
	QRectF frame(0, 0, width(), width() * 9.0 / 16.0);
	if (frame.height() > height()) {
		frame = QRectF(0, 0, height() * 16.0 / 9.0, height());
	}
	frame.moveCenter(rect().center());

	p.setPen(QPen(QColor(0x1F, 0x1F, 0x24), 1));
	p.setBrush(QColor(0x09, 0x09, 0x0B));
	p.drawRoundedRect(frame, 6, 6);

	switch (preset_) {
	case PresetId::Tutorial:
		drawScreen(p, toWidget(fullCanvas(canvas), frame, canvas).adjusted(4, 4, -4, -4));
		drawCamera(p, toWidget(tutorialCamera(canvas), frame, canvas), true);
		break;
	case PresetId::ScreenCamera: {
		QRectF left = frame;
		left.setWidth(frame.width() / 2 - 3);
		QRectF right = left;
		right.moveLeft(frame.x() + frame.width() / 2 + 3);
		drawScreen(p, left.adjusted(4, 4, -2, -4));
		drawCamera(p, right.adjusted(2, 4, -4, -4), false);
		break;
	}
	case PresetId::SideBySide:
		drawScreen(p, toWidget(sideBySideScreen(canvas), frame, canvas));
		drawCamera(p, toWidget(sideBySideCamera(canvas), frame, canvas), false);
		break;
	case PresetId::Opening: {
		p.setPen(Qt::NoPen);
		p.setBrush(palette::kAccent);
		const QRectF title(frame.center().x() - frame.width() * 0.2, frame.center().y() - frame.height() * 0.05,
				   frame.width() * 0.4, frame.height() * 0.1);
		p.drawRoundedRect(title, 3, 3);
		break;
	}
	}
}

} // namespace voltz
```

- [ ] **Step 2: Página**

`frontend/voltz/shell/PresetsPage.hpp`:
```cpp
#pragma once

#include "PresetBuilder.hpp"

#include <QWidget>

class QFrame;

namespace voltz {

// Tela "Presets": 4 cartões (miniatura, nome, descrição, Aplicar). Na primeira execução mostra uma faixa de
// boas-vindas com "Pular". Emite presetApplied depois de aplicar (a casca volta para a tela Gravar).
class PresetsPage : public QWidget {
	Q_OBJECT

public:
	explicit PresetsPage(QWidget *parent = nullptr);

	static bool isFirstRun();

signals:
	void presetApplied();
	void firstRunDismissed();

private:
	QWidget *buildCard(PresetId preset, const QString &nameKey, const QString &descKey);
	void apply(PresetId preset);
	void markFirstRunDone();

	QFrame *welcome_ = nullptr;
};

} // namespace voltz
```

`frontend/voltz/shell/PresetsPage.cpp`:
```cpp
#include "PresetsPage.hpp"

#include "PresetThumbnail.hpp"
#include "RecordingConfig.hpp"

#include <OBSApp.hpp>

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "moc_PresetsPage.cpp"

namespace voltz {

bool PresetsPage::isFirstRun()
{
	config_t *config = App()->GetUserConfig();
	config_set_default_bool(config, "Voltz", "PresetsDone", false);
	return !config_get_bool(config, "Voltz", "PresetsDone");
}

PresetsPage::PresetsPage(QWidget *parent) : QWidget(parent)
{
	auto *heading = new QLabel(QTStr("Voltz.Presets.Title"), this);
	heading->setObjectName("pageTitle");
	auto *subtitle = new QLabel(QTStr("Voltz.Presets.Subtitle"), this);
	subtitle->setObjectName("muted");

	welcome_ = new QFrame(this);
	welcome_->setObjectName("welcomeBanner");
	auto *welcomeText = new QLabel(QTStr("Voltz.Presets.FirstRun"), welcome_);
	welcomeText->setWordWrap(true);
	auto *skip = new QPushButton(QTStr("Voltz.Presets.Skip"), welcome_);
	auto *welcomeLayout = new QHBoxLayout(welcome_);
	welcomeLayout->setContentsMargins(14, 10, 10, 10);
	welcomeLayout->addWidget(welcomeText, 1);
	welcomeLayout->addWidget(skip);
	welcome_->setVisible(isFirstRun());
	connect(skip, &QPushButton::clicked, this, [this]() {
		markFirstRunDone();
		emit firstRunDismissed();
	});

	auto *grid = new QGridLayout();
	grid->setSpacing(16);
	grid->addWidget(buildCard(PresetId::Tutorial, "Voltz.Preset.Tutorial.Name", "Voltz.Preset.Tutorial.Desc"), 0, 0);
	grid->addWidget(
		buildCard(PresetId::ScreenCamera, "Voltz.Preset.ScreenCamera.Name", "Voltz.Preset.ScreenCamera.Desc"), 0,
		1);
	grid->addWidget(
		buildCard(PresetId::SideBySide, "Voltz.Preset.SideBySide.Name", "Voltz.Preset.SideBySide.Desc"), 1, 0);
	grid->addWidget(buildCard(PresetId::Opening, "Voltz.Preset.Opening.Name", "Voltz.Preset.Opening.Desc"), 1, 1);

	auto *content = new QWidget(this);
	auto *contentLayout = new QVBoxLayout(content);
	contentLayout->setContentsMargins(32, 28, 32, 28);
	contentLayout->setSpacing(10);
	contentLayout->addWidget(heading);
	contentLayout->addWidget(subtitle);
	contentLayout->addWidget(welcome_);
	contentLayout->addSpacing(8);
	contentLayout->addLayout(grid);
	contentLayout->addStretch(1);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(content);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->addWidget(scroll);
}

QWidget *PresetsPage::buildCard(PresetId preset, const QString &nameKey, const QString &descKey)
{
	auto *card = new QFrame(this);
	card->setObjectName("presetCard");

	auto *name = new QLabel(QTStr(nameKey.toUtf8().constData()), card);
	name->setObjectName("cardTitle");
	auto *desc = new QLabel(QTStr(descKey.toUtf8().constData()), card);
	desc->setObjectName("muted");
	desc->setWordWrap(true);
	auto *applyButton = new QPushButton(QTStr("Voltz.Presets.Apply"), card);
	applyButton->setObjectName("primaryButton");
	applyButton->setAccessibleName(QTStr(nameKey.toUtf8().constData()) + QStringLiteral(" — ") +
				       QTStr("Voltz.Presets.Apply"));

	auto *layout = new QVBoxLayout(card);
	layout->setContentsMargins(14, 14, 14, 14);
	layout->setSpacing(8);
	layout->addWidget(new PresetThumbnail(preset, card));
	layout->addWidget(name);
	layout->addWidget(desc, 1);
	layout->addWidget(applyButton, 0, Qt::AlignLeft);

	connect(applyButton, &QPushButton::clicked, this, [this, preset]() { this->apply(preset); });
	return card;
}

void PresetsPage::apply(PresetId preset)
{
	RecordingConfig::applyVoltzDefaults();
	OBSSource scene = applyPreset(preset);
	if (scene) {
		obs_frontend_set_current_scene(scene);
	}
	markFirstRunDone();
	emit presetApplied();
}

void PresetsPage::markFirstRunDone()
{
	config_t *config = App()->GetUserConfig();
	config_set_bool(config, "Voltz", "PresetsDone", true);
	config_save_safe(config, "tmp", nullptr);
	welcome_->hide();
}

} // namespace voltz
```

> `PresetsPage` usa `RecordingConfig::applyVoltzDefaults()`: **implemente nesta task o `RecordingConfig.hpp/.cpp`
> completo da Task 6 Step 1** (copie de lá) e registre-o no CMake; a Task 6 então só cria a `SettingsPage`.

- [ ] **Step 3: Casca usa a página e abre nela na primeira execução**

Em `frontend/voltz/shell/VoltzShell.cpp`:
- `#include "PresetsPage.hpp"`;
- trocar a página provisória de Presets (`buildPlaceholderPage(QTStr("Voltz.Presets.Title"), QTStr("Voltz.Presets.Soon"))`)
  por:
  ```cpp
  	auto *presets = new PresetsPage(pages_);
  	pages_->addWidget(presets);
  ```
  mantendo a ordem das páginas (Gravar=0, Projetos=1, Presets=2, Ajustes=3);
- depois de montar a navegação, conectar:
  ```cpp
  	connect(presets, &PresetsPage::presetApplied, this, [this]() { showPage(0); });
  	connect(presets, &PresetsPage::firstRunDismissed, this, [this]() { showPage(0); });
  	if (PresetsPage::isFirstRun()) {
  		showPage(2);
  	}
  ```
- criar o método privado `void showPage(int index);` que faz `navGroup_->button(index)->setChecked(true); selectPage(index);`
  (declarar no header).

Em `frontend/voltz/shell/voltz-shell.qss`, acrescentar:
```css
#voltzShell QFrame#presetCard {
	background: #111114;
	border: 1px solid #1F1F24;
	border-radius: 10px;
}

#voltzShell QFrame#presetCard:hover {
	border-color: #3A3A42;
}

#voltzShell QFrame#presetCard QWidget {
	background: transparent;
}

#voltzShell QLabel#cardTitle {
	font-size: 15px;
	font-weight: 600;
}

#voltzShell QFrame#welcomeBanner {
	background: rgba(212, 245, 60, 20);
	border: 1px solid rgba(212, 245, 60, 90);
	border-radius: 8px;
}

#voltzShell QPushButton#primaryButton {
	background: #D4F53C;
	color: #09090B;
	border: none;
	font-weight: 600;
}

#voltzShell QPushButton#primaryButton:hover {
	background: #E2FA6A;
}

#voltzShell QScrollArea {
	background: transparent;
	border: none;
}
```

Em `frontend/cmake/feature-voltz.cmake`: acrescentar `PresetThumbnail`, `PresetsPage` (e `RecordingConfig`, se criado agora).

- [ ] **Step 4: Compilar, smoke e verificação real dos presets**

Run: CONFIGURAR, COMPILAR APP, SMOKE.

Verificação (config portátil limpa; `--portable --multi`):
1. Ao abrir, a casca mostra a tela **Presets** com a faixa de boas-vindas (captura só da janela).
2. Com `tools/voltz-uia.ps1` (Task 2) acione "Tutorial — Aplicar" (accessibleName). Se UIA não funcionar, use
   o websocket para conferir o efeito depois de clicar manualmente não é possível — então registre e siga só com
   a parte que der para verificar.
3. Depois de aplicar: a casca volta para Gravar; `tools/voltz-ws.ps1` → `GetSceneList` contém "Tutorial";
   `GetSceneItemList` de "Tutorial" contém "Tela" e o grupo "Câmera redonda" (`isGroup: true`);
   `GetSourceFilterList` do grupo contém "Máscara redonda" (`mask_filter_v2`).
4. Aplique também "Só tela / Só câmera", "Lado a lado" e "Abertura / Pausa"; confira as cenas e itens.
5. Capture a janela com a cena Tutorial ativa e com "Lado a lado" ativa. Descreva: posição da câmera (círculo
   no canto inferior direito se houver câmera; se não houver dispositivo de vídeo, o grupo fica vazio — registre),
   tela à esquerda/câmera à direita no Lado a lado, título centralizado na Abertura.
6. Reabra o app com a MESMA config: a tela inicial é Gravar (não Presets); as cenas persistem.

- [ ] **Step 5: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): tela Presets com miniaturas e primeira execucao

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Tela Projetos

**Files:**
- Create: `frontend/voltz/shell/ProjectsPage.hpp/.cpp`
- Modify: `frontend/voltz/shell/VoltzShell.cpp`, `frontend/voltz/shell/voltz-shell.qss`, `frontend/cmake/feature-voltz.cmake`

- [ ] **Step 1: Página**

`frontend/voltz/shell/ProjectsPage.hpp`:
```cpp
#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTreeWidget;

namespace voltz {

class FrontendBridge;

// Tela "Projetos": gravações na pasta de saída atual (mais recentes primeiro), com abrir, mostrar na pasta e
// mover para a lixeira. Atualiza ao aparecer e ao terminar uma gravação. (No Plano 3 vira lista de projetos.)
class ProjectsPage : public QWidget {
	Q_OBJECT

public:
	explicit ProjectsPage(FrontendBridge *bridge, QWidget *parent = nullptr);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void refresh();
	QString selectedPath() const;
	void updateButtons();
	void openSelected();
	void showSelectedInFolder();
	void trashSelected();

	QString folder_;
	QLabel *folderLabel_ = nullptr;
	QLabel *empty_ = nullptr;
	QTreeWidget *list_ = nullptr;
	QPushButton *openButton_ = nullptr;
	QPushButton *showButton_ = nullptr;
	QPushButton *trashButton_ = nullptr;
};

} // namespace voltz
```

`frontend/voltz/shell/ProjectsPage.cpp`:
```cpp
#include "ProjectsPage.hpp"

#include "FrontendBridge.hpp"
#include "Icons.hpp"

#include <voltz/core/recording-files.hpp>

#include <OBSApp.hpp>
#include <widgets/OBSBasic.hpp>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "moc_ProjectsPage.cpp"

namespace voltz {

namespace {

constexpr int kPathRole = Qt::UserRole;
constexpr int kSortRole = Qt::UserRole + 1;

class SortableItem : public QTreeWidgetItem {
public:
	using QTreeWidgetItem::QTreeWidgetItem;
	bool operator<(const QTreeWidgetItem &other) const override
	{
		const int column = treeWidget() ? treeWidget()->sortColumn() : 0;
		const QVariant a = data(column, kSortRole);
		const QVariant b = other.data(column, kSortRole);
		if (a.isValid() && b.isValid()) {
			return a.toLongLong() < b.toLongLong();
		}
		return QTreeWidgetItem::operator<(other);
	}
};

} // namespace

ProjectsPage::ProjectsPage(FrontendBridge *bridge, QWidget *parent) : QWidget(parent)
{
	auto *heading = new QLabel(QTStr("Voltz.Projects.Title"), this);
	heading->setObjectName("pageTitle");
	folderLabel_ = new QLabel(this);
	folderLabel_->setObjectName("muted");
	folderLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

	auto *openFolder = new QPushButton(QTStr("Voltz.Projects.OpenFolder"), this);
	openFolder->setIcon(phosphorIcon(QStringLiteral("folder-simple"), palette::kText));
	auto *refreshButton = new QPushButton(QTStr("Voltz.Projects.Refresh"), this);

	auto *header = new QHBoxLayout();
	header->addWidget(folderLabel_, 1);
	header->addWidget(refreshButton);
	header->addWidget(openFolder);

	list_ = new QTreeWidget(this);
	list_->setObjectName("projectsList");
	list_->setColumnCount(3);
	list_->setHeaderLabels({QTStr("Voltz.Projects.Column.Name"), QTStr("Voltz.Projects.Column.Date"),
				QTStr("Voltz.Projects.Column.Size")});
	list_->setRootIsDecorated(false);
	list_->setUniformRowHeights(true);
	list_->setSortingEnabled(true);
	list_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	list_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	list_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

	empty_ = new QLabel(QTStr("Voltz.Projects.Empty"), this);
	empty_->setObjectName("muted");
	empty_->setAlignment(Qt::AlignCenter);

	openButton_ = new QPushButton(QTStr("Voltz.Projects.Open"), this);
	showButton_ = new QPushButton(QTStr("Voltz.Projects.ShowInFolder"), this);
	trashButton_ = new QPushButton(QTStr("Voltz.Projects.Delete"), this);
	trashButton_->setIcon(phosphorIcon(QStringLiteral("trash"), palette::kText));

	auto *actions = new QHBoxLayout();
	actions->addWidget(openButton_);
	actions->addWidget(showButton_);
	actions->addStretch(1);
	actions->addWidget(trashButton_);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(32, 28, 32, 28);
	root->setSpacing(10);
	root->addWidget(heading);
	root->addLayout(header);
	root->addWidget(empty_, 1);
	root->addWidget(list_, 1);
	root->addLayout(actions);

	connect(refreshButton, &QPushButton::clicked, this, &ProjectsPage::refresh);
	connect(openFolder, &QPushButton::clicked, this,
		[this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(folder_)); });
	connect(openButton_, &QPushButton::clicked, this, &ProjectsPage::openSelected);
	connect(showButton_, &QPushButton::clicked, this, &ProjectsPage::showSelectedInFolder);
	connect(trashButton_, &QPushButton::clicked, this, &ProjectsPage::trashSelected);
	connect(list_, &QTreeWidget::itemDoubleClicked, this, [this]() { openSelected(); });
	connect(list_, &QTreeWidget::itemSelectionChanged, this, &ProjectsPage::updateButtons);
	connect(bridge, &FrontendBridge::recordingStateChanged, this, [this](bool active, bool) {
		if (!active && isVisible()) {
			refresh();
		}
	});

	refresh();
}

void ProjectsPage::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	refresh();
}

void ProjectsPage::refresh()
{
	const char *outputPath = OBSBasic::Get()->GetCurrentOutputPath();
	folder_ = QString::fromUtf8(outputPath ? outputPath : "");
	folderLabel_->setText(QTStr("Voltz.Projects.Folder").arg(QDir::toNativeSeparators(folder_)));

	const QString selected = selectedPath();
	list_->setSortingEnabled(false);
	list_->clear();

	const QChar separator = QLocale().decimalPoint().at(0);
	const QFileInfoList files = QDir(folder_).entryInfoList(QDir::Files, QDir::Time);
	for (const QFileInfo &file : files) {
		if (!isRecordingFile(file.fileName().toStdString())) {
			continue;
		}
		auto *item = new SortableItem(list_);
		item->setText(0, file.fileName());
		item->setIcon(0, phosphorIcon(QStringLiteral("film-strip"), palette::kMuted));
		item->setData(0, kPathRole, file.absoluteFilePath());
		const QDateTime modified = file.lastModified();
		item->setText(1, QLocale().toString(modified, QLocale::ShortFormat));
		item->setData(1, kSortRole, modified.toMSecsSinceEpoch());
		item->setText(2, QString::fromStdString(formatFileSize(uint64_t(file.size()), separator.toLatin1())));
		item->setData(2, kSortRole, file.size());
		item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
		if (file.absoluteFilePath() == selected) {
			list_->setCurrentItem(item);
		}
	}
	list_->setSortingEnabled(true);
	list_->sortByColumn(1, Qt::DescendingOrder);

	const bool hasFiles = list_->topLevelItemCount() > 0;
	empty_->setVisible(!hasFiles);
	list_->setVisible(hasFiles);
	updateButtons();
}

QString ProjectsPage::selectedPath() const
{
	QTreeWidgetItem *item = list_->currentItem();
	return item && item->isSelected() ? item->data(0, kPathRole).toString() : QString();
}

void ProjectsPage::updateButtons()
{
	const bool hasSelection = !selectedPath().isEmpty();
	openButton_->setEnabled(hasSelection);
	showButton_->setEnabled(hasSelection);
	trashButton_->setEnabled(hasSelection);
}

void ProjectsPage::openSelected()
{
	const QString path = selectedPath();
	if (!path.isEmpty()) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
	}
}

void ProjectsPage::showSelectedInFolder()
{
	const QString path = selectedPath();
	if (path.isEmpty()) {
		return;
	}
#ifdef _WIN32
	QProcess::startDetached(QStringLiteral("explorer.exe"),
				{QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#else
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void ProjectsPage::trashSelected()
{
	const QString path = selectedPath();
	if (path.isEmpty()) {
		return;
	}
	const QString name = QFileInfo(path).fileName();
	if (QMessageBox::question(this, QTStr("Voltz.Projects.DeleteConfirm.Title"),
				  QTStr("Voltz.Projects.DeleteConfirm.Text").arg(name.toHtmlEscaped())) != QMessageBox::Yes) {
		return;
	}
	if (!QFile::moveToTrash(path)) {
		QMessageBox::warning(this, QTStr("Voltz.Projects.DeleteConfirm.Title"), QTStr("Voltz.Projects.DeleteFailed"));
	}
	refresh();
}

} // namespace voltz
```

> Observações: `film-strip` foi vendorizado no 2A (confira em `icons/regular`). `OBSBasic::GetCurrentOutputPath()`
> é público e devolve `const char *`. Ler a pasta durante a gravação é seguro.

- [ ] **Step 2: Casca e estilo**

Em `VoltzShell.cpp`: trocar a página provisória de Projetos por `pages_->addWidget(new ProjectsPage(bridge_, pages_));`
(mesma posição 1) e `#include "ProjectsPage.hpp"`. Se `buildPlaceholderPage` deixar de ter uso depois das Tasks
4–7, remova-o na Task 7.

Em `voltz-shell.qss`, acrescentar:
```css
#voltzShell QTreeWidget#projectsList {
	background: #111114;
	border: 1px solid #1F1F24;
	border-radius: 8px;
	outline: none;
}

#voltzShell QTreeWidget#projectsList::item {
	padding: 6px 4px;
	color: #F2F2F3;
}

#voltzShell QTreeWidget#projectsList::item:selected {
	background: rgba(212, 245, 60, 26);
	color: #F2F2F3;
}

#voltzShell QHeaderView::section {
	background: #111114;
	color: #82828C;
	border: none;
	border-bottom: 1px solid #1F1F24;
	padding: 6px 4px;
	font-size: 11px;
}
```

Em `feature-voltz.cmake`: `ProjectsPage.cpp/.hpp`.

- [ ] **Step 3: Compilar, smoke, verificação**

Run: CONFIGURAR, COMPILAR APP, SMOKE.
Verificação: rode `tools/voltz-smoke-record.ps1` **modificado só para esta verificação?** Não — ele apaga o
arquivo. Em vez disso: com config portátil, aponte a gravação para uma pasta de teste em
`$env:TEMP\voltz-proj-test` (escreva `SimpleOutput/FilePath` no `basic.ini` do perfil portátil antes de abrir:
`config\Voltz Studio\basic\profiles\<Perfil>\basic.ini`, seção `[SimpleOutput]`; descubra o nome do perfil
abrindo o app uma vez), crie lá 2 arquivos falsos (`a.mp4` de 1 KB, `b.mkv` de 2 MB, e `c.txt`), abra o app,
acione "Projetos" (UIA) e capture a janela: devem aparecer `b.mkv` e `a.mp4` (não o `.txt`), com data e
tamanho, mais recentes primeiro. Acione "Mover para a lixeira" não é necessário automatizar (diálogo modal).
Apague a pasta de teste ao final.

- [ ] **Step 4: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): tela Projetos com gravacoes da pasta de saida (abrir, mostrar na pasta, lixeira)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Configuração de gravação (`RecordingConfig`) e Ajustes › Gravação

**Files:**
- Create (ou completar): `frontend/voltz/shell/RecordingConfig.hpp/.cpp`
- Create: `frontend/voltz/shell/SettingsPage.hpp/.cpp`
- Modify: `frontend/voltz/shell/VoltzShell.cpp`, `frontend/voltz/shell/voltz-shell.qss`, `frontend/cmake/feature-voltz.cmake`

- [ ] **Step 1: `RecordingConfig`**

`frontend/voltz/shell/RecordingConfig.hpp`:
```cpp
#pragma once

#include <voltz/core/recording-settings.hpp>

#include <QString>

namespace voltz {

// Lê/escreve as configurações de gravação do perfil do OBS (modo de saída Simples).
class RecordingConfig {
public:
	static bool isSimpleMode();
	static QString folder();
	static void setFolder(const QString &path);
	static RecordingFormat format();
	static void setFormat(RecordingFormat format);
	static RecordingQuality quality();
	static void setQuality(RecordingQuality quality);
	// RecEncoder escolhido automaticamente (AMD → NVIDIA → Intel → x264) entre os encoders registrados.
	static std::string automaticEncoder();
	static std::string currentEncoder();

	// Primeira execução/preset: se RecQuality ainda for o padrão do OBS ("Stream"), passa a "HQ" com o
	// codificador automático e formato MP4 híbrido. Não sobrescreve escolhas já feitas.
	static void applyVoltzDefaults();

private:
	// Salva o perfil e, se não estiver gravando, recria as saídas para valer já.
	static void commit(bool needsOutputReset);
};

} // namespace voltz
```

`frontend/voltz/shell/RecordingConfig.cpp`:
```cpp
#include "RecordingConfig.hpp"

#include <widgets/OBSBasic.hpp>

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <cstring>
#include <string>
#include <vector>

namespace voltz {

namespace {

config_t *profile()
{
	return OBSBasic::Get()->Config();
}

std::string getString(const char *section, const char *key)
{
	const char *value = config_get_string(profile(), section, key);
	return value ? value : "";
}

} // namespace

bool RecordingConfig::isSimpleMode()
{
	return getString("Output", "Mode") != "Advanced";
}

QString RecordingConfig::folder()
{
	return QString::fromStdString(getString("SimpleOutput", "FilePath"));
}

void RecordingConfig::setFolder(const QString &path)
{
	config_set_string(profile(), "SimpleOutput", "FilePath", path.toUtf8().constData());
	commit(false);
}

RecordingFormat RecordingConfig::format()
{
	return formatFromObs(getString("SimpleOutput", "RecFormat2"));
}

void RecordingConfig::setFormat(RecordingFormat format)
{
	config_set_string(profile(), "SimpleOutput", "RecFormat2", obsRecFormat(format).c_str());
	commit(false);
}

RecordingQuality RecordingConfig::quality()
{
	return qualityFromObs(getString("SimpleOutput", "RecQuality"));
}

void RecordingConfig::setQuality(RecordingQuality quality)
{
	config_set_string(profile(), "SimpleOutput", "RecQuality", obsRecQuality(quality).c_str());
	config_set_string(profile(), "SimpleOutput", "RecEncoder", automaticEncoder().c_str());
	commit(true);
}

std::string RecordingConfig::automaticEncoder()
{
	std::vector<std::string> ids;
	const char *id = nullptr;
	for (size_t i = 0; obs_enum_encoder_types(i, &id); i++) {
		if (id) {
			ids.emplace_back(id);
		}
	}
	return pickRecEncoder(ids);
}

std::string RecordingConfig::currentEncoder()
{
	return getString("SimpleOutput", "RecEncoder");
}

void RecordingConfig::applyVoltzDefaults()
{
	if (!isSimpleMode() || getString("SimpleOutput", "RecQuality") != "Stream") {
		return;
	}
	config_set_string(profile(), "SimpleOutput", "RecQuality", "HQ");
	config_set_string(profile(), "SimpleOutput", "RecEncoder", automaticEncoder().c_str());
	config_set_string(profile(), "SimpleOutput", "RecFormat2", "hybrid_mp4");
	commit(true);
	blog(LOG_INFO, "[voltz] padroes de gravacao aplicados (HQ, %s, hybrid_mp4)", currentEncoder().c_str());
}

void RecordingConfig::commit(bool needsOutputReset)
{
	if (needsOutputReset && !obs_frontend_recording_active() && !obs_frontend_streaming_active() &&
	    !obs_frontend_replay_buffer_active() && !obs_frontend_virtualcam_active()) {
		OBSBasic::Get()->ResetOutputs();
	}
	config_save_safe(profile(), "tmp", nullptr);
}

} // namespace voltz
```

> Confira os nomes `obs_frontend_streaming_active`, `obs_frontend_replay_buffer_active`,
> `obs_frontend_virtualcam_active` em `obs-frontend-api.h` e `OBSBasic::ResetOutputs()` público (Plano 2B
> research: OBSBasic.hpp ~767). Se a Task 4 criou um stub, substitua pelo conteúdo acima.

- [ ] **Step 2: `SettingsPage` (seção Gravação + Sobre; a seção Captura entra na Task 7)**

`frontend/voltz/shell/SettingsPage.hpp`:
```cpp
#pragma once

#include <QWidget>

class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace voltz {

class FrontendBridge;

// Tela "Ajustes": Gravação (pasta, formato, qualidade, codificador automático), Captura (Task 7) e Sobre.
// Aplica cada mudança na hora; trava a seção Gravação enquanto grava.
class SettingsPage : public QWidget {
	Q_OBJECT

public:
	explicit SettingsPage(FrontendBridge *bridge, QWidget *parent = nullptr);

protected:
	void showEvent(QShowEvent *event) override;

private:
	QFrame *buildRecordingSection();
	QFrame *buildAboutSection();
	QFrame *makeSection(const QString &title, QVBoxLayout **body);
	void loadRecording();
	void setRecordingLocked(bool locked);

	FrontendBridge *bridge_;
	QVBoxLayout *sections_ = nullptr;
	QFrame *recording_ = nullptr;
	QLabel *recordingNotice_ = nullptr;
	QLabel *folder_ = nullptr;
	QPushButton *changeFolder_ = nullptr;
	QComboBox *format_ = nullptr;
	QComboBox *quality_ = nullptr;
	QLabel *encoder_ = nullptr;
	bool loading_ = false;
};

} // namespace voltz
```

`frontend/voltz/shell/SettingsPage.cpp`:
```cpp
#include "SettingsPage.hpp"

#include "FrontendBridge.hpp"
#include "RecordingConfig.hpp"

#include <voltz/core/voltz-version.hpp>

#include <OBSApp.hpp>
#include <widgets/OBSBasic.hpp>

#include <obs.h>

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

#include "moc_SettingsPage.cpp"

namespace voltz {

namespace {

QString encoderLabel(const std::string &encoder)
{
	const std::string key = "Voltz.Settings.Encoder." + encoder;
	const QString name = QTStr(key.c_str());
	return QTStr("Voltz.Settings.Encoder.Auto").arg(name == QString::fromStdString(key) ? QString::fromStdString(encoder) : name);
}

} // namespace

SettingsPage::SettingsPage(FrontendBridge *bridge, QWidget *parent) : QWidget(parent), bridge_(bridge)
{
	auto *heading = new QLabel(QTStr("Voltz.Settings.Title"), this);
	heading->setObjectName("pageTitle");

	auto *content = new QWidget(this);
	sections_ = new QVBoxLayout(content);
	sections_->setContentsMargins(32, 28, 32, 28);
	sections_->setSpacing(16);
	sections_->addWidget(heading);
	sections_->addWidget(buildRecordingSection());
	sections_->addWidget(buildAboutSection());
	sections_->addStretch(1);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(content);
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->addWidget(scroll);

	connect(bridge_, &FrontendBridge::recordingStateChanged, this,
		[this](bool active, bool) { setRecordingLocked(active); });
	loadRecording();
}

QFrame *SettingsPage::makeSection(const QString &title, QVBoxLayout **body)
{
	auto *section = new QFrame(this);
	section->setObjectName("panel");
	auto *layout = new QVBoxLayout(section);
	layout->setContentsMargins(16, 14, 16, 16);
	layout->setSpacing(10);
	auto *label = new QLabel(title.toUpper(), section);
	label->setObjectName("panelTitle");
	layout->addWidget(label);
	*body = layout;
	return section;
}

QFrame *SettingsPage::buildRecordingSection()
{
	QVBoxLayout *body = nullptr;
	recording_ = makeSection(QTStr("Voltz.Settings.Recording"), &body);

	recordingNotice_ = new QLabel(recording_);
	recordingNotice_->setObjectName("muted");
	recordingNotice_->setWordWrap(true);
	recordingNotice_->hide();
	body->addWidget(recordingNotice_);

	folder_ = new QLabel(recording_);
	folder_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	changeFolder_ = new QPushButton(QTStr("Voltz.Settings.ChangeFolder"), recording_);
	auto *folderRow = new QHBoxLayout();
	folderRow->addWidget(folder_, 1);
	folderRow->addWidget(changeFolder_);

	format_ = new QComboBox(recording_);
	format_->addItem(QTStr("Voltz.Settings.Format.Mp4"), int(RecordingFormat::Mp4));
	format_->addItem(QTStr("Voltz.Settings.Format.Mkv"), int(RecordingFormat::Mkv));
	format_->addItem(QTStr("Voltz.Settings.Format.Mov"), int(RecordingFormat::Mov));

	quality_ = new QComboBox(recording_);
	quality_->addItem(QTStr("Voltz.Settings.Quality.High"), int(RecordingQuality::High));
	quality_->addItem(QTStr("Voltz.Settings.Quality.Compact"), int(RecordingQuality::Compact));

	encoder_ = new QLabel(recording_);
	encoder_->setObjectName("muted");

	auto *form = new QFormLayout();
	form->setLabelAlignment(Qt::AlignLeft);
	form->setHorizontalSpacing(16);
	form->setVerticalSpacing(10);
	form->addRow(QTStr("Voltz.Settings.Folder"), folderRow);
	form->addRow(QTStr("Voltz.Settings.Format"), format_);
	form->addRow(QTStr("Voltz.Settings.Quality"), quality_);
	form->addRow(QTStr("Voltz.Settings.Encoder"), encoder_);
	body->addLayout(form);

	connect(changeFolder_, &QPushButton::clicked, this, [this]() {
		const QString chosen = QFileDialog::getExistingDirectory(this, QTStr("Voltz.Settings.Folder"),
									RecordingConfig::folder());
		if (!chosen.isEmpty()) {
			RecordingConfig::setFolder(chosen);
			loadRecording();
		}
	});
	connect(format_, &QComboBox::currentIndexChanged, this, [this]() {
		if (!loading_) {
			RecordingConfig::setFormat(RecordingFormat(format_->currentData().toInt()));
		}
	});
	connect(quality_, &QComboBox::currentIndexChanged, this, [this]() {
		if (!loading_) {
			RecordingConfig::setQuality(RecordingQuality(quality_->currentData().toInt()));
			loadRecording();
		}
	});
	return recording_;
}

QFrame *SettingsPage::buildAboutSection()
{
	QVBoxLayout *body = nullptr;
	QFrame *section = makeSection(QTStr("Voltz.Settings.About"), &body);

	const VersionInfo info{"Voltz Studio", VOLTZ_VERSION, obs_get_version_string()};
	auto *about = new QLabel(QString::fromStdString(aboutLine(info, QTStr("Voltz.About.BasedOn").toStdString())),
				 section);
	about->setObjectName("muted");
	about->setWordWrap(true);

	auto *classic = new QPushButton(QTStr("Voltz.Settings.OpenClassic"), section);
	connect(classic, &QPushButton::clicked, this, []() {
		OBSBasic *main = OBSBasic::Get();
		QMetaObject::invokeMethod(main, "SetShowing", Q_ARG(bool, true));
		main->raise();
		main->activateWindow();
	});
	auto *logs = new QPushButton(QTStr("Voltz.Settings.OpenLogs"), section);
	connect(logs, &QPushButton::clicked, this, []() {
		char path[512];
		if (GetAppConfigPath(path, sizeof(path), VOLTZ_CONFIG_DIR "/logs") > 0) {
			QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromUtf8(path)));
		}
	});

	auto *buttons = new QHBoxLayout();
	buttons->addWidget(classic);
	buttons->addWidget(logs);
	buttons->addStretch(1);
	body->addWidget(about);
	body->addLayout(buttons);
	return section;
}

void SettingsPage::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	loadRecording();
}

void SettingsPage::loadRecording()
{
	loading_ = true;
	const bool simple = RecordingConfig::isSimpleMode();
	folder_->setText(QDir::toNativeSeparators(RecordingConfig::folder()));
	format_->setCurrentIndex(format_->findData(int(RecordingConfig::format())));
	quality_->setCurrentIndex(quality_->findData(int(RecordingConfig::quality())));
	encoder_->setText(encoderLabel(RecordingConfig::currentEncoder()));
	loading_ = false;

	if (!simple) {
		recordingNotice_->setText(QTStr("Voltz.Settings.AdvancedMode"));
		recordingNotice_->show();
	}
	setRecordingLocked(bridge_->isRecording());
}

void SettingsPage::setRecordingLocked(bool locked)
{
	const bool editable = RecordingConfig::isSimpleMode() && !locked;
	changeFolder_->setEnabled(editable);
	format_->setEnabled(editable);
	quality_->setEnabled(editable);
	if (locked) {
		recordingNotice_->setText(QTStr("Voltz.Settings.LockedWhileRecording"));
		recordingNotice_->show();
	} else if (RecordingConfig::isSimpleMode()) {
		recordingNotice_->hide();
	}
}

} // namespace voltz
```

> `GetAppConfigPath` vem de `<OBSApp.hpp>` (confira; senão inclua o header que o declara). `VOLTZ_CONFIG_DIR`
> é definido pelo CMake para o alvo obs-studio. O `encoderLabel` com fallback é deliberado: se a chave não
> existir, o `QTStr` devolve a própria chave — então mostramos o id bruto.

- [ ] **Step 3: Casca**

Em `VoltzShell.cpp`: trocar a página de Ajustes atual (`buildSettingsPage()`) por
`pages_->addWidget(new SettingsPage(bridge_, pages_));` (posição 3) e `#include "SettingsPage.hpp"`. Remover
`buildSettingsPage` (e `buildPlaceholderPage`, se não sobrar uso) do `.hpp/.cpp`. O botão "Abrir modo clássico"
agora vive na seção Sobre.

Em `voltz-shell.qss`, acrescentar:
```css
#voltzShell QComboBox {
	background: #18181C;
	color: #F2F2F3;
	border: 1px solid #1F1F24;
	border-radius: 6px;
	padding: 5px 10px;
	min-height: 18px;
	max-height: 16777215px;
}

#voltzShell QComboBox:hover {
	border-color: #3A3A42;
}

#voltzShell QComboBox:disabled {
	color: #4A4A52;
}

#voltzShell QComboBox QAbstractItemView {
	background: #111114;
	color: #F2F2F3;
	border: 1px solid #1F1F24;
	selection-background-color: rgba(212, 245, 60, 40);
}
```

Em `feature-voltz.cmake`: `RecordingConfig.cpp/.hpp`, `SettingsPage.cpp/.hpp`.

- [ ] **Step 4: Compilar, smoke, verificação**

Run: CONFIGURAR, COMPILAR APP, SMOKE, `tools/voltz-smoke-record.ps1`.
Verificação com config portátil limpa:
1. Aplicar um preset (UIA) → log contém `[voltz] padroes de gravacao aplicados (HQ, amd, hybrid_mp4)` (nesta
   máquina AMD) e o `basic.ini` do perfil tem `RecQuality=HQ`, `RecEncoder=amd`, `RecFormat2=hybrid_mp4`.
2. Abrir Ajustes (UIA "Ajustes") e capturar a janela: seção GRAVAÇÃO com pasta, "MP4 (recomendado)", "Alta",
   "AMD (automático)"; seção SOBRE com a linha de versão e os botões.
3. `tools/voltz-smoke-record.ps1` continua `SMOKE GRAVACAO OK` (o smoke usa config própria — ok).

- [ ] **Step 5: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): Ajustes de gravacao (pasta, formato, qualidade, codificador automatico) e Sobre

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Ajustes › Captura (monitor, câmera, microfone)

**Files:**
- Create: `frontend/voltz/shell/DeviceSelector.hpp/.cpp`
- Modify: `frontend/voltz/shell/SettingsPage.hpp/.cpp`, `frontend/cmake/feature-voltz.cmake`

- [ ] **Step 1: `DeviceSelector`**

`frontend/voltz/shell/DeviceSelector.hpp`:
```cpp
#pragma once

#include <obs.hpp>

#include <QComboBox>

#include <functional>

namespace voltz {

// Combobox com as opções de uma propriedade de lista de uma fonte (ex.: "monitor_id", "video_device_id",
// "device_id"). Trocar a opção atualiza a fonte (obs_source_update) e salva a coleção.
class DeviceSelector : public QComboBox {
	Q_OBJECT

public:
	// `resolveSource` devolve a fonte atual (ref forte) a cada recarga — ela pode ser recriada pelo OBS.
	DeviceSelector(std::function<OBSSource()> resolveSource, const char *property, QWidget *parent = nullptr);

	// Recarrega opções e valor atual. Sem fonte: desabilita.
	void reload();

private:
	std::function<OBSSource()> resolveSource_;
	const char *property_;
	bool loading_ = false;
};

} // namespace voltz
```

`frontend/voltz/shell/DeviceSelector.cpp`:
```cpp
#include "DeviceSelector.hpp"

#include <obs-frontend-api.h>

#include "moc_DeviceSelector.cpp"

namespace voltz {

DeviceSelector::DeviceSelector(std::function<OBSSource()> resolveSource, const char *property, QWidget *parent)
	: QComboBox(parent),
	  resolveSource_(std::move(resolveSource)),
	  property_(property)
{
	connect(this, &QComboBox::currentIndexChanged, this, [this]() {
		if (loading_) {
			return;
		}
		OBSSource source = resolveSource_();
		if (!source) {
			return;
		}
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, property_, currentData().toString().toUtf8().constData());
		obs_source_update(source, settings);
		obs_frontend_save();
	});
	reload();
}

void DeviceSelector::reload()
{
	loading_ = true;
	clear();
	OBSSource source = resolveSource_();
	setEnabled(bool(source));
	if (source) {
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		const QString current = QString::fromUtf8(obs_data_get_string(settings, property_));
		obs_properties_t *props = obs_source_properties(source);
		if (obs_property_t *p = props ? obs_properties_get(props, property_) : nullptr) {
			const size_t count = obs_property_list_item_count(p);
			for (size_t i = 0; i < count; i++) {
				if (obs_property_list_item_disabled(p, i)) {
					continue;
				}
				const char *value = obs_property_list_item_string(p, i);
				addItem(QString::fromUtf8(obs_property_list_item_name(p, i)),
					QString::fromUtf8(value ? value : ""));
			}
		}
		obs_properties_destroy(props);
		const int index = findData(current);
		setCurrentIndex(index >= 0 ? index : -1);
	}
	loading_ = false;
}

} // namespace voltz
```

- [ ] **Step 2: Seção Captura**

Em `SettingsPage.hpp`: declarar `QFrame *buildCaptureSection();`, membros `class DeviceSelector;` (forward) e
`DeviceSelector *monitor_ = nullptr; DeviceSelector *camera_ = nullptr; DeviceSelector *microphone_ = nullptr;`,
`QLabel *captureHint_ = nullptr;`.

Em `SettingsPage.cpp`:
- includes `"DeviceSelector.hpp"`, `"SharedSources.hpp"`;
- no construtor, inserir `sections_->addWidget(buildCaptureSection());` entre Gravação e Sobre;
- implementar:
```cpp
QFrame *SettingsPage::buildCaptureSection()
{
	QVBoxLayout *body = nullptr;
	QFrame *section = makeSection(QTStr("Voltz.Settings.Capture"), &body);

	monitor_ = new DeviceSelector([]() { return findSharedSource(SharedSource::Screen); }, "monitor_id", section);
	camera_ = new DeviceSelector([]() { return findSharedSource(SharedSource::Camera); }, "video_device_id",
				     section);
	microphone_ = new DeviceSelector(
		[]() {
			OBSSourceAutoRelease mic = obs_get_output_source(3);
			return OBSSource(mic.Get());
		},
		"device_id", section);

	captureHint_ = new QLabel(QTStr("Voltz.Settings.NoSource"), section);
	captureHint_->setObjectName("muted");
	captureHint_->setWordWrap(true);

	auto *form = new QFormLayout();
	form->setHorizontalSpacing(16);
	form->setVerticalSpacing(10);
	form->addRow(QTStr("Voltz.Settings.Monitor"), monitor_);
	form->addRow(QTStr("Voltz.Settings.Camera"), camera_);
	form->addRow(QTStr("Voltz.Settings.Microphone"), microphone_);
	body->addLayout(form);
	body->addWidget(captureHint_);
	return section;
}
```
- em `showEvent`, depois de `loadRecording();`:
```cpp
	monitor_->reload();
	camera_->reload();
	microphone_->reload();
	captureHint_->setVisible(!monitor_->isEnabled() || !camera_->isEnabled());
```

Em `feature-voltz.cmake`: `DeviceSelector.cpp/.hpp`.

- [ ] **Step 3: Compilar, smoke, verificação**

Run: CONFIGURAR, COMPILAR APP, SMOKE.
Verificação (config portátil limpa):
1. Sem preset aplicado: Ajustes → Monitor e Câmera desabilitados com a dica "Aplique um preset…"; Microfone
   habilitado (se houver dispositivo) mostrando "Padrão"/"Default".
2. Aplicar "Tutorial" (UIA), voltar a Ajustes: Monitor lista os monitores com o atual selecionado; Câmera lista
   as câmeras (ou fica vazia se não houver — registrar).
3. Capturar a janela da tela Ajustes e descrever.
4. Trocar o monitor via websocket não é necessário; basta conferir que a seleção atual bate com
   `GetInputSettings` da fonte "Tela" (`monitor_id`).

- [ ] **Step 4: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): Ajustes de captura (monitor, camera e microfone) por propriedades das fontes

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Verificação, registro e memória

**Files:** `docs/voltz/backlog-plano-2.md`, `docs/voltz/upstream-patches.md` (só se algum arquivo upstream mudou),
`CLAUDE.md`, `AGENTS.md`

- [ ] **Step 1:** COMPILAR TUDO, RODAR TESTES (todos passam; informe o total de casos), SMOKE,
  `tools/voltz-smoke-record.ps1`.
- [ ] **Step 2:** Captura (só da janela) das telas Presets, Projetos e Ajustes com config portátil e preset
  Tutorial aplicado; descrever cada uma no relatório.
- [ ] **Step 3:** `git diff e9f142521..HEAD --stat` → confirmar que nenhum arquivo upstream do OBS mudou neste plano
  (se mudou, registrar em `upstream-patches.md`).
- [ ] **Step 4:** Backlog: nova seção `## Feito no Plano 2B` (Presets/primeira execução, Projetos, Ajustes de
  gravação e captura, padrões de gravação Voltz). Acrescentar em "Testes manuais pendentes": mover para a lixeira
  pela tela Projetos; trocar câmera/monitor em Ajustes; checar a câmera redonda com uma webcam real.
- [ ] **Step 5:** `CLAUDE.md`: Estado atual (+1 linha do 2B); Próximos passos item 1 → "Escrever e executar o
  Plano 2C (tema `.ovt` do clássico, bandeja, confirmação de saída própria, diálogo de falha, acabamento do Áudio,
  grupos no painel de Fontes)"; Decisões (+1 linha): "Presets: fontes compartilhadas (Tela/Câmera/Fundo) por UUID
  em user.ini [Voltz]; câmera redonda = grupo 'Câmera redonda' + mask_filter_v2 com `data/voltz/mask-circle.png`;
  grupo 'Tela Voltz' adiado para o Plano 4."; Armadilhas (+1 linha cada, se aplicável ao que surgiu na execução).
  `cp CLAUDE.md AGENTS.md`.
- [ ] **Step 6:** Commit `docs(voltz): registro do Plano 2B (backlog, memoria)` + trailer e `git push`.
