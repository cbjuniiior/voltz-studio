# Voltz Studio — Plano 4B: anotações (núcleo, fonte Voltz Anotações, `anotacoes.jsonl` e sobreposição só de exibição) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Pôr de pé tudo o que o desenho na tela (spec §6.2–§6.3) precisa **exceto a entrada do usuário**: traços
vetoriais em pixels da captura (`tela.mkv`), a fonte OBS "Voltz Anotações" que os desenha no programa como camada 2 da
Tela Voltz (grupos existentes migram de `voltz_tela_camadas` 1 → 2), a janela de sobreposição sobre o monitor da Tela
(sempre **transparente a cliques**, `WDA_EXCLUDEFROMCAPTURE`), o caso "queimadas" (Windows < 10 2004 ou afinidade
recusada → `"anotacoes_queimadas": true` e a camada não desenha) e o `anotacoes.jsonl` do projeto — com os traços
entrando, neste plano, **só** por pedidos de teste do obs-websocket.

**Architecture:** Lógica pura e testada no núcleo (`frontend/voltz/core/`): modelo do traço e do estilo
(`annotation-model`), forma + rasterizador por CPU em BGRA pré-multiplicado (`annotation-raster`, o MESMO desenho na
sobreposição e no programa), traços ao vivo (`stroke-store`), diário/consolidação do `anotacoes.jsonl`
(`annotation-journal`) e o manifesto com `arquivos.anotacoes`/`anotacoes_queimadas`. O plugin `voltz-sources` ganha
`AnnotationFeed` (estado com mutex, alimentado por procedimentos `voltz_annot_*` no proc handler global, como o
`MouseFeed` do 4A) e a fonte `voltz_annotations_source`, que rasteriza quando a versão muda e desenha uma textura
dinâmica. No frontend, `AnnotationController` (thread da UI) é o dono do `StrokeStore`: repassa cada mudança ao plugin,
à `OverlayWindow` (janela Win32 pura em camadas, nunca ativa, sem nenhum método que a torne clicável) e ao
`ProjectRecorder` (diário durante a gravação, consolidado no fim). Os testes injetam traços pelo fornecedor
"voltz-teste-casca" (registrado pelo frontend só com `VOLTZ_TEST_LIVE`), com a sobreposição posta **fora da área de
trabalho** por `VOLTZ_TEST_OVERLAY_RECT`. **Nenhum patch em arquivo do OBS.**

**Tech Stack:** C++17, Qt 6.11.1, libobs/frontend 32.2.2 (`obs_source_info`, `gs_texture_create(GS_DYNAMIC)`,
`gs_texture_set_image`, `gs_blend_function`, grupos de cena, `proc_handler_*`), obs-websocket 5.7.4 (API de
fornecedor `obs-websocket-api.h`, `GetSourceScreenshot`), Win32 (`CreateWindowExW` + `UpdateLayeredWindow`,
`SetWindowDisplayAffinity`/`GetWindowDisplayAffinity`, `CreateDIBSection`), nlohmann_json 3.11, doctest. Testes ao vivo
por `tools/voltz-ws.ps1`, `tools/voltz-uia.ps1`, P/Invoke só de leitura (`FindWindowEx`, `GetWindowLongPtr`,
`GetWindowDisplayAffinity`, `IsWindowVisible`) e `ffprobe` (só metadados).

**Spec:** `docs/superpowers/specs/2026-10-15-voltz-studio-base-gravacao-design.md` §3 (annotate/: OverlayWindow,
StrokeStore; fontes Voltz), §4.3 (Ajustes → Anotações/Atalhos — vão para o 4C, ver abaixo), §4.4 (Tela Voltz com 3
camadas), §5.1/§5.2 (`anotacoes.jsonl`, manifesto), §5.5 (tempos relativos ao t0), §6.2–§6.3, §8 (Windows < 10 2004),
§9 (serialização de traços testada) · **Backlog:** `docs/voltz/backlog-plano-4.md` ("Próximo: Plano 4B") e o fim de
`docs/superpowers/plans/2026-10-16-voltz-plano-4a-mouse.md` · **Modelo de formato:**
`docs/superpowers/plans/2026-10-16-voltz-plano-4a-mouse.md`.

**Base deste plano:** commit `4bfd03610` (branch `voltz`, publicado) + o commit que adiciona este arquivo. Contagem
de testes na base: **126** casos.

## Divisão do restante do Plano 4 (e o porquê)

O que sobrou do Plano 4 depois do 4A (desenho na tela) mistura dois riscos de natureza diferente: **(a)** dados, render
e arquivos — modelo do traço, `anotacoes.jsonl`, fonte no render do libobs, migração dos grupos, manifesto, janela que
só **mostra** o desenho por cima do monitor e fica fora da captura; **(b)** uma janela topmost que **captura a entrada
do mouse e do teclado** do usuário (modo desenho), com barra de ferramentas, ferramenta de texto, atalhos globais,
Ajustes › Anotações e Ajustes › Atalhos. Juntos passariam de 20 tasks e, pior, misturariam código que pode ser testado
de ponta a ponta sem tocar na entrada do usuário com código que **só** pode ser verificado à mão por ele. Por isso:

- **Plano 4B (este arquivo):** (a) inteiro. Entrega software verificável sozinho: traços injetados por teste aparecem
  no programa (Voltz Anotações), na sobreposição (fora da captura) e no `anotacoes.jsonl`; os grupos "Tela Voltz"
  ganham a camada; o caso "queimadas" está implementado e testado por simulação. **Não existe neste plano nenhum
  código que deixe a sobreposição clicável** — a garantia de segurança é estrutural, não depende de disciplina.
- **Plano 4C (próximo, descrito no fim):** modo desenho (entrada), saídas garantidas, barra, ferramentas (incluindo
  texto), atalhos, Ajustes › Anotações/Atalhos, sobreposição em todos os monitores, aviso visível de "queimadas" —
  verificado por roteiro manual do usuário.

**Fora do 4B, com o porquê:**
- **Modo desenho, barra, ferramenta de texto, atalhos `VoltzStudio.Draw`/limpar/desfazer, Ajustes › Anotações,
  Ajustes › Atalhos, cores rápidas/seletor:** Plano 4C (precisa da entrada do usuário; ver o fim deste arquivo).
- **Aviso visível de "queimadas" na casca:** 4C (o usuário só desenha no 4C; no 4B o caso fica no log e no manifesto).
- **Sobreposição em monitores que não são o da Tela:** 4C (no 4B só há o que mostrar sobre o monitor capturado).
- **Recuperação de projeto com `anotacoes.jsonl` de ponta a ponta:** o código entra (Task 9) e a consolidação tem
  teste unitário (Task 3); o teste ponta a ponta exigiria derrubar o app gravando, e o OBS mostra na abertura seguinte
  um diálogo modal ("modo de segurança") — proibido por automação. Fica no roteiro manual.

## Descobertas que decidem o desenho (conferidas no código 32.2.2)

- **O OBS reaplica a afinidade de captura em toda janela Qt:** `OBSApp::notify` (`frontend/OBSApp.cpp:1550-1587`)
  chama `OBSBasic::SetDisplayAffinity` (`frontend/widgets/OBSBasic.cpp:2208-2238`) no `QEvent::Show` de toda janela
  Qt do tipo Window/Dialog/Tool, trocando para `WDA_EXCLUDEFROMCAPTURE` **ou `WDA_NONE`** conforme
  `HideOBSWindowsFromCapture`; ligar/desligar a opção (`OBSBasicSettings.cpp:3060-3069`, `SettingsPage.cpp:320-327`)
  reaplica em todas as janelas visíveis. Só escapam janelas com a propriedade `isOBSProjectorWindow`. ⇒ Uma
  sobreposição feita em Qt perderia a exclusão sempre que o usuário (ou um teste, que usa
  `HideOBSWindowsFromCapture=false`) desligasse a opção. **A sobreposição é uma janela Win32 pura** (não é `QWidget`):
  o `OBSApp::notify` nunca a vê. (A barra do 4C, que será Qt, usa a propriedade `isOBSProjectorWindow`.)
- **`WDA_EXCLUDEFROMCAPTURE` antes do Windows 10 2004 vira caixa preta:** `SetDisplayAffinitySupported`
  (`frontend/utility/platform-windows.cpp:217-236`) só aceita build ≥ 19041 porque, antes, a flag age como
  `WDA_MONITOR`. ⇒ Sem suporte, a sobreposição **não** recebe afinidade (aparece na captura) e o caso vira "queimadas".
- **Fonte sem `OBS_SOURCE_SRGB` nem `OBS_SOURCE_CUSTOM_DRAW`:** `obs_source_main_render` (`libobs/obs-source.c:2863-2884`)
  desliga o `linear_srgb` e `obs_source_default_render` (`:2846-2860`) chama o `video_render` dentro da técnica "Draw"
  do efeito padrão; o render principal liga o framebuffer sRGB (`libobs/obs-video.c:165`). ⇒ A Voltz Anotações desliga o
  framebuffer sRGB durante o próprio desenho e mistura com `GS_BLEND_ONE/GS_BLEND_INVSRCALPHA` (alfa pré-multiplicado,
  como o `image_source`, `plugins/image-source/image-source.c:200-225`): a mistura fica em espaço gama, igual à da
  sobreposição (`UpdateLayeredWindow`) — o marca-texto tem a mesma cor nos dois.
- **Geometria na GPU não serve ao marca-texto:** o modo imediato do libobs vai a 512 vértices por lote
  (`libobs/graphics/graphics.c:69`) e cápsulas sobrepostas por segmento somariam alfa nas junções (o marca-texto
  translúcido ficaria "em contas"). ⇒ **Rasterizador por CPU no núcleo** com "maior cobertura por traço" (cada pixel do
  traço recebe a cobertura máxima entre as peças do traço; só então mistura). Testável com doctest pixel a pixel e
  reutilizado pela sobreposição (o mesmo desenho nos dois lugares).
- **Grupo:** `obs_scene_insert_group` (`libobs/obs-scene.c:3535-3599`) mantém a ordem do vetor (`items[0]` embaixo).
  Para acrescentar uma camada num grupo existente: `obs_scene_add(obs_sceneitem_group_get_scene(grupo), fonte)` —
  `obs_sceneitem_group_add_item` (`:3700-3734`) exige o item já na cena-mãe. `obs_scene_find_source` (`libobs/obs.h:1690`)
  acha a Tela dentro do grupo (para copiar o recorte).
- **Fornecedor do obs-websocket:** `PerformVendorRequest` (`plugins/obs-websocket/src/WebSocketApi.cpp:106-128`) chama o
  callback **sem lock**, na thread do websocket; `vendor_request_unregister` (`:316-345`) não espera callbacks em
  andamento; `vendor_register` (`:244-268`) só insere num mapa. ⇒ O frontend pode registrar um fornecedor depois do
  `OBSBasic::OBSInit` (todos os módulos carregados — é por isso que plugins só podem no `obs_module_post_load`), mas
  precisa de mutex próprio e de um ponteiro anulado no EXIT. `obs_data` não guarda lista de números: pontos vão como
  **texto JSON** (`"pts":"[[x,y],...]"`) e o estado volta como texto (`"estado":"{...}"`), como o `style` do 4A.
- **Procedimento sem parâmetro:** a declaração `"void voltz_annot_clear()"` é aceita (mesma forma de
  `"void save()"`, `plugins/obs-ffmpeg/obs-ffmpeg-mux.c:961`).
- **Coordenadas:** a Tela compartilhada de captura de monitor tem o tamanho do monitor em pixels físicos (o 3A grava o
  `cursor.jsonl` com `mapToCapture` 1:1). ⇒ Traços em pixels da captura (`obs_source_get_width/height` da Tela); a
  sobreposição estica a captura para o retângulo do monitor (`findMonitor`), escala 1 em produção.

---

## Decisões técnicas (e o porquê)

| Tema | Decisão | Por quê |
|---|---|---|
| Divisão | 4B (dados, render, arquivos, sobreposição só de exibição) e 4C (entrada: modo desenho, barra, atalhos, Ajustes) | Ver acima: o 4B não tem nenhum caminho de código que capture a entrada do usuário. |
| Coordenadas | Pixels da captura da **Tela compartilhada** (`tela.mkv`, as do `cursor.jsonl`); ponto (x,y) = pixel, centro em (x+0,5, y+0,5) | Uma única verdade para programa, sobreposição, editor e `anotacoes.jsonl` (spec §6.2). |
| Ferramentas no 4B | caneta, marca-texto, linha, seta, retângulo, elipse (`"ferr"`: `caneta`, `marca_texto`, `linha`, `seta`, `retangulo`, `elipse`); formas guardam só primeiro e último ponto | Texto exige caixa de digitação (entrada) e fonte tipográfica: 4C. |
| Desenho | Rasterizador por CPU no núcleo, BGRA pré-multiplicado, borda suavizada em ~1 px, **maior cobertura dentro do traço**; marca-texto = 3× a espessura, 40% de opacidade; seta com ponta cheia (max(12, 4×esp)); elipse = polígono de 24 a 180 lados | Mesmo resultado no programa e na sobreposição; marca-texto sem "contas" nas junções; testável. |
| Canal frontend → plugin | Procedimentos no proc handler global: `voltz_annot_begin(id, json)`, `voltz_annot_extend(id, points)`, `voltz_annot_remove(id)`, `voltz_annot_clear()`, `voltz_annot_burned(burned)`; `AnnotationFeed` (mutex + versão) no plugin | Mesmo mecanismo do 4A (`live-protocol.hpp`); `appendStrokePoints` é a mesma regra nos dois lados. |
| Render da fonte | `voltz_annotations_source` (`OBS_SOURCE_VIDEO | OBS_SOURCE_CAP_DISABLED`), tamanho = alvo (classe `TargetSize`, extraída da Voltz Mouse); rasteriza só quando a versão do feed ou o tamanho muda; textura `GS_BGRA` dinâmica; framebuffer sRGB desligado + `ONE/INVSRCALPHA` | Nada por quadro quando nada muda; cor igual à da sobreposição. |
| Tela Voltz | Versão de camadas **2**: Tela (0) · Voltz Anotações (1) · Voltz Mouse (2), travadas em (0,0), recorte copiado; grupos versão 1 ganham a camada na abertura e no `SCENE_COLLECTION_CHANGED` (sem depender de `voltz_tela_migrada`) | Spec §4.4; a versão no grupo evita duplicar a camada. |
| Sobreposição | Janela **Win32 pura** (`VoltzAnnotationOverlay`), `WS_POPUP`, `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`, `WM_NCHITTEST → HTTRANSPARENT`, `WM_MOUSEACTIVATE → MA_NOACTIVATE`; `UpdateLayeredWindow` com um DIB; criada só quando há traço e destruída no EXIT; escondida sem traço; **sem método para tirar o `WS_EX_TRANSPARENT`** (e a cada desenho os bits obrigatórios são reconferidos) | Segurança estrutural; fora do alcance do `OBSApp::notify`. |
| Fora da captura | `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` **sempre** (independe de `HideOBSWindowsFromCapture`), conferido por `GetWindowDisplayAffinity` | A sobreposição nunca deve entrar em `tela.mkv`/programa; o programa recebe o desenho pela fonte. |
| Queimadas | Sem `SetDisplayAffinitySupported()` (ou se a afinidade não pegar): sobreposição **sem** afinidade (aparece na captura), `voltz_annot_burned(true)` → a fonte não desenha (não duplica), `"anotacoes_queimadas": true` no manifesto; log `[voltz] anotacoes: queimadas (...)` | Spec §6.3/§8; nunca usar a flag onde ela vira caixa preta. |
| `anotacoes.jsonl` | Diário só de acréscimo durante a gravação (linha completa quando o traço termina; `{"id":N,"t_limpo":T}` quando sai da tela); traços já na tela no início entram com `t_ini = t_fim = 0`; em andamento no fim fecham no fim; **consolidado** no formato da spec ao finalizar e ao recuperar | Sobrevive a uma queda; o arquivo final tem uma linha por traço como na spec. |
| "Somem após N s" | `user.ini [Voltz] AnnotationStyle` (`somem_apos_s`, 0 = nunca); o traço sai no instante exato `t_fim + N s` (temporizador único) | Spec §6.2; o 4C põe a opção em Ajustes. |
| Manifesto | `arquivos.anotacoes` {arquivo, largura, altura, origem?} e `anotacoes_queimadas` (só quando verdadeiro) | Spec §5.2/§6.3; mesmo formato de `arquivos.cursor`. |
| Testes seguros | `VOLTZ_TEST_LIVE` liga o fornecedor "voltz-teste-casca" (traços, sondagem do buffer, estado) e o plugin ganha `AnnotationFeedState`/`AddAnnotationSource`/`FeedStroke`/`FeedClear`; `VOLTZ_TEST_OVERLAY_RECT` põe a sobreposição fora da área de trabalho; `VOLTZ_TEST_ANNOT_BURNED` simula Windows antigo; `VOLTZ_TEST_SCREEN_LAYERS=1` cria grupos versão 1 (para testar a migração) — todas exigem `VOLTZ_TEST_LIVE` | Ninguém mexe no mouse/teclado; nada é capturado da tela; a janela de teste nem aparece para o usuário. |

**Respostas aos pontos obrigatórios:** (1) canal de traços: procedimentos `voltz_annot_*` + `AnnotationFeed` (linhas
"Canal" e "Render da fonte"); (2) coordenadas: pixels da captura da Tela, sobreposição estica para o monitor (linha
"Coordenadas"); (3) migração do grupo: versão de camadas 1 → 2 (linha "Tela Voltz", Task 6); (4) sobreposição e saída
garantida: no 4B ela **nunca** captura entrada (linha "Sobreposição"); o desenho das saídas garantidas do modo
interativo está no fim (Plano 4C); (5) testes seguros: seção abaixo + linha "Testes seguros".

---

## Segurança dos testes (o ponto mais crítico)

O PC é do usuário e ele o usa durante os testes. Uma janela topmost que capture o mouse bloquearia o uso do PC. Os
mecanismos abaixo são **obrigatórios** e cada um tem um ponto de código ou de script que o garante:

1. **A sobreposição não tem modo clicável no 4B.** `OverlayWindow` só tem `create/destroy/excludeFromCapture/setRect/
   render/pixelHex/info`; nasce com `WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW |
   WS_EX_TOPMOST`, responde `HTTRANSPARENT` ao `WM_NCHITTEST` e `MA_NOACTIVATE` ao `WM_MOUSEACTIVATE`, e cada `render`
   reconfere os bits obrigatórios (`ensureClickThrough`). Mesmo se o app travar, os bits continuam (não dependem da
   thread da UI). A Task 7 tem um passo que **confere por `grep`** que não há `SetWindowLongPtr` que tire bits nem
   `SetCapture`/`SetForegroundWindow`/`SetFocus` no arquivo.
2. **Nos testes a sobreposição fica fora da área de trabalho.** `VOLTZ_TEST_OVERLAY_RECT` (só com `VOLTZ_TEST_LIVE`) =
   um retângulo à direita do desktop virtual (`SM_XVIRTUALSCREEN + SM_CXVIRTUALSCREEN + 200`): o usuário não vê nada e
   nada fica por cima de janela alguma. O desenho dela é conferido no **buffer** (pedido `ProbeOverlay`), nunca por
   captura de tela.
3. **Depois de cada traço injetado, `Assert-OverlaySafe`** lê (P/Invoke só de leitura) o estilo de cada janela
   `VoltzAnnotationOverlay` do PID lançado; se faltar algum bit obrigatório ou se ela for a janela em primeiro plano,
   **encerra o app pelo PID** e aborta o teste.
4. **No fim de todo teste/smoke:** o app fecha pelo PID lançado (`CloseMainWindow`, e `Stop-Process -Id` só se não
   fechar em 30 s) e o script confere, por `IsWindow` em cada hwnd visto, que **nenhuma** sobreposição ficou.
5. **Traços só por pedidos de teste** (`InjectStroke`, `ExtendStroke`, `EndStroke`, `UndoStroke`, `ClearStrokes` do
   fornecedor "voltz-teste-casca"; `FeedStroke`/`FeedClear` do "voltz-teste"). **Nunca** `SendInput`, `SetCursorPos`,
   `mouse_event`, `keybd_event`, `SendKeys`, `[System.Windows.Forms.Cursor]::Position`, nem abrir menus popup/diálogos
   modais por automação (UI Automation só `Invoke` em botões da casca, como no 4A).
6. **Capturas:** render do programa só por `GetSourceScreenshot` de cena **sintética** (`Assert-NoRealCapture` aborta se
   existir `monitor_capture`/`window_capture`/`game_capture`/`dshow_input`); `PrintWindow` só da janela da casca lançada;
   afinidade conferida por `GetWindowDisplayAffinity` (API), nunca por captura de tela; nunca a tela inteira; nunca
   quadros de vídeo (só `ffprobe -show_entries`).
7. **Nada real ligado:** presets só com `VOLTZ_TEST_PRESET_STANDINS=1` (Tela e Câmera de cor); nunca webcam nem monitor
   reais; nunca `Stop-Process -Name`/`taskkill /IM` (só o PID lançado); não abrir o Explorer; nunca tocar em
   `%APPDATA%\Voltz Studio`, `%APPDATA%\obs-studio` nem no que já existe em `%USERPROFILE%\Videos`; config portátil
   com backup e restauração em `try/finally`.
8. **O modo interativo (4C) não existe aqui.** Se algum passo deste plano parecer exigir capturar a entrada, **pare e
   reporte** — não improvise.

---

## Convenções

**Diretório de trabalho:** a raiz do repositório (`voltz-studio`), branch `voltz`. Leia o `CLAUDE.md` inteiro antes
(seções **Decisões** e **Armadilhas**). Base deste plano: commit `4bfd03610`.

```bash
CMAKE="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
CTEST="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe"
CLANGFMT="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin/clang-format.exe"
```

- **CONFIGURAR:** `"$CMAKE" --preset voltz-windows-x64`
- **COMPILAR APP:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target obs-studio --parallel -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
  (o `obs-studio` depende de todos os módulos, então compila também o `voltz-sources`)
- **COMPILAR PLUGIN:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target voltz-sources -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
- **COMPILAR TUDO:** `"$CMAKE" --build --preset voltz-windows-x64 --parallel -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
- **COMPILAR TESTES:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target voltz-core-tests -- -v:minimal -nologo -nodeReuse:false`
- **RODAR TESTES:** `"$CTEST" --test-dir build_x64/frontend/voltz/tests -C RelWithDebInfo --output-on-failure`
  (contagem: `build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe` → linha `test cases: N | N passed`)
- **SMOKE:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1`
- **SMOKE GRAVAÇÃO:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-record.ps1`
- **SMOKE PROJETO:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-project.ps1 -OutDir "<scratchpad>\smoke-projeto"`
- **SMOKE MOUSE:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-mouse.ps1 -OutDir "<scratchpad>\smoke-mouse"`
- **SMOKE ANOTAÇÕES** (Task 10): `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-annotations.ps1 -OutDir "<scratchpad>\smoke-anotacoes"`
- **FORMATAR:** `"$CLANGFMT" -i <arquivos .cpp/.hpp novos ou alterados em frontend/voltz/ ou plugins/voltz-sources/>`
  (nunca nos arquivos do OBS; nunca em `.effect`/`.ini`/`.ps1`)

`<scratchpad>` = `C:\Users\Cassio\AppData\Local\Temp\claude\C--Users-Cassio-Documents-Projetos-IA-voltz-studio\eee63625-8704-4453-920b-073461dee2a9\scratchpad`.

> **Regras fixas:**
> - **Sem patch no OBS.** `plugins/CMakeLists.txt` não muda (o plugin entra pelo `feature-voltz.cmake`). Se algo se
>   mostrar impossível sem mexer em arquivo do OBS, pare e reporte; se o controlador autorizar, a mudança vai dentro de
>   `#ifdef VOLTZ_STUDIO` (ou com comentário "Voltz Studio" no CMake) e é registrada em `docs/voltz/upstream-patches.md`
>   **no mesmo commit**.
> - Builds longos **em segundo plano**, nunca dois ao mesmo tempo. `MSB4018`/`ZERO_CHECK`/"Touch" no `build_x86` →
>   rode de novo. Arquivo novo (código, locale, ícone no `.qrc`, pasta) exige **CONFIGURAR** antes de compilar.
> - **Warnings como erro** (`/W3` + `CMAKE_COMPILE_WARNING_AS_ERROR`) valem para tudo, inclusive plugin, `voltz-core` e
>   testes: casts explícitos em conversões numéricas (`double`→`int`, `size_t`→`int`, `int64_t`→`int`); `%zu` para
>   `size_t`, `%lld` + `static_cast<long long>`, `%lu` para `DWORD`/`GetLastError()` no `blog`.
> - O alvo `obs-studio` compila com `QT_NO_CONTEXTLESS_CONNECT`: todo `connect` com lambda leva objeto de contexto.
> - Sinais/callbacks do libobs e do websocket chegam **fora** da thread da UI → só atômicos/mutex ali; o resto via
>   `QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)`.
> - QSS da casca: prefixo `#voltzShell`. Ícones no código sempre por `phosphorIcon()`; ícone novo = SVG **oficial** do
>   Phosphor 2.1.1 baixado do unpkg com versão exata.
> - Textos novos nos **dois** locales da casca (`frontend/data/locale/voltz-pt-BR.ini` e `voltz-en-US.ini`, UTF-8 sem
>   BOM) e nos dois do plugin (`plugins/voltz-sources/data/locale/pt-BR.ini` e `en-US.ini`), com paridade:
>   `diff <(cut -d= -f1 frontend/data/locale/voltz-en-US.ini | sort) <(cut -d= -f1 frontend/data/locale/voltz-pt-BR.ini | sort)`
>   e `diff <(cut -d= -f1 plugins/voltz-sources/data/locale/en-US.ini | sort) <(cut -d= -f1 plugins/voltz-sources/data/locale/pt-BR.ini | sort)` → sem saída.
> - Textos de `blog` e literais C em ASCII (sem acento), como no resto do código Voltz.
> - **Não** commitar `build-compile.log` nem `docs/voltz/backlog-plano-*.md` fora da Task 11. **Push só na Task 11**,
>   nunca `--force`.
>
> **Regras de segurança dos testes ao vivo (releia a seção "Segurança dos testes" antes de cada teste):** nunca mexer
> no mouse/teclado do usuário; traços só por pedidos de teste; sobreposição de teste sempre fora da área de trabalho
> (`$overlayOffscreen = $true` no bloco abaixo) e conferida por `Assert-OverlaySafe` depois de cada traço; app só pelo
> PID lançado; cenas só sintéticas; só metadados de vídeo; config portátil com backup; `Vídeos` intocada.
> PowerShell 5.1 lê `.ps1` sem BOM como ANSI: script do scratchpad com acento precisa de **BOM**; scripts versionados em
> `tools/` ficam em ASCII e montam acentos com `[char]0xF3` etc.

**Commits:** mensagem em PT-BR terminando com a linha `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

**Bloco de teste (PowerShell 5.1)** — copie para um `.ps1` **com BOM** no scratchpad, ajuste as variáveis marcadas e
ponha os passos da task no `try`. Ele guarda a config portátil, cria uma limpa (obs-websocket na porta 4455 sem
senha, pt-BR, sem confirmação ao sair, `HideOBSWindowsFromCapture=false` para o `PrintWindow`), liga os modos de
teste, calcula o retângulo de teste da sobreposição **fora da área de trabalho**, cria `gravacoes` no scratchpad e, no
fim, fecha o app pelo PID, confere que nenhuma sobreposição ficou e restaura/apaga tudo. **Valide dentro do `try`,
depois de `Close-App` + `Save-Log`: o `finally` apaga `$out`.**
```powershell
$root = "C:\Users\Cassio\Documents\Projetos IA\voltz-studio"
$rundir = Join-Path $root "build_x64\rundir\RelWithDebInfo"; $bin = Join-Path $rundir "bin\64bit"
$sp = "C:\Users\Cassio\AppData\Local\Temp\claude\C--Users-Cassio-Documents-Projetos-IA-voltz-studio\eee63625-8704-4453-920b-073461dee2a9\scratchpad"
$out = Join-Path $sp "gravacoes"
$cfg = Join-Path $rundir "config"; $bak = Join-Path $rundir "config.4bbak"
if (Test-Path $bak) { throw "backup anterior em $bak - restaure (Move-Item) antes de continuar" }
if (Test-Path $cfg) { Move-Item $cfg $bak }
$voltzCfg = Join-Path $cfg "Voltz Studio"
# ---------- ajuste por task ----------
$liveTest = $true          # VOLTZ_TEST_LIVE: pedidos de teste ligados; o mouse real nunca e lido
$standins = $true          # VOLTZ_TEST_PRESET_STANDINS: Tela/Camera de COR (nunca monitor/webcam)
$overlayOffscreen = $true  # VOLTZ_TEST_OVERLAY_RECT: sobreposicao FORA da area de trabalho (o usuario nao ve)
$burned = $false           # VOLTZ_TEST_ANNOT_BURNED: anotacoes "queimadas" (simula Windows < 10 2004)
$layersV1 = $false         # VOLTZ_TEST_SCREEN_LAYERS=1: presets criam a Tela Voltz versao 1 (sem anotacoes)
$voltzIni = "NextProjectName=Teste 4B`r`n"   # linhas da secao [Voltz] (ex.: 'AnnotationStyle={"somem_apos_s":1}')
# -------------------------------------
function New-CleanConfig {
  if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
  $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
  New-Item -ItemType Directory -Force $wsDir | Out-Null
  [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}')
  $ini = "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[BasicWindow]`r`nHideOBSWindowsFromCapture=false`r`n[Voltz]`r`n" + $voltzIni
  [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), $ini, (New-Object Text.UTF8Encoding $false)) }
New-CleanConfig
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class W { [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")] public static extern IntPtr GetWindowLongPtr(IntPtr h, int index);
 [DllImport("user32.dll")] public static extern bool GetWindowDisplayAffinity(IntPtr h, out uint affinity);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index); }
'@
[void][W]::SetProcessDPIAware()
# Retangulo de teste da sobreposicao: 640x360 A DIREITA de todo o desktop virtual (ninguem ve, nada fica por cima)
$vx = [W]::GetSystemMetrics(76); $vy = [W]::GetSystemMetrics(77); $vw = [W]::GetSystemMetrics(78)
$overlayRect = "{0},{1},640,360" -f ($vx + $vw + 200), $vy
$envs = [ordered]@{ VOLTZ_TEST_LIVE = $liveTest; VOLTZ_TEST_PRESET_STANDINS = $standins; VOLTZ_TEST_ANNOT_BURNED = $burned; VOLTZ_TEST_SCREEN_LAYERS = $layersV1 }
foreach ($k in $envs.Keys) { if ($envs[$k]) { Set-Item "Env:$k" "1" } else { Remove-Item "Env:$k" -ErrorAction SilentlyContinue } }
if ($overlayOffscreen) { $env:VOLTZ_TEST_OVERLAY_RECT = $overlayRect } else { Remove-Item Env:VOLTZ_TEST_OVERLAY_RECT -ErrorAction SilentlyContinue }
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"; $hadVoltzVideos = Test-Path -LiteralPath $voltzVideos
# Captura SO da janela do app (nunca da tela inteira)
function Shot([IntPtr]$h, [string]$n) { $r = New-Object W+RECT; [void][W]::GetWindowRect($h, [ref]$r)
  $b = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [System.Drawing.Graphics]::FromImage($b)
  $d = $g.GetHdc(); [void][W]::PrintWindow($h, $d, 2); $g.ReleaseHdc($d); $g.Dispose(); $b.Save("$sp\$n.png"); $b.Dispose() }
# Redimensiona SO a janela da casca lancada (SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)
function Resize-Shell([int]$w, [int]$h) { [void][W]::SetWindowPos($script:shell, [IntPtr]::Zero, 0, 0, $w, $h, 0x16); Start-Sleep 1 }
$uia = Join-Path $root "tools\voltz-uia.ps1"; $ws = Join-Path $root "tools\voltz-ws.ps1"
function Ws([string]$json, [switch]$Quiet) { $f = Join-Path $sp "req.json"
  [IO.File]::WriteAllText($f, $json, (New-Object Text.UTF8Encoding $false))
  $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $f
  if (-not $Quiet) { $lines | ForEach-Object { Write-Host $_ } }
  return ,@($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json }) }
function Uia([string]$name) { & powershell -ExecutionPolicy Bypass -File $uia -ProcessId $script:proc.Id -Name $name | Out-Host; return ($LASTEXITCODE -eq 0) }
$script:overlays = @()
function Start-App { $script:proc = Start-Process (Join-Path $bin "VoltzStudio.exe") -ArgumentList '--portable','--multi' -WorkingDirectory $bin -PassThru
  Start-Sleep 15; $script:proc.Refresh(); $script:shell = $script:proc.MainWindowHandle }
function Close-App { if ($script:proc -and -not $script:proc.HasExited) { $script:proc.Refresh(); [void]$script:proc.CloseMainWindow()
  if (-not $script:proc.WaitForExit(30000)) { Stop-Process -Id $script:proc.Id -Force } } }
function Save-Log { $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
  if ($log) { Copy-Item $log.FullName "$sp\ultimo-log.txt" -Force } }
function LogLines([string]$pattern) { if (Test-Path "$sp\ultimo-log.txt") { Select-String -LiteralPath "$sp\ultimo-log.txt" -Encoding UTF8 -SimpleMatch -Pattern $pattern | ForEach-Object { $_.Line } } }
# Fornecedores de teste: "voltz-teste" (plugin) e "voltz-teste-casca" (frontend); so existem com VOLTZ_TEST_LIVE=1
function Vendor([string]$type, [string]$dataJson = '{}', [string]$vendorName = 'voltz-teste') {
  $r = Ws ('[{"requestType":"CallVendorRequest","requestData":{"vendorName":"' + $vendorName + '","requestType":"' + $type + '","requestData":' + $dataJson + '}}]') -Quiet
  if ($r.Count -lt 1 -or -not $r[0].ok) { throw "pedido de teste $vendorName/$type falhou" }
  return $r[0].data.responseData }
function VendorShell([string]$type, [string]$dataJson = '{}') { return (Vendor $type $dataJson 'voltz-teste-casca') }
# Os pedidos da casca sao enfileirados para a thread da UI: espera um pouco antes de ler o estado
function Shell-State { Start-Sleep -Milliseconds 400; return ((VendorShell "AnnotationState").estado | ConvertFrom-Json) }
function Feed-State { return ((Vendor "AnnotationFeedState").estado | ConvertFrom-Json) }
function Stroke-Json([string]$tool, [string]$color, [int]$width, [string]$points, [switch]$Open) {
  $req = [ordered]@{ ferr = $tool; cor = $color; esp = $width; pts = $points }; if ($Open) { $req.aberto = $true }
  return (ConvertTo-Json $req -Compress) }
# Janelas de sobreposicao do PID lancado (P/Invoke so de leitura)
function Overlay-Windows([int]$procId) { $list = @(); $h = [IntPtr]::Zero
  while ($true) { $h = [W]::FindWindowEx([IntPtr]::Zero, $h, "VoltzAnnotationOverlay", $null); if ($h -eq [IntPtr]::Zero) { break }
    [uint32]$owner = 0; [void][W]::GetWindowThreadProcessId($h, [ref]$owner); if ($owner -eq $procId) { $list += $h } }
  return ,$list }
# Seguranca: toda sobreposicao do app precisa ser transparente a cliques e nunca estar em primeiro plano; se nao, o app
# (PID lancado) e encerrado na hora. WS_EX_TOPMOST|TRANSPARENT|TOOLWINDOW|LAYERED|NOACTIVATE = 0x080800A8.
function Assert-OverlaySafe { foreach ($h in (Overlay-Windows $script:proc.Id)) {
    if ($script:overlays -notcontains $h.ToInt64()) { $script:overlays += $h.ToInt64() }
    $ex = [W]::GetWindowLongPtr($h, -20).ToInt64()
    if ((($ex -band 0x080800A8) -ne 0x080800A8) -or ([W]::GetForegroundWindow() -eq $h)) {
      Stop-Process -Id $script:proc.Id -Force; throw ("sobreposicao insegura (estilo 0x{0:X}) - app encerrado" -f $ex) } } }
# Seguranca: nenhuma captura real no app (tela, janela, jogo, webcam)
function Assert-NoRealCapture { $inputs = @((Ws '[{"requestType":"GetInputList"}]' -Quiet)[0].data.inputs)
  $real = @($inputs | Where-Object { $_.inputKind -match 'monitor_capture|window_capture|game_capture|dshow_input' })
  if ($real.Count -gt 0) { throw ("captura real presente - abortando: " + (($real | ForEach-Object { $_.inputName }) -join ', ')) } }
# Render de uma cena SINTETICA em 640x360 - nunca tela real nem quadro de video
function Scene-Image([string]$scene) { Assert-NoRealCapture
  $r = Ws ('[{"requestType":"GetSourceScreenshot","requestData":{"sourceName":' + (ConvertTo-Json $scene) + ',"imageFormat":"png","imageWidth":640,"imageHeight":360}}]') -Quiet
  if ($r.Count -lt 1 -or -not $r[0].ok) { throw "GetSourceScreenshot falhou para '$scene'" }
  $bytes = [Convert]::FromBase64String(($r[0].data.imageData -replace '^data:image/png;base64,', ''))
  return [System.Drawing.Bitmap]::FromStream((New-Object IO.MemoryStream (,$bytes))) }
function Near($c, [int]$r, [int]$g, [int]$b, [int]$tol = 24) { ([Math]::Abs($c.R - $r) -le $tol) -and ([Math]::Abs($c.G - $g) -le $tol) -and ([Math]::Abs($c.B - $b) -le $tol) }
# Camadas do (primeiro) grupo de uma cena
function Screen-Layers([string]$scene) {
  $top = @((Ws ('[{"requestType":"GetSceneItemList","requestData":{"sceneName":' + (ConvertTo-Json $scene) + '}}]') -Quiet)[0].data.sceneItems)
  $group = @($top | Where-Object { $_.isGroup } | Select-Object -First 1)
  if ($group.Count -lt 1) { return ,@() }
  return ,@((Ws ('[{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":' + (ConvertTo-Json ([string]$group[0].sourceName)) + '}}]') -Quiet)[0].data.sceneItems) }
# user.ini [Voltz] com o app FECHADO (o app regrava o arquivo ao sair)
function Set-VoltzIni([string]$key, [string]$value) { $path = Join-Path $voltzCfg "user.ini"
  $lines = [System.Collections.Generic.List[string]]@(Get-Content -LiteralPath $path -Encoding UTF8)
  $start = $lines.IndexOf("[Voltz]"); if ($start -lt 0) { $lines.Add("[Voltz]"); $start = $lines.Count - 1 }
  $end = $lines.Count; for ($i = $start + 1; $i -lt $lines.Count; $i++) { if ($lines[$i].StartsWith("[")) { $end = $i; break } }
  $done = $false; for ($i = $start + 1; $i -lt $end; $i++) { if ($lines[$i].StartsWith("$key=")) { $lines[$i] = "$key=$value"; $done = $true } }
  if (-not $done) { $lines.Insert($end, "$key=$value") }
  [IO.File]::WriteAllLines($path, $lines, (New-Object Text.UTF8Encoding $false)) }
function Project-Dirs { @(Get-ChildItem -LiteralPath $out -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz") } | Sort-Object CreationTime) }
function Read-Json([string]$path) { (Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json) | ForEach-Object { $_ } }
function Read-Jsonl([string]$path) { ,@(Get-Content -LiteralPath $path -Encoding UTF8 | Where-Object { $_ } | ForEach-Object { $_ | ConvertFrom-Json }) }
$ffprobe = (Get-Command ffprobe -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $ffprobe) { $ffprobe = Get-ChildItem (Join-Path $root ".deps") -Filter ffprobe.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName }
# So metadados (nunca quadros)
function Probe([string]$file) { (& $ffprobe -v error -show_entries "stream=codec_type,codec_name,width,height:format=duration" -of json $file) | Out-String | ConvertFrom-Json }
$proc = $null
try {
  Start-App
  # ... passos da task (validacao no fim: Close-App; Save-Log; ...) ...
} finally {
  Close-App
  Save-Log
  foreach ($h in $script:overlays) { if ([W]::IsWindow([IntPtr]$h)) { Write-Host "FALHOU sobreposicao orfa hwnd=$h" } else { Write-Host "ok: sobreposicao $h destruida" } }
  if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
  if (Test-Path $bak) { Move-Item $bak $cfg }
  Remove-Item Env:VOLTZ_TEST_LIVE, Env:VOLTZ_TEST_PRESET_STANDINS, Env:VOLTZ_TEST_OVERLAY_RECT, Env:VOLTZ_TEST_ANNOT_BURNED, Env:VOLTZ_TEST_SCREEN_LAYERS -ErrorAction SilentlyContinue
  if (Test-Path $out) { Remove-Item -Recurse -Force $out }   # apaga gravacoes de teste sem abrir
  if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) { Remove-Item -Force -LiteralPath $voltzVideos }
}
```

**Preparo de gravação** (dentro do `try`, depois de aplicar um preset com `$standins = $true`): aponta a pasta de
gravações para `$out` e muta o áudio global.
```powershell
$r = Ws ('[{"requestType":"SetProfileParameter","requestData":{"parameterCategory":"SimpleOutput","parameterName":"FilePath","parameterValue":' + (ConvertTo-Json $out) + '}},{"requestType":"GetSpecialInputs"}]')
$special = $r[1].data
$mutes = @(); foreach ($n in @($special.desktop1, $special.desktop2, $special.mic1, $special.mic2, $special.mic3, $special.mic4)) { if ($n) { $mutes += @{ requestType = "SetInputMute"; requestData = @{ inputName = $n; inputMuted = $true } } } }
if ($mutes.Count -gt 0) { [void](Ws (ConvertTo-Json -InputObject $mutes -Depth 5 -Compress)) }
```

**Aplicar o preset "Só tela / Só câmera"** (primeira abertura com config limpa = página Presets): `Uia "Só tela / Só
câmera — Aplicar"`; depois `Start-Sleep 3`, `Assert-NoRealCapture` e
`[void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Só tela"}}]')`.

**Cores e pixels de referência:** Tela substituta = `#2A2A30` → (42,42,48); lima `#D4F53C` → (212,245,60). A Tela
2560×1440 cabe no canvas 1920×1080 com escala 0,75 e a `Scene-Image` tem 640×360 ⇒ pixel (x,y) da captura ≈
(x/4, y/4) na imagem. Linha lima de esp 16 de (640,720) a (1920,720) → faixa de 4 px em y=180, x=160..480. Marca-texto
`#FF4D4D` esp 8 (24 px, 40%) sobre a Tela → (127,56,60). Na sobreposição de teste (640×360 para 2560×1440, escala
0,25) os mesmos pontos caem em (320,180) e (320,270); lima opaco = `#FFD4F53C`; marca-texto = alfa `0x66` (102).
Tolerância 24 por canal (`Near`).

---

## Mapa de arquivos

**Novos — núcleo (`frontend/voltz/core/`, sem Qt/libobs) e testes**
| Arquivo | Responsabilidade |
|---|---|
| `core/annotation-model.hpp/.cpp` | ferramentas, `StrokePoint`, `Stroke`, regra de acrescentar pontos, JSON dos pontos e da linha do `anotacoes.jsonl`, `AnnotationStyle` (`user.ini [Voltz] AnnotationStyle`) |
| `core/annotation-raster.hpp/.cpp` | forma do traço (cápsulas + triângulo da seta), `RasterImage` BGRA pré-multiplicado, `clearRaster`, `rasterizeStroke` |
| `core/stroke-store.hpp/.cpp` | `LiveStroke`, `RemovedStroke`, `StrokeStore` (começar/acrescentar/terminar/desfazer/limpar/somem após) |
| `core/annotation-journal.hpp/.cpp` | `AnnotationJournal` (linhas do diário durante a gravação) e `consolidateAnnotations` |
| `tests/test-annotation-model.cpp`, `tests/test-annotation-raster.cpp`, `tests/test-stroke-store.cpp`, `tests/test-annotation-journal.cpp` | doctest do acima |

**Novos — plugin (`plugins/voltz-sources/`)**
| Arquivo | Responsabilidade |
|---|---|
| `target-size.hpp/.cpp` | `TargetSize`: tamanho da captura "alvo" (extraído da Voltz Mouse, usado pelas duas fontes) |
| `annotation-feed.hpp/.cpp` | `AnnotationFeed` (traços ao vivo com mutex e versão, "queimadas", contagem de mostradas) e os 5 procedimentos `voltz_annot_*` |
| `annotation-source.hpp/.cpp` | fonte `voltz_annotations_source` ("Voltz Anotações"): rasteriza na mudança, desenha a textura |

**Novos — frontend (alvo `obs-studio`) e ferramentas**
| Arquivo | Responsabilidade |
|---|---|
| `annotate/OverlayWindow.hpp/.cpp` | janela Win32 de sobreposição, sempre transparente a cliques, fora da captura, desenho por `UpdateLayeredWindow` |
| `annotate/AnnotationController.hpp/.cpp` | dono do `StrokeStore`; plugin, sobreposição, "queimadas", "somem após", sinais para o `ProjectRecorder`, estado de teste |
| `annotate/AnnotationTestVendor.hpp/.cpp` | fornecedor "voltz-teste-casca" (só `VOLTZ_TEST_LIVE`): `InjectStroke`, `ExtendStroke`, `EndStroke`, `UndoStroke`, `ClearStrokes`, `ProbeOverlay`, `AnnotationState` |
| `shell/icons/regular/pencil-simple.svg` | ícone da Voltz Anotações na lista de Fontes (Phosphor 2.1.1) |
| `tools/voltz-smoke-annotations.ps1` | smoke do 4B (camadas, render, sobreposição, desfazer/limpar, gravação, migração 1→2, queimadas, somem após) |
| `tools/voltz-annot-demo.ps1` | demonstração MANUAL (o usuário roda e olha) com a Tela real |

**Modificados (Voltz):** `frontend/voltz/core/live-protocol.hpp`, `frontend/voltz/core/project-manifest.hpp/.cpp`,
`frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`, `frontend/voltz/tests/test-project-manifest.cpp`,
`plugins/voltz-sources/CMakeLists.txt`, `plugins/voltz-sources/voltz-sources.cpp`, `plugins/voltz-sources/mouse-source.cpp`,
`plugins/voltz-sources/test-vendor.hpp/.cpp`, `plugins/voltz-sources/data/locale/en-US.ini`, `.../pt-BR.ini`,
`frontend/cmake/feature-voltz.cmake`, `frontend/voltz/shell/SharedSources.hpp/.cpp`, `frontend/voltz/shell/PresetBuilder.cpp`,
`frontend/voltz/shell/SourcesPanel.cpp`, `frontend/voltz/shell/VoltzShell.hpp/.cpp`, `frontend/voltz/shell/voltz-shell.qrc`,
`frontend/voltz/project/ProjectRecorder.hpp/.cpp`, `frontend/voltz/project/ProjectLibrary.cpp`,
`frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`, `tools/voltz-smoke-mouse.ps1`; na Task
11: `docs/voltz/backlog-plano-4.md`, `CLAUDE.md`, `AGENTS.md`, `docs/HISTORICO.md`.

**Modificados (OBS upstream):** nenhum. (`plugins/CMakeLists.txt` e `docs/voltz/upstream-patches.md` não mudam; a Task
11 confere.)

**Contagem de testes:** 126 (base) → 133 (Task 1) → 141 (Task 2) → 151 (Task 3) → 153 (Task 4).

---
### Task 1: Núcleo — modelo do traço, pontos, estilo das anotações e protocolo

**Files:**
- Create: `frontend/voltz/core/annotation-model.hpp`, `frontend/voltz/core/annotation-model.cpp`
- Create: `frontend/voltz/tests/test-annotation-model.cpp`
- Modify: `frontend/voltz/core/live-protocol.hpp`, `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

- [ ] **Step 1: Escrever o teste que falha**

Criar `frontend/voltz/tests/test-annotation-model.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/live-protocol.hpp>

#include <string>
#include <vector>

using voltz::AnnotationTool;
using voltz::RgbColor;
using voltz::Stroke;
using voltz::StrokePoint;

TEST_CASE("toolToString/toolFromString: as seis ferramentas e texto desconhecido")
{
	const AnnotationTool tools[] = {AnnotationTool::Pen,   AnnotationTool::Highlighter, AnnotationTool::Line,
					AnnotationTool::Arrow, AnnotationTool::Rectangle,   AnnotationTool::Ellipse};
	const char *names[] = {"caneta", "marca_texto", "linha", "seta", "retangulo", "elipse"};
	for (int i = 0; i < 6; i++) {
		CHECK(std::string(voltz::toolToString(tools[i])) == names[i]);
		AnnotationTool parsed = AnnotationTool::Pen;
		CHECK(voltz::toolFromString(names[i], parsed));
		CHECK(parsed == tools[i]);
	}
	AnnotationTool unchanged = AnnotationTool::Arrow;
	CHECK_FALSE(voltz::toolFromString("texto", unchanged));
	CHECK(unchanged == AnnotationTool::Arrow);
	CHECK_FALSE(voltz::isShapeTool(AnnotationTool::Pen));
	CHECK_FALSE(voltz::isShapeTool(AnnotationTool::Highlighter));
	CHECK(voltz::isShapeTool(AnnotationTool::Ellipse));
}

TEST_CASE("strokeToJsonLine: formato da spec, sem t_fim/t_limpo enquanto -1")
{
	Stroke stroke;
	stroke.id = 7;
	stroke.tool = AnnotationTool::Arrow;
	stroke.width = 4;
	stroke.points = {{10, 20}, {30, 40}};
	stroke.startMs = 1200;
	CHECK(voltz::strokeToJsonLine(stroke) ==
	      R"({"id":7,"ferr":"seta","cor":"#D4F53C","esp":4,"pts":[[10,20],[30,40]],"t_ini":1200})");
	stroke.endMs = 1850;
	stroke.clearedMs = 9000;
	CHECK(voltz::strokeToJsonLine(stroke) ==
	      R"({"id":7,"ferr":"seta","cor":"#D4F53C","esp":4,"pts":[[10,20],[30,40]],"t_ini":1200,"t_fim":1850,"t_limpo":9000})");
}

TEST_CASE("strokeFromJson: ida e volta, leitura tolerante e recusas")
{
	Stroke stroke;
	stroke.id = 3;
	stroke.tool = AnnotationTool::Highlighter;
	stroke.color = RgbColor{0xFF, 0x4D, 0x4D};
	stroke.width = 8;
	stroke.points = {{1, 2}, {3, 4}, {5, 6}};
	stroke.startMs = 10;
	stroke.endMs = 20;
	Stroke parsed;
	REQUIRE(voltz::strokeFromJson(voltz::strokeToJsonLine(stroke), parsed));
	CHECK(parsed.id == 3);
	CHECK(parsed.tool == AnnotationTool::Highlighter);
	CHECK(parsed.color == stroke.color);
	CHECK(parsed.width == 8);
	REQUIRE(parsed.points.size() == 3);
	CHECK(parsed.points[2] == StrokePoint{5, 6});
	CHECK(parsed.startMs == 10);
	CHECK(parsed.endMs == 20);
	CHECK(parsed.clearedMs == -1);

	Stroke loose;
	REQUIRE(voltz::strokeFromJson(
		R"({"id":9,"ferr":"linha","cor":"azul","esp":500,"pts":[[1.6,2],"x",[3]],"extra":true})", loose));
	CHECK(loose.color == RgbColor{0xD4, 0xF5, 0x3C});
	CHECK(loose.width == voltz::kStrokeWidthMax);
	REQUIRE(loose.points.size() == 1);
	CHECK(loose.points[0] == StrokePoint{2, 2});
	CHECK(loose.startMs == 0);

	Stroke untouched;
	untouched.id = 42;
	CHECK_FALSE(voltz::strokeFromJson("nao e json", untouched));
	CHECK_FALSE(voltz::strokeFromJson(R"({"ferr":"seta"})", untouched));
	CHECK_FALSE(voltz::strokeFromJson(R"({"id":1,"ferr":"texto"})", untouched));
	CHECK_FALSE(voltz::strokeFromJson("[1,2]", untouched));
	CHECK(untouched.id == 42);
}

TEST_CASE("appendStrokePoints: caneta acumula sem repetir; formas guardam primeiro e último")
{
	Stroke pen;
	voltz::appendStrokePoints(pen, {{1, 1}, {1, 1}, {2, 2}});
	voltz::appendStrokePoints(pen, {{2, 2}, {3, 3}});
	REQUIRE(pen.points.size() == 3);
	CHECK(pen.points[2] == StrokePoint{3, 3});

	Stroke arrow;
	arrow.tool = AnnotationTool::Arrow;
	voltz::appendStrokePoints(arrow, {{10, 10}});
	CHECK(arrow.points.size() == 1);
	voltz::appendStrokePoints(arrow, {{20, 20}, {30, 35}});
	voltz::appendStrokePoints(arrow, {{40, 45}});
	REQUIRE(arrow.points.size() == 2);
	CHECK(arrow.points[0] == StrokePoint{10, 10});
	CHECK(arrow.points[1] == StrokePoint{40, 45});

	Stroke full;
	std::vector<StrokePoint> many;
	for (int i = 0; i < static_cast<int>(voltz::kMaxStrokePoints) + 5; i++) {
		many.push_back(StrokePoint{i, 0});
	}
	voltz::appendStrokePoints(full, many);
	CHECK(full.points.size() == voltz::kMaxStrokePoints);
}

TEST_CASE("pointsToJson/pointsFromJson")
{
	CHECK(voltz::pointsToJson({{1, 2}, {-3, 4}}) == "[[1,2],[-3,4]]");
	CHECK(voltz::pointsToJson({}) == "[]");
	std::vector<StrokePoint> points;
	REQUIRE(voltz::pointsFromJson("[[5,6],[7,8]]", points));
	REQUIRE(points.size() == 2);
	CHECK(points[1] == StrokePoint{7, 8});
	CHECK_FALSE(voltz::pointsFromJson("{\"x\":1}", points));
	CHECK_FALSE(voltz::pointsFromJson("lixo", points));
	CHECK(points.size() == 2);
}

TEST_CASE("AnnotationStyle: padrão, limites e JSON")
{
	const voltz::AnnotationStyle style;
	CHECK(voltz::annotationStyleToJson(style) == R"({"ferr":"caneta","cor":"#D4F53C","esp":4,"somem_apos_s":0})");
	voltz::AnnotationStyle parsed;
	REQUIRE(voltz::annotationStyleFromJson(R"({"ferr":"seta","cor":"#4DA3FF","esp":0,"somem_apos_s":999})", parsed));
	CHECK(parsed.tool == AnnotationTool::Arrow);
	CHECK(parsed.color == RgbColor{0x4D, 0xA3, 0xFF});
	CHECK(parsed.width == voltz::kStrokeWidthMin);
	CHECK(parsed.fadeSeconds == voltz::kFadeSecondsMax);
	REQUIRE(voltz::annotationStyleFromJson(R"({"somem_apos_s":3})", parsed));
	CHECK(parsed.tool == AnnotationTool::Pen);
	CHECK(parsed.fadeSeconds == 3);
	CHECK_FALSE(voltz::annotationStyleFromJson("[]", parsed));
	CHECK(parsed.fadeSeconds == 0);
}

TEST_CASE("live-protocol: procedimentos das anotações declarados com o próprio nome")
{
	const char *pairs[][2] = {{voltz::live::kProcAnnotBegin, voltz::live::kDeclAnnotBegin},
				  {voltz::live::kProcAnnotExtend, voltz::live::kDeclAnnotExtend},
				  {voltz::live::kProcAnnotRemove, voltz::live::kDeclAnnotRemove},
				  {voltz::live::kProcAnnotClear, voltz::live::kDeclAnnotClear},
				  {voltz::live::kProcAnnotBurned, voltz::live::kDeclAnnotBurned}};
	for (const auto &pair : pairs) {
		CHECK(std::string(pair[1]).find(std::string(" ") + pair[0] + "(") != std::string::npos);
	}
	CHECK(std::string(voltz::live::kAnnotationsSourceId) == "voltz_annotations_source");
}
```
Em `frontend/voltz/tests/CMakeLists.txt`, na lista de `target_sources`, antes de `    test-atomic-file.cpp`,
acrescentar a linha `    test-annotation-model.cpp`.

- [ ] **Step 2: Rodar e ver falhar**

Run: CONFIGURAR e depois COMPILAR TESTES.
Expected: FAIL na compilação — `Cannot open include file: 'voltz/core/annotation-model.hpp'`.

- [ ] **Step 3: Protocolo das anotações**

Em `frontend/voltz/core/live-protocol.hpp`, trocar o bloco final:
```cpp
// Só testes: com VOLTZ_TEST_LIVE o plugin registra o fornecedor "voltz-teste" no obs-websocket (InjectMouse,
// MouseState, SetStyle) e o frontend não lê o mouse real.
constexpr const char *kTestEnv = "VOLTZ_TEST_LIVE";
constexpr const char *kTestVendor = "voltz-teste";
```
por:
```cpp
// Anotações (Plano 4B): fonte Voltz Anotações e procedimentos frontend → plugin. `json` = strokeToJsonLine (vale o `id`
// do procedimento); `points` = pointsToJson, acrescentados com appendStrokePoints (a mesma regra do StrokeStore).
// `burned` = anotações "queimadas" na captura (spec §6.3): a fonte não desenha, para não duplicar.
constexpr const char *kAnnotationsSourceId = "voltz_annotations_source";
constexpr const char *kProcAnnotBegin = "voltz_annot_begin";
constexpr const char *kDeclAnnotBegin = "void voltz_annot_begin(in int id, in string json)";
constexpr const char *kProcAnnotExtend = "voltz_annot_extend";
constexpr const char *kDeclAnnotExtend = "void voltz_annot_extend(in int id, in string points)";
constexpr const char *kProcAnnotRemove = "voltz_annot_remove";
constexpr const char *kDeclAnnotRemove = "void voltz_annot_remove(in int id)";
constexpr const char *kProcAnnotClear = "voltz_annot_clear";
constexpr const char *kDeclAnnotClear = "void voltz_annot_clear()";
constexpr const char *kProcAnnotBurned = "voltz_annot_burned";
constexpr const char *kDeclAnnotBurned = "void voltz_annot_burned(in bool burned)";

// Só testes: com VOLTZ_TEST_LIVE o plugin registra o fornecedor "voltz-teste" no obs-websocket (InjectMouse,
// MouseState, SetStyle, AddMouseSource e, no 4B, AnnotationFeedState, AddAnnotationSource, FeedStroke, FeedClear), o
// frontend registra "voltz-teste-casca" (traços e estado das anotações) e o mouse real não é lido. As variáveis abaixo
// só valem junto com VOLTZ_TEST_LIVE: retângulo da sobreposição "x,y,largura,altura" (pixels físicos) no lugar do
// monitor da Tela; anotações "queimadas" forçadas (como num Windows < 10 2004).
constexpr const char *kTestEnv = "VOLTZ_TEST_LIVE";
constexpr const char *kTestVendor = "voltz-teste";
constexpr const char *kTestVendorShell = "voltz-teste-casca";
constexpr const char *kTestOverlayRectEnv = "VOLTZ_TEST_OVERLAY_RECT";
constexpr const char *kTestBurnedEnv = "VOLTZ_TEST_ANNOT_BURNED";
```
(`testModeEnabled()` continua igual, logo abaixo.)

- [ ] **Step 4: Cabeçalho do modelo**

Criar `frontend/voltz/core/annotation-model.hpp`:
```cpp
#pragma once

#include "mouse-style.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Desenho na tela (spec §6.2). Ferramentas do Plano 4B; o texto entra no 4C.
enum class AnnotationTool { Pen, Highlighter, Line, Arrow, Rectangle, Ellipse };

// "caneta" | "marca_texto" | "linha" | "seta" | "retangulo" | "elipse" (campo "ferr" do anotacoes.jsonl).
const char *toolToString(AnnotationTool tool);
// Falso (e `out` intacto) para texto desconhecido.
bool toolFromString(const std::string &text, AnnotationTool &out);
// Linha, seta, retângulo e elipse: só o primeiro e o último ponto contam.
bool isShapeTool(AnnotationTool tool);

// Pixel da captura da Tela (as coordenadas de tela.mkv e do cursor.jsonl).
struct StrokePoint {
	int x = 0;
	int y = 0;
};

inline bool operator==(const StrokePoint &a, const StrokePoint &b)
{
	return a.x == b.x && a.y == b.y;
}

constexpr int kStrokeWidthMin = 1;
constexpr int kStrokeWidthMax = 48;
constexpr size_t kMaxStrokePoints = 20000;
constexpr int kFadeSecondsMax = 60;

int clampStrokeWidth(int width);

// Um traço. Tempos em ms relativos ao t0 do projeto (spec §5.5); -1 = ainda não aconteceu.
struct Stroke {
	int id = 0;
	AnnotationTool tool = AnnotationTool::Pen;
	RgbColor color{0xD4, 0xF5, 0x3C};
	int width = 4;
	std::vector<StrokePoint> points;
	int64_t startMs = 0;
	int64_t endMs = -1;
	int64_t clearedMs = -1;
};

// Acrescenta pontos. Caneta/marca-texto: no fim, sem repetir o último e até kMaxStrokePoints. Formas: o primeiro ponto
// fica e o segundo passa a ser o último recebido (no máximo 2 pontos).
void appendStrokePoints(Stroke &stroke, const std::vector<StrokePoint> &points);

// [[x,y],[x,y]] compacto.
std::string pointsToJson(const std::vector<StrokePoint> &points);
// Falso (e `out` intacto) se não for uma lista; itens que não são [número, número] são ignorados.
bool pointsFromJson(const std::string &json, std::vector<StrokePoint> &out);

// Linha do anotacoes.jsonl, sem "\n" (spec §6.2):
// {"id":7,"ferr":"seta","cor":"#D4F53C","esp":4,"pts":[[x,y],...],"t_ini":1200,"t_fim":1850,"t_limpo":9000}
// t_fim e t_limpo ficam de fora quando -1.
std::string strokeToJsonLine(const Stroke &stroke);
// Leitura tolerante: precisa de objeto com "id" inteiro e "ferr" conhecido; cor inválida = lima; espessura limitada;
// tempos ausentes = 0 (t_ini) e -1 (t_fim, t_limpo). Falso deixa `out` intacto.
bool strokeFromJson(const std::string &json, Stroke &out);

// Estilo das anotações (user.ini [Voltz] AnnotationStyle): ferramenta, cor e espessura do próximo traço e "traços somem
// após N s" (0 = nunca). {"ferr":"caneta","cor":"#D4F53C","esp":4,"somem_apos_s":0}
struct AnnotationStyle {
	AnnotationTool tool = AnnotationTool::Pen;
	RgbColor color{0xD4, 0xF5, 0x3C};
	int width = 4;
	int fadeSeconds = 0;
};

AnnotationStyle clampAnnotationStyle(AnnotationStyle style);
std::string annotationStyleToJson(const AnnotationStyle &style);
// `out` recebe o padrão + o que vier válido (limitado). Falso só para JSON inválido ou que não é objeto (aí `out` fica
// no padrão).
bool annotationStyleFromJson(const std::string &json, AnnotationStyle &out);

} // namespace voltz
```

- [ ] **Step 5: Implementação do modelo**

Criar `frontend/voltz/core/annotation-model.cpp`:
```cpp
#include "annotation-model.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace voltz {

namespace {

using Json = nlohmann::ordered_json;

struct ToolName {
	AnnotationTool tool;
	const char *name;
};

constexpr ToolName kToolNames[] = {
	{AnnotationTool::Pen, "caneta"},          {AnnotationTool::Highlighter, "marca_texto"},
	{AnnotationTool::Line, "linha"},          {AnnotationTool::Arrow, "seta"},
	{AnnotationTool::Rectangle, "retangulo"}, {AnnotationTool::Ellipse, "elipse"},
};

int toIntClamped(int64_t value)
{
	return static_cast<int>(std::clamp<int64_t>(value, -1000000000LL, 1000000000LL));
}

int64_t roundedNumber(const Json &value)
{
	if (value.is_number_integer()) {
		return value.get<int64_t>();
	}
	const double number = value.get<double>();
	if (number > 1e15) {
		return 1000000000000000LL;
	}
	if (number < -1e15) {
		return -1000000000000000LL;
	}
	return static_cast<int64_t>(std::llround(number));
}

// Número JSON em `key` → int64 (arredondado); falso se ausente ou não numérico.
bool readInt64(const Json &object, const char *key, int64_t &out)
{
	const auto it = object.find(key);
	if (it == object.end() || !it->is_number()) {
		return false;
	}
	out = roundedNumber(*it);
	return true;
}

Json pointsArray(const std::vector<StrokePoint> &points)
{
	Json array = Json::array();
	for (const StrokePoint &point : points) {
		array.push_back(Json::array({point.x, point.y}));
	}
	return array;
}

void readPoints(const Json &array, std::vector<StrokePoint> &out)
{
	for (const Json &item : array) {
		if (out.size() >= kMaxStrokePoints) {
			break;
		}
		if (!item.is_array() || item.size() < 2 || !item[0].is_number() || !item[1].is_number()) {
			continue;
		}
		out.push_back(StrokePoint{toIntClamped(roundedNumber(item[0])), toIntClamped(roundedNumber(item[1]))});
	}
}

} // namespace

const char *toolToString(AnnotationTool tool)
{
	for (const ToolName &entry : kToolNames) {
		if (entry.tool == tool) {
			return entry.name;
		}
	}
	return "caneta";
}

bool toolFromString(const std::string &text, AnnotationTool &out)
{
	for (const ToolName &entry : kToolNames) {
		if (text == entry.name) {
			out = entry.tool;
			return true;
		}
	}
	return false;
}

bool isShapeTool(AnnotationTool tool)
{
	return tool != AnnotationTool::Pen && tool != AnnotationTool::Highlighter;
}

int clampStrokeWidth(int width)
{
	return std::clamp(width, kStrokeWidthMin, kStrokeWidthMax);
}

void appendStrokePoints(Stroke &stroke, const std::vector<StrokePoint> &points)
{
	if (points.empty()) {
		return;
	}
	if (isShapeTool(stroke.tool)) {
		if (stroke.points.empty()) {
			stroke.points.push_back(points.front());
			if (points.size() == 1) {
				return;
			}
		}
		stroke.points.resize(1);
		stroke.points.push_back(points.back());
		return;
	}
	for (const StrokePoint &point : points) {
		if (stroke.points.size() >= kMaxStrokePoints) {
			break;
		}
		if (!stroke.points.empty() && stroke.points.back() == point) {
			continue;
		}
		stroke.points.push_back(point);
	}
}

std::string pointsToJson(const std::vector<StrokePoint> &points)
{
	return pointsArray(points).dump();
}

bool pointsFromJson(const std::string &json, std::vector<StrokePoint> &out)
{
	const Json array = Json::parse(json, nullptr, false);
	if (array.is_discarded() || !array.is_array()) {
		return false;
	}
	out.clear();
	readPoints(array, out);
	return true;
}

std::string strokeToJsonLine(const Stroke &stroke)
{
	Json json = Json::object();
	json["id"] = stroke.id;
	json["ferr"] = toolToString(stroke.tool);
	json["cor"] = formatHexColor(stroke.color);
	json["esp"] = stroke.width;
	json["pts"] = pointsArray(stroke.points);
	json["t_ini"] = stroke.startMs;
	if (stroke.endMs >= 0) {
		json["t_fim"] = stroke.endMs;
	}
	if (stroke.clearedMs >= 0) {
		json["t_limpo"] = stroke.clearedMs;
	}
	return json.dump(-1, ' ', false, Json::error_handler_t::replace);
}

bool strokeFromJson(const std::string &json, Stroke &out)
{
	const Json root = Json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		return false;
	}
	const auto idIt = root.find("id");
	if (idIt == root.end() || !idIt->is_number_integer()) {
		return false;
	}
	const auto toolIt = root.find("ferr");
	AnnotationTool tool = AnnotationTool::Pen;
	if (toolIt == root.end() || !toolIt->is_string() || !toolFromString(toolIt->get<std::string>(), tool)) {
		return false;
	}
	Stroke stroke;
	stroke.id = toIntClamped(idIt->get<int64_t>());
	stroke.tool = tool;
	const auto colorIt = root.find("cor");
	if (colorIt != root.end() && colorIt->is_string()) {
		parseHexColor(colorIt->get<std::string>(), stroke.color);
	}
	int64_t value = 0;
	if (readInt64(root, "esp", value)) {
		stroke.width = clampStrokeWidth(toIntClamped(value));
	}
	const auto pointsIt = root.find("pts");
	if (pointsIt != root.end() && pointsIt->is_array()) {
		readPoints(*pointsIt, stroke.points);
	}
	if (readInt64(root, "t_ini", value)) {
		stroke.startMs = value;
	}
	if (readInt64(root, "t_fim", value)) {
		stroke.endMs = value;
	}
	if (readInt64(root, "t_limpo", value)) {
		stroke.clearedMs = value;
	}
	out = std::move(stroke);
	return true;
}

AnnotationStyle clampAnnotationStyle(AnnotationStyle style)
{
	style.width = clampStrokeWidth(style.width);
	style.fadeSeconds = std::clamp(style.fadeSeconds, 0, kFadeSecondsMax);
	return style;
}

std::string annotationStyleToJson(const AnnotationStyle &style)
{
	Json json = Json::object();
	json["ferr"] = toolToString(style.tool);
	json["cor"] = formatHexColor(style.color);
	json["esp"] = style.width;
	json["somem_apos_s"] = style.fadeSeconds;
	return json.dump();
}

bool annotationStyleFromJson(const std::string &json, AnnotationStyle &out)
{
	out = AnnotationStyle{};
	const Json root = Json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		return false;
	}
	const auto toolIt = root.find("ferr");
	if (toolIt != root.end() && toolIt->is_string()) {
		toolFromString(toolIt->get<std::string>(), out.tool);
	}
	const auto colorIt = root.find("cor");
	if (colorIt != root.end() && colorIt->is_string()) {
		parseHexColor(colorIt->get<std::string>(), out.color);
	}
	int64_t value = 0;
	if (readInt64(root, "esp", value)) {
		out.width = toIntClamped(value);
	}
	if (readInt64(root, "somem_apos_s", value)) {
		out.fadeSeconds = toIntClamped(value);
	}
	out = clampAnnotationStyle(out);
	return true;
}

} // namespace voltz
```
Em `frontend/voltz/core/CMakeLists.txt`: na lista `PRIVATE`, antes de `    atomic-file.cpp`, acrescentar
`    annotation-model.cpp`; na lista `PUBLIC`, antes de `    atomic-file.hpp`, acrescentar `    annotation-model.hpp`.

- [ ] **Step 6: Rodar e ver passar**

Run: CONFIGURAR, COMPILAR TESTES, RODAR TESTES.
Expected: PASS; `voltz-core-tests.exe` → `test cases: 133 | 133 passed`.

- [ ] **Step 7: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/annotation-model.hpp frontend/voltz/core/annotation-model.cpp frontend/voltz/core/live-protocol.hpp frontend/voltz/tests/test-annotation-model.cpp
git add frontend/voltz/core/annotation-model.hpp frontend/voltz/core/annotation-model.cpp frontend/voltz/core/live-protocol.hpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/test-annotation-model.cpp frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): modelo do traco, pontos e estilo das anotacoes no nucleo; protocolo voltz_annot_*

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
(Se o clang-format mudou algo, RODAR TESTES de novo antes do commit.)

---

### Task 2: Núcleo — forma do traço e rasterizador (BGRA pré-multiplicado)

**Files:**
- Create: `frontend/voltz/core/annotation-raster.hpp`, `frontend/voltz/core/annotation-raster.cpp`
- Create: `frontend/voltz/tests/test-annotation-raster.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

- [ ] **Step 1: Escrever o teste que falha**

Criar `frontend/voltz/tests/test-annotation-raster.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/annotation-raster.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

using voltz::AnnotationTool;
using voltz::RgbColor;
using voltz::Stroke;
using voltz::StrokePoint;

namespace {

struct Canvas {
	int width = 0;
	int height = 0;
	std::vector<uint8_t> pixels;

	voltz::RasterImage image() { return voltz::RasterImage{pixels.data(), width, height, width * 4}; }
	// B, G, R, A
	const uint8_t *at(int x, int y) const
	{
		return &pixels[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4];
	}
};

Canvas makeCanvas(int width, int height)
{
	return Canvas{width, height, std::vector<uint8_t>(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0)};
}

Stroke makeStroke(AnnotationTool tool, RgbColor color, int width, std::vector<StrokePoint> points)
{
	Stroke stroke;
	stroke.tool = tool;
	stroke.color = color;
	stroke.width = width;
	stroke.points = std::move(points);
	return stroke;
}

const RgbColor kLime{0xD4, 0xF5, 0x3C};
const RgbColor kRed{0xFF, 0x4D, 0x4D};
const RgbColor kBlue{0x00, 0x00, 0xFF};

} // namespace

TEST_CASE("strokeShape: peças por ferramenta")
{
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Pen, kLime, 4, {{0, 0}, {5, 0}, {9, 3}}), 1, 1).capsules.size() == 2);
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Pen, kLime, 4, {{3, 3}}), 1, 1).capsules.size() == 1);
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Line, kLime, 4, {{0, 0}, {50, 9}}), 1, 1).capsules.size() == 1);
	const voltz::StrokeShape arrow = voltz::strokeShape(makeStroke(AnnotationTool::Arrow, kLime, 4, {{10, 32}, {110, 32}}), 1, 1);
	CHECK(arrow.capsules.size() == 1);
	CHECK(arrow.triangles.size() == 1);
	const voltz::StrokeShape shortArrow = voltz::strokeShape(makeStroke(AnnotationTool::Arrow, kLime, 4, {{10, 32}, {15, 32}}), 1, 1);
	CHECK(shortArrow.capsules.empty());
	CHECK(shortArrow.triangles.size() == 1);
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Rectangle, kLime, 4, {{10, 10}, {50, 30}}), 1, 1).capsules.size() == 4);
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Ellipse, kLime, 4, {{0, 0}, {10, 10}}), 1, 1).capsules.size() == 24);
	CHECK(voltz::strokeShape(makeStroke(AnnotationTool::Ellipse, kLime, 4, {{0, 0}, {2000, 1000}}), 1, 1).capsules.size() == 180);
	const voltz::StrokeShape scaled = voltz::strokeShape(makeStroke(AnnotationTool::Line, kLime, 8, {{0, 64}, {255, 64}}), 0.25, 0.25);
	REQUIRE(scaled.capsules.size() == 1);
	CHECK(scaled.capsules[0].radius == doctest::Approx(1.0));
	CHECK(scaled.capsules[0].y0 == doctest::Approx(16.125));
	CHECK(voltz::strokeShape(Stroke{}, 1, 1).capsules.empty());
}

TEST_CASE("rasterizeStroke: linha opaca com borda suavizada de ~1 px")
{
	Canvas canvas = makeCanvas(64, 32);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Line, kLime, 4, {{8, 16}, {56, 16}}), 1, 1);
	const uint8_t *center = canvas.at(32, 16);
	CHECK(center[0] == 0x3C);
	CHECK(center[1] == 0xF5);
	CHECK(center[2] == 0xD4);
	CHECK(center[3] == 255);
	const uint8_t *edge = canvas.at(32, 18); // distância 2 do eixo = metade da cobertura
	CHECK(edge[3] == 128);
	CHECK(edge[1] == 123);
	CHECK(canvas.at(32, 19)[3] == 0);
	CHECK(canvas.at(32, 10)[3] == 0);
}

TEST_CASE("rasterizeStroke: marca-texto a 40% sem escurecer nas junções")
{
	Canvas canvas = makeCanvas(64, 32);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Highlighter, kRed, 2, {{8, 16}, {32, 16}, {56, 16}}), 1, 1);
	const uint8_t *joint = canvas.at(32, 16);
	CHECK(joint[3] == 102);
	CHECK(joint[2] == 102);
	CHECK(joint[1] == 31);
	CHECK(canvas.at(20, 16)[3] == 102);
	CHECK(canvas.at(20, 18)[3] == 102); // marca-texto = 3x a espessura (6 px)
	CHECK(voltz::strokeRenderWidth(makeStroke(AnnotationTool::Highlighter, kRed, 2, {})) == doctest::Approx(6.0));
	CHECK(voltz::strokeOpacity(makeStroke(AnnotationTool::Highlighter, kRed, 2, {})) == doctest::Approx(0.4));
	CHECK(voltz::strokeOpacity(makeStroke(AnnotationTool::Pen, kRed, 2, {})) == doctest::Approx(1.0));
}

TEST_CASE("rasterizeStroke: seta com ponta cheia")
{
	Canvas canvas = makeCanvas(128, 64);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Arrow, kLime, 4, {{10, 32}, {110, 32}}), 1, 1);
	CHECK(canvas.at(50, 32)[3] == 255);  // haste
	CHECK(canvas.at(100, 36)[3] == 255); // dentro da ponta, fora da haste
	CHECK(canvas.at(100, 42)[3] == 0);
	CHECK(canvas.at(50, 37)[3] == 0);
}

TEST_CASE("rasterizeStroke: retângulo e elipse só no contorno")
{
	Canvas canvas = makeCanvas(128, 64);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Rectangle, kLime, 2, {{10, 10}, {50, 30}}), 1, 1);
	CHECK(canvas.at(30, 10)[3] == 255);
	CHECK(canvas.at(50, 20)[3] == 255);
	CHECK(canvas.at(30, 20)[3] == 0);

	Canvas oval = makeCanvas(128, 64);
	voltz::rasterizeStroke(oval.image(), makeStroke(AnnotationTool::Ellipse, kLime, 4, {{0, 0}, {99, 49}}), 1, 1);
	CHECK(oval.at(99, 25)[3] == 255);
	CHECK(oval.at(50, 0)[3] == 255);
	CHECK(oval.at(50, 25)[3] == 0);
}

TEST_CASE("rasterizeStroke: escala da captura para a imagem")
{
	Canvas canvas = makeCanvas(64, 32);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Line, kLime, 8, {{0, 64}, {255, 64}}), 0.25, 0.25);
	CHECK(canvas.at(32, 16)[3] == 255);
	CHECK(canvas.at(32, 18)[3] == 0);
}

TEST_CASE("rasterizeStroke: por cima do traço anterior e cortado na borda da imagem")
{
	Canvas canvas = makeCanvas(64, 32);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Line, kRed, 4, {{-100, 16}, {300, 16}}), 1, 1);
	voltz::rasterizeStroke(canvas.image(), makeStroke(AnnotationTool::Line, kBlue, 4, {{32, -50}, {32, 80}}), 1, 1);
	const uint8_t *cross = canvas.at(32, 16);
	CHECK(cross[0] == 255);
	CHECK(cross[2] == 0);
	CHECK(canvas.at(0, 16)[2] == 255);
	CHECK(canvas.at(63, 16)[2] == 255);
	CHECK(canvas.at(32, 0)[0] == 255);
}

TEST_CASE("clearRaster zera a imagem respeitando o stride")
{
	std::vector<uint8_t> pixels(static_cast<size_t>(8) * 4 * 2, 7);
	const voltz::RasterImage image{pixels.data(), 6, 2, 32};
	voltz::clearRaster(image);
	CHECK(pixels[0] == 0);
	CHECK(pixels[23] == 0);
	CHECK(pixels[24] == 7); // fora da largura útil da linha 0
	CHECK(pixels[32] == 0);
	voltz::rasterizeStroke(voltz::RasterImage{}, makeStroke(AnnotationTool::Pen, kLime, 4, {{1, 1}}), 1, 1);
}
```
Em `frontend/voltz/tests/CMakeLists.txt`, depois de `    test-annotation-model.cpp`, acrescentar
`    test-annotation-raster.cpp`.

- [ ] **Step 2: Rodar e ver falhar**

Run: CONFIGURAR e COMPILAR TESTES.
Expected: FAIL — `Cannot open include file: 'voltz/core/annotation-raster.hpp'`.

- [ ] **Step 3: Cabeçalho**

Criar `frontend/voltz/core/annotation-raster.hpp`:
```cpp
#pragma once

#include "annotation-model.hpp"

#include <cstdint>
#include <vector>

namespace voltz {

// Marca-texto: 3x a espessura escolhida, 40% de opacidade.
constexpr double kHighlighterOpacity = 0.4;
constexpr int kHighlighterWidthFactor = 3;

// Espessura desenhada e opacidade da ferramenta.
double strokeRenderWidth(const Stroke &stroke);
double strokeOpacity(const Stroke &stroke);

// Peças de um traço em pixels da imagem: cápsulas (segmento com pontas redondas, raio = metade da espessura) e
// triângulos cheios (ponta da seta).
struct Capsule {
	double x0 = 0.0;
	double y0 = 0.0;
	double x1 = 0.0;
	double y1 = 0.0;
	double radius = 0.0;
};

struct FilledTriangle {
	double x[3] = {0.0, 0.0, 0.0};
	double y[3] = {0.0, 0.0, 0.0};
};

struct StrokeShape {
	std::vector<Capsule> capsules;
	std::vector<FilledTriangle> triangles;
};

// Forma do traço com os pontos multiplicados por (scaleX, scaleY); (1, 1) = pixels da captura. O centro do pixel (x,y)
// é (x+0,5, y+0,5). Caneta/marca-texto: cápsula entre pontos seguidos (um ponto só = bolinha); linha: primeiro → último;
// seta: haste + ponta cheia (comprimento max(12, 4×esp), limitado ao tamanho da seta; largura = comprimento);
// retângulo: 4 lados; elipse: polígono fechado inscrito no retângulo primeiro/último (24 a 180 lados, ~8 px por lado).
StrokeShape strokeShape(const Stroke &stroke, double scaleX, double scaleY);

// Imagem BGRA com alfa pré-multiplicado (bytes B, G, R, A por pixel; linhas de `stride` bytes, de cima para baixo) — o
// layout do DIB de 32 bits do Windows e da textura GS_BGRA do libobs.
struct RasterImage {
	uint8_t *pixels = nullptr;
	int width = 0;
	int height = 0;
	int stride = 0;
};

void clearRaster(const RasterImage &image);

// Desenha por cima (src-over). Dentro do mesmo traço vale a MAIOR cobertura de cada pixel entre as peças, e só então
// mistura: o marca-texto translúcido não escurece nas junções. Bordas suavizadas em ~1 px; o que sai da imagem é cortado.
void rasterizeStroke(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY);

} // namespace voltz
```

- [ ] **Step 4: Implementação**

Criar `frontend/voltz/core/annotation-raster.cpp`:
```cpp
#include "annotation-raster.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace voltz {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kEllipseMinSides = 24;
constexpr int kEllipseMaxSides = 180;
constexpr double kEllipseSideLength = 8.0;
constexpr double kArrowHeadMin = 12.0;
constexpr double kArrowHeadPerWidth = 4.0;

struct Point {
	double x = 0.0;
	double y = 0.0;
};

Point scaledPoint(const StrokePoint &point, double scaleX, double scaleY)
{
	return Point{(point.x + 0.5) * scaleX, (point.y + 0.5) * scaleY};
}

void addCapsule(StrokeShape &shape, const Point &a, const Point &b, double radius)
{
	shape.capsules.push_back(Capsule{a.x, a.y, b.x, b.y, radius});
}

void addClosedPolygon(StrokeShape &shape, const std::vector<Point> &corners, double radius)
{
	for (size_t i = 0; i < corners.size(); i++) {
		addCapsule(shape, corners[i], corners[(i + 1) % corners.size()], radius);
	}
}

double capsuleDistance(double px, double py, const Capsule &capsule)
{
	const double dx = capsule.x1 - capsule.x0;
	const double dy = capsule.y1 - capsule.y0;
	const double lengthSq = dx * dx + dy * dy;
	double t = 0.0;
	if (lengthSq > 0.0) {
		t = std::clamp(((px - capsule.x0) * dx + (py - capsule.y0) * dy) / lengthSq, 0.0, 1.0);
	}
	const double ex = px - (capsule.x0 + t * dx);
	const double ey = py - (capsule.y0 + t * dy);
	return std::sqrt(ex * ex + ey * ey);
}

// Distância do ponto à borda do triângulo, positiva por dentro (qualquer orientação dos vértices).
double triangleDepth(double px, double py, const FilledTriangle &triangle)
{
	const double area = (triangle.x[1] - triangle.x[0]) * (triangle.y[2] - triangle.y[0]) -
			    (triangle.y[1] - triangle.y[0]) * (triangle.x[2] - triangle.x[0]);
	if (std::abs(area) < 1e-9) {
		return -1e9;
	}
	const double sign = area > 0.0 ? 1.0 : -1.0;
	double depth = 1e9;
	for (int i = 0; i < 3; i++) {
		const int j = (i + 1) % 3;
		const double ex = triangle.x[j] - triangle.x[i];
		const double ey = triangle.y[j] - triangle.y[i];
		const double length = std::sqrt(ex * ex + ey * ey);
		if (length < 1e-9) {
			continue;
		}
		depth = std::min(depth, (ex * (py - triangle.y[i]) - ey * (px - triangle.x[i])) * sign / length);
	}
	return depth;
}

// [low, high] em pixels inteiros [from, to) dentro de [limitLow, limitHigh).
void pixelSpan(double low, double high, int limitLow, int limitHigh, int &from, int &to)
{
	from = static_cast<int>(
		std::clamp(std::floor(low), static_cast<double>(limitLow), static_cast<double>(limitHigh)));
	to = static_cast<int>(
		std::clamp(std::ceil(high) + 1.0, static_cast<double>(limitLow), static_cast<double>(limitHigh)));
}

uint8_t blendChannel(int source, int destination, int alpha)
{
	return static_cast<uint8_t>((source * alpha + destination * (255 - alpha) + 127) / 255);
}

} // namespace

double strokeRenderWidth(const Stroke &stroke)
{
	const double width = static_cast<double>(clampStrokeWidth(stroke.width));
	return stroke.tool == AnnotationTool::Highlighter ? width * kHighlighterWidthFactor : width;
}

double strokeOpacity(const Stroke &stroke)
{
	return stroke.tool == AnnotationTool::Highlighter ? kHighlighterOpacity : 1.0;
}

StrokeShape strokeShape(const Stroke &stroke, double scaleX, double scaleY)
{
	StrokeShape shape;
	if (stroke.points.empty() || scaleX <= 0.0 || scaleY <= 0.0) {
		return shape;
	}
	const double scale = (scaleX + scaleY) / 2.0;
	const double radius = strokeRenderWidth(stroke) * scale / 2.0;
	const Point first = scaledPoint(stroke.points.front(), scaleX, scaleY);
	const Point last = scaledPoint(stroke.points.back(), scaleX, scaleY);
	switch (stroke.tool) {
	case AnnotationTool::Pen:
	case AnnotationTool::Highlighter:
		if (stroke.points.size() == 1) {
			addCapsule(shape, first, first, radius);
		}
		for (size_t i = 1; i < stroke.points.size(); i++) {
			addCapsule(shape, scaledPoint(stroke.points[i - 1], scaleX, scaleY),
				   scaledPoint(stroke.points[i], scaleX, scaleY), radius);
		}
		break;
	case AnnotationTool::Line:
		addCapsule(shape, first, last, radius);
		break;
	case AnnotationTool::Arrow: {
		const double dx = last.x - first.x;
		const double dy = last.y - first.y;
		const double length = std::sqrt(dx * dx + dy * dy);
		if (length < 1e-6) {
			addCapsule(shape, first, first, radius);
			break;
		}
		const double head = std::min(
			length, std::max(kArrowHeadMin, kArrowHeadPerWidth * clampStrokeWidth(stroke.width)) * scale);
		const double ux = dx / length;
		const double uy = dy / length;
		const Point base{last.x - ux * head, last.y - uy * head};
		if (length > head) {
			addCapsule(shape, first, base, radius);
		}
		const double half = head * 0.5;
		FilledTriangle tip;
		tip.x[0] = last.x;
		tip.y[0] = last.y;
		tip.x[1] = base.x - uy * half;
		tip.y[1] = base.y + ux * half;
		tip.x[2] = base.x + uy * half;
		tip.y[2] = base.y - ux * half;
		shape.triangles.push_back(tip);
		break;
	}
	case AnnotationTool::Rectangle:
		addClosedPolygon(shape, {first, Point{last.x, first.y}, last, Point{first.x, last.y}}, radius);
		break;
	case AnnotationTool::Ellipse: {
		const double centerX = (first.x + last.x) / 2.0;
		const double centerY = (first.y + last.y) / 2.0;
		const double radiusX = std::abs(last.x - first.x) / 2.0;
		const double radiusY = std::abs(last.y - first.y) / 2.0;
		if (radiusX < 0.5 && radiusY < 0.5) {
			addCapsule(shape, first, first, radius);
			break;
		}
		const double perimeter = 2.0 * kPi * std::sqrt((radiusX * radiusX + radiusY * radiusY) / 2.0);
		const int sides = std::clamp(static_cast<int>(std::ceil(perimeter / kEllipseSideLength)),
					     kEllipseMinSides, kEllipseMaxSides);
		std::vector<Point> corners;
		corners.reserve(static_cast<size_t>(sides));
		for (int i = 0; i < sides; i++) {
			const double angle = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(sides);
			corners.push_back(Point{centerX + radiusX * std::cos(angle), centerY + radiusY * std::sin(angle)});
		}
		addClosedPolygon(shape, corners, radius);
		break;
	}
	}
	return shape;
}

void clearRaster(const RasterImage &image)
{
	if (!image.pixels || image.width <= 0 || image.height <= 0) {
		return;
	}
	for (int y = 0; y < image.height; y++) {
		std::fill_n(image.pixels + static_cast<size_t>(y) * static_cast<size_t>(image.stride),
			    static_cast<size_t>(image.width) * 4, uint8_t{0});
	}
}

void rasterizeStroke(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY)
{
	if (!image.pixels || image.width <= 0 || image.height <= 0) {
		return;
	}
	const StrokeShape shape = strokeShape(stroke, scaleX, scaleY);
	if (shape.capsules.empty() && shape.triangles.empty()) {
		return;
	}

	double minX = 1e300;
	double minY = 1e300;
	double maxX = -1e300;
	double maxY = -1e300;
	auto grow = [&](double x, double y, double margin) {
		minX = std::min(minX, x - margin);
		maxX = std::max(maxX, x + margin);
		minY = std::min(minY, y - margin);
		maxY = std::max(maxY, y + margin);
	};
	for (const Capsule &capsule : shape.capsules) {
		grow(capsule.x0, capsule.y0, capsule.radius + 1.0);
		grow(capsule.x1, capsule.y1, capsule.radius + 1.0);
	}
	for (const FilledTriangle &triangle : shape.triangles) {
		for (int i = 0; i < 3; i++) {
			grow(triangle.x[i], triangle.y[i], 1.0);
		}
	}
	int left = 0;
	int right = 0;
	int top = 0;
	int bottom = 0;
	pixelSpan(minX, maxX, 0, image.width, left, right);
	pixelSpan(minY, maxY, 0, image.height, top, bottom);
	if (left >= right || top >= bottom) {
		return;
	}

	/* cobertura do traço inteiro (0..255): a maior entre as peças em cada pixel */
	const int maskWidth = right - left;
	std::vector<uint8_t> mask(static_cast<size_t>(maskWidth) * static_cast<size_t>(bottom - top), 0);
	auto cover = [&](int x, int y, double coverage) {
		if (coverage <= 0.0) {
			return;
		}
		const uint8_t value = static_cast<uint8_t>(std::lround(std::min(coverage, 1.0) * 255.0));
		uint8_t &cell = mask[static_cast<size_t>(y - top) * static_cast<size_t>(maskWidth) +
				     static_cast<size_t>(x - left)];
		cell = std::max(cell, value);
	};
	for (const Capsule &capsule : shape.capsules) {
		int x0 = 0;
		int x1 = 0;
		int y0 = 0;
		int y1 = 0;
		pixelSpan(std::min(capsule.x0, capsule.x1) - capsule.radius - 1.0,
			  std::max(capsule.x0, capsule.x1) + capsule.radius + 1.0, left, right, x0, x1);
		pixelSpan(std::min(capsule.y0, capsule.y1) - capsule.radius - 1.0,
			  std::max(capsule.y0, capsule.y1) + capsule.radius + 1.0, top, bottom, y0, y1);
		for (int y = y0; y < y1; y++) {
			for (int x = x0; x < x1; x++) {
				cover(x, y, capsule.radius - capsuleDistance(x + 0.5, y + 0.5, capsule) + 0.5);
			}
		}
	}
	for (const FilledTriangle &triangle : shape.triangles) {
		int x0 = 0;
		int x1 = 0;
		int y0 = 0;
		int y1 = 0;
		pixelSpan(std::min({triangle.x[0], triangle.x[1], triangle.x[2]}) - 1.0,
			  std::max({triangle.x[0], triangle.x[1], triangle.x[2]}) + 1.0, left, right, x0, x1);
		pixelSpan(std::min({triangle.y[0], triangle.y[1], triangle.y[2]}) - 1.0,
			  std::max({triangle.y[0], triangle.y[1], triangle.y[2]}) + 1.0, top, bottom, y0, y1);
		for (int y = y0; y < y1; y++) {
			for (int x = x0; x < x1; x++) {
				cover(x, y, triangleDepth(x + 0.5, y + 0.5, triangle) + 0.5);
			}
		}
	}

	const double opacity = strokeOpacity(stroke);
	const int blue = stroke.color.b;
	const int green = stroke.color.g;
	const int red = stroke.color.r;
	for (int y = top; y < bottom; y++) {
		uint8_t *row = image.pixels + static_cast<size_t>(y) * static_cast<size_t>(image.stride);
		const uint8_t *maskRow = mask.data() + static_cast<size_t>(y - top) * static_cast<size_t>(maskWidth);
		for (int x = left; x < right; x++) {
			const int value = maskRow[x - left];
			if (value == 0) {
				continue;
			}
			const int alpha = static_cast<int>(std::lround(value * opacity));
			if (alpha <= 0) {
				continue;
			}
			uint8_t *pixel = row + static_cast<size_t>(x) * 4;
			pixel[0] = blendChannel(blue, pixel[0], alpha);
			pixel[1] = blendChannel(green, pixel[1], alpha);
			pixel[2] = blendChannel(red, pixel[2], alpha);
			pixel[3] = static_cast<uint8_t>(alpha + (pixel[3] * (255 - alpha) + 127) / 255);
		}
	}
}

} // namespace voltz
```
Em `frontend/voltz/core/CMakeLists.txt`: depois de `    annotation-model.cpp` acrescentar `    annotation-raster.cpp`;
depois de `    annotation-model.hpp` acrescentar `    annotation-raster.hpp`.

- [ ] **Step 5: Rodar e ver passar**

Run: CONFIGURAR, COMPILAR TESTES, RODAR TESTES.
Expected: PASS; `test cases: 141 | 141 passed`. Se um valor de pixel divergir por ±1 (arredondamento), **não** afrouxe o
teste sem entender: a conta esperada está ao lado de cada `CHECK` (ex.: borda = cobertura 0,5 → `lround(127.5) = 128`;
marca-texto = `lround(255 × 0,4) = 102`; verde da borda = `(245×128 + 127) / 255 = 123`).

- [ ] **Step 6: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/annotation-raster.hpp frontend/voltz/core/annotation-raster.cpp frontend/voltz/tests/test-annotation-raster.cpp
git add frontend/voltz/core/annotation-raster.hpp frontend/voltz/core/annotation-raster.cpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/test-annotation-raster.cpp frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): forma e rasterizador dos tracos (BGRA pre-multiplicado, marca-texto sem juncoes)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Núcleo — `StrokeStore` e diário do `anotacoes.jsonl`

**Files:**
- Create: `frontend/voltz/core/stroke-store.hpp`, `frontend/voltz/core/stroke-store.cpp`
- Create: `frontend/voltz/core/annotation-journal.hpp`, `frontend/voltz/core/annotation-journal.cpp`
- Create: `frontend/voltz/tests/test-stroke-store.cpp`, `frontend/voltz/tests/test-annotation-journal.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

- [ ] **Step 1: Escrever os testes que falham**

Criar `frontend/voltz/tests/test-stroke-store.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/stroke-store.hpp>

#include <cstdint>
#include <vector>

using voltz::AnnotationTool;
using voltz::RgbColor;
using voltz::StrokePoint;
using voltz::StrokeStore;

namespace {
constexpr int64_t kMs = 1000000;
const RgbColor kLime{0xD4, 0xF5, 0x3C};
} // namespace

TEST_CASE("StrokeStore: começar, acrescentar e terminar; ids crescentes")
{
	StrokeStore store;
	const int first = store.begin(AnnotationTool::Pen, kLime, 99, StrokePoint{1, 1}, 100 * kMs);
	CHECK(first == 1);
	CHECK(store.openId() == 1);
	CHECK(store.extend({{2, 2}, {3, 3}}));
	const voltz::LiveStroke *ended = store.end(150 * kMs);
	REQUIRE(ended != nullptr);
	CHECK(ended->stroke.id == 1);
	CHECK(ended->stroke.width == voltz::kStrokeWidthMax);
	CHECK(ended->stroke.points.size() == 3);
	CHECK(ended->startNs == 100 * kMs);
	CHECK(ended->endNs == 150 * kMs);
	CHECK(store.openId() == 0);
	CHECK(store.end(160 * kMs) == nullptr);
	CHECK(store.begin(AnnotationTool::Arrow, kLime, 4, StrokePoint{0, 0}, 200 * kMs) == 2);
	REQUIRE(store.visible().size() == 2);
	CHECK(store.strokes()[1].tool == AnnotationTool::Arrow);
}

TEST_CASE("StrokeStore: só um traço em andamento; acrescentar sem traço falha")
{
	StrokeStore store;
	CHECK_FALSE(store.extend({{1, 1}}));
	CHECK(store.begin(AnnotationTool::Line, kLime, 4, StrokePoint{0, 0}, 0) == 1);
	CHECK(store.begin(AnnotationTool::Line, kLime, 4, StrokePoint{5, 5}, 1) == 0);
	CHECK_FALSE(store.extend({}));
	CHECK(store.extend({{7, 7}, {9, 9}}));
	REQUIRE(store.visible().size() == 1);
	CHECK(store.visible()[0].stroke.points.size() == 2);
	CHECK(store.visible()[0].stroke.points[1] == StrokePoint{9, 9});
}

TEST_CASE("StrokeStore: desfazer tira o mais recente, mesmo em andamento")
{
	StrokeStore store;
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 0);
	store.end(10 * kMs);
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{1, 1}, 20 * kMs);
	const std::vector<voltz::RemovedStroke> removed = store.undo(30 * kMs);
	REQUIRE(removed.size() == 1);
	CHECK(removed[0].live.stroke.id == 2);
	CHECK(removed[0].live.open());
	CHECK(removed[0].removedNs == 30 * kMs);
	CHECK(store.openId() == 0);
	CHECK(store.undo(40 * kMs).size() == 1);
	CHECK(store.undo(50 * kMs).empty());
}

TEST_CASE("StrokeStore: limpar tira todos na ordem")
{
	StrokeStore store;
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 0);
	store.end(1);
	store.begin(AnnotationTool::Ellipse, kLime, 4, StrokePoint{0, 0}, 2);
	const std::vector<voltz::RemovedStroke> removed = store.clear(9 * kMs);
	REQUIRE(removed.size() == 2);
	CHECK(removed[0].live.stroke.id == 1);
	CHECK(removed[1].live.stroke.id == 2);
	CHECK(removed[1].removedNs == 9 * kMs);
	CHECK(store.visible().empty());
	CHECK(store.clear(10 * kMs).empty());
	CHECK(store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 11) == 3);
}

TEST_CASE("StrokeStore: somem após N s no instante exato; em andamento não some")
{
	const int64_t lifetime = 1000 * kMs;
	StrokeStore store;
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 0);
	store.end(100 * kMs);
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 200 * kMs);
	store.end(300 * kMs);
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 400 * kMs);
	CHECK(store.nextExpiryNs(lifetime) == 1100 * kMs);
	CHECK(store.nextExpiryNs(0) == -1);
	CHECK(store.expire(5000 * kMs, 0).empty());
	const std::vector<voltz::RemovedStroke> removed = store.expire(1200 * kMs, lifetime);
	REQUIRE(removed.size() == 1);
	CHECK(removed[0].live.stroke.id == 1);
	CHECK(removed[0].removedNs == 1100 * kMs);
	CHECK(store.nextExpiryNs(lifetime) == 1300 * kMs);
	CHECK(store.expire(9000 * kMs, lifetime).size() == 1);
	REQUIRE(store.visible().size() == 1);
	CHECK(store.visible()[0].open());
	CHECK(store.nextExpiryNs(lifetime) == -1);
}
```
Criar `frontend/voltz/tests/test-annotation-journal.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/annotation-journal.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using voltz::AnnotationTool;
using voltz::LiveStroke;
using voltz::StrokePoint;

namespace {

constexpr int64_t kMs = 1000000;
constexpr int64_t kT0 = 5000 * kMs;

LiveStroke makeLive(int id, AnnotationTool tool, int64_t startNs, int64_t endNs, std::vector<StrokePoint> points)
{
	LiveStroke live;
	live.stroke.id = id;
	live.stroke.tool = tool;
	live.stroke.points = std::move(points);
	live.startNs = startNs;
	live.endNs = endNs;
	return live;
}

} // namespace

TEST_CASE("AnnotationJournal: linha completa ao terminar, uma vez, em ms relativos ao t0")
{
	voltz::AnnotationJournal journal(kT0);
	const LiveStroke live = makeLive(7, AnnotationTool::Arrow, kT0 + 1200 * kMs, kT0 + 1850 * kMs, {{10, 20}, {30, 40}});
	CHECK(journal.strokeFinished(live) ==
	      R"({"id":7,"ferr":"seta","cor":"#D4F53C","esp":4,"pts":[[10,20],[30,40]],"t_ini":1200,"t_fim":1850})");
	CHECK(journal.strokeFinished(live).empty());
	CHECK(journal.strokeCount() == 1);
	CHECK(journal.strokeFinished(makeLive(8, AnnotationTool::Pen, kT0, -1, {{1, 1}})).empty());
}

TEST_CASE("AnnotationJournal: sair da tela vira linha curta; traço ainda sem linha vira linha completa")
{
	voltz::AnnotationJournal journal(kT0);
	const LiveStroke done = makeLive(1, AnnotationTool::Line, kT0 + 100 * kMs, kT0 + 200 * kMs, {{0, 0}, {5, 5}});
	journal.strokeFinished(done);
	CHECK(journal.strokeRemoved(voltz::RemovedStroke{done, kT0 + 9000 * kMs}) == R"({"id":1,"t_limpo":9000})");
	const LiveStroke open = makeLive(2, AnnotationTool::Pen, kT0 + 300 * kMs, -1, {{1, 1}, {2, 2}});
	CHECK(journal.strokeRemoved(voltz::RemovedStroke{open, kT0 + 400 * kMs}) ==
	      R"({"id":2,"ferr":"caneta","cor":"#D4F53C","esp":4,"pts":[[1,1],[2,2]],"t_ini":300,"t_fim":400,"t_limpo":400})");
	CHECK(journal.strokeCount() == 2);
}

TEST_CASE("AnnotationJournal: traços de antes do início valem 0; em andamento no fim fecham no fim")
{
	voltz::AnnotationJournal journal(kT0);
	const std::vector<LiveStroke> visible = {
		makeLive(1, AnnotationTool::Rectangle, kT0 - 3000 * kMs, kT0 - 2000 * kMs, {{0, 0}, {9, 9}}),
		makeLive(2, AnnotationTool::Pen, kT0 - 500 * kMs, -1, {{4, 4}})};
	const std::vector<std::string> initial = journal.initialLines(visible);
	REQUIRE(initial.size() == 1);
	CHECK(initial[0] == R"({"id":1,"ferr":"retangulo","cor":"#D4F53C","esp":4,"pts":[[0,0],[9,9]],"t_ini":0,"t_fim":0})");
	const std::vector<std::string> closing = journal.finishOpen(visible, kT0 + 7000 * kMs);
	REQUIRE(closing.size() == 1);
	CHECK(closing[0] == R"({"id":2,"ferr":"caneta","cor":"#D4F53C","esp":4,"pts":[[4,4]],"t_ini":0,"t_fim":7000})");
	CHECK(journal.finishOpen(visible, kT0 + 8000 * kMs).empty());
}

TEST_CASE("consolidateAnnotations: junta t_limpo, mantém ordem e campos desconhecidos, ignora o resto")
{
	const std::string journal =
		"{\"id\":1,\"ferr\":\"seta\",\"cor\":\"#D4F53C\",\"esp\":4,\"pts\":[[1,2]],\"t_ini\":0,\"t_fim\":5,\"extra\":true}\n"
		"lixo\n"
		"{\"id\":2,\"ferr\":\"linha\",\"cor\":\"#D4F53C\",\"esp\":4,\"pts\":[[3,4]],\"t_ini\":6,\"t_fim\":7}\n"
		"{\"id\":1,\"t_limpo\":90}\n"
		"{\"id\":5,\"t_limpo\":99}\n"
		"{\"t_limpo\":1}\n";
	CHECK(voltz::consolidateAnnotations(journal) ==
	      "{\"id\":1,\"ferr\":\"seta\",\"cor\":\"#D4F53C\",\"esp\":4,\"pts\":[[1,2]],\"t_ini\":0,\"t_fim\":5,\"extra\":true,\"t_limpo\":90}\n"
	      "{\"id\":2,\"ferr\":\"linha\",\"cor\":\"#D4F53C\",\"esp\":4,\"pts\":[[3,4]],\"t_ini\":6,\"t_fim\":7}\n");
}

TEST_CASE("consolidateAnnotations: idempotente, aceita CRLF e última linha sem quebra")
{
	const std::string journal = "{\"id\":3,\"ferr\":\"caneta\",\"pts\":[]}\r\n{\"id\":3,\"t_limpo\":12}";
	const std::string once = voltz::consolidateAnnotations(journal);
	CHECK(once == "{\"id\":3,\"ferr\":\"caneta\",\"pts\":[],\"t_limpo\":12}\n");
	CHECK(voltz::consolidateAnnotations(once) == once);
	CHECK(voltz::consolidateAnnotations("").empty());
}
```
Em `frontend/voltz/tests/CMakeLists.txt`: antes de `    test-annotation-model.cpp` acrescentar
`    test-annotation-journal.cpp`; depois de `    test-source-tree.cpp` acrescentar `    test-stroke-store.cpp`.

- [ ] **Step 2: Rodar e ver falhar**

Run: CONFIGURAR e COMPILAR TESTES.
Expected: FAIL — `Cannot open include file: 'voltz/core/stroke-store.hpp'` (e `annotation-journal.hpp`).

- [ ] **Step 3: `StrokeStore`**

Criar `frontend/voltz/core/stroke-store.hpp`:
```cpp
#pragma once

#include "annotation-model.hpp"

#include <cstdint>
#include <vector>

namespace voltz {

// Traço na tela agora. Tempos absolutos no relógio do libobs (ns).
struct LiveStroke {
	Stroke stroke;
	int64_t startNs = 0;
	int64_t endNs = -1; // -1 = em andamento
	bool open() const { return endNs < 0; }
};

// Traço que saiu da tela (desfazer, limpar ou "somem após N s") e quando.
struct RemovedStroke {
	LiveStroke live;
	int64_t removedNs = 0;
};

// Traços na tela, na ordem em que começaram (o último fica por cima); no máximo um em andamento, sempre o último. Ids a
// partir de 1, nunca reaproveitados. Sem Qt nem libobs: o dono é o AnnotationController (thread da UI).
class StrokeStore {
public:
	// Começa um traço (espessura limitada) com o primeiro ponto. 0 se já há um em andamento.
	int begin(AnnotationTool tool, const RgbColor &color, int width, const StrokePoint &first, int64_t nowNs);
	// Acrescenta pontos ao traço em andamento (appendStrokePoints). Falso sem traço em andamento ou sem pontos.
	bool extend(const std::vector<StrokePoint> &points);
	// Termina o traço em andamento; nullptr se não há. O ponteiro vale até a próxima mudança.
	const LiveStroke *end(int64_t nowNs);
	// Tira o traço mais recente (em andamento ou não). Vazio se não há traço.
	std::vector<RemovedStroke> undo(int64_t nowNs);
	// Tira todos, na ordem em que estavam.
	std::vector<RemovedStroke> clear(int64_t nowNs);
	// "Somem após": tira os traços terminados com endNs + lifetimeNs <= nowNs, com removedNs = endNs + lifetimeNs (o
	// instante exato, não o do temporizador). lifetimeNs <= 0 = nunca somem.
	std::vector<RemovedStroke> expire(int64_t nowNs, int64_t lifetimeNs);
	// Próximo instante em que algum traço some (-1 se nenhum).
	int64_t nextExpiryNs(int64_t lifetimeNs) const;

	const std::vector<LiveStroke> &visible() const { return visible_; }
	std::vector<Stroke> strokes() const;
	// Id do traço em andamento (0 se nenhum).
	int openId() const;

private:
	std::vector<LiveStroke> visible_;
	int nextId_ = 1;
};

} // namespace voltz
```
Criar `frontend/voltz/core/stroke-store.cpp`:
```cpp
#include "stroke-store.hpp"

#include <algorithm>
#include <utility>

namespace voltz {

int StrokeStore::begin(AnnotationTool tool, const RgbColor &color, int width, const StrokePoint &first, int64_t nowNs)
{
	if (openId() != 0) {
		return 0;
	}
	LiveStroke live;
	live.stroke.id = nextId_++;
	live.stroke.tool = tool;
	live.stroke.color = color;
	live.stroke.width = clampStrokeWidth(width);
	appendStrokePoints(live.stroke, {first});
	live.startNs = nowNs;
	visible_.push_back(std::move(live));
	return visible_.back().stroke.id;
}

bool StrokeStore::extend(const std::vector<StrokePoint> &points)
{
	if (points.empty() || visible_.empty() || !visible_.back().open()) {
		return false;
	}
	appendStrokePoints(visible_.back().stroke, points);
	return true;
}

const LiveStroke *StrokeStore::end(int64_t nowNs)
{
	if (visible_.empty() || !visible_.back().open()) {
		return nullptr;
	}
	LiveStroke &live = visible_.back();
	live.endNs = std::max(nowNs, live.startNs);
	return &live;
}

std::vector<RemovedStroke> StrokeStore::undo(int64_t nowNs)
{
	if (visible_.empty()) {
		return {};
	}
	RemovedStroke removed{visible_.back(), nowNs};
	visible_.pop_back();
	return {removed};
}

std::vector<RemovedStroke> StrokeStore::clear(int64_t nowNs)
{
	std::vector<RemovedStroke> removed;
	removed.reserve(visible_.size());
	for (const LiveStroke &live : visible_) {
		removed.push_back(RemovedStroke{live, nowNs});
	}
	visible_.clear();
	return removed;
}

std::vector<RemovedStroke> StrokeStore::expire(int64_t nowNs, int64_t lifetimeNs)
{
	if (lifetimeNs <= 0) {
		return {};
	}
	std::vector<RemovedStroke> removed;
	std::vector<LiveStroke> kept;
	kept.reserve(visible_.size());
	for (LiveStroke &live : visible_) {
		if (!live.open() && live.endNs + lifetimeNs <= nowNs) {
			removed.push_back(RemovedStroke{live, live.endNs + lifetimeNs});
		} else {
			kept.push_back(std::move(live));
		}
	}
	visible_ = std::move(kept);
	return removed;
}

int64_t StrokeStore::nextExpiryNs(int64_t lifetimeNs) const
{
	if (lifetimeNs <= 0) {
		return -1;
	}
	int64_t next = -1;
	for (const LiveStroke &live : visible_) {
		if (!live.open() && (next < 0 || live.endNs + lifetimeNs < next)) {
			next = live.endNs + lifetimeNs;
		}
	}
	return next;
}

std::vector<Stroke> StrokeStore::strokes() const
{
	std::vector<Stroke> out;
	out.reserve(visible_.size());
	for (const LiveStroke &live : visible_) {
		out.push_back(live.stroke);
	}
	return out;
}

int StrokeStore::openId() const
{
	return !visible_.empty() && visible_.back().open() ? visible_.back().stroke.id : 0;
}

} // namespace voltz
```

- [ ] **Step 4: Diário e consolidação**

Criar `frontend/voltz/core/annotation-journal.hpp`:
```cpp
#pragma once

#include "stroke-store.hpp"

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace voltz {

constexpr const char *kAnnotationsJournalUpdateKey = "t_limpo";

// anotacoes.jsonl durante a gravação (spec §5.1/§6.2). O arquivo só cresce (sobrevive a uma queda): uma linha completa
// quando o traço termina e, quando ele sai da tela, a linha curta {"id":7,"t_limpo":9000}. No fim (e ao recuperar o
// projeto) consolidateAnnotations junta tudo no formato da spec: uma linha completa por traço, com t_limpo.
// Tempos em ms relativos a t0 (sem descontar pausas, spec §5.5), nunca negativos: o que já estava na tela no início
// vale 0. Linhas sem "\n".
class AnnotationJournal {
public:
	explicit AnnotationJournal(int64_t t0Ns) : t0Ns_(t0Ns) {}

	// Início da gravação: linha completa de cada traço já terminado (t_ini = t_fim = 0 se for de antes). Os em
	// andamento entram quando terminarem.
	std::vector<std::string> initialLines(const std::vector<LiveStroke> &visible);
	// O traço terminou: linha completa, uma vez por id ("" se em andamento ou já escrito).
	std::string strokeFinished(const LiveStroke &live);
	// O traço saiu da tela: linha curta; se ainda não tinha linha (em andamento), linha completa com t_fim = t_limpo.
	std::string strokeRemoved(const RemovedStroke &removed);
	// Fim da gravação: linha completa dos traços em andamento que ainda não têm linha (t_fim = endNs).
	std::vector<std::string> finishOpen(const std::vector<LiveStroke> &visible, int64_t endNs);
	// Traços com linha completa.
	size_t strokeCount() const { return written_.size(); }

private:
	int64_t relativeMs(int64_t ns) const;
	std::string fullLine(const LiveStroke &live, int64_t endNs, int64_t clearedNs);

	int64_t t0Ns_ = 0;
	std::set<int> written_;
};

// Junta o diário no formato da spec: cada linha completa (com "ferr") na ordem em que apareceu, com o "t_limpo" da linha
// curta de mesmo id; campos desconhecidos ficam; linhas inválidas, curtas de id desconhecido e repetidas saem. Cada
// linha termina com "\n". Idempotente.
std::string consolidateAnnotations(const std::string &jsonl);

} // namespace voltz
```
Criar `frontend/voltz/core/annotation-journal.cpp`:
```cpp
#include "annotation-journal.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <map>
#include <utility>

namespace voltz {

int64_t AnnotationJournal::relativeMs(int64_t ns) const
{
	const int64_t delta = ns - t0Ns_;
	return delta > 0 ? delta / 1000000 : 0;
}

std::string AnnotationJournal::fullLine(const LiveStroke &live, int64_t endNs, int64_t clearedNs)
{
	Stroke stroke = live.stroke;
	stroke.startMs = relativeMs(live.startNs);
	stroke.endMs = endNs >= 0 ? std::max(stroke.startMs, relativeMs(endNs)) : -1;
	stroke.clearedMs = clearedNs >= 0 ? std::max(std::max(stroke.startMs, stroke.endMs), relativeMs(clearedNs)) : -1;
	written_.insert(stroke.id);
	return strokeToJsonLine(stroke);
}

std::vector<std::string> AnnotationJournal::initialLines(const std::vector<LiveStroke> &visible)
{
	std::vector<std::string> lines;
	for (const LiveStroke &live : visible) {
		if (!live.open() && written_.count(live.stroke.id) == 0) {
			lines.push_back(fullLine(live, live.endNs, -1));
		}
	}
	return lines;
}

std::string AnnotationJournal::strokeFinished(const LiveStroke &live)
{
	if (live.open() || written_.count(live.stroke.id) != 0) {
		return {};
	}
	return fullLine(live, live.endNs, -1);
}

std::string AnnotationJournal::strokeRemoved(const RemovedStroke &removed)
{
	const LiveStroke &live = removed.live;
	if (written_.count(live.stroke.id) == 0) {
		return fullLine(live, live.open() ? removed.removedNs : live.endNs, removed.removedNs);
	}
	char buffer[96];
	std::snprintf(buffer, sizeof(buffer), "{\"id\":%d,\"%s\":%lld}", live.stroke.id, kAnnotationsJournalUpdateKey,
		      static_cast<long long>(relativeMs(removed.removedNs)));
	return buffer;
}

std::vector<std::string> AnnotationJournal::finishOpen(const std::vector<LiveStroke> &visible, int64_t endNs)
{
	std::vector<std::string> lines;
	for (const LiveStroke &live : visible) {
		if (live.open() && written_.count(live.stroke.id) == 0) {
			lines.push_back(fullLine(live, endNs, -1));
		}
	}
	return lines;
}

std::string consolidateAnnotations(const std::string &jsonl)
{
	using Json = nlohmann::ordered_json;
	std::vector<Json> strokes;
	std::map<int64_t, size_t> index;
	size_t start = 0;
	while (start < jsonl.size()) {
		size_t end = jsonl.find('\n', start);
		if (end == std::string::npos) {
			end = jsonl.size();
		}
		std::string line = jsonl.substr(start, end - start);
		start = end + 1;
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (line.empty()) {
			continue;
		}
		Json item = Json::parse(line, nullptr, false);
		if (item.is_discarded() || !item.is_object()) {
			continue;
		}
		const auto id = item.find("id");
		if (id == item.end() || !id->is_number_integer()) {
			continue;
		}
		const int64_t key = id->get<int64_t>();
		if (item.contains("ferr")) {
			if (index.emplace(key, strokes.size()).second) {
				strokes.push_back(std::move(item));
			}
			continue;
		}
		const auto cleared = item.find(kAnnotationsJournalUpdateKey);
		const auto found = index.find(key);
		if (cleared != item.end() && found != index.end()) {
			strokes[found->second][kAnnotationsJournalUpdateKey] = *cleared;
		}
	}
	std::string out;
	for (const Json &stroke : strokes) {
		out += stroke.dump(-1, ' ', false, Json::error_handler_t::replace);
		out += '\n';
	}
	return out;
}

} // namespace voltz
```
Em `frontend/voltz/core/CMakeLists.txt`: na lista `PRIVATE`, antes de `    annotation-model.cpp` acrescentar
`    annotation-journal.cpp` e depois de `    source-tree.cpp` acrescentar `    stroke-store.cpp`; na lista `PUBLIC`, antes
de `    annotation-model.hpp` acrescentar `    annotation-journal.hpp` e depois de `    source-tree.hpp` acrescentar
`    stroke-store.hpp`.

- [ ] **Step 5: Rodar e ver passar**

Run: CONFIGURAR, COMPILAR TESTES, RODAR TESTES.
Expected: PASS; `test cases: 151 | 151 passed`.

- [ ] **Step 6: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/stroke-store.hpp frontend/voltz/core/stroke-store.cpp frontend/voltz/core/annotation-journal.hpp frontend/voltz/core/annotation-journal.cpp frontend/voltz/tests/test-stroke-store.cpp frontend/voltz/tests/test-annotation-journal.cpp
git add frontend/voltz/core/stroke-store.hpp frontend/voltz/core/stroke-store.cpp frontend/voltz/core/annotation-journal.hpp frontend/voltz/core/annotation-journal.cpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/test-stroke-store.cpp frontend/voltz/tests/test-annotation-journal.cpp frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): StrokeStore e diario do anotacoes.jsonl (linhas curtas de t_limpo + consolidacao)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Núcleo — manifesto com `arquivos.anotacoes` e `anotacoes_queimadas`

**Files:**
- Modify: `frontend/voltz/core/project-manifest.hpp`, `frontend/voltz/core/project-manifest.cpp`
- Test: `frontend/voltz/tests/test-project-manifest.cpp`

- [ ] **Step 1: Escrever os testes que falham**

No fim de `frontend/voltz/tests/test-project-manifest.cpp`, acrescentar:
```cpp
TEST_CASE("manifesto: arquivos.anotacoes e anotacoes_queimadas")
{
	ProjectManifest manifest;
	manifest.annotations.present = true;
	manifest.annotations.file = voltz::kAnnotationsFileName;
	manifest.annotations.width = 2560;
	manifest.annotations.height = 1440;
	manifest.annotations.hasOrigin = true;
	manifest.annotations.origin.type = "monitor";
	manifest.annotations.origin.id = "\\\\.\\DISPLAY1";
	manifest.annotations.origin.rect[2] = 2560;
	manifest.annotations.origin.rect[3] = 1440;
	manifest.annotations.origin.scale = 1.25;
	manifest.annotationsBurned = true;
	const std::string json = voltz::serializeManifest(manifest);
	CHECK(json.find("\"anotacoes\": {") != std::string::npos);
	CHECK(json.find("\"arquivo\": \"anotacoes.jsonl\"") != std::string::npos);
	CHECK(json.find("\"anotacoes_queimadas\": true") != std::string::npos);
	const voltz::ManifestParse parsed = voltz::parseManifest(json);
	REQUIRE(parsed.ok);
	CHECK(parsed.manifest.annotations.present);
	CHECK(parsed.manifest.annotations.file == "anotacoes.jsonl");
	CHECK(parsed.manifest.annotations.width == 2560);
	CHECK(parsed.manifest.annotations.height == 1440);
	CHECK(parsed.manifest.annotations.hasOrigin);
	CHECK(parsed.manifest.annotations.origin.rect[2] == 2560);
	CHECK(parsed.manifest.annotations.origin.scale == doctest::Approx(1.25));
	CHECK(parsed.manifest.annotationsBurned);
}

TEST_CASE("manifesto: sem anotações nem queimadas, as chaves antigas saem ao reescrever")
{
	ProjectManifest manifest;
	manifest.annotations.present = true;
	manifest.annotations.file = voltz::kAnnotationsFileName;
	manifest.annotations.width = 10;
	manifest.annotations.height = 10;
	manifest.annotationsBurned = true;
	voltz::ManifestParse parsed = voltz::parseManifest(voltz::serializeManifest(manifest));
	REQUIRE(parsed.ok);
	parsed.manifest.annotations = voltz::AnnotationsFile{};
	parsed.manifest.annotationsBurned = false;
	const std::string again = voltz::serializeManifest(parsed.manifest);
	CHECK(again.find("anotacoes") == std::string::npos);
	const voltz::ManifestParse reparsed = voltz::parseManifest(again);
	REQUIRE(reparsed.ok);
	CHECK_FALSE(reparsed.manifest.annotations.present);
	CHECK_FALSE(reparsed.manifest.annotationsBurned);
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: COMPILAR TESTES.
Expected: FAIL — `'annotations': is not a member of 'voltz::ProjectManifest'` / `'kAnnotationsFileName': is not a member`.

- [ ] **Step 3: Modelo**

Em `frontend/voltz/core/project-manifest.hpp`, depois de
`constexpr const char *kCameraIsoFileName = "camera.mkv";`, acrescentar:
```cpp
constexpr const char *kAnnotationsFileName = "anotacoes.jsonl";
```
Depois da struct `IsoFile` (antes do comentário de `ManifestEvent`), acrescentar:
```cpp
// arquivos.anotacoes (4B): traços em pixels da captura da Tela (as coordenadas de tela.mkv); `origin` só com captura
// de monitor (mesmo formato de arquivos.cursor.origem).
struct AnnotationsFile {
	bool present = false;
	std::string file;
	int width = 0;
	int height = 0;
	bool hasOrigin = false;
	CaptureOrigin origin;
};
```
Em `struct ProjectManifest`, depois de `	CursorFile cursor;`, acrescentar:
```cpp
	AnnotationsFile annotations;
	// "anotacoes_queimadas" (spec §6.3): a sobreposição apareceu na captura (Windows < 10 2004 ou afinidade recusada) —
	// os traços estão gravados em tela.mkv e no programa pela própria captura. Só é escrito quando verdadeiro.
	bool annotationsBurned = false;
```

- [ ] **Step 4: Serializar e ler**

Em `frontend/voltz/core/project-manifest.cpp`, em `serializeManifest`, trocar:
```cpp
	} else {
		files.erase(std::string("cursor"));
	}

	if (m.hasMouse) {
```
por:
```cpp
	} else {
		files.erase(std::string("cursor"));
	}
	if (m.annotations.present) {
		Json &annotations = fileEntry(files, "anotacoes");
		annotations["arquivo"] = m.annotations.file;
		annotations["largura"] = m.annotations.width;
		annotations["altura"] = m.annotations.height;
		if (m.annotations.hasOrigin) {
			annotations["origem"] = originJson(m.annotations.origin);
		} else {
			annotations.erase(std::string("origem"));
		}
	} else {
		files.erase(std::string("anotacoes"));
	}
	if (m.annotationsBurned) {
		root["anotacoes_queimadas"] = true;
	} else {
		root.erase(std::string("anotacoes_queimadas"));
	}

	if (m.hasMouse) {
```
Em `parseManifest`, trocar:
```cpp
		readIso(*files, kScreenTrack, m.screen);
		readIso(*files, kCameraTrack, m.camera);
	}
```
por:
```cpp
		readIso(*files, kScreenTrack, m.screen);
		readIso(*files, kCameraTrack, m.camera);
		if (const Json *annotations = getChild(*files, "anotacoes", false)) {
			m.annotations.present = true;
			m.annotations.file = getString(*annotations, "arquivo");
			m.annotations.width = static_cast<int>(getInt(*annotations, "largura", 0));
			m.annotations.height = static_cast<int>(getInt(*annotations, "altura", 0));
			if (const Json *origin = getChild(*annotations, "origem", false)) {
				m.annotations.hasOrigin = true;
				readOrigin(*origin, m.annotations.origin);
			}
		}
	}
	const auto burned = root.find("anotacoes_queimadas");
	m.annotationsBurned = burned != root.end() && burned->is_boolean() && burned->get<bool>();
```

- [ ] **Step 5: Rodar e ver passar**

Run: COMPILAR TESTES, RODAR TESTES.
Expected: PASS; `test cases: 153 | 153 passed`.

- [ ] **Step 6: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/project-manifest.hpp frontend/voltz/core/project-manifest.cpp frontend/voltz/tests/test-project-manifest.cpp
git add frontend/voltz/core/project-manifest.hpp frontend/voltz/core/project-manifest.cpp frontend/voltz/tests/test-project-manifest.cpp
git commit -m "feat(voltz): manifesto com arquivos.anotacoes e anotacoes_queimadas

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 5: Plugin `voltz-sources` — `TargetSize`, `AnnotationFeed`, fonte Voltz Anotações e pedidos de teste

**Files:**
- Create: `plugins/voltz-sources/target-size.hpp`, `plugins/voltz-sources/target-size.cpp`
- Create: `plugins/voltz-sources/annotation-feed.hpp`, `plugins/voltz-sources/annotation-feed.cpp`
- Create: `plugins/voltz-sources/annotation-source.hpp`, `plugins/voltz-sources/annotation-source.cpp`
- Modify: `plugins/voltz-sources/mouse-source.cpp`, `plugins/voltz-sources/test-vendor.hpp`, `plugins/voltz-sources/test-vendor.cpp`,
  `plugins/voltz-sources/voltz-sources.cpp`, `plugins/voltz-sources/CMakeLists.txt`,
  `plugins/voltz-sources/data/locale/en-US.ini`, `plugins/voltz-sources/data/locale/pt-BR.ini`

O plugin não tem testes unitários (depende do libobs): a lógica dele está no núcleo (Tasks 1–3) e o comportamento é
conferido ao vivo no Step 10, com uma cena sintética e o feed alimentado pelos pedidos de teste do próprio plugin.

- [ ] **Step 1: `TargetSize` (extraído da Voltz Mouse)**

Criar `plugins/voltz-sources/target-size.hpp`:
```cpp
#pragma once

#include <obs.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace voltz {

// Tamanho da captura "alvo" de uma fonte Voltz (UUID no ajuste live::kTargetSetting): as camadas da Tela Voltz têm
// exatamente o tamanho da Tela, então o pixel (x,y) da captura cai no mesmo lugar em todas. A Tela pode carregar depois
// da camada: tick() tenta achá-la de novo a cada 60 tiques. setUuid na thread de quem atualiza a fonte; tick na thread
// gráfica; width/height em qualquer thread.
class TargetSize {
public:
	TargetSize() = default;
	~TargetSize();
	TargetSize(const TargetSize &) = delete;
	TargetSize &operator=(const TargetSize &) = delete;

	void setUuid(const std::string &uuid);
	void tick();
	uint32_t width() const { return width_.load(); }
	uint32_t height() const { return height_.load(); }

private:
	void resolve(); // com mutex_ preso

	std::mutex mutex_;
	std::string uuid_;
	obs_weak_source_t *target_ = nullptr;
	int retryTicks_ = 0;
	std::atomic<uint32_t> width_{0};
	std::atomic<uint32_t> height_{0};
};

} // namespace voltz
```
Criar `plugins/voltz-sources/target-size.cpp`:
```cpp
#include "target-size.hpp"

namespace voltz {

namespace {
constexpr int kRetryTicks = 60;
} // namespace

TargetSize::~TargetSize()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (target_) {
		obs_weak_source_release(target_);
		target_ = nullptr;
	}
}

void TargetSize::resolve()
{
	if (target_) {
		obs_weak_source_release(target_);
		target_ = nullptr;
	}
	if (uuid_.empty()) {
		return;
	}
	obs_source_t *target = obs_get_source_by_uuid(uuid_.c_str());
	if (target) {
		target_ = obs_source_get_weak_source(target);
		obs_source_release(target);
	}
}

void TargetSize::setUuid(const std::string &uuid)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (uuid == uuid_ && target_) {
		return;
	}
	uuid_ = uuid;
	retryTicks_ = 0;
	resolve();
}

void TargetSize::tick()
{
	obs_source_t *target = nullptr;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!target_ && !uuid_.empty() && ++retryTicks_ >= kRetryTicks) {
			retryTicks_ = 0;
			resolve();
		}
		if (target_) {
			target = obs_weak_source_get_source(target_);
			if (!target) {
				/* a Tela foi removida: solta e volta a procurar pelo UUID */
				obs_weak_source_release(target_);
				target_ = nullptr;
			}
		}
	}
	uint32_t width = 0;
	uint32_t height = 0;
	if (target) {
		width = obs_source_get_width(target);
		height = obs_source_get_height(target);
		obs_source_release(target);
	}
	width_.store(width);
	height_.store(height);
}

} // namespace voltz
```

- [ ] **Step 2: A Voltz Mouse passa a usar o `TargetSize`** (mesmo comportamento, sem código duplicado)

Em `plugins/voltz-sources/mouse-source.cpp`:
1. Depois de `#include "mouse-feed.hpp"`, acrescentar `#include "target-size.hpp"`.
2. Apagar a linha `constexpr int kTargetRetryTicks = 60;` (e a linha em branco depois dela).
3. Em `struct MouseSource`, trocar:
```cpp
	// Alvo: escrito no update (thread de quem atualiza) e lido no tick (thread gráfica).
	std::mutex targetMutex;
	std::string targetUuid;
	obs_weak_source_t *target = nullptr;
	int retryTicks = 0;
	std::atomic<uint32_t> width{0};
	std::atomic<uint32_t> height{0};
```
por:
```cpp
	TargetSize target;
```
4. Apagar a função inteira (com o comentário de cima):
```cpp
// Com targetMutex preso. A Tela pode carregar depois desta fonte: o tick tenta de novo a cada kTargetRetryTicks.
void resolveTarget(MouseSource *ctx)
{
	if (ctx->target) {
		obs_weak_source_release(ctx->target);
		ctx->target = nullptr;
	}
	if (ctx->targetUuid.empty()) {
		return;
	}
	obs_source_t *target = obs_get_source_by_uuid(ctx->targetUuid.c_str());
	if (target) {
		ctx->target = obs_source_get_weak_source(target);
		obs_source_release(target);
	}
}
```
5. Trocar o corpo de `mouseUpdate`:
```cpp
	auto *ctx = static_cast<MouseSource *>(data);
	const std::string uuid = obs_data_get_string(settings, live::kTargetSetting);
	std::lock_guard<std::mutex> lock(ctx->targetMutex);
	if (uuid == ctx->targetUuid && ctx->target) {
		return;
	}
	ctx->targetUuid = uuid;
	ctx->retryTicks = 0;
	resolveTarget(ctx);
```
por:
```cpp
	static_cast<MouseSource *>(data)->target.setUuid(obs_data_get_string(settings, live::kTargetSetting));
```
6. Em `mouseDestroy`, trocar:
```cpp
	{
		std::lock_guard<std::mutex> lock(ctx->targetMutex);
		if (ctx->target) {
			obs_weak_source_release(ctx->target);
			ctx->target = nullptr;
		}
	}
	delete ctx;
```
por:
```cpp
	delete ctx; /* o TargetSize solta a referência fraca da Tela */
```
7. Trocar `mouseWidth`, `mouseHeight` e `mouseTick` inteiros:
```cpp
uint32_t mouseWidth(void *data)
{
	return static_cast<MouseSource *>(data)->width.load();
}

uint32_t mouseHeight(void *data)
{
	return static_cast<MouseSource *>(data)->height.load();
}

void mouseTick(void *data, float)
{
	auto *ctx = static_cast<MouseSource *>(data);
	obs_source_t *target = nullptr;
	{
		std::lock_guard<std::mutex> lock(ctx->targetMutex);
		if (!ctx->target && !ctx->targetUuid.empty() && ++ctx->retryTicks >= kTargetRetryTicks) {
			ctx->retryTicks = 0;
			resolveTarget(ctx);
		}
		if (ctx->target) {
			target = obs_weak_source_get_source(ctx->target);
			if (!target) {
				/* a Tela foi removida: solta e volta a procurar pelo UUID */
				obs_weak_source_release(ctx->target);
				ctx->target = nullptr;
			}
		}
	}
	uint32_t width = 0;
	uint32_t height = 0;
	if (target) {
		width = obs_source_get_width(target);
		height = obs_source_get_height(target);
		obs_source_release(target);
	}
	ctx->width.store(width);
	ctx->height.store(height);
}
```
por:
```cpp
uint32_t mouseWidth(void *data)
{
	return static_cast<MouseSource *>(data)->target.width();
}

uint32_t mouseHeight(void *data)
{
	return static_cast<MouseSource *>(data)->target.height();
}

void mouseTick(void *data, float)
{
	static_cast<MouseSource *>(data)->target.tick();
}
```
8. Em `mouseRender`, trocar:
```cpp
	const uint32_t width = ctx->width.load();
	const uint32_t height = ctx->height.load();
```
por:
```cpp
	const uint32_t width = ctx->target.width();
	const uint32_t height = ctx->target.height();
```
Conferir: `grep -n "targetMutex\|targetUuid\|retryTicks\|resolveTarget\|width.load\|height.load" plugins/voltz-sources/mouse-source.cpp`
→ sem saída.

- [ ] **Step 3: `AnnotationFeed` e os procedimentos**

Criar `plugins/voltz-sources/annotation-feed.hpp`:
```cpp
#pragma once

#include <voltz/core/annotation-model.hpp>

#include <obs.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace voltz {

// Traços ao vivo das anotações, um estado só no processo. Escrito pelos procedimentos voltz_annot_* (thread da UI do
// frontend) e pelos pedidos de teste (thread do obs-websocket); lido pelas fontes Voltz Anotações na thread gráfica, que
// só copiam os traços quando a versão muda. Trechos curtos sob um mutex; o JSON é lido fora dele.
class AnnotationFeed {
public:
	static AnnotationFeed &instance();

	// Traço novo (ou substitui o de mesmo id) a partir do JSON do núcleo (strokeFromJson). Falso se inválido.
	bool begin(int id, const std::string &json);
	// Acrescenta pontos (pointsFromJson + appendStrokePoints) ao traço `id`.
	void extend(int id, const std::string &pointsJson);
	void remove(int id);
	void clear();
	// "Queimadas" (spec §6.3): a sobreposição aparece na captura, então as fontes não desenham.
	void setBurned(bool burned);
	// Copia os traços para `out` e devolve true se a versão mudou desde `knownVersion` (0 = nunca copiou); `version` e
	// `burned` sempre preenchidos.
	bool snapshotIfChanged(uint64_t knownVersion, std::vector<Stroke> &out, uint64_t &version, bool &burned);
	// {"tracos":N,"versao":V,"queimadas":false,"mostradas":M,"ids":[...]} (pedido de teste AnnotationFeedState).
	std::string describe();

	// Fontes Voltz Anotações sendo mostradas (show/hide do libobs; o destroy desconta o que o hide não descontou).
	void addShowing(int delta) { showing_.fetch_add(delta); }
	int showing() const { return showing_.load(); }

private:
	AnnotationFeed() = default;

	std::mutex mutex_;
	std::vector<Stroke> strokes_;
	uint64_t version_ = 1;
	bool burned_ = false;
	std::atomic<int> showing_{0};
};

// Registra voltz_annot_begin, voltz_annot_extend, voltz_annot_remove, voltz_annot_clear e voltz_annot_burned
// (live-protocol.hpp).
void registerAnnotationProcs(proc_handler_t *handler);

} // namespace voltz
```
Criar `plugins/voltz-sources/annotation-feed.cpp`:
```cpp
#include "annotation-feed.hpp"

#include <voltz/core/live-protocol.hpp>

#include <cstdio>
#include <utility>

namespace voltz {

namespace {

int idOf(calldata_t *params)
{
	return static_cast<int>(calldata_int(params, "id"));
}

void procBegin(void *, calldata_t *params)
{
	const char *json = calldata_string(params, "json");
	if (!json || !AnnotationFeed::instance().begin(idOf(params), json)) {
		blog(LOG_WARNING, "[voltz-sources] traco invalido ignorado");
	}
}

void procExtend(void *, calldata_t *params)
{
	const char *points = calldata_string(params, "points");
	if (points) {
		AnnotationFeed::instance().extend(idOf(params), points);
	}
}

void procRemove(void *, calldata_t *params)
{
	AnnotationFeed::instance().remove(idOf(params));
}

void procClear(void *, calldata_t *)
{
	AnnotationFeed::instance().clear();
}

void procBurned(void *, calldata_t *params)
{
	AnnotationFeed::instance().setBurned(calldata_bool(params, "burned"));
}

} // namespace

AnnotationFeed &AnnotationFeed::instance()
{
	static AnnotationFeed feed;
	return feed;
}

bool AnnotationFeed::begin(int id, const std::string &json)
{
	Stroke stroke;
	if (!strokeFromJson(json, stroke)) {
		return false;
	}
	stroke.id = id;
	std::lock_guard<std::mutex> lock(mutex_);
	for (Stroke &existing : strokes_) {
		if (existing.id == id) {
			existing = std::move(stroke);
			version_++;
			return true;
		}
	}
	strokes_.push_back(std::move(stroke));
	version_++;
	return true;
}

void AnnotationFeed::extend(int id, const std::string &pointsJson)
{
	std::vector<StrokePoint> points;
	if (!pointsFromJson(pointsJson, points) || points.empty()) {
		return;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	for (Stroke &stroke : strokes_) {
		if (stroke.id == id) {
			appendStrokePoints(stroke, points);
			version_++;
			return;
		}
	}
}

void AnnotationFeed::remove(int id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto it = strokes_.begin(); it != strokes_.end(); ++it) {
		if (it->id == id) {
			strokes_.erase(it);
			version_++;
			return;
		}
	}
}

void AnnotationFeed::clear()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!strokes_.empty()) {
		strokes_.clear();
		version_++;
	}
}

void AnnotationFeed::setBurned(bool burned)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (burned_ != burned) {
		burned_ = burned;
		version_++;
		blog(LOG_INFO, "[voltz-sources] anotacoes %s", burned ? "queimadas: a fonte nao desenha" : "desenhadas pela fonte");
	}
}

bool AnnotationFeed::snapshotIfChanged(uint64_t knownVersion, std::vector<Stroke> &out, uint64_t &version,
				       bool &burned)
{
	std::lock_guard<std::mutex> lock(mutex_);
	version = version_;
	burned = burned_;
	if (knownVersion == version_) {
		return false;
	}
	out = strokes_;
	return true;
}

std::string AnnotationFeed::describe()
{
	std::lock_guard<std::mutex> lock(mutex_);
	std::string ids;
	for (const Stroke &stroke : strokes_) {
		if (!ids.empty()) {
			ids += ',';
		}
		ids += std::to_string(stroke.id);
	}
	char buffer[160];
	std::snprintf(buffer, sizeof(buffer), "{\"tracos\":%zu,\"versao\":%llu,\"queimadas\":%s,\"mostradas\":%d,\"ids\":[",
		      strokes_.size(), static_cast<unsigned long long>(version_), burned_ ? "true" : "false",
		      showing_.load());
	return std::string(buffer) + ids + "]}";
}

void registerAnnotationProcs(proc_handler_t *handler)
{
	proc_handler_add(handler, live::kDeclAnnotBegin, procBegin, nullptr);
	proc_handler_add(handler, live::kDeclAnnotExtend, procExtend, nullptr);
	proc_handler_add(handler, live::kDeclAnnotRemove, procRemove, nullptr);
	proc_handler_add(handler, live::kDeclAnnotClear, procClear, nullptr);
	proc_handler_add(handler, live::kDeclAnnotBurned, procBurned, nullptr);
}

} // namespace voltz
```

- [ ] **Step 4: A fonte Voltz Anotações**

Criar `plugins/voltz-sources/annotation-source.hpp`:
```cpp
#pragma once

namespace voltz {

// Registra a fonte "voltz_annotations_source" (Voltz Anotações): tamanho = tamanho da captura do ajuste "alvo" (UUID);
// desenha os traços do AnnotationFeed com o rasterizador do núcleo (o mesmo da sobreposição) numa textura que só é
// refeita quando os traços mudam. Com "queimadas", não desenha. Fica fora do "Adicionar fonte"
// (OBS_SOURCE_CAP_DISABLED): quem cria é o preset/migração do Voltz.
void registerAnnotationsSource();

} // namespace voltz
```
Criar `plugins/voltz-sources/annotation-source.cpp`:
```cpp
#include "annotation-source.hpp"
#include "annotation-feed.hpp"
#include "target-size.hpp"

#include <voltz/core/annotation-raster.hpp>
#include <voltz/core/live-protocol.hpp>

#include <obs-module.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace voltz {

namespace {

struct AnnotationSource {
	TargetSize target;
	// Se esta fonte está contada em AnnotationFeed::showing (ver o mesmo campo da Voltz Mouse).
	std::atomic<bool> counted{false};
	// Só na thread gráfica (render; destroy dentro de obs_enter_graphics):
	gs_texture_t *texture = nullptr;
	uint32_t textureWidth = 0;
	uint32_t textureHeight = 0;
	uint64_t builtVersion = 0;
	bool hasStrokes = false;
	std::vector<uint8_t> pixels;
	std::vector<Stroke> strokes;
};

void annotationsShow(void *data)
{
	if (!static_cast<AnnotationSource *>(data)->counted.exchange(true)) {
		AnnotationFeed::instance().addShowing(1);
	}
}

void annotationsHide(void *data)
{
	if (static_cast<AnnotationSource *>(data)->counted.exchange(false)) {
		AnnotationFeed::instance().addShowing(-1);
	}
}

const char *annotationsName(void *)
{
	return obs_module_text("VoltzAnnotations");
}

void annotationsUpdate(void *data, obs_data_t *settings)
{
	static_cast<AnnotationSource *>(data)->target.setUuid(obs_data_get_string(settings, live::kTargetSetting));
}

void *annotationsCreate(obs_data_t *settings, obs_source_t *)
{
	auto *ctx = new AnnotationSource();
	annotationsUpdate(ctx, settings);
	return ctx;
}

void annotationsDestroy(void *data)
{
	auto *ctx = static_cast<AnnotationSource *>(data);
	annotationsHide(ctx);
	if (ctx->texture) {
		obs_enter_graphics();
		gs_texture_destroy(ctx->texture);
		obs_leave_graphics();
	}
	delete ctx;
}

void annotationsDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, live::kTargetSetting, "");
}

uint32_t annotationsWidth(void *data)
{
	return static_cast<AnnotationSource *>(data)->target.width();
}

uint32_t annotationsHeight(void *data)
{
	return static_cast<AnnotationSource *>(data)->target.height();
}

void annotationsTick(void *data, float)
{
	static_cast<AnnotationSource *>(data)->target.tick();
}

// Refaz a imagem (CPU, núcleo) e manda para a textura. Só quando a versão do feed ou o tamanho muda.
void rebuild(AnnotationSource *ctx, uint32_t width, uint32_t height, uint64_t version)
{
	ctx->pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
	const RasterImage image{ctx->pixels.data(), static_cast<int>(width), static_cast<int>(height),
				static_cast<int>(width) * 4};
	clearRaster(image);
	for (const Stroke &stroke : ctx->strokes) {
		rasterizeStroke(image, stroke, 1.0, 1.0);
	}
	if (!ctx->texture || ctx->textureWidth != width || ctx->textureHeight != height) {
		if (ctx->texture) {
			gs_texture_destroy(ctx->texture);
		}
		ctx->texture = gs_texture_create(width, height, GS_BGRA, 1, nullptr, GS_DYNAMIC);
		ctx->textureWidth = width;
		ctx->textureHeight = height;
	}
	if (ctx->texture) {
		gs_texture_set_image(ctx->texture, ctx->pixels.data(), width * 4, false);
	}
	ctx->hasStrokes = !ctx->strokes.empty();
	ctx->builtVersion = version;
}

void annotationsRender(void *data, gs_effect_t *effect)
{
	auto *ctx = static_cast<AnnotationSource *>(data);
	const uint32_t width = ctx->target.width();
	const uint32_t height = ctx->target.height();
	if (!width || !height || !effect) {
		return;
	}
	const bool resized = width != ctx->textureWidth || height != ctx->textureHeight;
	uint64_t version = 0;
	bool burned = false;
	const bool changed =
		AnnotationFeed::instance().snapshotIfChanged(resized ? 0 : ctx->builtVersion, ctx->strokes, version, burned);
	if (burned) {
		return;
	}
	if (changed || resized) {
		if (ctx->strokes.empty() && !ctx->texture) {
			/* nada a desenhar e nada alocado: não cria textura à toa */
			ctx->hasStrokes = false;
			ctx->builtVersion = version;
			return;
		}
		rebuild(ctx, width, height, version);
	}
	if (!ctx->hasStrokes || !ctx->texture) {
		return;
	}
	/* Fonte sem OBS_SOURCE_SRGB: a imagem é BGRA pré-multiplicada em espaço gama (a mesma da sobreposição). O render
	 * principal liga o framebuffer sRGB; desliga aqui para a mistura ficar em gama, igual ao UpdateLayeredWindow. */
	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(false);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), ctx->texture);
	gs_draw_sprite(ctx->texture, 0, width, height);
	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(previous);
}

} // namespace

void registerAnnotationsSource()
{
	obs_source_info info = {};
	info.id = live::kAnnotationsSourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	/* sem OBS_SOURCE_CUSTOM_DRAW: o libobs chama o render dentro da técnica "Draw" do efeito padrão */
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CAP_DISABLED;
	info.get_name = annotationsName;
	info.create = annotationsCreate;
	info.destroy = annotationsDestroy;
	info.update = annotationsUpdate;
	info.get_defaults = annotationsDefaults;
	info.get_width = annotationsWidth;
	info.get_height = annotationsHeight;
	info.video_tick = annotationsTick;
	info.video_render = annotationsRender;
	info.show = annotationsShow;
	info.hide = annotationsHide;
	info.icon_type = OBS_ICON_TYPE_CUSTOM;
	obs_register_source(&info);
}

} // namespace voltz
```

- [ ] **Step 5: Pedidos de teste do plugin**

Substituir `plugins/voltz-sources/test-vendor.hpp` inteiro por:
```cpp
#pragma once

namespace voltz {

// Só com VOLTZ_TEST_LIVE (chamar no obs_module_post_load): fornecedor "voltz-teste" do obs-websocket.
// Mouse (4A): InjectMouse {x,y,inside?,click?: "l"|"r"|"m"}, MouseState (x, y, inside, ripples, showing, style),
// SetStyle {style}, AddMouseSource {sceneName, targetName, name?}.
// Anotações (4B): AnnotationFeedState ("estado": texto JSON do AnnotationFeed), AddAnnotationSource {sceneName,
// targetName, name?}, FeedStroke {id, json} e FeedClear (alimentam o feed direto, sem o frontend).
// O CreateInput do websocket recusa fonte OBS_SOURCE_CAP_DISABLED, por isso os Add*. Nada mexe no mouse do usuário.
void registerTestVendor();

} // namespace voltz
```
Substituir `plugins/voltz-sources/test-vendor.cpp` inteiro por:
```cpp
#include "test-vendor.hpp"
#include "annotation-feed.hpp"
#include "mouse-feed.hpp"

#include <voltz/core/live-protocol.hpp>
#include <voltz/core/mouse-style.hpp>

#include <obs-websocket-api.h>
#include <util/platform.h>

namespace voltz {

namespace {

void injectMouse(obs_data_t *request, obs_data_t *response, void *)
{
	obs_data_set_default_bool(request, "inside", true);
	const int x = static_cast<int>(obs_data_get_int(request, "x"));
	const int y = static_cast<int>(obs_data_get_int(request, "y"));
	MouseFeed::instance().setPosition(x, y, obs_data_get_bool(request, "inside"));
	const char *click = obs_data_get_string(request, "click");
	if (click && *click) {
		const MouseButton button = click[0] == 'r'   ? MouseButton::Right
					   : click[0] == 'm' ? MouseButton::Middle
							     : MouseButton::Left;
		MouseFeed::instance().addClick(x, y, button, static_cast<int64_t>(os_gettime_ns()));
	}
	obs_data_set_bool(response, "ok", true);
}

void mouseState(obs_data_t *, obs_data_t *response, void *)
{
	const MouseSnapshot state = MouseFeed::instance().snapshot(static_cast<int64_t>(os_gettime_ns()));
	obs_data_set_int(response, "x", state.x);
	obs_data_set_int(response, "y", state.y);
	obs_data_set_bool(response, "inside", state.inside);
	obs_data_set_int(response, "ripples", static_cast<long long>(state.ripples.size()));
	obs_data_set_int(response, "showing", MouseFeed::instance().showing());
	obs_data_set_string(response, "style", mouseStyleToJson(state.style).c_str());
}

void setStyle(obs_data_t *request, obs_data_t *response, void *)
{
	obs_data_set_bool(response, "ok", MouseFeed::instance().setStyleJson(obs_data_get_string(request, "style")));
}

// Cria uma fonte Voltz `id` com alvo = UUID de `targetName` e a põe por cima na cena `sceneName`.
void addTargetedSource(obs_data_t *request, obs_data_t *response, const char *id, const char *defaultName)
{
	obs_source_t *sceneSource = obs_get_source_by_name(obs_data_get_string(request, "sceneName"));
	obs_source_t *target = obs_get_source_by_name(obs_data_get_string(request, "targetName"));
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	bool ok = false;
	if (scene && target) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, live::kTargetSetting, obs_source_get_uuid(target));
		const char *name = obs_data_get_string(request, "name");
		obs_source_t *source = obs_source_create(id, *name ? name : defaultName, settings, nullptr);
		if (source) {
			ok = obs_scene_add(scene, source) != nullptr;
			obs_source_release(source);
		}
		obs_data_release(settings);
	}
	obs_source_release(target);
	obs_source_release(sceneSource);
	obs_data_set_bool(response, "ok", ok);
}

void addMouseSource(obs_data_t *request, obs_data_t *response, void *)
{
	addTargetedSource(request, response, live::kMouseSourceId, "Mouse teste");
}

void addAnnotationSource(obs_data_t *request, obs_data_t *response, void *)
{
	addTargetedSource(request, response, live::kAnnotationsSourceId, "Anotacoes teste");
}

void annotationFeedState(obs_data_t *, obs_data_t *response, void *)
{
	obs_data_set_string(response, "estado", AnnotationFeed::instance().describe().c_str());
	obs_data_set_bool(response, "ok", true);
}

void feedStroke(obs_data_t *request, obs_data_t *response, void *)
{
	const bool ok = AnnotationFeed::instance().begin(static_cast<int>(obs_data_get_int(request, "id")),
							 obs_data_get_string(request, "json"));
	obs_data_set_bool(response, "ok", ok);
}

void feedClear(obs_data_t *, obs_data_t *response, void *)
{
	AnnotationFeed::instance().clear();
	obs_data_set_bool(response, "ok", true);
}

} // namespace

void registerTestVendor()
{
	obs_websocket_vendor vendor = obs_websocket_register_vendor(live::kTestVendor);
	if (!vendor) {
		blog(LOG_WARNING, "[voltz-sources] obs-websocket ausente: sem pedidos de teste");
		return;
	}
	obs_websocket_vendor_register_request(vendor, "InjectMouse", injectMouse, nullptr);
	obs_websocket_vendor_register_request(vendor, "MouseState", mouseState, nullptr);
	obs_websocket_vendor_register_request(vendor, "SetStyle", setStyle, nullptr);
	obs_websocket_vendor_register_request(vendor, "AddMouseSource", addMouseSource, nullptr);
	obs_websocket_vendor_register_request(vendor, "AddAnnotationSource", addAnnotationSource, nullptr);
	obs_websocket_vendor_register_request(vendor, "AnnotationFeedState", annotationFeedState, nullptr);
	obs_websocket_vendor_register_request(vendor, "FeedStroke", feedStroke, nullptr);
	obs_websocket_vendor_register_request(vendor, "FeedClear", feedClear, nullptr);
	blog(LOG_WARNING, "[voltz-sources] modo de teste (%s): pedidos '%s' ligados", live::kTestEnv,
	     live::kTestVendor);
}

} // namespace voltz
```

- [ ] **Step 6: Módulo, CMake e textos**

Substituir `plugins/voltz-sources/voltz-sources.cpp` inteiro por:
```cpp
#include "annotation-feed.hpp"
#include "annotation-source.hpp"
#include "mouse-feed.hpp"
#include "mouse-source.hpp"
#include "test-vendor.hpp"

#include <voltz/core/live-protocol.hpp>

#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("voltz-sources", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Voltz Studio: fontes Voltz (destaque do mouse e anotacoes)";
}

bool obs_module_load(void)
{
	voltz::registerMouseSource();
	voltz::registerMouseProcs(obs_get_proc_handler());
	voltz::registerAnnotationsSource();
	voltz::registerAnnotationProcs(obs_get_proc_handler());
	blog(LOG_INFO, "[voltz-sources] carregado");
	return true;
}

void obs_module_post_load(void)
{
	/* a API de fornecedor do obs-websocket só pode ser usada aqui (depois de todos os módulos carregarem) */
	if (voltz::live::testModeEnabled()) {
		voltz::registerTestVendor();
	}
}
```
Em `plugins/voltz-sources/CMakeLists.txt`, trocar o comentário e a lista de fontes:
```cmake
# Voltz Studio: fontes próprias (Voltz Mouse; a Voltz Anotações entra no Plano 4B). Adicionado por
# frontend/cmake/feature-voltz.cmake depois do voltz-core (que este módulo usa), e não por plugins/CMakeLists.txt.
add_library(voltz-sources MODULE)
add_library(Voltz::sources ALIAS voltz-sources)

target_sources(
  voltz-sources
  PRIVATE
    mouse-feed.cpp
    mouse-feed.hpp
    mouse-source.cpp
    mouse-source.hpp
    test-vendor.cpp
    test-vendor.hpp
    voltz-sources.cpp
)
```
por:
```cmake
# Voltz Studio: fontes próprias (Voltz Mouse e Voltz Anotações). Adicionado por frontend/cmake/feature-voltz.cmake
# depois do voltz-core (que este módulo usa), e não por plugins/CMakeLists.txt.
add_library(voltz-sources MODULE)
add_library(Voltz::sources ALIAS voltz-sources)

target_sources(
  voltz-sources
  PRIVATE
    annotation-feed.cpp
    annotation-feed.hpp
    annotation-source.cpp
    annotation-source.hpp
    mouse-feed.cpp
    mouse-feed.hpp
    mouse-source.cpp
    mouse-source.hpp
    target-size.cpp
    target-size.hpp
    test-vendor.cpp
    test-vendor.hpp
    voltz-sources.cpp
)
```
Locales do plugin (UTF-8 sem BOM): acrescentar no fim de `plugins/voltz-sources/data/locale/pt-BR.ini` a linha
`VoltzAnnotations="Voltz Anotações"` e no fim de `plugins/voltz-sources/data/locale/en-US.ini` a linha
`VoltzAnnotations="Voltz Annotations"`. Conferir a paridade do plugin (comando das Regras fixas) → sem saída.

- [ ] **Step 7: Formatar, configurar e compilar**

```bash
"$CLANGFMT" -i plugins/voltz-sources/target-size.hpp plugins/voltz-sources/target-size.cpp plugins/voltz-sources/annotation-feed.hpp plugins/voltz-sources/annotation-feed.cpp plugins/voltz-sources/annotation-source.hpp plugins/voltz-sources/annotation-source.cpp plugins/voltz-sources/mouse-source.cpp plugins/voltz-sources/test-vendor.hpp plugins/voltz-sources/test-vendor.cpp plugins/voltz-sources/voltz-sources.cpp
```
Run: CONFIGURAR; depois COMPILAR APP (segundo plano).
Expected: `exit=0`, nenhuma linha de erro.

- [ ] **Step 8: Conferir os arquivos instalados do módulo**

Run: `ls build_x64/rundir/RelWithDebInfo/obs-plugins/64bit/voltz-sources.dll build_x64/rundir/RelWithDebInfo/data/obs-plugins/voltz-sources/locale/pt-BR.ini`
Expected: os dois existem. (A Voltz Anotações não tem `.effect`: usa o efeito padrão do libobs.)

- [ ] **Step 9: Teste ao vivo do render pelo feed (sem frontend)**

Bloco de teste com `$liveTest = $true`, `$standins = $false`, `$overlayOffscreen = $false`, `$burned = $false`,
`$layersV1 = $false`. Passos no `try` (cena sintética 1920×1080 = canvas: pixel (x,y) ≈ (x/3, y/3) na imagem):
```powershell
Assert-NoRealCapture
[void](Ws '[{"requestType":"CreateScene","requestData":{"sceneName":"Anotacoes teste"}},{"requestType":"CreateInput","requestData":{"sceneName":"Anotacoes teste","inputName":"Fundo teste","inputKind":"color_source_v3","inputSettings":{"color":4281346602,"width":1920,"height":1080}}},{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Anotacoes teste"}}]')
$add = Vendor "AddAnnotationSource" '{"sceneName":"Anotacoes teste","targetName":"Fundo teste"}'
Start-Sleep -Seconds 2
$feed = Feed-State; "mostradas=$($feed.mostradas) tracos=$($feed.tracos) queimadas=$($feed.queimadas)"
# linha lima esp 12 de (480,540) a (1440,540) -> imagem y=180, x=160..480
$line = '{"id":1,"ferr":"linha","cor":"#D4F53C","esp":12,"pts":[[480,540],[1440,540]]}'
[void](Vendor "FeedStroke" ('{"id":1,"json":' + (ConvertTo-Json $line) + '}'))
Start-Sleep -Milliseconds 300
$img = Scene-Image "Anotacoes teste"; $on = $img.GetPixel(320, 180); $off = $img.GetPixel(320, 150); $img.Dispose()
"linha=$on fora=$off"
# marca-texto vermelho esp 8 (24 px a 40%) com juncao em (960,810) -> imagem (320,270)
$hl = '{"id":2,"ferr":"marca_texto","cor":"#FF4D4D","esp":8,"pts":[[480,810],[960,810],[1440,810]]}'
[void](Vendor "FeedStroke" ('{"id":2,"json":' + (ConvertTo-Json $hl) + '}'))
Start-Sleep -Milliseconds 300
$img = Scene-Image "Anotacoes teste"; $mark = $img.GetPixel(320, 270); $img.Dispose()
"marca-texto=$mark"
[void](Vendor "FeedClear")
Start-Sleep -Milliseconds 300
$img = Scene-Image "Anotacoes teste"; $after = $img.GetPixel(320, 180); $img.Dispose()
"depois de limpar=$after"
Close-App; Save-Log
LogLines "[voltz-sources]"
```
Expected: `mostradas=1 tracos=0 queimadas=False`; `linha` ≈ (212,245,60) e `fora` ≈ (42,42,48) (`Near`); `marca-texto`
≈ (127,56,60) — **não** (168,…) nem mais escuro na junção; `depois de limpar` ≈ (42,42,48); log com
`[voltz-sources] carregado` e `[voltz-sources] modo de teste`. Se a cor do marca-texto sair muito diferente (ex.
(83,…)), o framebuffer sRGB não foi desligado: confira o `gs_enable_framebuffer_srgb(false)` no render.

- [ ] **Step 10: A Voltz Mouse continua igual (regressão do `TargetSize`)**

Run: SMOKE MOUSE.
Expected: `SMOKE MOUSE OK` (o grupo ainda tem 2 camadas: a Voltz Anotações entra nos presets na Task 6).

- [ ] **Step 11: Commit**

```bash
git add plugins/voltz-sources/target-size.hpp plugins/voltz-sources/target-size.cpp plugins/voltz-sources/annotation-feed.hpp plugins/voltz-sources/annotation-feed.cpp plugins/voltz-sources/annotation-source.hpp plugins/voltz-sources/annotation-source.cpp plugins/voltz-sources/mouse-source.cpp plugins/voltz-sources/test-vendor.hpp plugins/voltz-sources/test-vendor.cpp plugins/voltz-sources/voltz-sources.cpp plugins/voltz-sources/CMakeLists.txt plugins/voltz-sources/data/locale/en-US.ini plugins/voltz-sources/data/locale/pt-BR.ini
git commit -m "feat(voltz): fonte Voltz Anotacoes no plugin (feed com mutex, procedimentos voltz_annot_*, TargetSize)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Tela Voltz com 3 camadas (presets e migração 1 → 2), fonte compartilhada e ícone

**Files:**
- Modify: `frontend/voltz/shell/SharedSources.hpp`, `frontend/voltz/shell/SharedSources.cpp`
- Modify: `frontend/voltz/shell/PresetBuilder.hpp`, `frontend/voltz/shell/PresetBuilder.cpp`
- Modify: `frontend/voltz/shell/SourcesPanel.cpp`, `frontend/voltz/shell/voltz-shell.qrc`
- Create: `frontend/voltz/shell/icons/regular/pencil-simple.svg`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`, `tools/voltz-smoke-mouse.ps1`

- [ ] **Step 1: Fonte compartilhada "Voltz Anotações"**

Em `frontend/voltz/shell/SharedSources.hpp`, trocar:
```cpp
// Mouse = fonte "Voltz Mouse" (plugin voltz-sources) com o ajuste "alvo" = UUID da Tela; só existe com o plugin.
enum class SharedSource { Screen, Camera, Background, Mouse };
```
por:
```cpp
// Mouse = fonte "Voltz Mouse" e Annotations = "Voltz Anotações" (plugin voltz-sources), ambas com o ajuste "alvo" = UUID
// da Tela; só existem com o plugin.
enum class SharedSource { Screen, Camera, Background, Mouse, Annotations };
```
e, no comentário de `ensureSharedSource`, logo depois de `Mouse = Voltz Mouse` (fim de linha; a frase continua na
linha seguinte com `// apontando para a Tela)`), acrescentar ` e Annotations = Voltz Anotações`.

Em `frontend/voltz/shell/SharedSources.cpp`, em `configKey`, trocar:
```cpp
	case SharedSource::Mouse:
		return "MouseSourceUuid";
	}
```
por:
```cpp
	case SharedSource::Mouse:
		return "MouseSourceUuid";
	case SharedSource::Annotations:
		return "AnnotationsSourceUuid";
	}
```
e, em `createShared`, logo depois do bloco `case SharedSource::Mouse: { ... break; }` (antes do `}` que fecha o
`switch`), acrescentar:
```cpp
	case SharedSource::Annotations: {
		if (!obs_source_get_display_name(live::kAnnotationsSourceId)) {
			blog(LOG_WARNING, "[voltz] plugin voltz-sources ausente: sem a fonte Voltz Anotacoes");
			return {};
		}
		id = live::kAnnotationsSourceId;
		name = QTStr("Voltz.Source.Annotations");
		OBSSource screen = findSharedSource(SharedSource::Screen);
		obs_data_set_string(settings, live::kTargetSetting, screen ? obs_source_get_uuid(screen) : "");
		break;
	}
```

- [ ] **Step 2: Grupos versão 2 nos presets**

Em `frontend/voltz/shell/PresetBuilder.cpp`, trocar:
```cpp
// Configurações privadas (salvas na coleção): na Tela, se a migração já rodou; no grupo, a versão das camadas
// (1 = Voltz Mouse; o Plano 4B sobe para 2 ao acrescentar a Voltz Anotações).
constexpr const char *kScreenMigratedKey = "voltz_tela_migrada";
constexpr const char *kScreenLayersKey = "voltz_tela_camadas";
constexpr int kScreenLayersVersion = 1;
```
por:
```cpp
// Configurações privadas (salvas na coleção): na Tela, se a migração já rodou; no grupo, a versão das camadas
// (1 = Tela + Voltz Mouse, do 4A; 2 = Tela + Voltz Anotações + Voltz Mouse, do 4B).
constexpr const char *kScreenMigratedKey = "voltz_tela_migrada";
constexpr const char *kScreenLayersKey = "voltz_tela_camadas";
constexpr int kScreenLayersMouseOnly = 1;
constexpr int kScreenLayersVersion = 2;
// Só testes (com VOLTZ_TEST_LIVE): "1" = os presets criam grupos versão 1 e a atualização não roda (testa a migração
// 1 → 2 sem editar a coleção à mão).
constexpr const char *kTestLayersEnv = "VOLTZ_TEST_SCREEN_LAYERS";

bool testLayersV1()
{
	return live::testModeEnabled() && qEnvironmentVariable(kTestLayersEnv) == QStringLiteral("1");
}
```
Trocar a função `syncMouseTarget` inteira:
```cpp
void syncMouseTarget(obs_source_t *mouse, obs_source_t *screen)
{
	const char *uuid = obs_source_get_uuid(screen);
	OBSDataAutoRelease settings = obs_source_get_settings(mouse);
	if (strcmp(obs_data_get_string(settings, live::kTargetSetting), uuid) == 0) {
		return;
	}
	OBSDataAutoRelease update = obs_data_create();
	obs_data_set_string(update, live::kTargetSetting, uuid);
	obs_source_update(mouse, update);
}
```
por:
```cpp
// Aponta o "alvo" de uma camada Voltz (Voltz Mouse ou Voltz Anotações) para a Tela.
void syncTarget(obs_source_t *layer, obs_source_t *screen)
{
	const char *uuid = obs_source_get_uuid(screen);
	OBSDataAutoRelease settings = obs_source_get_settings(layer);
	if (strcmp(obs_data_get_string(settings, live::kTargetSetting), uuid) == 0) {
		return;
	}
	OBSDataAutoRelease update = obs_data_create();
	obs_data_set_string(update, live::kTargetSetting, uuid);
	obs_source_update(layer, update);
}
```
Trocar a função `wrapScreenItem` inteira (do comentário `// Põe \`screenItem\` (item direto da cena) num grupo...`
até o `}` que a fecha) por:
```cpp
// Põe `screenItem` (item direto da cena) num grupo "Tela Voltz" com as camadas Voltz por cima, de baixo para cima:
// Voltz Anotações e Voltz Mouse (versão de camadas 2). O grupo assume a transformação, a ordem, a visibilidade e a trava
// do item; as camadas ficam em (0,0), escala 1, travadas e com o mesmo recorte — o pixel (x,y) da captura cai no mesmo
// lugar em todas. Devolve o item do grupo (null se não deu: sem plugin ou o libobs recusou).
obs_sceneitem_t *wrapScreenItem(obs_scene_t *scene, obs_sceneitem_t *screenItem)
{
	if (!scene || !screenItem) {
		return nullptr;
	}
	OBSSource mouse = ensureSharedSource(SharedSource::Mouse);
	if (!mouse) {
		return nullptr;
	}
	OBSSource annotations = testLayersV1() ? OBSSource() : ensureSharedSource(SharedSource::Annotations);
	obs_source_t *screen = obs_sceneitem_get_source(screenItem);
	syncTarget(mouse, screen);
	if (annotations) {
		syncTarget(annotations, screen);
	}

	obs_transform_info info = {};
	obs_sceneitem_get_info2(screenItem, &info);
	obs_sceneitem_crop crop = {};
	obs_sceneitem_get_crop(screenItem, &crop);
	const int order = obs_sceneitem_get_order_position(screenItem);
	const bool visible = obs_sceneitem_visible(screenItem);
	const bool locked = obs_sceneitem_locked(screenItem);

	obs_sceneitem_t *annotationsItem = annotations ? obs_scene_add(scene, annotations) : nullptr;
	obs_sceneitem_t *mouseItem = obs_scene_add(scene, mouse);
	if (!mouseItem) {
		if (annotationsItem) {
			obs_sceneitem_remove(annotationsItem);
		}
		return nullptr;
	}
	obs_sceneitem_set_visible(screenItem, true);
	std::vector<obs_sceneitem_t *> items = {screenItem};
	if (annotationsItem) {
		items.push_back(annotationsItem);
	}
	items.push_back(mouseItem);
	for (obs_sceneitem_t *layer : items) {
		placeLayer(layer);
		if (layer != screenItem) {
			obs_sceneitem_set_crop(layer, &crop);
		}
	}

	const QString name = uniqueSourceName(QTStr("Voltz.Source.ScreenGroup"));
	obs_sceneitem_t *group = obs_scene_insert_group(scene, name.toUtf8().constData(), items.data(), items.size());
	if (!group) {
		obs_sceneitem_remove(mouseItem);
		if (annotationsItem) {
			obs_sceneitem_remove(annotationsItem);
		}
		obs_sceneitem_set_info2(screenItem, &info);
		obs_sceneitem_set_visible(screenItem, visible);
		blog(LOG_WARNING, "[voltz] tela voltz: o libobs recusou o grupo; Tela mantida como estava");
		return nullptr;
	}
	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(obs_sceneitem_get_source(group));
	obs_data_set_int(privateSettings, kScreenLayersKey,
			 annotationsItem ? kScreenLayersVersion : kScreenLayersMouseOnly);
	for (obs_sceneitem_t *layer : items) {
		obs_sceneitem_set_locked(layer, true);
	}
	obs_sceneitem_set_info2(group, &info);
	obs_sceneitem_set_order_position(group, order);
	obs_sceneitem_set_visible(group, visible);
	obs_sceneitem_set_locked(group, locked);
	return group;
}
```

- [ ] **Step 3: Migração dos grupos versão 1 → 2**

Em `frontend/voltz/shell/PresetBuilder.cpp`, logo depois da função `markScreenMigrated()` (ainda dentro do
`namespace {`), acrescentar:
```cpp
// Grupos "Tela Voltz" da versão 1 (Tela + Voltz Mouse, do 4A) ganham a Voltz Anotações entre as duas (versão 2), em
// (0,0), escala 1, travada e com o recorte da Tela. Devolve quantos grupos mudaram.
int upgradeScreenGroups(obs_source_t *screen)
{
	struct Found {
		std::vector<OBSSceneItem> groups;
	} found;
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		obs_scene_t *scene = obs_scene_from_source(scenes.sources.array[i]);
		if (!scene) {
			continue;
		}
		/* só coleta (com referência): mexer no grupo durante o enum travaria o lock da cena */
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				if (obs_sceneitem_is_group(item)) {
					OBSDataAutoRelease privateSettings =
						obs_source_get_private_settings(obs_sceneitem_get_source(item));
					if (obs_data_get_int(privateSettings, kScreenLayersKey) == kScreenLayersMouseOnly) {
						static_cast<Found *>(param)->groups.emplace_back(item);
					}
				}
				return true;
			},
			&found);
	}
	obs_frontend_source_list_free(&scenes);
	if (found.groups.empty()) {
		return 0;
	}
	OBSSource annotations = ensureSharedSource(SharedSource::Annotations);
	if (!annotations) {
		return 0;
	}
	syncTarget(annotations, screen);
	int upgraded = 0;
	for (const OBSSceneItem &group : found.groups) {
		obs_scene_t *inner = obs_sceneitem_group_get_scene(group);
		obs_sceneitem_t *layer = inner ? obs_scene_add(inner, annotations) : nullptr;
		if (!layer) {
			continue;
		}
		placeLayer(layer);
		if (obs_sceneitem_t *screenLayer = obs_scene_find_source(inner, obs_source_get_name(screen))) {
			obs_sceneitem_crop crop = {};
			obs_sceneitem_get_crop(screenLayer, &crop);
			obs_sceneitem_set_crop(layer, &crop);
		}
		obs_sceneitem_set_order_position(layer, 1);
		obs_sceneitem_set_locked(layer, true);
		OBSDataAutoRelease privateSettings = obs_source_get_private_settings(obs_sceneitem_get_source(group));
		obs_data_set_int(privateSettings, kScreenLayersKey, kScreenLayersVersion);
		upgraded++;
	}
	blog(LOG_INFO, "[voltz] tela voltz: %d grupo(s) com a camada de anotacoes", upgraded);
	return upgraded;
}
```
Trocar a função `ensureScreenGroups()` inteira (do `void ensureScreenGroups()` até o `}` final dela, antes de
`} // namespace voltz`) por:
```cpp
void ensureScreenGroups()
{
	OBSSource screen = findSharedSource(SharedSource::Screen);
	if (!screen) {
		return;
	}
	if (OBSSource mouse = findSharedSource(SharedSource::Mouse)) {
		syncTarget(mouse, screen);
	}
	if (OBSSource annotations = findSharedSource(SharedSource::Annotations)) {
		syncTarget(annotations, screen);
	}
	const bool plugin = obs_source_get_display_name(live::kMouseSourceId) != nullptr;
	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(screen);
	int wrapped = 0;
	if (!obs_data_get_bool(privateSettings, kScreenMigratedKey)) {
		if (!plugin) {
			blog(LOG_WARNING, "[voltz] tela voltz: plugin voltz-sources ausente; migracao adiada");
			return;
		}
		struct Found {
			obs_source_t *screen = nullptr;
			obs_scene_t *scene = nullptr;
			std::vector<std::pair<OBSScene, OBSSceneItem>> items;
		} found;
		found.screen = screen;
		obs_frontend_source_list scenes = {};
		obs_frontend_get_scenes(&scenes);
		for (size_t i = 0; i < scenes.sources.num; i++) {
			found.scene = obs_scene_from_source(scenes.sources.array[i]);
			if (!found.scene) {
				continue;
			}
			/* só coleta (com referência): mexer na cena durante o enum travaria o lock dela */
			obs_scene_enum_items(
				found.scene,
				[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
					auto *search = static_cast<Found *>(param);
					if (!obs_sceneitem_is_group(item) &&
					    obs_sceneitem_get_source(item) == search->screen) {
						search->items.emplace_back(OBSScene(search->scene), OBSSceneItem(item));
					}
					return true;
				},
				&found);
		}
		obs_frontend_source_list_free(&scenes);
		for (auto &entry : found.items) {
			if (wrapScreenItem(entry.first, entry.second)) {
				wrapped++;
			}
		}
		obs_data_set_bool(privateSettings, kScreenMigratedKey, true);
		blog(LOG_INFO, "[voltz] tela voltz: %d item(ns) migrado(s)", wrapped);
	}
	/* versão de camadas: independe da migração (coleções migradas no 4A têm grupos versão 1) */
	const bool annotationsPlugin = obs_source_get_display_name(live::kAnnotationsSourceId) != nullptr;
	const int upgraded = plugin && annotationsPlugin && !testLayersV1() ? upgradeScreenGroups(screen) : 0;
	if (wrapped > 0 || upgraded > 0) {
		obs_frontend_save();
	}
}
```
Em `frontend/voltz/shell/PresetBuilder.hpp`, trocar o comentário de `ensureScreenGroups`:
```cpp
// Tela Voltz (spec §4.4) nas cenas que já existiam: na primeira vez em cada coleção, cada item DIRETO da Tela
// compartilhada vira um grupo "Tela Voltz" com a camada Voltz Mouse (o grupo herda transformação, ordem, visibilidade e
// trava). Marca a Tela (voltz_tela_migrada, nas configurações privadas salvas na coleção) para não repetir — quem
// desagrupar não vê o grupo voltar. Acerta também o "alvo" da Voltz Mouse. Sem o plugin voltz-sources, adia. Thread
// da UI.
```
por:
```cpp
// Tela Voltz (spec §4.4) nas cenas que já existiam: na primeira vez em cada coleção, cada item DIRETO da Tela
// compartilhada vira um grupo "Tela Voltz" com as camadas Voltz Anotações e Voltz Mouse (o grupo herda transformação,
// ordem, visibilidade e trava). Marca a Tela (voltz_tela_migrada, nas configurações privadas salvas na coleção) para não
// repetir — quem desagrupar não vê o grupo voltar. Depois, sempre: grupos da versão de camadas 1 (4A) ganham a Voltz
// Anotações (versão 2). Acerta também o "alvo" das camadas. Sem o plugin voltz-sources, adia. Thread da UI.
```

- [ ] **Step 4: Ícone, textos e lista de Fontes**

Baixar o SVG oficial (versão exata) e conferir:
```bash
curl -fsSL "https://unpkg.com/@phosphor-icons/core@2.1.1/assets/regular/pencil-simple.svg" -o frontend/voltz/shell/icons/regular/pencil-simple.svg
head -c 80 frontend/voltz/shell/icons/regular/pencil-simple.svg; echo
```
Expected: começa com `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" fill="currentColor">`.
Em `frontend/voltz/shell/voltz-shell.qrc`, depois de `    <file>icons/regular/pause.svg</file>`, acrescentar
`    <file>icons/regular/pencil-simple.svg</file>`. Conferir: `grep -c "<file>" frontend/voltz/shell/voltz-shell.qrc` → `84`.

Em `frontend/voltz/shell/SourcesPanel.cpp`, trocar:
```cpp
	const QIcon sourceIcon = row.isGroup ? phosphorIcon(QStringLiteral("stack"), palette::kMuted)
				 : strcmp(sourceId, live::kMouseSourceId) == 0
					 ? phosphorIcon(QStringLiteral("cursor-click"), palette::kMuted)
					 : OBSBasic::Get()->GetSourceIcon(sourceId);
```
por:
```cpp
	QIcon sourceIcon;
	if (row.isGroup) {
		sourceIcon = phosphorIcon(QStringLiteral("stack"), palette::kMuted);
	} else if (strcmp(sourceId, live::kMouseSourceId) == 0) {
		sourceIcon = phosphorIcon(QStringLiteral("cursor-click"), palette::kMuted);
	} else if (strcmp(sourceId, live::kAnnotationsSourceId) == 0) {
		sourceIcon = phosphorIcon(QStringLiteral("pencil-simple"), palette::kMuted);
	} else {
		sourceIcon = OBSBasic::Get()->GetSourceIcon(sourceId);
	}
```
Locales da casca (UTF-8 sem BOM), logo depois da linha que começa com `Voltz.Source.ScreenGroup=`:
- `frontend/data/locale/voltz-pt-BR.ini`: `Voltz.Source.Annotations="Voltz Anotações"`
- `frontend/data/locale/voltz-en-US.ini`: `Voltz.Source.Annotations="Voltz Annotations"`
Conferir a paridade da casca (comando das Regras fixas) → sem saída.

- [ ] **Step 5: O smoke do mouse passa a esperar 3 camadas**

Em `tools/voltz-smoke-mouse.ps1` (ASCII; nada de acento), trocar:
1. No cabeçalho: `confere o grupo "Tela Voltz" (Tela + Voltz Mouse),` → `confere o grupo "Tela Voltz" (Tela + Voltz Anotacoes + Voltz Mouse),`
2. A linha
```powershell
    Check "A: grupo = Tela (embaixo) + Voltz Mouse, travadas em (0,0)" ($inner.Count -eq 2 -and $inner[0].sourceName -eq "Tela" -and $inner[1].sourceName -eq "Voltz Mouse" -and $inner[1].inputKind -eq "voltz_mouse_source" -and $inner[0].sceneItemLocked -and $inner[1].sceneItemLocked -and $inner[1].sceneItemTransform.positionX -eq 0)
```
por
```powershell
    Check "A: grupo = Tela (embaixo) + Voltz Anotacoes + Voltz Mouse, travadas em (0,0)" ($inner.Count -eq 3 -and $inner[0].sourceName -eq "Tela" -and $inner[1].inputKind -eq "voltz_annotations_source" -and $inner[2].sourceName -eq "Voltz Mouse" -and $inner[2].inputKind -eq "voltz_mouse_source" -and $inner[0].sceneItemLocked -and $inner[1].sceneItemLocked -and $inner[2].sceneItemLocked -and $inner[2].sceneItemTransform.positionX -eq 0)
```
3. A linha
```powershell
    Check "B2: grupo com Tela + Voltz Mouse" ($inner.Count -eq 2 -and $inner[0].sourceName -eq "Tela" -and $inner[1].sourceName -eq "Voltz Mouse")
```
por
```powershell
    Check "B2: grupo com Tela + Voltz Anotacoes + Voltz Mouse" ($inner.Count -eq 3 -and $inner[0].sourceName -eq "Tela" -and $inner[1].inputKind -eq "voltz_annotations_source" -and $inner[2].sourceName -eq "Voltz Mouse")
```
4. A linha
```powershell
    Check "B3: cena com 2 itens e grupo intacto (2 camadas)" ($i3.Count -eq 2 -and $r.Count -eq 1 -and @($r[0].data.sceneItems).Count -eq 2)
```
por
```powershell
    Check "B3: cena com 2 itens e grupo intacto (3 camadas)" ($i3.Count -eq 2 -and $r.Count -eq 1 -and @($r[0].data.sceneItems).Count -eq 3)
```
Conferir que o arquivo continua ASCII: `grep -nP '[^\x00-\x7F]' tools/voltz-smoke-mouse.ps1` → sem saída.

- [ ] **Step 6: Formatar, configurar e compilar**

```bash
"$CLANGFMT" -i frontend/voltz/shell/SharedSources.hpp frontend/voltz/shell/SharedSources.cpp frontend/voltz/shell/PresetBuilder.hpp frontend/voltz/shell/PresetBuilder.cpp frontend/voltz/shell/SourcesPanel.cpp
grep -n "syncMouseTarget" frontend/voltz/shell/PresetBuilder.cpp; echo "sobras=$?"
```
Expected do `grep`: nenhuma linha e `sobras=1`.
Run: CONFIGURAR (ícone novo no `.qrc`); COMPILAR APP (segundo plano).
Expected: `exit=0`, nenhuma linha de erro.

- [ ] **Step 7: Presets com 3 camadas e a Voltz Mouse intacta**

Run: SMOKE MOUSE.
Expected: `SMOKE MOUSE OK` (fases A e B agora com 3 camadas).

- [ ] **Step 8: Migração 1 → 2 (grupo do 4A ganha a camada)**

Bloco de teste com `$liveTest = $true`, `$standins = $true`, `$overlayOffscreen = $false`, `$burned = $false`,
**`$layersV1 = $true`**. Passos no `try`:
```powershell
# 1) grupos versao 1 (como os que o 4A criou)
[void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
$l1 = Screen-Layers "Só tela"
"v1: " + (($l1 | ForEach-Object { $_.inputKind }) -join ', ')
Close-App; Save-Log
# 2) sem a variavel: o grupo ganha a camada de anotacoes no meio
Remove-Item Env:VOLTZ_TEST_SCREEN_LAYERS
Start-App
$l2 = Screen-Layers "Só tela"
"v2: " + (($l2 | ForEach-Object { "$($_.inputKind)@$($_.sceneItemTransform.positionX),$($_.sceneItemTransform.positionY) travada=$($_.sceneItemLocked)" }) -join ' | ')
Close-App; Save-Log; LogLines "[voltz] tela voltz:"
# 3) reabrir: nada a fazer
Start-App; $l3 = Screen-Layers "Só tela"; "v3: $($l3.Count) camadas"
Close-App; Save-Log; LogLines "[voltz] tela voltz:"
```
Expected: `v1: color_source_v3, voltz_mouse_source`; `v2: color_source_v3@0,0 travada=True | voltz_annotations_source@0,0
travada=True | voltz_mouse_source@0,0 travada=True`; o log da segunda abertura tem
`[voltz] tela voltz: 1 grupo(s) com a camada de anotacoes`; `v3: 3 camadas` e o log da terceira abertura **sem** linha
`[voltz] tela voltz:`.

- [ ] **Step 9: Commit**

```bash
git add frontend/voltz/shell/SharedSources.hpp frontend/voltz/shell/SharedSources.cpp frontend/voltz/shell/PresetBuilder.hpp frontend/voltz/shell/PresetBuilder.cpp frontend/voltz/shell/SourcesPanel.cpp frontend/voltz/shell/voltz-shell.qrc frontend/voltz/shell/icons/regular/pencil-simple.svg frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini tools/voltz-smoke-mouse.ps1
git commit -m "feat(voltz): Tela Voltz com a camada Voltz Anotacoes (presets e migracao da versao 1 para 2)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: `OverlayWindow` — janela Win32 de sobreposição, sempre transparente a cliques

**Files:**
- Create: `frontend/voltz/annotate/OverlayWindow.hpp`, `frontend/voltz/annotate/OverlayWindow.cpp`
- Modify: `frontend/cmake/feature-voltz.cmake`

Sem teste próprio: é exercitada ao vivo na Task 8 (pelo `AnnotationController`), sempre fora da área de trabalho.

- [ ] **Step 1: Cabeçalho**

Criar `frontend/voltz/annotate/OverlayWindow.hpp`:
```cpp
#pragma once

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/cursor-track.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Sobreposição das anotações sobre um monitor (spec §6.2). Janela Win32 PURA — não é QWidget: o OBSApp::notify
// reaplicaria nela a afinidade "esconder janelas do OBS" do usuário (WDA_NONE com a opção desligada). WS_POPUP em
// camadas, sempre no topo e SEMPRE transparente a cliques: WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
// WS_EX_TOOLWINDOW | WS_EX_TOPMOST, WM_NCHITTEST = HTTRANSPARENT, WM_MOUSEACTIVATE = MA_NOACTIVATE. NÃO existe método que
// tire o WS_EX_TRANSPARENT (o modo desenho do Plano 4C é outro código, com saídas garantidas); cada render reconfere os
// bits obrigatórios. Desenha com o rasterizador do núcleo num DIB BGRA pré-multiplicado e publica com
// UpdateLayeredWindow. Thread da UI (as mensagens da janela chegam pelo laço de eventos do Qt).
class OverlayWindow {
public:
	struct Info {
		bool exists = false;
		bool visible = false;
		uint64_t hwnd = 0;
		int64_t exStyle = 0;
		uint32_t affinity = 0;
		MonitorRect rect;
	};

	OverlayWindow() = default;
	~OverlayWindow();
	OverlayWindow(const OverlayWindow &) = delete;
	OverlayWindow &operator=(const OverlayWindow &) = delete;

	// Cria escondida sobre `rect` (pixels físicos do desktop). Falso se a janela não nasceu.
	bool create(const MonitorRect &rect);
	void destroy();
	// WDA_EXCLUDEFROMCAPTURE, conferido por GetWindowDisplayAffinity. Falso = a janela aparece na captura. Só chamar com
	// SetDisplayAffinitySupported() (antes do Windows 10 2004 a flag vira caixa preta).
	bool excludeFromCapture();
	// Novo retângulo (vale no próximo render).
	void setRect(const MonitorRect &rect);
	// Desenha `strokes` (pixels da captura captureWidth×captureHeight, esticados para o retângulo) e mostra sem ativar;
	// sem traços, limpa e esconde.
	void render(const std::vector<Stroke> &strokes, int captureWidth, int captureHeight);
	// Pixel do último desenho como "#AARRGGBB" (pré-multiplicado); "" fora do buffer. Só para testes.
	std::string pixelHex(int x, int y) const;
	Info info() const;

private:
	void ensureClickThrough();
	bool ensureBuffer(int width, int height);
	void releaseBuffer();
	void hide();

	void *hwnd_ = nullptr; // HWND
	void *memoryDc_ = nullptr;
	void *bitmap_ = nullptr;
	void *previousBitmap_ = nullptr;
	uint8_t *bits_ = nullptr;
	int bufferWidth_ = 0;
	int bufferHeight_ = 0;
	MonitorRect rect_;
	bool visible_ = false;
};

} // namespace voltz
```

- [ ] **Step 2: Implementação**

Criar `frontend/voltz/annotate/OverlayWindow.cpp`:
```cpp
#include "OverlayWindow.hpp"

#include <voltz/core/annotation-raster.hpp>

#include <util/base.h>

#include <cstddef>
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace voltz {

#ifdef _WIN32
namespace {

const wchar_t *kOverlayClass = L"VoltzAnnotationOverlay";
// Transparente a cliques (TRANSPARENT + LAYERED), nunca ativa (NOACTIVATE), fora da barra de tarefas e do Alt+Tab
// (TOOLWINDOW), sempre no topo (TOPMOST). Obrigatórios em toda a vida da janela no Plano 4B.
constexpr LONG_PTR kClickThroughStyle =
	WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

LRESULT CALLBACK overlayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_NCHITTEST:
		return HTTRANSPARENT; /* o clique segue para a janela de baixo */
	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;
	case WM_DPICHANGED:
		return 0; /* o retângulo é sempre o do monitor, em pixels físicos */
	case WM_CLOSE:
		return 0; /* só o Voltz destrói a sobreposição */
	default:
		break;
	}
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool registerOverlayClass()
{
	static bool registered = false;
	if (!registered) {
		WNDCLASSEXW windowClass = {};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = overlayProc;
		windowClass.hInstance = GetModuleHandleW(nullptr);
		windowClass.lpszClassName = kOverlayClass;
		registered = RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	}
	return registered;
}

} // namespace
#endif

OverlayWindow::~OverlayWindow()
{
	destroy();
}

bool OverlayWindow::create(const MonitorRect &rect)
{
#ifdef _WIN32
	destroy();
	if (rect.width <= 0 || rect.height <= 0 || !registerOverlayClass()) {
		return false;
	}
	HWND hwnd = CreateWindowExW(static_cast<DWORD>(kClickThroughStyle), kOverlayClass, L"Voltz Anotações",
				    WS_POPUP, rect.left, rect.top, rect.width, rect.height, nullptr, nullptr,
				    GetModuleHandleW(nullptr), nullptr);
	if (!hwnd) {
		blog(LOG_WARNING, "[voltz] anotacoes: CreateWindowEx falhou (%lu)", GetLastError());
		return false;
	}
	hwnd_ = hwnd;
	rect_ = rect;
	visible_ = false;
	return true;
#else
	(void)rect;
	return false;
#endif
}

void OverlayWindow::destroy()
{
#ifdef _WIN32
	if (hwnd_) {
		DestroyWindow(static_cast<HWND>(hwnd_));
		hwnd_ = nullptr;
	}
	releaseBuffer();
	visible_ = false;
#endif
}

bool OverlayWindow::excludeFromCapture()
{
#ifdef _WIN32
	HWND hwnd = static_cast<HWND>(hwnd_);
	if (!hwnd || !SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)) {
		return false;
	}
	DWORD affinity = 0;
	return GetWindowDisplayAffinity(hwnd, &affinity) && affinity == WDA_EXCLUDEFROMCAPTURE;
#else
	return false;
#endif
}

void OverlayWindow::setRect(const MonitorRect &rect)
{
	rect_ = rect;
}

void OverlayWindow::ensureClickThrough()
{
#ifdef _WIN32
	HWND hwnd = static_cast<HWND>(hwnd_);
	const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
	if ((style & kClickThroughStyle) != kClickThroughStyle) {
		SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style | kClickThroughStyle);
		blog(LOG_WARNING, "[voltz] anotacoes: estilo da sobreposicao refeito (transparente a cliques)");
	}
#endif
}

bool OverlayWindow::ensureBuffer(int width, int height)
{
#ifdef _WIN32
	if (bits_ && width == bufferWidth_ && height == bufferHeight_) {
		return true;
	}
	releaseBuffer();
	if (width <= 0 || height <= 0) {
		return false;
	}
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height; /* de cima para baixo, como o RasterImage */
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	HDC memoryDc = CreateCompatibleDC(nullptr);
	if (!memoryDc) {
		return false;
	}
	void *bits = nullptr;
	HBITMAP bitmap = CreateDIBSection(memoryDc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
	if (!bitmap || !bits) {
		if (bitmap) {
			DeleteObject(bitmap);
		}
		DeleteDC(memoryDc);
		return false;
	}
	previousBitmap_ = SelectObject(memoryDc, bitmap);
	memoryDc_ = memoryDc;
	bitmap_ = bitmap;
	bits_ = static_cast<uint8_t *>(bits);
	bufferWidth_ = width;
	bufferHeight_ = height;
	return true;
#else
	(void)width;
	(void)height;
	return false;
#endif
}

void OverlayWindow::releaseBuffer()
{
#ifdef _WIN32
	if (memoryDc_) {
		SelectObject(static_cast<HDC>(memoryDc_), static_cast<HGDIOBJ>(previousBitmap_));
		DeleteDC(static_cast<HDC>(memoryDc_));
	}
	if (bitmap_) {
		DeleteObject(static_cast<HGDIOBJ>(bitmap_));
	}
#endif
	memoryDc_ = nullptr;
	bitmap_ = nullptr;
	previousBitmap_ = nullptr;
	bits_ = nullptr;
	bufferWidth_ = 0;
	bufferHeight_ = 0;
}

void OverlayWindow::hide()
{
#ifdef _WIN32
	if (bits_) {
		clearRaster(RasterImage{bits_, bufferWidth_, bufferHeight_, bufferWidth_ * 4});
	}
	if (visible_ && hwnd_) {
		ShowWindow(static_cast<HWND>(hwnd_), SW_HIDE);
	}
	visible_ = false;
#endif
}

void OverlayWindow::render(const std::vector<Stroke> &strokes, int captureWidth, int captureHeight)
{
#ifdef _WIN32
	HWND hwnd = static_cast<HWND>(hwnd_);
	if (!hwnd) {
		return;
	}
	ensureClickThrough();
	if (strokes.empty() || captureWidth <= 0 || captureHeight <= 0) {
		hide();
		return;
	}
	if (!ensureBuffer(rect_.width, rect_.height)) {
		blog(LOG_WARNING, "[voltz] anotacoes: sem memoria para a sobreposicao %dx%d", rect_.width, rect_.height);
		hide();
		return;
	}
	const RasterImage image{bits_, bufferWidth_, bufferHeight_, bufferWidth_ * 4};
	clearRaster(image);
	const double scaleX = static_cast<double>(bufferWidth_) / static_cast<double>(captureWidth);
	const double scaleY = static_cast<double>(bufferHeight_) / static_cast<double>(captureHeight);
	for (const Stroke &stroke : strokes) {
		rasterizeStroke(image, stroke, scaleX, scaleY);
	}
	GdiFlush();
	POINT position{rect_.left, rect_.top};
	SIZE size{bufferWidth_, bufferHeight_};
	POINT source{0, 0};
	BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
	if (!UpdateLayeredWindow(hwnd, nullptr, &position, &size, static_cast<HDC>(memoryDc_), &source, 0, &blend,
				 ULW_ALPHA)) {
		blog(LOG_WARNING, "[voltz] anotacoes: UpdateLayeredWindow falhou (%lu)", GetLastError());
		return;
	}
	if (!visible_) {
		/* mostra SEM ativar: o foco do usuário não muda */
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
		visible_ = true;
	}
#else
	(void)strokes;
	(void)captureWidth;
	(void)captureHeight;
#endif
}

std::string OverlayWindow::pixelHex(int x, int y) const
{
	if (!bits_ || x < 0 || y < 0 || x >= bufferWidth_ || y >= bufferHeight_) {
		return {};
	}
	const uint8_t *pixel =
		bits_ + (static_cast<size_t>(y) * static_cast<size_t>(bufferWidth_) + static_cast<size_t>(x)) * 4;
	char text[16];
	std::snprintf(text, sizeof(text), "#%02X%02X%02X%02X", pixel[3], pixel[2], pixel[1], pixel[0]);
	return text;
}

OverlayWindow::Info OverlayWindow::info() const
{
	Info info;
	info.rect = rect_;
#ifdef _WIN32
	HWND hwnd = static_cast<HWND>(hwnd_);
	if (!hwnd) {
		return info;
	}
	info.exists = IsWindow(hwnd) != FALSE;
	info.visible = IsWindowVisible(hwnd) != FALSE;
	info.hwnd = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(hwnd));
	info.exStyle = static_cast<int64_t>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
	DWORD affinity = 0;
	if (GetWindowDisplayAffinity(hwnd, &affinity)) {
		info.affinity = static_cast<uint32_t>(affinity);
	}
#endif
	return info;
}

} // namespace voltz
```

- [ ] **Step 3: Conferência de segurança do código (obrigatória)**

```bash
grep -nE "SetCapture|SetForegroundWindow|SetFocus|SetActiveWindow|RegisterHotKey|SetWindowsHookEx|SendInput|SetCursorPos|mouse_event|keybd_event" frontend/voltz/annotate/OverlayWindow.cpp; echo "achados=$?"
grep -nE "&\s*~" frontend/voltz/annotate/OverlayWindow.cpp; echo "achados=$?"
```
Expected: nenhuma linha e `achados=1` nas duas (nada que capture entrada, roube o foco ou tire bits do estilo).

- [ ] **Step 4: Compilar**

Em `frontend/cmake/feature-voltz.cmake`, na lista de `target_sources(obs-studio PRIVATE ...)`, antes de
`    voltz/project/CursorTracker.cpp`, acrescentar:
```cmake
    voltz/annotate/OverlayWindow.cpp
    voltz/annotate/OverlayWindow.hpp
```
```bash
"$CLANGFMT" -i frontend/voltz/annotate/OverlayWindow.hpp frontend/voltz/annotate/OverlayWindow.cpp
```
Run: CONFIGURAR; COMPILAR APP (segundo plano).
Expected: `exit=0`, nenhuma linha de erro. (`WDA_EXCLUDEFROMCAPTURE` vem do SDK ≥ 10.0.19041; o projeto exige ≥ 10.0.20348.)

- [ ] **Step 5: Commit**

```bash
git add frontend/voltz/annotate/OverlayWindow.hpp frontend/voltz/annotate/OverlayWindow.cpp frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): janela de sobreposicao das anotacoes (Win32 em camadas, sempre transparente a cliques)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 8: `AnnotationController`, fornecedor de teste da casca e ligação na casca

**Files:**
- Create: `frontend/voltz/annotate/AnnotationController.hpp`, `frontend/voltz/annotate/AnnotationController.cpp`
- Create: `frontend/voltz/annotate/AnnotationTestVendor.hpp`, `frontend/voltz/annotate/AnnotationTestVendor.cpp`
- Modify: `frontend/cmake/feature-voltz.cmake`, `frontend/voltz/shell/VoltzShell.hpp`, `frontend/voltz/shell/VoltzShell.cpp`

- [ ] **Step 1: Cabeçalho do controlador**

Criar `frontend/voltz/annotate/AnnotationController.hpp`:
```cpp
#pragma once

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/cursor-track.hpp>
#include <voltz/core/stroke-store.hpp>

#include <QObject>
#include <QTimer>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace voltz {

class OverlayWindow;

// Anotações ao vivo (spec §6.2). Dono do StrokeStore (traços em pixels da captura da Tela compartilhada); repassa cada
// mudança ao plugin voltz-sources (procedimentos voltz_annot_*, desenhados pela fonte Voltz Anotações no programa), à
// sobreposição do monitor da Tela (OverlayWindow: transparente a cliques, fora da captura por WDA_EXCLUDEFROMCAPTURE) e a
// quem grava (ProjectRecorder → anotacoes.jsonl, pelos sinais). "Queimadas" (Windows < 10 2004 ou afinidade recusada,
// spec §6.3): a sobreposição fica sem afinidade (aparece na captura), a fonte não desenha (não duplica) e o manifesto
// marca anotacoes_queimadas. "Somem após N s" pelo user.ini [Voltz] AnnotationStyle. No Plano 4B os traços entram só
// pelo fornecedor de teste (VOLTZ_TEST_LIVE); o modo desenho é do 4C. Thread da UI.
class AnnotationController : public QObject {
	Q_OBJECT

public:
	explicit AnnotationController(QObject *parent = nullptr);
	~AnnotationController() override;

	// EXIT do frontend, antes de o libobs descarregar o plugin: destrói a sobreposição, para os temporizadores e os
	// pedidos de teste; depois disso nada mais é enviado ao plugin.
	void shutdown();

	// Começa um traço (termina antes o que estiver em andamento). Devolve o id (0 se desligado).
	int beginStroke(AnnotationTool tool, const RgbColor &color, int width, const StrokePoint &first);
	void extendStroke(const std::vector<StrokePoint> &points);
	void endStroke();
	void undo();
	void clear();
	// Só testes: guarda os pixels da sobreposição nesses pontos (pixels da janela) para o estado de teste.
	void probeOverlay(const std::vector<StrokePoint> &points);

	const std::vector<LiveStroke> &visibleStrokes() const { return store_.visible(); }
	const AnnotationStyle &style() const { return style_; }
	bool burned() const { return burned_; }

signals:
	// Direto (mesma thread): o ProjectRecorder escreve o diário na hora.
	void strokeEnded(const voltz::LiveStroke &stroke);
	void strokesRemoved(const std::vector<voltz::RemovedStroke> &removed);
	void burnedChanged(bool burned);

private:
	void removed(const std::vector<RemovedStroke> &strokes, bool all);
	void scheduleExpiry();
	void expire();
	int64_t lifetimeNs() const;
	// Recalcula o alvo da sobreposição (monitor da Tela) e redesenha se os traços (`strokesChanged`) ou o alvo mudaram.
	void refreshOverlay(bool strokesChanged);
	bool overlayTarget(MonitorRect &rect, int &captureWidth, int &captureHeight) const;
	void markBurned(const char *why);
	void publishTestState();

	StrokeStore store_;
	AnnotationStyle style_;
	std::unique_ptr<OverlayWindow> overlay_;
	MonitorRect overlayRect_;
	int captureWidth_ = 0;
	int captureHeight_ = 0;
	QTimer expiry_;
	QTimer overlayPoll_;
	std::vector<std::string> samples_;
	bool burned_ = false;
	bool testMode_ = false;
	bool shutdown_ = false;
	int overlayState_ = -1; // -1 desconhecido, 0 sem alvo, 1 com alvo (só para o log)
};

} // namespace voltz
```

- [ ] **Step 2: Implementação do controlador**

Criar `frontend/voltz/annotate/AnnotationController.cpp`:
```cpp
#include "AnnotationController.hpp"
#include "AnnotationTestVendor.hpp"
#include "OverlayWindow.hpp"

#include <voltz/core/live-protocol.hpp>
#include <voltz/project/MonitorLocator.hpp>
#include <voltz/shell/SharedSources.hpp>

#include <OBSApp.hpp>           // App()
#include <utility/platform.hpp> // SetDisplayAffinitySupported

#include <obs.hpp>
#include <util/config-file.h>
#include <util/platform.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QChar>
#include <QStringList>

#include <algorithm>
#include <cstring>
#include <utility>

#include "moc_AnnotationController.cpp"

namespace voltz {

namespace {

constexpr int kOverlayPollMs = 1000;
constexpr int64_t kNsPerMs = 1000000;
constexpr int64_t kMaxTimerMs = 3600000;

int64_t nowNs()
{
	return static_cast<int64_t>(os_gettime_ns());
}

// Procedimento do plugin. Sem o plugin, avisa uma vez: as anotações ficam só na sobreposição.
void callProc(const char *name, calldata_t *params)
{
	if (!proc_handler_call(obs_get_proc_handler(), name, params)) {
		static bool warned = false;
		if (!warned) {
			warned = true;
			blog(LOG_WARNING, "[voltz] plugin voltz-sources ausente: anotacoes sem a fonte Voltz Anotacoes");
		}
	}
}

void sendBegin(const Stroke &stroke)
{
	calldata_t params;
	calldata_init(&params);
	calldata_set_int(&params, "id", stroke.id);
	calldata_set_string(&params, "json", strokeToJsonLine(stroke).c_str());
	callProc(live::kProcAnnotBegin, &params);
	calldata_free(&params);
}

void sendExtend(int id, const std::vector<StrokePoint> &points)
{
	calldata_t params;
	calldata_init(&params);
	calldata_set_int(&params, "id", id);
	calldata_set_string(&params, "points", pointsToJson(points).c_str());
	callProc(live::kProcAnnotExtend, &params);
	calldata_free(&params);
}

void sendRemove(int id)
{
	uint8_t stack[128];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_int(&params, "id", id);
	callProc(live::kProcAnnotRemove, &params);
}

void sendClear()
{
	uint8_t stack[64];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	callProc(live::kProcAnnotClear, &params);
}

void sendBurned(bool burned)
{
	uint8_t stack[128];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_bool(&params, "burned", burned);
	callProc(live::kProcAnnotBurned, &params);
}

bool sameRect(const MonitorRect &a, const MonitorRect &b)
{
	return a.left == b.left && a.top == b.top && a.width == b.width && a.height == b.height;
}

// VOLTZ_TEST_OVERLAY_RECT="x,y,largura,altura" (só com VOLTZ_TEST_LIVE): retângulo da sobreposição no lugar do monitor
// da Tela — os testes o põem fora da área de trabalho.
bool testOverlayRect(MonitorRect &rect)
{
	if (!live::testModeEnabled()) {
		return false;
	}
	const QStringList parts = qEnvironmentVariable(live::kTestOverlayRectEnv).split(QLatin1Char(','));
	if (parts.size() != 4) {
		return false;
	}
	int values[4] = {0, 0, 0, 0};
	for (int i = 0; i < 4; i++) {
		bool ok = false;
		values[i] = parts[i].trimmed().toInt(&ok);
		if (!ok) {
			return false;
		}
	}
	if (values[2] <= 0 || values[3] <= 0) {
		return false;
	}
	rect = MonitorRect{values[0], values[1], values[2], values[3]};
	return true;
}

} // namespace

AnnotationController::AnnotationController(QObject *parent) : QObject(parent)
{
	testMode_ = live::testModeEnabled();
	const char *json = config_get_string(App()->GetUserConfig(), "Voltz", "AnnotationStyle");
	if (json && *json && !annotationStyleFromJson(json, style_)) {
		blog(LOG_WARNING, "[voltz] AnnotationStyle invalido no user.ini; usando o padrao");
	}
	if (style_.fadeSeconds > 0) {
		blog(LOG_INFO, "[voltz] anotacoes: somem apos %d s", style_.fadeSeconds);
	}
	const bool forced = testMode_ && qEnvironmentVariableIsSet(live::kTestBurnedEnv);
	/* Antes do Windows 10 2004 o WDA_EXCLUDEFROMCAPTURE vira caixa preta: nem tenta (spec §6.3). */
	burned_ = forced || !SetDisplayAffinitySupported();
	if (burned_) {
		blog(LOG_WARNING, "[voltz] anotacoes: queimadas (%s)",
		     forced ? "teste" : "Windows sem WDA_EXCLUDEFROMCAPTURE");
	}
	sendBurned(burned_);
	expiry_.setSingleShot(true);
	connect(&expiry_, &QTimer::timeout, this, [this]() { expire(); });
	overlayPoll_.setInterval(kOverlayPollMs);
	connect(&overlayPoll_, &QTimer::timeout, this, [this]() { refreshOverlay(false); });
	if (testMode_) {
		AnnotationTestVendor::start(this);
	}
	publishTestState();
}

AnnotationController::~AnnotationController()
{
	shutdown();
}

void AnnotationController::shutdown()
{
	if (shutdown_) {
		return;
	}
	shutdown_ = true;
	if (testMode_) {
		AnnotationTestVendor::stop();
	}
	expiry_.stop();
	overlayPoll_.stop();
	if (overlay_) {
		overlay_->destroy();
		overlay_.reset();
	}
	blog(LOG_INFO, "[voltz] anotacoes: desligadas (sobreposicao destruida)");
}

int AnnotationController::beginStroke(AnnotationTool tool, const RgbColor &color, int width, const StrokePoint &first)
{
	if (shutdown_) {
		return 0;
	}
	if (store_.openId() != 0) {
		endStroke();
	}
	const int id = store_.begin(tool, color, width, first, nowNs());
	if (id == 0) {
		return 0;
	}
	sendBegin(store_.visible().back().stroke);
	refreshOverlay(true);
	return id;
}

void AnnotationController::extendStroke(const std::vector<StrokePoint> &points)
{
	if (shutdown_ || !store_.extend(points)) {
		return;
	}
	sendExtend(store_.openId(), points);
	refreshOverlay(true);
}

void AnnotationController::endStroke()
{
	if (shutdown_) {
		return;
	}
	const LiveStroke *ended = store_.end(nowNs());
	if (!ended) {
		return;
	}
	const LiveStroke copy = *ended;
	emit strokeEnded(copy);
	scheduleExpiry();
	publishTestState();
}

void AnnotationController::undo()
{
	if (!shutdown_) {
		removed(store_.undo(nowNs()), false);
	}
}

void AnnotationController::clear()
{
	if (!shutdown_) {
		removed(store_.clear(nowNs()), true);
	}
}

void AnnotationController::removed(const std::vector<RemovedStroke> &strokes, bool all)
{
	if (strokes.empty()) {
		return;
	}
	if (all) {
		sendClear();
	} else {
		for (const RemovedStroke &stroke : strokes) {
			sendRemove(stroke.live.stroke.id);
		}
	}
	emit strokesRemoved(strokes);
	scheduleExpiry();
	refreshOverlay(true);
	blog(LOG_INFO, "[voltz] anotacoes: %d traco(s) removido(s), %d na tela", static_cast<int>(strokes.size()),
	     static_cast<int>(store_.visible().size()));
}

int64_t AnnotationController::lifetimeNs() const
{
	return static_cast<int64_t>(style_.fadeSeconds) * 1000 * kNsPerMs;
}

void AnnotationController::scheduleExpiry()
{
	const int64_t next = store_.nextExpiryNs(lifetimeNs());
	if (next < 0 || shutdown_) {
		expiry_.stop();
		return;
	}
	const int64_t waitMs = std::clamp<int64_t>((next - nowNs()) / kNsPerMs + 1, 0, kMaxTimerMs);
	expiry_.start(static_cast<int>(waitMs));
}

void AnnotationController::expire()
{
	if (shutdown_) {
		return;
	}
	removed(store_.expire(nowNs(), lifetimeNs()), false);
	scheduleExpiry();
}

bool AnnotationController::overlayTarget(MonitorRect &rect, int &captureWidth, int &captureHeight) const
{
	OBSSource screen = findSharedSource(SharedSource::Screen);
	if (!screen) {
		return false;
	}
	captureWidth = static_cast<int>(obs_source_get_width(screen));
	captureHeight = static_cast<int>(obs_source_get_height(screen));
	if (captureWidth <= 0 || captureHeight <= 0) {
		return false;
	}
	if (testOverlayRect(rect)) {
		return true;
	}
	if (strcmp(obs_source_get_unversioned_id(screen), "monitor_capture") != 0) {
		return false;
	}
	OBSDataAutoRelease settings = obs_source_get_settings(screen);
	const MonitorDescription monitor = findMonitor(obs_data_get_string(settings, "monitor_id"));
	if (!monitor.found) {
		return false;
	}
	rect = monitor.rect;
	return true;
}

void AnnotationController::refreshOverlay(bool strokesChanged)
{
	if (shutdown_) {
		return;
	}
	MonitorRect rect;
	int captureWidth = 0;
	int captureHeight = 0;
	const bool hasTarget = overlayTarget(rect, captureWidth, captureHeight);
	const int state = hasTarget ? 1 : 0;
	if (state != overlayState_) {
		overlayState_ = state;
		if (hasTarget) {
			blog(LOG_INFO, "[voltz] anotacoes: sobreposicao em (%d,%d %dx%d) para a captura %dx%d", rect.left,
			     rect.top, rect.width, rect.height, captureWidth, captureHeight);
		} else {
			blog(LOG_INFO, "[voltz] anotacoes: sem captura de monitor; sem sobreposicao");
		}
	}
	const std::vector<Stroke> strokes = store_.strokes();
	if (!hasTarget) {
		if (overlay_) {
			overlay_->render({}, 0, 0);
		}
		overlayPoll_.stop();
		publishTestState();
		return;
	}
	const bool moved = !sameRect(rect, overlayRect_) || captureWidth != captureWidth_ || captureHeight != captureHeight_;
	if (!strokesChanged && !moved) {
		return;
	}
	overlayRect_ = rect;
	captureWidth_ = captureWidth;
	captureHeight_ = captureHeight;
	if (!overlay_ && strokes.empty()) {
		overlayPoll_.stop();
		publishTestState();
		return;
	}
	if (!overlay_) {
		auto overlay = std::make_unique<OverlayWindow>();
		if (!overlay->create(rect)) {
			blog(LOG_WARNING, "[voltz] anotacoes: a sobreposicao nao foi criada");
			publishTestState();
			return;
		}
		overlay_ = std::move(overlay);
		if (!burned_ && !overlay_->excludeFromCapture()) {
			markBurned("afinidade recusada");
		}
		blog(LOG_INFO, "[voltz] anotacoes: sobreposicao criada (%s)",
		     burned_ ? "visivel na captura" : "fora da captura");
	} else {
		overlay_->setRect(rect);
	}
	overlay_->render(strokes, captureWidth, captureHeight);
	if (strokes.empty()) {
		overlayPoll_.stop();
	} else if (!overlayPoll_.isActive()) {
		overlayPoll_.start();
	}
	publishTestState();
}

void AnnotationController::markBurned(const char *why)
{
	if (burned_) {
		return;
	}
	burned_ = true;
	blog(LOG_WARNING, "[voltz] anotacoes: queimadas (%s)", why);
	sendBurned(true);
	emit burnedChanged(true);
}

void AnnotationController::probeOverlay(const std::vector<StrokePoint> &points)
{
	samples_.clear();
	for (const StrokePoint &point : points) {
		samples_.push_back(overlay_ ? overlay_->pixelHex(point.x, point.y) : std::string());
	}
	publishTestState();
}

void AnnotationController::publishTestState()
{
	if (!testMode_) {
		return;
	}
	QJsonArray ids;
	for (const LiveStroke &live : store_.visible()) {
		ids.append(live.stroke.id);
	}
	const OverlayWindow::Info info = overlay_ ? overlay_->info() : OverlayWindow::Info{};
	QJsonObject overlay;
	overlay.insert(QStringLiteral("criada"), info.exists);
	overlay.insert(QStringLiteral("visivel"), info.visible);
	overlay.insert(QStringLiteral("hwnd"), QString::number(info.hwnd));
	overlay.insert(QStringLiteral("estilo_ex"), QString::number(info.exStyle));
	overlay.insert(QStringLiteral("afinidade"), static_cast<int>(info.affinity));
	overlay.insert(QStringLiteral("retangulo"),
		       QJsonArray{info.rect.left, info.rect.top, info.rect.width, info.rect.height});
	QJsonArray samples;
	for (const std::string &sample : samples_) {
		samples.append(QString::fromStdString(sample));
	}
	OBSSource screen = findSharedSource(SharedSource::Screen);
	const int captureWidth = screen ? static_cast<int>(obs_source_get_width(screen)) : 0;
	const int captureHeight = screen ? static_cast<int>(obs_source_get_height(screen)) : 0;
	QJsonObject state;
	state.insert(QStringLiteral("visiveis"), ids);
	state.insert(QStringLiteral("aberto"), store_.openId());
	state.insert(QStringLiteral("ultimo"),
		     store_.visible().empty()
			     ? QString()
			     : QString::fromStdString(strokeToJsonLine(store_.visible().back().stroke)));
	state.insert(QStringLiteral("queimadas"), burned_);
	state.insert(QStringLiteral("estilo"), QString::fromStdString(annotationStyleToJson(style_)));
	state.insert(QStringLiteral("captura"), QJsonArray{captureWidth, captureHeight});
	state.insert(QStringLiteral("sobreposicao"), overlay);
	state.insert(QStringLiteral("amostras"), samples);
	state.insert(QStringLiteral("modo_desenho"), false);
	AnnotationTestVendor::publish(QJsonDocument(state).toJson(QJsonDocument::Compact).toStdString());
}

} // namespace voltz
```

- [ ] **Step 3: Fornecedor de teste da casca**

Criar `frontend/voltz/annotate/AnnotationTestVendor.hpp`:
```cpp
#pragma once

#include <string>

namespace voltz {

class AnnotationController;

// Só com VOLTZ_TEST_LIVE: fornecedor "voltz-teste-casca" do obs-websocket, registrado pela casca depois do
// OBSBasic::OBSInit (todos os módulos já carregados; plugins usam o obs_module_post_load pelo mesmo motivo). Dados por
// texto JSON (obs_data não guarda lista de números):
//   InjectStroke {ferr?, cor?, esp?, pts:"[[x,y],...]", aberto?} — começa, estende e (sem "aberto") termina um traço;
//   ExtendStroke {pts}, EndStroke, UndoStroke, ClearStrokes;
//   ProbeOverlay {pts} — guarda os pixels da sobreposição nesses pontos (lidos no próximo AnnotationState);
//   AnnotationState → {"estado": texto JSON publicado pelo AnnotationController, "ok"}.
// O callback roda na thread do websocket, sem lock do obs-websocket: os pedidos que mudam algo são ENFILEIRADOS para a
// thread da UI e respondem na hora (quem testa espera ~400 ms antes de ler o estado). Nada mexe no mouse nem no teclado.
class AnnotationTestVendor {
public:
	// Thread da UI. Registra o fornecedor (uma vez) e aponta os pedidos para `controller`.
	static void start(AnnotationController *controller);
	// EXIT (thread da UI): solta o controlador (pedidos em andamento viram no-op) e tira os pedidos do fornecedor.
	static void stop();
	// Thread da UI: estado devolvido por AnnotationState.
	static void publish(const std::string &stateJson);
};

} // namespace voltz
```
Criar `frontend/voltz/annotate/AnnotationTestVendor.cpp`:
```cpp
#include "AnnotationTestVendor.hpp"
#include "AnnotationController.hpp"

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/live-protocol.hpp>

#include <obs-websocket-api.h>

#include <QMetaObject>

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace voltz {

namespace {

std::mutex gMutex;
AnnotationController *gTarget = nullptr; // protegido por gMutex
std::string gState = "{}";               // protegido por gMutex
obs_websocket_vendor gVendor = nullptr;  // só na thread da UI

constexpr const char *kRequests[] = {"InjectStroke", "ExtendStroke", "EndStroke",      "UndoStroke",
				     "ClearStrokes", "ProbeOverlay", "AnnotationState"};

// Entrega `work` na thread da UI sem esperar; falso se o controlador já saiu (EXIT). O mutex é segurado enquanto o evento
// é postado: o stop() (que anula gTarget antes de o controlador morrer) espera; evento de objeto destruído o Qt descarta.
bool post(std::function<void(AnnotationController *)> work)
{
	std::lock_guard<std::mutex> lock(gMutex);
	if (!gTarget) {
		return false;
	}
	AnnotationController *target = gTarget;
	QMetaObject::invokeMethod(target, [target, work]() { work(target); }, Qt::QueuedConnection);
	return true;
}

std::vector<StrokePoint> pointsOf(obs_data_t *request)
{
	std::vector<StrokePoint> points;
	pointsFromJson(obs_data_get_string(request, "pts"), points);
	return points;
}

void injectStroke(obs_data_t *request, obs_data_t *response, void *)
{
	const std::vector<StrokePoint> points = pointsOf(request);
	const std::string toolText = obs_data_get_string(request, "ferr");
	const std::string colorText = obs_data_get_string(request, "cor");
	const int width = static_cast<int>(obs_data_get_int(request, "esp"));
	const bool open = obs_data_get_bool(request, "aberto");
	bool ok = !points.empty();
	if (ok) {
		ok = post([points, toolText, colorText, width, open](AnnotationController *controller) {
			AnnotationTool tool = controller->style().tool;
			toolFromString(toolText, tool);
			RgbColor color = controller->style().color;
			parseHexColor(colorText, color);
			const int strokeWidth = width > 0 ? width : controller->style().width;
			if (!controller->beginStroke(tool, color, strokeWidth, points.front())) {
				return;
			}
			if (points.size() > 1) {
				controller->extendStroke(std::vector<StrokePoint>(points.begin() + 1, points.end()));
			}
			if (!open) {
				controller->endStroke();
			}
		});
	}
	obs_data_set_bool(response, "ok", ok);
}

void extendStroke(obs_data_t *request, obs_data_t *response, void *)
{
	const std::vector<StrokePoint> points = pointsOf(request);
	const bool ok = !points.empty() &&
			post([points](AnnotationController *controller) { controller->extendStroke(points); });
	obs_data_set_bool(response, "ok", ok);
}

void endStroke(obs_data_t *, obs_data_t *response, void *)
{
	obs_data_set_bool(response, "ok", post([](AnnotationController *controller) { controller->endStroke(); }));
}

void undoStroke(obs_data_t *, obs_data_t *response, void *)
{
	obs_data_set_bool(response, "ok", post([](AnnotationController *controller) { controller->undo(); }));
}

void clearStrokes(obs_data_t *, obs_data_t *response, void *)
{
	obs_data_set_bool(response, "ok", post([](AnnotationController *controller) { controller->clear(); }));
}

void probeOverlay(obs_data_t *request, obs_data_t *response, void *)
{
	const std::vector<StrokePoint> points = pointsOf(request);
	obs_data_set_bool(response, "ok",
			  post([points](AnnotationController *controller) { controller->probeOverlay(points); }));
}

void annotationState(obs_data_t *, obs_data_t *response, void *)
{
	std::lock_guard<std::mutex> lock(gMutex);
	obs_data_set_string(response, "estado", gState.c_str());
	obs_data_set_bool(response, "ok", gTarget != nullptr);
}

} // namespace

void AnnotationTestVendor::start(AnnotationController *controller)
{
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gTarget = controller;
	}
	if (!gVendor) {
		gVendor = obs_websocket_register_vendor(live::kTestVendorShell);
	}
	if (!gVendor) {
		blog(LOG_WARNING, "[voltz] obs-websocket ausente: sem pedidos de teste '%s'", live::kTestVendorShell);
		return;
	}
	obs_websocket_vendor_register_request(gVendor, "InjectStroke", injectStroke, nullptr);
	obs_websocket_vendor_register_request(gVendor, "ExtendStroke", extendStroke, nullptr);
	obs_websocket_vendor_register_request(gVendor, "EndStroke", endStroke, nullptr);
	obs_websocket_vendor_register_request(gVendor, "UndoStroke", undoStroke, nullptr);
	obs_websocket_vendor_register_request(gVendor, "ClearStrokes", clearStrokes, nullptr);
	obs_websocket_vendor_register_request(gVendor, "ProbeOverlay", probeOverlay, nullptr);
	obs_websocket_vendor_register_request(gVendor, "AnnotationState", annotationState, nullptr);
	blog(LOG_WARNING, "[voltz] modo de teste (%s): pedidos '%s' ligados", live::kTestEnv, live::kTestVendorShell);
}

void AnnotationTestVendor::stop()
{
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gTarget = nullptr;
	}
	if (gVendor) {
		for (const char *request : kRequests) {
			obs_websocket_vendor_unregister_request(gVendor, request);
		}
	}
}

void AnnotationTestVendor::publish(const std::string &stateJson)
{
	std::lock_guard<std::mutex> lock(gMutex);
	gState = stateJson;
}

} // namespace voltz
```

- [ ] **Step 4: CMake e casca**

Em `frontend/cmake/feature-voltz.cmake`:
1. Trocar `target_link_libraries(obs-studio PRIVATE Voltz::core)` por
   `target_link_libraries(obs-studio PRIVATE Voltz::core OBS::websocket-api)` (só o cabeçalho `obs-websocket-api.h`).
2. Na lista de `target_sources(obs-studio PRIVATE ...)`, antes de `    voltz/annotate/OverlayWindow.cpp`, acrescentar:
```cmake
    voltz/annotate/AnnotationController.cpp
    voltz/annotate/AnnotationController.hpp
    voltz/annotate/AnnotationTestVendor.cpp
    voltz/annotate/AnnotationTestVendor.hpp
```
Em `frontend/voltz/shell/VoltzShell.hpp`: depois de `class FrontendBridge;` acrescentar `class AnnotationController;`;
depois de `	LiveMouse *liveMouse_ = nullptr;` acrescentar `	AnnotationController *annotations_ = nullptr;`.

Em `frontend/voltz/shell/VoltzShell.cpp`:
1. Depois de `#include <voltz/project/LiveMouse.hpp>`, acrescentar `#include <voltz/annotate/AnnotationController.hpp>`.
2. Trocar:
```cpp
	liveMouse_ = new LiveMouse(mouseStyles_, this);
```
por:
```cpp
	liveMouse_ = new LiveMouse(mouseStyles_, this);
	/* Anotações (4B): só exibe; os traços vêm do fornecedor de teste até o modo desenho do 4C. */
	annotations_ = new AnnotationController(this);
	recorder_->setAnnotationController(annotations_);
```
3. Trocar:
```cpp
		/* antes de o libobs descarregar o plugin: nada de chamar os procedimentos dele depois daqui */
		liveMouse_->shutdown();
```
por:
```cpp
		/* antes de o libobs descarregar o plugin: nada de chamar os procedimentos dele depois daqui; a sobreposição
		 * das anotações é destruída aqui */
		annotations_->shutdown();
		liveMouse_->shutdown();
```
(`ProjectRecorder::setAnnotationController` é implementado na Task 9; para compilar agora, acrescente só a declaração e
um corpo provisório.) Em `frontend/voltz/project/ProjectRecorder.hpp`, depois de `class MouseStyleStore;` acrescentar
`class AnnotationController;` e, depois da declaração de
`void setMouseStyleStore(MouseStyleStore *store);`, acrescentar:
```cpp
	// Anotações (Plano 4B): durante a gravação, os traços vão para anotacoes.jsonl (diário consolidado no fim) e o
	// "queimadas" vai para o manifesto. Chamar uma vez, antes de gravar.
	void setAnnotationController(AnnotationController *controller);
```
e, no fim de `frontend/voltz/project/ProjectRecorder.cpp` (antes de `} // namespace voltz`), um corpo provisório que a
Task 9 substitui:
```cpp
void ProjectRecorder::setAnnotationController(AnnotationController *controller)
{
	(void)controller;
}
```

- [ ] **Step 5: Formatar e compilar**

```bash
"$CLANGFMT" -i frontend/voltz/annotate/AnnotationController.hpp frontend/voltz/annotate/AnnotationController.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp frontend/voltz/shell/VoltzShell.hpp frontend/voltz/shell/VoltzShell.cpp frontend/voltz/project/ProjectRecorder.hpp frontend/voltz/project/ProjectRecorder.cpp
```
Run: CONFIGURAR; COMPILAR APP (segundo plano).
Expected: `exit=0`, nenhuma linha de erro. Se o moc reclamar do tipo dos sinais, confira que `stroke-store.hpp` está
incluído no `AnnotationController.hpp` (tipos completos) e que os sinais usam `voltz::` qualificado.

- [ ] **Step 6: Teste ao vivo — programa, sobreposição fora da área de trabalho, desfazer e limpar**

Bloco de teste com `$liveTest = $true`, `$standins = $true`, **`$overlayOffscreen = $true`**, `$burned = $false`,
`$layersV1 = $false`. Passos no `try`:
```powershell
[void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
[void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Só tela"}}]')
$st = Shell-State
"inicio: visiveis=$(@($st.visiveis).Count) criada=$($st.sobreposicao.criada) queimadas=$($st.queimadas) captura=$($st.captura -join 'x')"
[void](VendorShell "InjectStroke" (Stroke-Json "linha" "#D4F53C" 16 "[[640,720],[1920,720]]"))
$st = Shell-State; Assert-OverlaySafe; $feed = Feed-State
"um traco: casca=$(@($st.visiveis).Count) plugin=$($feed.tracos) mostradas=$($feed.mostradas)"
$img = Scene-Image "Só tela"; $on = $img.GetPixel(320, 180); $off = $img.GetPixel(320, 150); $img.Dispose()
"programa: linha=$on fora=$off"
$hwnd = [IntPtr][Int64]$st.sobreposicao.hwnd
$ex = [W]::GetWindowLongPtr($hwnd, -20).ToInt64(); [uint32]$aff = 0; [void][W]::GetWindowDisplayAffinity($hwnd, [ref]$aff)
"sobreposicao: visivel=$([W]::IsWindowVisible($hwnd)) estilo=0x$('{0:X}' -f $ex) afinidade=0x$('{0:X}' -f $aff) retangulo=$($st.sobreposicao.retangulo -join ',') primeiro_plano=$([W]::GetForegroundWindow() -eq $hwnd) vx+vw=$($vx + $vw)"
[void](VendorShell "ProbeOverlay" '{"pts":"[[320,180],[320,150]]"}')
$st = Shell-State; "buffer: $($st.amostras -join ' ')"
[void](VendorShell "InjectStroke" (Stroke-Json "marca_texto" "#FF4D4D" 8 "[[640,1080],[1280,1080],[1920,1080]]"))
[void](VendorShell "ProbeOverlay" '{"pts":"[[320,270]]"}')
$st = Shell-State; Assert-OverlaySafe
$img = Scene-Image "Só tela"; $hl = $img.GetPixel(320, 270); $img.Dispose()
"marca-texto: programa=$hl buffer=$($st.amostras[0])"
[void](VendorShell "UndoStroke")
$st = Shell-State; $feed = Feed-State
$img = Scene-Image "Só tela"; $hl2 = $img.GetPixel(320, 270); $img.Dispose()
"desfazer: casca=$(@($st.visiveis).Count) plugin=$($feed.tracos) pixel=$hl2"
[void](VendorShell "ClearStrokes")
$st = Shell-State; $feed = Feed-State
"limpar: casca=$(@($st.visiveis).Count) plugin=$($feed.tracos) visivel=$([W]::IsWindowVisible($hwnd))"
Close-App; Save-Log
"depois de fechar: janela existe=$([W]::IsWindow($hwnd))"
LogLines "[voltz] anotacoes"; LogLines "voltz-teste-casca"
```
Expected:
- `inicio: visiveis=0 criada=False queimadas=False captura=2560x1440`
- `um traco: casca=1 plugin=1 mostradas=1` (uma fonte compartilhada, contada uma vez)
- `programa: linha` ≈ (212,245,60), `fora` ≈ (42,42,48)
- `sobreposicao: visivel=True estilo=0x...` com os bits `0x080800A8` presentes, `afinidade=0x11`, retângulo começando em
  `vx+vw+200`, `primeiro_plano=False`
- `buffer: #FFD4F53C #00000000`
- `marca-texto: programa` ≈ (127,56,60), `buffer=#66661F1F` (alfa `66`)
- `desfazer: casca=1 plugin=1 pixel` ≈ (42,42,48)
- `limpar: casca=0 plugin=0 visivel=False`
- `depois de fechar: janela existe=False`; o `finally` imprime `ok: sobreposicao <hwnd> destruida`
- log: `[voltz] anotacoes: sobreposicao em (<vx+vw+200>,<vy> 640x360) para a captura 2560x1440`,
  `[voltz] anotacoes: sobreposicao criada (fora da captura)`, `[voltz] anotacoes: 1 traco(s) removido(s), 1 na tela`
  (desfazer), `[voltz] anotacoes: 1 traco(s) removido(s), 0 na tela` (limpar),
  `[voltz] anotacoes: desligadas (sobreposicao destruida)` e
  `[voltz] modo de teste (VOLTZ_TEST_LIVE): pedidos 'voltz-teste-casca' ligados`.

- [ ] **Step 7: Teste ao vivo — "queimadas" simulado**

Bloco de teste com `$liveTest = $true`, `$standins = $true`, `$overlayOffscreen = $true`, **`$burned = $true`**,
`$layersV1 = $false`. Passos no `try`:
```powershell
[void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
[void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Só tela"}}]')
[void](VendorShell "InjectStroke" (Stroke-Json "linha" "#D4F53C" 16 "[[640,720],[1920,720]]"))
$st = Shell-State; Assert-OverlaySafe; $feed = Feed-State
$img = Scene-Image "Só tela"; $on = $img.GetPixel(320, 180); $img.Dispose()
$hwnd = [IntPtr][Int64]$st.sobreposicao.hwnd; [uint32]$aff = 99; [void][W]::GetWindowDisplayAffinity($hwnd, [ref]$aff)
"queimadas: casca=$($st.queimadas) plugin=$($feed.queimadas) programa=$on afinidade=$aff visivel=$($st.sobreposicao.visivel)"
Close-App; Save-Log
LogLines "queimadas"
```
Expected: `queimadas: casca=True plugin=True programa` ≈ (42,42,48) (a camada não desenha) `afinidade=0 visivel=True`;
log com `[voltz] anotacoes: queimadas (teste)`, `[voltz-sources] anotacoes queimadas: a fonte nao desenha` e
`[voltz] anotacoes: sobreposicao criada (visivel na captura)`.

- [ ] **Step 8: Teste ao vivo — "somem após 1 s"**

Bloco de teste com os valores do Step 6 e
`$voltzIni = "NextProjectName=Teste 4B`r`n" + 'AnnotationStyle={"somem_apos_s":1}' + "`r`n"`. Passos no `try`:
```powershell
[void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
[void](VendorShell "InjectStroke" (Stroke-Json "seta" "#D4F53C" 8 "[[300,300],[900,600]]"))
$st1 = Shell-State; Assert-OverlaySafe
Start-Sleep -Seconds 2
$st2 = Shell-State; $feed = Feed-State
"somem: antes=$(@($st1.visiveis).Count) depois=$(@($st2.visiveis).Count) plugin=$($feed.tracos) visivel=$($st2.sobreposicao.visivel)"
Close-App; Save-Log
LogLines "[voltz] anotacoes"
```
Expected: `somem: antes=1 depois=0 plugin=0 visivel=False`; log com `[voltz] anotacoes: somem apos 1 s` e
`[voltz] anotacoes: 1 traco(s) removido(s), 0 na tela`.

- [ ] **Step 9: Regressão rápida**

Run: SMOKE e SMOKE MOUSE (um de cada vez).
Expected: os dois terminam OK (sem `VOLTZ_TEST_LIVE`, o `AnnotationController` não cria nada nem registra pedidos).

- [ ] **Step 10: Commit**

```bash
git add frontend/voltz/annotate/AnnotationController.hpp frontend/voltz/annotate/AnnotationController.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp frontend/cmake/feature-voltz.cmake frontend/voltz/shell/VoltzShell.hpp frontend/voltz/shell/VoltzShell.cpp frontend/voltz/project/ProjectRecorder.hpp frontend/voltz/project/ProjectRecorder.cpp
git commit -m "feat(voltz): AnnotationController (plugin, sobreposicao fora da captura, queimadas, somem apos) e pedidos de teste da casca

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: `ProjectRecorder` — `anotacoes.jsonl` e manifesto; recuperação consolida o diário

**Files:**
- Modify: `frontend/voltz/project/ProjectRecorder.hpp`, `frontend/voltz/project/ProjectRecorder.cpp`
- Modify: `frontend/voltz/project/ProjectLibrary.cpp`

- [ ] **Step 1: Cabeçalho**

Em `frontend/voltz/project/ProjectRecorder.hpp`:
1. Depois de `#include <voltz/core/iso-encoders.hpp>`, acrescentar `#include <voltz/core/annotation-journal.hpp>` e
   `#include <voltz/core/stroke-store.hpp>`; depois de `#include <QDir>`, acrescentar `#include <QFile>`.
2. No comentário da classe, trocar `marcadores.json, cursor.jsonl e, por fonte de papel` por
   `marcadores.json, cursor.jsonl, anotacoes.jsonl e, por fonte de papel`.
3. Depois de `	void onSpotlightToggled(bool active);`, acrescentar:
```cpp
	void startAnnotations();
	// Fecha o diário (traços em andamento terminam em `endNs`) e o consolida no formato da spec.
	void stopAnnotations(int64_t endNs);
	void appendAnnotationLines(const std::vector<std::string> &lines);
```
4. Depois de `	MouseStyleStore *mouseStyles_ = nullptr;`, acrescentar:
```cpp
	AnnotationController *annotations_ = nullptr;
	std::unique_ptr<AnnotationJournal> journal_;
	std::unique_ptr<QFile> annotationsFile_;
```

- [ ] **Step 2: Gravação do diário**

Em `frontend/voltz/project/ProjectRecorder.cpp`:
1. Depois de `#include "MonitorLocator.hpp"`, acrescentar `#include <voltz/annotate/AnnotationController.hpp>`.
2. No destrutor, trocar:
```cpp
	stopCursor();
	finishIsoTracks();
	if (registered_) {
```
por:
```cpp
	stopCursor();
	stopAnnotations(nowNs());
	finishIsoTracks();
	if (registered_) {
```
3. Em `onStarting`, trocar:
```cpp
		manifest_.hasMouse = true;
		manifest_.mouse = mouseStyles_->style();
	}
```
por:
```cpp
		manifest_.hasMouse = true;
		manifest_.mouse = mouseStyles_->style();
	}
	/* spec §6.3: com a sobreposição na captura, os traços já estão em tela.mkv e no programa */
	manifest_.annotationsBurned = annotations_ && annotations_->burned();
```
4. Em `onStarted`, trocar `	startCursor();\n	startIsoTracks();` por:
```cpp
	startCursor();
	startAnnotations();
	startIsoTracks();
```
5. Em `finish`, trocar:
```cpp
	heartbeat_.stop();
	stopCursor();
	finishIsoTracks();
```
por:
```cpp
	heartbeat_.stop();
	stopCursor();
	stopAnnotations(end);
	finishIsoTracks();
```
6. Trocar o corpo provisório da Task 8:
```cpp
void ProjectRecorder::setAnnotationController(AnnotationController *controller)
{
	(void)controller;
}
```
por:
```cpp
void ProjectRecorder::setAnnotationController(AnnotationController *controller)
{
	annotations_ = controller;
	if (!controller) {
		return;
	}
	connect(controller, &AnnotationController::strokeEnded, this, [this](const LiveStroke &stroke) {
		if (journal_) {
			appendAnnotationLines({journal_->strokeFinished(stroke)});
		}
	});
	connect(controller, &AnnotationController::strokesRemoved, this,
		[this](const std::vector<RemovedStroke> &removed) {
			if (!journal_) {
				return;
			}
			std::vector<std::string> lines;
			lines.reserve(removed.size());
			for (const RemovedStroke &stroke : removed) {
				lines.push_back(journal_->strokeRemoved(stroke));
			}
			appendAnnotationLines(lines);
		});
	connect(controller, &AnnotationController::burnedChanged, this, [this](bool burned) {
		if (burned && active_ && !manifest_.annotationsBurned) {
			manifest_.annotationsBurned = true;
			writeManifest();
		}
	});
}

void ProjectRecorder::startAnnotations()
{
	manifest_.annotations = AnnotationsFile{};
	if (!annotations_) {
		return;
	}
	/* coordenadas = pixels da captura da Tela compartilhada (as de tela.mkv e da Voltz Anotações) */
	OBSSource screen = findSharedSource(SharedSource::Screen);
	const int width = screen ? static_cast<int>(obs_source_get_width(screen)) : 0;
	const int height = screen ? static_cast<int>(obs_source_get_height(screen)) : 0;
	if (width <= 0 || height <= 0) {
		blog(LOG_INFO, "[voltz] anotacoes: sem Tela compartilhada; sem anotacoes.jsonl");
		return;
	}
	auto file = std::make_unique<QFile>(QDir(projectDir_).filePath(QString::fromUtf8(kAnnotationsFileName)));
	if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		blog(LOG_WARNING, "[voltz] anotacoes: nao foi possivel criar anotacoes.jsonl");
		return;
	}
	annotationsFile_ = std::move(file);
	journal_ = std::make_unique<AnnotationJournal>(timeline_.t0Ns());
	manifest_.annotations.present = true;
	manifest_.annotations.file = kAnnotationsFileName;
	manifest_.annotations.width = width;
	manifest_.annotations.height = height;
	if (strcmp(obs_source_get_unversioned_id(screen), "monitor_capture") == 0) {
		OBSDataAutoRelease settings = obs_source_get_settings(screen);
		const std::string monitorId = obs_data_get_string(settings, "monitor_id");
		const MonitorDescription monitor = findMonitor(monitorId);
		if (monitor.found) {
			manifest_.annotations.hasOrigin = true;
			manifest_.annotations.origin.type = "monitor";
			manifest_.annotations.origin.id = monitorId;
			manifest_.annotations.origin.rect[0] = monitor.rect.left;
			manifest_.annotations.origin.rect[1] = monitor.rect.top;
			manifest_.annotations.origin.rect[2] = monitor.rect.width;
			manifest_.annotations.origin.rect[3] = monitor.rect.height;
			manifest_.annotations.origin.scale = monitor.scale;
		}
	}
	const std::vector<std::string> initial = journal_->initialLines(annotations_->visibleStrokes());
	appendAnnotationLines(initial);
	blog(LOG_INFO, "[voltz] anotacoes: %dx%d, %d traco(s) ja na tela", width, height,
	     static_cast<int>(initial.size()));
}

void ProjectRecorder::appendAnnotationLines(const std::vector<std::string> &lines)
{
	if (!annotationsFile_) {
		return;
	}
	bool wrote = false;
	for (const std::string &line : lines) {
		if (line.empty()) {
			continue;
		}
		annotationsFile_->write(line.data(), static_cast<qint64>(line.size()));
		annotationsFile_->write("\n", 1);
		wrote = true;
	}
	if (wrote) {
		annotationsFile_->flush(); /* sobrevive a uma queda */
	}
}

void ProjectRecorder::stopAnnotations(int64_t endNs)
{
	if (!annotationsFile_) {
		journal_.reset();
		return;
	}
	if (journal_ && annotations_) {
		appendAnnotationLines(journal_->finishOpen(annotations_->visibleStrokes(), endNs));
	}
	const size_t strokes = journal_ ? journal_->strokeCount() : 0;
	const std::string path = nativeUtf8Path(annotationsFile_->fileName());
	annotationsFile_->close();
	annotationsFile_.reset();
	journal_.reset();
	/* o diário vira o formato da spec: uma linha por traço, com t_limpo */
	std::string text;
	std::string error;
	if (readFileText(path, text) && !writeFileAtomically(path, consolidateAnnotations(text), &error)) {
		blog(LOG_WARNING, "[voltz] anotacoes: nao foi possivel consolidar: %s", error.c_str());
	}
	blog(LOG_INFO, "[voltz] anotacoes: %zu traco(s) em anotacoes.jsonl", strokes);
}
```

- [ ] **Step 3: Recuperação consolida o diário**

Em `frontend/voltz/project/ProjectLibrary.cpp`:
1. Depois de `#include <voltz/core/atomic-file.hpp>`, acrescentar `#include <voltz/core/annotation-journal.hpp>`.
2. Em `ProjectLibrary::recover`, trocar:
```cpp
	recoverManifest(manifest, fileElapsedMs);
```
por:
```cpp
	recoverManifest(manifest, fileElapsedMs);
	/* anotacoes.jsonl que ficou só como diário (linhas curtas de t_limpo): junta no formato da spec */
	if (manifest.annotations.present && !manifest.annotations.file.empty()) {
		const std::string annotationsPath =
			nativeUtf8Path(dir.filePath(QString::fromStdString(manifest.annotations.file)));
		std::string journal;
		if (readFileText(annotationsPath, journal)) {
			writeFileAtomically(annotationsPath, consolidateAnnotations(journal));
		}
	}
```

- [ ] **Step 4: Formatar e compilar**

```bash
"$CLANGFMT" -i frontend/voltz/project/ProjectRecorder.hpp frontend/voltz/project/ProjectRecorder.cpp frontend/voltz/project/ProjectLibrary.cpp
```
Run: COMPILAR APP (segundo plano).
Expected: `exit=0`, nenhuma linha de erro.

- [ ] **Step 5: Teste ao vivo — gravação com traços**

Bloco de teste com `$liveTest = $true`, `$standins = $true`, `$overlayOffscreen = $true`, `$burned = $false`,
`$layersV1 = $false`. Passos no `try`:
```powershell
[void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
[void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Só tela"}}]')
# (Preparo de gravação, da seção Convenções)
$r = Ws ('[{"requestType":"SetProfileParameter","requestData":{"parameterCategory":"SimpleOutput","parameterName":"FilePath","parameterValue":' + (ConvertTo-Json $out) + '}},{"requestType":"GetSpecialInputs"}]')
$special = $r[1].data
$mutes = @(); foreach ($n in @($special.desktop1, $special.desktop2, $special.mic1, $special.mic2, $special.mic3, $special.mic4)) { if ($n) { $mutes += @{ requestType = "SetInputMute"; requestData = @{ inputName = $n; inputMuted = $true } } } }
if ($mutes.Count -gt 0) { [void](Ws (ConvertTo-Json -InputObject $mutes -Depth 5 -Compress)) }
# um retangulo ja na tela antes de gravar
[void](VendorShell "InjectStroke" (Stroke-Json "retangulo" "#4DA3FF" 6 "[[100,100],[600,400]]"))
Start-Sleep -Milliseconds 500; Assert-OverlaySafe; Assert-NoRealCapture
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 2
[void](VendorShell "InjectStroke" (Stroke-Json "seta" "#D4F53C" 8 "[[300,1200],[900,900]]")); Start-Sleep -Milliseconds 500
[void](VendorShell "InjectStroke" (Stroke-Json "caneta" "#FF4D4D" 6 "[[1000,1000],[1010,1004]]" -Open))
[void](VendorShell "ExtendStroke" '{"pts":"[[1020,1010],[1030,1020]]"}')
[void](VendorShell "EndStroke"); Start-Sleep -Milliseconds 500
[void](VendorShell "UndoStroke"); Start-Sleep -Milliseconds 500
[void](VendorShell "ClearStrokes"); Start-Sleep -Milliseconds 500
[void](VendorShell "InjectStroke" (Stroke-Json "elipse" "#D4F53C" 4 "[[1500,300],[2100,700]]")); Start-Sleep 1
Assert-OverlaySafe
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 5
Close-App; Save-Log
$d = (Project-Dirs)[-1]
$m = Read-Json (Join-Path $d.FullName "projeto.voltz")
"manifesto: status=$($m.status) anotacoes=$($m.arquivos.anotacoes | ConvertTo-Json -Compress) queimadas=$($m.anotacoes_queimadas) tela=$($m.arquivos.tela.largura)x$($m.arquivos.tela.altura)"
Get-Content -LiteralPath (Join-Path $d.FullName "anotacoes.jsonl") -Encoding UTF8
LogLines "[voltz] anotacoes:"
```
Expected:
- `manifesto: status=ok anotacoes={"arquivo":"anotacoes.jsonl","largura":2560,"altura":1440} queimadas= tela=2560x1440`
  (sem `origem`: a Tela substituta é cor; sem `anotacoes_queimadas`)
- `anotacoes.jsonl` com **4 linhas completas**, nesta ordem, nenhuma linha curta:
  `retangulo` (`"t_ini":0,"t_fim":0`, com `t_limpo`), `seta` (com `t_limpo` igual ao do retângulo — o mesmo "limpar"),
  `caneta` (4 pontos `[[1000,1000],[1010,1004],[1020,1010],[1030,1020]]`, `t_limpo` ≤ o da seta — o "desfazer"),
  `elipse` (`"pts":[[1500,300],[2100,700]]`, **sem** `t_limpo`); em todas `t_ini ≤ t_fim ≤ t_limpo`.
- log: `[voltz] anotacoes: 2560x1440, 1 traco(s) ja na tela` e `[voltz] anotacoes: 4 traco(s) em anotacoes.jsonl`.

- [ ] **Step 6: Regressão**

Run: RODAR TESTES (153), SMOKE PROJETO e SMOKE MOUSE (um de cada vez).
Expected: tudo OK (o smoke de projeto não tem Tela compartilhada → log `sem anotacoes.jsonl`, nada muda nele).

- [ ] **Step 7: Commit**

```bash
git add frontend/voltz/project/ProjectRecorder.hpp frontend/voltz/project/ProjectRecorder.cpp frontend/voltz/project/ProjectLibrary.cpp
git commit -m "feat(voltz): anotacoes.jsonl no projeto (diario consolidado no fim e na recuperacao) e anotacoes_queimadas no manifesto

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 10: Smoke das anotações e demonstração manual

**Files:**
- Create: `tools/voltz-smoke-annotations.ps1`, `tools/voltz-annot-demo.ps1`

- [ ] **Step 1: Smoke**

Criar `tools/voltz-smoke-annotations.ps1` (ASCII puro):
```powershell
<#
  Teste de integracao do Plano 4B (anotacoes: fonte Voltz Anotacoes, sobreposicao, anotacoes.jsonl).
  Seguranca (o PC e do usuario e esta em uso):
  - VOLTZ_TEST_LIVE=1: tracos SO pelos pedidos "voltz-teste-casca" (InjectStroke etc.), nunca pelo mouse/teclado; no
    Plano 4B a sobreposicao nao tem modo que capture entrada.
  - VOLTZ_TEST_PRESET_STANDINS=1: Tela 2560x1440 e Camera de COR (nenhuma captura de tela nem webcam);
    Assert-NoRealCapture aborta se houver monitor/janela/jogo/dshow.
  - VOLTZ_TEST_OVERLAY_RECT: a sobreposicao (transparente a cliques) fica num retangulo A DIREITA de todo o desktop
    virtual - o usuario nao a ve; o desenho dela e conferido no buffer (ProbeOverlay), nunca por captura de tela.
    Depois de cada traco, Assert-OverlaySafe le o estilo da janela (WS_EX_TRANSPARENT|LAYERED|NOACTIVATE|TOOLWINDOW|
    TOPMOST) e se ela esta em primeiro plano; se algo estiver errado, o app (PID lancado) e encerrado na hora.
  - Render do programa so por GetSourceScreenshot de cena sintetica; so metadados (nenhum quadro de video); config
    portatil guardada e restaurada; -OutDir apagado no fim; Videos conferida intacta; no fim, nenhuma sobreposicao orfa.
  Fases: A (preset, 3 camadas, programa + sobreposicao, desfazer/limpar, gravacao com anotacoes.jsonl),
  B (grupo Tela Voltz versao 1 -> 2; sem retangulo de teste nao ha sobreposicao), C (queimadas simulado),
  D (somem apos 1 s, gravando).
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-annotations.ps1 [-OutDir <pasta>] [-Keep]
#>
param(
    [string]$Config = "RelWithDebInfo",
    [string]$OutDir = (Join-Path $env:TEMP "voltz-smoke-annotations"),
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

# Acentos montados por codigo: este arquivo e ASCII (o PowerShell 5.1 le .ps1 sem BOM como ANSI)
$o = [string][char]0xF3; $a = [string][char]0xE2; $dash = [string][char]0x2014
$presetScreen = "S${o} tela / S${o} c${a}mera $dash Aplicar"
$sceneScreen = "S${o} tela"

$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.smokeannot-bak"
$voltzCfg = Join-Path $cfg "Voltz Studio"
$logDir = Join-Path $env:TEMP "voltz-smoke-annotations-logs"
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"
$hadVoltzVideos = Test-Path -LiteralPath $voltzVideos
$videosDir = [Environment]::GetFolderPath('MyVideos')
$videosBefore = @(Get-ChildItem -Force -LiteralPath $videosDir | ForEach-Object { "$($_.Name)|$($_.LastWriteTimeUtc.Ticks)|$($_.Attributes)" })

if ((Test-Path -LiteralPath $OutDir) -and @(Get-ChildItem -Force -LiteralPath $OutDir).Count -gt 0) {
    Write-Host "FALHOU -OutDir ja existe e nao esta vazio: $OutDir"; exit 1
}
if (Test-Path $bak) { Write-Host "FALHOU backup anterior em $bak (restaure ou apague antes de rodar)"; exit 1 }

Add-Type -AssemblyName System.Drawing
# P/Invoke SO de leitura (nenhuma funcao que mova o mouse, clique, digite ou mude janelas)
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class VoltzOverlayProbe {
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")] public static extern IntPtr GetWindowLongPtr(IntPtr h, int index);
 [DllImport("user32.dll")] public static extern bool GetWindowDisplayAffinity(IntPtr h, out uint affinity);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index); }
'@
$P = [VoltzOverlayProbe]
[void]$P::SetProcessDPIAware()
# Retangulo de teste: 640x360 A DIREITA de todo o desktop virtual (ninguem ve, nada fica por cima de janela alguma)
$vx = $P::GetSystemMetrics(76); $vy = $P::GetSystemMetrics(77); $vw = $P::GetSystemMetrics(78)
$overlayRect = "{0},{1},640,360" -f ($vx + $vw + 200), $vy
$script:overlays = @()

$checks = [ordered]@{}
function Check([string]$name, [bool]$ok) { $script:checks[$name] = $ok }

function Ws([string]$json) {
    $file = Join-Path $env:TEMP "voltz-smoke-annotations-req.json"
    [IO.File]::WriteAllText($file, $json, (New-Object Text.UTF8Encoding $false))
    $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $file
    Remove-Item -Force $file -ErrorAction SilentlyContinue
    # a virgula preserva o array mesmo com 1 elemento
    return ,@($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json })
}

function Uia([string]$name) {
    $result = & powershell -ExecutionPolicy Bypass -File $uia -ProcessId $script:proc.Id -Name $name
    Write-Host $result
    return ($LASTEXITCODE -eq 0)
}

function Vendor([string]$type, [string]$dataJson = '{}', [string]$vendorName = 'voltz-teste') {
    $r = Ws ('[{"requestType":"CallVendorRequest","requestData":{"vendorName":"' + $vendorName + '","requestType":"' + $type + '","requestData":' + $dataJson + '}}]')
    if ($r.Count -lt 1 -or -not $r[0].ok) { throw "pedido de teste $vendorName/$type falhou" }
    return $r[0].data.responseData
}

function VendorShell([string]$type, [string]$dataJson = '{}') { return (Vendor $type $dataJson 'voltz-teste-casca') }

# Os pedidos da casca sao enfileirados para a thread da UI: espera um pouco antes de ler o estado
function Shell-State { Start-Sleep -Milliseconds 400; return ((VendorShell "AnnotationState").estado | ConvertFrom-Json) }

function Feed-State { return ((Vendor "AnnotationFeedState").estado | ConvertFrom-Json) }

function Stroke-Json([string]$tool, [string]$color, [int]$width, [string]$points, [switch]$Open) {
    $req = [ordered]@{ ferr = $tool; cor = $color; esp = $width; pts = $points }
    if ($Open) { $req.aberto = $true }
    return (ConvertTo-Json $req -Compress)
}

# Janelas de sobreposicao do PID lancado
function Overlay-Windows([int]$procId) {
    $list = @(); $h = [IntPtr]::Zero
    while ($true) {
        $h = $P::FindWindowEx([IntPtr]::Zero, $h, "VoltzAnnotationOverlay", $null)
        if ($h -eq [IntPtr]::Zero) { break }
        [uint32]$owner = 0; [void]$P::GetWindowThreadProcessId($h, [ref]$owner)
        if ($owner -eq $procId) { $list += $h }
    }
    return ,$list
}

# Toda sobreposicao precisa ser transparente a cliques e nunca estar em primeiro plano; se nao, encerra o app (PID)
function Assert-OverlaySafe {
    foreach ($h in (Overlay-Windows $script:proc.Id)) {
        if ($script:overlays -notcontains $h.ToInt64()) { $script:overlays += $h.ToInt64() }
        $ex = $P::GetWindowLongPtr($h, -20).ToInt64()
        if ((($ex -band 0x080800A8) -ne 0x080800A8) -or ($P::GetForegroundWindow() -eq $h)) {
            Stop-Process -Id $script:proc.Id -Force
            throw ("sobreposicao insegura (estilo 0x{0:X}) - app encerrado" -f $ex)
        }
    }
}

# Seguranca: nenhuma captura real (tela, janela, jogo, webcam) pode existir no app
function Assert-NoRealCapture {
    $inputs = @((Ws '[{"requestType":"GetInputList"}]')[0].data.inputs)
    $real = @($inputs | Where-Object { $_.inputKind -match 'monitor_capture|window_capture|game_capture|dshow_input' })
    if ($real.Count -gt 0) { throw ("captura real presente - abortando: " + (($real | ForEach-Object { $_.inputName }) -join ', ')) }
}

# Render de cena SINTETICA em 640x360 - nunca a tela real nem quadro de video
function Scene-Image([string]$scene) {
    Assert-NoRealCapture
    $r = Ws ('[{"requestType":"GetSourceScreenshot","requestData":{"sourceName":' + (ConvertTo-Json $scene) + ',"imageFormat":"png","imageWidth":640,"imageHeight":360}}]')
    if ($r.Count -lt 1 -or -not $r[0].ok) { throw "GetSourceScreenshot falhou para '$scene'" }
    $bytes = [Convert]::FromBase64String(($r[0].data.imageData -replace '^data:image/png;base64,', ''))
    return [System.Drawing.Bitmap]::FromStream((New-Object IO.MemoryStream (, $bytes)))
}

function Near($c, [int]$r, [int]$g, [int]$b, [int]$tol = 24) {
    return ([Math]::Abs($c.R - $r) -le $tol) -and ([Math]::Abs($c.G - $g) -le $tol) -and ([Math]::Abs($c.B - $b) -le $tol)
}

# Camadas do (primeiro) grupo de uma cena
function Screen-Layers([string]$scene) {
    $top = @((Ws ('[{"requestType":"GetSceneItemList","requestData":{"sceneName":' + (ConvertTo-Json $scene) + '}}]'))[0].data.sceneItems)
    $group = @($top | Where-Object { $_.isGroup } | Select-Object -First 1)
    if ($group.Count -lt 1) { return ,@() }
    return ,@((Ws ('[{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":' + (ConvertTo-Json ([string]$group[0].sourceName)) + '}}]'))[0].data.sceneItems)
}

function Read-JsonFile([string]$path) {
    return (Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json) | ForEach-Object { $_ }
}

function Read-Jsonl([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return ,@() }
    return ,@(Get-Content -LiteralPath $path -Encoding UTF8 | Where-Object { $_ } | ForEach-Object { $_ | ConvertFrom-Json })
}

function New-CleanConfig([string]$voltzLines) {
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
    New-Item -ItemType Directory -Force $wsDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}')
    [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[Voltz]`r`n" + $voltzLines, (New-Object Text.UTF8Encoding $false))
}

# Modos de teste: VOLTZ_TEST_LIVE sempre; o resto conforme a fase
function Set-TestEnv([bool]$standins, [bool]$overlay, [bool]$burned, [bool]$layersV1) {
    $env:VOLTZ_TEST_LIVE = "1"
    if ($standins) { $env:VOLTZ_TEST_PRESET_STANDINS = "1" } else { Remove-Item Env:VOLTZ_TEST_PRESET_STANDINS -ErrorAction SilentlyContinue }
    if ($overlay) { $env:VOLTZ_TEST_OVERLAY_RECT = $overlayRect } else { Remove-Item Env:VOLTZ_TEST_OVERLAY_RECT -ErrorAction SilentlyContinue }
    if ($burned) { $env:VOLTZ_TEST_ANNOT_BURNED = "1" } else { Remove-Item Env:VOLTZ_TEST_ANNOT_BURNED -ErrorAction SilentlyContinue }
    if ($layersV1) { $env:VOLTZ_TEST_SCREEN_LAYERS = "1" } else { Remove-Item Env:VOLTZ_TEST_SCREEN_LAYERS -ErrorAction SilentlyContinue }
}

function Start-App {
    $script:proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
    Start-Sleep -Seconds 15
    return (-not $script:proc.HasExited)
}

# Fecha o app lancado aqui (so pelo PID) e guarda o log desta execucao como <tag>.txt
function Close-App([string]$tag) {
    $clean = $false
    if ($script:proc -and -not $script:proc.HasExited) {
        $script:proc.Refresh()
        [void]$script:proc.CloseMainWindow()
        $clean = $script:proc.WaitForExit(30000)
        if (-not $clean) { Stop-Process -Id $script:proc.Id -Force }
    }
    $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
    if ($log) { Copy-Item $log.FullName (Join-Path $logDir "$tag.txt") -Force }
    return $clean
}

function LogHas([string]$tag, [string]$text) {
    $file = Join-Path $logDir "$tag.txt"
    return [bool]((Test-Path $file) -and (Select-String -LiteralPath $file -SimpleMatch $text -Encoding UTF8 -Quiet))
}

# Gravacoes em -OutDir e audio global mutado
function Prepare-Recording {
    $r = Ws ('[{"requestType":"SetProfileParameter","requestData":{"parameterCategory":"SimpleOutput","parameterName":"FilePath","parameterValue":' + (ConvertTo-Json $OutDir) + '}},{"requestType":"GetSpecialInputs"}]')
    $special = $r[1].data
    $mutes = @()
    foreach ($n in @($special.desktop1, $special.desktop2, $special.mic1, $special.mic2, $special.mic3, $special.mic4)) {
        if ($n) { $mutes += @{ requestType = "SetInputMute"; requestData = @{ inputName = $n; inputMuted = $true } } }
    }
    if ($mutes.Count -gt 0) { [void](Ws (ConvertTo-Json -InputObject $mutes -Depth 5 -Compress)) }
}

function Project-Dir([string]$like) {
    return ,@(Get-ChildItem -LiteralPath $OutDir -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like $like -and (Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz")) })
}

function Program-Scene { [void](Ws ('[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":' + (ConvertTo-Json $sceneScreen) + '}}]')) }

if (Test-Path $cfg) { Move-Item $cfg $bak }
if (Test-Path $logDir) { Remove-Item -Recurse -Force $logDir }
New-Item -ItemType Directory -Force $logDir | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null
$proc = $null
try {
    # ---------------- Fase A: camadas, programa, sobreposicao, desfazer/limpar, gravacao ----------------
    New-CleanConfig ("NextProjectName=Smoke anotacoes A`r`n")
    Set-TestEnv $true $true $false $false
    Check "A: app aberto" (Start-App)
    Check "A: preset 'So tela / So camera' (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    $inner = Screen-Layers $sceneScreen
    Check "A: grupo = Tela + Voltz Anotacoes + Voltz Mouse, travadas em (0,0)" ($inner.Count -eq 3 -and $inner[0].sourceName -eq "Tela" -and $inner[1].inputKind -eq "voltz_annotations_source" -and $inner[2].inputKind -eq "voltz_mouse_source" -and $inner[1].sceneItemLocked -and $inner[1].sceneItemTransform.positionX -eq 0 -and $inner[1].sceneItemTransform.positionY -eq 0)
    Program-Scene
    $st = Shell-State
    Check "A: sem tracos nem sobreposicao no inicio (captura $($st.captura -join 'x'))" (@($st.visiveis).Count -eq 0 -and -not $st.sobreposicao.criada -and -not $st.queimadas -and $st.captura[0] -eq 2560 -and $st.captura[1] -eq 1440)
    # linha lima esp 16 de (640,720) a (1920,720): na imagem 640x360 e na sobreposicao 640x360, y=180
    [void](VendorShell "InjectStroke" (Stroke-Json "linha" "#D4F53C" 16 "[[640,720],[1920,720]]"))
    $st = Shell-State; Assert-OverlaySafe; $feed = Feed-State
    Check "A: um traco na casca e no plugin, camada mostrada" (@($st.visiveis).Count -eq 1 -and $feed.tracos -eq 1 -and $feed.mostradas -ge 1)
    $img = Scene-Image $sceneScreen; $on = $img.GetPixel(320, 180); $off = $img.GetPixel(320, 150); $img.Dispose()
    Check "A: programa com a linha lima ($on) e a Tela fora dela ($off)" ((Near $on 212 245 60) -and (Near $off 42 42 48))
    $hwnd = [IntPtr][Int64]$st.sobreposicao.hwnd
    $ex = $P::GetWindowLongPtr($hwnd, -20).ToInt64(); [uint32]$aff = 0; [void]$P::GetWindowDisplayAffinity($hwnd, [ref]$aff)
    Check ("A: sobreposicao visivel, transparente a cliques, fora da captura (estilo 0x{0:X}, afinidade 0x{1:X})" -f $ex, $aff) ($P::IsWindowVisible($hwnd) -and (($ex -band 0x080800A8) -eq 0x080800A8) -and $aff -eq 0x11 -and $st.sobreposicao.afinidade -eq 17 -and ($P::GetForegroundWindow() -ne $hwnd))
    Check "A: sobreposicao de teste fora da area de trabalho ($($st.sobreposicao.retangulo -join ','))" ($st.sobreposicao.retangulo[0] -ge ($vx + $vw) -and $st.sobreposicao.retangulo[2] -eq 640)
    [void](VendorShell "ProbeOverlay" '{"pts":"[[320,180],[320,150]]"}')
    $st = Shell-State
    Check "A: buffer da sobreposicao: lima opaco na linha, transparente fora ($($st.amostras -join ' '))" ($st.amostras[0] -eq "#FFD4F53C" -and $st.amostras[1] -eq "#00000000")
    # marca-texto vermelho esp 8 (24 px a 40%) com juncao em (1280,1080) -> (320,270)
    [void](VendorShell "InjectStroke" (Stroke-Json "marca_texto" "#FF4D4D" 8 "[[640,1080],[1280,1080],[1920,1080]]"))
    [void](VendorShell "ProbeOverlay" '{"pts":"[[320,270]]"}')
    $st = Shell-State; Assert-OverlaySafe
    $img = Scene-Image $sceneScreen; $hl = $img.GetPixel(320, 270); $img.Dispose()
    Check "A: marca-texto a 40% sem escurecer na juncao: programa ($hl), sobreposicao ($($st.amostras[0]))" ((Near $hl 127 56 60) -and ([string]$st.amostras[0]).StartsWith("#66"))
    [void](VendorShell "UndoStroke")
    $st = Shell-State; $feed = Feed-State
    $img = Scene-Image $sceneScreen; $hl2 = $img.GetPixel(320, 270); $img.Dispose()
    Check "A: desfazer tira o marca-texto ($hl2)" (@($st.visiveis).Count -eq 1 -and $feed.tracos -eq 1 -and (Near $hl2 42 42 48))
    [void](VendorShell "ClearStrokes")
    $st = Shell-State; $feed = Feed-State
    Check "A: limpar esvazia e esconde a sobreposicao" (@($st.visiveis).Count -eq 0 -and $feed.tracos -eq 0 -and -not $P::IsWindowVisible($hwnd) -and -not $st.sobreposicao.visivel)

    # Gravacao: retangulo antes de gravar; seta; caneta aberta+estendida+terminada; desfazer (caneta); limpar
    # (retangulo + seta); elipse ate o fim
    Prepare-Recording
    [void](VendorShell "InjectStroke" (Stroke-Json "retangulo" "#4DA3FF" 6 "[[100,100],[600,400]]"))
    Start-Sleep -Milliseconds 500; Assert-OverlaySafe; Assert-NoRealCapture
    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "A: StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 2
    [void](VendorShell "InjectStroke" (Stroke-Json "seta" "#D4F53C" 8 "[[300,1200],[900,900]]"))
    Start-Sleep -Milliseconds 500
    [void](VendorShell "InjectStroke" (Stroke-Json "caneta" "#FF4D4D" 6 "[[1000,1000],[1010,1004]]" -Open))
    [void](VendorShell "ExtendStroke" '{"pts":"[[1020,1010],[1030,1020]]"}')
    [void](VendorShell "EndStroke")
    Start-Sleep -Milliseconds 500
    [void](VendorShell "UndoStroke")
    Start-Sleep -Milliseconds 500
    [void](VendorShell "ClearStrokes")
    Start-Sleep -Milliseconds 500
    [void](VendorShell "InjectStroke" (Stroke-Json "elipse" "#D4F53C" 4 "[[1500,300],[2100,700]]"))
    Start-Sleep -Seconds 1
    Assert-OverlaySafe
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "A: StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 5
    Check "A: encerrou limpo" (Close-App "A")
    $dirs = Project-Dir "*Smoke anotacoes A*"
    Check "A: uma pasta de projeto" ($dirs.Count -eq 1)
    if ($dirs.Count -eq 1) {
        $dir = $dirs[0].FullName
        $m = Read-JsonFile (Join-Path $dir "projeto.voltz")
        Check "A: manifesto ok com arquivos.anotacoes 2560x1440 e sem queimadas" ($m.status -eq "ok" -and $m.arquivos.anotacoes.arquivo -eq "anotacoes.jsonl" -and $m.arquivos.anotacoes.largura -eq 2560 -and $m.arquivos.anotacoes.altura -eq 1440 -and $null -eq $m.anotacoes_queimadas)
        Check "A: tela.mkv continua sendo a captura (2560x1440)" ($m.arquivos.tela -and $m.arquivos.tela.largura -eq 2560 -and $m.arquivos.tela.altura -eq 1440)
        $lines = Read-Jsonl (Join-Path $dir "anotacoes.jsonl")
        $tools = @($lines | ForEach-Object { $_.ferr }) -join ','
        Check "A: anotacoes.jsonl consolidado na ordem retangulo,seta,caneta,elipse ($tools)" ($lines.Count -eq 4 -and $tools -eq "retangulo,seta,caneta,elipse")
        if ($lines.Count -eq 4) {
            $ret = $lines[0]; $seta = $lines[1]; $caneta = $lines[2]; $elipse = $lines[3]
            Check "A: retangulo de antes da gravacao em 0 e limpo junto com a seta" ($ret.t_ini -eq 0 -and $ret.t_fim -eq 0 -and $ret.t_limpo -gt 0 -and $ret.t_limpo -eq $seta.t_limpo)
            Check "A: seta em pixels da captura" ($seta.pts[0][0] -eq 300 -and $seta.pts[0][1] -eq 1200 -and $seta.pts[1][0] -eq 900 -and $seta.esp -eq 8 -and $seta.cor -eq "#D4F53C" -and $seta.t_ini -gt 0 -and $seta.t_fim -ge $seta.t_ini)
            Check "A: caneta com 4 pontos, desfeita antes do limpar" (@($caneta.pts).Count -eq 4 -and $caneta.t_fim -ge $caneta.t_ini -and $caneta.t_limpo -ge $caneta.t_fim -and $caneta.t_limpo -le $seta.t_limpo)
            Check "A: elipse na tela ate o fim (sem t_limpo)" ($null -eq $elipse.t_limpo -and $elipse.t_ini -ge $seta.t_limpo -and $elipse.t_fim -ge $elipse.t_ini)
        }
    }
    Check "A: log - pedidos da casca em modo de teste" (LogHas "A" "pedidos 'voltz-teste-casca' ligados")
    Check "A: log - 4 tracos no anotacoes.jsonl" (LogHas "A" "[voltz] anotacoes: 4 traco(s) em anotacoes.jsonl")
    Check "A: log - sobreposicao destruida no EXIT" (LogHas "A" "[voltz] anotacoes: desligadas (sobreposicao destruida)")

    # ---------------- Fase B: grupo Tela Voltz versao 1 (sem anotacoes) -> 2 ----------------
    New-CleanConfig ""
    Set-TestEnv $true $false $false $true
    Check "B1: app aberto" (Start-App)
    Check "B1: preset (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    $l1 = Screen-Layers $sceneScreen
    Check "B1: grupo versao 1 (Tela + Voltz Mouse)" ($l1.Count -eq 2 -and $l1[1].inputKind -eq "voltz_mouse_source")
    Check "B1: encerrou limpo" (Close-App "B1")
    Set-TestEnv $true $false $false $false
    Check "B2: app aberto" (Start-App)
    $l2 = Screen-Layers $sceneScreen
    Check "B2: grupo ganhou a Voltz Anotacoes no meio, travada em (0,0)" ($l2.Count -eq 3 -and $l2[0].sourceName -eq "Tela" -and $l2[1].inputKind -eq "voltz_annotations_source" -and $l2[2].inputKind -eq "voltz_mouse_source" -and $l2[1].sceneItemLocked -and $l2[1].sceneItemTransform.positionX -eq 0 -and $l2[1].sceneItemTransform.positionY -eq 0)
    Program-Scene
    [void](VendorShell "InjectStroke" (Stroke-Json "linha" "#D4F53C" 16 "[[640,720],[1920,720]]"))
    $st = Shell-State; Assert-OverlaySafe
    $img = Scene-Image $sceneScreen; $on = $img.GetPixel(320, 180); $img.Dispose()
    Check "B2: a camada migrada desenha ($on)" (Near $on 212 245 60)
    Check "B2: Tela de cor e sem retangulo de teste: nenhuma sobreposicao" ((-not $st.sobreposicao.criada) -and (@(Overlay-Windows $script:proc.Id).Count -eq 0))
    Check "B2: encerrou limpo" (Close-App "B2")
    Check "B2: log - 1 grupo com a camada de anotacoes" (LogHas "B2" "[voltz] tela voltz: 1 grupo(s) com a camada de anotacoes")
    Check "B2: log - sem captura de monitor, sem sobreposicao" (LogHas "B2" "[voltz] anotacoes: sem captura de monitor; sem sobreposicao")
    Check "B3: app aberto" (Start-App)
    $l3 = Screen-Layers $sceneScreen
    Check "B3: grupo intacto (3 camadas)" ($l3.Count -eq 3)
    Check "B3: encerrou limpo" (Close-App "B3")
    Check "B3: nao migra de novo" (-not (LogHas "B3" "[voltz] tela voltz:"))

    # ---------------- Fase C: queimadas (simula Windows < 10 2004) ----------------
    New-CleanConfig ("NextProjectName=Smoke anotacoes C`r`n")
    Set-TestEnv $true $true $true $false
    Check "C: app aberto" (Start-App)
    Check "C: preset (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    Program-Scene
    [void](VendorShell "InjectStroke" (Stroke-Json "linha" "#D4F53C" 16 "[[640,720],[1920,720]]"))
    $st = Shell-State; Assert-OverlaySafe; $feed = Feed-State
    $img = Scene-Image $sceneScreen; $on = $img.GetPixel(320, 180); $img.Dispose()
    $hwnd = [IntPtr][Int64]$st.sobreposicao.hwnd
    [uint32]$aff = 99; [void]$P::GetWindowDisplayAffinity($hwnd, [ref]$aff)
    Check "C: queimadas na casca e no plugin; a camada nao desenha ($on)" ($st.queimadas -and $feed.queimadas -and (Near $on 42 42 48))
    Check "C: sobreposicao sem afinidade (aparece na captura) e visivel (afinidade $aff)" ($aff -eq 0 -and $st.sobreposicao.visivel)
    Prepare-Recording
    Assert-NoRealCapture
    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "C: StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 3
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "C: StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 5
    Check "C: encerrou limpo" (Close-App "C")
    $dirs = Project-Dir "*Smoke anotacoes C*"
    if ($dirs.Count -eq 1) {
        $m = Read-JsonFile (Join-Path $dirs[0].FullName "projeto.voltz")
        $lines = Read-Jsonl (Join-Path $dirs[0].FullName "anotacoes.jsonl")
        Check "C: manifesto com anotacoes_queimadas e a linha de antes da gravacao no anotacoes.jsonl" ($m.anotacoes_queimadas -eq $true -and $lines.Count -eq 1 -and $lines[0].ferr -eq "linha" -and $lines[0].t_ini -eq 0)
    } else {
        Check "C: uma pasta de projeto" $false
    }
    Check "C: log - queimadas (teste)" (LogHas "C" "[voltz] anotacoes: queimadas (teste)")

    # ---------------- Fase D: somem apos 1 s, gravando ----------------
    New-CleanConfig ("NextProjectName=Smoke anotacoes D`r`n" + 'AnnotationStyle={"somem_apos_s":1}' + "`r`n")
    Set-TestEnv $true $true $false $false
    Check "D: app aberto" (Start-App)
    Check "D: preset (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    Prepare-Recording
    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "D: StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 1
    [void](VendorShell "InjectStroke" (Stroke-Json "seta" "#D4F53C" 8 "[[300,300],[900,600]]"))
    $st1 = Shell-State; Assert-OverlaySafe
    Start-Sleep -Seconds 2
    $st2 = Shell-State; $feed = Feed-State
    Check "D: o traco some sozinho (antes $(@($st1.visiveis).Count), depois $(@($st2.visiveis).Count), plugin $($feed.tracos))" (@($st1.visiveis).Count -eq 1 -and @($st2.visiveis).Count -eq 0 -and $feed.tracos -eq 0 -and -not $st2.sobreposicao.visivel)
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "D: StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 5
    Check "D: encerrou limpo" (Close-App "D")
    $dirs = Project-Dir "*Smoke anotacoes D*"
    if ($dirs.Count -eq 1) {
        $lines = Read-Jsonl (Join-Path $dirs[0].FullName "anotacoes.jsonl")
        $gap = if ($lines.Count -eq 1) { $lines[0].t_limpo - $lines[0].t_fim } else { -1 }
        Check "D: t_limpo = t_fim + 1000 ($gap)" ($lines.Count -eq 1 -and $gap -eq 1000)
    } else {
        Check "D: uma pasta de projeto" $false
    }
    Check "D: log - somem apos 1 s" (LogHas "D" "[voltz] anotacoes: somem apos 1 s")
} catch {
    Check "sem excecao ($($_.Exception.Message))" $false
} finally {
    if ($proc -and -not $proc.HasExited) {
        $proc.Refresh()
        [void]$proc.CloseMainWindow()
        if (-not $proc.WaitForExit(30000)) { Stop-Process -Id $proc.Id -Force }
    }
    Remove-Item Env:VOLTZ_TEST_LIVE, Env:VOLTZ_TEST_PRESET_STANDINS, Env:VOLTZ_TEST_OVERLAY_RECT, Env:VOLTZ_TEST_ANNOT_BURNED, Env:VOLTZ_TEST_SCREEN_LAYERS -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 5
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    if (Test-Path $bak) { Move-Item $bak $cfg }
    if (-not $Keep -and (Test-Path -LiteralPath $OutDir)) { Remove-Item -Recurse -Force -LiteralPath $OutDir; Write-Host "apagado: $OutDir" }
    if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) {
        Remove-Item -Force -LiteralPath $voltzVideos
        Write-Host "apagado (vazio): $voltzVideos"
    }
}

$orphans = @($script:overlays | Where-Object { $P::IsWindow([IntPtr]$_) })
Check "nenhuma sobreposicao orfa ($($script:overlays.Count) vista(s))" ($script:overlays.Count -ge 1 -and $orphans.Count -eq 0)
$videosAfter = @(Get-ChildItem -Force -LiteralPath $videosDir | ForEach-Object { "$($_.Name)|$($_.LastWriteTimeUtc.Ticks)|$($_.Attributes)" })
Check "Videos intacta (antes $($videosBefore.Count), depois $($videosAfter.Count))" (-not (Compare-Object $videosBefore $videosAfter))
Check "config portatil restaurada sem sobra de backup" (-not (Test-Path $bak))
$failed = 0
foreach ($k in $checks.Keys) {
    if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
}
Write-Host "logs: $logDir"
if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE ANOTACOES OK"
```

- [ ] **Step 2: Demonstração manual (o usuário roda e olha)**

Criar `tools/voltz-annot-demo.ps1` (ASCII puro):
```powershell
<#
  Demonstracao MANUAL das anotacoes do Plano 4B: o usuario roda e OLHA (nao e teste automatico).
  -Launch: guarda a config portatil (config.annotdemo-bak), cria uma limpa com o obs-websocket ligado (porta 4455, sem
    senha) e abre o Voltz portatil com VOLTZ_TEST_LIVE=1 (os tracos entram pelos pedidos de teste; o halo do mouse nao
    acompanha o mouse real nesse modo). Na tela Presets, aplique VOCE MESMO um preset com a Tela real (ex.: Tutorial).
  -Action desenhar: seta, retangulo, elipse, caneta (onda) e marca-texto, em pixels da captura da Tela.
  -Action desfazer | limpar | estado.
  -Close: fecha o Voltz aberto pelo -Launch (pelo PID salvo) e restaura a config portatil.
  O desenho aparece por cima do monitor capturado (janela transparente a cliques: clique e digite normalmente por baixo
  dela) e no programa (fonte Voltz Anotacoes); a captura da Tela NAO o contem. Este script nao move o mouse, nao clica e
  nao digita.
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-annot-demo.ps1 -Launch
       powershell -ExecutionPolicy Bypass -File tools/voltz-annot-demo.ps1 -Action desenhar
       powershell -ExecutionPolicy Bypass -File tools/voltz-annot-demo.ps1 -Close
#>
param(
    [switch]$Launch,
    [ValidateSet("", "desenhar", "desfazer", "limpar", "estado")][string]$Action = "",
    [switch]$Close,
    [string]$Config = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rundir = Join-Path $root "build_x64\rundir\$Config"
$bin = Join-Path $rundir "bin\64bit"
$exe = Join-Path $bin "VoltzStudio.exe"
$ws = Join-Path $PSScriptRoot "voltz-ws.ps1"
$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.annotdemo-bak"
$pidFile = Join-Path $env:TEMP "voltz-annot-demo.pid"

function Ws([string]$json) {
    $file = Join-Path $env:TEMP "voltz-annot-demo-req.json"
    [IO.File]::WriteAllText($file, $json, (New-Object Text.UTF8Encoding $false))
    $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $file
    Remove-Item -Force $file -ErrorAction SilentlyContinue
    return ,@($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json })
}

function Shell([string]$type, [string]$dataJson = '{}') {
    $r = Ws ('[{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste-casca","requestType":"' + $type + '","requestData":' + $dataJson + '}}]')
    if ($r.Count -lt 1 -or -not $r[0].ok) { throw "pedido $type falhou (o Voltz foi aberto com -Launch?)" }
    return $r[0].data.responseData
}

function Stroke([string]$tool, [string]$color, [int]$width, $points) {
    $pts = "[" + (($points | ForEach-Object { "[{0},{1}]" -f [int]$_[0], [int]$_[1] }) -join ",") + "]"
    $req = [ordered]@{ ferr = $tool; cor = $color; esp = $width; pts = $pts }
    [void](Shell "InjectStroke" (ConvertTo-Json $req -Compress))
}

if ($Launch) {
    if (-not (Test-Path $exe)) { throw "executavel nao encontrado: $exe" }
    if (Test-Path $bak) { throw "ja existe $bak (rode -Close antes)" }
    if (Test-Path $cfg) { Move-Item $cfg $bak }
    $voltzCfg = Join-Path $cfg "Voltz Studio"
    $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
    New-Item -ItemType Directory -Force $wsDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}')
    [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n", (New-Object Text.UTF8Encoding $false))
    $env:VOLTZ_TEST_LIVE = "1"
    $proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
    Remove-Item Env:VOLTZ_TEST_LIVE
    Set-Content -LiteralPath $pidFile -Value $proc.Id
    Write-Host "Voltz aberto (PID $($proc.Id)). Aplique um preset com a Tela real e rode -Action desenhar."
    exit 0
}

if ($Close) {
    if (Test-Path $pidFile) {
        $id = [int](Get-Content -LiteralPath $pidFile)
        $p = Get-Process -Id $id -ErrorAction SilentlyContinue
        if ($p -and $p.ProcessName -eq "VoltzStudio") {
            [void]$p.CloseMainWindow()
            if (-not $p.WaitForExit(30000)) { Stop-Process -Id $id -Force }
        }
        Remove-Item -LiteralPath $pidFile
    }
    Start-Sleep -Seconds 2
    if (Test-Path $bak) {
        if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
        Move-Item $bak $cfg
    }
    Write-Host "Voltz fechado e config portatil restaurada."
    exit 0
}

switch ($Action) {
    "desenhar" {
        $st = (Shell "AnnotationState").estado | ConvertFrom-Json
        $w = [int]$st.captura[0]; $h = [int]$st.captura[1]
        if ($w -le 0) { throw "sem Tela compartilhada: aplique um preset primeiro" }
        $e = [Math]::Max(4, [int]($w / 320))
        Stroke "seta" "#D4F53C" $e @(@((0.12 * $w), (0.25 * $h)), @((0.32 * $w), (0.42 * $h)))
        Stroke "retangulo" "#4DA3FF" $e @(@((0.45 * $w), (0.18 * $h)), @((0.8 * $w), (0.42 * $h)))
        Stroke "elipse" "#FF4D4D" $e @(@((0.1 * $w), (0.55 * $h)), @((0.35 * $w), (0.85 * $h)))
        $wave = @()
        for ($i = 0; $i -le 60; $i++) { $wave += , @(((0.45 + 0.4 * $i / 60) * $w), ((0.75 + 0.05 * [Math]::Sin($i / 4.0)) * $h)) }
        Stroke "caneta" "#D4F53C" $e $wave
        Stroke "marca_texto" "#FFE14D" $e @(@((0.45 * $w), (0.6 * $h)), @((0.85 * $w), (0.6 * $h)))
        Write-Host "5 tracos desenhados (captura ${w}x${h}); a sobreposicao fica sobre o monitor da Tela."
    }
    "desfazer" { [void](Shell "UndoStroke"); Write-Host "ultimo traco desfeito" }
    "limpar" { [void](Shell "ClearStrokes"); Write-Host "tracos limpos" }
    "estado" { (Shell "AnnotationState").estado }
    default { Write-Host "use -Launch, -Action desenhar|desfazer|limpar|estado ou -Close" }
}
```

- [ ] **Step 3: Conferir ASCII e segurança dos scripts**

```bash
grep -nP '[^\x00-\x7F]' tools/voltz-smoke-annotations.ps1 tools/voltz-annot-demo.ps1; echo "nao-ascii=$?"
grep -nE "SendInput|SetCursorPos|mouse_event|keybd_event|SendKeys|Cursor\]::Position|taskkill|Stop-Process -Name|explorer" tools/voltz-smoke-annotations.ps1 tools/voltz-annot-demo.ps1; echo "proibidos=$?"
```
Expected: `nao-ascii=1` e `proibidos=1` (nenhuma linha).

- [ ] **Step 4: Rodar o smoke**

Run: SMOKE ANOTAÇÕES (`-OutDir "<scratchpad>\smoke-anotacoes"`).
Expected: todas as linhas `OK`, última linha `SMOKE ANOTACOES OK`. Se `A: marca-texto ...` falhar só na cor do programa,
revise o `gs_enable_framebuffer_srgb(false)` da Task 5; se `A: buffer da sobreposicao` falhar, confira o retângulo
(640×360) e a escala 0,25.

- [ ] **Step 5: Commit**

```bash
git add tools/voltz-smoke-annotations.ps1 tools/voltz-annot-demo.ps1
git commit -m "test(voltz): smoke das anotacoes (camadas, sobreposicao fora da area de trabalho, jsonl, migracao, queimadas, somem apos) e demo manual

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: Verificação, registro, poda do `CLAUDE.md` e push

**Files:** `docs/voltz/backlog-plano-4.md`, `docs/voltz/upstream-patches.md` (conferência), `CLAUDE.md`, `AGENTS.md`,
`docs/HISTORICO.md`

- [ ] **Step 1:** COMPILAR TUDO (segundo plano), RODAR TESTES (esperado **153** casos), SMOKE, SMOKE GRAVAÇÃO, SMOKE
  PROJETO, SMOKE PROJETO `-ProgramOnly`, SMOKE MOUSE e SMOKE ANOTAÇÕES (um de cada vez). Todos OK.
- [ ] **Step 2:** Captura final **só da janela** da casca (bloco de teste com `$standins = $true`,
  `$overlayOffscreen = $true`; preset "Só tela / Só câmera" por UIA; `InjectStroke` da linha lima e do marca-texto da
  Task 8; `Shot $script:shell "4b-gravar"`): a prévia mostra a Tela cinza com os traços e a lista de Fontes mostra o grupo
  "Tela Voltz" aberto com "Voltz Anotações" (ícone de lápis) entre a Tela e a Voltz Mouse. Descreva a imagem no
  relatório. Nenhuma captura da tela inteira, nenhum quadro de vídeo, nenhuma captura da sobreposição.
- [ ] **Step 3:** Nenhum arquivo do OBS mudou:
  `git diff 4bfd03610..HEAD --stat -- . ':(exclude)frontend/voltz' ':(exclude)plugins/voltz-sources' ':(exclude)docs' ':(exclude)tools' ':(exclude)frontend/data/locale/voltz-pt-BR.ini' ':(exclude)frontend/data/locale/voltz-en-US.ini' ':(exclude)frontend/cmake/feature-voltz.cmake' ':(exclude)CLAUDE.md' ':(exclude)AGENTS.md'`
  → sem saída; `git diff 4bfd03610..HEAD -- plugins/CMakeLists.txt docs/voltz/upstream-patches.md` → sem saída.
- [ ] **Step 4: Backlog do Plano 4** — em `docs/voltz/backlog-plano-4.md`:
  1. Logo depois da seção `## Feito no Plano 4A` (antes de `## Próximo: Plano 4B (desenho na tela)`), acrescentar:
```markdown
## Feito no Plano 4B
- **Núcleo:** modelo do traço (`annotation-model`: caneta, marca-texto, linha, seta, retângulo, elipse; formas guardam
  primeiro e último ponto; linha do `anotacoes.jsonl` da spec; `AnnotationStyle` em `user.ini [Voltz] AnnotationStyle`
  com `somem_apos_s`), rasterizador por CPU em BGRA pré-multiplicado (`annotation-raster`: cápsulas + ponta da seta,
  maior cobertura por traço — marca-texto 3× a espessura a 40% sem "contas" nas junções), `StrokeStore` (um traço em
  andamento, desfazer, limpar, "somem após" no instante exato) e diário do `anotacoes.jsonl` (`annotation-journal`:
  linha completa ao terminar, linha curta `{"id","t_limpo"}` ao sair; consolidado no fim e na recuperação).
- **Plugin:** fonte **Voltz Anotações** (`voltz_annotations_source`, fora do "Adicionar fonte"), `TargetSize` comum às
  duas fontes, `AnnotationFeed` com mutex e versão alimentado por `voltz_annot_begin/extend/remove/clear/burned`;
  rasteriza só quando muda; framebuffer sRGB desligado + `ONE/INVSRCALPHA` (mesma cor da sobreposição).
- **Tela Voltz versão 2:** Tela · Voltz Anotações · Voltz Mouse; grupos versão 1 (4A) ganham a camada na abertura e na
  troca de coleção (log `[voltz] tela voltz: N grupo(s) com a camada de anotacoes`).
- **Sobreposição** (`OverlayWindow`): janela Win32 pura sobre o monitor da Tela, `WS_EX_LAYERED | TRANSPARENT |
  NOACTIVATE | TOOLWINDOW | TOPMOST`, `HTTRANSPARENT`, `MA_NOACTIVATE`, sem método que a torne clicável;
  `WDA_EXCLUDEFROMCAPTURE` sempre, conferido por `GetWindowDisplayAffinity`; criada só com traço, escondida sem traço,
  destruída no EXIT.
- **Queimadas:** sem `SetDisplayAffinitySupported()` ou com afinidade recusada → sobreposição na captura, fonte não
  desenha, `"anotacoes_queimadas": true`.
- **Projeto:** `anotacoes.jsonl` (traços já na tela no início com `t_ini = t_fim = 0`), `arquivos.anotacoes` com
  `origem` quando a Tela é captura de monitor.
- Testes: 153 casos; `tools/voltz-smoke-annotations.ps1` (fases A–D); `tools/voltz-annot-demo.ps1` (demo manual);
  fornecedor "voltz-teste-casca" (`InjectStroke`, `ExtendStroke`, `EndStroke`, `UndoStroke`, `ClearStrokes`,
  `ProbeOverlay`, `AnnotationState`) e pedidos novos no "voltz-teste" (`AnnotationFeedState`, `AddAnnotationSource`,
  `FeedStroke`, `FeedClear`); `VOLTZ_TEST_OVERLAY_RECT`, `VOLTZ_TEST_ANNOT_BURNED`, `VOLTZ_TEST_SCREEN_LAYERS`.
```
  2. Trocar a seção `## Próximo: Plano 4B (desenho na tela)` inteira (título e parágrafo) por:
```markdown
## Próximo: Plano 4C (modo desenho)
Modo desenho com captura de entrada e saídas garantidas (Esc, clique direito, atalho, perda de foco, inatividade,
vigia da thread da UI), barra de ferramentas (Qt com `isOBSProjectorWindow` + `WDA_EXCLUDEFROMCAPTURE`), ferramenta de
texto, cores rápidas/seletor/espessura, atalhos `VoltzStudio.Draw` (Ctrl+Shift+D)/limpar/desfazer, Ajustes › Anotações
e Ajustes › Atalhos, sobreposição em todos os monitores, aviso visível de "queimadas". Ver o fim de
`docs/superpowers/plans/2026-10-16-voltz-plano-4b-anotacoes.md`.
```
  3. No fim do arquivo, acrescentar:
```markdown
## Pendências e limites do 4B
- Traços só entram por pedidos de teste (`VOLTZ_TEST_LIVE`): o usuário ainda não desenha (Plano 4C).
- A sobreposição só existe sobre o monitor da Tela compartilhada de captura de monitor; Tela de janela/outra captura →
  sem sobreposição (os traços continuam no programa e no `anotacoes.jsonl`).
- A fonte e a sobreposição re-rasterizam a imagem inteira a cada mudança (CPU, ~15 MB em 2560×1440): ok para traços
  injetados; no 4C, com o mouse desenhando a 60 Hz, medir e, se preciso, redesenhar só o retângulo sujo
  (`UpdateLayeredWindowIndirect` com `prcDirty`) e rasterizar fora da thread gráfica.
- `AnnotationFeed` e `StrokeStore` não limitam o número de traços (só os pontos por traço: 20 000).
- "Somem após" sem animação (o traço sai inteiro no instante `t_fim + N s`).
- O `anotacoes.jsonl` usa a Tela compartilhada do início da gravação; trocar o monitor no meio não vira evento.
- Pedidos de teste da casca são enfileirados (sem resposta síncrona): quem testa espera ~400 ms antes de ler o estado.
- O fornecedor "voltz-teste-casca" não tem `unregister` do fornecedor em si (só dos pedidos, no EXIT).

## Testes manuais pendentes do 4B
- `tools/voltz-annot-demo.ps1 -Launch` com o preset Tutorial **real**, depois `-Action desenhar`: os 5 traços aparecem
  sobre o monitor, alinhados com a prévia; clicar e digitar por baixo dos traços funciona; a sobreposição não aparece
  na barra de tarefas nem no Alt+Tab; esconder "Voltz Anotações" (olho, dentro do grupo) some os traços da prévia mas
  não do monitor (a captura não contém a sobreposição).
- Gravar ~10 s com traços: `programa.mp4` com os traços; `tela.mkv` sem; `anotacoes.jsonl` com os 5 traços em pixels
  da captura; manifesto com `arquivos.anotacoes.origem` do monitor.
- Escala 125%/150% e monitor à esquerda do principal (origem negativa): traços alinhados na sobreposição e na prévia.
- Trocar o monitor da Tela em Ajustes › Captura com traços na tela: em ~1 s a sobreposição vai para o novo monitor.
- Recuperação: fechar o app pelo Gerenciador de Tarefas gravando com traços; reabrir; Projetos › Recuperar: o
  `anotacoes.jsonl` fica consolidado (sem linhas curtas).
- Windows < 10 2004 (máquina virtual, se houver): log `queimadas`, manifesto `anotacoes_queimadas: true`, traços uma
  vez só no programa.
- Desempenho com dezenas de traços na tela: sistema responsivo; CPU parada sem mudanças.
- Textos novos em en-US ("Voltz Annotations").
```
- [ ] **Step 5: `CLAUDE.md` — poda e atualização** (depois `cp CLAUDE.md AGENTS.md`). O arquivo está com ~13 KB; o
  alvo é **≤ 12 000 bytes** (`wc -c`). Em `docs/HISTORICO.md`, acrescentar no fim a seção
  `## Poda do Plano 4B (texto que saiu do CLAUDE.md, 2026-10-16)` e mover para ela, **verbatim**, cada trecho abaixo
  antes de trocá-lo no `CLAUDE.md`:
  1. *Stack & estrutura*: o item que começa com `  - Smoke: \`powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1\`; gravação real:`
     (4 linhas, até `(presets com substitutas, render, migração)`) → trocar por:
```markdown
  - Smokes (`powershell -ExecutionPolicy Bypass -File tools/<nome>.ps1`; todos guardam/restauram a config portátil):
    `voltz-smoke` · `voltz-smoke-record` (grava ~15 s em `Vídeos\Voltz Studio`) · `voltz-smoke-project -OutDir <vazia>` ·
    `voltz-smoke-mouse -OutDir <vazia>` · `voltz-smoke-annotations -OutDir <vazia>`; demo manual: `voltz-annot-demo`.
```
  2. *Estado atual*: os dois itens (`- Planos 1–3B concluídos ...` e `- Plano 4A concluído: ...`) → trocar por:
```markdown
- Planos 1–4A concluídos (fundação, casca, presets, tema, gravação em projeto com manifesto/marcadores/cursor/ISO,
  plugin voltz-sources com Voltz Mouse e grupo Tela Voltz); relato em `docs/HISTORICO.md`; pendências em
  `docs/voltz/backlog-plano-*.md`; patches no OBS em `docs/voltz/upstream-patches.md`.
- Plano 4B concluído: traços no núcleo (modelo, rasterizador, StrokeStore, `anotacoes.jsonl`), fonte Voltz Anotações
  (camada 2 da Tela Voltz, migração 1→2), sobreposição só de exibição fora da captura, "queimadas", "somem após".
  Traços só por pedidos de teste até o 4C. Testes: 153 casos + `tools/voltz-smoke-annotations.ps1`.
```
  3. *Decisões*: os itens `- Gravação em projeto sem patch: ...` (4 linhas) e `- ISO sem patch: ...` (2 linhas) → trocar
     pelo item único:
```markdown
- Gravação em projeto e ISO sem patch no OBS (`ProfileOverride` troca pasta/nome no RECORDING_STARTING; `IsoTrack` =
  canvas privado → encoder próprio → ffmpeg_muxer); detalhes em `docs/HISTORICO.md`.
```
     e o item `- Tela Voltz = um grupo por cena ...` (2 linhas) → trocar por:
```markdown
- Tela Voltz = um grupo por cena (Tela + Voltz Anotações + Voltz Mouse compartilhadas, travadas em (0,0)); migração
  uma vez por coleção (`voltz_tela_migrada`) e versão de camadas no grupo (`voltz_tela_camadas` 1 → 2, sempre); ISO na captura.
- Anotações: traços em pixels da captura da Tela; rasterizador por CPU no núcleo (o mesmo na sobreposição e na fonte);
  sobreposição = janela Win32 pura, sempre transparente a cliques no 4B, `WDA_EXCLUDEFROMCAPTURE` sempre (sem suporte →
  "queimadas": fonte não desenha + manifesto); `anotacoes.jsonl` = diário só de acréscimo, consolidado no fim/recuperação.
```
  4. *Armadilhas*: mover estes itens inteiros: `- Build Tools precisa do componente **ATL** ...`,
     `- Instalar componente do VS falha com 8006 ...`, `- Nada de IO, log, lock ou alocação dentro do gancho \`WH_MOUSE_LL\` ...`,
     `- nlohmann em alvo estático: ...` (2 linhas), `- \`ffmpeg_muxer\`/\`mp4_output\` são \`OBS_OUTPUT_AV\`: ...` (2 linhas),
     `- ISO: liberar saída → encoders → canvas ...` (2 linhas), `- \`obs_output_start\` que falha ...`,
     `- Os "skipped frames" do vídeo ...`, `- Sinal de saída enfileirado para a UI ...` → no lugar do primeiro deles,
     um só item:
```markdown
- Armadilhas arquivadas (ATL/8006 do VS, gancho `WH_MOUSE_LL`, nlohmann em alvo estático, saídas/ordem/falha do ISO,
  quadros pulados, geração do projeto): `docs/HISTORICO.md` → "Poda do Plano 4B".
```
     Trocar `- Fornecedor do obs-websocket só no \`obs_module_post_load\`; \`CreateInput\` recusa fontes \`OBS_SOURCE_CAP_DISABLED\`.` por:
```markdown
- Fornecedor do obs-websocket: plugin só no `obs_module_post_load`; frontend depois do `OBSInit`, com mutex próprio (o
  callback roda sem lock e o unregister não espera); `obs_data` não guarda lista de números (pontos/estado como texto
  JSON); `CreateInput` recusa fontes `OBS_SOURCE_CAP_DISABLED`.
```
     E acrescentar no fim das Armadilhas (só as que se confirmaram na execução):
```markdown
- `OBSApp::notify` reaplica `HideOBSWindowsFromCapture` (inclusive `WDA_NONE`) em toda janela Qt mostrada: janela que
  precisa ficar fora da captura é Win32 pura ou tem a propriedade `isOBSProjectorWindow`.
- `WDA_EXCLUDEFROMCAPTURE` antes do Windows 10 2004 vira caixa preta: só com `SetDisplayAffinitySupported()`.
- Fonte sem `OBS_SOURCE_SRGB` que mistura alfa: desligar o framebuffer sRGB no render (o principal o liga) e usar
  `ONE/INVSRCALPHA` com imagem pré-multiplicada, senão a cor translúcida muda.
- Testes com sobreposição: `VOLTZ_TEST_OVERLAY_RECT` fora da área de trabalho + `Assert-OverlaySafe` depois de cada
  traço; nunca modo clicável em teste automático.
```
  5. *Próximos passos* → trocar os dois itens por:
```markdown
1. Escrever e executar o Plano 4C (modo desenho com saídas garantidas, barra, texto, atalhos, Ajustes › Anotações/Atalhos,
   sobreposição em todos os monitores) a partir do fim do plano 4B e do `docs/voltz/backlog-plano-4.md`.
2. Depois: Plano 5 (marca visual + instalador + CI).
```
  Conferir: `wc -c CLAUDE.md` ≤ 12000. Se passar, mover também (verbatim, para a mesma seção do HISTORICO) os itens de
  QSS da casca (`- Especificidade QSS: ...` (2 linhas), `- \`#voltzShell QWidget { font-size }\` ...`,
  `- \`objectName\` usado por regra QSS ...`) e pôr no lugar `- Armadilhas de QSS da casca (especificidade, font-size,
  objectName antes do setStyleSheet): \`docs/HISTORICO.md\` → "Poda do Plano 4B".`; conferir de novo. Depois
  `cp CLAUDE.md AGENTS.md` e `diff CLAUDE.md AGENTS.md` → sem saída. Em `docs/HISTORICO.md`, acrescentar ainda
  `## Plano 4B (2026-10-16)` com 3–4 linhas (núcleo das anotações e rasterizador, fonte Voltz Anotações + Tela Voltz
  v2, sobreposição só de exibição fora da captura + queimadas, `anotacoes.jsonl`; 153 casos + smoke; pendências em
  backlog-plano-4.md).
- [ ] **Step 6:** Commit e push:
```bash
git add docs/voltz/backlog-plano-4.md docs/HISTORICO.md CLAUDE.md AGENTS.md
git commit -m "docs(voltz): registro do Plano 4B (backlog, memoria podada)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push origin voltz
```
Expected: push sem `--force`, `voltz -> voltz`.

---
## Roteiro de teste manual do Plano 4B (para o usuário, depois da Task 11)

Nada aqui é automatizado: é o usuário quem roda, olha e usa o PC. O 4B não tem modo desenho — os traços vêm do script de
demonstração, e a sobreposição **sempre** deixa o clique passar.

1. `powershell -ExecutionPolicy Bypass -File tools/voltz-annot-demo.ps1 -Launch` → na tela Presets, aplicar **Tutorial**
   (monitor e webcam reais) → `... -Action desenhar`.
   - Os 5 traços (seta lima, retângulo azul, elipse vermelha, onda lima, marca-texto amarelo) aparecem por cima do monitor
     capturado e **na mesma posição** na prévia do Voltz.
   - Clicar, arrastar e digitar em qualquer janela por baixo dos traços funciona normalmente; o foco nunca vai para a
     sobreposição; ela não aparece na barra de tarefas nem no Alt+Tab.
   - Em Fontes, abrir o grupo "Tela Voltz" e esconder **Voltz Anotações** (olho): os traços somem da prévia mas continuam
     no monitor ⇒ a captura da Tela não contém a sobreposição. Mostrar de novo.
   - (Opcional) Compartilhar a tela numa chamada ou capturar o mesmo monitor em outro programa: os traços não aparecem.
2. Gravar ~10 s com os traços; abrir no player: `programa.mp4` **com** os traços, `tela.mkv` **sem**; `anotacoes.jsonl`
   com 5 linhas em pixels da captura; `projeto.voltz → arquivos.anotacoes` com `origem` do monitor.
3. `-Action desfazer` (some a última) e `-Action limpar` (somem todas): na sobreposição e na prévia.
4. Escala do Windows 125%/150% e um monitor à esquerda do principal: traços alinhados entre sobreposição e prévia.
5. Trocar o monitor da Tela em Ajustes › Captura com traços na tela: em ~1 s a sobreposição muda de monitor.
6. Recuperação: gravando com traços, fechar o app pelo Gerenciador de Tarefas; reabrir (o OBS pode oferecer o modo de
   segurança — responder normalmente); Projetos › Recuperar: o `anotacoes.jsonl` fica consolidado (sem linhas curtas
   `{"id":..,"t_limpo":..}`).
7. `-Close`: a sobreposição some, o Voltz fecha e a config portátil volta a ser a de antes.
8. (Se houver uma VM com Windows 10 anterior ao 2004) log `[voltz] anotacoes: queimadas`, manifesto
   `"anotacoes_queimadas": true` e os traços aparecem uma vez só no programa (pela captura).

---

## Auto-revisão (feita ao escrever)

- **Cobertura da spec (parte do 4B):** §3 `annotate/OverlayWindow` (Task 7) e `StrokeStore` (Task 3), fonte "Voltz
  Anotações" no plugin (Task 5); §4.4 Tela Voltz com 3 camadas do tamanho da captura (Tasks 5 e 6); §5.1/§5.2
  `anotacoes.jsonl` e `arquivos.anotacoes` (Tasks 3, 4, 9); §5.5 tempos relativos ao t0 sem descontar pausas (Task 3);
  §6.2 sobreposição topmost com `WDA_EXCLUDEFROMCAPTURE`, transparente a cliques fora do modo desenho, traços vetoriais
  em coordenadas de `tela.mkv`, formato `{"id","ferr","cor","esp","pts","t_ini","t_fim","t_limpo"}`, desfazer, limpar,
  "somem após N s", ferramentas caneta/marca-texto/linha/seta/retângulo/elipse (Tasks 1–3, 5, 7, 8); §6.3 e §8 Windows
  < 10 2004 → `anotacoes_queimadas` + camada que não duplica (Tasks 4, 5, 8, 9); §9 serialização de traços testada
  (Tasks 1–3) e integração por websocket (Task 10). **Fora do 4B, com motivo e destino:** modo desenho/entrada, barra,
  texto, cores rápidas/seletor, atalhos (`Ctrl+Shift+D`, Esc), Ajustes › Anotações e Ajustes › Atalhos (§4.3, §6.2) →
  Plano 4C (abaixo).
- **Segurança:** nenhum passo injeta entrada, captura a tela inteira ou lê quadros; a sobreposição de teste fica fora
  da área de trabalho e é conferida por API; `Assert-OverlaySafe` encerra o app (PID) se a janela deixar de ser
  transparente a cliques; o código da sobreposição é conferido por `grep` (Task 7 Step 3); scripts conferidos por `grep`
  (Task 10 Step 3); o fim de cada teste confere que nenhuma sobreposição ficou.
- **Placeholders:** nenhum "TBD". Onde só a execução decide há critério objetivo: pixels com tolerância 24 (`Near`),
  valores exatos do rasterizador com a conta ao lado, linhas de log exatas, ordem das linhas do `anotacoes.jsonl`.
- **Consistência de nomes:** núcleo `AnnotationTool`, `toolToString`, `toolFromString`, `isShapeTool`, `StrokePoint`,
  `kStrokeWidthMin/Max`, `kMaxStrokePoints`, `kFadeSecondsMax`, `clampStrokeWidth`, `Stroke` (`id`, `tool`, `color`,
  `width`, `points`, `startMs`, `endMs`, `clearedMs`), `appendStrokePoints`, `pointsToJson`, `pointsFromJson`,
  `strokeToJsonLine`, `strokeFromJson`, `AnnotationStyle` (`tool`, `color`, `width`, `fadeSeconds`),
  `clampAnnotationStyle`, `annotationStyleToJson`, `annotationStyleFromJson`; `kHighlighterOpacity`,
  `kHighlighterWidthFactor`, `strokeRenderWidth`, `strokeOpacity`, `Capsule`, `FilledTriangle`, `StrokeShape`,
  `strokeShape`, `RasterImage`, `clearRaster`, `rasterizeStroke`; `LiveStroke` (`stroke`, `startNs`, `endNs`, `open`),
  `RemovedStroke` (`live`, `removedNs`), `StrokeStore` (`begin`, `extend`, `end`, `undo`, `clear`, `expire`,
  `nextExpiryNs`, `visible`, `strokes`, `openId`); `AnnotationJournal` (`initialLines`, `strokeFinished`,
  `strokeRemoved`, `finishOpen`, `strokeCount`), `consolidateAnnotations`, `kAnnotationsJournalUpdateKey`; manifesto
  `kAnnotationsFileName`, `AnnotationsFile`, `annotations`, `annotationsBurned`; `voltz::live::{kAnnotationsSourceId,
  kProcAnnot*/kDeclAnnot* (Begin, Extend, Remove, Clear, Burned), kTestVendorShell, kTestOverlayRectEnv,
  kTestBurnedEnv}`; plugin `TargetSize` (`setUuid`, `tick`, `width`, `height`), `AnnotationFeed` (`instance`, `begin`,
  `extend`, `remove`, `clear`, `setBurned`, `snapshotIfChanged`, `describe`, `addShowing`, `showing`),
  `registerAnnotationProcs`, `registerAnnotationsSource`, pedidos `AnnotationFeedState`/`AddAnnotationSource`/
  `FeedStroke`/`FeedClear`; frontend `OverlayWindow` (`create`, `destroy`, `excludeFromCapture`, `setRect`, `render`,
  `pixelHex`, `info`, `Info`), `AnnotationController` (`shutdown`, `beginStroke`, `extendStroke`, `endStroke`, `undo`,
  `clear`, `probeOverlay`, `visibleStrokes`, `style`, `burned`; sinais `strokeEnded`, `strokesRemoved`,
  `burnedChanged`), `AnnotationTestVendor` (`start`, `stop`, `publish`), pedidos `InjectStroke`/`ExtendStroke`/
  `EndStroke`/`UndoStroke`/`ClearStrokes`/`ProbeOverlay`/`AnnotationState`; `SharedSource::Annotations`
  (`AnnotationsSourceUuid`), `syncTarget`, `upgradeScreenGroups`, `testLayersV1`, `kScreenLayersMouseOnly`,
  `kScreenLayersVersion = 2`, `VOLTZ_TEST_SCREEN_LAYERS`; `ProjectRecorder::setAnnotationController`,
  `startAnnotations`, `stopAnnotations`, `appendAnnotationLines`; classe de janela `VoltzAnnotationOverlay`; locale
  `Voltz.Source.Annotations` (casca) e `VoltzAnnotations` (plugin); variáveis `VOLTZ_TEST_LIVE`,
  `VOLTZ_TEST_PRESET_STANDINS`, `VOLTZ_TEST_OVERLAY_RECT`, `VOLTZ_TEST_ANNOT_BURNED`, `VOLTZ_TEST_SCREEN_LAYERS` iguais
  no código, no bloco de teste e no smoke.
- **Contagem de testes:** 126 (base) → 133 (Task 1: 7 casos) → 141 (Task 2: 8 casos) → 151 (Task 3: 5 + 5 casos) → 153
  (Task 4: 2 casos).

---

## Plano 4C (próximo, a escrever depois do 4B) — modo desenho

- **Modo desenho** (`AnnotationController` ganha `setDrawMode(bool)` e uma `DrawSession` nova; a `OverlayWindow` do 4B
  continua só de exibição): atalho do OBS `VoltzStudio.Draw` (padrão Ctrl+Shift+D) liga/desliga; ligado, uma janela
  **de entrada** própria (`DrawInputWindow`, Win32, `WS_EX_TOPMOST | WS_EX_TOOLWINDOW`, sem `WS_EX_TRANSPARENT`,
  `WDA_EXCLUDEFROMCAPTURE`) cobre os monitores e converte o mouse (pixels físicos → `mapToCapture` da Tela) em
  `beginStroke/extendStroke/endStroke`.
- **Saídas garantidas (todas obrigatórias, cada uma com teste da máquina de estados no núcleo com relógio falso):**
  Esc (`WM_KEYDOWN` na janela de entrada); clique direito; o mesmo atalho; perda de foco (`WM_ACTIVATE` com
  `WA_INACTIVE`, `WM_KILLFOCUS`, `WM_CANCELMODE` — Alt+Tab, tecla Windows, outro app no primeiro plano); **inatividade**
  (30 s sem entrada); **vigia da UI** (thread própria: se a thread da UI ficar > 1 s sem pulsar, aplica
  `WS_EX_TRANSPARENT` com `SetWindowLongPtr` e `ShowWindowAsync(SW_HIDE)` — nada de `SendMessage` para a thread
  travada); EXIT do app. O modo **nunca** liga sozinho (não é salvo; começa desligado em toda abertura) e é **recusado**
  com `VOLTZ_TEST_LIVE` (teste automático não consegue ligá-lo). `SetCapture` só entre botão apertado e solto.
- **Barra de ferramentas** (Qt, propriedade `isOBSProjectorWindow` + `WDA_EXCLUDEFROMCAPTURE`, sem roubar o foco do app
  gravado): caneta, marca-texto, linha, seta, retângulo, elipse, **texto** (caixa de digitação; o texto vira imagem no
  frontend e um traço `"ferr":"texto"` com `"texto"`/`"tam"` no `anotacoes.jsonl`), 4 cores rápidas + seletor,
  espessura, desfazer, limpar, "somem após N s".
- **Ajustes › Anotações** (estilo padrão, "somem após", aviso de "queimadas") e **Ajustes › Atalhos** (`OBSHotkeyEdit`
  para marcador, holofote, desenhar, limpar, desfazer); sobreposição em **todos** os monitores (traço fora do monitor
  da Tela fica só na sobreposição e não entra no `anotacoes.jsonl`); aviso visível de "queimadas" ao ligar o modo.
- **Desempenho:** redesenho só do retângulo sujo (`UpdateLayeredWindowIndirect` + `prcDirty`) e rasterização da fonte
  fora da thread gráfica, se a medição com o mouse a 60 Hz pedir.
- **Testes:** núcleo (máquina de estados do modo desenho e de todas as saídas com relógio falso, conversão de
  coordenadas da entrada, texto no jsonl); integração só com o modo desligado (como no 4B); a interação real é verificada
  **só** pelo roteiro manual do usuário: ligar/desligar pelo atalho; sair por Esc, clique direito, Alt+Tab, tecla
  Windows e 30 s parado; desenhar cada ferramenta em cada monitor; texto com acentos; travar a UI de propósito (build de
  depuração) e ver o clique voltar a passar em ~1 s; gravar e conferir `programa.mp4`/`tela.mkv`/`anotacoes.jsonl`.
