# Voltz Studio — Plano 2A: Fundamentos da casca — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deixar a tela Gravar completa e confiável: erros de gravação visíveis na casca, textos traduzíveis
(pt-BR/en-US), ícones Phosphor + fonte DM Sans, barra de título escura, tema da casca isolado do tema do OBS,
cenas por UUID, pausa só quando suportada, e painéis de **Fontes** e **Áudio** funcionais.

**Architecture:** Continua o desenho do Plano 1: casca Qt (`frontend/voltz/shell/`) fala com o OBS pela frontend
API, por sinais de cena/saída do libobs e por `OBSBasic::Get()` só para diálogos do OBS (adicionar fonte).
Lógica pura nova vai para `frontend/voltz/core/` com testes doctest. Cada painel é um widget próprio
(`ScenesPanel`, `SourcesPanel`, `AudioPanel`, `ErrorBanner`) que a `RecordPage` compõe.

**Tech Stack:** C++17, Qt 6 (Widgets, Svg), libobs 32.2.2, frontend API do OBS, doctest 2.4.11,
Phosphor Icons 2.1.1 (`@phosphor-icons/core`, MIT), DM Sans (OFL-1.1, via `@expo-google-fonts/dm-sans` 0.4.2).

**Spec:** `docs/superpowers/specs/2026-10-15-voltz-studio-base-gravacao-design.md` (§4) ·
**Backlog atendido:** `docs/voltz/backlog-plano-2.md` (erros de gravação, nativeEvent, cenas por UUID, pausa,
ícones Phosphor, DM Sans, i18n do `aboutLine` e das mensagens de upload, vazamento do tema do OBS).
**Fica para o 2B:** Presets, Projetos, Ajustes, tema `.ovt` do modo clássico, bandeja, diálogos de saída/falha.

**Decisão deste plano:** barra de título **nativa do Windows em modo escuro** (DWM: dark mode + cor `#111114`),
em vez de janela sem bordas — mantém snap/arrastar/maximizar/redimensionar do sistema.

---

## Convenções

**Diretório de trabalho:** a raiz do repositório (`voltz-studio`), branch `voltz`.

**Comandos (Git Bash).** O estado do shell não persiste entre comandos — defina as variáveis no mesmo comando:

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

> ⚠️ Flags do MSBuild com `-` (nunca `/`) no Git Bash. Builds longos: rode em segundo plano e espere terminar;
> nunca dois builds ao mesmo tempo. Falha `MSB4018 "Touch"`/`ZERO_CHECK` no `build_x86` é corrida conhecida:
> rode de novo. Arquivos novos exigem CONFIGURAR antes de compilar.
>
> **Formatação:** ao final de cada task, rode `clang-format -i` (VS Build Tools:
> `/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/bin/clang-format.exe`) nos
> arquivos `.cpp/.hpp` novos/alterados em `frontend/voltz/`. Em arquivos do OBS, formate só as linhas Voltz.
>
> **Upstream:** toda alteração em arquivo C/C++ do OBS fica em `#ifdef VOLTZ_STUDIO` e é registrada em
> `docs/voltz/upstream-patches.md` (Task 9).

**Commits:** terminar toda mensagem com `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

---

## Mapa de arquivos

**Novos**
| Arquivo | Responsabilidade |
|---|---|
| `frontend/voltz/core/recording-error.hpp/.cpp` | código de parada da gravação → chaves de texto do OBS |
| `frontend/voltz/core/svg-recolor.hpp/.cpp` | troca `currentColor` de um SVG por uma cor |
| `frontend/voltz/tests/test-recording-error.cpp`, `test-svg-recolor.cpp` | testes |
| `frontend/data/locale/voltz-en-US.ini`, `voltz-pt-BR.ini` | textos da casca |
| `frontend/voltz/shell/ShellLocale.hpp/.cpp` | carrega os `.ini` do Voltz no lookup de textos do OBS |
| `frontend/voltz/shell/Icons.hpp/.cpp` | ícones Phosphor recoloridos + carga da DM Sans |
| `frontend/voltz/shell/icons/{regular,fill}/*.svg`, `icons/LICENSE-phosphor.txt` | ícones vendorizados |
| `frontend/voltz/shell/fonts/DMSans-*.ttf`, `fonts/OFL.txt` | fonte vendorizada |
| `frontend/voltz/shell/WindowChrome.hpp/.cpp` | barra de título escura (DWM) e repasse de WM_MOVE/WM_DISPLAYCHANGE |
| `frontend/voltz/shell/ScenesPanel.hpp/.cpp` | lista de cenas por UUID |
| `frontend/voltz/shell/ErrorBanner.hpp/.cpp` | faixa de erro dispensável |
| `frontend/voltz/shell/SourcesPanel.hpp/.cpp` | fontes da cena atual |
| `frontend/voltz/shell/AudioPanel.hpp/.cpp` | som do sistema + microfone |

**Modificados:** `frontend/voltz/core/voltz-version.hpp/.cpp`, `frontend/voltz/core/CMakeLists.txt`,
`frontend/voltz/tests/CMakeLists.txt`, `frontend/voltz/tests/test-version.cpp`, `frontend/cmake/feature-voltz.cmake`,
`frontend/voltz/shell/{FrontendBridge,RecordPage,VoltzShell}.{hpp,cpp}`, `voltz-shell.qss`, `voltz-shell.qrc`;
upstream: `frontend/dialogs/OBSAbout.cpp`, `frontend/widgets/OBSBasic_MainControls.cpp`, `frontend/utility/CrashHandler.cpp`.

---

### Task 1: Núcleo — erros de gravação, recolor de SVG e `aboutLine` traduzível

**Files:**
- Create: `frontend/voltz/core/recording-error.hpp`, `recording-error.cpp`, `svg-recolor.hpp`, `svg-recolor.cpp`
- Create: `frontend/voltz/tests/test-recording-error.cpp`, `test-svg-recolor.cpp`
- Modify: `frontend/voltz/core/voltz-version.hpp/.cpp`, `frontend/voltz/core/CMakeLists.txt`
- Modify: `frontend/voltz/tests/test-version.cpp`, `frontend/voltz/tests/CMakeLists.txt`
- Modify: `frontend/dialogs/OBSAbout.cpp`, `frontend/voltz/shell/VoltzShell.cpp` (chamadas de `aboutLine`)

- [ ] **Step 1: Testes (falham)**

`frontend/voltz/tests/test-recording-error.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/recording-error.hpp>

using voltz::LastErrorUse;
using voltz::RecordingStopKind;

TEST_CASE("classifyRecordingStop mapeia os códigos OBS_OUTPUT_*")
{
	CHECK(voltz::classifyRecordingStop(0) == RecordingStopKind::Success);
	CHECK(voltz::classifyRecordingStop(-6) == RecordingStopKind::Unsupported);
	CHECK(voltz::classifyRecordingStop(-7) == RecordingStopKind::NoSpace);
	CHECK(voltz::classifyRecordingStop(-8) == RecordingStopKind::EncodeError);
	CHECK(voltz::classifyRecordingStop(-1) == RecordingStopKind::Generic);
	CHECK(voltz::classifyRecordingStop(-4) == RecordingStopKind::Generic);
	CHECK(voltz::classifyRecordingStop(42) == RecordingStopKind::Generic);
}

TEST_CASE("recordingErrorText: sucesso não tem texto")
{
	const voltz::RecordingErrorText t = voltz::recordingErrorText(RecordingStopKind::Success, true);
	CHECK(t.titleKey.empty());
	CHECK(t.messageKey.empty());
	CHECK(t.lastErrorUse == LastErrorUse::None);
}

TEST_CASE("recordingErrorText: sem suporte e sem espaço ignoram o last_error")
{
	const voltz::RecordingErrorText u = voltz::recordingErrorText(RecordingStopKind::Unsupported, true);
	CHECK(u.titleKey == "Output.RecordFail.Title");
	CHECK(u.messageKey == "Output.RecordFail.Unsupported");
	CHECK(u.lastErrorUse == LastErrorUse::None);

	const voltz::RecordingErrorText n = voltz::recordingErrorText(RecordingStopKind::NoSpace, true);
	CHECK(n.titleKey == "Output.RecordNoSpace.Title");
	CHECK(n.messageKey == "Output.RecordNoSpace.Msg");
	CHECK(n.lastErrorUse == LastErrorUse::None);
}

TEST_CASE("recordingErrorText: erro de codificação usa %1 quando há last_error")
{
	const voltz::RecordingErrorText with = voltz::recordingErrorText(RecordingStopKind::EncodeError, true);
	CHECK(with.titleKey == "Output.RecordError.Title");
	CHECK(with.messageKey == "Output.RecordError.EncodeErrorMsg.LastError");
	CHECK(with.lastErrorUse == LastErrorUse::FormatArg);

	const voltz::RecordingErrorText without = voltz::recordingErrorText(RecordingStopKind::EncodeError, false);
	CHECK(without.messageKey == "Output.RecordError.EncodeErrorMsg");
	CHECK(without.lastErrorUse == LastErrorUse::None);
}

TEST_CASE("recordingErrorText: erro genérico anexa o last_error quando existe")
{
	const voltz::RecordingErrorText with = voltz::recordingErrorText(RecordingStopKind::Generic, true);
	CHECK(with.titleKey == "Output.RecordError.Title");
	CHECK(with.messageKey == "Output.RecordError.Msg");
	CHECK(with.lastErrorUse == LastErrorUse::Append);

	const voltz::RecordingErrorText without = voltz::recordingErrorText(RecordingStopKind::Generic, false);
	CHECK(without.lastErrorUse == LastErrorUse::None);
}
```

`frontend/voltz/tests/test-svg-recolor.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/svg-recolor.hpp>

TEST_CASE("recolorSvg troca todas as ocorrências de currentColor")
{
	const std::string svg = R"(<svg fill="currentColor"><path stroke="currentColor" d="M0 0"/></svg>)";
	CHECK(voltz::recolorSvg(svg, "#D4F53C") == R"(<svg fill="#D4F53C"><path stroke="#D4F53C" d="M0 0"/></svg>)");
}

TEST_CASE("recolorSvg sem currentColor devolve o texto igual")
{
	const std::string svg = R"(<svg fill="#000"/>)";
	CHECK(voltz::recolorSvg(svg, "#FFFFFF") == svg);
}

TEST_CASE("recolorSvg com entrada vazia devolve vazio")
{
	CHECK(voltz::recolorSvg("", "#FFFFFF").empty());
}
```

Em `frontend/voltz/tests/test-version.cpp`, trocar o primeiro caso:
```cpp
TEST_CASE("aboutLine credita o OBS Studio e a licença")
{
	const voltz::VersionInfo info{"Voltz Studio", "1.0.0", "32.2.2"};
	CHECK(voltz::aboutLine(info) == "Voltz Studio 1.0.0 · baseado no OBS Studio 32.2.2 (GPL-2.0)");
}
```
por:
```cpp
TEST_CASE("aboutLine credita o OBS Studio e a licença com o rótulo traduzido")
{
	const voltz::VersionInfo info{"Voltz Studio", "1.0.0", "32.2.2"};
	CHECK(voltz::aboutLine(info, "baseado no OBS Studio") ==
	      "Voltz Studio 1.0.0 · baseado no OBS Studio 32.2.2 (GPL-2.0)");
	CHECK(voltz::aboutLine(info, "based on OBS Studio") ==
	      "Voltz Studio 1.0.0 · based on OBS Studio 32.2.2 (GPL-2.0)");
}
```

Em `frontend/voltz/tests/CMakeLists.txt`, trocar:
```cmake
  PRIVATE test-main.cpp test-preview-fit.cpp test-recording-clock.cpp test-time-format.cpp test-version.cpp
```
por:
```cmake
  PRIVATE
    test-main.cpp
    test-preview-fit.cpp
    test-recording-clock.cpp
    test-recording-error.cpp
    test-svg-recolor.cpp
    test-time-format.cpp
    test-version.cpp
```

- [ ] **Step 2: Headers e CMake**

`frontend/voltz/core/recording-error.hpp`:
```cpp
#pragma once

#include <string>

namespace voltz {

enum class RecordingStopKind { Success, Unsupported, NoSpace, EncodeError, Generic };

// Como usar o texto "last_error" que a saída de gravação devolve ao parar.
enum class LastErrorUse { None, FormatArg, Append };

struct RecordingErrorText {
	std::string titleKey; // vazio quando não houve erro
	std::string messageKey;
	LastErrorUse lastErrorUse = LastErrorUse::None;
};

// Códigos OBS_OUTPUT_* (libobs/obs-defs.h): 0 sucesso, -6 sem suporte, -7 sem espaço, -8 erro de codificação;
// qualquer outro é erro genérico.
RecordingStopKind classifyRecordingStop(int code);

// Mesmas chaves de texto que OBSBasic::RecordingStop usa, para a casca mostrar o erro igual ao OBS.
RecordingErrorText recordingErrorText(RecordingStopKind kind, bool hasLastError);

} // namespace voltz
```

`frontend/voltz/core/svg-recolor.hpp`:
```cpp
#pragma once

#include <string>

namespace voltz {

// Troca todas as ocorrências de "currentColor" (padrão dos SVGs do Phosphor) pela cor dada, ex. "#D4F53C".
std::string recolorSvg(const std::string &svg, const std::string &color);

} // namespace voltz
```

Em `frontend/voltz/core/voltz-version.hpp`, trocar:
```cpp
// "Voltz Studio 1.0.0 · baseado no OBS Studio 32.2.2 (GPL-2.0)"
std::string aboutLine(const VersionInfo &info);
```
por:
```cpp
// "Voltz Studio 1.0.0 · <basedOnLabel> 32.2.2 (GPL-2.0)"; o rótulo vem traduzido
// (ex.: "baseado no OBS Studio" / "based on OBS Studio").
std::string aboutLine(const VersionInfo &info, const std::string &basedOnLabel);
```

Em `frontend/voltz/core/CMakeLists.txt`, trocar o bloco `target_sources(...)` por:
```cmake
target_sources(
  voltz-core
  PRIVATE preview-fit.cpp recording-clock.cpp recording-error.cpp svg-recolor.cpp time-format.cpp voltz-version.cpp
  PUBLIC
    preview-fit.hpp
    recording-clock.hpp
    recording-error.hpp
    svg-recolor.hpp
    time-format.hpp
    voltz-version.hpp
)
```

Criar os `.cpp` novos só com o include (para configurar): 
```bash
printf '#include "recording-error.hpp"\n' > frontend/voltz/core/recording-error.cpp
printf '#include "svg-recolor.hpp"\n' > frontend/voltz/core/svg-recolor.cpp
```

- [ ] **Step 3: Configurar e compilar os testes — devem FALHAR**

Run: CONFIGURAR, COMPILAR TESTES
Expected: erros — `aboutLine` não aceita 2 argumentos (ou link `LNK2019` para `classifyRecordingStop`,
`recordingErrorText`, `recolorSvg`).

- [ ] **Step 4: Implementar**

`frontend/voltz/core/recording-error.cpp`:
```cpp
#include "recording-error.hpp"

namespace voltz {

RecordingStopKind classifyRecordingStop(int code)
{
	switch (code) {
	case 0:
		return RecordingStopKind::Success;
	case -6:
		return RecordingStopKind::Unsupported;
	case -7:
		return RecordingStopKind::NoSpace;
	case -8:
		return RecordingStopKind::EncodeError;
	default:
		return RecordingStopKind::Generic;
	}
}

RecordingErrorText recordingErrorText(RecordingStopKind kind, bool hasLastError)
{
	switch (kind) {
	case RecordingStopKind::Success:
		return {};
	case RecordingStopKind::Unsupported:
		return {"Output.RecordFail.Title", "Output.RecordFail.Unsupported", LastErrorUse::None};
	case RecordingStopKind::NoSpace:
		return {"Output.RecordNoSpace.Title", "Output.RecordNoSpace.Msg", LastErrorUse::None};
	case RecordingStopKind::EncodeError:
		if (hasLastError) {
			return {"Output.RecordError.Title", "Output.RecordError.EncodeErrorMsg.LastError",
				LastErrorUse::FormatArg};
		}
		return {"Output.RecordError.Title", "Output.RecordError.EncodeErrorMsg", LastErrorUse::None};
	case RecordingStopKind::Generic:
		return {"Output.RecordError.Title", "Output.RecordError.Msg",
			hasLastError ? LastErrorUse::Append : LastErrorUse::None};
	}
	return {};
}

} // namespace voltz
```

`frontend/voltz/core/svg-recolor.cpp`:
```cpp
#include "svg-recolor.hpp"

namespace voltz {

std::string recolorSvg(const std::string &svg, const std::string &color)
{
	static const std::string token = "currentColor";
	std::string out;
	out.reserve(svg.size());
	size_t pos = 0;
	while (true) {
		const size_t found = svg.find(token, pos);
		if (found == std::string::npos) {
			out.append(svg, pos, std::string::npos);
			break;
		}
		out.append(svg, pos, found - pos);
		out.append(color);
		pos = found + token.size();
	}
	return out;
}

} // namespace voltz
```

Em `frontend/voltz/core/voltz-version.cpp`, trocar:
```cpp
std::string aboutLine(const VersionInfo &info)
{
	return info.product + " " + info.productVersion + " · baseado no OBS Studio " + info.engineVersion +
	       " (GPL-2.0)";
}
```
por:
```cpp
std::string aboutLine(const VersionInfo &info, const std::string &basedOnLabel)
{
	return info.product + " " + info.productVersion + " · " + basedOnLabel + " " + info.engineVersion +
	       " (GPL-2.0)";
}
```
(Se o `clang-format` do Plano 1 tiver quebrado a linha de outro jeito, ajuste o trecho equivalente — o corpo
deve ficar como acima.)

- [ ] **Step 5: Atualizar quem chama `aboutLine` (provisório; a Task 2 troca por texto traduzido)**

Em `frontend/dialogs/OBSAbout.cpp`, dentro do bloco `#ifdef VOLTZ_STUDIO` de `ShowAbout`, trocar
`voltz::aboutLine(info)` por `voltz::aboutLine(info, "baseado no OBS Studio")`.

Em `frontend/voltz/shell/VoltzShell.cpp`, trocar as duas ocorrências de `aboutLine(currentVersion())` por
`aboutLine(currentVersion(), "baseado no OBS Studio")`.

- [ ] **Step 6: Testes passam e app compila**

Run: COMPILAR TESTES, RODAR TESTES, depois COMPILAR APP
Expected: `100% tests passed`; o executável de testes rodado direto reporta `test cases: 34 | 34 passed`
(26 anteriores + 5 de recording-error + 3 de svg-recolor; o caso de `aboutLine` foi substituído, não somado).
App: `exit=0`.

- [ ] **Step 7: Commit**

```bash
git add frontend/voltz/core frontend/voltz/tests/CMakeLists.txt frontend/voltz/tests/test-recording-error.cpp frontend/voltz/tests/test-svg-recolor.cpp frontend/voltz/tests/test-version.cpp frontend/dialogs/OBSAbout.cpp frontend/voltz/shell/VoltzShell.cpp
git commit -m "feat(voltz): nucleo com erros de gravacao, recolor de SVG e aboutLine com rotulo traduzivel

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Textos da casca em pt-BR e en-US

**Files:**
- Create: `frontend/data/locale/voltz-en-US.ini`, `frontend/data/locale/voltz-pt-BR.ini`
- Create: `frontend/voltz/shell/ShellLocale.hpp`, `ShellLocale.cpp`
- Modify: `frontend/cmake/feature-voltz.cmake`, `frontend/voltz/shell/VoltzShell.cpp`, `frontend/voltz/shell/RecordPage.cpp`
- Modify (upstream, dentro dos `#ifdef VOLTZ_STUDIO` existentes): `frontend/dialogs/OBSAbout.cpp`,
  `frontend/widgets/OBSBasic_MainControls.cpp`, `frontend/utility/CrashHandler.cpp`

O OBS carrega `locale/en-US.ini` + `locale/<idioma>.ini` num lookup de textos; `text_lookup_add` acrescenta
arquivos (chaves posteriores sobrescrevem). Os `.ini` em `frontend/data/` são copiados para
`rundir/.../data/obs-studio/` no build. A casca acrescenta `voltz-en-US.ini` e depois `voltz-<idioma>.ini`.

- [ ] **Step 1: Arquivos de texto**

`frontend/data/locale/voltz-en-US.ini` (UTF-8, sem BOM):
```ini
Voltz.Nav.Record="Record"
Voltz.Nav.Projects="Projects"
Voltz.Nav.Presets="Presets"
Voltz.Nav.Settings="Settings"
Voltz.Record.Start="Record"
Voltz.Record.Stop="Stop"
Voltz.Record.Pause="Pause"
Voltz.Record.Resume="Resume"
Voltz.Record.Starting="Starting…"
Voltz.Record.Stopping="Finishing…"
Voltz.Record.PauseUnavailable="This recording format does not support pausing."
Voltz.Panel.Scenes="SCENES"
Voltz.Panel.Sources="SOURCES"
Voltz.Panel.Audio="AUDIO"
Voltz.Sources.Add="Add source"
Voltz.Sources.Remove="Remove"
Voltz.Sources.RemoveConfirm.Title="Remove source"
Voltz.Sources.RemoveConfirm.Text="Remove \"%1\" from this scene?"
Voltz.Sources.Properties="Properties"
Voltz.Sources.Filters="Filters"
Voltz.Sources.MoveUp="Move up"
Voltz.Sources.MoveDown="Move down"
Voltz.Sources.ToggleVisible="Show/hide"
Voltz.Sources.ToggleLock="Lock/unlock"
Voltz.Sources.Empty="No sources in this scene yet. Click + to add one."
Voltz.Audio.Desktop="System audio"
Voltz.Audio.Mic="Microphone"
Voltz.Audio.Empty="No audio device configured."
Voltz.Error.Dismiss="Dismiss"
Voltz.Projects.Title="Projects"
Voltz.Projects.Soon="Coming soon: your recordings show up here."
Voltz.Presets.Title="Presets"
Voltz.Presets.Soon="Coming soon: ready-made layouts and settings."
Voltz.Settings.Title="Settings"
Voltz.Settings.OpenClassic="Open classic mode (full OBS)"
Voltz.About.BasedOn="based on OBS Studio"
Voltz.LogUpload.Disabled="Log uploading is disabled in Voltz Studio. The logs folder has been opened."
Voltz.CrashUpload.Disabled="Crash report uploading is disabled in Voltz Studio. The crash reports folder has been opened."
```

`frontend/data/locale/voltz-pt-BR.ini` (UTF-8, sem BOM):
```ini
Voltz.Nav.Record="Gravar"
Voltz.Nav.Projects="Projetos"
Voltz.Nav.Presets="Presets"
Voltz.Nav.Settings="Ajustes"
Voltz.Record.Start="Gravar"
Voltz.Record.Stop="Parar"
Voltz.Record.Pause="Pausar"
Voltz.Record.Resume="Retomar"
Voltz.Record.Starting="Iniciando…"
Voltz.Record.Stopping="Finalizando…"
Voltz.Record.PauseUnavailable="Este formato de gravação não permite pausar."
Voltz.Panel.Scenes="CENAS"
Voltz.Panel.Sources="FONTES"
Voltz.Panel.Audio="ÁUDIO"
Voltz.Sources.Add="Adicionar fonte"
Voltz.Sources.Remove="Remover"
Voltz.Sources.RemoveConfirm.Title="Remover fonte"
Voltz.Sources.RemoveConfirm.Text="Remover \"%1\" desta cena?"
Voltz.Sources.Properties="Propriedades"
Voltz.Sources.Filters="Filtros"
Voltz.Sources.MoveUp="Mover para cima"
Voltz.Sources.MoveDown="Mover para baixo"
Voltz.Sources.ToggleVisible="Mostrar/ocultar"
Voltz.Sources.ToggleLock="Travar/destravar"
Voltz.Sources.Empty="Nenhuma fonte nesta cena. Clique em + para adicionar."
Voltz.Audio.Desktop="Som do sistema"
Voltz.Audio.Mic="Microfone"
Voltz.Audio.Empty="Nenhum dispositivo de áudio configurado."
Voltz.Error.Dismiss="Fechar"
Voltz.Projects.Title="Projetos"
Voltz.Projects.Soon="Em breve: suas gravações aparecem aqui."
Voltz.Presets.Title="Presets"
Voltz.Presets.Soon="Em breve: layouts e configurações prontas."
Voltz.Settings.Title="Ajustes"
Voltz.Settings.OpenClassic="Abrir modo clássico (OBS completo)"
Voltz.About.BasedOn="baseado no OBS Studio"
Voltz.LogUpload.Disabled="O envio de logs está desativado no Voltz Studio. A pasta de logs foi aberta."
Voltz.CrashUpload.Disabled="O envio de relatórios de falha está desativado no Voltz Studio. A pasta de relatórios foi aberta."
```

- [ ] **Step 2: Carregador**

`frontend/voltz/shell/ShellLocale.hpp`:
```cpp
#pragma once

namespace voltz {

// Acrescenta locale/voltz-en-US.ini e locale/voltz-<idioma>.ini ao lookup de textos do OBS.
// Idempotente: só carrega na primeira chamada. Chamar depois do OBSApp::InitLocale.
void loadShellLocale();

} // namespace voltz
```

`frontend/voltz/shell/ShellLocale.cpp`:
```cpp
#include "ShellLocale.hpp"

#include <OBSApp.hpp>
#include <utility/platform.hpp>

#include <util/text-lookup.h>

#include <string>

namespace voltz {

namespace {

bool addLocaleFile(const std::string &language)
{
	std::string path;
	const std::string relative = "locale/voltz-" + language + ".ini";
	if (!GetDataFilePath(relative.c_str(), path)) {
		return false;
	}
	return text_lookup_add(App()->GetTextLookup(), path.c_str());
}

} // namespace

void loadShellLocale()
{
	static bool loaded = false;
	if (loaded) {
		return;
	}
	loaded = true;

	if (!addLocaleFile("en-US")) {
		blog(LOG_WARNING, "[voltz] locale/voltz-en-US.ini nao encontrado");
	}
	const std::string language = App()->GetLocale();
	if (language != "en-US" && !addLocaleFile(language)) {
		blog(LOG_INFO, "[voltz] sem traducao da casca para '%s'; usando ingles", language.c_str());
	}
}

} // namespace voltz
```

Em `frontend/cmake/feature-voltz.cmake`, no bloco `target_sources(obs-studio PRIVATE ...)`, acrescentar
(em ordem alfabética com os demais) `voltz/shell/ShellLocale.cpp` e `voltz/shell/ShellLocale.hpp`.

- [ ] **Step 3: Casca usa os textos**

Em `frontend/voltz/shell/VoltzShell.cpp`:
- adicionar `#include "ShellLocale.hpp"` e `#include <qt-wrappers.hpp>` aos includes;
- primeira linha do construtor (antes de `setObjectName`): `loadShellLocale();`
- trocar as chamadas `aboutLine(currentVersion(), "baseado no OBS Studio")` por
  `aboutLine(currentVersion(), QTStr("Voltz.About.BasedOn").toStdString())`;
- trocar os textos fixos:
  - `QStringLiteral("Projetos")` → `QTStr("Voltz.Projects.Title")`
  - `QStringLiteral("Em breve: suas gravações aparecem aqui.")` → `QTStr("Voltz.Projects.Soon")`
  - `QStringLiteral("Presets")` → `QTStr("Voltz.Presets.Title")`
  - `QStringLiteral("Em breve: layouts e configurações prontas.")` → `QTStr("Voltz.Presets.Soon")`
  - `QStringLiteral("Ajustes")` → `QTStr("Voltz.Settings.Title")`
  - `QStringLiteral("Abrir modo clássico (OBS completo)")` → `QTStr("Voltz.Settings.OpenClassic")`
  - a lista `labels` da barra lateral vira:
    ```cpp
    const QStringList labels = {QTStr("Voltz.Nav.Record"), QTStr("Voltz.Nav.Projects"),
    			    QTStr("Voltz.Nav.Presets"), QTStr("Voltz.Nav.Settings")};
    ```
    (os glifos ●▤◫⚙ saem; os ícones entram na Task 4).

Em `frontend/voltz/shell/RecordPage.cpp`:
- adicionar `#include <qt-wrappers.hpp>`;
- `QStringLiteral("CENAS")` → `QTStr("Voltz.Panel.Scenes")`; `QStringLiteral("FONTES")` → `QTStr("Voltz.Panel.Sources")`;
- em `updateRecordingUi`: `QStringLiteral("■  Parar")` → `QTStr("Voltz.Record.Stop")`,
  `QStringLiteral("●  Gravar")` → `QTStr("Voltz.Record.Start")`, `QStringLiteral("▶  Retomar")` →
  `QTStr("Voltz.Record.Resume")`, `QStringLiteral("⏸  Pausar")` → `QTStr("Voltz.Record.Pause")`;
- em `showRecordingTransition`: `QStringLiteral("Finalizando…")` → `QTStr("Voltz.Record.Stopping")`,
  `QStringLiteral("Iniciando…")` → `QTStr("Voltz.Record.Starting")`.
- o texto provisório `"Fontes e áudio chegam no próximo plano."` fica (some na Task 7).

- [ ] **Step 4: Textos do OBS (upstream) traduzíveis**

Em `frontend/dialogs/OBSAbout.cpp` (bloco Voltz de `ShowAbout`): trocar `"baseado no OBS Studio"` por
`QTStr("Voltz.About.BasedOn").toStdString()`. O "Sobre" só abre depois da casca existir, então os textos já
estão carregados.

Em `frontend/widgets/OBSBasic_MainControls.cpp` (bloco Voltz de `UploadLog`): a mensagem fixa em português
(variável `uploadDisabledMsg`) passa a ser `QTStr("Voltz.LogUpload.Disabled")`.

Em `frontend/utility/CrashHandler.cpp` (bloco Voltz de `uploadCrashLogToServer`): a mensagem fixa em
português passa a ser `QTStr("Voltz.CrashUpload.Disabled")` (o arquivo já inclui `qt-wrappers.hpp`).

- [ ] **Step 5: Configurar, compilar, verificar**

Run: CONFIGURAR, COMPILAR APP
Expected: `exit=0`.

```bash
ls build_x64/rundir/RelWithDebInfo/data/obs-studio/locale/ | grep voltz
grep -rn "QStringLiteral(\"[A-ZÁÉÍÓÚa-z]" frontend/voltz/shell/*.cpp | grep -v "Fontes e áudio chegam" | grep -v ":/voltz" 
```
Expected: `voltz-en-US.ini` e `voltz-pt-BR.ini` listados; o grep não acha textos de interface fixos (só caminhos
de recurso, se houver).

Run: SMOKE → `SMOKE OK`. Depois:
```bash
grep -c "voltz-en-US.ini nao encontrado" "build_x64/rundir/RelWithDebInfo/config/Voltz Studio/logs/"*.txt
```
Expected: `0`.

- [ ] **Step 6: Commit**

```bash
git add frontend/data/locale/voltz-en-US.ini frontend/data/locale/voltz-pt-BR.ini frontend/voltz/shell frontend/cmake/feature-voltz.cmake frontend/dialogs/OBSAbout.cpp frontend/widgets/OBSBasic_MainControls.cpp frontend/utility/CrashHandler.cpp
git commit -m "feat(voltz): textos da casca em pt-BR e en-US via lookup do OBS

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Ícones Phosphor e fonte DM Sans

**Files:**
- Create: `frontend/voltz/shell/icons/regular/*.svg`, `icons/fill/*.svg`, `icons/LICENSE-phosphor.txt`
- Create: `frontend/voltz/shell/fonts/DMSans-Regular.ttf`, `DMSans-Medium.ttf`, `DMSans-SemiBold.ttf`, `DMSans-Bold.ttf`, `fonts/OFL.txt`
- Create: `frontend/voltz/shell/Icons.hpp`, `Icons.cpp`
- Modify: `frontend/voltz/shell/voltz-shell.qrc`, `frontend/cmake/feature-voltz.cmake`

- [ ] **Step 1: Vendorizar os ícones (versão fixa 2.1.1)**

```bash
TMP="$(mktemp -d)" && cd "$TMP" && npm pack @phosphor-icons/core@2.1.1 --silent && tar -xzf phosphor-icons-core-2.1.1.tgz && cd - >/dev/null
DEST=frontend/voltz/shell/icons
mkdir -p "$DEST/regular" "$DEST/fill"
for n in record stop pause play folder-simple squares-four gear-six monitor webcam microphone speaker-high eye eye-slash lock-simple lock-simple-open plus trash caret-up caret-down sliders-horizontal faders warning-circle x image text-t app-window video-camera; do
  cp "$TMP/package/assets/regular/$n.svg" "$DEST/regular/$n.svg"
  cp "$TMP/package/assets/fill/$n-fill.svg" "$DEST/fill/$n-fill.svg"
done
cp "$TMP/package/LICENSE" "$DEST/LICENSE-phosphor.txt"
ls "$DEST/regular" | wc -l; ls "$DEST/fill" | wc -l; head -c 80 "$DEST/regular/record.svg"; echo
rm -rf "$TMP"
```
Expected: `27` e `27`; o SVG começa com `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" fill="currentColor">`.

- [ ] **Step 2: Vendorizar a DM Sans (versão fixa 0.4.2)**

```bash
TMP="$(mktemp -d)" && cd "$TMP" && npm pack @expo-google-fonts/dm-sans@0.4.2 --silent && tar -xzf expo-google-fonts-dm-sans-0.4.2.tgz && cd - >/dev/null
DEST=frontend/voltz/shell/fonts
mkdir -p "$DEST"
cp "$TMP/package/400Regular/DMSans_400Regular.ttf" "$DEST/DMSans-Regular.ttf"
cp "$TMP/package/500Medium/DMSans_500Medium.ttf" "$DEST/DMSans-Medium.ttf"
cp "$TMP/package/600SemiBold/DMSans_600SemiBold.ttf" "$DEST/DMSans-SemiBold.ttf"
cp "$TMP/package/700Bold/DMSans_700Bold.ttf" "$DEST/DMSans-Bold.ttf"
cp "$TMP/package/LICENSE_FONT" "$DEST/OFL.txt"
ls -la "$DEST"; head -3 "$DEST/OFL.txt"
rm -rf "$TMP"
```
Expected: 4 `.ttf` (dezenas de KB cada) e `OFL.txt` mencionando "SIL Open Font License".

- [ ] **Step 3: Recursos Qt**

Substituir `frontend/voltz/shell/voltz-shell.qrc` por (gere a lista de ícones com o comando abaixo para não
errar nomes):
```bash
{
  echo '<RCC>'
  echo '  <qresource prefix="/voltz">'
  echo '    <file>voltz-shell.qss</file>'
  for f in fonts/DMSans-Regular.ttf fonts/DMSans-Medium.ttf fonts/DMSans-SemiBold.ttf fonts/DMSans-Bold.ttf; do echo "    <file>$f</file>"; done
  for f in $(cd frontend/voltz/shell && ls icons/regular/*.svg icons/fill/*.svg); do echo "    <file>$f</file>"; done
  echo '  </qresource>'
  echo '</RCC>'
} > frontend/voltz/shell/voltz-shell.qrc
grep -c "<file>" frontend/voltz/shell/voltz-shell.qrc
```
Expected: `59` (1 qss + 4 fontes + 54 ícones).

- [ ] **Step 4: `Icons`**

`frontend/voltz/shell/Icons.hpp`:
```cpp
#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace voltz {

namespace palette {
inline const QColor kText(0xF2, 0xF2, 0xF3);
inline const QColor kMuted(0x82, 0x82, 0x8C);
inline const QColor kAccent(0xD4, 0xF5, 0x3C);
inline const QColor kRec(0xFF, 0x4D, 0x4D);
inline const QColor kOnLight(0x09, 0x09, 0x0B);
} // namespace palette

enum class IconWeight { Regular, Fill };

// Ícone Phosphor (":/voltz/icons/...") recolorido. `name` sem sufixo, ex. "microphone".
QIcon phosphorIcon(const QString &name, const QColor &color, IconWeight weight = IconWeight::Regular);

// Ícone de dois estados para botões checáveis: Off = Regular na cor `off`, On = Fill na cor `on`.
QIcon phosphorToggleIcon(const QString &name, const QColor &off, const QColor &on);

// Registra a DM Sans (4 pesos) no QFontDatabase. Idempotente.
void loadShellFonts();

} // namespace voltz
```

`frontend/voltz/shell/Icons.cpp`:
```cpp
#include "Icons.hpp"

#include <voltz/core/svg-recolor.hpp>

#include <obs.h>

#include <QFile>
#include <QFontDatabase>
#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace voltz {

namespace {

constexpr int kSizes[] = {16, 20, 24, 32, 40, 48};

QString resourcePath(const QString &name, IconWeight weight)
{
	return weight == IconWeight::Fill ? QStringLiteral(":/voltz/icons/fill/%1-fill.svg").arg(name)
					  : QStringLiteral(":/voltz/icons/regular/%1.svg").arg(name);
}

QByteArray recoloredSvg(const QString &name, const QColor &color, IconWeight weight)
{
	QFile file(resourcePath(name, weight));
	if (!file.open(QIODevice::ReadOnly)) {
		blog(LOG_WARNING, "[voltz] icone ausente: %s", qUtf8Printable(file.fileName()));
		return {};
	}
	const std::string svg = file.readAll().toStdString();
	return QByteArray::fromStdString(recolorSvg(svg, color.name(QColor::HexRgb).toStdString()));
}

void addPixmaps(QIcon &icon, const QByteArray &svg, QIcon::State state)
{
	if (svg.isEmpty()) {
		return;
	}
	QSvgRenderer renderer(svg);
	for (int size : kSizes) {
		QPixmap pixmap(size, size);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		renderer.render(&painter);
		painter.end();
		icon.addPixmap(pixmap, QIcon::Normal, state);
	}
}

} // namespace

QIcon phosphorIcon(const QString &name, const QColor &color, IconWeight weight)
{
	static QHash<QString, QIcon> cache;
	const QString key = QStringLiteral("%1|%2|%3").arg(name, color.name()).arg(int(weight));
	auto it = cache.find(key);
	if (it != cache.end()) {
		return it.value();
	}
	QIcon icon;
	addPixmaps(icon, recoloredSvg(name, color, weight), QIcon::Off);
	cache.insert(key, icon);
	return icon;
}

QIcon phosphorToggleIcon(const QString &name, const QColor &off, const QColor &on)
{
	QIcon icon;
	addPixmaps(icon, recoloredSvg(name, off, IconWeight::Regular), QIcon::Off);
	addPixmaps(icon, recoloredSvg(name, on, IconWeight::Fill), QIcon::On);
	return icon;
}

void loadShellFonts()
{
	static bool loaded = false;
	if (loaded) {
		return;
	}
	loaded = true;
	const char *files[] = {":/voltz/fonts/DMSans-Regular.ttf", ":/voltz/fonts/DMSans-Medium.ttf",
			       ":/voltz/fonts/DMSans-SemiBold.ttf", ":/voltz/fonts/DMSans-Bold.ttf"};
	for (const char *file : files) {
		if (QFontDatabase::addApplicationFont(QString::fromLatin1(file)) < 0) {
			blog(LOG_WARNING, "[voltz] falha ao carregar fonte %s", file);
		}
	}
}

} // namespace voltz
```

Em `frontend/cmake/feature-voltz.cmake`, acrescentar `voltz/shell/Icons.cpp` e `voltz/shell/Icons.hpp` ao
`target_sources(obs-studio ...)`.

- [ ] **Step 5: Carregar as fontes na casca**

Em `frontend/voltz/shell/VoltzShell.cpp`: `#include "Icons.hpp"` e, logo após `loadShellLocale();` no
construtor, `loadShellFonts();`.

- [ ] **Step 6: Configurar, compilar, smoke**

Run: CONFIGURAR, COMPILAR APP, SMOKE
Expected: `exit=0`, `SMOKE OK`, e no log do smoke nenhuma linha `[voltz] falha ao carregar fonte` nem
`[voltz] icone ausente`:
```bash
grep -E "\[voltz\] (falha ao carregar fonte|icone ausente)" "build_x64/rundir/RelWithDebInfo/config/Voltz Studio/logs/"*.txt | wc -l
```
Expected: `0`.

- [ ] **Step 7: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): icones Phosphor 2.1.1 recoloridos e fonte DM Sans embutidos na casca

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Janela — barra de título escura, monitor/HDR, tema isolado e navegação com ícones

**Files:**
- Create: `frontend/voltz/shell/WindowChrome.hpp`, `WindowChrome.cpp`
- Modify: `frontend/voltz/shell/VoltzShell.hpp/.cpp`, `frontend/voltz/shell/voltz-shell.qss`, `frontend/cmake/feature-voltz.cmake`

O tema do OBS é aplicado na aplicação inteira (`QWidget`, `QPushButton { max-height: ... }`, `QListWidget`...).
Por isso todas as regras da casca passam a ser prefixadas com `#voltzShell` (objectName da janela) e
sobrescrevem explicitamente alturas/margens.

- [ ] **Step 1: `WindowChrome`**

`frontend/voltz/shell/WindowChrome.hpp`:
```cpp
#pragma once

class QWidget;

namespace voltz {

// Barra de título nativa escura (Windows 10 20H1+/11: modo escuro; Windows 11: cor da barra e do texto).
// No-op fora do Windows.
void applyDarkTitleBar(QWidget *window);

// Repassa WM_MOVE/WM_DISPLAYCHANGE aos OBSQTDisplay filhos de `window` (cor/HDR acompanham o monitor),
// como o OBSBasic::nativeEvent faz. Chamar de dentro do nativeEvent da janela.
void forwardDisplayEvents(QWidget *window, void *message);

} // namespace voltz
```

`frontend/voltz/shell/WindowChrome.cpp`:
```cpp
#include "WindowChrome.hpp"

#include <widgets/OBSQTDisplay.hpp>

#include <QWidget>

#ifdef _WIN32
#include <Windows.h>
#include <dwmapi.h>
#endif

namespace voltz {

void applyDarkTitleBar(QWidget *window)
{
#ifdef _WIN32
	HWND hwnd = reinterpret_cast<HWND>(window->winId());
	const BOOL dark = TRUE;
	DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
	const COLORREF caption = RGB(0x11, 0x11, 0x14);
	DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));
	const COLORREF text = RGB(0xF2, 0xF2, 0xF3);
	DwmSetWindowAttribute(hwnd, 36 /* DWMWA_TEXT_COLOR */, &text, sizeof(text));
#else
	(void)window;
#endif
}

void forwardDisplayEvents(QWidget *window, void *message)
{
#ifdef _WIN32
	const MSG &msg = *static_cast<MSG *>(message);
	if (msg.message == WM_MOVE) {
		for (OBSQTDisplay *const display : window->findChildren<OBSQTDisplay *>()) {
			display->OnMove();
		}
	} else if (msg.message == WM_DISPLAYCHANGE) {
		for (OBSQTDisplay *const display : window->findChildren<OBSQTDisplay *>()) {
			display->OnDisplayChange();
		}
	}
#else
	(void)window;
	(void)message;
#endif
}

} // namespace voltz
```

Em `frontend/cmake/feature-voltz.cmake`: acrescentar `voltz/shell/WindowChrome.cpp` e
`voltz/shell/WindowChrome.hpp` ao `target_sources`, e no final do arquivo:
```cmake
if(OS_WINDOWS)
  target_link_libraries(obs-studio PRIVATE dwmapi)
endif()
```

- [ ] **Step 2: `VoltzShell` — nativeEvent, chrome e ícones da navegação**

Em `frontend/voltz/shell/VoltzShell.hpp`, na seção `protected:`, depois de `closeEvent`, acrescentar:
```cpp
	bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
```

Em `frontend/voltz/shell/VoltzShell.cpp`:
- `#include "WindowChrome.hpp"` nos includes;
- no fim do construtor, antes do `blog(...)`: `applyDarkTitleBar(this);`
- implementar (antes de `closeEvent`):
```cpp
bool VoltzShell::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
	forwardDisplayEvents(this, message);
	return QMainWindow::nativeEvent(eventType, message, result);
}
```
- em `buildSideNav`, trocar o laço de criação dos botões por (mantendo `labels` da Task 2):
```cpp
	const QStringList icons = {QStringLiteral("record"), QStringLiteral("folder-simple"),
				   QStringLiteral("squares-four"), QStringLiteral("gear-six")};
	for (int i = 0; i < labels.size(); i++) {
		auto *button = new QToolButton(nav);
		button->setText(labels[i]);
		button->setIcon(phosphorToggleIcon(icons[i], palette::kMuted, palette::kAccent));
		button->setIconSize(QSize(22, 22));
		button->setCheckable(true);
		button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		navGroup_->addButton(button, i);
		if (i == labels.size() - 1) {
			layout->addStretch(1);
		}
		layout->addWidget(button);
	}
```

- [ ] **Step 3: Tema isolado**

Substituir `frontend/voltz/shell/voltz-shell.qss` por:
```css
/* Todas as regras prefixadas com #voltzShell para vencer o tema global do OBS (Yami etc.). */

#voltzShell, #voltzShell QWidget {
	background: #09090B;
	color: #F2F2F3;
	font-family: "DM Sans", "Segoe UI";
	font-size: 13px;
}

#voltzShell QLabel {
	background: transparent;
}

#voltzShell QFrame#sideNav {
	background: #111114;
	border: none;
	border-right: 1px solid #1F1F24;
}

#voltzShell QFrame#sideNav QToolButton {
	background: transparent;
	color: #82828C;
	border: none;
	border-radius: 8px;
	padding: 8px 2px 6px 2px;
	margin: 0;
	font-size: 11px;
	font-weight: 500;
}

#voltzShell QFrame#sideNav QToolButton:hover {
	color: #F2F2F3;
	background: #18181C;
}

#voltzShell QFrame#sideNav QToolButton:checked {
	color: #D4F53C;
	background: rgba(212, 245, 60, 26);
}

#voltzShell QFrame#panel {
	background: #111114;
	border: 1px solid #1F1F24;
	border-radius: 8px;
}

#voltzShell QFrame#panel QWidget {
	background: transparent;
}

#voltzShell QLabel#panelTitle {
	color: #82828C;
	font-size: 10px;
	font-weight: 600;
}

#voltzShell QLabel#muted {
	color: #82828C;
}

#voltzShell QLabel#pageTitle {
	font-size: 20px;
	font-weight: 700;
}

#voltzShell QListWidget {
	background: transparent;
	border: none;
	outline: none;
	padding: 0;
}

#voltzShell QListWidget::item {
	padding: 6px 8px;
	border: none;
	border-radius: 4px;
	color: #F2F2F3;
}

#voltzShell QListWidget::item:hover {
	background: #18181C;
}

#voltzShell QListWidget::item:selected {
	background: rgba(212, 245, 60, 26);
	color: #F2F2F3;
	border-left: 2px solid #D4F53C;
}

#voltzShell QFrame#bottomBar {
	background: #111114;
	border: none;
	border-top: 1px solid #1F1F24;
}

#voltzShell QPushButton {
	background: #18181C;
	color: #F2F2F3;
	border: 1px solid #1F1F24;
	border-radius: 16px;
	padding: 7px 16px;
	margin: 0;
	min-height: 18px;
	max-height: 16777215px;
}

#voltzShell QPushButton:hover {
	border-color: #3A3A42;
}

#voltzShell QPushButton:disabled {
	color: #4A4A52;
}

#voltzShell QPushButton#recordButton {
	background: #FF4D4D;
	color: #FFFFFF;
	border: none;
	font-weight: 600;
	padding: 8px 22px;
}

#voltzShell QPushButton#recordButton[recording="true"] {
	background: #F2F2F3;
	color: #09090B;
}

#voltzShell QPushButton#recordButton:disabled {
	background: #3A3A42;
	color: #82828C;
}

#voltzShell QToolButton#iconButton {
	background: transparent;
	border: none;
	border-radius: 6px;
	padding: 4px;
	margin: 0;
}

#voltzShell QToolButton#iconButton:hover {
	background: #18181C;
}

#voltzShell QToolButton#iconButton:disabled {
	background: transparent;
}

#voltzShell QFrame#errorBanner {
	background: rgba(255, 77, 77, 31);
	border: 1px solid rgba(255, 77, 77, 102);
	border-radius: 8px;
}

#voltzShell QFrame#errorBanner QLabel {
	color: #F2F2F3;
}

#voltzShell QLabel#errorTitle {
	font-weight: 600;
}

#voltzShell QLabel#timeLabel {
	color: #82828C;
	font-family: "Cascadia Mono", "Consolas";
}
```

- [ ] **Step 4: Configurar, compilar, smoke**

Run: CONFIGURAR, COMPILAR APP, SMOKE
Expected: `exit=0`, `SMOKE OK`.

- [ ] **Step 5: Conferência visual rápida**

Abrir `build_x64/rundir/RelWithDebInfo/bin/64bit/VoltzStudio.exe --portable --multi` (a partir dessa pasta),
tirar uma captura da janela (PowerShell abaixo) e conferir: barra de título escura; barra lateral com 4 ícones
Phosphor e rótulos em português; item "Gravar" em lima com ícone preenchido; fonte DM Sans.
```powershell
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
[System.Drawing.Graphics]::FromImage($bmp).CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
$bmp.Save("$env:TEMP\voltz-task4.png")
```
Feche o app pela janela. Anexe ao relatório o caminho da captura (fica fora do repositório).

- [ ] **Step 6: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): barra de titulo escura, repasse de monitor/HDR, tema da casca isolado e navegacao com icones

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Cenas por UUID, pausa só quando suportada e `ScenesPanel`

**Files:**
- Modify: `frontend/voltz/shell/FrontendBridge.hpp/.cpp`
- Create: `frontend/voltz/shell/ScenesPanel.hpp`, `ScenesPanel.cpp`
- Modify: `frontend/voltz/shell/RecordPage.hpp/.cpp`, `frontend/cmake/feature-voltz.cmake`

- [ ] **Step 1: `FrontendBridge` — API por UUID e capacidade de pausa**

Em `frontend/voltz/shell/FrontendBridge.hpp`:
- antes da classe, acrescentar:
```cpp
struct SceneEntry {
	QString uuid;
	QString name;
};
```
- trocar
```cpp
	QStringList sceneNames() const;
	QString currentSceneName() const;
	void setCurrentScene(const QString &name);
```
por
```cpp
	QList<SceneEntry> scenes() const;
	QString currentSceneUuid() const;
	void setCurrentSceneByUuid(const QString &uuid);
```
- depois de `void setRecordingPaused(bool paused);` acrescentar `bool canPauseRecording() const;`
- trocar o sinal `void currentSceneChanged(const QString &name);` por `void currentSceneChanged(const QString &uuid);`
- trocar `#include <QStringList>` por `#include <QList>` e `#include <QString>`.

Em `frontend/voltz/shell/FrontendBridge.cpp`, substituir `sceneNames`, `currentSceneName` e `setCurrentScene`
por:
```cpp
QList<SceneEntry> FrontendBridge::scenes() const
{
	QList<SceneEntry> result;
	struct obs_frontend_source_list list = {};
	obs_frontend_get_scenes(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		obs_source_t *source = list.sources.array[i];
		result.append({QString::fromUtf8(obs_source_get_uuid(source)),
			       QString::fromUtf8(obs_source_get_name(source))});
	}
	obs_frontend_source_list_free(&list);
	return result;
}

QString FrontendBridge::currentSceneUuid() const
{
	OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
	return scene ? QString::fromUtf8(obs_source_get_uuid(scene)) : QString();
}

void FrontendBridge::setCurrentSceneByUuid(const QString &uuid)
{
	OBSSourceAutoRelease scene = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (scene) {
		obs_frontend_set_current_scene(scene);
	}
}
```
e acrescentar:
```cpp
bool FrontendBridge::canPauseRecording() const
{
	OBSOutputAutoRelease output = obs_frontend_get_recording_output();
	return output && obs_output_can_pause(output);
}
```
Em `dispatch`, trocar as duas ocorrências de `emit currentSceneChanged(currentSceneName());` por
`emit currentSceneChanged(currentSceneUuid());`.

- [ ] **Step 2: `ScenesPanel`**

`frontend/voltz/shell/ScenesPanel.hpp`:
```cpp
#pragma once

#include <QFrame>

class QListWidget;

namespace voltz {

class FrontendBridge;

// Painel "CENAS": lista as cenas na ordem do OBS e troca a cena atual ao clicar. Identifica por UUID.
class ScenesPanel : public QFrame {
	Q_OBJECT

public:
	explicit ScenesPanel(FrontendBridge *bridge, QWidget *parent = nullptr);

private:
	void refresh();
	void selectScene(const QString &uuid);

	FrontendBridge *bridge_;
	QListWidget *list_ = nullptr;
	bool updating_ = false;
};

} // namespace voltz
```

`frontend/voltz/shell/ScenesPanel.cpp`:
```cpp
#include "ScenesPanel.hpp"

#include "FrontendBridge.hpp"

#include <qt-wrappers.hpp>

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

#include "moc_ScenesPanel.cpp"

namespace voltz {

ScenesPanel::ScenesPanel(FrontendBridge *bridge, QWidget *parent) : QFrame(parent), bridge_(bridge)
{
	setObjectName("panel");

	auto *title = new QLabel(QTStr("Voltz.Panel.Scenes"), this);
	title->setObjectName("panelTitle");
	list_ = new QListWidget(this);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(10, 10, 10, 10);
	layout->setSpacing(8);
	layout->addWidget(title);
	layout->addWidget(list_, 1);

	connect(list_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
		if (!updating_ && current) {
			bridge_->setCurrentSceneByUuid(current->data(Qt::UserRole).toString());
		}
	});
	connect(bridge_, &FrontendBridge::sceneListChanged, this, &ScenesPanel::refresh);
	connect(bridge_, &FrontendBridge::currentSceneChanged, this, &ScenesPanel::selectScene);

	refresh();
}

void ScenesPanel::refresh()
{
	updating_ = true;
	list_->clear();
	for (const SceneEntry &scene : bridge_->scenes()) {
		auto *item = new QListWidgetItem(scene.name, list_);
		item->setData(Qt::UserRole, scene.uuid);
	}
	updating_ = false;
	selectScene(bridge_->currentSceneUuid());
}

void ScenesPanel::selectScene(const QString &uuid)
{
	updating_ = true;
	QListWidgetItem *match = nullptr;
	for (int i = 0; i < list_->count(); i++) {
		if (list_->item(i)->data(Qt::UserRole).toString() == uuid) {
			match = list_->item(i);
			break;
		}
	}
	if (match) {
		list_->setCurrentItem(match);
	} else {
		list_->clearSelection();
		list_->setCurrentItem(nullptr);
	}
	updating_ = false;
}

} // namespace voltz
```

- [ ] **Step 3: `RecordPage` nova (compõe painéis; coluna lateral pronta para Fontes/Áudio)**

Substituir `frontend/voltz/shell/RecordPage.hpp` por:
```cpp
#pragma once

#include <voltz/core/recording-clock.hpp>

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace voltz {

class FrontendBridge;
class ScenesPanel;
class VoltzPreview;

// Tela "Gravar" (layout H2): cenas | prévia | coluna lateral (fontes + áudio), barra inferior com
// Pausar/Gravar/tempo.
class RecordPage : public QWidget {
	Q_OBJECT

public:
	explicit RecordPage(FrontendBridge *bridge, QWidget *parent = nullptr);

private:
	void updateRecordingUi(bool active, bool paused);
	void showRecordingTransition(bool stopping);
	void tickClock();

	FrontendBridge *bridge_;
	ScenesPanel *scenes_ = nullptr;
	VoltzPreview *preview_ = nullptr;
	QVBoxLayout *centerColumn_ = nullptr;
	QVBoxLayout *sideColumn_ = nullptr;
	QPushButton *pauseButton_ = nullptr;
	QPushButton *recordButton_ = nullptr;
	QLabel *timeLabel_ = nullptr;

	QTimer ticker_;
	QElapsedTimer monotonic_;
	RecordingClock recordingClock_;
	bool pendingStart_ = false;
};

} // namespace voltz
```

Substituir `frontend/voltz/shell/RecordPage.cpp` por:
```cpp
#include "RecordPage.hpp"

#include "FrontendBridge.hpp"
#include "Icons.hpp"
#include "ScenesPanel.hpp"
#include "VoltzPreview.hpp"

#include <voltz/core/time-format.hpp>

#include <qt-wrappers.hpp>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include "moc_RecordPage.cpp"

namespace voltz {

RecordPage::RecordPage(FrontendBridge *bridge, QWidget *parent) : QWidget(parent), bridge_(bridge)
{
	monotonic_.start();

	scenes_ = new ScenesPanel(bridge_, this);
	scenes_->setFixedWidth(200);
	preview_ = new VoltzPreview(this);

	centerColumn_ = new QVBoxLayout();
	centerColumn_->setContentsMargins(0, 0, 0, 0);
	centerColumn_->setSpacing(12);
	centerColumn_->addWidget(preview_, 1);

	auto *sideHolder = new QWidget(this);
	sideHolder->setFixedWidth(280);
	sideColumn_ = new QVBoxLayout(sideHolder);
	sideColumn_->setContentsMargins(0, 0, 0, 0);
	sideColumn_->setSpacing(12);
	auto *sourcesHint = new QLabel(QStringLiteral("Fontes e áudio chegam no próximo plano."), sideHolder);
	sourcesHint->setObjectName("muted");
	sourcesHint->setWordWrap(true);
	sideColumn_->addWidget(sourcesHint);
	sideColumn_->addStretch(1);

	auto *body = new QHBoxLayout();
	body->setContentsMargins(12, 12, 12, 12);
	body->setSpacing(12);
	body->addWidget(scenes_);
	body->addLayout(centerColumn_, 1);
	body->addWidget(sideHolder);

	pauseButton_ = new QPushButton(this);
	pauseButton_->setIconSize(QSize(16, 16));
	recordButton_ = new QPushButton(this);
	recordButton_->setObjectName("recordButton");
	recordButton_->setIconSize(QSize(16, 16));
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

	connect(recordButton_, &QPushButton::clicked, this, [this]() {
		if (bridge_->isRecording()) {
			bridge_->stopRecording();
		} else {
			bridge_->startRecording();
		}
	});
	connect(pauseButton_, &QPushButton::clicked, this,
		[this]() { bridge_->setRecordingPaused(!bridge_->isRecordingPaused()); });

	connect(bridge_, &FrontendBridge::recordingStateChanged, this, &RecordPage::updateRecordingUi);
	connect(bridge_, &FrontendBridge::recordingTransition, this, &RecordPage::showRecordingTransition);

	updateRecordingUi(bridge_->isRecording(), bridge_->isRecordingPaused());
}

void RecordPage::updateRecordingUi(bool active, bool paused)
{
	const int64_t now = monotonic_.elapsed();
	if (active && !recordingClock_.running()) {
		recordingClock_.start(now);
	}
	if (active && paused) {
		recordingClock_.pause(now);
	}
	if (active && !paused) {
		recordingClock_.resume(now);
	}
	if (!active) {
		recordingClock_.stop();
	}

	if (active) {
		ticker_.start();
	} else {
		ticker_.stop();
	}

	pendingStart_ = false;
	recordButton_->setEnabled(true);
	recordButton_->setText(active ? QTStr("Voltz.Record.Stop") : QTStr("Voltz.Record.Start"));
	recordButton_->setIcon(active ? phosphorIcon(QStringLiteral("stop"), palette::kOnLight, IconWeight::Fill)
				      : phosphorIcon(QStringLiteral("record"), palette::kText, IconWeight::Fill));
	recordButton_->setProperty("recording", active);
	recordButton_->style()->unpolish(recordButton_);
	recordButton_->style()->polish(recordButton_);

	const bool canPause = active && bridge_->canPauseRecording();
	pauseButton_->setEnabled(canPause);
	pauseButton_->setToolTip(active && !canPause ? QTStr("Voltz.Record.PauseUnavailable") : QString());
	pauseButton_->setText(active && paused ? QTStr("Voltz.Record.Resume") : QTStr("Voltz.Record.Pause"));
	pauseButton_->setIcon(active && paused ? phosphorIcon(QStringLiteral("play"), palette::kText, IconWeight::Fill)
					       : phosphorIcon(QStringLiteral("pause"), palette::kText, IconWeight::Fill));

	tickClock();
}

void RecordPage::showRecordingTransition(bool stopping)
{
	/* Enquanto inicia/finaliza, um segundo clique chamaria StopRecording com a saída em
	 * "stopping" (parada forçada, pode truncar o arquivo): botões travados até STARTED/STOPPED. */
	recordButton_->setEnabled(false);
	pauseButton_->setEnabled(false);
	recordButton_->setText(stopping ? QTStr("Voltz.Record.Stopping") : QTStr("Voltz.Record.Starting"));

	if (!stopping) {
		/* Falha síncrona ao iniciar (OBSBasic::StartRecording emite STARTING, mas o start da saída
		 * falha e não vem STARTED nem STOPPED): sem isto o botão ficaria preso em "Iniciando…". */
		pendingStart_ = true;
		QTimer::singleShot(0, this, [this]() {
			if (pendingStart_ && !bridge_->isRecording()) {
				updateRecordingUi(false, false);
			}
		});
	}
}

void RecordPage::tickClock()
{
	timeLabel_->setText(QString::fromStdString(formatClock(recordingClock_.elapsedMs(monotonic_.elapsed()))));
}

} // namespace voltz
```

Em `frontend/cmake/feature-voltz.cmake`: acrescentar `voltz/shell/ScenesPanel.cpp` e `voltz/shell/ScenesPanel.hpp`.

- [ ] **Step 4: Configurar, compilar, smoke**

Run: CONFIGURAR, COMPILAR APP, SMOKE
Expected: `exit=0`, `SMOKE OK`. Conferir que não sobrou uso da API antiga:
```bash
grep -rn "sceneNames\|currentSceneName\|setCurrentScene(" frontend/voltz/shell | wc -l
```
Expected: `0`.

- [ ] **Step 5: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): cenas por UUID (ScenesPanel), pausa so quando suportada e icones nos botoes de gravacao

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Erros de gravação visíveis na casca (`ErrorBanner`)

**Files:**
- Modify: `frontend/voltz/shell/FrontendBridge.hpp/.cpp`
- Create: `frontend/voltz/shell/ErrorBanner.hpp`, `ErrorBanner.cpp`
- Modify: `frontend/voltz/shell/RecordPage.hpp/.cpp`, `frontend/cmake/feature-voltz.cmake`

Nenhum evento do frontend carrega o código de erro. A saída de gravação (`obs_frontend_get_recording_output()`)
emite o sinal libobs `stop` com `code` (int) e `last_error` (string), numa thread do libobs. A saída é recriada
quando as configurações mudam, então a ponte se reconecta a cada `RECORDING_STARTING`.

- [ ] **Step 1: `FrontendBridge` escuta o `stop` da saída**

Em `frontend/voltz/shell/FrontendBridge.hpp`:
- `#include <obs.hpp>` nos includes;
- novo sinal: `void recordingFailed(int code, const QString &lastError);`
- em `private:`:
```cpp
	void watchRecordingOutput();
	static void handleOutputStop(void *data, calldata_t *params);
	OBSSignal recordingStopSignal_;
```

Em `frontend/voltz/shell/FrontendBridge.cpp`:
- implementar:
```cpp
void FrontendBridge::watchRecordingOutput()
{
	OBSOutputAutoRelease output = obs_frontend_get_recording_output();
	if (!output) {
		recordingStopSignal_.Disconnect();
		return;
	}
	recordingStopSignal_.Connect(obs_output_get_signal_handler(output), "stop", &FrontendBridge::handleOutputStop,
				     this);
}

void FrontendBridge::handleOutputStop(void *data, calldata_t *params)
{
	/* Thread do libobs: só copia os dados e entrega na thread da UI. */
	auto *bridge = static_cast<FrontendBridge *>(data);
	const int code = int(calldata_int(params, "code"));
	const char *error = calldata_string(params, "last_error");
	const QString lastError = error ? QString::fromUtf8(error) : QString();
	if (code == 0) {
		return;
	}
	QMetaObject::invokeMethod(bridge, [bridge, code, lastError]() { emit bridge->recordingFailed(code, lastError); },
				  Qt::QueuedConnection);
}
```
- em `dispatch`, no `case OBS_FRONTEND_EVENT_RECORDING_STARTING:` chamar `watchRecordingOutput();` antes do
  `emit recordingTransition(false);`;
- no `case OBS_FRONTEND_EVENT_EXIT:`, antes de remover o callback: `recordingStopSignal_.Disconnect();`.

- [ ] **Step 2: `ErrorBanner`**

`frontend/voltz/shell/ErrorBanner.hpp`:
```cpp
#pragma once

#include <QFrame>

class QLabel;

namespace voltz {

// Faixa vermelha dispensável com título + mensagem (aceita HTML simples, como os textos do OBS).
class ErrorBanner : public QFrame {
	Q_OBJECT

public:
	explicit ErrorBanner(QWidget *parent = nullptr);

	void showError(const QString &title, const QString &message);

private:
	QLabel *title_ = nullptr;
	QLabel *message_ = nullptr;
};

} // namespace voltz
```

`frontend/voltz/shell/ErrorBanner.cpp`:
```cpp
#include "ErrorBanner.hpp"

#include "Icons.hpp"

#include <qt-wrappers.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

#include "moc_ErrorBanner.cpp"

namespace voltz {

ErrorBanner::ErrorBanner(QWidget *parent) : QFrame(parent)
{
	setObjectName("errorBanner");

	auto *icon = new QLabel(this);
	icon->setPixmap(phosphorIcon(QStringLiteral("warning-circle"), palette::kRec, IconWeight::Fill).pixmap(20, 20));
	icon->setAlignment(Qt::AlignTop);

	title_ = new QLabel(this);
	title_->setObjectName("errorTitle");
	message_ = new QLabel(this);
	message_->setWordWrap(true);
	message_->setTextFormat(Qt::RichText);

	auto *texts = new QVBoxLayout();
	texts->setSpacing(2);
	texts->addWidget(title_);
	texts->addWidget(message_);

	auto *close = new QToolButton(this);
	close->setObjectName("iconButton");
	close->setIcon(phosphorIcon(QStringLiteral("x"), palette::kMuted));
	close->setIconSize(QSize(16, 16));
	close->setToolTip(QTStr("Voltz.Error.Dismiss"));
	connect(close, &QToolButton::clicked, this, &QWidget::hide);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(12, 10, 8, 10);
	layout->setSpacing(10);
	layout->addWidget(icon);
	layout->addLayout(texts, 1);
	layout->addWidget(close, 0, Qt::AlignTop);

	hide();
}

void ErrorBanner::showError(const QString &title, const QString &message)
{
	title_->setText(title);
	message_->setText(message);
	show();
}

} // namespace voltz
```

Em `frontend/cmake/feature-voltz.cmake`: acrescentar `voltz/shell/ErrorBanner.cpp` e `voltz/shell/ErrorBanner.hpp`.

- [ ] **Step 3: `RecordPage` mostra o erro**

Em `frontend/voltz/shell/RecordPage.hpp`:
- `class ErrorBanner;` nas declarações antecipadas (namespace voltz);
- membro `ErrorBanner *errorBanner_ = nullptr;`;
- método privado `void showRecordingError(int code, const QString &lastError);`

Em `frontend/voltz/shell/RecordPage.cpp`:
- includes: `#include "ErrorBanner.hpp"` e `#include <voltz/core/recording-error.hpp>`;
- no construtor, trocar `centerColumn_->addWidget(preview_, 1);` por:
```cpp
	errorBanner_ = new ErrorBanner(this);
	centerColumn_->addWidget(errorBanner_);
	centerColumn_->addWidget(preview_, 1);
```
- junto dos outros `connect(bridge_, ...)`:
```cpp
	connect(bridge_, &FrontendBridge::recordingFailed, this, &RecordPage::showRecordingError);
```
- em `showRecordingTransition`, quando `!stopping` (antes de `pendingStart_ = true;`): `errorBanner_->hide();`
- implementar:
```cpp
void RecordPage::showRecordingError(int code, const QString &lastError)
{
	const RecordingErrorText text = recordingErrorText(classifyRecordingStop(code), !lastError.isEmpty());
	if (text.titleKey.empty()) {
		return;
	}
	QString message = QTStr(text.messageKey.c_str());
	if (text.lastErrorUse == LastErrorUse::FormatArg) {
		message = message.arg(lastError.toHtmlEscaped());
	} else if (text.lastErrorUse == LastErrorUse::Append) {
		message += QStringLiteral("<br><br>") + lastError.toHtmlEscaped();
	}
	blog(LOG_WARNING, "[voltz] erro de gravacao (codigo %d): %s", code, qUtf8Printable(lastError));
	errorBanner_->showError(QTStr(text.titleKey.c_str()), message);
}
```

- [ ] **Step 4: Configurar, compilar, smoke**

Run: CONFIGURAR, COMPILAR APP, SMOKE, RODAR TESTES
Expected: `exit=0`, `SMOKE OK`, `100% tests passed`.

- [ ] **Step 5: Provar o caminho de erro de verdade (disco cheio simulado)**

Simular falta de espaço com um disco virtual pequeno é intrusivo; em vez disso, force um erro de saída
gravando num caminho que deixa de existir no meio da gravação **não é confiável**. O teste aceito aqui é:
1. Unit tests (`recording-error`) cobrem o mapeamento código → texto.
2. Teste manual dirigido: em Ajustes → modo clássico → Configurações → Saída, apontar a gravação para um
   **pendrive**; iniciar gravação na casca; **remover o pendrive** durante a gravação. Esperado: a faixa vermelha
   aparece na casca com o título "Erro de gravação" e o log tem `[voltz] erro de gravacao (codigo ...)`.
Se não houver pendrive disponível, registre no relatório que o passo 2 ficou para o roteiro manual da Task 9.

- [ ] **Step 6: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): erros de gravacao visiveis na casca (sinal stop da saida + ErrorBanner)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Painel de Fontes (`SourcesPanel`)

**Files:**
- Create: `frontend/voltz/shell/SourcesPanel.hpp`, `SourcesPanel.cpp`
- Modify: `frontend/voltz/shell/RecordPage.hpp/.cpp`, `frontend/cmake/feature-voltz.cmake`

Mostra as fontes da cena atual, **camada de cima primeiro** (o `obs_scene_enum_items` vai de baixo para cima).
Escuta os sinais da cena (`item_add`, `item_remove`, `reorder`, `refresh`, `item_visible`, `item_locked`) e o
sinal global `source_rename`; todos chegam em threads do libobs e são repassados à UI com `Qt::QueuedConnection`.
"Adicionar" reaproveita o diálogo do OBS disparando a ação `actionAddSource` do `OBSBasic` (o slot exige uma
QAction como remetente).

- [ ] **Step 1: Header**

`frontend/voltz/shell/SourcesPanel.hpp`:
```cpp
#pragma once

#include <obs.hpp>

#include <QFrame>

class QLabel;
class QListWidget;
class QToolButton;

namespace voltz {

class FrontendBridge;

// Painel "FONTES": fontes da cena atual com mostrar/ocultar, travar, ordem, adicionar, remover,
// propriedades e filtros.
class SourcesPanel : public QFrame {
	Q_OBJECT

public:
	explicit SourcesPanel(FrontendBridge *bridge, QWidget *parent = nullptr);

private slots:
	void refresh();

private:
	void bindCurrentScene();
	obs_scene_t *currentScene() const;
	obs_sceneitem_t *selectedItem() const;
	QWidget *buildRow(obs_sceneitem_t *item);
	void updateButtons();
	void addSource();
	void removeSelected();
	void moveSelected(obs_order_movement movement);
	void openProperties();
	void openFilters();
	void showContextMenu(const QPoint &pos);

	static void handleSceneSignal(void *data, calldata_t *params);

	FrontendBridge *bridge_;
	OBSWeakSource sceneSource_;
	std::vector<OBSSignal> sceneSignals_;
	OBSSignal renameSignal_;

	QListWidget *list_ = nullptr;
	QLabel *empty_ = nullptr;
	QToolButton *removeButton_ = nullptr;
	QToolButton *upButton_ = nullptr;
	QToolButton *downButton_ = nullptr;
	QToolButton *propertiesButton_ = nullptr;
};

} // namespace voltz
```

- [ ] **Step 2: Implementação**

`frontend/voltz/shell/SourcesPanel.cpp`:
```cpp
#include "SourcesPanel.hpp"

#include "FrontendBridge.hpp"
#include "Icons.hpp"

#include <widgets/OBSBasic.hpp>

#include <obs-frontend-api.h>
#include <qt-wrappers.hpp>

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <vector>

#include "moc_SourcesPanel.cpp"

namespace voltz {

namespace {

constexpr int kItemIdRole = Qt::UserRole;

QToolButton *makeIconButton(const QString &icon, const QString &tooltip, QWidget *parent)
{
	auto *button = new QToolButton(parent);
	button->setObjectName("iconButton");
	button->setIcon(phosphorIcon(icon, palette::kMuted));
	button->setIconSize(QSize(16, 16));
	button->setToolTip(tooltip);
	return button;
}

} // namespace

SourcesPanel::SourcesPanel(FrontendBridge *bridge, QWidget *parent) : QFrame(parent), bridge_(bridge)
{
	setObjectName("panel");

	auto *title = new QLabel(QTStr("Voltz.Panel.Sources"), this);
	title->setObjectName("panelTitle");
	QToolButton *addButton = makeIconButton(QStringLiteral("plus"), QTStr("Voltz.Sources.Add"), this);

	auto *header = new QHBoxLayout();
	header->setContentsMargins(0, 0, 0, 0);
	header->addWidget(title, 1);
	header->addWidget(addButton);

	list_ = new QListWidget(this);
	list_->setContextMenuPolicy(Qt::CustomContextMenu);
	empty_ = new QLabel(QTStr("Voltz.Sources.Empty"), this);
	empty_->setObjectName("muted");
	empty_->setWordWrap(true);

	removeButton_ = makeIconButton(QStringLiteral("trash"), QTStr("Voltz.Sources.Remove"), this);
	upButton_ = makeIconButton(QStringLiteral("caret-up"), QTStr("Voltz.Sources.MoveUp"), this);
	downButton_ = makeIconButton(QStringLiteral("caret-down"), QTStr("Voltz.Sources.MoveDown"), this);
	propertiesButton_ =
		makeIconButton(QStringLiteral("sliders-horizontal"), QTStr("Voltz.Sources.Properties"), this);

	auto *footer = new QHBoxLayout();
	footer->setContentsMargins(0, 0, 0, 0);
	footer->setSpacing(2);
	footer->addWidget(removeButton_);
	footer->addWidget(upButton_);
	footer->addWidget(downButton_);
	footer->addStretch(1);
	footer->addWidget(propertiesButton_);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(10, 10, 10, 8);
	layout->setSpacing(6);
	layout->addLayout(header);
	layout->addWidget(empty_);
	layout->addWidget(list_, 1);
	layout->addLayout(footer);

	connect(addButton, &QToolButton::clicked, this, &SourcesPanel::addSource);
	connect(removeButton_, &QToolButton::clicked, this, &SourcesPanel::removeSelected);
	connect(upButton_, &QToolButton::clicked, this, [this]() { moveSelected(OBS_ORDER_MOVE_UP); });
	connect(downButton_, &QToolButton::clicked, this, [this]() { moveSelected(OBS_ORDER_MOVE_DOWN); });
	connect(propertiesButton_, &QToolButton::clicked, this, &SourcesPanel::openProperties);
	connect(list_, &QListWidget::itemDoubleClicked, this, [this]() { openProperties(); });
	connect(list_, &QListWidget::currentRowChanged, this, [this]() { updateButtons(); });
	connect(list_, &QListWidget::customContextMenuRequested, this, &SourcesPanel::showContextMenu);
	connect(bridge_, &FrontendBridge::currentSceneChanged, this, [this]() { bindCurrentScene(); });
	connect(bridge_, &FrontendBridge::sceneListChanged, this, [this]() { bindCurrentScene(); });

	renameSignal_.Connect(obs_get_signal_handler(), "source_rename", &SourcesPanel::handleSceneSignal, this);
	bindCurrentScene();
}

void SourcesPanel::handleSceneSignal(void *data, calldata_t *)
{
	/* Thread do libobs: só agenda o refresh na thread da UI. */
	QMetaObject::invokeMethod(static_cast<SourcesPanel *>(data), "refresh", Qt::QueuedConnection);
}

void SourcesPanel::bindCurrentScene()
{
	sceneSignals_.clear();
	OBSSourceAutoRelease source = obs_frontend_get_current_scene();
	sceneSource_ = OBSGetWeakRef(source);
	if (source) {
		signal_handler_t *handler = obs_source_get_signal_handler(source);
		for (const char *signal : {"item_add", "item_remove", "reorder", "refresh", "item_visible", "item_locked"}) {
			sceneSignals_.emplace_back(handler, signal, &SourcesPanel::handleSceneSignal, this);
		}
	}
	refresh();
}

obs_scene_t *SourcesPanel::currentScene() const
{
	OBSSource source = OBSGetStrongRef(sceneSource_);
	return source ? obs_scene_from_source(source) : nullptr;
}

obs_sceneitem_t *SourcesPanel::selectedItem() const
{
	obs_scene_t *scene = currentScene();
	QListWidgetItem *row = list_->currentItem();
	if (!scene || !row) {
		return nullptr;
	}
	return obs_scene_find_sceneitem_by_id(scene, row->data(kItemIdRole).toLongLong());
}

void SourcesPanel::refresh()
{
	const qint64 selectedId = list_->currentItem() ? list_->currentItem()->data(kItemIdRole).toLongLong() : -1;
	list_->clear();

	std::vector<obs_sceneitem_t *> items;
	if (obs_scene_t *scene = currentScene()) {
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				static_cast<std::vector<obs_sceneitem_t *> *>(param)->push_back(item);
				return true;
			},
			&items);
	}

	/* enum vai de baixo para cima; a lista mostra a camada de cima primeiro */
	for (auto it = items.rbegin(); it != items.rend(); ++it) {
		auto *row = new QListWidgetItem(list_);
		const qint64 id = obs_sceneitem_get_id(*it);
		row->setData(kItemIdRole, id);
		row->setSizeHint(QSize(0, 34));
		list_->setItemWidget(row, buildRow(*it));
		if (id == selectedId) {
			list_->setCurrentItem(row);
		}
	}

	empty_->setVisible(items.empty());
	list_->setVisible(!items.empty());
	updateButtons();
}

QWidget *SourcesPanel::buildRow(obs_sceneitem_t *item)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const qint64 id = obs_sceneitem_get_id(item);

	auto *row = new QWidget(list_);
	auto *layout = new QHBoxLayout(row);
	layout->setContentsMargins(6, 0, 4, 0);
	layout->setSpacing(6);

	auto *icon = new QLabel(row);
	icon->setPixmap(OBSBasic::Get()->GetSourceIcon(obs_source_get_id(source)).pixmap(16, 16));
	auto *name = new QLabel(QString::fromUtf8(obs_source_get_name(source)), row);
	name->setTextInteractionFlags(Qt::NoTextInteraction);

	auto *visible = new QToolButton(row);
	visible->setObjectName("iconButton");
	visible->setCheckable(true);
	visible->setChecked(obs_sceneitem_visible(item));
	visible->setIcon(obs_sceneitem_visible(item) ? phosphorIcon(QStringLiteral("eye"), palette::kText)
						     : phosphorIcon(QStringLiteral("eye-slash"), palette::kMuted));
	visible->setIconSize(QSize(16, 16));
	visible->setToolTip(QTStr("Voltz.Sources.ToggleVisible"));

	auto *lock = new QToolButton(row);
	lock->setObjectName("iconButton");
	lock->setCheckable(true);
	lock->setChecked(obs_sceneitem_locked(item));
	lock->setIcon(obs_sceneitem_locked(item) ? phosphorIcon(QStringLiteral("lock-simple"), palette::kAccent,
							       IconWeight::Fill)
						 : phosphorIcon(QStringLiteral("lock-simple-open"), palette::kMuted));
	lock->setIconSize(QSize(16, 16));
	lock->setToolTip(QTStr("Voltz.Sources.ToggleLock"));

	layout->addWidget(icon);
	layout->addWidget(name, 1);
	layout->addWidget(lock);
	layout->addWidget(visible);

	/* Os botões só mudam o estado no OBS; o sinal da cena dispara refresh() e redesenha a linha. */
	connect(visible, &QToolButton::clicked, this, [this, id](bool checked) {
		if (obs_scene_t *scene = currentScene()) {
			if (obs_sceneitem_t *target = obs_scene_find_sceneitem_by_id(scene, id)) {
				obs_sceneitem_set_visible(target, checked);
			}
		}
	});
	connect(lock, &QToolButton::clicked, this, [this, id](bool checked) {
		if (obs_scene_t *scene = currentScene()) {
			if (obs_sceneitem_t *target = obs_scene_find_sceneitem_by_id(scene, id)) {
				obs_sceneitem_set_locked(target, checked);
			}
		}
	});
	return row;
}

void SourcesPanel::updateButtons()
{
	const int row = list_->currentRow();
	const bool hasSelection = row >= 0;
	removeButton_->setEnabled(hasSelection);
	propertiesButton_->setEnabled(hasSelection);
	upButton_->setEnabled(hasSelection && row > 0);
	downButton_->setEnabled(hasSelection && row < list_->count() - 1);
}

void SourcesPanel::addSource()
{
	/* OBSBasic::AddSourceDialog só abre quando chamado por uma QAction (usa sender()). */
	if (QAction *action = OBSBasic::Get()->findChild<QAction *>(QStringLiteral("actionAddSource"))) {
		action->trigger();
	} else {
		blog(LOG_WARNING, "[voltz] acao actionAddSource nao encontrada");
	}
}

void SourcesPanel::removeSelected()
{
	obs_sceneitem_t *item = selectedItem();
	if (!item) {
		return;
	}
	const QString name = QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item)));
	const auto answer = QMessageBox::question(this, QTStr("Voltz.Sources.RemoveConfirm.Title"),
						  QTStr("Voltz.Sources.RemoveConfirm.Text").arg(name));
	if (answer != QMessageBox::Yes) {
		return;
	}
	if (obs_sceneitem_t *again = selectedItem()) {
		obs_sceneitem_remove(again);
	}
}

void SourcesPanel::moveSelected(obs_order_movement movement)
{
	if (obs_sceneitem_t *item = selectedItem()) {
		obs_sceneitem_set_order(item, movement);
	}
}

void SourcesPanel::openProperties()
{
	if (obs_sceneitem_t *item = selectedItem()) {
		obs_frontend_open_source_properties(obs_sceneitem_get_source(item));
	}
}

void SourcesPanel::openFilters()
{
	if (obs_sceneitem_t *item = selectedItem()) {
		obs_frontend_open_source_filters(obs_sceneitem_get_source(item));
	}
}

void SourcesPanel::showContextMenu(const QPoint &pos)
{
	if (!list_->itemAt(pos)) {
		return;
	}
	QMenu menu(this);
	menu.addAction(QTStr("Voltz.Sources.Properties"), this, &SourcesPanel::openProperties);
	menu.addAction(QTStr("Voltz.Sources.Filters"), this, &SourcesPanel::openFilters);
	menu.addSeparator();
	menu.addAction(QTStr("Voltz.Sources.Remove"), this, &SourcesPanel::removeSelected);
	menu.exec(list_->viewport()->mapToGlobal(pos));
}

} // namespace voltz
```

> Observação para o implementador: `OBSWeakSource`, `OBSGetWeakRef` (→ `OBSWeakSource`), `OBSGetStrongRef`
> (→ `OBSSource`, não AutoRelease), `OBSSourceAutoRelease` e `OBSSignal` (movível, pode ficar em `std::vector`)
> vêm de `libobs/obs.hpp`. Se algum nome divergir, use o equivalente de `obs.hpp` e reporte.

Em `frontend/cmake/feature-voltz.cmake`: acrescentar `voltz/shell/SourcesPanel.cpp` e `voltz/shell/SourcesPanel.hpp`.

- [ ] **Step 3: Colocar o painel na coluna lateral**

Em `frontend/voltz/shell/RecordPage.cpp`:
- `#include "SourcesPanel.hpp"`;
- no construtor, trocar:
```cpp
	auto *sourcesHint = new QLabel(QStringLiteral("Fontes e áudio chegam no próximo plano."), sideHolder);
	sourcesHint->setObjectName("muted");
	sourcesHint->setWordWrap(true);
	sideColumn_->addWidget(sourcesHint);
	sideColumn_->addStretch(1);
```
por:
```cpp
	sideColumn_->addWidget(new SourcesPanel(bridge_, sideHolder), 3);
```

- [ ] **Step 4: Configurar, compilar, smoke**

Run: CONFIGURAR, COMPILAR APP, SMOKE
Expected: `exit=0`, `SMOKE OK`.

- [ ] **Step 5: Teste manual dirigido**

Abrir `VoltzStudio.exe --portable --multi` (config limpa: apague `build_x64/rundir/RelWithDebInfo/config` antes).
1. Painel FONTES mostra "Nenhuma fonte nesta cena…". 
2. `+` abre o diálogo "Adicionar fonte" do OBS; adicionar "Captura de tela" → aparece na lista com ícone e a prévia mostra a tela.
3. Adicionar uma "Imagem" ou "Texto" → ela aparece **em cima** da lista.
4. Olho alterna a visibilidade (prévia reflete); cadeado alterna e muda o ícone.
5. Setas mudam a ordem; duplo clique abre Propriedades; botão direito → Filtros abre o diálogo de filtros.
6. Lixeira pede confirmação e remove.
7. Trocar de cena no painel CENAS → FONTES muda para a nova cena.
Registre OK/FALHOU por item no relatório.

- [ ] **Step 6: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): painel de fontes da cena atual (visibilidade, trava, ordem, adicionar, remover, propriedades, filtros)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Painel de Áudio (`AudioPanel`)

**Files:**
- Create: `frontend/voltz/shell/AudioPanel.hpp`, `AudioPanel.cpp`
- Modify: `frontend/voltz/shell/RecordPage.cpp`, `frontend/cmake/feature-voltz.cmake`

Usa o `VolumeControl` do próprio OBS (medidor + volume + mudo) para os canais globais 1 ("Som do sistema") e
3 ("Microfone"), configurado como no `AudioMixer::createVolumeControl`. Reconstrói quando o sinal global
`channel_change` indica troca de dispositivo.

- [ ] **Step 1: Header**

`frontend/voltz/shell/AudioPanel.hpp`:
```cpp
#pragma once

#include <obs.hpp>

#include <QFrame>

class QLabel;
class QVBoxLayout;

namespace voltz {

// Painel "ÁUDIO": som do sistema (canal 1) e microfone (canal 3) com medidor, volume e mudo.
class AudioPanel : public QFrame {
	Q_OBJECT

public:
	explicit AudioPanel(QWidget *parent = nullptr);

private slots:
	void rebuild();

private:
	void addChannel(uint32_t channel, const QString &label);
	static void handleChannelChange(void *data, calldata_t *params);

	QVBoxLayout *controls_ = nullptr;
	QLabel *empty_ = nullptr;
	OBSSignal channelSignal_;
};

} // namespace voltz
```

- [ ] **Step 2: Implementação**

`frontend/voltz/shell/AudioPanel.cpp`:
```cpp
#include "AudioPanel.hpp"

#include <components/VolumeControl.hpp>
#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>

#include <QLabel>
#include <QLayoutItem>
#include <QVBoxLayout>

#include "moc_AudioPanel.cpp"

namespace voltz {

namespace {

constexpr uint32_t kDesktopChannel = 1;
constexpr uint32_t kMicChannel = 3;

} // namespace

AudioPanel::AudioPanel(QWidget *parent) : QFrame(parent)
{
	setObjectName("panel");

	auto *title = new QLabel(QTStr("Voltz.Panel.Audio"), this);
	title->setObjectName("panelTitle");
	empty_ = new QLabel(QTStr("Voltz.Audio.Empty"), this);
	empty_->setObjectName("muted");
	empty_->setWordWrap(true);

	controls_ = new QVBoxLayout();
	controls_->setContentsMargins(0, 0, 0, 0);
	controls_->setSpacing(6);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(10, 10, 10, 10);
	layout->setSpacing(8);
	layout->addWidget(title);
	layout->addWidget(empty_);
	layout->addLayout(controls_);
	layout->addStretch(1);

	channelSignal_.Connect(obs_get_signal_handler(), "channel_change", &AudioPanel::handleChannelChange, this);
	rebuild();
}

void AudioPanel::handleChannelChange(void *data, calldata_t *params)
{
	const long long channel = calldata_int(params, "channel");
	if (channel != kDesktopChannel && channel != kMicChannel) {
		return;
	}
	QMetaObject::invokeMethod(static_cast<AudioPanel *>(data), "rebuild", Qt::QueuedConnection);
}

void AudioPanel::rebuild()
{
	while (QLayoutItem *item = controls_->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	addChannel(kDesktopChannel, QTStr("Voltz.Audio.Desktop"));
	addChannel(kMicChannel, QTStr("Voltz.Audio.Mic"));
	empty_->setVisible(controls_->count() == 0);
}

void AudioPanel::addChannel(uint32_t channel, const QString &label)
{
	OBSSourceAutoRelease source = obs_get_output_source(channel);
	if (!source) {
		return;
	}

	auto *control = new VolumeControl(source, this, false);
	control->setName(label);
	control->setGlobalInMixer(true);
	control->enableSlider(true);

	config_t *config = OBSBasic::Get()->Config();
	control->setMeterDecayRate(config_get_double(config, "Audio", "MeterDecayRate"));
	control->setPeakMeterType(config_get_uint(config, "Audio", "PeakMeterType") == 1 ? TRUE_PEAK_METER
											 : SAMPLE_PEAK_METER);
	controls_->addWidget(control);
}

} // namespace voltz
```

> Observação para o implementador: confira em `frontend/components/VolumeControl.hpp` que `setName`,
> `setGlobalInMixer`, `enableSlider`, `setMeterDecayRate` e `setPeakMeterType` são públicos e com essas
> assinaturas; se `setName` não substituir o nome exibido (o OBS pode reler o nome da fonte), mantenha o nome da
> fonte e reporte. Não altere `VolumeControl`.

Em `frontend/cmake/feature-voltz.cmake`: acrescentar `voltz/shell/AudioPanel.cpp` e `voltz/shell/AudioPanel.hpp`.

- [ ] **Step 3: Colocar o painel na coluna lateral**

Em `frontend/voltz/shell/RecordPage.cpp`: `#include "AudioPanel.hpp"` e, logo após
`sideColumn_->addWidget(new SourcesPanel(bridge_, sideHolder), 3);`:
```cpp
	sideColumn_->addWidget(new AudioPanel(sideHolder), 2);
```

- [ ] **Step 4: Configurar, compilar, smoke**

Run: CONFIGURAR, COMPILAR APP, SMOKE
Expected: `exit=0`, `SMOKE OK`.

- [ ] **Step 5: Teste manual dirigido**

Com o app aberto (config portátil limpa):
1. Painel ÁUDIO mostra "Som do sistema" e "Microfone" (se houver dispositivos padrão) com medidores se mexendo
   ao tocar um som / falar.
2. Arrastar o volume e clicar em mudo funciona; o modo clássico reflete as mudanças no mixer.
3. No modo clássico → Configurações → Áudio, desativar o microfone → o controle "Microfone" some da casca;
   reativar → volta.
Registre OK/FALHOU por item.

- [ ] **Step 6: Commit**

```bash
git add frontend/voltz/shell frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): painel de audio com som do sistema e microfone (VolumeControl do OBS)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Verificação, registro e memória

**Files:**
- Modify: `docs/voltz/upstream-patches.md`, `docs/voltz/backlog-plano-2.md`, `CLAUDE.md`, `AGENTS.md`

- [ ] **Step 1: Automático**

Run: COMPILAR TUDO, RODAR TESTES, SMOKE, `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-record.ps1`
Expected: `exit=0`, `100% tests passed` (34 casos no executável), `SMOKE OK`, `SMOKE GRAVACAO OK`.

- [ ] **Step 2: Captura de tela da tela Gravar completa**

Com config portátil limpa, abrir o app, adicionar uma "Captura de tela" pelo `+` do painel FONTES e tirar a
captura (PowerShell da Task 4, salvando em `$env:TEMP\voltz-plano2a.png`). Fechar o app pela janela.
Descrever no relatório: layout H2 (cenas | prévia | fontes+áudio), barra inferior com Pausar/Gravar com ícones,
barra de título escura, fonte DM Sans, nenhum glifo de texto (●▤◫⚙⏸▶) na UI.

- [ ] **Step 3: `upstream-patches.md`**

Atualizar as linhas de `frontend/dialogs/OBSAbout.cpp`, `frontend/widgets/OBSBasic_MainControls.cpp` e
`frontend/utility/CrashHandler.cpp` para dizer que os textos Voltz vêm do locale (`Voltz.*`). Acrescentar uma
linha para `frontend/data/locale/voltz-*.ini` (arquivos novos na pasta de dados do OBS; não alteram os `.ini` do
OBS). Conferir com `git diff ba2f32bdf..HEAD --stat` que nenhum outro arquivo upstream mudou neste plano.

- [ ] **Step 4: Backlog**

Em `docs/voltz/backlog-plano-2.md`, mover para uma seção nova `## Feito no Plano 2A` os itens atendidos:
erros de gravação invisíveis, nativeEvent, cenas por UUID, pausa só quando suportada, ícones Phosphor, DM Sans,
`aboutLine` traduzível, mensagens fixas em português no upload, tema do OBS vazando para a casca
(resumo de 1 linha cada). Os demais itens continuam para o 2B.

- [ ] **Step 5: `CLAUDE.md`**

- **Estado atual:** acrescentar 1 linha: "Plano 2A concluído: tela Gravar completa (cenas por UUID, fontes,
  áudio, erros de gravação visíveis), textos pt-BR/en-US (`frontend/data/locale/voltz-*.ini`), ícones Phosphor,
  DM Sans, barra de título escura."
- **Próximos passos:** item 1 vira "Escrever e executar o Plano 2B (Presets, Projetos, Ajustes, tema `.ovt` do
  clássico, bandeja, diálogos de saída/falha) a partir de `docs/voltz/backlog-plano-2.md`."
- **Armadilhas:** acrescentar (1 linha cada):
  - Regras da casca no QSS precisam do prefixo `#voltzShell` (o tema do OBS é global e tem `QPushButton { max-height }`).
  - Sinais do libobs (cena, saída, `channel_change`, `source_rename`) chegam fora da thread da UI: repassar com `Qt::QueuedConnection`.
  - `OBSBasic::AddSourceDialog` só abre se chamado por QAction → disparar `actionAddSource` com `trigger()`.
  - A saída de gravação é recriada ao mudar configurações: reconectar o sinal `stop` a cada `RECORDING_STARTING`.
- `cp CLAUDE.md AGENTS.md`.

- [ ] **Step 6: Commit e push**

```bash
git add docs/voltz/upstream-patches.md docs/voltz/backlog-plano-2.md CLAUDE.md AGENTS.md
git commit -m "docs(voltz): registro do Plano 2A (patches, backlog, memoria)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push
```
