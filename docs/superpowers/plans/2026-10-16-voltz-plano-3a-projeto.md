# Voltz Studio — Plano 3A: gravação em projeto (manifesto, programa, marcadores, cursor) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Toda gravação do Voltz Studio passa a ser um **projeto**: uma pasta `AAAA-MM-DD HHhMM · Nome` dentro da
pasta de gravações com `programa.mp4` (MP4 híbrido, 3 faixas de áudio mix/mic/sistema, capítulos), `projeto.voltz`
(manifesto v1 da spec §5.2), `marcadores.json` e `cursor.jsonl`; a tela Projetos lista e recupera projetos; a barra
inferior da tela Gravar ganha nome do projeto, ⚑ Marcador, estimativa de espaço e avisos de disco (§8).

**Architecture:** Lógica pura e testada em `frontend/voltz/core/` (escrita atômica, nome da pasta, linha do tempo,
manifesto/marcadores em JSON com nlohmann, conversão de coordenadas e amostragem do cursor, espaço em disco).
`frontend/voltz/project/` (novo, compilado no alvo `obs-studio`) tem o `ProjectRecorder` — reage aos eventos do
frontend do OBS e **não conhece a casca** —, o `CursorTracker` (gancho `WH_MOUSE_LL` em thread própria + thread de
escrita), o `MonitorLocator` e a `ProjectLibrary` (leitura/recuperação para a tela Projetos). **Nenhum patch em
arquivo do OBS:** a gravação cai na pasta do projeto por uma troca temporária do perfil durante o
`OBS_FRONTEND_EVENT_RECORDING_STARTING` (o `OBSBasic::StartRecording` lê a pasta logo depois, na mesma pilha).

**Tech Stack:** C++17, Qt 6.11.1, libobs/frontend 32.2.2, nlohmann_json 3.11 (já nas deps do OBS), doctest, Win32
(`SetWindowsHookExW(WH_MOUSE_LL)`, `EnumDisplayMonitors`, `GetDpiForMonitor`, `MoveFileExW`). Testes de integração
por obs-websocket (`tools/voltz-ws.ps1`), UI Automation (`tools/voltz-uia.ps1`), `ffprobe` e captura **só da janela**.

**Spec:** `docs/superpowers/specs/2026-10-15-voltz-studio-base-gravacao-design.md` §3, §4.2, §4.3, §5, §8, §9 ·
**Backlog:** `docs/voltz/backlog-plano-2.md` (o de gravação passa para `docs/voltz/backlog-plano-3.md`, criado na Task 9).

**Divisão do Plano 3 (decidida pelo controlador):**
- **3A (este):** projeto + programa + marcadores + pausas + cursor + Projetos/recuperação + avisos de disco. O
  manifesto já nasce no formato v1 da spec §5.2, com `arquivos.programa` preenchido e **`tela`/`camera` ausentes**
  (o leitor tolera ausência e preserva campos desconhecidos ao reescrever).
- **3B (depois):** ISO `tela.mkv`/`camera.mkv` via `obs_canvas`, cadeia de encoders para 3 sessões, `offset_ms` dos
  ISO e eventos `iso_falhou`.
- **Plano 4:** anotações (`anotacoes.jsonl`) e destaque do mouse (bloco `mouse` do manifesto).

**Fora do 3A (vai para `backlog-plano-3.md`):** cursor em captura de **janela** (evento `janela`; no 3A só monitor —
sem monitor, o projeto sai sem `cursor.jsonl` e com evento `cursor_indisponivel`); teclas/atalhos no cursor (opcional
na spec, desligado); rolagem horizontal; renomear a **pasta** do projeto; marcadores criados pelo atalho do próprio
OBS ("Adicionar marcador de capítulo") ou pelo `CreateRecordChapter` do websocket (viram capítulo, mas não entram no
`marcadores.json`); fontes de áudio de **cena** (não globais) continuam em todas as faixas; RecTracks no modo
Avançado; Ajustes › Atalhos.

**Descobertas que decidem o desenho (conferidas no código 32.2.2):**
- `OBSBasic::StartRecording` (`frontend/widgets/OBSBasic_Recording.cpp:113-137`) emite
  `OBS_FRONTEND_EVENT_RECORDING_STARTING` **antes** de `outputHandler->StartRecording()`, na mesma pilha da thread da
  UI; esse por sua vez chama `ConfigureRecording`, que lê `SimpleOutput/FilePath`, `Output/FilenameFormatting` e
  `Output/OverwriteIfExists` (`frontend/utility/SimpleOutput.cpp:824-857`; no modo Avançado,
  `AdvOut/RecFilePath`|`FFFilePath` em `AdvancedOutput.cpp:821-824`). `GetOutputFilename` (`frontend/OBSApp.cpp`)
  monta `<pasta>/<formato>.<ext>`. ⇒ trocar pasta + `FilenameFormatting=programa` no STARTING e desfazer no próximo
  giro do laço de eventos põe o `programa.mp4` na pasta do projeto **sem patch** e sem o perfil salvo ver a troca.
  Vale para qualquer origem (casca, clássico, atalho, websocket, `--startrecording`).
- **Multi-faixa no modo Simples já existe:** com qualidade `HQ`/`Small` o `SimpleOutput` cria 6 encoders AAC
  (`SimpleOutput.cpp:171-184`) e liga os que estão nos bits de `SimpleOutput/RecTracks`
  (`SimpleOutput.cpp:771-803`; lido a cada início). Padrão do OBS: `RecTracks = 1` (`OBSBasic.cpp:769`); só a
  qualidade `Stream` (ou FLV) grava 1 faixa. ⇒ **ficamos no modo Simples** e ligamos `RecTracks = 7` (faixas 1-3)
  quando ainda é o padrão.
- Mixers das fontes: padrão `0xFF` ao criar (`libobs/obs-source.c:260`) e `0x3F` ao carregar sem valor
  (`libobs/obs.c:2315`) — "todas as faixas". A rota Voltz (sistema → 1+3, microfone → 1+2) só é aplicada a fontes
  **globais** ainda em "todas as faixas" (não sobrescreve escolha do usuário).
- Capítulos: `obs_frontend_recording_add_chapter` (`frontend/OBSStudioAPI.cpp:279-294`) recusa com a gravação
  pausada e só existe no MP4/MOV híbrido; a saída loga `Adding chapter "<nome>" at hh:mm:ss.mmm`
  (`plugins/obs-outputs/mp4-output.c:162`) — evidência no log mesmo sem `ffprobe`.
- Atalhos: `obs_hotkey_register_frontend`; o OBS salva atalhos de frontend no perfil `[Hotkeys] <nome> =
  {"bindings":[...]}` (`frontend/settings/OBSBasicSettings.cpp:3742-3746`) e carrega do mesmo jeito
  (`OBSBasic_Hotkeys.cpp:94-112`). `TriggerHotkeyByName` do obs-websocket chama o callback **na thread do websocket**
  (`plugins/obs-websocket/src/requesthandler/RequestHandler_General.cpp:268`) ⇒ o callback só enfileira.
- Eventos do frontend rodam **do último registrado para o primeiro** (`OBSStudioAPI.cpp:752`): criando o
  `ProjectRecorder` depois da `FrontendBridge`, o projeto reage antes da casca.
- No `OBS_FRONTEND_EVENT_EXIT` a saída de gravação ainda está ativa (`OBSBasic.cpp:2089`; ela só é destruída em
  `outputHandler.reset()`, `OBSBasic.cpp:1490`) ⇒ fechar o app gravando fecha o manifesto com `ok` +
  `app_fechado_gravando`.
- AutoRemux com MP4 híbrido: entrada = saída ⇒ `media_remux_job_create` recusa (`libobs/media-io/media-remux.c:145`);
  nada sobrescreve o `programa.mp4`.
- `obs_frontend_get_last_recording()` devolve o caminho escolhido no `GetRecordingFilename`
  (`frontend/utility/BasicOutputHandler.cpp:441-450`) ⇒ nome real do arquivo do programa (ex. `programa.mkv` se o
  usuário escolheu MKV).
- obs-websocket desta versão **não** tem `SetRecordDirectory`; usar `SetProfileParameter`
  (`RequestHandler_Config.cpp:392`). `CreateRecordChapter`, `TriggerHotkeyByName`, `GetHotkeyList`,
  `PauseRecord`/`ResumeRecord` existem.
- `ffprobe` existe nesta máquina (PATH via winget e `.deps/obs-deps-2026-07-15-x64/bin/ffprobe.exe`); não é
  instalado nem distribuído. O app (recuperação) **não** depende dele.
- `nlohmann/json.hpp` 3.11 está em `.deps/obs-deps-2026-07-15-x64/include` e o frontend já usa
  `find_package(nlohmann_json 3.11 REQUIRED)` (`frontend/cmake/os-windows.cmake:13`).
- **Warnings como erro** estão ligados por padrão (`cmake/common/compiler_common.cmake:85`, `/W3`): todo código novo
  (inclusive o `voltz-core` e os testes) precisa compilar sem aviso — casts explícitos em conversões numéricas,
  `WIN32_LEAN_AND_MEAN` sempre com `#ifndef`.
- Qt 6.11: `QDateTime::toOffsetFromUtc` **não** está depreciado (o `toTimeSpec` está).

---

## Decisões técnicas (e o porquê)

| Tema | Decisão | Por quê |
|---|---|---|
| JSON | **nlohmann_json** (`ordered_json`) no `voltz-core`, ligado `PRIVATE`; a API do núcleo só expõe structs e `std::string` | Já está nas deps e no frontend; testável sem libobs/Qt; `ordered_json` mantém a ordem da spec e preserva campos desconhecidos (3B/Plano 4) ao reescrever. `obs_data` exigiria libobs nos testes. |
| `programa.mp4` na pasta | Troca temporária (`ProfileOverride`) de pasta + `Output/FilenameFormatting=programa` no `RECORDING_STARTING`, desfeita com `QTimer::singleShot(0)` | Sem patch no OBS; cobre toda origem de início; `config_remove_value` devolve o padrão sem criar valor do usuário. Alternativa (criar saída própria) duplicaria o `SimpleOutput`. |
| 3 faixas | Continuar no modo **Simples**: `RecTracks=7` se ainda padrão + rota de mixers das fontes globais | O Simples já suporta multi-faixa (HQ/Small); mudar para Avançado obrigaria a casca a gerir encoders. Com `Stream` fica 1 faixa (registrado em `faixas_audio`). |
| Cursor | Gancho `WH_MOUSE_LL` numa **thread só dele** (só enfileira, sem IO) + **thread de escrita** a ~60 Hz (coalescência + conversão + buffer, descarga a cada 1 s) | Pega cliques/rolagem curtos (timer + `GetAsyncKeyState` perde cliques < 16 ms e não vê rolagem); o gancho volta em µs mesmo com disco lento, então o mouse do sistema não atrasa e o Windows não remove o gancho por `LowLevelHooksTimeout`. |
| Coordenadas | Gancho e `EnumDisplayMonitors` em **pixels físicos** (processo PMv2); captura de monitor = retângulo físico do monitor; escala só vai para o manifesto | `MSLLHOOKSTRUCT.pt` é "per-monitor aware"; o `monitor_capture` grava na resolução nativa. Captura reduzida é suportada pela conversão (testada). |
| Duração na recuperação | `max(pulsação do manifesto, mtime(programa) − criado_em)`; manifesto reescrito a cada 5 s durante a gravação | Sem `ffprobe`/libav no app; erro ≤ ~5 s, documentado. Duração exata é trabalho do editor (etapa 3). |
| Validação do MP4 | `ffprobe` se existir (PATH ou `.deps`); senão, log (`Adding chapter`, encoders `simple_aac_recordingN`) | Nada novo instalado. |
| Pasta padrão | `%USERPROFILE%\Videos\Voltz Studio` só se `SimpleOutput/FilePath`/`AdvOut/RecFilePath` não têm valor do usuário | Não sobrescreve escolha; `OutputPathValid` exige a pasta existir ⇒ criada. |
| Atalho de marcador | Atalho de frontend `VoltzStudio.AddMarker`, padrão **Ctrl+Shift+M** quando não há nada salvo no perfil | Mesma família do Ctrl+Shift+D da spec §6.2; editável em Configurações → Atalhos do modo clássico. |
| Renomear | Só o `nome` do manifesto; a pasta mantém o nome | Pasta pode estar aberta no editor/Explorer; nome com data continua único. |
| Teste de disco | Variável de ambiente `VOLTZ_FAKE_FREE_BYTES` (só testes) substitui o espaço livre real | Único jeito de testar < 5 GB / < 1 GB sem encher o disco. |

---

## Convenções

**Diretório de trabalho:** a raiz do repositório (`voltz-studio`), branch `voltz`. Leia o `CLAUDE.md` antes
(seção **Armadilhas**). Base deste plano: commit `555ca2417`.

```bash
CMAKE="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
CTEST="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe"
CLANGFMT="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/bin/clang-format.exe"
```

- **CONFIGURAR:** `"$CMAKE" --preset voltz-windows-x64`
- **COMPILAR APP:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target obs-studio --parallel -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
- **COMPILAR TUDO:** `"$CMAKE" --build --preset voltz-windows-x64 --parallel -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
- **COMPILAR TESTES:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target voltz-core-tests -- -v:minimal -nologo -nodeReuse:false`
- **RODAR TESTES:** `"$CTEST" --test-dir build_x64/frontend/voltz/tests -C RelWithDebInfo --output-on-failure`
  (para ver a contagem de casos: `build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe` → linha
  `test cases: N | N passed`)
- **SMOKE:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1`
- **SMOKE GRAVAÇÃO:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-record.ps1`
- **SMOKE PROJETO** (novo, Task 8): `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-project.ps1 -OutDir "<scratchpad>\smoke-projeto"`
- **FORMATAR:** `"$CLANGFMT" -i <arquivos .cpp/.hpp novos ou alterados em frontend/voltz/>` (nunca nos arquivos do OBS)

> **Regras fixas:**
> - Builds longos **em segundo plano**, nunca dois ao mesmo tempo. `MSB4018`/`ZERO_CHECK`/"Touch" no `build_x86` →
>   rode de novo. Arquivo novo (código, `data/`, ícone no `.qrc`) exige **CONFIGURAR** antes de compilar.
> - **Nunca** encerre processos que você não iniciou: só pelo PID que você lançou (`Stop-Process -Id $proc.Id`);
>   nunca `taskkill /IM`, nunca `Stop-Process -Name`. Ler a lista de processos é permitido.
> - **Nunca** capture a tela inteira — só o retângulo de uma janela do app, via `PrintWindow` (bloco abaixo).
>   **Nunca** capture a tela Gravar com câmera ligada: os testes usam config portátil limpa **sem aplicar preset**
>   (sem câmera) e a "Tela teste" fica numa cena que nunca vai ao programa, com o item desativado.
> - **Nunca** mova o mouse nem digite na sessão do usuário (nada de `SendInput`/`SetCursorPos`/`SendKeys`). O
>   `CursorTracker` é testado por unidade (núcleo) e pelos arquivos (posição inicial). UI Automation `Invoke` não move
>   o mouse e é permitido.
> - Gravações de teste vão para a pasta `gravacoes` do **scratchpad** (`SetProfileParameter`) e são apagadas no fim;
>   o áudio global é **mutado** antes de gravar.
> - App sempre com `--portable --multi`, com **backup e restauração** da config portátil
>   (`build_x64/rundir/RelWithDebInfo/config`) antes de qualquer teste ou smoke — os smokes antigos **apagam** essa
>   pasta (o novo smoke de projeto guarda e restaura sozinho).
> - `HideOBSWindowsFromCapture=false` no `user.ini` portátil para qualquer captura (`PrintWindow` sai preto com o
>   padrão ligado).
> - O alvo compila com `QT_NO_CONTEXTLESS_CONNECT`: todo `connect` com lambda leva objeto de contexto.
> - QSS da casca: toda regra com prefixo `#voltzShell`; `setObjectName` **antes** do `setStyleSheet` do pai fazer
>   efeito (crie o widget com nome antes de inseri-lo). Ícones usados em QSS (`image:`/`qproperty-icon:`) vêm de
>   `icons/qss/` (`tools/voltz-qss-icons.sh`); no código, sempre `phosphorIcon()`. Ícones Phosphor novos só da
>   **2.1.1 oficial** (unpkg com versão exata).
> - Sinais do libobs (saída, fonte) e callbacks de atalho chegam fora da thread da UI → `Qt::QueuedConnection`
>   (`QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)`).
> - PowerShell 5.1 lê `.ps1` sem BOM como ANSI: script com acento (ex. `-Name "Recuperar gravações"`) precisa ser
>   salvo **com BOM**; `Select-String` em log com caminho acentuado precisa de `-Encoding UTF8`.
> - `clang-format -i` só nos arquivos de `frontend/voltz/`. Arquivos do OBS **não** são alterados neste plano.
> - Textos novos nos **dois** locales (`frontend/data/locale/voltz-pt-BR.ini` e `voltz-en-US.ini`, UTF-8 sem BOM),
>   com paridade: `diff <(cut -d= -f1 frontend/data/locale/voltz-en-US.ini | sort) <(cut -d= -f1 frontend/data/locale/voltz-pt-BR.ini | sort)` → sem saída.
> - `docs/voltz/backlog-plano-*.md` só é commitado na Task 9. Push só na Task 9.

**Commits:** terminar a mensagem com a linha `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

**Bloco de teste (PowerShell 5.1)** — copie para um `.ps1` **com BOM** no scratchpad e coloque os passos da task no
`try`. Ele guarda a config portátil, cria uma limpa com obs-websocket (porta 4455, sem senha), idioma pt-BR e sem
confirmação ao sair, cria `gravacoes` no scratchpad e restaura/apaga tudo no fim.
```powershell
$root = "C:\Users\Cassio\Documents\Projetos IA\voltz-studio"
$rundir = Join-Path $root "build_x64\rundir\RelWithDebInfo"; $bin = Join-Path $rundir "bin\64bit"
$sp = "<caminho absoluto do scratchpad>"
$out = Join-Path $sp "gravacoes"
$cfg = Join-Path $rundir "config"; $bak = Join-Path $rundir "config.3abak"
if (Test-Path $bak) { throw "backup anterior em $bak - restaure (Move-Item) antes de continuar" }
if (Test-Path $cfg) { Move-Item $cfg $bak }
$voltzCfg = Join-Path $cfg "Voltz Studio"
$wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
New-Item -ItemType Directory -Force $wsDir | Out-Null
[IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}')
$userIni = "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[BasicWindow]`r`nHideOBSWindowsFromCapture=false`r`n"   # ajuste por task
[IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), $userIni, (New-Object Text.UTF8Encoding $false))
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null
$fakeFree = $null   # ajuste por task (ex. "536870912" = 0,5 GB): simula espaço livre via VOLTZ_FAKE_FREE_BYTES
if ($fakeFree) { $env:VOLTZ_FAKE_FREE_BYTES = $fakeFree }
# A partir da Task 4 o app cria Vídeos\Voltz Studio (pasta padrão); se este teste a criou e ela ficou vazia, apaga.
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"; $hadVoltzVideos = Test-Path -LiteralPath $voltzVideos
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class W { [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f); }
'@
[void][W]::SetProcessDPIAware()
function Shot([IntPtr]$h, [string]$n) { $r = New-Object W+RECT; [void][W]::GetWindowRect($h, [ref]$r)
  $b = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [System.Drawing.Graphics]::FromImage($b)
  $d = $g.GetHdc(); [void][W]::PrintWindow($h, $d, 2); $g.ReleaseHdc($d); $g.Dispose(); $b.Save("$sp\$n.png"); $b.Dispose() }
$uia = Join-Path $root "tools\voltz-uia.ps1"; $ws = Join-Path $root "tools\voltz-ws.ps1"
# Pedidos ao obs-websocket: devolve as respostas (objetos com index/requestType/ok/data)
function Ws([string]$json) { $f = Join-Path $sp "req.json"
  [IO.File]::WriteAllText($f, $json, (New-Object Text.UTF8Encoding $false))
  $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $f
  $lines | ForEach-Object { Write-Host $_ }
  return @($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json }) }
function Uia([string]$name) { & powershell -ExecutionPolicy Bypass -File $uia -ProcessId $proc.Id -Name $name | Out-Host; return ($LASTEXITCODE -eq 0) }
function Close-App { if (-not $proc.HasExited) { $proc.Refresh(); [void]$proc.CloseMainWindow(); if (-not $proc.WaitForExit(30000)) { Stop-Process -Id $proc.Id -Force } } }
function Project-Dirs { @(Get-ChildItem -LiteralPath $out -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz") }) }
function Read-Json([string]$path) { (Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json) | ForEach-Object { $_ } }
$ffprobe = (Get-Command ffprobe -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $ffprobe) { $ffprobe = Get-ChildItem (Join-Path $root ".deps") -Filter ffprobe.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName }
$proc = Start-Process (Join-Path $bin "VoltzStudio.exe") -ArgumentList '--portable','--multi' -WorkingDirectory $bin -PassThru
Start-Sleep 15
try {
  $proc.Refresh(); $shell = $proc.MainWindowHandle   # HWND da casca
  # ... passos da task ...
} finally {
  Close-App
  $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
  if ($log) { Copy-Item $log.FullName "$sp\ultimo-log.txt" -Force }
  if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
  if (Test-Path $bak) { Move-Item $bak $cfg }
  Remove-Item Env:VOLTZ_FAKE_FREE_BYTES -ErrorAction SilentlyContinue
  if (Test-Path $out) { Remove-Item -Recurse -Force $out }
  if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) { Remove-Item -Force -LiteralPath $voltzVideos }
}
```

**Preparo de gravação** (primeiros passos dentro do `try` nas Tasks 4-7): aplica os padrões Voltz pelo "Pular" da
primeira execução (qualidade HQ + codificador automático + MP4 híbrido, `RecordingConfig::applyVoltzDefaults`), aponta
a pasta de gravações para `$out` e muta o áudio global.
```powershell
if (-not (Uia "Pular")) { throw "botao Pular nao encontrado" }
Start-Sleep 2
$r = Ws ('[{"requestType":"SetProfileParameter","requestData":{"parameterCategory":"SimpleOutput","parameterName":"FilePath","parameterValue":' + (ConvertTo-Json $out) + '}},{"requestType":"GetSpecialInputs"}]')
$special = $r[1].data
$mutes = @(); foreach ($n in @($special.desktop1, $special.desktop2, $special.mic1, $special.mic2, $special.mic3, $special.mic4)) { if ($n) { $mutes += @{ requestType = "SetInputMute"; requestData = @{ inputName = $n; inputMuted = $true } } } }
if ($mutes.Count -gt 0) { [void](Ws (ConvertTo-Json -InputObject $mutes -Depth 5 -Compress)) }
```

**Tela de teste** (Tasks 5 e 8): uma captura de monitor numa cena que nunca vai ao programa, com o item desativado —
só para o `ProjectRecorder` achar o monitor (nada é capturado nem gravado).
```powershell
[void](Ws '[{"requestType":"CreateScene","requestData":{"sceneName":"Fontes de teste"}},{"requestType":"CreateInput","requestData":{"sceneName":"Fontes de teste","inputName":"Tela teste","inputKind":"monitor_capture","sceneItemEnabled":false}}]')
$items = (Ws '[{"requestType":"GetInputPropertiesListPropertyItems","requestData":{"inputName":"Tela teste","propertyName":"monitor_id"}}]')[0].data.propertyItems
$monitorId = ($items | Where-Object { $_.itemEnabled } | Select-Object -First 1).itemValue
[void](Ws ('[{"requestType":"SetInputSettings","requestData":{"inputName":"Tela teste","inputSettings":{"monitor_id":' + (ConvertTo-Json $monitorId) + '}}}]'))
```

---

## Mapa de arquivos

**Novos — núcleo (`frontend/voltz/core/`, sem Qt/libobs) e testes**
| Arquivo | Responsabilidade |
|---|---|
| `core/atomic-file.hpp/.cpp` | escrita atômica (temporário + `MoveFileExW`), leitura de arquivo, caminhos UTF-8 |
| `core/project-naming.hpp/.cpp` | saneamento do nome, `AAAA-MM-DD HHhMM · Nome`, sufixo `(2)` em colisão |
| `core/project-timeline.hpp/.cpp` | `t0`, pausas, ms relativos, duração bruta/líquida |
| `core/project-manifest.hpp/.cpp` | `projeto.voltz` v1: modelo, JSON (nlohmann), leitura tolerante, preservação, recuperação |
| `core/project-markers.hpp/.cpp` | `marcadores.json` |
| `core/cursor-track.hpp/.cpp` | desktop → pixels da captura, amostragem/coalescência 60 Hz, linha do `cursor.jsonl` |
| `core/disk-space.hpp/.cpp` | níveis de disco (5 GB/1 GB), estimativa GB/min, textos de espaço |
| `tests/test-atomic-file.cpp`, `test-project-naming.cpp`, `test-project-timeline.cpp`, `test-project-manifest.cpp`, `test-project-markers.cpp`, `test-cursor-track.cpp`, `test-disk-space.cpp` | doctest |

**Novos — projeto (`frontend/voltz/project/`, alvo `obs-studio`)**
| Arquivo | Responsabilidade |
|---|---|
| `project/ProjectRecorder.hpp/.cpp` | ciclo do projeto pelos eventos do frontend: pasta, troca temporária do perfil, manifesto, pausas, faixas, marcadores + atalho, pulsação, disco, saída do app |
| `project/MonitorLocator.hpp/.cpp` | `monitor_id` do `monitor_capture` → retângulo físico + escala (mesma busca do win-capture) |
| `project/CursorTracker.hpp/.cpp` | gancho `WH_MOUSE_LL` (thread própria) + thread de escrita do `cursor.jsonl` |
| `project/ProjectLibrary.hpp/.cpp` | varredura da pasta de gravações, nomes recentes, renomear, recuperar |
| `tools/voltz-smoke-project.ps1` | teste de integração da gravação em projeto |
| `frontend/voltz/shell/icons/{regular,fill}/{flag,hard-drives}*.svg` | ícones Phosphor 2.1.1 |

**Modificados (Voltz):** `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`,
`frontend/cmake/feature-voltz.cmake`, `frontend/voltz/shell/RecordingConfig.hpp/.cpp`, `VoltzShell.hpp/.cpp`,
`RecordPage.hpp/.cpp`, `ProjectsPage.hpp/.cpp`, `ErrorBanner.hpp/.cpp`, `Icons.hpp`, `voltz-shell.qss`,
`voltz-shell.qrc`, `frontend/data/locale/voltz-*.ini`, `tools/voltz-smoke.ps1`, `tools/voltz-smoke-record.ps1`.

**Modificados (OBS upstream):** nenhum. (`docs/voltz/upstream-patches.md` não muda; a Task 9 confere.)

---

### Task 1: Núcleo — escrita atômica e nome da pasta do projeto

**Files:**
- Create: `frontend/voltz/core/atomic-file.hpp`, `frontend/voltz/core/atomic-file.cpp`
- Create: `frontend/voltz/core/project-naming.hpp`, `frontend/voltz/core/project-naming.cpp`
- Create: `frontend/voltz/tests/test-atomic-file.cpp`, `frontend/voltz/tests/test-project-naming.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

- [ ] **Step 1: Escrever os testes (falham: cabeçalhos não existem)**

`frontend/voltz/tests/test-atomic-file.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/atomic-file.hpp>

#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

// Pasta limpa em %TEMP%\voltz-core-tests\<nome> (nome em UTF-8).
fs::path freshDir(const char *name)
{
	const fs::path dir = fs::temp_directory_path() / "voltz-core-tests" / fs::u8path(name);
	std::error_code ec;
	fs::remove_all(dir, ec);
	fs::create_directories(dir);
	return dir;
}

std::string utf8(const fs::path &path)
{
	return path.u8string();
}

} // namespace

TEST_CASE("writeFileAtomically cria o arquivo e não deixa o temporário")
{
	const fs::path dir = freshDir("atomic-new");
	const fs::path file = dir / "projeto.voltz";
	std::string error;
	REQUIRE(voltz::writeFileAtomically(utf8(file), "{\"a\":1}\n", &error));
	CHECK(error.empty());
	std::string back;
	REQUIRE(voltz::readFileText(utf8(file), back));
	CHECK(back == "{\"a\":1}\n");
	CHECK_FALSE(fs::exists(dir / "projeto.voltz.tmp"));
}

TEST_CASE("writeFileAtomically substitui o conteúdo anterior por inteiro")
{
	const fs::path file = freshDir("atomic-replace") / "m.json";
	REQUIRE(voltz::writeFileAtomically(utf8(file), std::string(4096, 'x')));
	REQUIRE(voltz::writeFileAtomically(utf8(file), "curto"));
	std::string back;
	REQUIRE(voltz::readFileText(utf8(file), back));
	CHECK(back == "curto");
}

TEST_CASE("writeFileAtomically aceita caminho com acentos e ponto médio")
{
	const fs::path file = freshDir("2026-10-15 15h30 · Ação") / "marcadores.json";
	REQUIRE(voltz::writeFileAtomically(utf8(file), "[]\n"));
	std::string back;
	REQUIRE(voltz::readFileText(utf8(file), back));
	CHECK(back == "[]\n");
}

TEST_CASE("writeFileAtomically falha sem criar nada quando a pasta não existe")
{
	const fs::path file = freshDir("atomic-missing") / "nao-existe" / "m.json";
	std::string error;
	CHECK_FALSE(voltz::writeFileAtomically(utf8(file), "x", &error));
	CHECK_FALSE(error.empty());
	CHECK_FALSE(fs::exists(file));
}

TEST_CASE("readFileText devolve falso para arquivo inexistente")
{
	std::string out = "intacto";
	CHECK_FALSE(voltz::readFileText(utf8(freshDir("atomic-read") / "nada.txt"), out));
	CHECK(out == "intacto");
}
```
(`fs::u8path` e `path::u8string()` devolvendo `std::string` valem em C++17, que é o padrão do projeto —
`cmake/common/compiler_common.cmake:11`; não há aviso de depreciação nesse padrão.)

`frontend/voltz/tests/test-project-naming.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/project-naming.hpp>

#include <set>
#include <string>

TEST_CASE("sanitizeProjectName troca caracteres proibidos no Windows por _")
{
	CHECK(voltz::sanitizeProjectName("Aula 03", "Gravação") == "Aula 03");
	CHECK(voltz::sanitizeProjectName("a<b>c:d\"e/f\\g|h?i*j", "x") == "a_b_c_d_e_f_g_h_i_j");
	CHECK(voltz::sanitizeProjectName("tab\there", "x") == "tab_here");
	CHECK(voltz::sanitizeProjectName("Revisão · ç", "x") == "Revisão · ç");
}

TEST_CASE("sanitizeProjectName tira espaços e pontos das pontas e usa o padrão se sobrar nada")
{
	CHECK(voltz::sanitizeProjectName("  Aula final.  ", "x") == "Aula final");
	CHECK(voltz::sanitizeProjectName(" ... ", "Gravação") == "Gravação");
	CHECK(voltz::sanitizeProjectName("", "Gravação") == "Gravação");
}

TEST_CASE("sanitizeProjectName corta no limite de bytes sem quebrar UTF-8")
{
	std::string longName;
	for (int i = 0; i < 100; i++) {
		longName += "ç"; // 2 bytes
	}
	const std::string cut = voltz::sanitizeProjectName(longName, "x", 81);
	CHECK(cut.size() == 80);
	CHECK(cut.substr(78) == "ç");
}

TEST_CASE("projectFolderName segue 'AAAA-MM-DD HHhMM · Nome'")
{
	CHECK(voltz::projectFolderName({2026, 10, 5, 9, 7}, "Aula 03") == "2026-10-05 09h07 · Aula 03");
	CHECK(voltz::projectFolderName({2026, 12, 31, 23, 59}, "X") == "2026-12-31 23h59 · X");
}

TEST_CASE("uniqueFolderName acrescenta (2), (3)… só quando o nome já existe")
{
	const std::set<std::string> taken = {"x", "x (2)"};
	auto exists = [&taken](const std::string &name) { return taken.count(name) > 0; };
	CHECK(voltz::uniqueFolderName("y", exists) == "y");
	CHECK(voltz::uniqueFolderName("x", exists) == "x (3)");
}
```

No `frontend/voltz/tests/CMakeLists.txt`, acrescentar à lista de `target_sources` (ordem alfabética):
`test-atomic-file.cpp` (logo depois de `test-main.cpp`) e `test-project-naming.cpp` (depois de `test-preview-fit.cpp`).

- [ ] **Step 2: Rodar e ver falhar**

Run: CONFIGURAR; COMPILAR TESTES.
Expected: erro de compilação `Cannot open include file: 'voltz/core/atomic-file.hpp'` (e `project-naming.hpp`).

- [ ] **Step 3: Implementar `atomic-file`**

`frontend/voltz/core/atomic-file.hpp`:
```cpp
#pragma once

#include <string>

namespace voltz {

// Escreve `content` em `utf8Path` de forma atômica: grava "<arquivo>.tmp" ao lado, força para o disco e troca pelo
// destino (MoveFileExW com REPLACE_EXISTING|WRITE_THROUGH no Windows). Em falha, o destino anterior fica intacto,
// o temporário é apagado e `error` (se dado) recebe a causa. Não cria pastas.
bool writeFileAtomically(const std::string &utf8Path, const std::string &content, std::string *error = nullptr);

// Lê o arquivo inteiro (binário). Falso se não abrir; `out` só muda em caso de sucesso.
bool readFileText(const std::string &utf8Path, std::string &out);

} // namespace voltz
```

`frontend/voltz/core/atomic-file.cpp`:
```cpp
#include "atomic-file.hpp"

#include <cstdio>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#endif

namespace voltz {

namespace {

#ifdef _WIN32
std::wstring widen(const std::string &utf8)
{
	if (utf8.empty()) {
		return {};
	}
	const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
	std::wstring wide(static_cast<size_t>(size > 0 ? size : 0), L'\0');
	if (size > 0) {
		MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), size);
	}
	return wide;
}
#endif

FILE *openFile(const std::string &utf8Path, bool write)
{
#ifdef _WIN32
	return _wfopen(widen(utf8Path).c_str(), write ? L"wb" : L"rb");
#else
	return std::fopen(utf8Path.c_str(), write ? "wb" : "rb");
#endif
}

void removeFile(const std::string &utf8Path)
{
#ifdef _WIN32
	_wremove(widen(utf8Path).c_str());
#else
	std::remove(utf8Path.c_str());
#endif
}

void setError(std::string *error, const std::string &message)
{
	if (error) {
		*error = message;
	}
}

} // namespace

bool writeFileAtomically(const std::string &utf8Path, const std::string &content, std::string *error)
{
	const std::string tmp = utf8Path + ".tmp";
	FILE *file = openFile(tmp, true);
	if (!file) {
		setError(error, "nao foi possivel criar " + tmp);
		return false;
	}
	const size_t written = content.empty() ? 0 : std::fwrite(content.data(), 1, content.size(), file);
	bool ok = written == content.size() && std::fflush(file) == 0;
#ifdef _WIN32
	ok = ok && _commit(_fileno(file)) == 0;
#endif
	ok = (std::fclose(file) == 0) && ok;
	if (!ok) {
		removeFile(tmp);
		setError(error, "falha ao gravar " + tmp);
		return false;
	}
#ifdef _WIN32
	if (!MoveFileExW(widen(tmp).c_str(), widen(utf8Path).c_str(),
			 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		const unsigned long code = GetLastError();
		removeFile(tmp);
		setError(error, "falha ao substituir " + utf8Path + " (erro " + std::to_string(code) + ")");
		return false;
	}
#else
	if (std::rename(tmp.c_str(), utf8Path.c_str()) != 0) {
		removeFile(tmp);
		setError(error, "falha ao substituir " + utf8Path);
		return false;
	}
#endif
	return true;
}

bool readFileText(const std::string &utf8Path, std::string &out)
{
	FILE *file = openFile(utf8Path, false);
	if (!file) {
		return false;
	}
	std::string content;
	char buffer[8192];
	size_t count = 0;
	while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
		content.append(buffer, count);
	}
	const bool ok = std::ferror(file) == 0;
	std::fclose(file);
	if (ok) {
		out = std::move(content);
	}
	return ok;
}

} // namespace voltz
```
(`_CRT_SECURE_NO_WARNINGS`, `UNICODE` e `NOMINMAX` já vêm de `cmake/windows/compilerconfig.cmake` para todos os
alvos, inclusive os testes.)

- [ ] **Step 4: Implementar `project-naming`**

`frontend/voltz/core/project-naming.hpp`:
```cpp
#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace voltz {

struct LocalDateTime {
	int year = 0;
	int month = 0;
	int day = 0;
	int hour = 0;
	int minute = 0;
};

// Nome seguro para pasta no Windows: troca <>:"/\|?* e caracteres de controle por "_", tira espaços e pontos das
// pontas e corta em `maxBytes` bytes sem quebrar UTF-8. Vazio vira `fallback`. Nomes reservados (CON, NUL…) não são
// tratados porque a pasta sempre começa pela data.
std::string sanitizeProjectName(const std::string &name, const std::string &fallback, size_t maxBytes = 80);

// "2026-10-15 15h30 · Aula 03" (spec §5.1). `safeName` já saneado.
std::string projectFolderName(const LocalDateTime &when, const std::string &safeName);

// `base` se `exists(base)` for falso; senão "base (2)", "base (3)"… (até 999).
std::string uniqueFolderName(const std::string &base, const std::function<bool(const std::string &)> &exists);

} // namespace voltz
```

`frontend/voltz/core/project-naming.cpp`:
```cpp
#include "project-naming.hpp"

#include <cstdio>

namespace voltz {

namespace {

bool isForbidden(unsigned char c)
{
	if (c < 0x20 || c == 0x7F) {
		return true;
	}
	switch (c) {
	case '<':
	case '>':
	case ':':
	case '"':
	case '/':
	case '\\':
	case '|':
	case '?':
	case '*':
		return true;
	default:
		return false;
	}
}

std::string trimEnds(const std::string &text)
{
	const size_t first = text.find_first_not_of(" .");
	if (first == std::string::npos) {
		return {};
	}
	const size_t last = text.find_last_not_of(" .");
	return text.substr(first, last - first + 1);
}

} // namespace

std::string sanitizeProjectName(const std::string &name, const std::string &fallback, size_t maxBytes)
{
	std::string clean;
	clean.reserve(name.size());
	for (const char ch : name) {
		clean += isForbidden(static_cast<unsigned char>(ch)) ? '_' : ch;
	}
	clean = trimEnds(clean);
	if (clean.size() > maxBytes) {
		size_t cut = maxBytes;
		/* não corta no meio de um caractere UTF-8: recua enquanto o byte do corte for de continuação */
		while (cut > 0 && (static_cast<unsigned char>(clean[cut]) & 0xC0) == 0x80) {
			cut--;
		}
		clean = trimEnds(clean.substr(0, cut));
	}
	return clean.empty() ? fallback : clean;
}

std::string projectFolderName(const LocalDateTime &when, const std::string &safeName)
{
	char stamp[32];
	std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %02dh%02d", when.year, when.month, when.day, when.hour,
		      when.minute);
	return std::string(stamp) + " \xC2\xB7 " + safeName;
}

std::string uniqueFolderName(const std::string &base, const std::function<bool(const std::string &)> &exists)
{
	if (!exists(base)) {
		return base;
	}
	for (int n = 2; n < 1000; n++) {
		std::string candidate = base + " (" + std::to_string(n) + ")";
		if (!exists(candidate)) {
			return candidate;
		}
	}
	return base + " (1000)";
}

} // namespace voltz
```

No `frontend/voltz/core/CMakeLists.txt`, acrescentar em ordem alfabética: na lista `PRIVATE` `atomic-file.cpp`
(primeiro da lista) e `project-naming.cpp` (antes de `recording-clock.cpp`); na lista `PUBLIC` os `.hpp`
correspondentes nas mesmas posições.

- [ ] **Step 5: CONFIGURAR, COMPILAR TESTES, RODAR TESTES**

Expected: tudo passa; `voltz-core-tests.exe` → `test cases: 58 | 58 passed` (48 + 10).

- [ ] **Step 6: Formatar, testar de novo e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/atomic-file.* frontend/voltz/core/project-naming.* frontend/voltz/tests/test-atomic-file.cpp frontend/voltz/tests/test-project-naming.cpp
```
COMPILAR TESTES + RODAR TESTES de novo (58 passam), então:
```bash
git add frontend/voltz/core/atomic-file.* frontend/voltz/core/project-naming.* frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/test-atomic-file.cpp frontend/voltz/tests/test-project-naming.cpp frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): nucleo do projeto - escrita atomica e nome da pasta

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Núcleo — linha do tempo, manifesto v1 e marcadores

**Files:**
- Create: `frontend/voltz/core/project-timeline.hpp/.cpp`, `frontend/voltz/core/project-manifest.hpp/.cpp`,
  `frontend/voltz/core/project-markers.hpp/.cpp`
- Create: `frontend/voltz/tests/test-project-timeline.cpp`, `test-project-manifest.cpp`, `test-project-markers.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt` (nlohmann_json), `frontend/voltz/tests/CMakeLists.txt`

Formato (spec §5.2) com as adições do 3A, todas toleradas pelo leitor: `duracao_liquida_ms`,
`arquivos.programa.faixas_audio`, `arquivos.cursor` (`arquivo`, `largura`, `altura`, `origem` com `tipo`, `id`,
`retangulo`, `escala` — no 3B a mesma `origem` vai também para `arquivos.tela`) e `eventos[].codigo`. Pausa ainda
aberta (manifesto escrito durante a pausa) sai com `"fim_ms": null`. Tempos em ms relativos a `t0_ns`, **sem descontar
pausas** (§5.5). `duracao_ms` = bruta (t0 → fim); `duracao_liquida_ms` = bruta − pausas (o que o `programa.mp4` tem).

- [ ] **Step 1: Escrever os testes (falham)**

`frontend/voltz/tests/test-project-timeline.cpp`:
```cpp
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
```

`frontend/voltz/tests/test-project-manifest.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/project-manifest.hpp>

#include <string>

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
```

`frontend/voltz/tests/test-project-markers.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/project-markers.hpp>

#include <string>
#include <vector>

TEST_CASE("serializeMarkers sem marcadores grava lista vazia")
{
	CHECK(voltz::serializeMarkers({}) == "[]\n");
}

TEST_CASE("marcadores: ida e volta mantém ordem, tempo e nome com acento")
{
	const std::vector<voltz::Marker> markers = {{12345, "Marcador 1"}, {67890, "Introdução"}};
	const std::string json = voltz::serializeMarkers(markers);
	CHECK(json.find("\"t_ms\": 12345") != std::string::npos);
	CHECK(json.find("\"nome\": \"Marcador 1\"") != std::string::npos);
	std::vector<voltz::Marker> back;
	REQUIRE(voltz::parseMarkers(json, back));
	REQUIRE(back.size() == 2);
	CHECK(back[0].tMs == 12345);
	CHECK(back[1].tMs == 67890);
	CHECK(back[1].name == "Introdução");
}

TEST_CASE("parseMarkers ignora itens inválidos e recusa o que não é lista")
{
	std::vector<voltz::Marker> out;
	REQUIRE(voltz::parseMarkers(R"([{"t_ms":5,"nome":"A"},{"x":1},7,{"t_ms":9}])", out));
	REQUIRE(out.size() == 1);
	CHECK(out[0].name == "A");
	CHECK_FALSE(voltz::parseMarkers("{}", out));
	CHECK_FALSE(voltz::parseMarkers("nada", out));
}
```

No `frontend/voltz/tests/CMakeLists.txt`: acrescentar `test-project-manifest.cpp`, `test-project-markers.cpp` e
`test-project-timeline.cpp` em ordem alfabética (em volta de `test-project-naming.cpp`); e, antes de
`add_executable(voltz-core-tests)`:
```cmake
# O voltz-core usa nlohmann_json (PRIVATE, só nos .cpp); o alvo importado precisa existir aqui para o link do teste.
find_package(nlohmann_json 3.11 REQUIRED)
```

- [ ] **Step 2: Rodar e ver falhar**

Run: CONFIGURAR; COMPILAR TESTES. Expected: `Cannot open include file: 'voltz/core/project-timeline.hpp'` (e os outros).

- [ ] **Step 3: Implementar `project-timeline`**

`frontend/voltz/core/project-timeline.hpp`:
```cpp
#pragma once

#include <cstdint>
#include <vector>

namespace voltz {

// Pausa em ms relativos ao t0. endMs < 0: pausa ainda aberta.
struct PauseSpan {
	int64_t startMs = 0;
	int64_t endMs = -1;
};

// Duração líquida: `grossMs` menos o trecho de cada pausa dentro de [0, grossMs]; pausa aberta vai até o fim.
int64_t netDuration(int64_t grossMs, const std::vector<PauseSpan> &pauses);

// Linha do tempo do projeto no relógio do libobs (os_gettime_ns). Tempos em ms relativos ao t0, sem descontar
// pausas (spec §5.5); as pausas ficam registradas para o editor. Não lê relógio: quem chama passa o "agora".
class ProjectTimeline {
public:
	void start(int64_t t0Ns);
	void pause(int64_t nowNs);
	void resume(int64_t nowNs);
	// Fecha uma pausa aberta e congela as durações.
	void stop(int64_t nowNs);

	bool started() const { return started_; }
	bool paused() const { return paused_; }
	bool stopped() const { return stopped_; }
	int64_t t0Ns() const { return t0Ns_; }

	int64_t relativeMs(int64_t nowNs) const;
	int64_t grossMs(int64_t nowNs) const;
	int64_t netMs(int64_t nowNs) const;
	const std::vector<PauseSpan> &pauses() const { return pauses_; }

private:
	bool started_ = false;
	bool paused_ = false;
	bool stopped_ = false;
	int64_t t0Ns_ = 0;
	int64_t stopNs_ = 0;
	std::vector<PauseSpan> pauses_;
};

} // namespace voltz
```

`frontend/voltz/core/project-timeline.cpp`:
```cpp
#include "project-timeline.hpp"

#include <algorithm>

namespace voltz {

int64_t netDuration(int64_t grossMs, const std::vector<PauseSpan> &pauses)
{
	if (grossMs <= 0) {
		return 0;
	}
	int64_t paused = 0;
	for (const PauseSpan &pause : pauses) {
		const int64_t start = std::clamp<int64_t>(pause.startMs, 0, grossMs);
		const int64_t end = std::clamp<int64_t>(pause.endMs < 0 ? grossMs : pause.endMs, 0, grossMs);
		if (end > start) {
			paused += end - start;
		}
	}
	return std::max<int64_t>(0, grossMs - paused);
}

void ProjectTimeline::start(int64_t t0Ns)
{
	started_ = true;
	paused_ = false;
	stopped_ = false;
	t0Ns_ = t0Ns;
	stopNs_ = 0;
	pauses_.clear();
}

void ProjectTimeline::pause(int64_t nowNs)
{
	if (!started_ || stopped_ || paused_) {
		return;
	}
	paused_ = true;
	pauses_.push_back(PauseSpan{relativeMs(nowNs), -1});
}

void ProjectTimeline::resume(int64_t nowNs)
{
	if (!started_ || stopped_ || !paused_) {
		return;
	}
	paused_ = false;
	pauses_.back().endMs = std::max(pauses_.back().startMs, relativeMs(nowNs));
}

void ProjectTimeline::stop(int64_t nowNs)
{
	if (!started_ || stopped_) {
		return;
	}
	if (paused_) {
		resume(nowNs);
	}
	stopped_ = true;
	stopNs_ = nowNs;
}

int64_t ProjectTimeline::relativeMs(int64_t nowNs) const
{
	if (!started_) {
		return 0;
	}
	const int64_t delta = nowNs - t0Ns_;
	return delta > 0 ? delta / 1000000 : 0;
}

int64_t ProjectTimeline::grossMs(int64_t nowNs) const
{
	return relativeMs(stopped_ ? stopNs_ : nowNs);
}

int64_t ProjectTimeline::netMs(int64_t nowNs) const
{
	return netDuration(grossMs(nowNs), pauses_);
}

} // namespace voltz
```

- [ ] **Step 4: Implementar `project-manifest`**

`frontend/voltz/core/project-manifest.hpp`:
```cpp
#pragma once

#include "project-timeline.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

constexpr int kManifestVersion = 1;
constexpr const char *kManifestFileName = "projeto.voltz";
constexpr const char *kProgramBaseName = "programa";
constexpr const char *kMarkersFileName = "marcadores.json";
constexpr const char *kCursorFileName = "cursor.jsonl";

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

// eventos[]: { "t_ms", "tipo", "faixa"?, "codigo"? }
struct ManifestEvent {
	int64_t tMs = 0;
	std::string type;
	std::string track;
	int code = 0;
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
	CursorFile cursor;
	std::vector<ManifestEvent> events;
	// JSON lido do disco. Ao reescrever, os campos que esta versão não conhece (tela, camera, mouse… do 3B/Plano 4)
	// voltam intactos.
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

} // namespace voltz
```

`frontend/voltz/core/project-manifest.cpp`:
```cpp
#include "project-manifest.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace voltz {

namespace {

using Json = nlohmann::ordered_json;

int64_t getInt(const Json &object, const char *key, int64_t fallback)
{
	const auto it = object.find(key);
	if (it == object.end()) {
		return fallback;
	}
	if (it->is_number_integer()) {
		return it->get<int64_t>();
	}
	if (it->is_number_float()) {
		return static_cast<int64_t>(std::llround(it->get<double>()));
	}
	return fallback;
}

double getDouble(const Json &object, const char *key, double fallback)
{
	const auto it = object.find(key);
	return it != object.end() && it->is_number() ? it->get<double>() : fallback;
}

std::string getString(const Json &object, const char *key)
{
	const auto it = object.find(key);
	return it != object.end() && it->is_string() ? it->get<std::string>() : std::string();
}

// Filho objeto (wantArray = false) ou lista (true); nulo se ausente ou de outro tipo.
const Json *getChild(const Json &object, const char *key, bool wantArray)
{
	const auto it = object.find(key);
	if (it == object.end()) {
		return nullptr;
	}
	if (wantArray ? it->is_array() : it->is_object()) {
		return &*it;
	}
	return nullptr;
}

Json fpsValue(double fps)
{
	const double rounded = std::round(fps * 1000.0) / 1000.0;
	if (std::floor(rounded) == rounded) {
		return Json(static_cast<int64_t>(rounded));
	}
	return Json(rounded);
}

} // namespace

const char *statusToString(ProjectStatus status)
{
	switch (status) {
	case ProjectStatus::Recording:
		return "gravando";
	case ProjectStatus::Ok:
		return "ok";
	case ProjectStatus::Recovered:
		return "recuperado";
	case ProjectStatus::Failed:
		break;
	}
	return "falhou";
}

ProjectStatus statusFromString(const std::string &text)
{
	if (text == "gravando") {
		return ProjectStatus::Recording;
	}
	if (text == "ok") {
		return ProjectStatus::Ok;
	}
	if (text == "recuperado") {
		return ProjectStatus::Recovered;
	}
	return ProjectStatus::Failed;
}

int64_t manifestNetDurationMs(const ProjectManifest &manifest)
{
	return netDuration(manifest.durationMs, manifest.pauses);
}

std::string serializeManifest(const ProjectManifest &m)
{
	Json root = Json::object();
	if (!m.preservedJson.empty()) {
		Json base = Json::parse(m.preservedJson, nullptr, false);
		if (base.is_object()) {
			root = std::move(base);
		}
	}
	root["formato"] = "voltz-projeto";
	root["versao"] = m.version;
	root["app"] = m.app;
	root["motor"] = m.engine;
	root["nome"] = m.name;
	root["criado_em"] = m.createdAt;
	root["status"] = statusToString(m.status);
	root["relogio"] = "obs_ns";
	root["t0_ns"] = m.t0Ns;
	root["duracao_ms"] = m.durationMs;
	root["duracao_liquida_ms"] = manifestNetDurationMs(m);

	Json pauses = Json::array();
	for (const PauseSpan &pause : m.pauses) {
		Json item = Json::object();
		item["inicio_ms"] = pause.startMs;
		item["fim_ms"] = pause.endMs < 0 ? Json(nullptr) : Json(pause.endMs);
		pauses.push_back(std::move(item));
	}
	root["pausas"] = std::move(pauses);

	if (!root.contains("arquivos") || !root["arquivos"].is_object()) {
		root["arquivos"] = Json::object();
	}
	Json &files = root["arquivos"];
	if (m.program.present) {
		Json program = Json::object();
		program["arquivo"] = m.program.file;
		program["largura"] = m.program.width;
		program["altura"] = m.program.height;
		program["fps"] = fpsValue(m.program.fps);
		program["offset_ms"] = m.program.offsetMs;
		program["faixas_audio"] = m.program.audioTracks;
		files["programa"] = std::move(program);
	}
	if (m.cursor.present) {
		Json origin = Json::object();
		origin["tipo"] = m.cursor.origin.type;
		origin["id"] = m.cursor.origin.id;
		origin["retangulo"] = Json::array(
			{m.cursor.origin.rect[0], m.cursor.origin.rect[1], m.cursor.origin.rect[2], m.cursor.origin.rect[3]});
		origin["escala"] = m.cursor.origin.scale;
		Json cursor = Json::object();
		cursor["arquivo"] = m.cursor.file;
		cursor["largura"] = m.cursor.width;
		cursor["altura"] = m.cursor.height;
		cursor["origem"] = std::move(origin);
		files["cursor"] = std::move(cursor);
	}

	Json events = Json::array();
	for (const ManifestEvent &event : m.events) {
		Json item = Json::object();
		item["t_ms"] = event.tMs;
		item["tipo"] = event.type;
		if (!event.track.empty()) {
			item["faixa"] = event.track;
		}
		if (event.code != 0) {
			item["codigo"] = event.code;
		}
		events.push_back(std::move(item));
	}
	root["eventos"] = std::move(events);

	return root.dump(2, ' ', false, Json::error_handler_t::replace) + "\n";
}

ManifestParse parseManifest(const std::string &text)
{
	ManifestParse result;
	const Json root = Json::parse(text, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		result.error = "JSON invalido";
		return result;
	}
	if (getString(root, "formato") != "voltz-projeto") {
		result.error = "formato desconhecido";
		return result;
	}

	ProjectManifest &m = result.manifest;
	m.version = static_cast<int>(getInt(root, "versao", kManifestVersion));
	result.newerVersion = m.version > kManifestVersion;
	m.app = getString(root, "app");
	m.engine = getString(root, "motor");
	m.name = getString(root, "nome");
	m.createdAt = getString(root, "criado_em");
	m.status = statusFromString(getString(root, "status"));
	m.t0Ns = getInt(root, "t0_ns", 0);
	m.durationMs = getInt(root, "duracao_ms", 0);

	if (const Json *pauses = getChild(root, "pausas", true)) {
		for (const Json &item : *pauses) {
			if (!item.is_object()) {
				continue;
			}
			m.pauses.push_back(PauseSpan{getInt(item, "inicio_ms", 0), getInt(item, "fim_ms", -1)});
		}
	}

	if (const Json *files = getChild(root, "arquivos", false)) {
		if (const Json *program = getChild(*files, "programa", false)) {
			m.program.present = true;
			m.program.file = getString(*program, "arquivo");
			m.program.width = static_cast<int>(getInt(*program, "largura", 0));
			m.program.height = static_cast<int>(getInt(*program, "altura", 0));
			m.program.fps = getDouble(*program, "fps", 0.0);
			m.program.offsetMs = getInt(*program, "offset_ms", 0);
			m.program.audioTracks = static_cast<int>(getInt(*program, "faixas_audio", 0));
		}
		if (const Json *cursor = getChild(*files, "cursor", false)) {
			m.cursor.present = true;
			m.cursor.file = getString(*cursor, "arquivo");
			m.cursor.width = static_cast<int>(getInt(*cursor, "largura", 0));
			m.cursor.height = static_cast<int>(getInt(*cursor, "altura", 0));
			if (const Json *origin = getChild(*cursor, "origem", false)) {
				m.cursor.origin.type = getString(*origin, "tipo");
				m.cursor.origin.id = getString(*origin, "id");
				m.cursor.origin.scale = getDouble(*origin, "escala", 1.0);
				if (const Json *rect = getChild(*origin, "retangulo", true)) {
					for (size_t i = 0; i < 4 && i < rect->size(); i++) {
						if ((*rect)[i].is_number()) {
							m.cursor.origin.rect[i] = static_cast<int>((*rect)[i].get<int64_t>());
						}
					}
				}
			}
		}
	}

	if (const Json *events = getChild(root, "eventos", true)) {
		for (const Json &item : *events) {
			if (!item.is_object()) {
				continue;
			}
			m.events.push_back(ManifestEvent{getInt(item, "t_ms", 0), getString(item, "tipo"),
							 getString(item, "faixa"),
							 static_cast<int>(getInt(item, "codigo", 0))});
		}
	}

	m.preservedJson = text;
	result.ok = true;
	return result;
}

void recoverManifest(ProjectManifest &manifest, int64_t fileElapsedMs)
{
	const int64_t gross = std::max<int64_t>(manifest.durationMs, std::max<int64_t>(0, fileElapsedMs));
	manifest.durationMs = gross;
	for (PauseSpan &pause : manifest.pauses) {
		if (pause.endMs < 0) {
			pause.endMs = std::max(pause.startMs, gross);
		}
	}
	manifest.status = ProjectStatus::Recovered;
	manifest.events.push_back(ManifestEvent{gross, "recuperado", "", 0});
}

} // namespace voltz
```
Notas para quem implementa: `getInt(item, "fim_ms", -1)` devolve `-1` para `null` (não é número) — é assim que a
pausa aberta volta. Se o compilador acusar ambiguidade em `object.find(key)` com `const char *` no `ordered_json`,
troque por `object.find(std::string(key))`.

- [ ] **Step 5: Implementar `project-markers`**

`frontend/voltz/core/project-markers.hpp`:
```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Um item do marcadores.json (spec §5.1): { "t_ms": 12345, "nome": "Marcador 1" } — t_ms relativo ao t0.
struct Marker {
	int64_t tMs = 0;
	std::string name;
};

// Lista JSON indentada com "\n" no fim ("[]\n" sem marcadores).
std::string serializeMarkers(const std::vector<Marker> &markers);

// Limpa `out` e lê os itens válidos (t_ms numérico e nome texto). Falso se não for uma lista JSON.
bool parseMarkers(const std::string &json, std::vector<Marker> &out);

} // namespace voltz
```

`frontend/voltz/core/project-markers.cpp`:
```cpp
#include "project-markers.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace voltz {

namespace {
using Json = nlohmann::ordered_json;
} // namespace

std::string serializeMarkers(const std::vector<Marker> &markers)
{
	Json list = Json::array();
	for (const Marker &marker : markers) {
		Json item = Json::object();
		item["t_ms"] = marker.tMs;
		item["nome"] = marker.name;
		list.push_back(std::move(item));
	}
	return list.dump(2, ' ', false, Json::error_handler_t::replace) + "\n";
}

bool parseMarkers(const std::string &text, std::vector<Marker> &out)
{
	out.clear();
	const Json list = Json::parse(text, nullptr, false);
	if (list.is_discarded() || !list.is_array()) {
		return false;
	}
	for (const Json &item : list) {
		if (!item.is_object()) {
			continue;
		}
		const auto time = item.find("t_ms");
		const auto name = item.find("nome");
		if (time == item.end() || !time->is_number() || name == item.end() || !name->is_string()) {
			continue;
		}
		out.push_back(Marker{time->get<int64_t>(), name->get<std::string>()});
	}
	return true;
}

} // namespace voltz
```

- [ ] **Step 6: CMake do núcleo**

Em `frontend/voltz/core/CMakeLists.txt`, logo depois de `cmake_minimum_required(...)`:
```cmake
# JSON do manifesto do projeto (mesma biblioteca que o frontend do OBS já usa; só nos .cpp do núcleo)
find_package(nlohmann_json 3.11 REQUIRED)
```
Acrescentar `project-manifest.cpp`, `project-markers.cpp`, `project-timeline.cpp` (em volta de
`project-naming.cpp`, ordem alfabética) na lista `PRIVATE` e os `.hpp` na `PUBLIC`; depois de
`target_compile_features(...)`:
```cmake
target_link_libraries(voltz-core PRIVATE nlohmann_json::nlohmann_json)
```

- [ ] **Step 7: CONFIGURAR, COMPILAR TESTES, RODAR TESTES**

Expected: tudo passa; `test cases: 75 | 75 passed` (58 + 6 + 8 + 3).
Se o CMake acusar `target "nlohmann_json::nlohmann_json" not found` ao gerar `voltz-core-tests`, confira o
`find_package` do Step 1 no `tests/CMakeLists.txt` (o `obs-studio` já tem o seu em `frontend/cmake/os-windows.cmake:13`).

- [ ] **Step 8: COMPILAR APP** (segundo plano) — o `obs-studio` liga o `voltz-core`; precisa continuar `exit=0`.

- [ ] **Step 9: Formatar, testar de novo e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/project-timeline.* frontend/voltz/core/project-manifest.* frontend/voltz/core/project-markers.* frontend/voltz/tests/test-project-timeline.cpp frontend/voltz/tests/test-project-manifest.cpp frontend/voltz/tests/test-project-markers.cpp
```
COMPILAR TESTES + RODAR TESTES (75 passam), então:
```bash
git add frontend/voltz/core frontend/voltz/tests/test-project-timeline.cpp frontend/voltz/tests/test-project-manifest.cpp frontend/voltz/tests/test-project-markers.cpp frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): nucleo do projeto - manifesto v1, marcadores e linha do tempo

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Núcleo — cursor (coordenadas, amostragem, `cursor.jsonl`) e espaço em disco

**Files:**
- Create: `frontend/voltz/core/cursor-track.hpp/.cpp`, `frontend/voltz/core/disk-space.hpp/.cpp`
- Create: `frontend/voltz/tests/test-cursor-track.cpp`, `frontend/voltz/tests/test-disk-space.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

Regras do cursor (spec §5.6): uma linha por evento — `{"t":1234,"x":812,"y":440,"e":"move"}`; `e ∈ move|down|up|
scroll|fora`; `b` (`l|r|m`) em cliques; `d` em rolagem (em "notches": 120 unidades do Windows = 1, positivo = para
cima, pode ser fracionário em touchpad — formatado sem `printf` de ponto flutuante para não depender da locale C);
`move` a ~60 Hz só quando a posição muda (o amostrador recebe um lote por tique de ~16 ms e grava só o último
movimento, mas um movimento pendente sai **antes** de um clique/rolagem para manter a ordem); fora da captura: um
único `{"t":..,"e":"fora"}` por saída; ao voltar, o próximo `move` é gravado mesmo se repetir a posição. As
coordenadas de entrada são **pixels físicos do desktop virtual** (o processo é PMv2: `MSLLHOOKSTRUCT.pt` e
`EnumDisplayMonitors` já vêm físicos), então DPI 100/125/150% mapeia 1:1 no retângulo físico do monitor; se a
captura tiver tamanho diferente do monitor, as coordenadas são escaladas. Captura de **janela** fica para depois
(backlog).

Disco (§8): `< 5 GB` livre = aviso; `< 1 GB` = parar. Estimativa antes de gravar: vídeo por bits/pixel (Alta 0,12;
Arquivo menor 0,05; qualidade "Stream" usa o bitrate da transmissão) + 192 kbps por faixa de áudio. Durante a
gravação, a casca troca pela medida real (tamanho do `programa.mp4` / duração líquida, a partir de 10 s).

- [ ] **Step 1: Escrever os testes (falham)**

`frontend/voltz/tests/test-cursor-track.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/cursor-track.hpp>

#include <vector>

using voltz::CaptureMapping;
using voltz::CapturePoint;
using voltz::CursorEvent;
using voltz::CursorEventType;
using voltz::CursorSampler;
using voltz::MonitorRect;
using voltz::MouseButton;
using voltz::RawMouseEvent;
using voltz::RawMouseKind;
using voltz::cursorLine;
using voltz::mapToCapture;

namespace {

constexpr int64_t kMs = 1000000; // ns por ms

CaptureMapping identity(int left, int top, int width, int height)
{
	return CaptureMapping{MonitorRect{left, top, width, height}, width, height};
}

RawMouseEvent move(int64_t ms, int x, int y)
{
	RawMouseEvent event;
	event.tNs = ms * kMs;
	event.kind = RawMouseKind::Move;
	event.x = x;
	event.y = y;
	return event;
}

RawMouseEvent button(int64_t ms, RawMouseKind kind, MouseButton which, int x, int y)
{
	RawMouseEvent event = move(ms, x, y);
	event.kind = kind;
	event.button = which;
	return event;
}

RawMouseEvent wheel(int64_t ms, int x, int y, int delta)
{
	RawMouseEvent event = move(ms, x, y);
	event.kind = RawMouseKind::Wheel;
	event.wheelDelta = delta;
	return event;
}

} // namespace

TEST_CASE("mapToCapture: monitor único mapeia 1:1 e fora da área é 'fora'")
{
	const CaptureMapping m = identity(0, 0, 1920, 1080);
	CapturePoint p = mapToCapture(m, 0, 0);
	CHECK(p.inside);
	CHECK(p.x == 0);
	CHECK(p.y == 0);
	p = mapToCapture(m, 1919, 1079);
	CHECK(p.inside);
	CHECK(p.x == 1919);
	CHECK(p.y == 1079);
	CHECK_FALSE(mapToCapture(m, 1920, 5).inside);
	CHECK_FALSE(mapToCapture(m, -1, 5).inside);
	CHECK_FALSE(mapToCapture(m, 5, 1080).inside);
}

TEST_CASE("mapToCapture: monitor à esquerda do principal (origem negativa)")
{
	const CaptureMapping m = identity(-2560, 0, 2560, 1440);
	CapturePoint p = mapToCapture(m, -2560, 0);
	CHECK(p.inside);
	CHECK(p.x == 0);
	CHECK(p.y == 0);
	p = mapToCapture(m, -1, 1439);
	CHECK(p.inside);
	CHECK(p.x == 2559);
	CHECK(p.y == 1439);
	CHECK_FALSE(mapToCapture(m, 0, 0).inside);
}

TEST_CASE("mapToCapture: monitor deslocado para cima (y negativo)")
{
	const CaptureMapping m = identity(1920, -300, 1280, 1024);
	CapturePoint p = mapToCapture(m, 1920, -300);
	CHECK(p.inside);
	CHECK(p.x == 0);
	CHECK(p.y == 0);
	p = mapToCapture(m, 2000, 700);
	CHECK(p.inside);
	CHECK(p.x == 80);
	CHECK(p.y == 1000);
	CHECK_FALSE(mapToCapture(m, 2000, 724).inside);
}

TEST_CASE("mapToCapture: DPI 125% e 150% - gancho e retângulo em pixels físicos (PMv2) mapeiam 1:1")
{
	// 125%: 2560x1440 físicos (2048x1152 lógicos). 150% à esquerda: 2880x1620 físicos.
	const CaptureMapping m125 = identity(0, 0, 2560, 1440);
	CapturePoint p = mapToCapture(m125, 2559, 1439);
	CHECK(p.inside);
	CHECK(p.x == 2559);
	CHECK(p.y == 1439);
	const CaptureMapping m150 = identity(-2880, 0, 2880, 1620);
	p = mapToCapture(m150, -1440, 810);
	CHECK(p.inside);
	CHECK(p.x == 1440);
	CHECK(p.y == 810);
}

TEST_CASE("mapToCapture: captura em resolução menor escala as coordenadas")
{
	const CaptureMapping m{MonitorRect{0, 0, 2560, 1440}, 1920, 1080};
	CapturePoint p = mapToCapture(m, 2559, 1439);
	CHECK(p.x == 1919);
	CHECK(p.y == 1079);
	p = mapToCapture(m, 1280, 720);
	CHECK(p.x == 960);
	CHECK(p.y == 540);
}

TEST_CASE("cursorLine segue o formato do cursor.jsonl (spec §5.6)")
{
	CHECK(cursorLine(CursorEvent{1234, CursorEventType::Move, 812, 440}) ==
	      R"({"t":1234,"x":812,"y":440,"e":"move"})");
	CHECK(cursorLine(CursorEvent{5, CursorEventType::Down, 1, 2, MouseButton::Left}) ==
	      R"({"t":5,"x":1,"y":2,"e":"down","b":"l"})");
	CHECK(cursorLine(CursorEvent{6, CursorEventType::Up, 1, 2, MouseButton::Right}) ==
	      R"({"t":6,"x":1,"y":2,"e":"up","b":"r"})");
	CHECK(cursorLine(CursorEvent{7, CursorEventType::Down, 1, 2, MouseButton::Middle}) ==
	      R"({"t":7,"x":1,"y":2,"e":"down","b":"m"})");
	CHECK(cursorLine(CursorEvent{8, CursorEventType::Scroll, 3, 4, MouseButton::None, 120}) ==
	      R"({"t":8,"x":3,"y":4,"e":"scroll","d":1})");
	CHECK(cursorLine(CursorEvent{8, CursorEventType::Scroll, 3, 4, MouseButton::None, -240}) ==
	      R"({"t":8,"x":3,"y":4,"e":"scroll","d":-2})");
	CHECK(cursorLine(CursorEvent{8, CursorEventType::Scroll, 3, 4, MouseButton::None, 60}) ==
	      R"({"t":8,"x":3,"y":4,"e":"scroll","d":0.5})");
	CHECK(cursorLine(CursorEvent{8, CursorEventType::Scroll, 3, 4, MouseButton::None, -8}) ==
	      R"({"t":8,"x":3,"y":4,"e":"scroll","d":-0.067})");
	CHECK(cursorLine(CursorEvent{9, CursorEventType::Outside}) == R"({"t":9,"e":"fora"})");
}

TEST_CASE("CursorSampler::initial registra a posição inicial (ou 'fora')")
{
	CursorSampler inside(identity(0, 0, 1920, 1080), 1000 * kMs);
	std::vector<CursorEvent> events = inside.initial(1500 * kMs, 100, 200);
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == CursorEventType::Move);
	CHECK(events[0].tMs == 500);
	CHECK(events[0].x == 100);
	CHECK(events[0].y == 200);

	CursorSampler outside(identity(0, 0, 1920, 1080), 0);
	events = outside.initial(0, 5000, 5000);
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == CursorEventType::Outside);
}

TEST_CASE("CursorSampler junta os movimentos de um tique no último")
{
	CursorSampler sampler(identity(0, 0, 1920, 1080), 0);
	const std::vector<CursorEvent> events = sampler.process({move(1, 10, 10), move(5, 20, 20), move(9, 30, 30)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].tMs == 9);
	CHECK(events[0].x == 30);
	CHECK(events[0].y == 30);
}

TEST_CASE("CursorSampler: clique e rolagem saem em ordem, com o movimento pendente antes")
{
	CursorSampler sampler(identity(0, 0, 1920, 1080), 0);
	std::vector<CursorEvent> events =
		sampler.process({move(1, 100, 100), button(2, RawMouseKind::Down, MouseButton::Left, 100, 100),
				 button(3, RawMouseKind::Up, MouseButton::Left, 100, 100), move(4, 110, 100)});
	REQUIRE(events.size() == 4);
	CHECK(events[0].type == CursorEventType::Move);
	CHECK(events[1].type == CursorEventType::Down);
	CHECK(events[1].button == MouseButton::Left);
	CHECK(events[1].tMs == 2);
	CHECK(events[2].type == CursorEventType::Up);
	CHECK(events[3].type == CursorEventType::Move);
	CHECK(events[3].x == 110);

	events = sampler.process({wheel(10, 110, 100, -120)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == CursorEventType::Scroll);
	CHECK(events[0].wheelDelta == -120);
}

TEST_CASE("CursorSampler: parado não repete; sair gera um 'fora'; voltar registra de novo")
{
	CursorSampler sampler(identity(0, 0, 1920, 1080), 0);
	CHECK(sampler.process({move(1, 50, 50)}).size() == 1);
	CHECK(sampler.process({move(2, 50, 50)}).empty());
	std::vector<CursorEvent> events = sampler.process(
		{move(3, 5000, 50), move(4, 6000, 50), button(5, RawMouseKind::Down, MouseButton::Left, 6000, 50)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == CursorEventType::Outside);
	CHECK(events[0].tMs == 3);
	events = sampler.process({move(6, 50, 50)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == CursorEventType::Move);
}

TEST_CASE("CursorSampler: tempos em ms relativos ao t0, nunca negativos")
{
	CursorSampler sampler(identity(0, 0, 1920, 1080), 10000 * kMs);
	std::vector<CursorEvent> events = sampler.process({move(10500, 1, 1)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].tMs == 500);
	events = sampler.process({move(9000, 2, 2)});
	REQUIRE(events.size() == 1);
	CHECK(events[0].tMs == 0);
}
```

`frontend/voltz/tests/test-disk-space.cpp`:
```cpp
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

TEST_CASE("formatRate: MB/min inteiro abaixo de 1 GB, GB/min com uma casa acima")
{
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
```
(Conta dos valores: Alta = 1920×1080×60×0,12 = 14.929.920 bit/s + 3×192.000 = 15.505.920 bit/s → ÷8×60 =
116.294.400 B/min ≈ 110,9 MiB → "~111". Arquivo menor = 6.220.800 + 576.000 → 50.976.000 B/min ≈ 48,6 MiB → "~49".
Stream = 2.500.000 + 192.000 → 20.190.000 B/min.)

No `frontend/voltz/tests/CMakeLists.txt`: acrescentar `test-cursor-track.cpp` e `test-disk-space.cpp` (ordem
alfabética, logo depois de `test-atomic-file.cpp`).

- [ ] **Step 2: Rodar e ver falhar**

Run: CONFIGURAR; COMPILAR TESTES. Expected: `Cannot open include file: 'voltz/core/cursor-track.hpp'` (e `disk-space.hpp`).

- [ ] **Step 3: Implementar `cursor-track`**

`frontend/voltz/core/cursor-track.hpp`:
```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Retângulo de um monitor em pixels físicos do desktop virtual (pode ter origem negativa).
struct MonitorRect {
	int left = 0;
	int top = 0;
	int width = 0;
	int height = 0;
};

// Monitor capturado e o tamanho em pixels da captura (igual ao do monitor no monitor_capture).
struct CaptureMapping {
	MonitorRect monitor;
	int captureWidth = 0;
	int captureHeight = 0;
};

struct CapturePoint {
	bool inside = false;
	int x = 0;
	int y = 0;
};

// Ponto do desktop (pixels físicos) → pixels da captura; `inside` falso fora do monitor capturado.
CapturePoint mapToCapture(const CaptureMapping &mapping, int desktopX, int desktopY);

enum class RawMouseKind { Move, Down, Up, Wheel };
enum class MouseButton { None, Left, Right, Middle };

// Evento cru vindo do gancho de mouse: tempo no relógio do libobs (ns) e posição física do desktop.
struct RawMouseEvent {
	int64_t tNs = 0;
	RawMouseKind kind = RawMouseKind::Move;
	int x = 0;
	int y = 0;
	MouseButton button = MouseButton::None;
	int wheelDelta = 0; // unidades do Windows (WHEEL_DELTA = 120); positivo = para cima
};

enum class CursorEventType { Move, Down, Up, Scroll, Outside };

// Uma linha do cursor.jsonl (tempo em ms relativos ao t0, posição em pixels da captura).
struct CursorEvent {
	int64_t tMs = 0;
	CursorEventType type = CursorEventType::Move;
	int x = 0;
	int y = 0;
	MouseButton button = MouseButton::None;
	int wheelDelta = 0;
};

// Linha JSON sem "\n": {"t":1234,"x":812,"y":440,"e":"move"} · {"t":5,"x":1,"y":2,"e":"down","b":"l"} ·
// {"t":8,"x":3,"y":4,"e":"scroll","d":-0.5} · {"t":9,"e":"fora"}
std::string cursorLine(const CursorEvent &event);

// Converte eventos crus em linhas do cursor.jsonl. Chamado pela thread de escrita a cada tique (~16 ms) com tudo o
// que o gancho enfileirou: só o último movimento do tique é gravado (60 Hz), um movimento pendente sai antes de
// clique/rolagem, posição repetida não gera linha, sair da captura gera um único "fora" e cliques fora são ignorados.
class CursorSampler {
public:
	CursorSampler(const CaptureMapping &mapping, int64_t t0Ns);

	// Posição no início da gravação (sempre gera "move" ou "fora").
	std::vector<CursorEvent> initial(int64_t tNs, int desktopX, int desktopY);
	std::vector<CursorEvent> process(const std::vector<RawMouseEvent> &batch);

private:
	int64_t relativeMs(int64_t tNs) const;

	CaptureMapping mapping_;
	int64_t t0Ns_ = 0;
	bool hasLast_ = false;
	int lastX_ = 0;
	int lastY_ = 0;
	bool outside_ = false;
};

} // namespace voltz
```

`frontend/voltz/core/cursor-track.cpp`:
```cpp
#include "cursor-track.hpp"

#include <cstdio>

namespace voltz {

CapturePoint mapToCapture(const CaptureMapping &mapping, int desktopX, int desktopY)
{
	CapturePoint point;
	const MonitorRect &rect = mapping.monitor;
	if (rect.width <= 0 || rect.height <= 0 || mapping.captureWidth <= 0 || mapping.captureHeight <= 0) {
		return point;
	}
	const int64_t dx = static_cast<int64_t>(desktopX) - rect.left;
	const int64_t dy = static_cast<int64_t>(desktopY) - rect.top;
	if (dx < 0 || dy < 0 || dx >= rect.width || dy >= rect.height) {
		return point;
	}
	point.inside = true;
	point.x = static_cast<int>(dx * mapping.captureWidth / rect.width);
	point.y = static_cast<int>(dy * mapping.captureHeight / rect.height);
	return point;
}

namespace {

const char *eventName(CursorEventType type)
{
	switch (type) {
	case CursorEventType::Move:
		return "move";
	case CursorEventType::Down:
		return "down";
	case CursorEventType::Up:
		return "up";
	case CursorEventType::Scroll:
		return "scroll";
	case CursorEventType::Outside:
		break;
	}
	return "fora";
}

char buttonCode(MouseButton button)
{
	switch (button) {
	case MouseButton::Right:
		return 'r';
	case MouseButton::Middle:
		return 'm';
	case MouseButton::Left:
	case MouseButton::None:
		break;
	}
	return 'l';
}

// 120 unidades = 1 "notch", arredondado ao milésimo, sem printf de ponto flutuante (independe da locale C).
std::string formatNotches(int wheelDelta)
{
	const int64_t scaled = static_cast<int64_t>(wheelDelta) * 1000;
	const int64_t thousandths = (scaled + (scaled >= 0 ? 60 : -60)) / 120;
	const bool negative = thousandths < 0;
	const int64_t magnitude = negative ? -thousandths : thousandths;
	std::string text = std::to_string(magnitude / 1000);
	const int64_t fraction = magnitude % 1000;
	if (fraction != 0) {
		char digits[8];
		std::snprintf(digits, sizeof(digits), "%03lld", static_cast<long long>(fraction));
		std::string decimals = digits;
		while (!decimals.empty() && decimals.back() == '0') {
			decimals.pop_back();
		}
		text += "." + decimals;
	}
	return negative ? "-" + text : text;
}

CursorEventType typeFor(RawMouseKind kind)
{
	switch (kind) {
	case RawMouseKind::Down:
		return CursorEventType::Down;
	case RawMouseKind::Up:
		return CursorEventType::Up;
	case RawMouseKind::Wheel:
		return CursorEventType::Scroll;
	case RawMouseKind::Move:
		break;
	}
	return CursorEventType::Move;
}

} // namespace

std::string cursorLine(const CursorEvent &event)
{
	char buffer[128];
	if (event.type == CursorEventType::Outside) {
		std::snprintf(buffer, sizeof(buffer), "{\"t\":%lld,\"e\":\"fora\"}", static_cast<long long>(event.tMs));
		return buffer;
	}
	std::snprintf(buffer, sizeof(buffer), "{\"t\":%lld,\"x\":%d,\"y\":%d,\"e\":\"%s\"",
		      static_cast<long long>(event.tMs), event.x, event.y, eventName(event.type));
	std::string line = buffer;
	if (event.type == CursorEventType::Down || event.type == CursorEventType::Up) {
		line += ",\"b\":\"";
		line += buttonCode(event.button);
		line += '"';
	} else if (event.type == CursorEventType::Scroll) {
		line += ",\"d\":";
		line += formatNotches(event.wheelDelta);
	}
	line += '}';
	return line;
}

CursorSampler::CursorSampler(const CaptureMapping &mapping, int64_t t0Ns) : mapping_(mapping), t0Ns_(t0Ns) {}

int64_t CursorSampler::relativeMs(int64_t tNs) const
{
	const int64_t delta = tNs - t0Ns_;
	return delta > 0 ? delta / 1000000 : 0;
}

std::vector<CursorEvent> CursorSampler::initial(int64_t tNs, int desktopX, int desktopY)
{
	RawMouseEvent raw;
	raw.tNs = tNs;
	raw.kind = RawMouseKind::Move;
	raw.x = desktopX;
	raw.y = desktopY;
	return process({raw});
}

std::vector<CursorEvent> CursorSampler::process(const std::vector<RawMouseEvent> &batch)
{
	std::vector<CursorEvent> out;
	bool hasPending = false;
	CursorEvent pending;
	auto flushPending = [&]() {
		if (hasPending && (!hasLast_ || pending.x != lastX_ || pending.y != lastY_)) {
			out.push_back(pending);
			hasLast_ = true;
			lastX_ = pending.x;
			lastY_ = pending.y;
		}
		hasPending = false;
	};

	for (const RawMouseEvent &raw : batch) {
		const CapturePoint point = mapToCapture(mapping_, raw.x, raw.y);
		const int64_t t = relativeMs(raw.tNs);
		if (!point.inside) {
			flushPending();
			if (!outside_) {
				CursorEvent outsideEvent;
				outsideEvent.tMs = t;
				outsideEvent.type = CursorEventType::Outside;
				out.push_back(outsideEvent);
				outside_ = true;
				hasLast_ = false;
			}
			continue;
		}
		outside_ = false;
		if (raw.kind == RawMouseKind::Move) {
			pending = CursorEvent{t, CursorEventType::Move, point.x, point.y};
			hasPending = true;
			continue;
		}
		flushPending();
		out.push_back(CursorEvent{t, typeFor(raw.kind), point.x, point.y, raw.button, raw.wheelDelta});
		hasLast_ = true;
		lastX_ = point.x;
		lastY_ = point.y;
	}
	flushPending();
	return out;
}

} // namespace voltz
```

- [ ] **Step 4: Implementar `disk-space`**

`frontend/voltz/core/disk-space.hpp`:
```cpp
#pragma once

#include <cstdint>
#include <string>

namespace voltz {

constexpr uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;
constexpr uint64_t kDiskLowBytes = 5 * kGiB;      // spec §8: faixa de aviso
constexpr uint64_t kDiskCriticalBytes = 1 * kGiB; // spec §8: para a gravação

enum class DiskLevel { Ok, Low, Critical };

DiskLevel diskLevel(uint64_t freeBytes);

struct RateInput {
	int width = 1920;
	int height = 1080;
	double fps = 60.0;
	bool compact = false;       // qualidade "Arquivo menor" (Small)
	bool streamEncoder = false; // qualidade "Stream": usa o bitrate da transmissão
	int streamKbps = 0;
	int audioTracks = 1;
	int audioKbps = 192;
};

// Estimativa de bytes por minuto antes de gravar (bits/pixel: Alta 0,12; Arquivo menor 0,05).
uint64_t estimateBytesPerMinute(const RateInput &input);

// Medida real: bytes do arquivo / duração líquida. 0 antes de 10 s (medida instável).
uint64_t measuredBytesPerMinute(uint64_t bytes, int64_t elapsedMs);

// "~111 MB/min" ou "~1,2 GB/min" (base 1024).
std::string formatRate(uint64_t bytesPerMinute, char decimalSeparator);

// "42 GB" (≥ 10 GB), "4,2 GB" (1–10 GB) ou "850 MB"; sempre arredondado para baixo.
std::string formatFreeSpace(uint64_t bytes, char decimalSeparator);

} // namespace voltz
```

`frontend/voltz/core/disk-space.cpp`:
```cpp
#include "disk-space.hpp"

#include <cmath>

namespace voltz {

DiskLevel diskLevel(uint64_t freeBytes)
{
	if (freeBytes < kDiskCriticalBytes) {
		return DiskLevel::Critical;
	}
	if (freeBytes < kDiskLowBytes) {
		return DiskLevel::Low;
	}
	return DiskLevel::Ok;
}

uint64_t estimateBytesPerMinute(const RateInput &input)
{
	double videoBitsPerSecond = 0.0;
	if (input.streamEncoder) {
		videoBitsPerSecond = static_cast<double>(input.streamKbps) * 1000.0;
	} else {
		const double bitsPerPixel = input.compact ? 0.05 : 0.12;
		videoBitsPerSecond =
			static_cast<double>(input.width) * static_cast<double>(input.height) * input.fps * bitsPerPixel;
	}
	const double audioBitsPerSecond =
		static_cast<double>(input.audioTracks) * static_cast<double>(input.audioKbps) * 1000.0;
	const double bytesPerMinute = (videoBitsPerSecond + audioBitsPerSecond) / 8.0 * 60.0;
	return bytesPerMinute > 0.0 ? static_cast<uint64_t>(std::llround(bytesPerMinute)) : 0;
}

uint64_t measuredBytesPerMinute(uint64_t bytes, int64_t elapsedMs)
{
	if (elapsedMs < 10000) {
		return 0;
	}
	return bytes * 60000ULL / static_cast<uint64_t>(elapsedMs);
}

std::string formatRate(uint64_t bytesPerMinute, char decimalSeparator)
{
	if (bytesPerMinute >= kGiB) {
		const long long tenths =
			std::llround(static_cast<double>(bytesPerMinute) / static_cast<double>(kGiB) * 10.0);
		return "~" + std::to_string(tenths / 10) + decimalSeparator + std::to_string(tenths % 10) + " GB/min";
	}
	const long long mebibytes = std::llround(static_cast<double>(bytesPerMinute) / (1024.0 * 1024.0));
	return "~" + std::to_string(mebibytes) + " MB/min";
}

std::string formatFreeSpace(uint64_t bytes, char decimalSeparator)
{
	if (bytes >= 10 * kGiB) {
		return std::to_string(bytes / kGiB) + " GB";
	}
	if (bytes >= kGiB) {
		const uint64_t tenths = bytes * 10 / kGiB;
		return std::to_string(tenths / 10) + decimalSeparator + std::to_string(tenths % 10) + " GB";
	}
	return std::to_string(bytes / (1024ULL * 1024ULL)) + " MB";
}

} // namespace voltz
```

No `frontend/voltz/core/CMakeLists.txt`: acrescentar `cursor-track.cpp` e `disk-space.cpp` (depois de
`atomic-file.cpp`) na lista `PRIVATE` e os `.hpp` na `PUBLIC`.

- [ ] **Step 5: CONFIGURAR, COMPILAR TESTES, RODAR TESTES**

Expected: tudo passa; `test cases: 91 | 91 passed` (75 + 11 + 5).

- [ ] **Step 6: Formatar, testar de novo e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/cursor-track.* frontend/voltz/core/disk-space.* frontend/voltz/tests/test-cursor-track.cpp frontend/voltz/tests/test-disk-space.cpp
```
COMPILAR TESTES + RODAR TESTES (91 passam), então:
```bash
git add frontend/voltz/core/cursor-track.* frontend/voltz/core/disk-space.* frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/test-cursor-track.cpp frontend/voltz/tests/test-disk-space.cpp frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): nucleo do projeto - cursor (coordenadas e amostragem) e espaco em disco

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: `ProjectRecorder` — programa na pasta do projeto, manifesto, pausas, 3 faixas, marcadores e disco

**Files:**
- Create: `frontend/voltz/project/ProjectRecorder.hpp`, `frontend/voltz/project/ProjectRecorder.cpp`
- Modify: `frontend/voltz/shell/RecordingConfig.hpp`, `frontend/voltz/shell/RecordingConfig.cpp`
- Modify: `frontend/voltz/shell/VoltzShell.hpp`, `frontend/voltz/shell/VoltzShell.cpp`
- Modify: `frontend/cmake/feature-voltz.cmake`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`

Fluxo (todo na thread da UI, pelos eventos do frontend):
1. `RECORDING_STARTING` → cria `<pasta>/AAAA-MM-DD HHhMM · Nome` (nome = `user.ini [Voltz] NextProjectName`, padrão
   "Gravação"), escreve `projeto.voltz` (`gravando`) e `marcadores.json` (`[]`), aplica a rota de faixas nas fontes
   globais, liga os sinais `start`/`stopping`/`stop` da saída e faz a **troca temporária** do perfil (pasta do modo
   atual + `Output/FilenameFormatting=programa`). `QTimer::singleShot(0)` desfaz a troca; se a gravação não estiver
   ativa nesse momento (falha síncrona do `obs_output_start`), o projeto fecha como `falhou` (`inicio_falhou`).
2. `RECORDING_STARTED` → `t0_ns` = instante do sinal `start` da saída (fallback: agora); nome real do arquivo por
   `obs_frontend_get_last_recording()`; pulsação de 5 s (reescreve o manifesto com `duracao_ms` e checa o disco).
3. `RECORDING_PAUSED/UNPAUSED` → `pausas`.
4. Marcador (`addMarker`, botão na Task 7, atalho `VoltzStudio.AddMarker` aqui) → capítulo no MP4 +
   `marcadores.json` + manifesto.
5. Disco `< 1 GB` (pulsação ou logo no início) → evento `disco_cheio`, `obs_frontend_recording_stop()` (parada normal,
   arquivo finalizado) e sinal para a casca.
6. `RECORDING_STOPPED` → fim = instante do sinal `stopping` da saída (pedido de parada; o arquivo termina ali, a
   finalização do MP4 vem depois) → `ok` (ou `falhou` + `erro_gravacao` com `codigo` se a saída parou com erro).
7. `EXIT` com projeto ativo → `ok` + `app_fechado_gravando` (a saída ainda finaliza o arquivo depois).

- [ ] **Step 1: `RecordingConfig` — chave da pasta, faixas, padrões de projeto, estimativa e troca temporária**

Substituir `frontend/voltz/shell/RecordingConfig.hpp` inteiro por:
```cpp
#pragma once

#include <voltz/core/recording-settings.hpp>

#include <QString>

#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Lê/escreve as configurações de gravação do perfil do OBS (modo de saída Simples) e as regras de pasta/faixas da
// gravação em projeto. Não é UI: o ProjectRecorder também usa.
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
	// RecQuality "Stream" (padrão do OBS): a gravação reusa o codificador da transmissão (StreamEncoder).
	static bool recordsWithStreamEncoder();
	static std::string streamEncoder();

	// Primeira execução/preset: se RecQuality ainda for o padrão do OBS ("Stream"), passa a "HQ" com o
	// codificador automático e formato MP4 híbrido. Não sobrescreve escolhas já feitas.
	static void applyVoltzDefaults();

	// Chave do perfil com a pasta de gravação no modo atual (Simples: SimpleOutput/FilePath; Avançado: AdvOut/
	// RecFilePath ou, com FFmpeg personalizado gravando em arquivo, AdvOut/FFFilePath). `section` nulo: FFmpeg para
	// URL (sem pasta, sem projeto).
	struct OutputPathKey {
		const char *section = nullptr;
		const char *key = nullptr;
	};
	static OutputPathKey outputPathKey();
	// Pasta de gravação em uso (mesma regra do OBSBasic::GetCurrentOutputPath; vazio com FFmpeg para URL).
	static QString outputFolder();
	// Faixas de áudio da próxima gravação: 1 com qualidade "Stream" ou FLV; senão os bits de RecTracks.
	static int recordingAudioTracks();
	// Padrões da gravação em projeto (spec §5.1), sem sobrescrever escolhas: pasta "Vídeos\Voltz Studio" enquanto
	// SimpleOutput/FilePath e AdvOut/RecFilePath não têm valor do usuário, e 3 faixas (mix, mic, sistema) enquanto
	// SimpleOutput/RecTracks é o padrão. Cria a pasta Voltz quando ela é a pasta em uso.
	static void applyProjectDefaults();
	// Estimativa de bytes por minuto com a configuração atual (antes de haver medição real).
	static uint64_t estimatedBytesPerMinute();

private:
	// Salva o perfil e, se não estiver gravando, recria as saídas para valer já.
	static void commit(bool needsOutputReset);
};

// Troca temporária de valores do perfil do OBS. restore() devolve exatamente o estado anterior: regrava o valor do
// usuário ou, se só havia o padrão, remove a chave (não cria valor do usuário). Não salva o perfil em disco.
class ProfileOverride {
public:
	ProfileOverride() = default;
	ProfileOverride(const ProfileOverride &) = delete;
	ProfileOverride &operator=(const ProfileOverride &) = delete;
	~ProfileOverride() { restore(); }

	void set(const char *section, const char *key, const std::string &value);
	void restore();
	bool active() const { return !saved_.empty(); }

private:
	struct Saved {
		std::string section;
		std::string key;
		std::string value;
		bool hadUserValue = false;
	};
	std::vector<Saved> saved_;
};

} // namespace voltz
```

Em `frontend/voltz/shell/RecordingConfig.cpp`:
- Includes: depois de `#include <widgets/OBSBasic.hpp>`, acrescentar
  ```cpp
  #include <utility/platform.hpp> // GetDefaultVideoSavePath

  #include <voltz/core/disk-space.hpp>
  ```
  e depois de `#include <util/config-file.h>`, `#include <QDir>`.
- No namespace anônimo, depois de `getString(...)`, acrescentar:
  ```cpp
  // Faixas da gravação em projeto (spec §5.1): 1 = mix, 2 = microfone, 3 = som do sistema.
  constexpr int64_t kVoltzRecTracks = (1 << 0) | (1 << 1) | (1 << 2);

  int trackCount(int64_t bits)
  {
  	int count = 0;
  	for (int i = 0; i < 6; i++) {
  		if (bits & (int64_t(1) << i)) {
  			count++;
  		}
  	}
  	return count > 0 ? count : 1;
  }

  QString voltzDefaultFolder()
  {
  	return QDir::toNativeSeparators(QString::fromStdString(GetDefaultVideoSavePath()) +
  					QStringLiteral("/Voltz Studio"));
  }
  ```
- Antes de `void RecordingConfig::commit(...)`, acrescentar:
  ```cpp
  RecordingConfig::OutputPathKey RecordingConfig::outputPathKey()
  {
  	if (isSimpleMode()) {
  		return {"SimpleOutput", "FilePath"};
  	}
  	if (getString("AdvOut", "RecType") == "FFmpeg") {
  		if (!config_get_bool(profile(), "AdvOut", "FFOutputToFile")) {
  			return {};
  		}
  		return {"AdvOut", "FFFilePath"};
  	}
  	return {"AdvOut", "RecFilePath"};
  }

  QString RecordingConfig::outputFolder()
  {
  	const OutputPathKey key = outputPathKey();
  	return key.section ? QString::fromStdString(getString(key.section, key.key)) : QString();
  }

  int RecordingConfig::recordingAudioTracks()
  {
  	if (isSimpleMode()) {
  		if (getString("SimpleOutput", "RecQuality") == "Stream" ||
  		    getString("SimpleOutput", "RecFormat2") == "flv") {
  			return 1;
  		}
  		return trackCount(config_get_int(profile(), "SimpleOutput", "RecTracks"));
  	}
  	return trackCount(config_get_int(profile(), "AdvOut", "RecTracks"));
  }

  void RecordingConfig::applyProjectDefaults()
  {
  	config_t *config = profile();
  	const QString voltzFolder = voltzDefaultFolder();
  	const std::string voltzFolderUtf8 = voltzFolder.toUtf8().toStdString();
  	bool changed = false;
  	for (const OutputPathKey &key : {OutputPathKey{"SimpleOutput", "FilePath"}, OutputPathKey{"AdvOut", "RecFilePath"}}) {
  		if (!config_has_user_value(config, key.section, key.key)) {
  			config_set_string(config, key.section, key.key, voltzFolderUtf8.c_str());
  			changed = true;
  		}
  	}
  	if (!config_has_user_value(config, "SimpleOutput", "RecTracks")) {
  		config_set_int(config, "SimpleOutput", "RecTracks", kVoltzRecTracks);
  		changed = true;
  	}
  	/* O OBS recusa gravar numa pasta que não existe (OBSBasic::OutputPathValid): cria só a pasta Voltz. */
  	if (QDir::toNativeSeparators(outputFolder()).compare(voltzFolder, Qt::CaseInsensitive) == 0) {
  		QDir().mkpath(voltzFolder);
  	}
  	if (changed) {
  		commit(false);
  		blog(LOG_INFO, "[voltz] padroes de projeto aplicados (pasta '%s', %d faixa(s) de audio)",
  		     voltzFolderUtf8.c_str(), recordingAudioTracks());
  	}
  }

  uint64_t RecordingConfig::estimatedBytesPerMinute()
  {
  	RateInput input;
  	obs_video_info ovi = {};
  	if (obs_get_video_info(&ovi)) {
  		input.width = static_cast<int>(ovi.output_width);
  		input.height = static_cast<int>(ovi.output_height);
  		input.fps = ovi.fps_den ? static_cast<double>(ovi.fps_num) / static_cast<double>(ovi.fps_den) : 60.0;
  	}
  	input.audioTracks = recordingAudioTracks();
  	if (isSimpleMode()) {
  		const std::string quality = getString("SimpleOutput", "RecQuality");
  		input.streamEncoder = quality == "Stream";
  		input.compact = quality == "Small";
  		input.streamKbps = static_cast<int>(config_get_int(profile(), "SimpleOutput", "VBitrate"));
  	}
  	return estimateBytesPerMinute(input);
  }

  void ProfileOverride::set(const char *section, const char *key, const std::string &value)
  {
  	config_t *config = profile();
  	Saved saved;
  	saved.section = section;
  	saved.key = key;
  	saved.hadUserValue = config_has_user_value(config, section, key);
  	if (saved.hadUserValue) {
  		const char *current = config_get_string(config, section, key);
  		saved.value = current ? current : "";
  	}
  	saved_.push_back(saved);
  	config_set_string(config, section, key, value.c_str());
  }

  void ProfileOverride::restore()
  {
  	if (saved_.empty()) {
  		return;
  	}
  	config_t *config = profile();
  	for (auto it = saved_.rbegin(); it != saved_.rend(); ++it) {
  		if (it->hadUserValue) {
  			config_set_string(config, it->section.c_str(), it->key.c_str(), it->value.c_str());
  		} else {
  			config_remove_value(config, it->section.c_str(), it->key.c_str());
  		}
  	}
  	saved_.clear();
  }
  ```
  (`estimateBytesPerMinute` e `RateInput` vêm de `<voltz/core/disk-space.hpp>`; dentro de `RecordingConfig` não há
  membro com esse nome, então a chamada não qualificada resolve para a função livre.)

- [ ] **Step 2: Criar `frontend/voltz/project/ProjectRecorder.hpp`**

```cpp
#pragma once

#include <voltz/core/project-manifest.hpp>
#include <voltz/core/project-markers.hpp>
#include <voltz/core/project-timeline.hpp>
#include <voltz/shell/RecordingConfig.hpp>

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QDir>
#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Caminho de pasta/arquivo do Qt → UTF-8 com separadores do Windows, para as funções do núcleo.
inline std::string nativeUtf8Path(const QString &path)
{
	return QDir::toNativeSeparators(path).toUtf8().toStdString();
}

// Gravação em projeto (spec §5). Toda gravação — pela casca, modo clássico, atalho, obs-websocket ou
// --startrecording — vira a pasta "<pasta de gravações>/AAAA-MM-DD HHhMM · Nome" com programa.mp4, projeto.voltz e
// marcadores.json. Não conhece a casca: reage aos eventos do frontend do OBS e emite sinais (thread da UI).
class ProjectRecorder : public QObject {
	Q_OBJECT

public:
	explicit ProjectRecorder(QObject *parent = nullptr);
	~ProjectRecorder() override;

	// Nome do próximo projeto (user.ini [Voltz] NextProjectName; vazio = "Gravação").
	QString nextProjectName() const;
	void setNextProjectName(const QString &name);

	bool isActive() const { return active_; }
	QString currentProjectDir() const { return projectDir_; }
	QString currentProjectName() const { return QString::fromStdString(manifest_.name); }

	// "Marcador N" agora: capítulo no MP4 + marcadores.json + manifesto. Falso sem projeto gravando ou com a
	// gravação pausada (o OBS não aceita capítulo pausado).
	bool addMarker();

	// Bytes/min medidos no arquivo do programa do projeto atual (0 sem projeto ou antes de 10 s).
	uint64_t measuredBytesPerMinute() const;

	// Espaço livre da pasta. VOLTZ_FAKE_FREE_BYTES (só para testes) substitui o valor real. Falso se não souber.
	static bool freeBytes(const QString &folder, quint64 &out);

signals:
	void projectStarted(const QString &dir, const QString &name);
	void projectFinished(const QString &dir, const QString &status);
	void markerAdded(int number, qint64 tMs);
	void stoppedForLowDisk(quint64 freeBytes);
	void manifestWriteFailed(const QString &backupPath);

private:
	static void handleEvent(enum obs_frontend_event event, void *data);
	static void handleHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void handleOutputStart(void *data, calldata_t *params);
	static void handleOutputStopping(void *data, calldata_t *params);
	static void handleOutputStop(void *data, calldata_t *params);

	void dispatch(enum obs_frontend_event event);
	void onStarting();
	void onStarted();
	void onPaused(bool paused);
	void onStopped();
	void onExit();
	void finish(ProjectStatus status, const char *eventType, int code = 0);
	void heartbeat();
	void checkDisk();
	bool writeManifest();
	bool writeMarkers();
	void registerHotkey();
	void loadHotkeyBinding();
	void unregisterHotkey();

	bool registered_ = true;
	bool active_ = false;
	bool lowDiskStop_ = false;
	bool manifestWarned_ = false;
	QString projectDir_;
	ProjectManifest manifest_;
	ProjectTimeline timeline_;
	std::vector<Marker> markers_;
	ProfileOverride override_;
	QTimer heartbeat_;
	obs_hotkey_id markerHotkey_ = OBS_INVALID_HOTKEY_ID;
	OBSSignal startSignal_;
	OBSSignal stoppingSignal_;
	OBSSignal stopSignal_;
	std::atomic<uint64_t> outputStartNs_{0};
	std::atomic<uint64_t> outputStopNs_{0};
	std::atomic<long long> lastStopCode_{0};
};

} // namespace voltz
```

- [ ] **Step 3: Criar `frontend/voltz/project/ProjectRecorder.cpp`**

```cpp
#include "ProjectRecorder.hpp"

#include <voltz/core/atomic-file.hpp>
#include <voltz/core/disk-space.hpp>
#include <voltz/core/project-naming.hpp>

#include <OBSApp.hpp> // QTStr, App(), GetAppConfigPath

#include <util/config-file.h>
#include <util/platform.h>

#include <QDateTime>
#include <QFileInfo>
#include <QStorageInfo>

#include <algorithm>
#include <cstring>

#include "moc_ProjectRecorder.cpp"

namespace voltz {

namespace {

constexpr const char *kMarkerHotkeyName = "VoltzStudio.AddMarker";
constexpr int kHeartbeatMs = 5000;

int64_t nowNs()
{
	return static_cast<int64_t>(os_gettime_ns());
}

std::string utf8(const QString &text)
{
	return text.toUtf8().toStdString();
}

// "2026-10-15T15:30:12-03:00"
QString isoNowWithOffset()
{
	const QDateTime now = QDateTime::currentDateTime();
	return now.toOffsetFromUtc(now.offsetFromUtc()).toString(Qt::ISODate);
}

// Faixas do Voltz (spec §5.1): 1 = mix, 2 = microfone, 3 = som do sistema. Só nas fontes globais de áudio (canais
// 1-6) que ainda estão em "todas as faixas" (0xFF ao criar, 0x3F ao carregar sem valor): escolha do usuário fica.
void applyVoltzAudioRouting()
{
	for (uint32_t channel = 1; channel <= 6; channel++) {
		OBSSourceAutoRelease source = obs_get_output_source(channel);
		if (!source || (obs_source_get_audio_mixers(source) & 0x3F) != 0x3F) {
			continue;
		}
		const char *id = obs_source_get_unversioned_id(source);
		uint32_t mixers = 0;
		if (strcmp(id, "wasapi_output_capture") == 0) {
			mixers = (1u << 0) | (1u << 2);
		} else if (strcmp(id, "wasapi_input_capture") == 0) {
			mixers = (1u << 0) | (1u << 1);
		} else {
			continue;
		}
		obs_source_set_audio_mixers(source, mixers);
		blog(LOG_INFO, "[voltz] faixas de audio de '%s': %s", obs_source_get_name(source),
		     mixers == 5u ? "1+3 (sistema)" : "1+2 (microfone)");
	}
}

} // namespace

ProjectRecorder::ProjectRecorder(QObject *parent) : QObject(parent)
{
	heartbeat_.setInterval(kHeartbeatMs);
	connect(&heartbeat_, &QTimer::timeout, this, &ProjectRecorder::heartbeat);
	obs_frontend_add_event_callback(&ProjectRecorder::handleEvent, this);
	/* A casca nasce depois do OBSBasic::OBSInit: perfil e atalhos do OBS já estão carregados. */
	RecordingConfig::applyProjectDefaults();
	registerHotkey();
}

ProjectRecorder::~ProjectRecorder()
{
	if (registered_) {
		obs_frontend_remove_event_callback(&ProjectRecorder::handleEvent, this);
	}
	unregisterHotkey();
}

QString ProjectRecorder::nextProjectName() const
{
	const char *name = config_get_string(App()->GetUserConfig(), "Voltz", "NextProjectName");
	const QString value = QString::fromUtf8(name ? name : "").trimmed();
	return value.isEmpty() ? QTStr("Voltz.Project.DefaultName") : value;
}

void ProjectRecorder::setNextProjectName(const QString &name)
{
	const QString trimmed = name.trimmed();
	if (trimmed == nextProjectName()) {
		return;
	}
	config_set_string(App()->GetUserConfig(), "Voltz", "NextProjectName", trimmed.toUtf8().constData());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

void ProjectRecorder::handleEvent(enum obs_frontend_event event, void *data)
{
	static_cast<ProjectRecorder *>(data)->dispatch(event);
}

void ProjectRecorder::dispatch(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		onStarting();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		onStarted();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		onPaused(true);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		onPaused(false);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		onStopped();
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		RecordingConfig::applyProjectDefaults();
		loadHotkeyBinding();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		onExit();
		break;
	default:
		break;
	}
}

void ProjectRecorder::onStarting()
{
	if (active_) {
		/* STARTING sem o STOPPED do projeto anterior (não deveria acontecer): fecha o anterior primeiro. */
		finish(ProjectStatus::Failed, "interrompido");
	}
	const RecordingConfig::OutputPathKey key = RecordingConfig::outputPathKey();
	if (!key.section) {
		blog(LOG_INFO, "[voltz] gravacao para URL (FFmpeg personalizado): sem pasta de projeto");
		return;
	}
	const QString rootPath = RecordingConfig::outputFolder();
	const QDir root(rootPath);
	if (rootPath.isEmpty() || !root.exists()) {
		blog(LOG_WARNING, "[voltz] pasta de gravacoes inexistente: sem pasta de projeto");
		return;
	}

	const QString name = nextProjectName();
	const QDateTime now = QDateTime::currentDateTime();
	const LocalDateTime when{now.date().year(), now.date().month(), now.date().day(), now.time().hour(),
				 now.time().minute()};
	const std::string safeName = sanitizeProjectName(utf8(name), utf8(QTStr("Voltz.Project.DefaultName")));
	const std::string folder =
		uniqueFolderName(projectFolderName(when, safeName), [&root](const std::string &candidate) {
			return root.exists(QString::fromStdString(candidate));
		});
	if (!root.mkdir(QString::fromStdString(folder))) {
		blog(LOG_ERROR, "[voltz] nao foi possivel criar a pasta do projeto '%s'", folder.c_str());
		return;
	}
	projectDir_ = root.absoluteFilePath(QString::fromStdString(folder));

	manifest_ = ProjectManifest{};
	manifest_.app = std::string("Voltz Studio ") + VOLTZ_VERSION;
	manifest_.engine = std::string("libobs ") + obs_get_version_string();
	manifest_.name = utf8(name);
	manifest_.createdAt = utf8(isoNowWithOffset());
	manifest_.status = ProjectStatus::Recording;
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	manifest_.program.present = true;
	manifest_.program.file = std::string(kProgramBaseName) + ".mp4";
	manifest_.program.width = static_cast<int>(ovi.output_width);
	manifest_.program.height = static_cast<int>(ovi.output_height);
	manifest_.program.fps =
		ovi.fps_den ? static_cast<double>(ovi.fps_num) / static_cast<double>(ovi.fps_den) : 0.0;
	manifest_.program.audioTracks = RecordingConfig::recordingAudioTracks();
	timeline_ = ProjectTimeline{};
	markers_.clear();
	lowDiskStop_ = false;
	manifestWarned_ = false;
	outputStartNs_ = 0;
	outputStopNs_ = 0;
	lastStopCode_ = 0;
	active_ = true;
	writeManifest();
	writeMarkers();

	applyVoltzAudioRouting();

	OBSOutputAutoRelease output = obs_frontend_get_recording_output();
	if (output) {
		/* (não chamar a variável de "signals": é palavra-chave/macro do Qt) */
		signal_handler_t *handler = obs_output_get_signal_handler(output);
		startSignal_.Connect(handler, "start", &ProjectRecorder::handleOutputStart, this);
		stoppingSignal_.Connect(handler, "stopping", &ProjectRecorder::handleOutputStopping, this);
		stopSignal_.Connect(handler, "stop", &ProjectRecorder::handleOutputStop, this);
	}

	/* O OBSBasic::StartRecording lê a pasta e o formato do nome do arquivo logo depois deste evento, na mesma pilha
	 * da thread da UI (SimpleOutput/AdvancedOutput::ConfigureRecording). Troca temporária, desfeita quando a pilha
	 * volta ao laço de eventos: o perfil salvo nunca vê a pasta do projeto. */
	override_.set(key.section, key.key, nativeUtf8Path(projectDir_));
	override_.set("Output", "FilenameFormatting", kProgramBaseName);
	QTimer::singleShot(0, this, [this]() {
		override_.restore();
		OBSOutputAutoRelease recording = obs_frontend_get_recording_output();
		const bool running = obs_frontend_recording_active() || (recording && obs_output_active(recording));
		if (active_ && !timeline_.started() && !running) {
			blog(LOG_WARNING, "[voltz] a gravacao nao iniciou; projeto marcado como falhou");
			finish(ProjectStatus::Failed, "inicio_falhou");
		}
	});
	blog(LOG_INFO, "[voltz] projeto: %s", qUtf8Printable(projectDir_));
}

void ProjectRecorder::onStarted()
{
	if (!active_ || timeline_.started()) {
		return;
	}
	const uint64_t startNs = outputStartNs_.load();
	timeline_.start(startNs ? static_cast<int64_t>(startNs) : nowNs());
	manifest_.t0Ns = timeline_.t0Ns();
	if (char *last = obs_frontend_get_last_recording()) {
		const QFileInfo file(QString::fromUtf8(last));
		bfree(last);
		if (QDir(file.absolutePath()) == QDir(projectDir_)) {
			manifest_.program.file = utf8(file.fileName());
		} else {
			blog(LOG_WARNING, "[voltz] programa fora da pasta do projeto: %s", qUtf8Printable(file.filePath()));
		}
	}
	writeManifest();
	heartbeat_.start();
	blog(LOG_INFO, "[voltz] projeto gravando: %s (%d faixa(s) de audio)", manifest_.program.file.c_str(),
	     manifest_.program.audioTracks);
	emit projectStarted(projectDir_, QString::fromStdString(manifest_.name));
	checkDisk();
}

void ProjectRecorder::onPaused(bool paused)
{
	if (!active_ || !timeline_.started()) {
		return;
	}
	const int64_t now = nowNs();
	if (paused) {
		timeline_.pause(now);
	} else {
		timeline_.resume(now);
	}
	manifest_.pauses = timeline_.pauses();
	manifest_.durationMs = timeline_.grossMs(now);
	writeManifest();
}

void ProjectRecorder::onStopped()
{
	if (!active_) {
		return;
	}
	const int code = static_cast<int>(lastStopCode_.load());
	if (code != 0) {
		blog(LOG_WARNING, "[voltz] gravacao terminou com erro (codigo %d)", code);
		finish(ProjectStatus::Failed, "erro_gravacao", code);
	} else {
		finish(ProjectStatus::Ok, nullptr);
	}
}

void ProjectRecorder::onExit()
{
	if (active_) {
		/* Fechar o app gravando: a saída ainda está ativa e o OBS finaliza o arquivo ao destruí-la, depois deste
		 * evento (OBSBasic::~OBSBasic → outputHandler.reset()). */
		finish(ProjectStatus::Ok, "app_fechado_gravando");
	}
	unregisterHotkey();
	/* Seguro durante o próprio evento: OBSStudioAPI::on_event percorre os callbacks de trás para frente */
	obs_frontend_remove_event_callback(&ProjectRecorder::handleEvent, this);
	registered_ = false;
}

void ProjectRecorder::finish(ProjectStatus status, const char *eventType, int code)
{
	const uint64_t stopNs = outputStopNs_.load();
	const int64_t end = stopNs ? static_cast<int64_t>(stopNs) : nowNs();
	if (timeline_.started()) {
		if (eventType) {
			manifest_.events.push_back(ManifestEvent{timeline_.relativeMs(end), eventType, "", code});
		}
		timeline_.stop(end);
		manifest_.durationMs = timeline_.grossMs(end);
		manifest_.pauses = timeline_.pauses();
	} else if (eventType) {
		manifest_.events.push_back(ManifestEvent{0, eventType, "", code});
	}
	manifest_.status = status;
	heartbeat_.stop();
	writeManifest();
	startSignal_.Disconnect();
	stoppingSignal_.Disconnect();
	stopSignal_.Disconnect();
	override_.restore();
	active_ = false;
	blog(LOG_INFO, "[voltz] projeto finalizado (%s, %lld ms, %d marcador(es)): %s", statusToString(status),
	     static_cast<long long>(manifest_.durationMs), static_cast<int>(markers_.size()),
	     qUtf8Printable(projectDir_));
	emit projectFinished(projectDir_, QString::fromUtf8(statusToString(status)));
}

void ProjectRecorder::heartbeat()
{
	if (!active_ || !timeline_.started()) {
		return;
	}
	const int64_t now = nowNs();
	manifest_.durationMs = timeline_.grossMs(now);
	manifest_.pauses = timeline_.pauses();
	writeManifest();
	checkDisk();
}

void ProjectRecorder::checkDisk()
{
	quint64 free = 0;
	if (!active_ || lowDiskStop_ || !freeBytes(projectDir_, free) || diskLevel(free) != DiskLevel::Critical) {
		return;
	}
	lowDiskStop_ = true;
	manifest_.events.push_back(ManifestEvent{timeline_.relativeMs(nowNs()), "disco_cheio", "", 0});
	writeManifest();
	blog(LOG_WARNING, "[voltz] disco quase cheio (%llu bytes livres): parando a gravacao",
	     static_cast<unsigned long long>(free));
	/* Parada normal (não forçada): o OBS finaliza o arquivo e o STOPPED fecha o manifesto como "ok". */
	obs_frontend_recording_stop();
	emit stoppedForLowDisk(free);
}

bool ProjectRecorder::freeBytes(const QString &folder, quint64 &out)
{
	bool fake = false;
	const quint64 fakeValue = qEnvironmentVariable("VOLTZ_FAKE_FREE_BYTES").toULongLong(&fake);
	if (fake) {
		out = fakeValue;
		return true;
	}
	if (folder.isEmpty()) {
		return false;
	}
	const QStorageInfo storage(folder);
	if (!storage.isValid() || storage.bytesAvailable() < 0) {
		return false;
	}
	out = static_cast<quint64>(storage.bytesAvailable());
	return true;
}

uint64_t ProjectRecorder::measuredBytesPerMinute() const
{
	if (!active_ || !timeline_.started()) {
		return 0;
	}
	const QFileInfo program(QDir(projectDir_).filePath(QString::fromStdString(manifest_.program.file)));
	const qint64 size = program.exists() ? program.size() : 0;
	/* qualificado: a função livre do núcleo tem o mesmo nome deste método */
	return voltz::measuredBytesPerMinute(static_cast<uint64_t>(size), timeline_.netMs(nowNs()));
}

bool ProjectRecorder::writeManifest()
{
	const std::string json = serializeManifest(manifest_);
	const std::string path = nativeUtf8Path(QDir(projectDir_).filePath(QString::fromUtf8(kManifestFileName)));
	std::string error;
	/* §8: tenta de novo; se persistir, guarda uma cópia em <config>/recovery e avisa uma vez. */
	if (writeFileAtomically(path, json, &error) || writeFileAtomically(path, json, &error)) {
		return true;
	}
	blog(LOG_ERROR, "[voltz] falha ao gravar o manifesto: %s", error.c_str());
	char dir[512];
	if (GetAppConfigPath(dir, sizeof(dir), VOLTZ_CONFIG_DIR "/recovery") <= 0) {
		return false;
	}
	const QString recoveryDir = QString::fromUtf8(dir);
	QDir().mkpath(recoveryDir);
	const QString backup =
		QDir(recoveryDir).filePath(QFileInfo(projectDir_).fileName() + QStringLiteral(".voltz"));
	if (writeFileAtomically(nativeUtf8Path(backup), json, &error)) {
		blog(LOG_WARNING, "[voltz] copia do manifesto em %s", qUtf8Printable(backup));
		if (!manifestWarned_) {
			manifestWarned_ = true;
			emit manifestWriteFailed(backup);
		}
	}
	return false;
}

bool ProjectRecorder::writeMarkers()
{
	const std::string path = nativeUtf8Path(QDir(projectDir_).filePath(QString::fromUtf8(kMarkersFileName)));
	std::string error;
	if (writeFileAtomically(path, serializeMarkers(markers_), &error)) {
		return true;
	}
	blog(LOG_ERROR, "[voltz] falha ao gravar marcadores.json: %s", error.c_str());
	return false;
}

bool ProjectRecorder::addMarker()
{
	if (!active_ || !timeline_.started() || timeline_.paused()) {
		blog(LOG_INFO, "[voltz] marcador ignorado: sem projeto gravando ou gravacao pausada");
		return false;
	}
	const int64_t now = nowNs();
	const int number = static_cast<int>(markers_.size()) + 1;
	const QString name = QTStr("Voltz.Marker.Name").arg(number);
	const int64_t tMs = timeline_.relativeMs(now);
	/* Capítulo no MP4/MOV híbrido; em outros formatos o OBS devolve falso e fica só o marcadores.json. */
	const bool chapter = obs_frontend_recording_add_chapter(name.toUtf8().constData());
	markers_.push_back(Marker{tMs, utf8(name)});
	writeMarkers();
	manifest_.durationMs = timeline_.grossMs(now);
	writeManifest();
	blog(LOG_INFO, "[voltz] marcador %d em %lld ms (capitulo no MP4: %s)", number, static_cast<long long>(tMs),
	     chapter ? "sim" : "nao");
	emit markerAdded(number, tMs);
	return true;
}

void ProjectRecorder::registerHotkey()
{
	if (markerHotkey_ != OBS_INVALID_HOTKEY_ID) {
		return;
	}
	markerHotkey_ = obs_hotkey_register_frontend(kMarkerHotkeyName,
						     QTStr("Voltz.Hotkey.AddMarker").toUtf8().constData(),
						     &ProjectRecorder::handleHotkey, this);
	loadHotkeyBinding();
}

void ProjectRecorder::loadHotkeyBinding()
{
	if (markerHotkey_ == OBS_INVALID_HOTKEY_ID) {
		return;
	}
	/* Mesmo formato do OBS (OBSBasic::InitHotkeys / OBSBasicSettings::SaveHotkeySettings): perfil
	 * [Hotkeys] <nome> = {"bindings":[...]}. Nada salvo: Ctrl+Shift+M. Lista vazia salva = sem atalho. */
	config_t *profile = obs_frontend_get_profile_config();
	const char *json = profile ? config_get_string(profile, "Hotkeys", kMarkerHotkeyName) : nullptr;
	OBSDataArrayAutoRelease bindings;
	if (json && *json) {
		OBSDataAutoRelease data = obs_data_create_from_json(json);
		if (data) {
			bindings = obs_data_get_array(data, "bindings");
		}
	} else {
		bindings = obs_data_array_create();
		OBSDataAutoRelease combo = obs_data_create();
		obs_data_set_bool(combo, "control", true);
		obs_data_set_bool(combo, "shift", true);
		obs_data_set_string(combo, "key", "OBS_KEY_M");
		obs_data_array_push_back(bindings, combo);
	}
	obs_hotkey_load(markerHotkey_, bindings);
}

void ProjectRecorder::unregisterHotkey()
{
	if (markerHotkey_ == OBS_INVALID_HOTKEY_ID) {
		return;
	}
	obs_hotkey_unregister(markerHotkey_);
	markerHotkey_ = OBS_INVALID_HOTKEY_ID;
}

void ProjectRecorder::handleHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<ProjectRecorder *>(data);
	/* Teclado: chega pela rota do OBSBasic (thread da UI). obs-websocket TriggerHotkeyByName: thread do websocket,
	 * com o lock dos atalhos do libobs preso — só enfileira. */
	QMetaObject::invokeMethod(self, [self]() { self->addMarker(); }, Qt::QueuedConnection);
}

void ProjectRecorder::handleOutputStart(void *data, calldata_t *)
{
	static_cast<ProjectRecorder *>(data)->outputStartNs_.store(os_gettime_ns());
}

void ProjectRecorder::handleOutputStopping(void *data, calldata_t *)
{
	static_cast<ProjectRecorder *>(data)->outputStopNs_.store(os_gettime_ns());
}

void ProjectRecorder::handleOutputStop(void *data, calldata_t *params)
{
	auto *self = static_cast<ProjectRecorder *>(data);
	self->lastStopCode_.store(calldata_int(params, "code"));
	if (self->outputStopNs_.load() == 0) {
		self->outputStopNs_.store(os_gettime_ns());
	}
}

} // namespace voltz
```
Notas: os sinais `start`/`stopping`/`stop` da saída podem chegar em thread do libobs — só gravam atômicos; quem lê é
a thread da UI nos eventos `STARTED`/`STOPPED`. `VOLTZ_VERSION`/`VOLTZ_CONFIG_DIR` são definições do alvo
`obs-studio` (`feature-voltz.cmake`).

- [ ] **Step 4: Ligar ao alvo e criar na casca**

`frontend/cmake/feature-voltz.cmake`: no `target_sources(obs-studio PRIVATE ...)`, antes de
`voltz/shell/AudioPanel.cpp`:
```cmake
    voltz/project/ProjectRecorder.cpp
    voltz/project/ProjectRecorder.hpp
```

`frontend/voltz/shell/VoltzShell.hpp`: depois de `class FrontendBridge;` acrescentar `class ProjectRecorder;`; depois
de `FrontendBridge *bridge_ = nullptr;` acrescentar `ProjectRecorder *recorder_ = nullptr;`.

`frontend/voltz/shell/VoltzShell.cpp`: depois de `#include <voltz/core/voltz-version.hpp>` acrescentar
`#include <voltz/project/ProjectRecorder.hpp>`; trocar
```cpp
	bridge_ = new FrontendBridge(this);
```
por
```cpp
	bridge_ = new FrontendBridge(this);
	/* Criado depois da ponte: os callbacks do frontend rodam do último registrado para o primeiro
	 * (OBSStudioAPI::on_event), então o projeto já está atualizado quando a ponte avisa a casca. */
	recorder_ = new ProjectRecorder(this);
```

- [ ] **Step 5: Textos**

Acrescentar ao fim de `frontend/data/locale/voltz-pt-BR.ini`:
```ini
Voltz.Project.DefaultName="Gravação"
Voltz.Marker.Name="Marcador %1"
Voltz.Hotkey.AddMarker="Voltz Studio: adicionar marcador"
```
e ao fim de `frontend/data/locale/voltz-en-US.ini`:
```ini
Voltz.Project.DefaultName="Recording"
Voltz.Marker.Name="Marker %1"
Voltz.Hotkey.AddMarker="Voltz Studio: add marker"
```
Conferir a paridade (comando das Convenções) → sem saída.

- [ ] **Step 6: CONFIGURAR + COMPILAR APP (segundo plano) + RODAR TESTES**

Expected: `exit=0`, sem `: error`; 91 casos passam. Erros prováveis e correção: `C2440`/ambiguidade no
`for (const OutputPathKey &key : {...})` → declarar antes `const OutputPathKey keys[] = {{"SimpleOutput", "FilePath"},
{"AdvOut", "RecFilePath"}};` e iterar `keys`; `config_remove_value` não declarado → falta `#include
<util/config-file.h>` (já está no `RecordingConfig.cpp`).

- [ ] **Step 7: Verificação A — gravação com pausa e marcador por atalho**

Com o **bloco de teste** (user.ini padrão do bloco), dentro do `try`: **Preparo de gravação**, depois
```powershell
$r = Ws '[{"requestType":"GetHotkeyList"}]'
"atalho registrado: " + ($r[0].data.hotkeys -contains "VoltzStudio.AddMarker")
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 3
[void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.AddMarker"}}]'); Start-Sleep 1
[void](Ws '[{"requestType":"PauseRecord"}]'); Start-Sleep 2
[void](Ws '[{"requestType":"ResumeRecord"}]'); Start-Sleep 2
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 4
Close-App
$dirs = Project-Dirs; "projetos: $($dirs.Count) -> $($dirs.Name)"
$d = $dirs[0].FullName; Get-ChildItem -LiteralPath $d | Format-Table Name, Length | Out-Host
Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $d "projeto.voltz") | Out-Host
Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $d "marcadores.json") | Out-Host
if ($ffprobe) { & $ffprobe -v error -show_entries "stream=index,codec_type:format=duration" -show_chapters -of compact (Join-Path $d "programa.mp4") | Out-Host }
Get-ChildItem (Join-Path $voltzCfg "basic\profiles") -Recurse -Filter basic.ini | ForEach-Object { Select-String -LiteralPath $_.FullName -Pattern "FilenameFormatting|FilePath|RecTracks" } | Out-Host
```
Critérios:
1. `atalho registrado: True`; **uma** pasta `AAAA-MM-DD HHhMM · Gravação` com `programa.mp4` (> 0),
   `projeto.voltz`, `marcadores.json` (e nenhum `.tmp`).
2. Manifesto: `status: ok`, `nome: Gravação`, `t0_ns > 0`, uma pausa com `fim_ms > inicio_ms` (≈ 2000 ms ± 700),
   `duracao_ms` ≈ 7–12 s, `duracao_liquida_ms = duracao_ms − pausa`, `arquivos.programa.arquivo: programa.mp4`,
   `faixas_audio: 3`, sem `tela`/`camera`.
3. `marcadores.json`: 1 item `Marcador 1` com `t_ms` ≈ 3000 ± 1500.
4. `ffprobe`: 1 stream de vídeo + **3** de áudio, **1** capítulo (`Marcador 1`), duração ≈ `duracao_liquida_ms` ± 1 s.
   Sem `ffprobe`: o log (`$sp\ultimo-log.txt`) tem `Adding chapter "Marcador 1"` e três encoders
   `simple_aac_recording0/1/2` iniciados depois de `==== Recording Start`.
5. `basic.ini`: na seção `[SimpleOutput]`, `FilePath=` igual a `$out` (valor do teste) e `RecTracks=7`; na
   `[AdvOut]`, `RecFilePath=` com `...\Videos\Voltz Studio` (padrão Voltz); **nenhuma** linha
   `FilenameFormatting=programa` e **nenhum** valor com o caminho da pasta do projeto. (Prova que a troca temporária
   foi desfeita e não foi salva.)
6. Log: `[voltz] padroes de projeto aplicados`, `[voltz] faixas de audio de ...` (para cada fonte global existente),
   `[voltz] projeto gravando: programa.mp4 (3 faixa(s) de audio)`, `[voltz] marcador 1 em ... (capitulo no MP4: sim)`,
   `[voltz] projeto finalizado (ok, ...)`; **não** tem `[voltz] a gravacao nao iniciou`.
Se o `programa.mp4` cair fora da pasta (critério 1 falha e o log mostra `programa fora da pasta do projeto`), a troca
no STARTING não chegou ao `ConfigureRecording`: confira com um `blog` temporário em `onStarting` se o evento chega
antes da linha `Writing Hybrid MP4/MOV file` do log; se chegar depois, pare e relate (a alternativa é iniciar a
gravação pela casca com a troca feita antes de `obs_frontend_recording_start()`, perdendo as outras origens).

- [ ] **Step 8: Verificação B — fechar o app gravando**

Bloco de teste: **Preparo de gravação**, `[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 4; Close-App`,
depois leia o manifesto do único projeto. Critério: `status: ok`, `eventos` com um `app_fechado_gravando`,
`duracao_ms` ≈ 4000 ± 1500, `programa.mp4` > 0 e reproduzível (`ffprobe` sem erro).

- [ ] **Step 9: Verificação C — disco quase cheio (simulado)**

Bloco de teste com `$fakeFree = "536870912"` (0,5 GB): **Preparo de gravação**,
`[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 8; $st = Ws '[{"requestType":"GetRecordStatus"}]'; "ativa: " + $st[0].data.outputActive`,
`Close-App`, leia o manifesto. Critério: `ativa: False`; manifesto `status: ok` com evento `disco_cheio`; log com
`[voltz] disco quase cheio (536870912 bytes livres)`; `programa.mp4` > 0.

- [ ] **Step 10: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/project/ProjectRecorder.* frontend/voltz/shell/RecordingConfig.* frontend/voltz/shell/VoltzShell.*
```
COMPILAR APP de novo (segundo plano, `exit=0`), então:
```bash
git add frontend/voltz/project frontend/voltz/shell/RecordingConfig.* frontend/voltz/shell/VoltzShell.* frontend/cmake/feature-voltz.cmake frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): gravacao em projeto (programa.mp4 na pasta, manifesto, pausas, 3 faixas, marcadores)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: `CursorTracker` — `cursor.jsonl` com gancho de mouse em thread própria

**Files:**
- Create: `frontend/voltz/project/MonitorLocator.hpp/.cpp`, `frontend/voltz/project/CursorTracker.hpp/.cpp`
- Modify: `frontend/voltz/project/ProjectRecorder.hpp/.cpp`, `frontend/cmake/feature-voltz.cmake`

Desenho e riscos:
- **Thread do gancho** (`voltz-cursor-hook`, prioridade acima do normal): cria a fila de mensagens, instala
  `SetWindowsHookExW(WH_MOUSE_LL)` e fica em `GetMessageW`. O `mouseHook` só copia `pt`/botão/rolagem + `os_gettime_ns()`
  para uma fila protegida por mutex (seção de microssegundos) e chama `CallNextHookEx`. **Nenhum IO, conversão ou log
  no gancho** — gancho lento atrasa o mouse do sistema todo e, depois de `LowLevelHooksTimeout`, o Windows o remove
  sem aviso. Fila com teto (20 000 eventos): se a escrita travar, eventos são descartados (contados no log), nunca o
  mouse. Parar = `PostThreadMessageW(WM_QUIT)` + `join` (o gancho roda na própria thread que o instalou, então depois
  do `join` nenhum callback pode estar em curso).
- **Thread de escrita** (`voltz-cursor-writer`): a cada ~16 ms troca a fila por um vetor vazio, passa pelo
  `CursorSampler` (núcleo) e acumula linhas; grava no arquivo a cada 1 s ou 64 KB (e no fim). Grava antes a posição
  inicial (`GetPhysicalCursorPos`), para o `cursor.jsonl` nunca nascer vazio.
- **Qual tela:** a fonte compartilhada "Tela" (`user.ini [Voltz] ScreenSourceUuid`) se for `monitor_capture`; senão a
  primeira `monitor_capture` da coleção. `monitor_id` → monitor pela mesma busca do `win-capture`
  (`plugins/win-capture/duplicator-monitor-capture.c:185-291`: `DeviceID` de `EnumDisplayDevicesA(...,
  EDD_GET_DEVICE_INTERFACE_NAME)` e, na falta, o nome GDI `\\.\DISPLAYn`). Captura = retângulo físico do monitor.
  Escala por `GetDpiForMonitor(MDT_EFFECTIVE_DPI)` só para o manifesto. Sem monitor (só captura de janela, ou nenhuma
  Tela): sem `cursor.jsonl` e evento `cursor_indisponivel`. Trocar de monitor no meio da gravação não é acompanhado
  (backlog).
- Pausa: o rastreio continua (os tempos são relativos ao t0, sem descontar pausas; o editor aplica `pausas`).
- **Depuração:** não pare num breakpoint dentro de `mouseHook` com o mouse em uso — o mouse do sistema congela até
  o timeout.

- [ ] **Step 1: `frontend/voltz/project/MonitorLocator.hpp`**

```cpp
#pragma once

#include <voltz/core/cursor-track.hpp>

#include <string>

namespace voltz {

struct MonitorDescription {
	bool found = false;
	std::string id;      // monitor_id da fonte
	MonitorRect rect;    // pixels físicos do desktop virtual
	double scale = 1.0;  // DPI efetivo / 96
};

// Monitor de uma fonte monitor_capture pelo "monitor_id" (mesma busca do plugin win-capture: DeviceID da interface
// do monitor e, se não achar, o nome GDI "\\.\DISPLAYn"). found = false se o monitor não existe mais.
MonitorDescription findMonitor(const std::string &monitorId);

} // namespace voltz
```

- [ ] **Step 2: `frontend/voltz/project/MonitorLocator.cpp`**

```cpp
#include "MonitorLocator.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellscalingapi.h>
#endif

namespace voltz {

#ifdef _WIN32
namespace {

struct MonitorSearch {
	std::string id;
	bool byGdiName = false;
	HMONITOR handle = nullptr;
	RECT rect = {};
};

BOOL CALLBACK matchMonitor(HMONITOR handle, HDC, LPRECT rect, LPARAM param)
{
	auto *search = reinterpret_cast<MonitorSearch *>(param);
	MONITORINFOEXA info = {};
	info.cbSize = sizeof(info);
	if (!GetMonitorInfoA(handle, reinterpret_cast<LPMONITORINFO>(&info))) {
		return TRUE;
	}
	bool match = false;
	if (search->byGdiName) {
		match = search->id == info.szDevice;
	} else {
		DISPLAY_DEVICEA device = {};
		device.cb = sizeof(device);
		if (EnumDisplayDevicesA(info.szDevice, 0, &device, EDD_GET_DEVICE_INTERFACE_NAME)) {
			match = search->id == device.DeviceID;
		}
	}
	if (match) {
		search->handle = handle;
		search->rect = *rect;
	}
	return match ? FALSE : TRUE;
}

} // namespace
#endif

MonitorDescription findMonitor(const std::string &monitorId)
{
	MonitorDescription result;
#ifdef _WIN32
	if (monitorId.empty()) {
		return result;
	}
	MonitorSearch search;
	search.id = monitorId;
	EnumDisplayMonitors(nullptr, nullptr, matchMonitor, reinterpret_cast<LPARAM>(&search));
	if (!search.handle) {
		search.byGdiName = true;
		EnumDisplayMonitors(nullptr, nullptr, matchMonitor, reinterpret_cast<LPARAM>(&search));
	}
	if (!search.handle) {
		return result;
	}
	result.found = true;
	result.id = monitorId;
	result.rect = MonitorRect{static_cast<int>(search.rect.left), static_cast<int>(search.rect.top),
				  static_cast<int>(search.rect.right - search.rect.left),
				  static_cast<int>(search.rect.bottom - search.rect.top)};
	UINT dpiX = 96;
	UINT dpiY = 96;
	if (SUCCEEDED(GetDpiForMonitor(search.handle, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
		result.scale = static_cast<double>(dpiX) / 96.0;
	}
#else
	(void)monitorId;
#endif
	return result;
}

} // namespace voltz
```

- [ ] **Step 3: `frontend/voltz/project/CursorTracker.hpp`**

```cpp
#pragma once

#include <voltz/core/cursor-track.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace voltz {

// Registra o mouse em cursor.jsonl (spec §5.6) enquanto o projeto grava. Um gancho WH_MOUSE_LL numa thread só dele
// (só enfileira) e uma thread de escrita (60 Hz, conversão para pixels da captura, gravação com buffer). Nada roda
// na thread da UI. Só uma instância ativa por vez. start/stop chamados da thread da UI.
class CursorTracker {
public:
	CursorTracker() = default;
	~CursorTracker() { stop(); }
	CursorTracker(const CursorTracker &) = delete;
	CursorTracker &operator=(const CursorTracker &) = delete;

	// `utf8Path`: caminho do cursor.jsonl; `t0Ns`: início do projeto (os_gettime_ns). Falso se o arquivo não abrir
	// ou o gancho não instalar.
	bool start(const std::string &utf8Path, const CaptureMapping &mapping, int64_t t0Ns);
	// Desinstala o gancho, grava o que falta e fecha o arquivo. Idempotente.
	void stop();

	bool running() const { return running_; }
	uint64_t linesWritten() const { return lines_.load(); }
	uint64_t droppedEvents() const { return dropped_.load(); }

	// Chamado pelo gancho (thread do gancho): só enfileira.
	void push(const RawMouseEvent &event);

private:
	void hookThreadMain(std::promise<bool> ready);
	void writerThreadMain();

	FILE *file_ = nullptr;
	CaptureMapping mapping_;
	int64_t t0Ns_ = 0;
	std::thread hookThread_;
	std::thread writerThread_;
	std::atomic<unsigned long> hookThreadId_{0};
	std::mutex mutex_;
	std::condition_variable wake_;
	std::vector<RawMouseEvent> queue_;
	bool stopping_ = false;
	bool running_ = false;
	std::atomic<uint64_t> lines_{0};
	std::atomic<uint64_t> dropped_{0};
};

} // namespace voltz
```

- [ ] **Step 4: `frontend/voltz/project/CursorTracker.cpp`**

```cpp
#include "CursorTracker.hpp"

#include <util/platform.h>
#include <util/threading.h>

#include <chrono>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace voltz {

namespace {

constexpr size_t kMaxQueued = 20000;
constexpr auto kTick = std::chrono::milliseconds(16);
constexpr auto kFlushEvery = std::chrono::seconds(1);
constexpr size_t kFlushBytes = 64 * 1024;

std::atomic<CursorTracker *> gActive{nullptr};

#ifdef _WIN32
FILE *openForWrite(const std::string &utf8Path)
{
	const int size = MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, nullptr, 0);
	if (size <= 0) {
		return nullptr;
	}
	std::wstring wide(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, wide.data(), size);
	return _wfopen(wide.c_str(), L"wb");
}

// Roda na thread do gancho a cada evento de mouse do sistema: precisa voltar em microssegundos.
LRESULT CALLBACK mouseHook(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION) {
		if (CursorTracker *tracker = gActive.load(std::memory_order_acquire)) {
			const auto *info = reinterpret_cast<const MSLLHOOKSTRUCT *>(lParam);
			RawMouseEvent event;
			event.tNs = static_cast<int64_t>(os_gettime_ns());
			event.x = info->pt.x;
			event.y = info->pt.y;
			bool known = true;
			switch (wParam) {
			case WM_MOUSEMOVE:
				event.kind = RawMouseKind::Move;
				break;
			case WM_LBUTTONDOWN:
				event.kind = RawMouseKind::Down;
				event.button = MouseButton::Left;
				break;
			case WM_LBUTTONUP:
				event.kind = RawMouseKind::Up;
				event.button = MouseButton::Left;
				break;
			case WM_RBUTTONDOWN:
				event.kind = RawMouseKind::Down;
				event.button = MouseButton::Right;
				break;
			case WM_RBUTTONUP:
				event.kind = RawMouseKind::Up;
				event.button = MouseButton::Right;
				break;
			case WM_MBUTTONDOWN:
				event.kind = RawMouseKind::Down;
				event.button = MouseButton::Middle;
				break;
			case WM_MBUTTONUP:
				event.kind = RawMouseKind::Up;
				event.button = MouseButton::Middle;
				break;
			case WM_MOUSEWHEEL:
				event.kind = RawMouseKind::Wheel;
				event.wheelDelta = static_cast<short>(HIWORD(info->mouseData));
				break;
			default: /* rolagem horizontal e botões X: fora do 3A */
				known = false;
				break;
			}
			if (known) {
				tracker->push(event);
			}
		}
	}
	return CallNextHookEx(nullptr, code, wParam, lParam);
}
#endif

} // namespace

bool CursorTracker::start(const std::string &utf8Path, const CaptureMapping &mapping, int64_t t0Ns)
{
#ifdef _WIN32
	if (running_) {
		return false;
	}
	CursorTracker *expected = nullptr;
	if (!gActive.compare_exchange_strong(expected, this)) {
		return false; // outro rastreador ativo
	}
	file_ = openForWrite(utf8Path);
	if (!file_) {
		gActive.store(nullptr);
		return false;
	}
	mapping_ = mapping;
	t0Ns_ = t0Ns;
	stopping_ = false;
	queue_.clear();
	lines_ = 0;
	dropped_ = 0;

	std::promise<bool> ready;
	std::future<bool> installed = ready.get_future();
	hookThread_ = std::thread(&CursorTracker::hookThreadMain, this, std::move(ready));
	if (!installed.get()) {
		hookThread_.join();
		std::fclose(file_);
		file_ = nullptr;
		gActive.store(nullptr);
		return false;
	}
	writerThread_ = std::thread(&CursorTracker::writerThreadMain, this);
	running_ = true;
	return true;
#else
	(void)utf8Path;
	(void)mapping;
	(void)t0Ns;
	return false;
#endif
}

void CursorTracker::stop()
{
#ifdef _WIN32
	if (!running_) {
		return;
	}
	running_ = false;
	CursorTracker *self = this;
	gActive.compare_exchange_strong(self, nullptr);
	PostThreadMessageW(hookThreadId_.load(), WM_QUIT, 0, 0);
	if (hookThread_.joinable()) {
		hookThread_.join();
	}
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stopping_ = true;
	}
	wake_.notify_all();
	if (writerThread_.joinable()) {
		writerThread_.join();
	}
	if (file_) {
		std::fclose(file_);
		file_ = nullptr;
	}
#endif
}

void CursorTracker::push(const RawMouseEvent &event)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (queue_.size() >= kMaxQueued) {
		dropped_++;
		return;
	}
	queue_.push_back(event);
}

void CursorTracker::hookThreadMain(std::promise<bool> ready)
{
#ifdef _WIN32
	os_set_thread_name("voltz-cursor-hook");
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	MSG msg;
	/* cria a fila de mensagens desta thread antes de publicar o id (o PostThreadMessage do stop precisa dela) */
	PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
	hookThreadId_.store(GetCurrentThreadId());
	HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, mouseHook, GetModuleHandleW(nullptr), 0);
	ready.set_value(hook != nullptr);
	if (!hook) {
		return;
	}
	while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	UnhookWindowsHookEx(hook);
#else
	ready.set_value(false);
#endif
}

void CursorTracker::writerThreadMain()
{
	os_set_thread_name("voltz-cursor-writer");
	CursorSampler sampler(mapping_, t0Ns_);
	std::string buffer;
	auto append = [&](const std::vector<CursorEvent> &events) {
		for (const CursorEvent &event : events) {
			buffer += cursorLine(event);
			buffer += '\n';
		}
		lines_ += events.size();
	};
#ifdef _WIN32
	POINT point;
	if (GetPhysicalCursorPos(&point)) {
		append(sampler.initial(static_cast<int64_t>(os_gettime_ns()), point.x, point.y));
	}
#endif
	std::vector<RawMouseEvent> batch;
	auto lastFlush = std::chrono::steady_clock::now();
	bool done = false;
	while (!done) {
		{
			std::unique_lock<std::mutex> lock(mutex_);
			wake_.wait_for(lock, kTick, [this]() { return stopping_; });
			batch.swap(queue_);
			done = stopping_;
		}
		append(sampler.process(batch));
		batch.clear();
		const auto now = std::chrono::steady_clock::now();
		if (!buffer.empty() && (done || buffer.size() >= kFlushBytes || now - lastFlush >= kFlushEvery)) {
			std::fwrite(buffer.data(), 1, buffer.size(), file_);
			std::fflush(file_);
			buffer.clear();
			lastFlush = now;
		}
	}
}

} // namespace voltz
```

- [ ] **Step 5: Ligar no `ProjectRecorder`**

`frontend/voltz/project/ProjectRecorder.hpp`:
- depois de `#include <atomic>` acrescentar `#include <memory>`;
- logo depois de `namespace voltz {` acrescentar `class CursorTracker;`;
- depois de `void unregisterHotkey();` acrescentar
  ```cpp
  	void startCursor();
  	void stopCursor();
  ```
- depois de `std::vector<Marker> markers_;` acrescentar `std::unique_ptr<CursorTracker> cursor_;`.

`frontend/voltz/project/ProjectRecorder.cpp`:
- depois de `#include "ProjectRecorder.hpp"` acrescentar
  ```cpp
  #include "CursorTracker.hpp"
  #include "MonitorLocator.hpp"
  ```
  e depois de `#include <voltz/core/project-naming.hpp>`, `#include <voltz/shell/SharedSources.hpp>`;
- no namespace anônimo, depois de `applyVoltzAudioRouting()`:
  ```cpp
  // Tela do cursor: a "Tela" compartilhada se for captura de monitor; senão a primeira captura de monitor da coleção.
  OBSSource findScreenCaptureSource()
  {
  	OBSSource shared = findSharedSource(SharedSource::Screen);
  	if (shared && strcmp(obs_source_get_unversioned_id(shared), "monitor_capture") == 0) {
  		return shared;
  	}
  	OBSSource found;
  	obs_enum_sources(
  		[](void *param, obs_source_t *source) {
  			if (strcmp(obs_source_get_unversioned_id(source), "monitor_capture") != 0) {
  				return true;
  			}
  			*static_cast<OBSSource *>(param) = source;
  			return false;
  		},
  		&found);
  	return found;
  }
  ```
- no destrutor, primeira linha: `stopCursor();`
- em `onStarted()`, trocar `	writeManifest();\n	heartbeat_.start();` por
  ```cpp
  	startCursor();
  	writeManifest();
  	heartbeat_.start();
  ```
- em `finish(...)`, trocar `	heartbeat_.stop();\n	writeManifest();` por
  ```cpp
  	heartbeat_.stop();
  	stopCursor();
  	writeManifest();
  ```
- no fim do arquivo (antes de `} // namespace voltz`):
  ```cpp
  void ProjectRecorder::startCursor()
  {
  	stopCursor();
  	OBSSource screen = findScreenCaptureSource();
  	std::string monitorId;
  	MonitorDescription monitor;
  	if (screen) {
  		OBSDataAutoRelease settings = obs_source_get_settings(screen);
  		monitorId = obs_data_get_string(settings, "monitor_id");
  		monitor = findMonitor(monitorId);
  	}
  	if (!monitor.found) {
  		/* Sem captura de monitor (ex. só captura de janela, fora do 3A): projeto sem cursor.jsonl. */
  		blog(LOG_INFO, "[voltz] cursor: nenhuma captura de monitor encontrada; sem cursor.jsonl");
  		manifest_.events.push_back(ManifestEvent{0, "cursor_indisponivel", "", 0});
  		return;
  	}
  	const CaptureMapping mapping{monitor.rect, monitor.rect.width, monitor.rect.height};
  	auto tracker = std::make_unique<CursorTracker>();
  	const QString path = QDir(projectDir_).filePath(QString::fromUtf8(kCursorFileName));
  	if (!tracker->start(nativeUtf8Path(path), mapping, timeline_.t0Ns())) {
  		blog(LOG_WARNING, "[voltz] cursor: nao foi possivel iniciar o gancho do mouse");
  		manifest_.events.push_back(ManifestEvent{0, "cursor_indisponivel", "", 0});
  		return;
  	}
  	cursor_ = std::move(tracker);
  	manifest_.cursor.present = true;
  	manifest_.cursor.file = kCursorFileName;
  	manifest_.cursor.width = monitor.rect.width;
  	manifest_.cursor.height = monitor.rect.height;
  	manifest_.cursor.origin.type = "monitor";
  	manifest_.cursor.origin.id = monitorId;
  	manifest_.cursor.origin.rect[0] = monitor.rect.left;
  	manifest_.cursor.origin.rect[1] = monitor.rect.top;
  	manifest_.cursor.origin.rect[2] = monitor.rect.width;
  	manifest_.cursor.origin.rect[3] = monitor.rect.height;
  	manifest_.cursor.origin.scale = monitor.scale;
  	blog(LOG_INFO, "[voltz] cursor: monitor (%d,%d %dx%d) escala %d%%", monitor.rect.left, monitor.rect.top,
  	     monitor.rect.width, monitor.rect.height, static_cast<int>(monitor.scale * 100.0 + 0.5));
  }

  void ProjectRecorder::stopCursor()
  {
  	if (!cursor_) {
  		return;
  	}
  	cursor_->stop();
  	blog(LOG_INFO, "[voltz] cursor: %llu linha(s), %llu evento(s) descartado(s)",
  	     static_cast<unsigned long long>(cursor_->linesWritten()),
  	     static_cast<unsigned long long>(cursor_->droppedEvents()));
  	cursor_.reset();
  }
  ```

`frontend/cmake/feature-voltz.cmake`: antes de `voltz/project/ProjectRecorder.cpp` acrescentar
```cmake
    voltz/project/CursorTracker.cpp
    voltz/project/CursorTracker.hpp
    voltz/project/MonitorLocator.cpp
    voltz/project/MonitorLocator.hpp
```
e trocar `target_link_libraries(obs-studio PRIVATE dwmapi)` por
`target_link_libraries(obs-studio PRIVATE dwmapi shcore)` (`GetDpiForMonitor`).

- [ ] **Step 6: CONFIGURAR + COMPILAR APP (segundo plano)**

Expected: `exit=0`. Se `GetDpiForMonitor` não linkar (`LNK2019`), confira o `shcore`.

- [ ] **Step 7: Verificação — cursor.jsonl com a Tela de teste**

Bloco de teste (user.ini padrão), no `try`: **Preparo de gravação**, **Tela de teste**, depois
```powershell
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 4
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 3
Close-App
$d = (Project-Dirs)[0].FullName
$m = Read-Json (Join-Path $d "projeto.voltz"); $m.arquivos.cursor | ConvertTo-Json -Depth 5 | Out-Host
Get-Content -Encoding UTF8 -LiteralPath (Join-Path $d "cursor.jsonl") | Select-Object -First 5 | Out-Host
Select-String -LiteralPath "$sp\ultimo-log.txt" -Pattern "\[voltz\] cursor|display:" -Encoding UTF8 | Out-Host
```
(o `ultimo-log.txt` só é copiado no `finally`; para ler dentro do `try`, copie antes o log mais recente de
`$voltzCfg\logs` para `$sp\ultimo-log.txt`.)
Critérios:
1. `arquivos.cursor`: `arquivo: cursor.jsonl`, `largura`/`altura` > 0 e iguais a `retangulo[2]`/`[3]`;
   `origem.tipo: monitor`; `origem.id` = o `monitor_id` escolhido; `escala` ≥ 1.
2. `cursor.jsonl` tem ≥ 1 linha; toda linha é JSON com `t` ≥ 0 e `e` ∈ {move, down, up, scroll, fora}; para `e ≠ fora`,
   `0 ≤ x < largura` e `0 ≤ y < altura`. (Sem mexer no mouse, normalmente há só a posição inicial: `move` se o
   cursor está nesse monitor, `fora` se está em outro.)
3. Log: `[voltz] cursor: monitor (L,T WxH) escala N%` com `W×H` igual ao `display: ... (WxH)` que o
   `win-capture` registrou para a "Tela teste" (mesmo processo PMv2 → mesmo retângulo físico), e
   `[voltz] cursor: N linha(s), 0 evento(s) descartado(s)`.
4. O mouse do usuário não ficou lento durante o teste (pergunte se ele estava usando a máquina; se notar atraso,
   pare e relate — o gancho precisa ser revisto antes de seguir).

Depois, **sem** a Tela de teste (só Preparo + 3 s gravando): manifesto **sem** `arquivos.cursor`, com evento
`cursor_indisponivel`, sem `cursor.jsonl`; log `[voltz] cursor: nenhuma captura de monitor encontrada`.

- [ ] **Step 8: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/project/*.hpp frontend/voltz/project/*.cpp
```
COMPILAR APP (segundo plano, `exit=0`), então:
```bash
git add frontend/voltz/project frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): cursor.jsonl com gancho de mouse em thread propria

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Projetos — lista de projetos, renomear e recuperar gravação interrompida

**Files:**
- Create: `frontend/voltz/project/ProjectLibrary.hpp`, `frontend/voltz/project/ProjectLibrary.cpp`
- Modify (reescrita): `frontend/voltz/shell/ProjectsPage.hpp`, `frontend/voltz/shell/ProjectsPage.cpp`
- Modify: `frontend/voltz/shell/VoltzShell.cpp`, `frontend/voltz/shell/voltz-shell.qss`, `frontend/cmake/feature-voltz.cmake`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`

A tela passa a listar **projetos** (subpastas com `projeto.voltz`) e continua listando **vídeos soltos** na raiz
(gravações antigas, de antes do 3A). Colunas: Nome · Data · Duração (líquida) · Tamanho (soma dos arquivos da pasta)
· Status (`OK`, `Gravando` para o projeto ativo, `Recuperável` para `gravando` órfão, `Recuperado`, `Falhou`;
manifesto de versão mais nova: `—`, só leitura). Ações: Abrir (o vídeo do programa; sem ele, a pasta), Mostrar na
pasta, Renomear (só o `nome` do manifesto), Recuperar gravação (selecionada) e uma faixa "Recuperar gravações" quando
houver projetos interrompidos, Mover para a lixeira (pasta inteira). O projeto em gravação não pode ser renomeado,
recuperado nem apagado.

- [ ] **Step 1: `frontend/voltz/project/ProjectLibrary.hpp`**

```cpp
#pragma once

#include <voltz/core/project-manifest.hpp>

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace voltz {

// Uma linha da tela Projetos: projeto (pasta com projeto.voltz) ou vídeo solto na raiz da pasta de gravações.
struct ProjectEntry {
	bool isProject = false;
	bool unreadable = false;  // projeto.voltz ilegível
	bool newerFormat = false; // versao > 1: só leitura
	QString path;             // pasta do projeto ou o próprio arquivo
	QString programFile;      // vídeo a abrir
	QString name;
	QDateTime date;
	qint64 durationMs = -1; // líquida; -1 = desconhecida
	quint64 sizeBytes = 0;  // soma dos arquivos da pasta (sem subpastas)
	ProjectStatus status = ProjectStatus::Ok;
};

// Leitura e manutenção dos projetos de uma pasta de gravações (thread da UI; IO pequeno).
class ProjectLibrary {
public:
	// Projetos e vídeos soltos, mais recentes primeiro.
	static QList<ProjectEntry> scan(const QString &folder);
	// Nomes distintos dos projetos mais recentes (seletor da tela Gravar).
	static QStringList recentNames(const QString &folder, int max = 8);
	static bool readManifest(const QString &projectDir, ProjectManifest &out, bool *newerFormat = nullptr);
	// Troca só o "nome" do manifesto (a pasta mantém o nome: arquivos podem estar abertos em outro programa).
	static bool rename(const QString &projectDir, const QString &newName);
	// Projeto "gravando" órfão (o app caiu): fecha o manifesto com a duração estimada — maior entre a última pulsação
	// e mtime(programa) − criado_em — e status "recuperado". Falso se não for um projeto "gravando" v1.
	static bool recover(const QString &projectDir);
};

} // namespace voltz
```

- [ ] **Step 2: `frontend/voltz/project/ProjectLibrary.cpp`**

```cpp
#include "ProjectLibrary.hpp"

#include "ProjectRecorder.hpp" // nativeUtf8Path

#include <voltz/core/atomic-file.hpp>
#include <voltz/core/recording-files.hpp>

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <string>
#include <utility>

namespace voltz {

namespace {

QString manifestPath(const QString &projectDir)
{
	return QDir(projectDir).filePath(QString::fromUtf8(kManifestFileName));
}

} // namespace

bool ProjectLibrary::readManifest(const QString &projectDir, ProjectManifest &out, bool *newerFormat)
{
	std::string text;
	if (!readFileText(nativeUtf8Path(manifestPath(projectDir)), text)) {
		return false;
	}
	ManifestParse parsed = parseManifest(text);
	if (!parsed.ok) {
		return false;
	}
	out = std::move(parsed.manifest);
	if (newerFormat) {
		*newerFormat = parsed.newerVersion;
	}
	return true;
}

QList<ProjectEntry> ProjectLibrary::scan(const QString &folder)
{
	QList<ProjectEntry> entries;
	const QDir root(folder);
	if (folder.isEmpty() || !root.exists()) {
		return entries;
	}

	for (const QFileInfo &dirInfo : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		const QString dir = dirInfo.absoluteFilePath();
		if (!QFileInfo::exists(manifestPath(dir))) {
			continue;
		}
		ProjectEntry entry;
		entry.isProject = true;
		entry.path = dir;
		ProjectManifest manifest;
		if (readManifest(dir, manifest, &entry.newerFormat)) {
			entry.name = QString::fromStdString(manifest.name);
			entry.date = QDateTime::fromString(QString::fromStdString(manifest.createdAt), Qt::ISODate);
			entry.durationMs = manifestNetDurationMs(manifest);
			entry.status = manifest.status;
			if (manifest.program.present && !manifest.program.file.empty()) {
				entry.programFile = QDir(dir).filePath(QString::fromStdString(manifest.program.file));
			}
		} else {
			entry.unreadable = true;
			entry.status = ProjectStatus::Failed;
		}
		if (entry.name.isEmpty()) {
			entry.name = dirInfo.fileName();
		}
		if (!entry.date.isValid()) {
			entry.date = dirInfo.lastModified();
		}
		for (const QFileInfo &file : QDir(dir).entryInfoList(QDir::Files)) {
			entry.sizeBytes += static_cast<quint64>(file.size());
		}
		entries.append(entry);
	}

	for (const QFileInfo &file : root.entryInfoList(QDir::Files)) {
		if (!isRecordingFile(file.fileName().toStdString())) {
			continue;
		}
		ProjectEntry entry;
		entry.path = file.absoluteFilePath();
		entry.programFile = entry.path;
		entry.name = file.fileName();
		entry.date = file.lastModified();
		entry.sizeBytes = static_cast<quint64>(file.size());
		entries.append(entry);
	}

	std::sort(entries.begin(), entries.end(),
		  [](const ProjectEntry &a, const ProjectEntry &b) { return a.date > b.date; });
	return entries;
}

QStringList ProjectLibrary::recentNames(const QString &folder, int max)
{
	QStringList names;
	for (const ProjectEntry &entry : scan(folder)) {
		if (!entry.isProject || entry.unreadable || names.contains(entry.name)) {
			continue;
		}
		names.append(entry.name);
		if (names.size() >= max) {
			break;
		}
	}
	return names;
}

bool ProjectLibrary::rename(const QString &projectDir, const QString &newName)
{
	const QString trimmed = newName.trimmed();
	ProjectManifest manifest;
	bool newer = false;
	if (trimmed.isEmpty() || !readManifest(projectDir, manifest, &newer) || newer) {
		return false;
	}
	manifest.name = trimmed.toUtf8().toStdString();
	return writeFileAtomically(nativeUtf8Path(manifestPath(projectDir)), serializeManifest(manifest));
}

bool ProjectLibrary::recover(const QString &projectDir)
{
	ProjectManifest manifest;
	bool newer = false;
	if (!readManifest(projectDir, manifest, &newer) || newer || manifest.status != ProjectStatus::Recording) {
		return false;
	}
	qint64 fileElapsedMs = 0;
	const QFileInfo program(QDir(projectDir).filePath(QString::fromStdString(manifest.program.file)));
	const QDateTime created = QDateTime::fromString(QString::fromStdString(manifest.createdAt), Qt::ISODate);
	if (manifest.program.present && program.exists() && created.isValid()) {
		fileElapsedMs = std::max<qint64>(0, created.msecsTo(program.lastModified()));
	}
	recoverManifest(manifest, fileElapsedMs);
	const bool ok = writeFileAtomically(nativeUtf8Path(manifestPath(projectDir)), serializeManifest(manifest));
	blog(LOG_INFO, "[voltz] recuperar projeto (%s, %lld ms): %s", ok ? "ok" : "falhou",
	     static_cast<long long>(manifest.durationMs), qUtf8Printable(projectDir));
	return ok;
}

} // namespace voltz
```
(`blog` vem de `<obs.h>`, incluído por `ProjectRecorder.hpp` → `obs.hpp`.)

- [ ] **Step 3: Reescrever `frontend/voltz/shell/ProjectsPage.hpp`**

```cpp
#pragma once

#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace voltz {

class ProjectRecorder;

// Tela "Projetos" (spec §4.3): projetos (pastas com projeto.voltz) e vídeos soltos da pasta de gravações, mais
// recentes primeiro. Abrir, mostrar na pasta, renomear (nome do manifesto), recuperar gravação interrompida e mover
// para a lixeira (pasta inteira). Atualiza ao aparecer e quando um projeto começa/termina.
class ProjectsPage : public QWidget {
	Q_OBJECT

public:
	explicit ProjectsPage(ProjectRecorder *recorder, QWidget *parent = nullptr);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void refresh();
	QTreeWidgetItem *selectedItem() const;
	void updateButtons();
	void openSelected();
	void showSelectedInFolder();
	void renameSelected();
	void recoverSelected();
	void recoverAll();
	void trashSelected();

	ProjectRecorder *recorder_;
	QString folder_;
	QLabel *folderLabel_ = nullptr;
	QLabel *empty_ = nullptr;
	QFrame *recoverBanner_ = nullptr;
	QLabel *recoverText_ = nullptr;
	QTreeWidget *list_ = nullptr;
	QPushButton *openButton_ = nullptr;
	QPushButton *showButton_ = nullptr;
	QPushButton *renameButton_ = nullptr;
	QPushButton *recoverButton_ = nullptr;
	QPushButton *trashButton_ = nullptr;
};

} // namespace voltz
```

- [ ] **Step 4: Reescrever `frontend/voltz/shell/ProjectsPage.cpp`**

```cpp
#include "ProjectsPage.hpp"

#include "Icons.hpp"
#include "RecordingConfig.hpp"

#include <voltz/core/project-manifest.hpp>
#include <voltz/core/recording-files.hpp>
#include <voltz/core/time-format.hpp>
#include <voltz/project/ProjectLibrary.hpp>
#include <voltz/project/ProjectRecorder.hpp>

#include <OBSApp.hpp>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
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
constexpr int kProjectRole = Qt::UserRole + 2;
constexpr int kProgramRole = Qt::UserRole + 3;
constexpr int kActiveRole = Qt::UserRole + 4;
constexpr int kRecoverableRole = Qt::UserRole + 5;
constexpr int kNewerRole = Qt::UserRole + 6;

enum Column { ColName = 0, ColDate, ColDuration, ColSize, ColStatus };

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

QString statusText(const ProjectEntry &entry, bool active)
{
	if (!entry.isProject) {
		return QString();
	}
	if (active) {
		return QTStr("Voltz.Projects.Status.Recording");
	}
	if (entry.newerFormat) {
		return QStringLiteral("—");
	}
	switch (entry.status) {
	case ProjectStatus::Ok:
		return QTStr("Voltz.Projects.Status.Ok");
	case ProjectStatus::Recording:
		return QTStr("Voltz.Projects.Status.Recoverable");
	case ProjectStatus::Recovered:
		return QTStr("Voltz.Projects.Status.Recovered");
	case ProjectStatus::Failed:
		break;
	}
	return QTStr("Voltz.Projects.Status.Failed");
}

} // namespace

ProjectsPage::ProjectsPage(ProjectRecorder *recorder, QWidget *parent) : QWidget(parent), recorder_(recorder)
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

	recoverBanner_ = new QFrame(this);
	recoverBanner_->setObjectName("recoverBanner");
	recoverText_ = new QLabel(recoverBanner_);
	recoverText_->setWordWrap(true);
	auto *recoverAllButton = new QPushButton(QTStr("Voltz.Projects.RecoverAll"), recoverBanner_);
	recoverAllButton->setObjectName("primaryButton");
	auto *bannerLayout = new QHBoxLayout(recoverBanner_);
	bannerLayout->setContentsMargins(14, 10, 10, 10);
	bannerLayout->setSpacing(12);
	bannerLayout->addWidget(recoverText_, 1);
	bannerLayout->addWidget(recoverAllButton);
	recoverBanner_->hide();

	list_ = new QTreeWidget(this);
	list_->setObjectName("projectsList");
	list_->setColumnCount(5);
	list_->setHeaderLabels({QTStr("Voltz.Projects.Column.Name"), QTStr("Voltz.Projects.Column.Date"),
				QTStr("Voltz.Projects.Column.Duration"), QTStr("Voltz.Projects.Column.Size"),
				QTStr("Voltz.Projects.Column.Status")});
	list_->setRootIsDecorated(false);
	list_->setUniformRowHeights(true);
	list_->setSortingEnabled(true);
	list_->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);
	for (int column = ColDate; column <= ColStatus; column++) {
		list_->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
	}
	list_->headerItem()->setTextAlignment(ColDuration, Qt::AlignRight | Qt::AlignVCenter);
	list_->headerItem()->setTextAlignment(ColSize, Qt::AlignRight | Qt::AlignVCenter);

	empty_ = new QLabel(QTStr("Voltz.Projects.Empty"), this);
	empty_->setObjectName("muted");
	empty_->setAlignment(Qt::AlignCenter);

	openButton_ = new QPushButton(QTStr("Voltz.Projects.Open"), this);
	showButton_ = new QPushButton(QTStr("Voltz.Projects.ShowInFolder"), this);
	renameButton_ = new QPushButton(QTStr("Voltz.Projects.Rename"), this);
	recoverButton_ = new QPushButton(QTStr("Voltz.Projects.Recover"), this);
	trashButton_ = new QPushButton(QTStr("Voltz.Projects.Delete"), this);
	trashButton_->setIcon(phosphorIcon(QStringLiteral("trash"), palette::kText));

	auto *actions = new QHBoxLayout();
	actions->addWidget(openButton_);
	actions->addWidget(showButton_);
	actions->addWidget(renameButton_);
	actions->addWidget(recoverButton_);
	actions->addStretch(1);
	actions->addWidget(trashButton_);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(32, 28, 32, 28);
	root->setSpacing(10);
	root->addWidget(heading);
	root->addLayout(header);
	root->addWidget(recoverBanner_);
	root->addWidget(empty_, 1);
	root->addWidget(list_, 1);
	root->addLayout(actions);

	connect(refreshButton, &QPushButton::clicked, this, &ProjectsPage::refresh);
	connect(openFolder, &QPushButton::clicked, this,
		[this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(folder_)); });
	connect(openButton_, &QPushButton::clicked, this, &ProjectsPage::openSelected);
	connect(showButton_, &QPushButton::clicked, this, &ProjectsPage::showSelectedInFolder);
	connect(renameButton_, &QPushButton::clicked, this, &ProjectsPage::renameSelected);
	connect(recoverButton_, &QPushButton::clicked, this, &ProjectsPage::recoverSelected);
	connect(recoverAllButton, &QPushButton::clicked, this, &ProjectsPage::recoverAll);
	connect(trashButton_, &QPushButton::clicked, this, &ProjectsPage::trashSelected);
	connect(list_, &QTreeWidget::itemDoubleClicked, this, [this]() { openSelected(); });
	connect(list_, &QTreeWidget::itemSelectionChanged, this, &ProjectsPage::updateButtons);
	connect(recorder_, &ProjectRecorder::projectStarted, this, [this](const QString &, const QString &) {
		if (isVisible()) {
			refresh();
		}
	});
	connect(recorder_, &ProjectRecorder::projectFinished, this, [this](const QString &, const QString &) {
		if (isVisible()) {
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
	folder_ = RecordingConfig::outputFolder();
	folderLabel_->setText(QTStr("Voltz.Projects.Folder").arg(QDir::toNativeSeparators(folder_)));

	QTreeWidgetItem *previous = selectedItem();
	const QString selected = previous ? previous->data(ColName, kPathRole).toString() : QString();
	const QDir activeDir(recorder_->isActive() ? recorder_->currentProjectDir() : QString());
	const bool hasActive = recorder_->isActive();

	list_->setSortingEnabled(false);
	list_->clear();
	const char separator = QLocale().decimalPoint().at(0).toLatin1();
	int recoverable = 0;
	for (const ProjectEntry &entry : ProjectLibrary::scan(folder_)) {
		const bool active = hasActive && entry.isProject && QDir(entry.path) == activeDir;
		const bool canRecover =
			entry.isProject && !active && !entry.newerFormat && entry.status == ProjectStatus::Recording;
		auto *item = new SortableItem(list_);
		item->setText(ColName, entry.name);
		item->setIcon(ColName, phosphorIcon(entry.isProject ? QStringLiteral("folder-simple")
								    : QStringLiteral("film-strip"),
						    palette::kMuted));
		item->setData(ColName, kPathRole, entry.path);
		item->setData(ColName, kProjectRole, entry.isProject);
		item->setData(ColName, kProgramRole, entry.programFile);
		item->setData(ColName, kActiveRole, active);
		item->setData(ColName, kRecoverableRole, canRecover);
		item->setData(ColName, kNewerRole, entry.newerFormat);
		if (entry.newerFormat) {
			item->setToolTip(ColName, QTStr("Voltz.Projects.NewerFormat"));
		}
		item->setText(ColDate, QLocale().toString(entry.date.toLocalTime(), QLocale::ShortFormat));
		item->setData(ColDate, kSortRole, entry.date.toMSecsSinceEpoch());
		item->setText(ColDuration, entry.durationMs >= 0 ? QString::fromStdString(formatClock(entry.durationMs))
								 : QStringLiteral("—"));
		item->setData(ColDuration, kSortRole, entry.durationMs);
		item->setTextAlignment(ColDuration, Qt::AlignRight | Qt::AlignVCenter);
		item->setText(ColSize, QString::fromStdString(formatFileSize(entry.sizeBytes, separator)));
		item->setData(ColSize, kSortRole, static_cast<qlonglong>(entry.sizeBytes));
		item->setTextAlignment(ColSize, Qt::AlignRight | Qt::AlignVCenter);
		item->setText(ColStatus, statusText(entry, active));
		if (canRecover) {
			recoverable++;
			item->setIcon(ColStatus, phosphorIcon(QStringLiteral("warning-circle"), palette::kAccent));
		} else if (entry.isProject && !active && !entry.newerFormat && entry.status == ProjectStatus::Failed) {
			item->setIcon(ColStatus, phosphorIcon(QStringLiteral("warning-circle"), palette::kRec));
		}
		if (entry.path == selected) {
			list_->setCurrentItem(item);
		}
	}
	list_->setSortingEnabled(true);
	list_->sortByColumn(ColDate, Qt::DescendingOrder);

	const bool hasItems = list_->topLevelItemCount() > 0;
	empty_->setVisible(!hasItems);
	list_->setVisible(hasItems);
	recoverText_->setText(recoverable == 1 ? QTStr("Voltz.Projects.RecoverBanner.One")
					       : QTStr("Voltz.Projects.RecoverBanner.Many").arg(recoverable));
	recoverBanner_->setVisible(recoverable > 0);
	updateButtons();
}

QTreeWidgetItem *ProjectsPage::selectedItem() const
{
	const QList<QTreeWidgetItem *> items = list_->selectedItems();
	return items.isEmpty() ? nullptr : items.first();
}

void ProjectsPage::updateButtons()
{
	QTreeWidgetItem *item = selectedItem();
	const bool has = item != nullptr;
	const bool project = has && item->data(ColName, kProjectRole).toBool();
	const bool active = has && item->data(ColName, kActiveRole).toBool();
	const bool newer = has && item->data(ColName, kNewerRole).toBool();
	openButton_->setEnabled(has);
	showButton_->setEnabled(has);
	renameButton_->setEnabled(project && !active && !newer);
	recoverButton_->setEnabled(has && item->data(ColName, kRecoverableRole).toBool());
	trashButton_->setEnabled(has && !active);
}

void ProjectsPage::openSelected()
{
	QTreeWidgetItem *item = selectedItem();
	if (!item) {
		return;
	}
	const QString program = item->data(ColName, kProgramRole).toString();
	const QString target =
		!program.isEmpty() && QFileInfo::exists(program) ? program : item->data(ColName, kPathRole).toString();
	QDesktopServices::openUrl(QUrl::fromLocalFile(target));
}

void ProjectsPage::showSelectedInFolder()
{
	QTreeWidgetItem *item = selectedItem();
	if (!item) {
		return;
	}
	const QString path = item->data(ColName, kPathRole).toString();
	if (item->data(ColName, kProjectRole).toBool()) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
		return;
	}
#ifdef _WIN32
	QProcess::startDetached(QStringLiteral("explorer.exe"),
				{QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#else
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void ProjectsPage::renameSelected()
{
	QTreeWidgetItem *item = selectedItem();
	if (!item || !item->data(ColName, kProjectRole).toBool() || item->data(ColName, kActiveRole).toBool()) {
		return;
	}
	const QString current = item->text(ColName);
	bool ok = false;
	const QString name = QInputDialog::getText(this, QTStr("Voltz.Projects.Rename.Title"),
						   QTStr("Voltz.Projects.Rename.Label"), QLineEdit::Normal, current,
						   &ok)
				     .trimmed();
	if (!ok || name.isEmpty() || name == current) {
		return;
	}
	if (!ProjectLibrary::rename(item->data(ColName, kPathRole).toString(), name)) {
		QMessageBox::warning(this, QTStr("Voltz.Projects.Rename.Title"), QTStr("Voltz.Projects.RenameFailed"));
	}
	refresh();
}

void ProjectsPage::recoverSelected()
{
	QTreeWidgetItem *item = selectedItem();
	if (!item || !item->data(ColName, kRecoverableRole).toBool()) {
		return;
	}
	const QString name = item->text(ColName);
	if (!ProjectLibrary::recover(item->data(ColName, kPathRole).toString())) {
		QMessageBox::warning(this, QTStr("Voltz.Projects.Recover"),
				     QTStr("Voltz.Projects.RecoverFailed").arg(name.toHtmlEscaped()));
	}
	refresh();
}

void ProjectsPage::recoverAll()
{
	QStringList failed;
	for (int i = 0; i < list_->topLevelItemCount(); i++) {
		QTreeWidgetItem *item = list_->topLevelItem(i);
		if (!item->data(ColName, kRecoverableRole).toBool()) {
			continue;
		}
		if (!ProjectLibrary::recover(item->data(ColName, kPathRole).toString())) {
			failed.append(item->text(ColName));
		}
	}
	refresh();
	if (!failed.isEmpty()) {
		QMessageBox::warning(
			this, QTStr("Voltz.Projects.Recover"),
			QTStr("Voltz.Projects.RecoverFailed").arg(failed.join(QStringLiteral(", ")).toHtmlEscaped()));
	}
}

void ProjectsPage::trashSelected()
{
	QTreeWidgetItem *item = selectedItem();
	if (!item || item->data(ColName, kActiveRole).toBool()) {
		return;
	}
	const QString path = item->data(ColName, kPathRole).toString();
	const QString name = item->text(ColName).toHtmlEscaped();
	const QString question = item->data(ColName, kProjectRole).toBool()
					 ? QTStr("Voltz.Projects.DeleteProjectConfirm.Text").arg(name)
					 : QTStr("Voltz.Projects.DeleteConfirm.Text").arg(name);
	if (QMessageBox::question(this, QTStr("Voltz.Projects.DeleteConfirm.Title"), question) != QMessageBox::Yes) {
		return;
	}
	/* QFile::moveToTrash usa o IFileOperation no Windows e aceita pasta: o projeto vai inteiro para a lixeira. */
	if (!QFile::moveToTrash(path)) {
		QMessageBox::warning(this, QTStr("Voltz.Projects.DeleteConfirm.Title"),
				     QTStr("Voltz.Projects.DeleteFailed"));
	}
	refresh();
}

} // namespace voltz
```

- [ ] **Step 5: Casca, CMake, QSS e textos**

`frontend/voltz/shell/VoltzShell.cpp`: trocar `pages_->addWidget(new ProjectsPage(bridge_, pages_));` por
`pages_->addWidget(new ProjectsPage(recorder_, pages_));`.

`frontend/cmake/feature-voltz.cmake`: antes de `voltz/project/ProjectRecorder.cpp` acrescentar
```cmake
    voltz/project/ProjectLibrary.cpp
    voltz/project/ProjectLibrary.hpp
```

`frontend/voltz/shell/voltz-shell.qss`: trocar o seletor `#voltzShell QFrame#welcomeBanner {` por
`#voltzShell QFrame#welcomeBanner,\n#voltzShell QFrame#recoverBanner {` (mesmo visual lima da faixa de boas-vindas).

Textos — acrescentar ao fim de `frontend/data/locale/voltz-pt-BR.ini`:
```ini
Voltz.Projects.Column.Duration="Duração"
Voltz.Projects.Column.Status="Status"
Voltz.Projects.Status.Ok="OK"
Voltz.Projects.Status.Recording="Gravando"
Voltz.Projects.Status.Recoverable="Recuperável"
Voltz.Projects.Status.Recovered="Recuperado"
Voltz.Projects.Status.Failed="Falhou"
Voltz.Projects.Rename="Renomear"
Voltz.Projects.Rename.Title="Renomear projeto"
Voltz.Projects.Rename.Label="Nome do projeto:"
Voltz.Projects.RenameFailed="Não foi possível renomear o projeto."
Voltz.Projects.Recover="Recuperar gravação"
Voltz.Projects.RecoverAll="Recuperar gravações"
Voltz.Projects.RecoverBanner.One="Uma gravação foi interrompida (o app fechou enquanto gravava) e pode ser recuperada."
Voltz.Projects.RecoverBanner.Many="%1 gravações foram interrompidas (o app fechou enquanto gravava) e podem ser recuperadas."
Voltz.Projects.RecoverFailed="Não foi possível recuperar: %1"
Voltz.Projects.DeleteProjectConfirm.Text="Mover o projeto \"%1\" (a pasta inteira) para a lixeira?"
Voltz.Projects.NewerFormat="Criado por uma versão mais nova do Voltz Studio: só leitura."
```
e ao fim de `frontend/data/locale/voltz-en-US.ini`:
```ini
Voltz.Projects.Column.Duration="Duration"
Voltz.Projects.Column.Status="Status"
Voltz.Projects.Status.Ok="OK"
Voltz.Projects.Status.Recording="Recording"
Voltz.Projects.Status.Recoverable="Recoverable"
Voltz.Projects.Status.Recovered="Recovered"
Voltz.Projects.Status.Failed="Failed"
Voltz.Projects.Rename="Rename"
Voltz.Projects.Rename.Title="Rename project"
Voltz.Projects.Rename.Label="Project name:"
Voltz.Projects.RenameFailed="Could not rename the project."
Voltz.Projects.Recover="Recover recording"
Voltz.Projects.RecoverAll="Recover recordings"
Voltz.Projects.RecoverBanner.One="A recording was interrupted (the app closed while recording) and can be recovered."
Voltz.Projects.RecoverBanner.Many="%1 recordings were interrupted (the app closed while recording) and can be recovered."
Voltz.Projects.RecoverFailed="Could not recover: %1"
Voltz.Projects.DeleteProjectConfirm.Text="Move the project \"%1\" (the whole folder) to the trash?"
Voltz.Projects.NewerFormat="Created by a newer version of Voltz Studio: read-only."
```
Paridade dos locales → sem saída.

- [ ] **Step 6: CONFIGURAR + COMPILAR APP (segundo plano)**

Expected: `exit=0`.

- [ ] **Step 7: Verificação visual + recuperação**

Bloco de teste (salvo **com BOM**: usa "Recuperar gravações"), no `try`: **Preparo de gravação**; grave um projeto
real de 3 s (`StartRecord`, `Start-Sleep 3`, `StopRecord`, `Start-Sleep 3`); depois crie as peças de teste em `$out`:
```powershell
$utf8 = New-Object Text.UTF8Encoding $false
$broken = Join-Path $out "2026-01-02 10h00 - Interrompido"; New-Item -ItemType Directory -Force $broken | Out-Null
[IO.File]::WriteAllText((Join-Path $broken "projeto.voltz"), '{"formato":"voltz-projeto","versao":1,"app":"Voltz Studio 1.0.0","motor":"libobs 32.2.2","nome":"Interrompido","criado_em":"2026-01-02T10:00:00-03:00","status":"gravando","relogio":"obs_ns","t0_ns":1,"duracao_ms":30000,"pausas":[],"arquivos":{"programa":{"arquivo":"programa.mp4","largura":1920,"altura":1080,"fps":60,"offset_ms":0}},"eventos":[]}', $utf8)
$fake = Join-Path $broken "programa.mp4"; [IO.File]::WriteAllBytes($fake, (New-Object byte[] 2048))
(Get-Item -LiteralPath $fake).LastWriteTime = [DateTime]::Parse("2026-01-02T10:00:42-03:00")
$future = Join-Path $out "2026-01-03 11h00 - Futuro"; New-Item -ItemType Directory -Force $future | Out-Null
$futureJson = '{"formato":"voltz-projeto","versao":2,"nome":"Futuro","criado_em":"2026-01-03T11:00:00-03:00","status":"gravando","duracao_ms":1000,"arquivos":{"tela":{"arquivo":"tela.mkv"}}}'
[IO.File]::WriteAllText((Join-Path $future "projeto.voltz"), $futureJson, $utf8)
[IO.File]::WriteAllBytes((Join-Path $out "solto.mp4"), (New-Object byte[] 1024))
[void](Uia "Projetos"); Start-Sleep 2; Shot $shell "t6-projetos"
[void](Uia "Recuperar gravações"); Start-Sleep 2; Shot $shell "t6-recuperado"
Read-Json (Join-Path $broken "projeto.voltz") | ConvertTo-Json -Depth 5 | Out-Host
"futuro intacto: " + ((Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $future "projeto.voltz")) -eq $futureJson)
```
Critérios:
1. `t6-projetos`: 4 linhas por data decrescente — no topo (hoje) `solto.mp4` (sem status, duração `—`) e o projeto
   real ("Gravação", duração ~00:00:03, status `OK`); depois "Futuro" (03/01, status `—`) e "Interrompido" (02/01,
   status `Recuperável` com ícone lima); faixa lima "Uma gravação foi interrompida…" com o botão "Recuperar gravações"; colunas Nome/Data/Duração/
   Tamanho/Status legíveis no tema da casca.
2. `t6-recuperado`: faixa sumiu; "Interrompido" com status `Recuperado` e duração `00:00:42`.
3. Manifesto de "Interrompido": `status: recuperado`, `duracao_ms` entre 41000 e 43000, último evento
   `recuperado`; campos originais (`app`, `motor`, `arquivos.programa`) preservados.
4. `futuro intacto: True` (versão mais nova não é tocada).
5. Log com `[voltz] recuperar projeto (ok, 42000 ms)` (± 1000).
Descreva as capturas no relatório. **Testes manuais** (diálogos modais não são automatizados): Renomear (o nome
muda na lista e no `projeto.voltz`; a pasta não muda) e Mover para a lixeira de um projeto (a pasta inteira vai para a
Lixeira). Se o `QFile::moveToTrash` falhar para pasta, aparece "Não foi possível mover…" — registre no backlog.

- [ ] **Step 8: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/project/ProjectLibrary.* frontend/voltz/shell/ProjectsPage.* frontend/voltz/shell/VoltzShell.cpp
```
COMPILAR APP (segundo plano, `exit=0`), então:
```bash
git add frontend/voltz/project/ProjectLibrary.* frontend/voltz/shell/ProjectsPage.* frontend/voltz/shell/VoltzShell.cpp frontend/voltz/shell/voltz-shell.qss frontend/cmake/feature-voltz.cmake frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): tela Projetos lista projetos, renomeia e recupera gravacoes interrompidas

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Barra inferior da tela Gravar — nome do projeto, ⚑ Marcador, espaço em disco e avisos

**Files:**
- Modify (reescrita): `frontend/voltz/shell/RecordPage.hpp`, `frontend/voltz/shell/RecordPage.cpp`
- Modify: `frontend/voltz/shell/ErrorBanner.hpp`, `frontend/voltz/shell/ErrorBanner.cpp`, `frontend/voltz/shell/Icons.hpp`
- Modify: `frontend/voltz/shell/VoltzShell.cpp`, `frontend/voltz/shell/voltz-shell.qss`, `frontend/voltz/shell/voltz-shell.qrc`
- Create: `frontend/voltz/shell/icons/regular/flag.svg`, `icons/fill/flag-fill.svg`, `icons/regular/hard-drives.svg`,
  `icons/fill/hard-drives-fill.svg`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`

Barra (spec §4.2), da esquerda para a direita: **nome do projeto** (combo editável — o texto é o nome do próximo
projeto, salvo em `[Voltz] NextProjectName`; a lista traz os 8 nomes recentes; travado durante a gravação, mostrando o
projeto atual) · ⚑ **Marcador** (habilitado só gravando e sem pausa; ao marcar, aparece "Marcador N em 00:01:23" por
2,5 s ao lado do tempo) · Pausar · Gravar/Parar + tempo · **espaço** ("~111 MB/min · 42 GB livres"; estimativa antes
de gravar, medida real depois de 10 s). Faixas na prévia: **âmbar** "Pouco espaço em disco" abaixo de 5 GB (some ao
fechar até o nível voltar a OK) e **vermelha** "Gravação parada: disco quase cheio" quando o `ProjectRecorder` para
por disco (< 1 GB). A checagem roda a cada 30 s parado e 5 s gravando (`QStorageInfo`, barato).

- [ ] **Step 1: Ícones Phosphor 2.1.1 (oficiais, versão exata)**

```bash
DEST=frontend/voltz/shell/icons
for n in flag hard-drives; do
  curl -fsSL "https://unpkg.com/@phosphor-icons/core@2.1.1/assets/regular/$n.svg" -o "$DEST/regular/$n.svg"
  curl -fsSL "https://unpkg.com/@phosphor-icons/core@2.1.1/assets/fill/$n-fill.svg" -o "$DEST/fill/$n-fill.svg"
done
head -c 80 "$DEST/regular/flag.svg"; echo
```
Expected: começa com `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" fill="currentColor">`.
No `voltz-shell.qrc`, acrescentar (ordem alfabética de cada bloco): `<file>icons/fill/flag-fill.svg</file>` (depois
de `faders-fill`), `<file>icons/fill/hard-drives-fill.svg</file>` (depois de `gear-six-fill`),
`<file>icons/regular/flag.svg</file>` (depois de `film-strip`), `<file>icons/regular/hard-drives.svg</file>` (depois
de `gear-six`). Conferir: `grep -c "<file>" frontend/voltz/shell/voltz-shell.qrc` → `82` (78 + 4).

- [ ] **Step 2: Faixa de aviso (âmbar) no `ErrorBanner` e cor no `Icons.hpp`**

`frontend/voltz/shell/Icons.hpp`: no `namespace palette`, depois de `kRec`, acrescentar
`inline const QColor kWarn(0xF5, 0xC8, 0x3C);` (mesma cor do `warning-circle-warn.svg` de `tools/voltz-qss-icons.sh`).

`frontend/voltz/shell/ErrorBanner.hpp` — substituir a classe por:
```cpp
// Faixa dispensável com título + mensagem (aceita HTML simples, como os textos do OBS). Vermelha (erro) por padrão;
// setWarning(true) a deixa âmbar (aviso).
class ErrorBanner : public QFrame {
	Q_OBJECT

public:
	explicit ErrorBanner(QWidget *parent = nullptr);

	void showError(const QString &title, const QString &message);
	void setWarning(bool warning);

signals:
	// O usuário fechou a faixa pelo X.
	void dismissed();

private:
	QLabel *icon_ = nullptr;
	QLabel *title_ = nullptr;
	QLabel *message_ = nullptr;
};
```

`frontend/voltz/shell/ErrorBanner.cpp`:
- acrescentar `#include <QStyle>`;
- trocar `auto *icon = new QLabel(this);` por `icon_ = new QLabel(this);` e as duas linhas seguintes que usam `icon`
  por `icon_` (`icon_->setPixmap(...)`, `icon_->setAlignment(Qt::AlignTop);`), e `layout->addWidget(icon);` por
  `layout->addWidget(icon_);`;
- trocar `connect(close, &QToolButton::clicked, this, &QWidget::hide);` por
  ```cpp
  	connect(close, &QToolButton::clicked, this, [this]() {
  		hide();
  		emit dismissed();
  	});
  ```
- acrescentar no fim do namespace:
  ```cpp
  void ErrorBanner::setWarning(bool warning)
  {
  	setProperty("warning", warning);
  	icon_->setPixmap(phosphorIcon(QStringLiteral("warning-circle"), warning ? palette::kWarn : palette::kRec,
  				      IconWeight::Fill)
  				 .pixmap(20, 20));
  	style()->unpolish(this);
  	style()->polish(this);
  }
  ```

- [ ] **Step 3: Reescrever `frontend/voltz/shell/RecordPage.hpp`**

```cpp
#pragma once

#include <voltz/core/recording-clock.hpp>

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

namespace voltz {

class ErrorBanner;
class FrontendBridge;
class ProjectRecorder;
class ScenesPanel;
class VoltzPreview;

// Tela "Gravar" (layout H2): cenas | prévia | coluna lateral (fontes + áudio). Barra inferior (spec §4.2): nome do
// projeto · Marcador · Pausar · Gravar + tempo · estimativa de espaço. Faixas na prévia: erro de gravação e disco.
class RecordPage : public QWidget {
	Q_OBJECT

public:
	RecordPage(FrontendBridge *bridge, ProjectRecorder *recorder, QWidget *parent = nullptr);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void updateRecordingUi(bool active, bool paused);
	void showRecordingTransition(bool stopping);
	void showRecordingError(int code, const QString &lastError);
	void tickClock();
	void showMarkerToast(int number);
	void reloadProjectNames();
	void commitProjectName();
	void refreshDiskInfo();

	FrontendBridge *bridge_;
	ProjectRecorder *recorder_;
	ScenesPanel *scenes_ = nullptr;
	VoltzPreview *preview_ = nullptr;
	ErrorBanner *errorBanner_ = nullptr;
	ErrorBanner *diskBanner_ = nullptr;
	QComboBox *projectName_ = nullptr;
	QPushButton *markerButton_ = nullptr;
	QPushButton *pauseButton_ = nullptr;
	QPushButton *recordButton_ = nullptr;
	QLabel *timeLabel_ = nullptr;
	QLabel *markerToast_ = nullptr;
	QLabel *spaceLabel_ = nullptr;

	QTimer ticker_;
	QTimer diskTimer_;
	QTimer markerToastTimer_;
	QElapsedTimer monotonic_;
	RecordingClock recordingClock_;
	bool pendingStart_ = false;
	bool diskBannerDismissed_ = false;
};

} // namespace voltz
```

- [ ] **Step 4: Reescrever `frontend/voltz/shell/RecordPage.cpp`**

```cpp
#include "RecordPage.hpp"

#include "AudioPanel.hpp"
#include "ErrorBanner.hpp"
#include "FrontendBridge.hpp"
#include "Icons.hpp"
#include "RecordingConfig.hpp"
#include "ScenesPanel.hpp"
#include "SourcesPanel.hpp"
#include "VoltzPreview.hpp"

#include <voltz/core/disk-space.hpp>
#include <voltz/core/recording-error.hpp>
#include <voltz/core/time-format.hpp>
#include <voltz/project/ProjectLibrary.hpp>
#include <voltz/project/ProjectRecorder.hpp>

#include <OBSApp.hpp> // QTStr

#include <QComboBox>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

#include "moc_RecordPage.cpp"

namespace voltz {

namespace {

constexpr int kDiskIdleMs = 30000;
constexpr int kDiskRecordingMs = 5000;
constexpr int kMarkerToastMs = 2500;

char decimalSeparator()
{
	return QLocale().decimalPoint().at(0).toLatin1();
}

QString freeSpaceText(quint64 bytes)
{
	return QString::fromStdString(formatFreeSpace(bytes, decimalSeparator()));
}

} // namespace

RecordPage::RecordPage(FrontendBridge *bridge, ProjectRecorder *recorder, QWidget *parent)
	: QWidget(parent),
	  bridge_(bridge),
	  recorder_(recorder)
{
	monotonic_.start();

	scenes_ = new ScenesPanel(bridge_, this);
	scenes_->setFixedWidth(200);
	preview_ = new VoltzPreview(this);

	auto *centerColumn = new QVBoxLayout();
	centerColumn->setContentsMargins(0, 0, 0, 0);
	centerColumn->setSpacing(12);
	errorBanner_ = new ErrorBanner(this);
	diskBanner_ = new ErrorBanner(this);
	diskBanner_->setWarning(true);
	centerColumn->addWidget(errorBanner_);
	centerColumn->addWidget(diskBanner_);
	centerColumn->addWidget(preview_, 1);

	auto *sideHolder = new QWidget(this);
	sideHolder->setFixedWidth(280);
	auto *sideColumn = new QVBoxLayout(sideHolder);
	sideColumn->setContentsMargins(0, 0, 0, 0);
	sideColumn->setSpacing(12);
	sideColumn->addWidget(new SourcesPanel(bridge_, sideHolder), 3);
	sideColumn->addWidget(new AudioPanel(sideHolder), 2);

	auto *body = new QHBoxLayout();
	body->setContentsMargins(12, 12, 12, 12);
	body->setSpacing(12);
	body->addWidget(scenes_);
	body->addLayout(centerColumn, 1);
	body->addWidget(sideHolder);

	projectName_ = new QComboBox(this);
	projectName_->setObjectName("projectName");
	projectName_->setEditable(true);
	projectName_->setInsertPolicy(QComboBox::NoInsert);
	projectName_->setMinimumWidth(220);
	projectName_->setMaxVisibleItems(8);
	projectName_->setToolTip(QTStr("Voltz.Record.ProjectName.Hint"));
	projectName_->setAccessibleName(QTStr("Voltz.Record.ProjectName"));
	projectName_->lineEdit()->setPlaceholderText(QTStr("Voltz.Project.DefaultName"));

	markerButton_ = new QPushButton(QTStr("Voltz.Record.Marker"), this);
	markerButton_->setIcon(phosphorIcon(QStringLiteral("flag"), palette::kText));
	markerButton_->setIconSize(QSize(16, 16));
	markerButton_->setToolTip(QTStr("Voltz.Record.Marker.Hint"));
	pauseButton_ = new QPushButton(this);
	pauseButton_->setIconSize(QSize(16, 16));
	recordButton_ = new QPushButton(this);
	recordButton_->setObjectName("recordButton");
	recordButton_->setIconSize(QSize(16, 16));
	timeLabel_ = new QLabel(QString::fromStdString(formatClock(0)), this);
	timeLabel_->setObjectName("timeLabel");
	markerToast_ = new QLabel(this);
	markerToast_->setObjectName("markerToast");
	markerToast_->setMinimumWidth(180);
	auto *spaceIcon = new QLabel(this);
	spaceIcon->setPixmap(phosphorIcon(QStringLiteral("hard-drives"), palette::kMuted).pixmap(16, 16));
	spaceLabel_ = new QLabel(this);
	spaceLabel_->setObjectName("spaceLabel");

	auto *bottomBar = new QFrame(this);
	bottomBar->setObjectName("bottomBar");
	bottomBar->setFixedHeight(60);
	auto *bar = new QHBoxLayout(bottomBar);
	bar->setContentsMargins(16, 0, 16, 0);
	bar->setSpacing(12);
	bar->addWidget(projectName_);
	bar->addStretch(1);
	bar->addWidget(markerButton_);
	bar->addWidget(pauseButton_);
	bar->addWidget(recordButton_);
	bar->addWidget(timeLabel_);
	bar->addWidget(markerToast_);
	bar->addStretch(1);
	bar->addWidget(spaceIcon);
	bar->addWidget(spaceLabel_);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);
	root->addLayout(body, 1);
	root->addWidget(bottomBar);

	ticker_.setInterval(250);
	connect(&ticker_, &QTimer::timeout, this, &RecordPage::tickClock);
	diskTimer_.setInterval(kDiskIdleMs);
	connect(&diskTimer_, &QTimer::timeout, this, &RecordPage::refreshDiskInfo);
	diskTimer_.start();
	markerToastTimer_.setSingleShot(true);
	markerToastTimer_.setInterval(kMarkerToastMs);
	connect(&markerToastTimer_, &QTimer::timeout, markerToast_, &QLabel::clear);

	connect(recordButton_, &QPushButton::clicked, this, [this]() {
		if (bridge_->isRecording()) {
			bridge_->stopRecording();
		} else {
			commitProjectName();
			bridge_->startRecording();
		}
	});
	connect(pauseButton_, &QPushButton::clicked, this,
		[this]() { bridge_->setRecordingPaused(!bridge_->isRecordingPaused()); });
	connect(markerButton_, &QPushButton::clicked, this, [this]() { recorder_->addMarker(); });
	connect(projectName_->lineEdit(), &QLineEdit::editingFinished, this, &RecordPage::commitProjectName);
	connect(projectName_, &QComboBox::activated, this, [this](int) { commitProjectName(); });

	connect(bridge_, &FrontendBridge::recordingStateChanged, this, &RecordPage::updateRecordingUi);
	connect(bridge_, &FrontendBridge::recordingTransition, this, &RecordPage::showRecordingTransition);
	connect(bridge_, &FrontendBridge::recordingFailed, this, &RecordPage::showRecordingError);

	connect(recorder_, &ProjectRecorder::markerAdded, this, [this](int number, qint64) { showMarkerToast(number); });
	connect(recorder_, &ProjectRecorder::projectStarted, this, [this](const QString &, const QString &name) {
		const QSignalBlocker blocker(projectName_);
		projectName_->setEditText(name);
	});
	connect(recorder_, &ProjectRecorder::projectFinished, this, [this](const QString &, const QString &) {
		reloadProjectNames();
		refreshDiskInfo();
	});
	connect(recorder_, &ProjectRecorder::stoppedForLowDisk, this, [this](quint64 freeBytes) {
		errorBanner_->showError(QTStr("Voltz.Disk.Stopped.Title"),
					QTStr("Voltz.Disk.Stopped.Text").arg(freeSpaceText(freeBytes)));
	});
	connect(recorder_, &ProjectRecorder::manifestWriteFailed, this, [this](const QString &backupPath) {
		errorBanner_->showError(
			QTStr("Voltz.Project.ManifestFailed.Title"),
			QTStr("Voltz.Project.ManifestFailed.Text").arg(QDir::toNativeSeparators(backupPath).toHtmlEscaped()));
	});
	connect(diskBanner_, &ErrorBanner::dismissed, this, [this]() { diskBannerDismissed_ = true; });

	reloadProjectNames();
	updateRecordingUi(bridge_->isRecording(), bridge_->isRecordingPaused());
	refreshDiskInfo();
}

void RecordPage::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	if (!recorder_->isActive()) {
		reloadProjectNames();
	}
	refreshDiskInfo();
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
	pauseButton_->setIcon(active && paused
				      ? phosphorIcon(QStringLiteral("play"), palette::kText, IconWeight::Fill)
				      : phosphorIcon(QStringLiteral("pause"), palette::kText, IconWeight::Fill));

	markerButton_->setEnabled(active && !paused && recorder_->isActive());
	projectName_->setEnabled(!active);
	diskTimer_.setInterval(active ? kDiskRecordingMs : kDiskIdleMs);

	tickClock();
}

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

void RecordPage::showRecordingTransition(bool stopping)
{
	/* Enquanto inicia/finaliza, um segundo clique chamaria StopRecording com a saída em
	 * "stopping" (parada forçada, pode truncar o arquivo): botões travados até STARTED/STOPPED. */
	recordButton_->setEnabled(false);
	pauseButton_->setEnabled(false);
	markerButton_->setEnabled(false);
	recordButton_->setText(stopping ? QTStr("Voltz.Record.Stopping") : QTStr("Voltz.Record.Starting"));

	if (!stopping) {
		/* Falha síncrona ao iniciar (OBSBasic::StartRecording emite STARTING, mas o start da saída
		 * falha e não vem STARTED nem STOPPED): sem isto o botão ficaria preso em "Iniciando…". */
		errorBanner_->hide();
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

void RecordPage::showMarkerToast(int number)
{
	/* Mostra o tempo do cronômetro (líquido, como o usuário vê), não o t_ms bruto do marcadores.json. */
	const QString clock = QString::fromStdString(formatClock(recordingClock_.elapsedMs(monotonic_.elapsed())));
	markerToast_->setText(QTStr("Voltz.Record.MarkerAdded").arg(QTStr("Voltz.Marker.Name").arg(number), clock));
	markerToastTimer_.start();
}

void RecordPage::reloadProjectNames()
{
	const QString current = recorder_->isActive() ? recorder_->currentProjectName() : recorder_->nextProjectName();
	const QSignalBlocker blocker(projectName_);
	projectName_->clear();
	projectName_->addItems(ProjectLibrary::recentNames(RecordingConfig::outputFolder()));
	projectName_->setEditText(current);
}

void RecordPage::commitProjectName()
{
	if (recorder_->isActive()) {
		return;
	}
	recorder_->setNextProjectName(projectName_->currentText());
	if (projectName_->currentText().trimmed().isEmpty()) {
		const QSignalBlocker blocker(projectName_);
		projectName_->setEditText(recorder_->nextProjectName());
	}
}

void RecordPage::refreshDiskInfo()
{
	const QString folder = RecordingConfig::outputFolder();
	uint64_t rate = recorder_->measuredBytesPerMinute();
	if (rate == 0) {
		rate = RecordingConfig::estimatedBytesPerMinute();
	}
	const QString rateText = QString::fromStdString(formatRate(rate, decimalSeparator()));
	quint64 free = 0;
	const bool known = ProjectRecorder::freeBytes(folder, free);
	spaceLabel_->setText(known ? QTStr("Voltz.Record.Space").arg(rateText, freeSpaceText(free)) : rateText);
	spaceLabel_->setToolTip(QTStr("Voltz.Record.Space.Hint").arg(QDir::toNativeSeparators(folder)));

	const DiskLevel level = known ? diskLevel(free) : DiskLevel::Ok;
	if (level == DiskLevel::Ok) {
		diskBannerDismissed_ = false;
		diskBanner_->hide();
	} else if (!diskBannerDismissed_) {
		diskBanner_->showError(QTStr("Voltz.Disk.Low.Title"), QTStr("Voltz.Disk.Low.Text").arg(freeSpaceText(free)));
	}
}

} // namespace voltz
```
(`commitProjectName()` também no clique de Gravar: o texto digitado vale mesmo sem o usuário sair do campo.)

- [ ] **Step 5: Casca, QSS e textos**

`frontend/voltz/shell/VoltzShell.cpp`: trocar `pages_->addWidget(new RecordPage(bridge_, pages_));` por
`pages_->addWidget(new RecordPage(bridge_, recorder_, pages_));`.

`frontend/voltz/shell/voltz-shell.qss` — acrescentar depois da regra `#voltzShell QLabel#timeLabel { ... }`:
```css
#voltzShell QLabel#markerToast {
	color: #D4F53C;
	font-weight: 600;
}

#voltzShell QLabel#spaceLabel {
	color: #82828C;
	font-size: 12px;
}

#voltzShell QComboBox#projectName QLineEdit {
	background: transparent;
	border: none;
	padding: 0;
	color: #F2F2F3;
}
```
e depois da regra `#voltzShell QLabel#errorTitle { ... }`:
```css
#voltzShell QFrame#errorBanner[warning="true"] {
	background: rgba(245, 200, 60, 26);
	border: 1px solid rgba(245, 200, 60, 102);
}
```

Textos — ao fim de `frontend/data/locale/voltz-pt-BR.ini`:
```ini
Voltz.Record.ProjectName="Nome do projeto"
Voltz.Record.ProjectName.Hint="Nome do próximo projeto. Cada gravação vira uma pasta com data, hora e este nome."
Voltz.Record.Marker="Marcador"
Voltz.Record.Marker.Hint="Marca este momento (vira capítulo no vídeo). Atalho padrão: Ctrl+Shift+M"
Voltz.Record.MarkerAdded="%1 em %2"
Voltz.Record.Space="%1 · %2 livres"
Voltz.Record.Space.Hint="Estimativa do tamanho da gravação e espaço livre em %1"
Voltz.Disk.Low.Title="Pouco espaço em disco"
Voltz.Disk.Low.Text="Restam %1 na pasta de gravações. Abaixo de 1 GB a gravação é parada automaticamente."
Voltz.Disk.Stopped.Title="Gravação parada: disco quase cheio"
Voltz.Disk.Stopped.Text="Restavam %1. O projeto foi finalizado e salvo."
Voltz.Project.ManifestFailed.Title="Não foi possível salvar o projeto"
Voltz.Project.ManifestFailed.Text="O arquivo projeto.voltz não pôde ser gravado na pasta do projeto. Uma cópia foi guardada em %1."
```
e ao fim de `frontend/data/locale/voltz-en-US.ini`:
```ini
Voltz.Record.ProjectName="Project name"
Voltz.Record.ProjectName.Hint="Name of the next project. Each recording becomes a folder with the date, time and this name."
Voltz.Record.Marker="Marker"
Voltz.Record.Marker.Hint="Marks this moment (becomes a chapter in the video). Default shortcut: Ctrl+Shift+M"
Voltz.Record.MarkerAdded="%1 at %2"
Voltz.Record.Space="%1 · %2 free"
Voltz.Record.Space.Hint="Estimated recording size and free space in %1"
Voltz.Disk.Low.Title="Low disk space"
Voltz.Disk.Low.Text="%1 left in the recordings folder. Below 1 GB the recording stops automatically."
Voltz.Disk.Stopped.Title="Recording stopped: disk almost full"
Voltz.Disk.Stopped.Text="Only %1 was left. The project was finalized and saved."
Voltz.Project.ManifestFailed.Title="Could not save the project"
Voltz.Project.ManifestFailed.Text="The projeto.voltz file could not be written to the project folder. A copy was saved to %1."
```
Paridade dos locales → sem saída.

- [ ] **Step 6: CONFIGURAR + COMPILAR APP (segundo plano)**

Expected: `exit=0` (arquivos novos no `.qrc` ⇒ CONFIGURAR antes).

- [ ] **Step 7: Verificação visual**

Rodada 1 — bloco de teste com
`$userIni = "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[BasicWindow]`r`nHideOBSWindowsFromCapture=false`r`n[Voltz]`r`nNextProjectName=Aula 03`r`n"`,
no `try`: **Preparo de gravação**, depois
```powershell
Start-Sleep 1; Shot $shell "t7-parado"
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 3
[void](Uia "Marcador"); Start-Sleep 1; Shot $shell "t7-marcador"
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 4; Shot $shell "t7-depois"
"pasta: " + (Project-Dirs).Name
```
Critérios: `t7-parado` — barra com o combo "Aula 03" à esquerda, ⚑ Marcador desabilitado, Pausar desabilitado,
Gravar, `00:00:00`, ícone de disco + "~NN MB/min · NN GB livres" à direita (valores coerentes com o disco), sem faixa
âmbar (disco real com folga) e prévia preta (sem câmera); `t7-marcador` — Marcador habilitado, "Marcador 1 em
00:00:0X" em lima ao lado do tempo, combo desabilitado; `t7-depois` — barra de volta ao repouso e o combo com "Aula
03"; `pasta: AAAA-MM-DD HHhMM · Aula 03`. (Para ver a lista do combo, `& powershell -File $uia -ProcessId $proc.Id
-List | Select-String "Aula 03"` deve achar o item.)

Rodada 2 — `$fakeFree = "4294967296"` (4 GB), user.ini padrão: **Preparo de gravação** (o "Pular" leva à tela
Gravar), `Start-Sleep 2; Shot $shell "t7-disco-baixo"`.
Critério: faixa **âmbar** "Pouco espaço em disco — Restam 4,0 GB na pasta de gravações…" acima da prévia e
"… · 4,0 GB livres" na barra.

Rodada 3 — `$fakeFree = "536870912"` (0,5 GB): **Preparo de gravação**, `StartRecord`, `Start-Sleep 8`,
`Shot $shell "t7-disco-parou"`. Critério: faixa **vermelha** "Gravação parada: disco quase cheio — Restavam 512 MB…"
e faixa âmbar; botão de volta a "Gravar"; manifesto do projeto com `disco_cheio` e `status: ok`.
Descreva as 5 capturas no relatório.

- [ ] **Step 8: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/shell/RecordPage.* frontend/voltz/shell/ErrorBanner.* frontend/voltz/shell/Icons.hpp frontend/voltz/shell/VoltzShell.cpp
```
COMPILAR APP (segundo plano, `exit=0`), então:
```bash
git add frontend/voltz/shell frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): barra inferior com projeto, marcador, espaco em disco e avisos

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Teste de integração `tools/voltz-smoke-project.ps1` e smokes atualizados

**Files:**
- Create: `tools/voltz-smoke-project.ps1`
- Modify: `tools/voltz-smoke-record.ps1`, `tools/voltz-smoke.ps1`

O smoke novo grava ~12 s pela obs-websocket com troca de cena, **dois** marcadores (botão "Marcador" por UI
Automation e atalho `VoltzStudio.AddMarker` por `TriggerHotkeyByName`) e uma pausa, fecha o app e valida a pasta do
projeto. Não move o mouse: o `cursor.jsonl` é validado pelo formato e pelos limites (a posição inicial garante ≥ 1
linha; cliques/rolagem estão cobertos pelos testes do núcleo). Áudio global mutado. Guarda e restaura a config
portátil sozinho e recusa rodar se já existir um backup (pode ser a config real de uma execução que caiu). Script só
com ASCII (sem BOM) — os nomes de UI usados ("Pular", "Marcador") não têm acento.

Os smokes antigos passam a gravar dentro de uma pasta de projeto (padrão `Videos\Voltz Studio`): o
`voltz-smoke-record.ps1` precisa ler o log em UTF-8 (caminho com "·" e acento), conferir que o manifesto foi fechado
ao sair gravando e apagar a **pasta do projeto** que ele mesmo criou; os dois removem `Videos\Voltz Studio` se a
execução a criou e ela ficou vazia.

- [ ] **Step 1: Criar `tools/voltz-smoke-project.ps1`**

```powershell
<#
  Teste de integracao da gravacao em projeto (Plano 3A).
  Abre o Voltz Studio em modo portatil com config limpa (a config portatil existente e guardada e restaurada),
  aplica os padroes Voltz ("Pular"), aponta a pasta de gravacoes para -OutDir e, via obs-websocket + UI Automation,
  grava ~12 s com troca de cena, 2 marcadores (botao "Marcador" e atalho VoltzStudio.AddMarker) e uma pausa.
  Fecha o app e valida a pasta do projeto: programa.mp4, projeto.voltz (status ok, pausa, duracoes), marcadores.json,
  cursor.jsonl (linhas validas dentro da captura) e, com ffprobe (PATH ou .deps), 3 faixas de audio + 2 capitulos.
  Nao move o mouse nem digita. Audio global mutado. Apaga -OutDir no fim (use -Keep para inspecionar).
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-project.ps1 [-OutDir <pasta>] [-Keep]
#>
param(
    [string]$Config = "RelWithDebInfo",
    [string]$OutDir = (Join-Path $env:TEMP "voltz-smoke-project"),
    [switch]$Keep
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rundir = Join-Path $root "build_x64\rundir\$Config"
$bin = Join-Path $rundir "bin\64bit"
$exe = Join-Path $bin "VoltzStudio.exe"
$ws = Join-Path $PSScriptRoot "voltz-ws.ps1"
$uia = Join-Path $PSScriptRoot "voltz-uia.ps1"
if (-not (Test-Path $exe)) { Write-Host "FALHOU executavel nao encontrado: $exe"; exit 1 }

$checks = [ordered]@{}
function Check([string]$name, [bool]$ok) { $script:checks[$name] = $ok }

function Ws([string]$json) {
    $file = Join-Path $env:TEMP "voltz-smoke-project-req.json"
    [IO.File]::WriteAllText($file, $json, (New-Object Text.UTF8Encoding $false))
    $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $file
    Remove-Item -Force $file -ErrorAction SilentlyContinue
    return @($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json })
}

function Uia([string]$name) {
    $result = & powershell -ExecutionPolicy Bypass -File $uia -ProcessId $script:proc.Id -Name $name
    Write-Host $result
    return ($LASTEXITCODE -eq 0)
}

function Read-JsonFile([string]$path) {
    return (Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json) | ForEach-Object { $_ }
}

# Config portatil: guarda e cria uma limpa (websocket sem senha, pt-BR, sem confirmacao ao sair, nome do projeto)
$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.smokeproject-bak"
if (Test-Path $bak) { Write-Host "FALHOU backup anterior em $bak (restaure ou apague antes de rodar)"; exit 1 }
if (Test-Path $cfg) { Move-Item $cfg $bak }
$voltzCfg = Join-Path $cfg "Voltz Studio"
$wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
New-Item -ItemType Directory -Force $wsDir | Out-Null
[IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}')
[IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[Voltz]`r`nNextProjectName=Smoke projeto`r`n", (New-Object Text.UTF8Encoding $false))
if (Test-Path $OutDir) { Remove-Item -Recurse -Force $OutDir }
New-Item -ItemType Directory -Force $OutDir | Out-Null

$ffprobe = (Get-Command ffprobe -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $ffprobe) {
    $ffprobe = Get-ChildItem (Join-Path $root ".deps") -Filter ffprobe.exe -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}

$logCopy = Join-Path $env:TEMP "voltz-smoke-project-log.txt"
if (Test-Path $logCopy) { Remove-Item -Force $logCopy }
$proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
try {
    Start-Sleep -Seconds 15
    Check "app aberto" (-not $proc.HasExited)
    Check "Pular (padroes Voltz)" (Uia "Pular")
    Start-Sleep -Seconds 2

    $r = Ws ('[{"requestType":"SetProfileParameter","requestData":{"parameterCategory":"SimpleOutput","parameterName":"FilePath","parameterValue":' + (ConvertTo-Json $OutDir) + '}},' +
        '{"requestType":"GetSpecialInputs"},{"requestType":"GetHotkeyList"},' +
        '{"requestType":"CreateScene","requestData":{"sceneName":"Cena B"}},' +
        '{"requestType":"CreateScene","requestData":{"sceneName":"Fontes de teste"}},' +
        '{"requestType":"CreateInput","requestData":{"sceneName":"Fontes de teste","inputName":"Tela teste","inputKind":"monitor_capture","sceneItemEnabled":false}}]')
    Check "pasta de gravacoes apontada para -OutDir" ($r.Count -ge 1 -and $r[0].ok)
    Check "atalho VoltzStudio.AddMarker registrado" ($r.Count -ge 3 -and (@($r[2].data.hotkeys) -contains "VoltzStudio.AddMarker"))
    $special = $r[1].data
    $mutes = @()
    foreach ($n in @($special.desktop1, $special.desktop2, $special.mic1, $special.mic2, $special.mic3, $special.mic4)) {
        if ($n) { $mutes += @{ requestType = "SetInputMute"; requestData = @{ inputName = $n; inputMuted = $true } } }
    }
    if ($mutes.Count -gt 0) { [void](Ws (ConvertTo-Json -InputObject $mutes -Depth 5 -Compress)) }

    # "Tela teste": captura de monitor numa cena que nunca vai ao programa (item desativado) - so para o cursor
    $items = (Ws '[{"requestType":"GetInputPropertiesListPropertyItems","requestData":{"inputName":"Tela teste","propertyName":"monitor_id"}}]')[0].data.propertyItems
    $monitorId = (@($items) | Where-Object { $_.itemEnabled } | Select-Object -First 1).itemValue
    Check "monitor para a Tela de teste" ([bool]$monitorId)
    if ($monitorId) { [void](Ws ('[{"requestType":"SetInputSettings","requestData":{"inputName":"Tela teste","inputSettings":{"monitor_id":' + (ConvertTo-Json $monitorId) + '}}}]')) }

    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 3
    [void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Cena B"}}]')
    Start-Sleep -Seconds 1
    Check "botao Marcador (UI Automation)" (Uia "Marcador")
    Start-Sleep -Seconds 1
    $r = Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.AddMarker"}}]'
    Check "atalho de marcador (websocket)" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 1
    [void](Ws '[{"requestType":"PauseRecord"}]')
    Start-Sleep -Seconds 2
    [void](Ws '[{"requestType":"ResumeRecord"}]')
    Start-Sleep -Seconds 3
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 4
} finally {
    $cleanExit = $false
    if (-not $proc.HasExited) {
        $proc.Refresh()
        [void]$proc.CloseMainWindow()
        $cleanExit = $proc.WaitForExit(30000)
        if (-not $cleanExit) { Stop-Process -Id $proc.Id -Force }
    }
    Check "encerrou limpo ao fechar a janela" $cleanExit
    $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime | Select-Object -Last 1
    if ($log) { Copy-Item $log.FullName $logCopy -Force }
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    if (Test-Path $bak) { Move-Item $bak $cfg }
}

function LogHas([string]$text) { return [bool]((Test-Path $logCopy) -and (Select-String -LiteralPath $logCopy -SimpleMatch $text -Encoding UTF8 -Quiet)) }

$projects = @(Get-ChildItem -LiteralPath $OutDir -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz") })
Check "uma pasta de projeto em -OutDir" ($projects.Count -eq 1)
if ($projects.Count -eq 1) {
    $dir = $projects[0].FullName
    Check "nome da pasta 'AAAA-MM-DD HHhMM . Smoke projeto'" ($projects[0].Name -match '^\d{4}-\d{2}-\d{2} \d{2}h\d{2} . Smoke projeto$')
    $m = Read-JsonFile (Join-Path $dir "projeto.voltz")
    Check "manifesto formato/versao" ($m.formato -eq "voltz-projeto" -and $m.versao -eq 1)
    Check "manifesto status ok" ($m.status -eq "ok")
    Check "manifesto nome" ($m.nome -eq "Smoke projeto")
    Check "t0_ns preenchido" ([int64]$m.t0_ns -gt 0)
    $pauses = @($m.pausas)
    $pauseMs = 0
    if ($pauses.Count -eq 1 -and $null -ne $pauses[0].fim_ms) { $pauseMs = $pauses[0].fim_ms - $pauses[0].inicio_ms }
    Check "uma pausa fechada entre 1,5 e 5 s ($pauseMs ms)" ($pauses.Count -eq 1 -and $pauseMs -ge 1500 -and $pauseMs -le 5000)
    Check "duracao bruta entre 8 e 40 s ($($m.duracao_ms) ms)" ($m.duracao_ms -ge 8000 -and $m.duracao_ms -le 40000)
    Check "duracao liquida = bruta - pausa" ($m.duracao_liquida_ms -eq ($m.duracao_ms - $pauseMs))
    $prog = Join-Path $dir ([string]$m.arquivos.programa.arquivo)
    Check "programa.mp4 existe e > 0" ($m.arquivos.programa.arquivo -eq "programa.mp4" -and (Test-Path -LiteralPath $prog) -and (Get-Item -LiteralPath $prog).Length -gt 0)
    Check "manifesto declara 3 faixas de audio" ($m.arquivos.programa.faixas_audio -eq 3)
    Check "manifesto sem tela/camera (ficam para o 3B)" (-not $m.arquivos.tela -and -not $m.arquivos.camera)
    Check "nenhum arquivo .tmp sobrando" (@(Get-ChildItem -LiteralPath $dir -Filter *.tmp).Count -eq 0)

    $markers = @(Read-JsonFile (Join-Path $dir "marcadores.json"))
    Check "2 marcadores (Marcador 1, Marcador 2) em ordem" ($markers.Count -eq 2 -and $markers[0].nome -eq "Marcador 1" -and $markers[1].nome -eq "Marcador 2" -and $markers[0].t_ms -lt $markers[1].t_ms -and $markers[1].t_ms -lt $m.duracao_ms)

    $cursorFile = Join-Path $dir "cursor.jsonl"
    $cursorOk = $false
    $cursorLines = 0
    if ($m.arquivos.cursor -and (Test-Path -LiteralPath $cursorFile)) {
        $w = [int]$m.arquivos.cursor.largura
        $h = [int]$m.arquivos.cursor.altura
        $cursorOk = $w -gt 0 -and $h -gt 0
        foreach ($line in Get-Content -Encoding UTF8 -LiteralPath $cursorFile) {
            if (-not $line) { continue }
            $cursorLines++
            $e = $line | ConvertFrom-Json
            if (@("move", "down", "up", "scroll", "fora") -notcontains $e.e -or $null -eq $e.t -or $e.t -lt 0) { $cursorOk = $false; break }
            if ($e.e -ne "fora" -and ($e.x -lt 0 -or $e.y -lt 0 -or $e.x -ge $w -or $e.y -ge $h)) { $cursorOk = $false; break }
        }
    }
    Check "cursor.jsonl com >= 1 linha valida dentro da captura ($cursorLines linha(s))" ($cursorOk -and $cursorLines -ge 1)

    Check "log: capitulos 'Marcador 1' e 'Marcador 2'" ((LogHas 'Adding chapter "Marcador 1"') -and (LogHas 'Adding chapter "Marcador 2"'))
    Check "log: projeto finalizado (ok" (LogHas '[voltz] projeto finalizado (ok')
    Check "log: sem 'a gravacao nao iniciou'" (-not (LogHas '[voltz] a gravacao nao iniciou'))

    if ($ffprobe) {
        $probe = (& $ffprobe -v error -show_entries "stream=codec_type:format=duration" -show_chapters -of json $prog) | Out-String | ConvertFrom-Json
        $audio = @($probe.streams | Where-Object { $_.codec_type -eq "audio" }).Count
        $chapters = @($probe.chapters).Count
        $durMs = [double]$probe.format.duration * 1000
        Check "ffprobe: 3 faixas de audio ($audio)" ($audio -eq 3)
        Check "ffprobe: 2 capitulos ($chapters)" ($chapters -eq 2)
        Check "ffprobe: duracao ~ liquida +-1 s ($([int]$durMs) x $($m.duracao_liquida_ms) ms)" ([math]::Abs($durMs - $m.duracao_liquida_ms) -le 1000)
        if ($chapters -ge 1 -and $markers.Count -ge 1) {
            Check "ffprobe: capitulo 1 ~ marcador 1 +-700 ms" ([math]::Abs([double]$probe.chapters[0].start_time * 1000 - $markers[0].t_ms) -le 700)
        }
    } else {
        Write-Host "AVISO ffprobe nao encontrado: faixas/capitulos/duracao nao conferidos pelo ffprobe (use o log)"
    }
}

$failed = 0
foreach ($k in $checks.Keys) {
    if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
}
if (Test-Path $logCopy) { Write-Host "log: $logCopy" }
if (-not $Keep) { Remove-Item -Recurse -Force -LiteralPath $OutDir -ErrorAction SilentlyContinue; Write-Host "apagado: $OutDir" }
if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE PROJETO OK"
```
Conferir que o arquivo ficou só com ASCII: `grep -nP '[^\x00-\x7F]' tools/voltz-smoke-project.ps1` → sem saída.

- [ ] **Step 2: `tools/voltz-smoke-record.ps1` — pasta de projeto, log em UTF-8 e limpeza**

1. Logo depois da linha `if (-not (Test-Path $exe)) { ... }`, acrescentar:
```powershell
# Plano 3A: a pasta padrao de gravacoes e Videos\Voltz Studio (o app cria). Se esta execucao a criou e ela ficar
# vazia, e apagada no fim.
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"
$hadVoltzVideos = Test-Path -LiteralPath $voltzVideos
```
2. Na extração do caminho, trocar
`$m = Select-String -Path $log.FullName -Pattern "Writing .*?file '(.+)'\.\.\.\s*$" | Select-Object -Last 1`
por
`$m = Select-String -Path $log.FullName -Pattern "Writing .*?file '(.+)'\.\.\.\s*$" -Encoding UTF8 | Select-Object -Last 1`
(o caminho agora tem "·" e o nome padrão "Gravação").
3. Trocar o bloco
```powershell
# Apaga somente o arquivo exato que o app gravou (caminho vindo do log)
foreach ($f in $files) { Remove-Item -Force $f.FullName; Write-Host "apagado: $($f.FullName)" }
```
por
```powershell
# Plano 3A: a gravacao fica numa pasta de projeto (programa.mp4 + projeto.voltz + marcadores.json [+ cursor.jsonl]).
# Apaga a pasta inteira so se ela tem projeto.voltz e foi criada nesta execucao; senao, so o arquivo exato.
$projDir = if ($recPath) { Split-Path -Parent $recPath } else { $null }
$isProject = $projDir -and (Test-Path -LiteralPath (Join-Path $projDir "projeto.voltz")) -and ((Get-Item -LiteralPath $projDir).CreationTime -ge $start)
if ($isProject) {
    $manifest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $projDir "projeto.voltz") | ConvertFrom-Json
    $closed = $manifest.status -eq "ok" -and @($manifest.eventos | Where-Object { $_.tipo -eq "app_fechado_gravando" }).Count -eq 1
    if ($closed) { Write-Host "OK     projeto.voltz fechado ao sair gravando (status ok, app_fechado_gravando)" }
    else { Write-Host "FALHOU projeto.voltz nao foi fechado ao sair gravando (status '$($manifest.status)')"; $failed++ }
    Remove-Item -Recurse -Force -LiteralPath $projDir
    Write-Host "apagado: $projDir"
} else {
    foreach ($f in $files) { Remove-Item -Force $f.FullName; Write-Host "apagado: $($f.FullName)" }
}
if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) {
    Remove-Item -Force -LiteralPath $voltzVideos
    Write-Host "apagado (vazio): $voltzVideos"
}
```

- [ ] **Step 3: `tools/voltz-smoke.ps1` — mesma limpeza da pasta Voltz**

Depois de `if (-not (Test-Path $exe)) { ... }`, as duas linhas de `$voltzVideos`/`$hadVoltzVideos` do Step 2.1; antes de
`if ($failed -gt 0) { exit 1 }`, o bloco `if (-not $hadVoltzVideos -and ...) { ... }` do fim do Step 2.3. E,
como o app agora aplica os padrões de projeto ao abrir, acrescentar aos checks, dentro do
`if (-not $SkipShell) { ... }` (o `ProjectRecorder` nasce com a casca):
```powershell
$checks["log contem '[voltz] padroes de projeto aplicados'"] = [bool]($log -and (Select-String -Path $log.FullName -Pattern '\[voltz\] padroes de projeto aplicados' -Quiet))
```

- [ ] **Step 4: Rodar os três**

Backup da config portátil antes (os smokes antigos a apagam): `mv build_x64/rundir/RelWithDebInfo/config
build_x64/rundir/RelWithDebInfo/config.3abak` (se existir). Então, em sequência (nunca dois ao mesmo tempo):
SMOKE, SMOKE GRAVAÇÃO, SMOKE PROJETO (`-OutDir "<scratchpad>\smoke-projeto"`). Restaure a config
(`rm -rf .../config && mv .../config.3abak .../config`).
Expected: `SMOKE OK`, `SMOKE GRAVACAO OK` (com a linha `OK projeto.voltz fechado ao sair gravando`) e
`SMOKE PROJETO OK` com todos os checks `OK`; nenhuma pasta nova em `Vídeos` (a `Voltz Studio` só fica se já existia).
Se um check do smoke de projeto falhar, rode de novo com `-Keep` e inspecione `-OutDir` e o log copiado antes de
mexer no código.

- [ ] **Step 5: Commit**

```bash
git add tools/voltz-smoke-project.ps1 tools/voltz-smoke-record.ps1 tools/voltz-smoke.ps1
git commit -m "test(voltz): smoke de gravacao em projeto e smokes atualizados para a pasta de projeto

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Verificação, registro e push

**Files:** `docs/voltz/backlog-plano-3.md` (novo), `docs/voltz/backlog-plano-2.md`, `docs/voltz/upstream-patches.md`
(conferência), `CLAUDE.md`, `AGENTS.md`

- [ ] **Step 1:** COMPILAR TUDO (segundo plano), RODAR TESTES (todos passam; informe o total — esperado **91**
  casos), SMOKE, SMOKE GRAVAÇÃO, SMOKE PROJETO (com backup/restauração da config portátil, como na Task 8).
- [ ] **Step 2:** Capturas finais **só da janela** (bloco de teste, config limpa, sem câmera, **Preparo de
  gravação** + uma gravação de 3 s para Projetos não ficar vazia): Gravar (barra inferior completa), Projetos,
  Presets, Ajustes. Descreva cada uma no relatório.
- [ ] **Step 3:** Nenhum arquivo do OBS mudou:
  `git diff 555ca2417..HEAD --stat -- . ':(exclude)frontend/voltz' ':(exclude)docs' ':(exclude)tools' ':(exclude)frontend/data/locale/voltz-pt-BR.ini' ':(exclude)frontend/data/locale/voltz-en-US.ini' ':(exclude)frontend/cmake/feature-voltz.cmake' ':(exclude)CLAUDE.md' ':(exclude)AGENTS.md'`
  → sem saída. `docs/voltz/upstream-patches.md` fica igual (nada a registrar no 3A).
- [ ] **Step 4: Backlog do Plano 3** — criar `docs/voltz/backlog-plano-3.md`:
  ```markdown
  # Backlog do Plano 3 (gravação em projeto)

  ## Feito no Plano 3A
  - **Projeto por gravação:** toda gravação (casca, clássico, atalho, websocket, `--startrecording`) vira
    `<pasta>/AAAA-MM-DD HHhMM · Nome` com `programa.mp4` (MP4 híbrido), `projeto.voltz` v1, `marcadores.json` e
    `cursor.jsonl`. Sem patch no OBS: troca temporária de pasta + `FilenameFormatting=programa` no
    `RECORDING_STARTING` (`ProfileOverride`), desfeita no próximo giro do laço.
  - **Manifesto v1** (núcleo testado, nlohmann `ordered_json`): escrita atômica, pulsação a cada 5 s, pausas,
    `duracao_ms`/`duracao_liquida_ms`, `eventos` (`disco_cheio`, `app_fechado_gravando`, `inicio_falhou`,
    `erro_gravacao`, `cursor_indisponivel`, `recuperado`), campos desconhecidos preservados ao reescrever; cópia em
    `<config>/recovery` se a escrita falhar.
  - **3 faixas** no modo Simples (`RecTracks=7` se padrão; mixers das fontes globais: sistema 1+3, microfone 1+2, só
    se ainda em "todas as faixas").
  - **Marcadores:** botão ⚑ e atalho `VoltzStudio.AddMarker` (padrão Ctrl+Shift+M) → capítulo + `marcadores.json`.
  - **Cursor:** gancho `WH_MOUSE_LL` em thread própria + escrita a 60 Hz; coordenadas físicas → pixels da captura de
    monitor; posição inicial sempre gravada.
  - **Projetos:** lista projetos e vídeos soltos (duração, tamanho, status), renomear (manifesto), recuperar
    gravação interrompida (duração = maior entre pulsação e mtime − criado_em), lixeira da pasta inteira.
  - **Barra inferior:** nome do projeto (recentes), Marcador, estimativa/medida de GB/min e espaço livre; avisos de
    disco (< 5 GB âmbar; < 1 GB para limpo). `VOLTZ_FAKE_FREE_BYTES` simula espaço nos testes.
  - **Pasta padrão** `Vídeos\Voltz Studio` quando o usuário não escolheu pasta.
  - Smoke novo `tools/voltz-smoke-project.ps1` (ffprobe: 3 faixas, 2 capítulos, duração).

  ## Plano 3B (próximo)
  - ISO `tela.mkv`/`camera.mkv` via `obs_canvas` (spec §5.3), só a fonte de captura, resolução nativa.
  - Cadeia de encoders AMF → NVENC → QSV → x264 para 3 sessões (§5.4); sem GPU para 3 → só programa + aviso +
    evento.
  - `offset_ms` do primeiro quadro de cada ISO (§5.5); `arquivos.tela.origem` (mesmo formato do `arquivos.cursor`);
    eventos `iso_falhou` com `faixa`.

  ## Pendências e limites do 3A
  - Cursor em **captura de janela** (evento `janela` com o retângulo; hoje: sem `cursor.jsonl` +
    `cursor_indisponivel`); trocar de monitor no meio da gravação; rolagem horizontal; teclas/atalhos (opcional).
  - Marcador pelo atalho do próprio OBS ("Adicionar marcador de capítulo") ou `CreateRecordChapter` do websocket vira
    capítulo mas não entra no `marcadores.json`.
  - Fontes de áudio de **cena** (não globais) continuam em todas as faixas (entram em "mic" e "sistema").
  - Modo de saída Avançado: `AdvOut/RecTracks` não é ajustado; divisão automática de arquivo (`RecSplitFile`) geraria
    vários `programa*`.
  - Formato MKV/MOV escolhido pelo usuário: programa vira `programa.mkv`/`.mov`, sem capítulos no MKV.
  - Renomear projeto troca só o `nome` do manifesto (a pasta mantém o nome).
  - Falha síncrona ao iniciar (`inicio_falhou`) não foi exercitada por teste automático.
  - **Pasta de gravação inválida:** o OBS avisa só por balão da bandeja (`GetOutputFilename`, `OBSApp.cpp`); a casca
    não mostra nada. (veio do backlog do Plano 2)

  ## Testes manuais pendentes do 3A
  - Renomear projeto e mover projeto (pasta) para a lixeira pela tela Projetos.
  - Atalho Ctrl+Shift+M real durante a gravação (marcador + capítulo) e trocar o atalho em Configurações → Atalhos.
  - Cliques e rolagem reais aparecem no `cursor.jsonl` com coordenadas certas em monitor 125%/150% e em monitor à
    esquerda do principal; o mouse não fica lento durante a gravação.
  - Recuperação depois de um encerramento real do processo gravando (finalizar pelo Gerenciador de Tarefas): o
    `programa.mp4` híbrido toca e o projeto aparece "Recuperável".
  - Encher o disco de verdade (pendrive pequeno) → aviso âmbar e parada limpa.
  ```
- [ ] **Step 5:** `docs/voltz/backlog-plano-2.md`: remover o item **Pasta de gravação inválida** de "Casca / ciclo de
  vida" (foi para o backlog do 3) e acrescentar no topo, depois do título, a linha
  `> Pendências de gravação (Plano 3 em diante) ficam em `docs/voltz/backlog-plano-3.md`.`
- [ ] **Step 6: `CLAUDE.md`** (depois `cp CLAUDE.md AGENTS.md`):
  - *Estado atual*, +1 linha: `- Plano 3A concluído: toda gravação vira pasta de projeto (programa.mp4 com 3 faixas e capítulos, projeto.voltz v1, marcadores.json, cursor.jsonl), Projetos lista/recupera, barra inferior com nome do projeto, Marcador, espaço e avisos de disco. Testes: 91 casos + tools/voltz-smoke-project.ps1.`
  - *Próximos passos* → `1. Escrever e executar o Plano 3B (ISO tela.mkv/camera.mkv via obs_canvas, encoders p/ 3 sessões, offset_ms, iso_falhou) a partir de docs/voltz/backlog-plano-3.md e spec §5.3–5.5.` e `2. Depois: Plano 4 (anotações + mouse), 5 (marca visual + instalador + CI).`
  - *Decisões*, +1 linha: `- Gravação em projeto sem patch no OBS: no RECORDING_STARTING o ProjectRecorder troca temporariamente a pasta e Output/FilenameFormatting=programa (ProfileOverride) e desfaz no próximo giro do laço; 3 faixas no modo Simples (RecTracks=7 + mixers das fontes globais, só se ainda padrão). Núcleo do projeto em JSON com nlohmann (ordered_json, preserva campos desconhecidos).`
  - *Armadilhas*, +1 linha cada (só as que se confirmaram na execução): callbacks do frontend rodam do último
    registrado para o primeiro (criar o `ProjectRecorder` depois da `FrontendBridge`); `TriggerHotkeyByName` chama o
    callback do atalho na thread do websocket com o lock dos atalhos preso (só enfileirar); `Select-String` do PS 5.1
    precisa de `-Encoding UTF8` para caminhos com acento no log; `signals` é macro do Qt (não usar como nome de
    variável); warnings como erro valem também para o `voltz-core` e os testes; nada de IO/log dentro do gancho
    `WH_MOUSE_LL`.
  - Conferir o tamanho: `wc -c CLAUDE.md` abaixo de ~10 KB; se passar, mover narrativa para `docs/HISTORICO.md`
    (criar se não existir) e deixar o destilado.
- [ ] **Step 7:** Commit e push:
```bash
git add docs/voltz/backlog-plano-3.md docs/voltz/backlog-plano-2.md CLAUDE.md AGENTS.md
git commit -m "docs(voltz): registro do Plano 3A (backlog, memoria)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push origin voltz
```

---

## Auto-revisão (feita ao escrever)

- **Cobertura do escopo:** núcleo puro testado — manifesto v1 (modelo, JSON, atomicidade, leitura tolerante,
  versão, preservação, status), nome da pasta (saneamento, colisão), linha do tempo, `cursor.jsonl` (formato,
  60 Hz, fora), conversão de coordenadas (multi-monitor, origem negativa, DPI 125/150%, captura escalada),
  `marcadores.json`, espaço em disco (T1-T3); `ProjectRecorder` com programa na pasta, MP4 híbrido, 3 faixas,
  pausas, `t0_ns` pelo relógio do libobs, marcador por atalho (frontend hotkey) e capítulo, disco < 1 GB, saída do
  app, falha ao iniciar (T4); `CursorTracker` com gancho LL em thread própria e escrita bufferizada (T5); Projetos
  com projetos + vídeos soltos, renomear, lixeira, recuperar, pasta padrão `Vídeos\Voltz Studio` (T4/T6); barra
  inferior com nome/recentes, Marcador + feedback, Gravar/Pausar, estimativa, avisos de disco (T7); integração por
  websocket/UIA/ffprobe sem mexer no mouse (T8); registro/backlog/memória/push (T9). Captura de janela no cursor,
  ISO e anotações ficam explicitamente fora (3B/Plano 4/backlog).
- **Placeholders:** nenhum "TBD". Onde só a execução decide há passo com critério e alternativa: troca no STARTING
  chegar ao `ConfigureRecording` (T4 Step 7, critérios 1 e 5), `find` do `ordered_json` com `const char *` (T2 Step 4),
  `nlohmann_json` visível nos testes (T2 Step 7), capítulos/faixas sem `ffprobe` (T4/T8 pelo log), retângulo físico do
  monitor (T5, comparado ao log do `win-capture`), `QFile::moveToTrash` com pasta (T6, manual).
- **Consistência de nomes:** núcleo `writeFileAtomically`/`readFileText`, `sanitizeProjectName`/`projectFolderName`/
  `uniqueFolderName`/`LocalDateTime`, `ProjectTimeline`/`PauseSpan`/`netDuration`, `ProjectManifest`/`ProgramFile`/
  `CursorFile`/`CaptureOrigin`/`ManifestEvent`/`ManifestParse`/`serializeManifest`/`parseManifest`/
  `recoverManifest`/`manifestNetDurationMs`/`statusToString`/`statusFromString`/`kManifestFileName`/`kProgramBaseName`/
  `kMarkersFileName`/`kCursorFileName`, `Marker`/`serializeMarkers`/`parseMarkers`, `MonitorRect`/`CaptureMapping`/
  `CapturePoint`/`mapToCapture`/`RawMouseEvent`/`RawMouseKind`/`MouseButton`/`CursorEvent`/`CursorEventType`/
  `cursorLine`/`CursorSampler`, `DiskLevel`/`diskLevel`/`RateInput`/`estimateBytesPerMinute`/
  `measuredBytesPerMinute`/`formatRate`/`formatFreeSpace`/`kGiB`; projeto `ProjectRecorder` (`addMarker`,
  `measuredBytesPerMinute`, `freeBytes`, `nextProjectName`/`setNextProjectName`, `currentProjectDir`/
  `currentProjectName`, sinais `projectStarted`/`projectFinished`/`markerAdded`/`stoppedForLowDisk`/
  `manifestWriteFailed`), `nativeUtf8Path`, `CursorTracker`, `findMonitor`/`MonitorDescription`, `ProjectLibrary`/
  `ProjectEntry`; casca `RecordingConfig::outputPathKey`/`outputFolder`/`recordingAudioTracks`/
  `applyProjectDefaults`/`estimatedBytesPerMinute`, `ProfileOverride`, `ErrorBanner::setWarning`/`dismissed`,
  `palette::kWarn`; locale `Voltz.Project.*`, `Voltz.Marker.Name`, `Voltz.Hotkey.AddMarker`, `Voltz.Record.*`,
  `Voltz.Disk.*`, `Voltz.Projects.*` iguais nos dois `.ini`; atalho `VoltzStudio.AddMarker` igual no código e nos
  testes.
- **Contagem de testes:** 48 (base) → 58 (T1) → 75 (T2) → 91 (T3).
