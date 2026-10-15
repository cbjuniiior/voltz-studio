# Voltz Studio — Plano 1: Fundação — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transformar o build do OBS 32.2.2 em `VoltzStudio.exe` com identidade própria, config separada, sem
conexões com obsproject.com, infraestrutura de testes, e um **protótipo da casca Voltz** (janela nova com
prévia, lista de cenas e gravar/pausar) controlando o `OBSBasic` oculto — validando o risco #1 da spec.

**Architecture:** Código Voltz novo em `frontend/voltz/` (`core/` sem Qt + testes doctest; `shell/` Qt).
Toda alteração em arquivo do OBS fica dentro de `#ifdef VOLTZ_STUDIO` (ou em CMake/preset) e é listada em
`docs/voltz/upstream-patches.md`. A casca fala com o OBS apenas pela frontend API (`obs-frontend-api.h`) e
pelo `OBSBasic::Get()` para o modo clássico.

**Tech Stack:** C++17, Qt 6 (do obs-deps), libobs 32.2.2, CMake 3.31 (VS 2022 Build Tools), MSBuild,
doctest 2.4.11, PowerShell 5.1 (smoke test).

**Spec:** `docs/superpowers/specs/2026-10-15-voltz-studio-base-gravacao-design.md`

---

## Roteiro dos planos (etapas 1+2 da spec)

| Plano | Conteúdo | Depende de |
|---|---|---|
| **1 — Fundação (este)** | preset de build, testes, identidade, config, phone-home off, câmera virtual própria, protótipo da casca | — |
| 2 — Casca completa | layout H2 final, fontes/áudio, Projetos/Presets/Ajustes, presets de fábrica, tema `.ovt`, DM Sans, locale, bandeja | 1 |
| 3 — Gravação em projeto | manifesto, ISO via `obs_canvas`, cursor, marcadores/capítulos, recuperação, encoders | 1 (UI em 2) |
| 4 — Anotações + mouse | overlay `WDA_EXCLUDEFROMCAPTURE`, barra, fontes `Voltz Anotações`/`Voltz Mouse` | 2, 3 |
| 5 — Marca visual + instalador + CI | logo SVG/ICO, ícones, Inno Setup, Windows Sandbox, workflow próprio | 1 |

Os planos 2–5 são escritos depois deste, aproveitando o que o protótipo revelar.

---

## Convenções deste plano

**Diretório de trabalho:** `C:\Users\Cassio\Documents\Projetos IA\voltz-studio` (branch `voltz`).

**Comandos (Git Bash).** O estado do shell não persiste entre comandos — defina as variáveis no mesmo comando:

```bash
CMAKE="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
CTEST="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe"
```

- **CONFIGURAR:** `"$CMAKE" --preset voltz-windows-x64`
- **COMPILAR TUDO:** `"$CMAKE" --build --preset voltz-windows-x64 --parallel -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
- **COMPILAR SÓ OS TESTES:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target voltz-core-tests -- -v:minimal -nologo -nodeReuse:false`
- **RODAR TESTES:** `"$CTEST" --test-dir build_x64/frontend/voltz/tests -C RelWithDebInfo --output-on-failure`
- **SMOKE:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1` (criado na Task 4)

> ⚠️ No Git Bash, flags do MSBuild com `/` viram caminho: use sempre `-v:minimal`, nunca `/v:minimal`.
> Compilação completa leva ~20–40 min na primeira vez; incrementais, poucos minutos.

**Commits:** terminar toda mensagem com a linha `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

---

## Mapa de arquivos

**Novos**
| Arquivo | Responsabilidade |
|---|---|
| `frontend/cmake/feature-voltz.cmake` | liga o código Voltz ao alvo `obs-studio` (defines, libs, fontes) |
| `frontend/voltz/core/CMakeLists.txt` | biblioteca estática `voltz-core` (sem Qt) |
| `frontend/voltz/core/voltz-version.hpp/.cpp` | identidade: `VersionInfo`, `aboutLine`, `windowTitle`, URLs |
| `frontend/voltz/core/preview-fit.hpp/.cpp` | encaixe centralizado da prévia (`fitCentered`) |
| `frontend/voltz/core/recording-clock.hpp/.cpp` | relógio de gravação com pausas (`RecordingClock`) |
| `frontend/voltz/core/time-format.hpp/.cpp` | `formatClock(ms)` → `HH:MM:SS` |
| `frontend/voltz/tests/CMakeLists.txt` + `test-*.cpp` | testes doctest via CTest |
| `frontend/voltz/tests/third_party/doctest/` | doctest 2.4.11 (MIT) |
| `frontend/voltz/shell/FrontendBridge.hpp/.cpp` | eventos/comandos da frontend API → sinais Qt |
| `frontend/voltz/shell/VoltzPreview.hpp/.cpp` | prévia (`OBSQTDisplay` + draw callback) |
| `frontend/voltz/shell/RecordPage.hpp/.cpp` | tela Gravar (cenas, prévia, barra gravar/pausar) |
| `frontend/voltz/shell/VoltzShell.hpp/.cpp` | janela principal Voltz (navegação lateral + páginas) |
| `frontend/voltz/shell/voltz-shell.qss`, `voltz-shell.qrc` | tema da casca |
| `tools/voltz-smoke.ps1` | smoke test automatizado |
| `docs/voltz/upstream-patches.md` | registro de toda alteração em arquivo do OBS |

**Modificados (upstream)**
`CMakePresets.json`, `.gitignore`, `cmake/common/bootstrap.cmake`, `frontend/CMakeLists.txt`,
`frontend/cmake/os-windows.cmake`, `frontend/cmake/windows/obs.rc.in`, `frontend/obs-main.cpp`,
`frontend/OBSApp.hpp`, `frontend/OBSApp.cpp`, `frontend/widgets/OBSBasic.cpp`,
`frontend/widgets/OBSBasic_MainControls.cpp`, `frontend/dialogs/OBSAbout.cpp`,
`frontend/utility/CrashHandler.cpp`, `frontend/utility/platform-windows.cpp`, 14 arquivos com caminho
`"obs-studio/..."` (Task 4), `plugins/win-dshow/virtualcam-module/virtualcam-filter.cpp`,
`plugins/win-dshow/virtualcam-module/virtualcam-module.cpp`, `plugins/win-dshow/shared-memory-queue.c`,
`shared/obs-shared-memory-queue/shared-memory-queue.c`.

---

### Task 1: Preset de build versionado do Voltz

**Files:**
- Modify: `CMakePresets.json` (configurePresets após `windows-x64`; buildPresets no início)
- Delete: `CMakeUserPresets.json` (preset local provisório, não versionado)

- [ ] **Step 1: Adicionar o configure preset**

Em `CMakePresets.json`, logo depois do bloco cujo `"name": "windows-x64"` (o que termina com
`"ENABLE_BROWSER": true\n      }\n    },`), inserir:

```json
    {
      "name": "voltz-windows-x64",
      "displayName": "Voltz Studio x64 (VS 2022)",
      "description": "Build do Voltz Studio (x64) com Visual Studio 2022",
      "inherits": ["windows-x64"],
      "generator": "Visual Studio 17 2022",
      "cacheVariables": {
        "VIRTUALCAM_GUID": {"type": "STRING", "value": "EBB0E657-2F91-4DE0-BF14-BD030B7BF5D7"},
        "ENABLE_AJA": false,
        "ENABLE_DECKLINK": false,
        "ENABLE_WHATSNEW": false,
        "ENABLE_SERVICE_UPDATES": false,
        "ENABLE_COMPAT_UPDATES": false
      }
    },
```

- [ ] **Step 2: Adicionar o build preset**

Em `"buildPresets": [`, inserir como primeiro item:

```json
    {
      "name": "voltz-windows-x64",
      "configurePreset": "voltz-windows-x64",
      "displayName": "Voltz Studio x64",
      "description": "Build do Voltz Studio (x64)",
      "configuration": "RelWithDebInfo"
    },
```

- [ ] **Step 3: Remover o preset local e validar o JSON**

```bash
rm -f CMakeUserPresets.json && python -c "import json;json.load(open('CMakePresets.json'));print('json ok')"
```
Expected: `json ok`

- [ ] **Step 4: Configurar**

Run: CONFIGURAR
Expected: termina com `-- Build files have been written to: .../build_x64`; a seção `Disabled Modules` lista
`aja`, `decklink` (e `obs-libfdk`, `test-input`).

- [ ] **Step 5: Conferir o cache**

```bash
grep -E "^(ENABLE_AJA|ENABLE_DECKLINK|ENABLE_WHATSNEW|ENABLE_SERVICE_UPDATES|ENABLE_COMPAT_UPDATES|VIRTUALCAM_GUID)" build_x64/CMakeCache.txt
```
Expected: os cinco `ENABLE_*` com `OFF` (ou `FALSE`) e `VIRTUALCAM_GUID:STRING=EBB0E657-2F91-4DE0-BF14-BD030B7BF5D7`.

- [ ] **Step 6: Compilar**

Run: COMPILAR TUDO
Expected: `exit=0` e nenhuma linha de erro.

- [ ] **Step 7: Commit**

```bash
git add CMakePresets.json
git commit -m "build(voltz): preset voltz-windows-x64 (VS 2022, sem AJA/Decklink/phone-home, GUID proprio da camera virtual)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Biblioteca `voltz-core` + testes (doctest)

**Files:**
- Create: `frontend/cmake/feature-voltz.cmake`
- Create: `frontend/voltz/core/CMakeLists.txt`, `voltz-version.hpp/.cpp`, `preview-fit.hpp/.cpp`,
  `recording-clock.hpp/.cpp`, `time-format.hpp/.cpp`
- Create: `frontend/voltz/tests/CMakeLists.txt`, `test-main.cpp`, `test-version.cpp`, `test-preview-fit.cpp`,
  `test-recording-clock.cpp`, `test-time-format.cpp`
- Create: `frontend/voltz/tests/third_party/doctest/doctest.h`, `LICENSE.txt`
- Modify: `frontend/CMakeLists.txt` (incluir `feature-voltz.cmake` após `include(cmake/feature-idian-playground.cmake)`)

- [ ] **Step 1: Baixar o doctest**

```bash
mkdir -p frontend/voltz/tests/third_party/doctest && \
curl -fsSL -o frontend/voltz/tests/third_party/doctest/doctest.h https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h && \
curl -fsSL -o frontend/voltz/tests/third_party/doctest/LICENSE.txt https://raw.githubusercontent.com/doctest/doctest/v2.4.11/LICENSE.txt && \
grep -m1 "DOCTEST_VERSION_STR" frontend/voltz/tests/third_party/doctest/doctest.h
```
Expected: `#define DOCTEST_VERSION_STR "2.4.11"`

- [ ] **Step 2: Escrever os testes (falham — nada implementado)**

`frontend/voltz/tests/test-main.cpp`:
```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
```

`frontend/voltz/tests/test-version.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/voltz-version.hpp>

TEST_CASE("aboutLine credita o OBS Studio e a licença")
{
	const voltz::VersionInfo info{"Voltz Studio", "1.0.0", "32.2.2"};
	CHECK(voltz::aboutLine(info) == "Voltz Studio 1.0.0 · baseado no OBS Studio 32.2.2 (GPL-2.0)");
}

TEST_CASE("windowTitle sem projeto mostra produto e versão")
{
	const voltz::VersionInfo info{"Voltz Studio", "1.0.0", "32.2.2"};
	CHECK(voltz::windowTitle(info, "") == "Voltz Studio 1.0.0");
}

TEST_CASE("windowTitle com projeto prefixa o nome do projeto")
{
	const voltz::VersionInfo info{"Voltz Studio", "1.0.0", "32.2.2"};
	CHECK(voltz::windowTitle(info, "Aula 03") == "Aula 03 — Voltz Studio 1.0.0");
}
```

`frontend/voltz/tests/test-preview-fit.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/preview-fit.hpp>

TEST_CASE("fitCentered: mesma proporção ocupa a janela inteira")
{
	const voltz::FitRect r = voltz::fitCentered(1920, 1080, 1280, 720);
	CHECK(r.x == 0);
	CHECK(r.y == 0);
	CHECK(r.cx == 1280);
	CHECK(r.cy == 720);
	CHECK(r.scale == doctest::Approx(1280.0 / 1920.0));
}

TEST_CASE("fitCentered: janela mais larga centraliza na horizontal")
{
	const voltz::FitRect r = voltz::fitCentered(1920, 1080, 1600, 720);
	CHECK(r.x == 160);
	CHECK(r.y == 0);
	CHECK(r.cx == 1280);
	CHECK(r.cy == 720);
}

TEST_CASE("fitCentered: janela mais alta centraliza na vertical")
{
	const voltz::FitRect r = voltz::fitCentered(1920, 1080, 1280, 1000);
	CHECK(r.x == 0);
	CHECK(r.y == 140);
	CHECK(r.cx == 1280);
	CHECK(r.cy == 720);
}

TEST_CASE("fitCentered: margem é respeitada em todas as bordas")
{
	const voltz::FitRect r = voltz::fitCentered(1920, 1080, 1300, 740, 10);
	CHECK(r.x == 10);
	CHECK(r.y == 10);
	CHECK(r.cx == 1280);
	CHECK(r.cy == 720);
}

TEST_CASE("fitCentered: entradas inválidas retornam tamanho zero")
{
	CHECK(voltz::fitCentered(0, 1080, 1280, 720).cx == 0);
	CHECK(voltz::fitCentered(1920, 1080, 0, 720).cx == 0);
	CHECK(voltz::fitCentered(1920, 1080, 15, 15, 10).cx == 0);
}
```

`frontend/voltz/tests/test-recording-clock.cpp`:
```cpp
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
```

`frontend/voltz/tests/test-time-format.cpp`:
```cpp
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
```

`frontend/voltz/tests/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.28...3.30)

enable_testing()

add_executable(voltz-core-tests)

target_sources(
  voltz-core-tests
  PRIVATE test-main.cpp test-preview-fit.cpp test-recording-clock.cpp test-time-format.cpp test-version.cpp
)

target_include_directories(voltz-core-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest")
target_link_libraries(voltz-core-tests PRIVATE Voltz::core)
set_target_properties(voltz-core-tests PROPERTIES FOLDER frontend/voltz)

add_test(NAME voltz-core-tests COMMAND voltz-core-tests)
```

- [ ] **Step 3: Escrever os headers e o CMake da biblioteca (sem implementação)**

`frontend/voltz/core/voltz-version.hpp`:
```cpp
#pragma once

#include <string>

namespace voltz {

inline constexpr const char *kProjectUrl = "https://github.com/cbjuniiior/voltz-studio";
inline constexpr const char *kIssuesUrl = "https://github.com/cbjuniiior/voltz-studio/issues";

struct VersionInfo {
	std::string product;        // "Voltz Studio"
	std::string productVersion; // "1.0.0"
	std::string engineVersion;  // obs_get_version_string(), ex.: "32.2.2"
};

// "Voltz Studio 1.0.0 · baseado no OBS Studio 32.2.2 (GPL-2.0)"
std::string aboutLine(const VersionInfo &info);

// "Voltz Studio 1.0.0" ou, com projeto, "Aula 03 — Voltz Studio 1.0.0"
std::string windowTitle(const VersionInfo &info, const std::string &projectName);

} // namespace voltz
```

`frontend/voltz/core/preview-fit.hpp`:
```cpp
#pragma once

namespace voltz {

struct FitRect {
	int x = 0;
	int y = 0;
	int cx = 0;
	int cy = 0;
	float scale = 0.0f;
};

// Encaixa uma imagem base (baseCX x baseCY) centralizada numa janela (windowCX x windowCY),
// mantendo a proporção e deixando `margin` px livres em cada borda.
// Retorna um FitRect de tamanho zero se alguma dimensão for inválida.
FitRect fitCentered(int baseCX, int baseCY, int windowCX, int windowCY, int margin = 0);

} // namespace voltz
```

`frontend/voltz/core/recording-clock.hpp`:
```cpp
#pragma once

#include <cstdint>

namespace voltz {

// Relógio de gravação que desconta pausas. Todos os tempos em ms de um relógio monotônico
// fornecido por quem chama (não lê o relógio do sistema — testável).
class RecordingClock {
public:
	void start(int64_t nowMs);
	void pause(int64_t nowMs);
	void resume(int64_t nowMs);
	void stop();

	bool running() const { return running_; }
	bool paused() const { return paused_; }

	int64_t elapsedMs(int64_t nowMs) const;

private:
	bool running_ = false;
	bool paused_ = false;
	int64_t startMs_ = 0;
	int64_t pausedTotalMs_ = 0;
	int64_t pausedAtMs_ = 0;
};

} // namespace voltz
```

`frontend/voltz/core/time-format.hpp`:
```cpp
#pragma once

#include <cstdint>
#include <string>

namespace voltz {

// 3725000 -> "01:02:05". Horas sem limite de dígitos (mínimo 2). Negativo vira "00:00:00".
std::string formatClock(int64_t milliseconds);

} // namespace voltz
```

`frontend/voltz/core/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.28...3.30)

add_library(voltz-core STATIC)
add_library(Voltz::core ALIAS voltz-core)

target_sources(
  voltz-core
  PRIVATE preview-fit.cpp recording-clock.cpp time-format.cpp voltz-version.cpp
  PUBLIC preview-fit.hpp recording-clock.hpp time-format.hpp voltz-version.hpp
)

# Inclui a partir de frontend/ para usar <voltz/core/...>
target_include_directories(voltz-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_compile_features(voltz-core PUBLIC cxx_std_17)
set_target_properties(voltz-core PROPERTIES FOLDER frontend/voltz)
```

Criar os `.cpp` vazios só com o include do próprio header (para o CMake configurar):

```bash
for f in preview-fit recording-clock time-format voltz-version; do
  printf '#include "%s.hpp"\n' "$f" > "frontend/voltz/core/$f.cpp"
done
```

`frontend/cmake/feature-voltz.cmake`:
```cmake
# Voltz Studio — código próprio ligado ao alvo obs-studio

option(ENABLE_VOLTZ_TESTS "Build Voltz Studio unit tests" ON)

add_subdirectory(voltz/core)

if(ENABLE_VOLTZ_TESTS)
  add_subdirectory(voltz/tests)
endif()

target_link_libraries(obs-studio PRIVATE Voltz::core)
```

Em `frontend/CMakeLists.txt`, trocar:
```cmake
include(cmake/feature-idian-playground.cmake)
```
por:
```cmake
include(cmake/feature-idian-playground.cmake)
include(cmake/feature-voltz.cmake)
```

- [ ] **Step 4: Configurar e compilar os testes — devem FALHAR no link**

Run: CONFIGURAR, depois COMPILAR SÓ OS TESTES
Expected: falha de link `LNK2019`/`LNK2001` com `unresolved external symbol` para `voltz::aboutLine`,
`voltz::fitCentered`, `voltz::RecordingClock::start`, `voltz::formatClock` etc.

- [ ] **Step 5: Implementar**

`frontend/voltz/core/voltz-version.cpp`:
```cpp
#include "voltz-version.hpp"

namespace voltz {

std::string aboutLine(const VersionInfo &info)
{
	return info.product + " " + info.productVersion + " · baseado no OBS Studio " + info.engineVersion +
	       " (GPL-2.0)";
}

std::string windowTitle(const VersionInfo &info, const std::string &projectName)
{
	const std::string base = info.product + " " + info.productVersion;
	if (projectName.empty())
		return base;
	return projectName + " — " + base;
}

} // namespace voltz
```

`frontend/voltz/core/preview-fit.cpp`:
```cpp
#include "preview-fit.hpp"

namespace voltz {

FitRect fitCentered(int baseCX, int baseCY, int windowCX, int windowCY, int margin)
{
	const int availCX = windowCX - 2 * margin;
	const int availCY = windowCY - 2 * margin;
	if (baseCX <= 0 || baseCY <= 0 || availCX <= 0 || availCY <= 0)
		return {};

	const double baseAspect = double(baseCX) / double(baseCY);
	const double availAspect = double(availCX) / double(availCY);

	FitRect r;
	if (availAspect > baseAspect) {
		r.cy = availCY;
		r.cx = int(double(availCY) * baseAspect + 0.5);
	} else {
		r.cx = availCX;
		r.cy = int(double(availCX) / baseAspect + 0.5);
	}
	r.scale = float(r.cx) / float(baseCX);
	r.x = margin + (availCX - r.cx) / 2;
	r.y = margin + (availCY - r.cy) / 2;
	return r;
}

} // namespace voltz
```

`frontend/voltz/core/recording-clock.cpp`:
```cpp
#include "recording-clock.hpp"

namespace voltz {

void RecordingClock::start(int64_t nowMs)
{
	running_ = true;
	paused_ = false;
	startMs_ = nowMs;
	pausedTotalMs_ = 0;
	pausedAtMs_ = 0;
}

void RecordingClock::pause(int64_t nowMs)
{
	if (!running_ || paused_)
		return;
	paused_ = true;
	pausedAtMs_ = nowMs;
}

void RecordingClock::resume(int64_t nowMs)
{
	if (!running_ || !paused_)
		return;
	pausedTotalMs_ += nowMs - pausedAtMs_;
	paused_ = false;
}

void RecordingClock::stop()
{
	running_ = false;
	paused_ = false;
}

int64_t RecordingClock::elapsedMs(int64_t nowMs) const
{
	if (!running_)
		return 0;
	const int64_t end = paused_ ? pausedAtMs_ : nowMs;
	const int64_t elapsed = end - startMs_ - pausedTotalMs_;
	return elapsed > 0 ? elapsed : 0;
}

} // namespace voltz
```

`frontend/voltz/core/time-format.cpp`:
```cpp
#include "time-format.hpp"

#include <cstdio>

namespace voltz {

std::string formatClock(int64_t milliseconds)
{
	if (milliseconds < 0)
		milliseconds = 0;
	const int64_t totalSeconds = milliseconds / 1000;
	const long long hours = static_cast<long long>(totalSeconds / 3600);
	const int minutes = static_cast<int>((totalSeconds / 60) % 60);
	const int seconds = static_cast<int>(totalSeconds % 60);

	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%02lld:%02d:%02d", hours, minutes, seconds);
	return buffer;
}

} // namespace voltz
```

- [ ] **Step 6: Compilar e rodar os testes — devem PASSAR**

Run: COMPILAR SÓ OS TESTES, depois RODAR TESTES
Expected: `100% tests passed, 0 tests failed out of 1`. (Para ver cada caso:
`build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe` → `[doctest] test cases: 16 | 16 passed`.)

- [ ] **Step 7: Compilar tudo (garante que o obs-studio linka com voltz-core)**

Run: COMPILAR TUDO
Expected: `exit=0`.

- [ ] **Step 8: Commit**

```bash
git add frontend/cmake/feature-voltz.cmake frontend/CMakeLists.txt frontend/voltz/core frontend/voltz/tests
git commit -m "feat(voltz): biblioteca voltz-core (versao, encaixe da previa, relogio de gravacao, formato de tempo) com testes doctest

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Identidade — nome, versão, executável

**Files:**
- Modify: `cmake/common/bootstrap.cmake:44-48`
- Modify: `frontend/cmake/feature-voltz.cmake`
- Modify: `frontend/cmake/windows/obs.rc.in`
- Modify: `frontend/CMakeLists.txt:115`
- Modify: `plugins/win-dshow/virtualcam-module/virtualcam-filter.cpp:35`

- [ ] **Step 1: Variáveis globais de marca**

Em `cmake/common/bootstrap.cmake`, trocar:
```cmake
set(OBS_COMPANY_NAME "OBS Project")
set(OBS_PRODUCT_NAME "OBS Studio")
set(OBS_WEBSITE "https://www.obsproject.com")
set(OBS_COMMENTS "Free and open source software for video recording and live streaming")
set(OBS_LEGAL_COPYRIGHT "(C) Lain Bailey")
```
por:
```cmake
# Voltz Studio: identidade do produto (o motor continua sendo libobs/OBS Studio, GPL-2.0)
set(OBS_COMPANY_NAME "VoltzPixel")
set(OBS_PRODUCT_NAME "Voltz Studio")
set(OBS_WEBSITE "https://github.com/cbjuniiior/voltz-studio")
set(OBS_COMMENTS "Voltz Studio - gravador de tela e camera, baseado no OBS Studio")
set(OBS_LEGAL_COPYRIGHT "(C) VoltzPixel. Baseado no OBS Studio (C) Lain Bailey")
```

- [ ] **Step 2: Versão do produto e defines**

Substituir o conteúdo de `frontend/cmake/feature-voltz.cmake` por:
```cmake
# Voltz Studio — código próprio ligado ao alvo obs-studio

option(ENABLE_VOLTZ_TESTS "Build Voltz Studio unit tests" ON)

set(VOLTZ_VERSION_MAJOR 1)
set(VOLTZ_VERSION_MINOR 0)
set(VOLTZ_VERSION_PATCH 0)
set(VOLTZ_VERSION "${VOLTZ_VERSION_MAJOR}.${VOLTZ_VERSION_MINOR}.${VOLTZ_VERSION_PATCH}")

add_subdirectory(voltz/core)

if(ENABLE_VOLTZ_TESTS)
  add_subdirectory(voltz/tests)
endif()

target_link_libraries(obs-studio PRIVATE Voltz::core)
target_compile_definitions(obs-studio PRIVATE VOLTZ_STUDIO "VOLTZ_VERSION=\"${VOLTZ_VERSION}\"")
```

- [ ] **Step 3: Recurso de versão do executável**

Substituir o conteúdo de `frontend/cmake/windows/obs.rc.in` por (o ícone continua o do OBS até o Plano 5):
```
IDI_ICON1 ICON DISCARDABLE "cmake/windows/obs-studio.ico"

1 VERSIONINFO
FILEVERSION ${VOLTZ_VERSION_MAJOR},${VOLTZ_VERSION_MINOR},${VOLTZ_VERSION_PATCH},0
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904B0"
    BEGIN
      VALUE "CompanyName", "${OBS_COMPANY_NAME}"
      VALUE "FileDescription", "${OBS_PRODUCT_NAME}"
      VALUE "FileVersion", "${VOLTZ_VERSION}"
      VALUE "InternalName", "VoltzStudio"
      VALUE "OriginalFilename", "VoltzStudio.exe"
      VALUE "ProductName", "${OBS_PRODUCT_NAME}"
      VALUE "ProductVersion", "${VOLTZ_VERSION}"
      VALUE "Comments", "${OBS_COMMENTS}"
      VALUE "LegalCopyright", "${OBS_LEGAL_COPYRIGHT}"
    END
  END

  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x0409, 0x04B0
  END
END
```

- [ ] **Step 4: Nome do executável**

Em `frontend/CMakeLists.txt`, trocar:
```cmake
set_target_properties_obs(obs-studio PROPERTIES FOLDER frontend OUTPUT_NAME "$<IF:$<PLATFORM_ID:Windows>,obs64,obs>")
```
por:
```cmake
set_target_properties_obs(obs-studio PROPERTIES FOLDER frontend OUTPUT_NAME "$<IF:$<PLATFORM_ID:Windows>,VoltzStudio,obs>")
```

- [ ] **Step 5: Câmera virtual reconhece o novo executável**

Em `plugins/win-dshow/virtualcam-module/virtualcam-filter.cpp`, trocar:
```cpp
#ifdef _WIN64
	const wchar_t *obs_process = L"obs64.exe";
#else
```
por:
```cpp
#ifdef _WIN64
	const wchar_t *obs_process = L"VoltzStudio.exe";
#else
```

- [ ] **Step 6: Configurar e compilar**

Run: CONFIGURAR, depois COMPILAR TUDO
Expected: `exit=0`.

- [ ] **Step 7: Verificar o executável e a versão**

```bash
powershell -NoProfile -Command "\$f='build_x64/rundir/RelWithDebInfo/bin/64bit/VoltzStudio.exe'; if(Test-Path \$f){(Get-Item \$f).VersionInfo | Select-Object ProductName,CompanyName,ProductVersion,FileDescription,OriginalFilename | Format-List}else{'NAO EXISTE'}"
```
Expected:
```
ProductName      : Voltz Studio
CompanyName      : VoltzPixel
ProductVersion   : 1.0.0
FileDescription  : Voltz Studio
OriginalFilename : VoltzStudio.exe
```

- [ ] **Step 8: Remover o executável antigo do rundir (sobra do build anterior)**

```bash
rm -f build_x64/rundir/RelWithDebInfo/bin/64bit/obs64.exe build_x64/rundir/RelWithDebInfo/bin/64bit/obs64.pdb
```

- [ ] **Step 9: Commit**

```bash
git add cmake/common/bootstrap.cmake frontend/cmake/feature-voltz.cmake frontend/cmake/windows/obs.rc.in frontend/CMakeLists.txt plugins/win-dshow/virtualcam-module/virtualcam-filter.cpp
git commit -m "feat(voltz): identidade Voltz Studio (VoltzStudio.exe, versao 1.0.0, VoltzPixel)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Pasta de configuração própria, mutexes e smoke test

**Files:**
- Modify: `frontend/cmake/feature-voltz.cmake` (define `VOLTZ_CONFIG_DIR`)
- Modify: 14 arquivos em `frontend/` com literais `"obs-studio/..."` e `"/obs-studio/..."` (lista no Step 4)
- Modify: `frontend/utility/platform-windows.cpp` (mutexes)
- Create: `tools/voltz-smoke.ps1`
- Modify: `.gitignore` (versionar `tools/`)

- [ ] **Step 1: Criar o smoke test**

`tools/voltz-smoke.ps1`:
```powershell
<#
  Smoke test do Voltz Studio.
  Abre o app em modo portatil com configuracao limpa, espera, fecha pela janela principal
  e confere pasta de configuracao, log e encerramento.
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1 [-SkipShell] [-Seconds 20]
#>
param(
    [string]$Config = "RelWithDebInfo",
    [int]$Seconds = 20,
    [switch]$SkipShell
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rundir = Join-Path $root "build_x64\rundir\$Config"
$bin = Join-Path $rundir "bin\64bit"
$exe = Join-Path $bin "VoltzStudio.exe"
if (-not (Test-Path $exe)) { Write-Host "FALHOU executavel nao encontrado: $exe"; exit 1 }

$cfg = Join-Path $rundir "config"
if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }

$proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
Start-Sleep -Seconds $Seconds
$aliveAfterStart = -not $proc.HasExited

$cleanExit = $false
if ($aliveAfterStart) {
    $proc.Refresh()
    [void]$proc.CloseMainWindow()
    $cleanExit = $proc.WaitForExit(20000)
    if (-not $cleanExit) { Stop-Process -Id $proc.Id -Force }
}

$voltzCfg = Join-Path $cfg "Voltz Studio"
$log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime | Select-Object -Last 1

$checks = [ordered]@{}
$checks["processo vivo apos $Seconds s"] = $aliveAfterStart
$checks["pasta de config 'Voltz Studio' criada"] = (Test-Path $voltzCfg)
$checks["pasta 'obs-studio' ausente"] = -not (Test-Path (Join-Path $cfg "obs-studio"))
$checks["log gerado"] = [bool]$log
if (-not $SkipShell) {
    $checks["log contem '[voltz] shell pronta'"] = [bool]($log -and (Select-String -Path $log.FullName -Pattern '\[voltz\] shell pronta' -Quiet))
    $checks["encerrou limpo ao fechar a janela"] = $cleanExit
}

$failed = 0
foreach ($k in $checks.Keys) {
    if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
}
if ($log) { Write-Host "log: $($log.FullName)" }
if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE OK"
```

Em `.gitignore`, no bloco `# Voltz Studio`, acrescentar `!tools`:
```
# Voltz Studio
!AGENTS.md
!CLAUDE.md
!installer
!tools
```

- [ ] **Step 2: Rodar o smoke — deve FALHAR (ainda usa `obs-studio`)**

Run: `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1 -SkipShell`
Expected: `FALHOU pasta de config 'Voltz Studio' criada` e `FALHOU pasta 'obs-studio' ausente`; exit 1.

- [ ] **Step 3: Definir `VOLTZ_CONFIG_DIR`**

Em `frontend/cmake/feature-voltz.cmake`, trocar a última linha:
```cmake
target_compile_definitions(obs-studio PRIVATE VOLTZ_STUDIO "VOLTZ_VERSION=\"${VOLTZ_VERSION}\"")
```
por:
```cmake
target_compile_definitions(
  obs-studio
  PRIVATE VOLTZ_STUDIO "VOLTZ_VERSION=\"${VOLTZ_VERSION}\"" "VOLTZ_CONFIG_DIR=\"Voltz Studio\""
)
```

- [ ] **Step 4: Trocar os literais de caminho**

Os literais `"obs-studio/..."` e `"obs-studio\\..."` viram concatenação `VOLTZ_CONFIG_DIR "/..."`.
Ficam de fora: `frontend/updater/` (não será compilado), `MacUpdateThread.cpp` (macOS),
`GoLiveAPI_PostData.cpp` e `models/multitrack-video.hpp` (são nome de cliente, não caminho).

```bash
files=$(grep -rlE '"obs-studio[/\\]' frontend --include=*.cpp --include=*.hpp | grep -v -e '^frontend/updater/' -e 'MacUpdateThread.cpp')
echo "$files"
sed -i 's/"obs-studio\([/\\]\)/VOLTZ_CONFIG_DIR "\1/g' $files
```
Expected (lista impressa, 12 arquivos):
```
frontend/OBSApp.cpp
frontend/OBSApp_Themes.cpp
frontend/dialogs/OBSLogViewer.cpp
frontend/importer/OBSImporter.cpp
frontend/obs-main.cpp
frontend/plugin-manager/PluginManager.cpp
frontend/utility/AutoUpdateThread.cpp
frontend/utility/CrashHandler.cpp
frontend/utility/CrashHandler_Windows.cpp
frontend/utility/WhatsNewInfoThread.cpp
frontend/widgets/OBSBasic.cpp
frontend/widgets/OBSBasic_MainControls.cpp
```
(A ordem pode variar. Se a lista tiver arquivos diferentes, pare e revise antes de seguir.)

Alguns caminhos de **configuração** começam com barra (`"/obs-studio/..."`) e são concatenados à pasta do
usuário. Trocar só nestes 4 arquivos — **não** usar este sed em `platform-windows.cpp`/`platform-x11.cpp`, onde
`"data/obs-studio/"` e `OBS_DATA_PATH "/obs-studio/"` são a pasta de dados da instalação e devem continuar iguais:
```bash
sed -i 's|"/obs-studio/|"/" VOLTZ_CONFIG_DIR "/|g' frontend/OBSApp.cpp frontend/OBSApp_Themes.cpp frontend/widgets/OBSBasic_Profiles.cpp frontend/widgets/OBSBasic_SceneCollections.cpp
grep -n 'VOLTZ_CONFIG_DIR "/user.ini"\|VOLTZ_CONFIG_DIR "/global.ini"\|VOLTZ_CONFIG_DIR "/themes"\|VOLTZ_CONFIG_DIR "/basic/profiles/"\|VOLTZ_CONFIG_DIR "/basic/scenes/"' frontend/OBSApp.cpp frontend/OBSApp_Themes.cpp frontend/widgets/OBSBasic_Profiles.cpp frontend/widgets/OBSBasic_SceneCollections.cpp | wc -l
```
Expected: `6` (user.ini ×2, global.ini, themes, profiles, scenes).

Conferir que não sobrou nenhum caminho de config (as pastas de dados `data/obs-studio/` continuam, de propósito):
```bash
grep -rnE '"/?obs-studio[/\\]' frontend --include=*.cpp --include=*.hpp | grep -v -e '^frontend/updater/' -e 'MacUpdateThread.cpp' -e 'platform-windows.cpp' -e 'platform-x11.cpp' | wc -l
```
Expected: `0`

- [ ] **Step 5: Mutexes próprios**

Em `frontend/utility/platform-windows.cpp`, trocar:
```cpp
	if (!portable_mode) {
		name = "OBSStudioCore";
```
por:
```cpp
	if (!portable_mode) {
		name = "VoltzStudioCore";
```
e trocar:
```cpp
		name = "OBSStudioPortable";
```
por:
```cpp
		name = "VoltzStudioPortable";
```

- [ ] **Step 6: Configurar, compilar e rodar o smoke — deve PASSAR**

Run: CONFIGURAR, COMPILAR TUDO, depois `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1 -SkipShell`
Expected:
```
OK     processo vivo apos 20 s
OK     pasta de config 'Voltz Studio' criada
OK     pasta 'obs-studio' ausente
OK     log gerado
SMOKE OK
```

- [ ] **Step 7: Commit**

```bash
git add .gitignore tools/voltz-smoke.ps1 frontend
git commit -m "feat(voltz): configuracao em '%APPDATA%/Voltz Studio', mutexes proprios e smoke test

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Sem atualizador e sem envio de dados ao obsproject.com

**Files:**
- Modify: `frontend/obs-main.cpp:75`
- Modify: `frontend/cmake/os-windows.cmake` (`add_subdirectory(updater)`)
- Modify: `frontend/widgets/OBSBasic_MainControls.cpp` (`UploadLog`, links de ajuda)
- Modify: `frontend/utility/CrashHandler.cpp` (`uploadCrashLogToServer`)
- Modify: `frontend/dialogs/OBSAbout.cpp`
- Modify: `frontend/widgets/OBSBasic.cpp` (assistente de primeira execução)

- [ ] **Step 1: Atualizador desligado por padrão**

Em `frontend/obs-main.cpp`, trocar:
```cpp
bool opt_disable_updater = false;
```
por:
```cpp
#ifdef VOLTZ_STUDIO
bool opt_disable_updater = true; /* Voltz: sem o atualizador do OBS */
#else
bool opt_disable_updater = false;
#endif
```

- [ ] **Step 2: Não compilar o `updater.exe`**

Em `frontend/cmake/os-windows.cmake`, trocar:
```cmake
add_subdirectory(updater)
```
por:
```cmake
# Voltz Studio: o atualizador do OBS (obsproject.com) não é compilado nem distribuído
if(NOT VOLTZ_VERSION)
  add_subdirectory(updater)
endif()
```
(`VOLTZ_VERSION` é definido em `feature-voltz.cmake`, incluído antes de `os-windows.cmake`.)

- [ ] **Step 3: Log não é enviado — abre a pasta**

Em `frontend/widgets/OBSBasic_MainControls.cpp`, adicionar no bloco de includes do topo:
```cpp
#ifdef VOLTZ_STUDIO
#include <voltz/core/voltz-version.hpp>
#endif
```

No início de `OBSBasic::UploadLog`, trocar:
```cpp
void OBSBasic::UploadLog(const char *subdir, const char *file, const LogUploadType uploadType)
{
	BPtr<char> fileString{ReadLogFile(subdir, file)};
```
por:
```cpp
void OBSBasic::UploadLog(const char *subdir, const char *file, const LogUploadType uploadType)
{
#ifdef VOLTZ_STUDIO
	/* Voltz: nenhum log sai da máquina — abre a pasta local de logs */
	UNUSED_PARAMETER(file);
	char logDir[512];
	if (GetAppConfigPath(logDir, sizeof(logDir), subdir) > 0) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(QT_UTF8(logDir)));
	}
	emit App()->logUploadFailed(uploadType,
				    QStringLiteral("O envio de logs está desativado no Voltz Studio. A pasta de logs foi aberta."));
	return;
#endif
	BPtr<char> fileString{ReadLogFile(subdir, file)};
```

- [ ] **Step 4: Links de ajuda apontam para o Voltz**

No mesmo arquivo, trocar:
```cpp
void OBSBasic::on_actionHelpPortal_triggered()
{
	QUrl url = QUrl("https://obsproject.com/help", QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
}

void OBSBasic::on_actionWebsite_triggered()
{
	QUrl url = QUrl("https://obsproject.com", QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
}

void OBSBasic::on_actionDiscord_triggered()
{
	QUrl url = QUrl("https://obsproject.com/discord", QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
}
```
por:
```cpp
void OBSBasic::on_actionHelpPortal_triggered()
{
#ifdef VOLTZ_STUDIO
	QUrl url = QUrl(voltz::kProjectUrl, QUrl::TolerantMode);
#else
	QUrl url = QUrl("https://obsproject.com/help", QUrl::TolerantMode);
#endif
	QDesktopServices::openUrl(url);
}

void OBSBasic::on_actionWebsite_triggered()
{
#ifdef VOLTZ_STUDIO
	QUrl url = QUrl(voltz::kProjectUrl, QUrl::TolerantMode);
#else
	QUrl url = QUrl("https://obsproject.com", QUrl::TolerantMode);
#endif
	QDesktopServices::openUrl(url);
}

void OBSBasic::on_actionDiscord_triggered()
{
#ifdef VOLTZ_STUDIO
	QUrl url = QUrl(voltz::kIssuesUrl, QUrl::TolerantMode);
#else
	QUrl url = QUrl("https://obsproject.com/discord", QUrl::TolerantMode);
#endif
	QDesktopServices::openUrl(url);
}
```

E trocar:
```cpp
	QString addr("https://github.com/obsproject/obs-studio/releases");
	QUrl url(QString("%1/%2").arg(addr, obs_get_version_string()), QUrl::TolerantMode);
```
por:
```cpp
#ifdef VOLTZ_STUDIO
	QUrl url(QString("%1/releases").arg(voltz::kProjectUrl), QUrl::TolerantMode);
#else
	QString addr("https://github.com/obsproject/obs-studio/releases");
	QUrl url(QString("%1/%2").arg(addr, obs_get_version_string()), QUrl::TolerantMode);
#endif
```

- [ ] **Step 5: Relatório de falha fica só no disco**

Em `frontend/utility/CrashHandler.cpp`, trocar:
```cpp
void CrashHandler::uploadCrashLogToServer()
{
	std::string crashLogFileContent = getCrashLogFileContent(lastCrashLogFile_);
```
por:
```cpp
void CrashHandler::uploadCrashLogToServer()
{
#ifdef VOLTZ_STUDIO
	/* Voltz: relatórios de falha não são enviados; ficam na pasta crashes */
	blog(LOG_INFO, "[voltz] envio de relatorio de falha desativado");
	return;
#endif
	std::string crashLogFileContent = getCrashLogFileContent(lastCrashLogFile_);
```

- [ ] **Step 6: "Sobre" sem busca remota e com a identidade Voltz**

Em `frontend/dialogs/OBSAbout.cpp`, adicionar no bloco de includes do topo:
```cpp
#ifdef VOLTZ_STUDIO
#include <voltz/core/voltz-version.hpp>
#endif
```

Trocar:
```cpp
	QString ver = obs_get_version_string();

	ui->version->setText(ver + bitness);
```
por:
```cpp
	QString ver = obs_get_version_string();

	ui->version->setText(ver + bitness);
#ifdef VOLTZ_STUDIO
	ui->name->setText(QStringLiteral("Voltz Studio"));
	ui->version->setText(QStringLiteral(VOLTZ_VERSION) + bitness);
#endif
```

Trocar:
```cpp
	OBSBasic *main = OBSBasic::Get();
	if (main->patronJson.empty() && !main->patronJsonThread) {
		RemoteTextThread *thread =
			new RemoteTextThread("https://obsproject.com/patreon/about-box.json", "application/json");
		QObject::connect(thread, &RemoteTextThread::Result, main, &OBSBasic::UpdatePatronJson);
		QObject::connect(thread, &RemoteTextThread::Result, this, &OBSAbout::ShowAbout);
		main->patronJsonThread.reset(thread);
		thread->start();
	} else {
		ShowAbout();
	}
}
```
por:
```cpp
#ifdef VOLTZ_STUDIO
	/* Voltz: sem buscar a lista de apoiadores no obsproject.com */
	ShowAbout();
#else
	OBSBasic *main = OBSBasic::Get();
	if (main->patronJson.empty() && !main->patronJsonThread) {
		RemoteTextThread *thread =
			new RemoteTextThread("https://obsproject.com/patreon/about-box.json", "application/json");
		QObject::connect(thread, &RemoteTextThread::Result, main, &OBSBasic::UpdatePatronJson);
		QObject::connect(thread, &RemoteTextThread::Result, this, &OBSAbout::ShowAbout);
		main->patronJsonThread.reset(thread);
		thread->start();
	} else {
		ShowAbout();
	}
#endif
}
```

Trocar:
```cpp
void OBSAbout::ShowAbout()
{
	OBSBasic *main = OBSBasic::Get();
```
por:
```cpp
void OBSAbout::ShowAbout()
{
#ifdef VOLTZ_STUDIO
	const voltz::VersionInfo info{"Voltz Studio", VOLTZ_VERSION, obs_get_version_string()};
	ui->textBrowser->setHtml(QString::fromStdString(voltz::aboutLine(info)).toHtmlEscaped());
	return;
#endif
	OBSBasic *main = OBSBasic::Get();
```

- [ ] **Step 7: Sem assistente de configuração do OBS na primeira execução**

Em `frontend/widgets/OBSBasic.cpp`, trocar:
```cpp
	if (!first_run && !has_last_version && !Active()) {
		QMetaObject::invokeMethod(this, "on_autoConfigure_triggered", Qt::QueuedConnection);
	}
```
por:
```cpp
#ifdef VOLTZ_STUDIO
	/* Voltz: a primeira execução usa os presets do Voltz (Plano 2), não o assistente do OBS */
	UNUSED_PARAMETER(has_last_version);
#else
	if (!first_run && !has_last_version && !Active()) {
		QMetaObject::invokeMethod(this, "on_autoConfigure_triggered", Qt::QueuedConnection);
	}
#endif
```

- [ ] **Step 8: Configurar, compilar, verificar**

Run: CONFIGURAR, COMPILAR TUDO
Expected: `exit=0`.

```bash
grep -c "obsproject.com" frontend/widgets/OBSBasic_MainControls.cpp frontend/utility/CrashHandler.cpp frontend/dialogs/OBSAbout.cpp
ls build_x64/frontend/updater 2>/dev/null | head -1 ; echo "updater-dir-check-done"
```
Expected: as contagens só correspondem a linhas dentro dos blocos `#else` (upstream preservado); o diretório
`build_x64/frontend/updater` pode existir de builds antigos, mas não deve haver alvo `updater` novo — confirme com:
```bash
grep -c "updater.vcxproj" build_x64/obs-studio.sln
```
Expected: `0`

Run: `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1 -SkipShell`
Expected: `SMOKE OK`.

- [ ] **Step 9: Commit**

```bash
git add frontend
git commit -m "feat(voltz): sem atualizador, sem envio de log/crash, Sobre e links de ajuda do Voltz, sem assistente do OBS

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Câmera virtual própria (convive com a do OBS)

**Files:**
- Modify: `plugins/win-dshow/virtualcam-module/virtualcam-module.cpp:162,186`
- Modify: `plugins/win-dshow/shared-memory-queue.c:5`
- Modify: `shared/obs-shared-memory-queue/shared-memory-queue.c:5`

(O GUID novo já entrou pelo preset na Task 1.)

- [ ] **Step 1: Nome amigável**

Em `plugins/win-dshow/virtualcam-module/virtualcam-module.cpp`, trocar as duas ocorrências de
`L"OBS Virtual Camera"` por `L"Voltz Virtual Camera"`:
```bash
sed -i 's/L"OBS Virtual Camera"/L"Voltz Virtual Camera"/g' plugins/win-dshow/virtualcam-module/virtualcam-module.cpp
grep -c 'L"Voltz Virtual Camera"' plugins/win-dshow/virtualcam-module/virtualcam-module.cpp
```
Expected: `2`

- [ ] **Step 2: Memória compartilhada própria**

```bash
sed -i 's/#define VIDEO_NAME L"OBSVirtualCamVideo"/#define VIDEO_NAME L"VoltzVirtualCamVideo"/' plugins/win-dshow/shared-memory-queue.c shared/obs-shared-memory-queue/shared-memory-queue.c
grep -n "VIDEO_NAME" plugins/win-dshow/shared-memory-queue.c shared/obs-shared-memory-queue/shared-memory-queue.c | head -2
```
Expected: as duas linhas com `L"VoltzVirtualCamVideo"`.

- [ ] **Step 3: Compilar**

Run: COMPILAR TUDO
Expected: `exit=0`.

- [ ] **Step 4: Conferir o GUID gerado**

```bash
grep -rh "EBB0E657" build_x64/plugins/win-dshow/virtualcam-module/ 2>/dev/null | head -3
```
Expected: pelo menos uma linha contendo `EBB0E657` (header `virtualcam-guid.h` / `.def` / `.bat` gerados).

- [ ] **Step 5: Commit**

```bash
git add plugins/win-dshow shared/obs-shared-memory-queue
git commit -m "feat(voltz): camera virtual 'Voltz Virtual Camera' com memoria compartilhada propria

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: `FrontendBridge` — ponte entre a casca e o OBS

**Files:**
- Create: `frontend/voltz/shell/FrontendBridge.hpp`, `frontend/voltz/shell/FrontendBridge.cpp`
- Modify: `frontend/cmake/feature-voltz.cmake` (fontes da casca)

- [ ] **Step 1: Header**

`frontend/voltz/shell/FrontendBridge.hpp`:
```cpp
#pragma once

#include <obs-frontend-api.h>

#include <QObject>
#include <QStringList>

namespace voltz {

// Traduz eventos da frontend API do OBS em sinais Qt e expõe os comandos que a casca usa.
// Vive na thread da UI; os eventos do frontend também chegam nela.
class FrontendBridge : public QObject {
	Q_OBJECT

public:
	explicit FrontendBridge(QObject *parent = nullptr);
	~FrontendBridge() override;

	QStringList sceneNames() const;
	QString currentSceneName() const;
	void setCurrentScene(const QString &name);

	bool isRecording() const;
	bool isRecordingPaused() const;
	void startRecording();
	void stopRecording();
	void setRecordingPaused(bool paused);

signals:
	void sceneListChanged();
	void currentSceneChanged(const QString &name);
	void recordingStateChanged(bool active, bool paused);
	void exiting();

private:
	static void handleEvent(enum obs_frontend_event event, void *data);
	void dispatch(enum obs_frontend_event event);
};

} // namespace voltz
```

- [ ] **Step 2: Implementação**

`frontend/voltz/shell/FrontendBridge.cpp`:
```cpp
#include "FrontendBridge.hpp"

#include <obs.hpp>

#include "moc_FrontendBridge.cpp"

namespace voltz {

FrontendBridge::FrontendBridge(QObject *parent) : QObject(parent)
{
	obs_frontend_add_event_callback(&FrontendBridge::handleEvent, this);
}

FrontendBridge::~FrontendBridge()
{
	obs_frontend_remove_event_callback(&FrontendBridge::handleEvent, this);
}

QStringList FrontendBridge::sceneNames() const
{
	QStringList names;
	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		names << QString::fromUtf8(obs_source_get_name(scenes.sources.array[i]));
	}
	obs_frontend_source_list_free(&scenes);
	return names;
}

QString FrontendBridge::currentSceneName() const
{
	OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
	return scene ? QString::fromUtf8(obs_source_get_name(scene)) : QString();
}

void FrontendBridge::setCurrentScene(const QString &name)
{
	OBSSourceAutoRelease scene = obs_get_source_by_name(name.toUtf8().constData());
	if (scene)
		obs_frontend_set_current_scene(scene);
}

bool FrontendBridge::isRecording() const
{
	return obs_frontend_recording_active();
}

bool FrontendBridge::isRecordingPaused() const
{
	return obs_frontend_recording_paused();
}

void FrontendBridge::startRecording()
{
	obs_frontend_recording_start();
}

void FrontendBridge::stopRecording()
{
	obs_frontend_recording_stop();
}

void FrontendBridge::setRecordingPaused(bool paused)
{
	obs_frontend_recording_pause(paused);
}

void FrontendBridge::handleEvent(enum obs_frontend_event event, void *data)
{
	static_cast<FrontendBridge *>(data)->dispatch(event);
}

void FrontendBridge::dispatch(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		emit sceneListChanged();
		emit currentSceneChanged(currentSceneName());
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		emit currentSceneChanged(currentSceneName());
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		emit recordingStateChanged(true, false);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		emit recordingStateChanged(true, true);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		emit recordingStateChanged(false, false);
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		emit exiting();
		break;
	default:
		break;
	}
}

} // namespace voltz
```

- [ ] **Step 3: Registrar no CMake**

Em `frontend/cmake/feature-voltz.cmake`, acrescentar ao final:
```cmake
target_sources(obs-studio PRIVATE voltz/shell/FrontendBridge.cpp voltz/shell/FrontendBridge.hpp)
```

- [ ] **Step 4: Configurar e compilar**

Run: CONFIGURAR, COMPILAR TUDO
Expected: `exit=0` (a classe ainda não é usada; o objetivo é compilar e passar pelo moc).

- [ ] **Step 5: Commit**

```bash
git add frontend/voltz/shell/FrontendBridge.hpp frontend/voltz/shell/FrontendBridge.cpp frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): FrontendBridge (cenas, gravacao e eventos do OBS como sinais Qt)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: `VoltzPreview` — prévia do programa

**Files:**
- Create: `frontend/voltz/shell/VoltzPreview.hpp`, `frontend/voltz/shell/VoltzPreview.cpp`
- Modify: `frontend/cmake/feature-voltz.cmake`

- [ ] **Step 1: Header**

`frontend/voltz/shell/VoltzPreview.hpp`:
```cpp
#pragma once

#include <widgets/OBSQTDisplay.hpp>

#include <cstdint>

namespace voltz {

// Mostra a saída principal do OBS (o que será gravado), centralizada e sem distorção.
class VoltzPreview : public OBSQTDisplay {
	Q_OBJECT

public:
	explicit VoltzPreview(QWidget *parent = nullptr);
	~VoltzPreview() override;

private:
	static void draw(void *data, uint32_t cx, uint32_t cy);
};

} // namespace voltz
```

- [ ] **Step 2: Implementação**

`frontend/voltz/shell/VoltzPreview.cpp`:
```cpp
#include "VoltzPreview.hpp"

#include <voltz/core/preview-fit.hpp>

#include <obs.hpp>

#include <QColor>

#include "moc_VoltzPreview.cpp"

namespace voltz {

VoltzPreview::VoltzPreview(QWidget *parent) : OBSQTDisplay(parent)
{
	SetDisplayBackgroundColor(QColor(0x09, 0x09, 0x0B));
	connect(this, &OBSQTDisplay::DisplayCreated, this, [this](OBSQTDisplay *) {
		obs_display_add_draw_callback(GetDisplay(), &VoltzPreview::draw, this);
	});
}

VoltzPreview::~VoltzPreview()
{
	if (obs_display_t *display = GetDisplay())
		obs_display_remove_draw_callback(display, &VoltzPreview::draw, this);
}

void VoltzPreview::draw(void *, uint32_t cx, uint32_t cy)
{
	obs_video_info ovi;
	if (!obs_get_video_info(&ovi))
		return;

	const FitRect r = fitCentered(int(ovi.base_width), int(ovi.base_height), int(cx), int(cy));
	if (r.cx <= 0 || r.cy <= 0)
		return;

	gs_viewport_push();
	gs_projection_push();

	gs_ortho(0.0f, float(ovi.base_width), 0.0f, float(ovi.base_height), -100.0f, 100.0f);
	gs_set_viewport(r.x, r.y, r.cx, r.cy);
	obs_render_main_texture_src_color_only();
	gs_load_vertexbuffer(nullptr);

	gs_projection_pop();
	gs_viewport_pop();
}

} // namespace voltz
```

- [ ] **Step 3: Registrar no CMake**

Em `frontend/cmake/feature-voltz.cmake`, trocar:
```cmake
target_sources(obs-studio PRIVATE voltz/shell/FrontendBridge.cpp voltz/shell/FrontendBridge.hpp)
```
por:
```cmake
target_sources(
  obs-studio
  PRIVATE
    voltz/shell/FrontendBridge.cpp
    voltz/shell/FrontendBridge.hpp
    voltz/shell/VoltzPreview.cpp
    voltz/shell/VoltzPreview.hpp
)
```

- [ ] **Step 4: Configurar e compilar**

Run: CONFIGURAR, COMPILAR TUDO
Expected: `exit=0`.

- [ ] **Step 5: Commit**

```bash
git add frontend/voltz/shell/VoltzPreview.hpp frontend/voltz/shell/VoltzPreview.cpp frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): VoltzPreview (previa do programa via OBSQTDisplay + fitCentered)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: `RecordPage` — tela Gravar (protótipo)

**Files:**
- Create: `frontend/voltz/shell/RecordPage.hpp`, `frontend/voltz/shell/RecordPage.cpp`
- Modify: `frontend/cmake/feature-voltz.cmake`

- [ ] **Step 1: Header**

`frontend/voltz/shell/RecordPage.hpp`:
```cpp
#pragma once

#include <voltz/core/recording-clock.hpp>

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

namespace voltz {

class FrontendBridge;
class VoltzPreview;

// Tela "Gravar": cenas | prévia | painel lateral, e barra inferior com Pausar/Gravar/tempo.
class RecordPage : public QWidget {
	Q_OBJECT

public:
	explicit RecordPage(FrontendBridge *bridge, QWidget *parent = nullptr);

private:
	void refreshScenes();
	void selectScene(const QString &name);
	void updateRecordingUi(bool active, bool paused);
	void tickClock();

	FrontendBridge *bridge_;
	QListWidget *scenes_ = nullptr;
	VoltzPreview *preview_ = nullptr;
	QPushButton *pauseButton_ = nullptr;
	QPushButton *recordButton_ = nullptr;
	QLabel *timeLabel_ = nullptr;

	QTimer ticker_;
	QElapsedTimer monotonic_;
	RecordingClock recordingClock_;
	bool updatingList_ = false;
};

} // namespace voltz
```

- [ ] **Step 2: Implementação**

`frontend/voltz/shell/RecordPage.cpp`:
```cpp
#include "RecordPage.hpp"

#include "FrontendBridge.hpp"
#include "VoltzPreview.hpp"

#include <voltz/core/time-format.hpp>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include "moc_RecordPage.cpp"

namespace voltz {

namespace {

QFrame *makePanel(const QString &title, QWidget *content, QWidget *parent)
{
	auto *panel = new QFrame(parent);
	panel->setObjectName("panel");
	auto *layout = new QVBoxLayout(panel);
	layout->setContentsMargins(10, 10, 10, 10);
	layout->setSpacing(8);
	auto *label = new QLabel(title, panel);
	label->setObjectName("panelTitle");
	layout->addWidget(label);
	layout->addWidget(content, 1);
	return panel;
}

} // namespace

RecordPage::RecordPage(FrontendBridge *bridge, QWidget *parent) : QWidget(parent), bridge_(bridge)
{
	monotonic_.start();

	scenes_ = new QListWidget(this);
	preview_ = new VoltzPreview(this);

	auto *sourcesHint = new QLabel(QStringLiteral("Fontes e áudio chegam no próximo plano."), this);
	sourcesHint->setObjectName("muted");
	sourcesHint->setWordWrap(true);
	sourcesHint->setAlignment(Qt::AlignTop | Qt::AlignLeft);

	QFrame *scenesPanel = makePanel(QStringLiteral("CENAS"), scenes_, this);
	scenesPanel->setFixedWidth(200);
	QFrame *sidePanel = makePanel(QStringLiteral("FONTES"), sourcesHint, this);
	sidePanel->setFixedWidth(260);

	auto *body = new QHBoxLayout();
	body->setContentsMargins(12, 12, 12, 12);
	body->setSpacing(12);
	body->addWidget(scenesPanel);
	body->addWidget(preview_, 1);
	body->addWidget(sidePanel);

	pauseButton_ = new QPushButton(this);
	recordButton_ = new QPushButton(this);
	recordButton_->setObjectName("recordButton");
	timeLabel_ = new QLabel(QString::fromStdString(formatClock(0)), this);
	timeLabel_->setObjectName("timeLabel");

	auto *bottomBar = new QFrame(this);
	bottomBar->setObjectName("bottomBar");
	bottomBar->setFixedHeight(60);
	auto *bar = new QHBoxLayout(bottomBar);
	bar->setContentsMargins(16, 0, 16, 0);
	bar->setSpacing(12);
	bar->addStretch(1);
	bar->addWidget(pauseButton_);
	bar->addWidget(recordButton_);
	bar->addWidget(timeLabel_);
	bar->addStretch(1);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);
	root->addLayout(body, 1);
	root->addWidget(bottomBar);

	ticker_.setInterval(250);
	connect(&ticker_, &QTimer::timeout, this, &RecordPage::tickClock);

	connect(scenes_, &QListWidget::currentTextChanged, this, [this](const QString &name) {
		if (!updatingList_ && !name.isEmpty())
			bridge_->setCurrentScene(name);
	});
	connect(recordButton_, &QPushButton::clicked, this, [this]() {
		if (bridge_->isRecording())
			bridge_->stopRecording();
		else
			bridge_->startRecording();
	});
	connect(pauseButton_, &QPushButton::clicked, this,
		[this]() { bridge_->setRecordingPaused(!bridge_->isRecordingPaused()); });

	connect(bridge_, &FrontendBridge::sceneListChanged, this, &RecordPage::refreshScenes);
	connect(bridge_, &FrontendBridge::currentSceneChanged, this, &RecordPage::selectScene);
	connect(bridge_, &FrontendBridge::recordingStateChanged, this, &RecordPage::updateRecordingUi);

	refreshScenes();
	updateRecordingUi(bridge_->isRecording(), bridge_->isRecordingPaused());
}

void RecordPage::refreshScenes()
{
	updatingList_ = true;
	scenes_->clear();
	scenes_->addItems(bridge_->sceneNames());
	updatingList_ = false;
	selectScene(bridge_->currentSceneName());
}

void RecordPage::selectScene(const QString &name)
{
	updatingList_ = true;
	const QList<QListWidgetItem *> matches = scenes_->findItems(name, Qt::MatchExactly);
	if (!matches.isEmpty())
		scenes_->setCurrentItem(matches.first());
	updatingList_ = false;
}

void RecordPage::updateRecordingUi(bool active, bool paused)
{
	const int64_t now = monotonic_.elapsed();
	if (active && !recordingClock_.running())
		recordingClock_.start(now);
	if (active && paused)
		recordingClock_.pause(now);
	if (active && !paused)
		recordingClock_.resume(now);
	if (!active)
		recordingClock_.stop();

	if (active)
		ticker_.start();
	else
		ticker_.stop();

	recordButton_->setText(active ? QStringLiteral("■  Parar") : QStringLiteral("●  Gravar"));
	recordButton_->setProperty("recording", active);
	recordButton_->style()->unpolish(recordButton_);
	recordButton_->style()->polish(recordButton_);

	pauseButton_->setEnabled(active);
	pauseButton_->setText(active && paused ? QStringLiteral("▶  Retomar") : QStringLiteral("⏸  Pausar"));

	tickClock();
}

void RecordPage::tickClock()
{
	timeLabel_->setText(QString::fromStdString(formatClock(recordingClock_.elapsedMs(monotonic_.elapsed()))));
}

} // namespace voltz
```

- [ ] **Step 3: Registrar no CMake**

Em `frontend/cmake/feature-voltz.cmake`, trocar o bloco `target_sources(obs-studio ...)` por:
```cmake
target_sources(
  obs-studio
  PRIVATE
    voltz/shell/FrontendBridge.cpp
    voltz/shell/FrontendBridge.hpp
    voltz/shell/RecordPage.cpp
    voltz/shell/RecordPage.hpp
    voltz/shell/VoltzPreview.cpp
    voltz/shell/VoltzPreview.hpp
)
```

- [ ] **Step 4: Configurar e compilar**

Run: CONFIGURAR, COMPILAR TUDO
Expected: `exit=0`.

- [ ] **Step 5: Commit**

```bash
git add frontend/voltz/shell/RecordPage.hpp frontend/voltz/shell/RecordPage.cpp frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): RecordPage (cenas, previa e barra gravar/pausar com relogio)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: `VoltzShell` — janela principal e OBSBasic oculta

**Files:**
- Create: `frontend/voltz/shell/VoltzShell.hpp`, `VoltzShell.cpp`, `voltz-shell.qss`, `voltz-shell.qrc`
- Modify: `frontend/cmake/feature-voltz.cmake`
- Modify: `frontend/OBSApp.hpp`, `frontend/OBSApp.cpp`
- Modify: `frontend/widgets/OBSBasic.cpp` (não mostrar no início; fechar = esconder enquanto a casca existir)

- [ ] **Step 1: Rodar o smoke completo — deve FALHAR (casca não existe)**

Run: `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1`
Expected: `FALHOU log contem '[voltz] shell pronta'`; exit 1.

- [ ] **Step 2: Tema da casca**

`frontend/voltz/shell/voltz-shell.qss`:
```css
QWidget {
	background: #09090B;
	color: #F2F2F3;
	font-family: "DM Sans", "Segoe UI";
	font-size: 13px;
}

QFrame#sideNav {
	background: #111114;
	border-right: 1px solid #1F1F24;
}

QFrame#sideNav QToolButton {
	background: transparent;
	color: #82828C;
	border: none;
	border-radius: 8px;
	padding: 8px 4px;
	font-size: 11px;
}

QFrame#sideNav QToolButton:hover {
	color: #F2F2F3;
	background: #18181C;
}

QFrame#sideNav QToolButton:checked {
	color: #D4F53C;
	background: rgba(212, 245, 60, 26);
}

QFrame#panel {
	background: #111114;
	border: 1px solid #1F1F24;
	border-radius: 8px;
}

QLabel#panelTitle {
	color: #82828C;
	font-size: 10px;
	font-weight: 600;
	background: transparent;
}

QLabel#muted {
	color: #82828C;
	background: transparent;
}

QLabel#pageTitle {
	font-size: 20px;
	font-weight: 700;
	background: transparent;
}

QListWidget {
	background: transparent;
	border: none;
	outline: none;
}

QListWidget::item {
	padding: 6px 8px;
	border-radius: 4px;
}

QListWidget::item:hover {
	background: #18181C;
}

QListWidget::item:selected {
	background: rgba(212, 245, 60, 26);
	color: #F2F2F3;
	border-left: 2px solid #D4F53C;
}

QFrame#bottomBar {
	background: #111114;
	border-top: 1px solid #1F1F24;
}

QPushButton {
	background: #18181C;
	color: #F2F2F3;
	border: 1px solid #1F1F24;
	border-radius: 16px;
	padding: 7px 16px;
}

QPushButton:hover {
	border-color: #3A3A42;
}

QPushButton:disabled {
	color: #4A4A52;
}

QPushButton#recordButton {
	background: #FF4D4D;
	color: #FFFFFF;
	border: none;
	font-weight: 600;
	padding: 8px 22px;
}

QPushButton#recordButton[recording="true"] {
	background: #F2F2F3;
	color: #09090B;
}

QLabel#timeLabel {
	color: #82828C;
	font-family: "Cascadia Mono", "Consolas";
	background: transparent;
}
```

`frontend/voltz/shell/voltz-shell.qrc`:
```xml
<RCC>
  <qresource prefix="/voltz">
    <file>voltz-shell.qss</file>
  </qresource>
</RCC>
```

- [ ] **Step 3: Header da casca**

`frontend/voltz/shell/VoltzShell.hpp`:
```cpp
#pragma once

#include <QMainWindow>

class QButtonGroup;
class QStackedWidget;

namespace voltz {

class FrontendBridge;

// Janela principal do Voltz Studio. A janela do OBS (OBSBasic) fica oculta como motor
// e pode ser aberta em Ajustes → "Abrir modo clássico".
class VoltzShell : public QMainWindow {
	Q_OBJECT

public:
	explicit VoltzShell(QWidget *parent = nullptr);

protected:
	void closeEvent(QCloseEvent *event) override;

private:
	QWidget *buildSideNav();
	QWidget *buildPlaceholderPage(const QString &title, const QString &text);
	QWidget *buildSettingsPage();
	void selectPage(int index);

	FrontendBridge *bridge_ = nullptr;
	QStackedWidget *pages_ = nullptr;
	QButtonGroup *navGroup_ = nullptr;
	bool exiting_ = false;
};

} // namespace voltz
```

- [ ] **Step 4: Implementação da casca**

`frontend/voltz/shell/VoltzShell.cpp`:
```cpp
#include "VoltzShell.hpp"

#include "FrontendBridge.hpp"
#include "RecordPage.hpp"

#include <OBSApp.hpp>
#include <widgets/OBSBasic.hpp>
#include <voltz/core/voltz-version.hpp>

#include <obs.h>

#include <QButtonGroup>
#include <QCloseEvent>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "moc_VoltzShell.cpp"

namespace voltz {

namespace {

VersionInfo currentVersion()
{
	return VersionInfo{"Voltz Studio", VOLTZ_VERSION, obs_get_version_string()};
}

QString loadStyleSheet()
{
	QFile file(QStringLiteral(":/voltz/voltz-shell.qss"));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return QString();
	return QString::fromUtf8(file.readAll());
}

} // namespace

VoltzShell::VoltzShell(QWidget *parent) : QMainWindow(parent)
{
	setObjectName("voltzShell");
	setWindowTitle(QString::fromStdString(windowTitle(currentVersion(), "")));
	setMinimumSize(1024, 640);
	resize(1280, 800);
	setStyleSheet(loadStyleSheet());

	bridge_ = new FrontendBridge(this);

	pages_ = new QStackedWidget(this);
	pages_->addWidget(new RecordPage(bridge_, pages_));
	pages_->addWidget(buildPlaceholderPage(QStringLiteral("Projetos"),
					       QStringLiteral("Em breve: suas gravações aparecem aqui.")));
	pages_->addWidget(buildPlaceholderPage(QStringLiteral("Presets"),
					       QStringLiteral("Em breve: layouts e configurações prontas.")));
	pages_->addWidget(buildSettingsPage());

	auto *central = new QWidget(this);
	auto *layout = new QHBoxLayout(central);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(buildSideNav());
	layout->addWidget(pages_, 1);
	setCentralWidget(central);

	connect(bridge_, &FrontendBridge::exiting, this, [this]() {
		exiting_ = true;
		hide();
		deleteLater();
	});

	blog(LOG_INFO, "[voltz] shell pronta (%s)", aboutLine(currentVersion()).c_str());
}

QWidget *VoltzShell::buildSideNav()
{
	auto *nav = new QFrame(this);
	nav->setObjectName("sideNav");
	nav->setFixedWidth(76);
	auto *layout = new QVBoxLayout(nav);
	layout->setContentsMargins(8, 12, 8, 12);
	layout->setSpacing(6);

	navGroup_ = new QButtonGroup(this);
	navGroup_->setExclusive(true);

	const QStringList labels = {QStringLiteral("●\nGravar"), QStringLiteral("▤\nProjetos"),
				    QStringLiteral("◫\nPresets"), QStringLiteral("⚙\nAjustes")};
	for (int i = 0; i < labels.size(); i++) {
		auto *button = new QToolButton(nav);
		button->setText(labels[i]);
		button->setCheckable(true);
		button->setToolButtonStyle(Qt::ToolButtonTextOnly);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		navGroup_->addButton(button, i);
		if (i == labels.size() - 1)
			layout->addStretch(1);
		layout->addWidget(button);
	}

	connect(navGroup_, &QButtonGroup::idClicked, this, &VoltzShell::selectPage);
	navGroup_->button(0)->setChecked(true);
	return nav;
}

QWidget *VoltzShell::buildPlaceholderPage(const QString &title, const QString &text)
{
	auto *page = new QWidget(this);
	auto *layout = new QVBoxLayout(page);
	layout->setContentsMargins(32, 28, 32, 28);
	layout->setSpacing(8);

	auto *heading = new QLabel(title, page);
	heading->setObjectName("pageTitle");
	auto *body = new QLabel(text, page);
	body->setObjectName("muted");
	body->setWordWrap(true);

	layout->addWidget(heading);
	layout->addWidget(body);
	layout->addStretch(1);
	return page;
}

QWidget *VoltzShell::buildSettingsPage()
{
	QWidget *page = buildPlaceholderPage(QStringLiteral("Ajustes"),
					     QString::fromStdString(aboutLine(currentVersion())));

	auto *classic = new QPushButton(QStringLiteral("Abrir modo clássico (OBS completo)"), page);
	connect(classic, &QPushButton::clicked, this, []() {
		OBSBasic *main = OBSBasic::Get();
		main->show();
		main->raise();
		main->activateWindow();
	});

	auto *layout = static_cast<QVBoxLayout *>(page->layout());
	layout->insertWidget(2, classic, 0, Qt::AlignLeft);
	return page;
}

void VoltzShell::selectPage(int index)
{
	pages_->setCurrentIndex(index);
}

void VoltzShell::closeEvent(QCloseEvent *event)
{
	if (exiting_) {
		event->accept();
		return;
	}

	/* Fechar a casca encerra o app: quem conduz o desligamento é o OBSBasic
	 * (confirmações, salvar cenas, liberar fontes). Ao final ele emite
	 * OBS_FRONTEND_EVENT_EXIT e a casca se destrói antes do obs_shutdown. */
	event->ignore();
	App()->SetVoltzQuitting(true);
	QMetaObject::invokeMethod(OBSBasic::Get(), "close", Qt::QueuedConnection);
}

} // namespace voltz
```

- [ ] **Step 5: Registrar no CMake**

Em `frontend/cmake/feature-voltz.cmake`, trocar o bloco `target_sources(obs-studio ...)` por:
```cmake
target_sources(
  obs-studio
  PRIVATE
    voltz/shell/FrontendBridge.cpp
    voltz/shell/FrontendBridge.hpp
    voltz/shell/RecordPage.cpp
    voltz/shell/RecordPage.hpp
    voltz/shell/VoltzPreview.cpp
    voltz/shell/VoltzPreview.hpp
    voltz/shell/VoltzShell.cpp
    voltz/shell/VoltzShell.hpp
    voltz/shell/voltz-shell.qrc
)
```

- [ ] **Step 6: `OBSApp` cria e conhece a casca**

Em `frontend/OBSApp.hpp`, trocar:
```cpp
class PluginManager;
} // namespace OBS
```
por:
```cpp
class PluginManager;
} // namespace OBS

#ifdef VOLTZ_STUDIO
namespace voltz {
class VoltzShell;
}
#endif
```

Trocar:
```cpp
	QPointer<OBSMainWindow> mainWindow;
```
por:
```cpp
	QPointer<OBSMainWindow> mainWindow;
#ifdef VOLTZ_STUDIO
	QPointer<voltz::VoltzShell> voltzShell;
	bool voltzQuitting = false;
#endif
```

Trocar:
```cpp
	inline QMainWindow *GetMainWindow() const { return mainWindow.data(); }
```
por:
```cpp
	inline QMainWindow *GetMainWindow() const { return mainWindow.data(); }
#ifdef VOLTZ_STUDIO
	inline bool HasVoltzShell() const { return !voltzShell.isNull(); }
	inline bool IsVoltzQuitting() const { return voltzQuitting; }
	inline void SetVoltzQuitting(bool quitting) { voltzQuitting = quitting; }
#endif
```

Em `frontend/OBSApp.cpp`, trocar:
```cpp
#include <widgets/OBSBasic.hpp>
```
por:
```cpp
#include <widgets/OBSBasic.hpp>
#ifdef VOLTZ_STUDIO
#include <voltz/shell/VoltzShell.hpp>
#endif
```

E trocar:
```cpp
	mainWindow->OBSInit();

	connect(OBSBasic::Get(), &OBSBasic::mainWindowClosed, crashHandler_.get(),
```
por:
```cpp
	mainWindow->OBSInit();

#ifdef VOLTZ_STUDIO
	voltzShell = new voltz::VoltzShell();
	voltzShell->show();
#endif

	connect(OBSBasic::Get(), &OBSBasic::mainWindowClosed, crashHandler_.get(),
```

- [ ] **Step 7: `OBSBasic` começa oculta e "fechar" vira "esconder" enquanto a casca existir**

Em `frontend/widgets/OBSBasic.cpp`, trocar:
```cpp
#ifdef _WIN32
	SetWin32DropStyle(this);

	if (!hideWindowOnStart) {
		show();
	}
#endif
```
por:
```cpp
#ifdef _WIN32
	SetWin32DropStyle(this);

#ifdef VOLTZ_STUDIO
	/* Voltz: a casca Voltz é a janela principal; OBSBasic fica oculta (modo clássico) */
	hideWindowOnStart = true;
#endif
	if (!hideWindowOnStart) {
		show();
	}
#endif
```

E, em `OBSBasic::closeEvent`, trocar:
```cpp
	if (isVisible()) {
		config_set_string(App()->GetUserConfig(), "BasicWindow", "geometry",
				  saveGeometry().toBase64().constData());
	}
```
por:
```cpp
	if (isVisible()) {
		config_set_string(App()->GetUserConfig(), "BasicWindow", "geometry",
				  saveGeometry().toBase64().constData());
	}

#ifdef VOLTZ_STUDIO
	/* Voltz: fechar o modo clássico só esconde; quem encerra o app é a casca */
	if (App()->HasVoltzShell() && !App()->IsVoltzQuitting()) {
		hide();
		event->ignore();
		return;
	}
#endif
```

- [ ] **Step 8: Configurar e compilar**

Run: CONFIGURAR, COMPILAR TUDO
Expected: `exit=0`.

- [ ] **Step 9: Smoke completo — deve PASSAR**

Run: `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1`
Expected:
```
OK     processo vivo apos 20 s
OK     pasta de config 'Voltz Studio' criada
OK     pasta 'obs-studio' ausente
OK     log gerado
OK     log contem '[voltz] shell pronta'
OK     encerrou limpo ao fechar a janela
SMOKE OK
```
Se `encerrou limpo` falhar: abrir o log indicado e procurar a última linha antes do fim — o fluxo esperado é
`[voltz] shell pronta` … `==== Shutting down ==================================================`.

- [ ] **Step 10: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake frontend/OBSApp.hpp frontend/OBSApp.cpp frontend/widgets/OBSBasic.cpp
git commit -m "feat(voltz): casca VoltzShell como janela principal, OBSBasic oculta como motor e modo classico

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: Verificação ponta a ponta, registro de patches e memória do projeto

**Files:**
- Create: `docs/voltz/upstream-patches.md`
- Modify: `CLAUDE.md`, `AGENTS.md`

- [ ] **Step 1: Testes unitários e smoke**

Run: RODAR TESTES, depois `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1`
Expected: `100% tests passed` e `SMOKE OK`.

- [ ] **Step 2: Roteiro manual (gravar de verdade)**

Apagar a config portátil e abrir o app:
```bash
rm -rf build_x64/rundir/RelWithDebInfo/config && cd build_x64/rundir/RelWithDebInfo/bin/64bit && ./VoltzStudio.exe --portable &
```
Conferir, nesta ordem, anotando OK/FALHOU:

1. Abre **só** a janela "Voltz Studio 1.0.0" (preta, barra lateral com Gravar/Projetos/Presets/Ajustes);
   nenhuma janela do OBS e nenhum assistente de configuração.
2. A prévia mostra a imagem do programa (com config nova, um quadro preto é o esperado — ainda não há fontes).
3. A lista CENAS mostra a cena padrão, selecionada.
4. Ajustes → "Abrir modo clássico" abre a janela completa do OBS. Nela, criar uma cena "Teste" e adicionar
   uma fonte "Captura de tela". **Na casca**, a lista de cenas passa a mostrar "Teste" e a prévia mostra a tela.
5. Fechar a janela clássica (X): ela some, a casca continua aberta.
6. Clicar em outra cena na casca: a prévia muda.
7. Clicar **● Gravar**: o botão vira **■ Parar**, o relógio conta. **⏸ Pausar**: o relógio congela;
   **▶ Retomar**: volta a contar. **■ Parar**: o relógio volta a `00:00:00`.
8. Um arquivo novo de vídeo aparece em `%USERPROFILE%\Videos` com a duração gravada (descontada a pausa).
9. Fechar a casca (X): o app encerra em poucos segundos; nenhum processo `VoltzStudio.exe` fica no
   Gerenciador de Tarefas.

Se algum item falhar, registre o sintoma e trate antes de seguir (não marque a task como concluída).

- [ ] **Step 3: Registro dos patches no código do OBS**

`docs/voltz/upstream-patches.md`:
```markdown
# Alterações em arquivos do OBS (upstream)

Toda alteração em arquivo do OBS fica dentro de `#ifdef VOLTZ_STUDIO` (C/C++) ou é comentada com
"Voltz Studio" (CMake). Ao atualizar o OBS (merge de uma tag nova do upstream), revise cada item abaixo.

| Arquivo | O que muda | Por quê |
|---|---|---|
| `CMakePresets.json` | preset `voltz-windows-x64` | VS 2022, opções desligadas, GUID da câmera virtual |
| `cmake/common/bootstrap.cmake` | `OBS_COMPANY_NAME`, `OBS_PRODUCT_NAME`, site, comentário, copyright | identidade nos `.rc` de todos os módulos |
| `frontend/CMakeLists.txt` | `include(cmake/feature-voltz.cmake)`; `OUTPUT_NAME VoltzStudio` | código Voltz e nome do executável |
| `frontend/cmake/os-windows.cmake` | não compila `updater` | sem atualizador do OBS |
| `frontend/cmake/windows/obs.rc.in` | versão/empresa/produto do Voltz | propriedades do `.exe` |
| `frontend/obs-main.cpp` | `opt_disable_updater = true` | sem atualizador |
| 14 arquivos em `frontend/` | `"obs-studio/..."` e `"/obs-studio/..."` → `VOLTZ_CONFIG_DIR` | config em `%APPDATA%\Voltz Studio` |
| `frontend/utility/platform-windows.cpp` | mutex `VoltzStudioCore` / `VoltzStudioPortable` | convive com OBS aberto |
| `frontend/widgets/OBSBasic_MainControls.cpp` | `UploadLog` abre pasta; links de ajuda | nada sai para obsproject.com |
| `frontend/utility/CrashHandler.cpp` | `uploadCrashLogToServer` não envia | idem |
| `frontend/dialogs/OBSAbout.cpp` | nome/versão Voltz; sem busca do Patreon | idem + identidade |
| `frontend/widgets/OBSBasic.cpp` | sem assistente na 1ª execução; começa oculta; fechar = esconder com casca | casca é a janela principal |
| `frontend/OBSApp.hpp/.cpp` | cria `VoltzShell`; flags `HasVoltzShell`/`IsVoltzQuitting` | ciclo de vida da casca |
| `plugins/win-dshow/virtualcam-module/virtualcam-filter.cpp` | `VoltzStudio.exe` | detecção do próprio app |
| `plugins/win-dshow/virtualcam-module/virtualcam-module.cpp` | "Voltz Virtual Camera" | nome da câmera |
| `plugins/win-dshow/shared-memory-queue.c`, `shared/obs-shared-memory-queue/shared-memory-queue.c` | `VoltzVirtualCamVideo` | não colidir com a câmera do OBS |
| `.gitignore` | `!AGENTS.md`, `!CLAUDE.md`, `!installer`, `!tools` | versionar arquivos do Voltz na raiz |
```

- [ ] **Step 4: Atualizar a memória do projeto**

Em `CLAUDE.md`, substituir as seções **Estado atual** e **Próximos passos** por:
```markdown
## Estado atual
- Plano 1 (Fundação) concluído: `VoltzStudio.exe` 1.0.0, config em `%APPDATA%\Voltz Studio`, sem phone-home,
  câmera virtual própria, `voltz-core` + testes doctest, casca `VoltzShell` (Gravar com cenas/prévia/gravar/pausar)
  sobre `OBSBasic` oculta. Patches no OBS listados em `docs/voltz/upstream-patches.md`.

## Próximos passos
1. Escrever e executar o Plano 2 (casca completa: layout H2 final, fontes/áudio, Projetos/Presets/Ajustes, tema `.ovt`, DM Sans, locale, bandeja).
2. Depois: Plano 3 (gravação em projeto), 4 (anotações + mouse), 5 (marca visual + instalador + CI).
```
Na seção **Stack & estrutura**, trocar a linha do preset local por:
```markdown
  - `cmake --preset voltz-windows-x64` (versionado em `CMakePresets.json`, VS 17 2022, saída `build_x64/`)
  - `cmake --build --preset voltz-windows-x64 --parallel -- -v:minimal -nologo -nodeReuse:false`
  - Testes: `ctest --test-dir build_x64/frontend/voltz/tests -C RelWithDebInfo --output-on-failure`
  - Smoke: `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1`
```
e remover as duas linhas antigas (`cmake --preset voltz-x64 ...` e `cmake --build --preset voltz-x64 ...`),
trocando também `obs64.exe` por `VoltzStudio.exe` na linha do executável.

Depois: `cp CLAUDE.md AGENTS.md`

- [ ] **Step 5: Commit**

```bash
git add docs/voltz/upstream-patches.md CLAUDE.md AGENTS.md
git commit -m "docs(voltz): registro de patches no OBS e memoria do projeto apos o Plano 1

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
