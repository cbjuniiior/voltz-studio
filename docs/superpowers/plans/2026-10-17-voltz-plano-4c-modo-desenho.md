# Voltz Studio — Plano 4C: modo desenho (sessão com saídas garantidas, janela de entrada em thread própria, barra, atalhos e redesenho parcial) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** O usuário passa a **desenhar na tela** (spec §6.2): o atalho `VoltzStudio.Draw` (padrão Ctrl+Shift+D) ou o
botão "Desenhar" da tela Gravar liga o modo desenho sobre o monitor da Tela; uma barra flutuante escolhe ferramenta
(caneta, marca-texto, linha, seta, retângulo, elipse), cor (4 rápidas + 8 a mais), espessura, desfazer, limpar e "traços
somem após"; o modo **sai sempre** por Esc, clique direito, o mesmo atalho, o botão Sair, perda de foco, Alt+F4, troca de
monitores, 30 s parado ou pelo vigia da thread da UI (1 s) — e **nunca liga sozinho nem em teste automático**
(`VOLTZ_TEST_LIVE`). Junto: redesenho só do retângulo sujo (sobreposição e fonte), medidas de tempo, aviso visível de
"queimadas", janelas fantasma desligadas, `ensureClickThrough` que devolve o "sempre no topo", cliques sem onda durante o
desenho e o conserto do `-Close` da demo do 4B.

**Architecture:** As decisões ficam no núcleo, puras e testadas com relógio injetado: `draw-session` (entrada →
comandos de traço; todas as saídas garantidas; recusa de entrada) e o retângulo sujo em `annotation-raster`
(`strokeBounds`, `redrawRegion`, `appendDirtyRect`, `DirtyHistory`). A captura de entrada é uma camada Win32 **fina e
isolada num único arquivo** (`annotate/DrawInputWindow.cpp`): uma janela própria numa **thread própria** sobre o monitor
da Tela, que roda a `DrawSession`, decide as saídas sem depender da thread da UI (o vigia é ela mesma) e entrega comandos
à UI por uma fila com mutex. Na UI, `DrawModeController` liga/desliga (só por atalho ou botão), pulsa a cada 250 ms,
mostra a `DrawToolbar` (Qt, sem foco, fora da captura, com um "buraco" na janela de entrada para ser clicável) e
repassa os comandos ao `AnnotationController` do 4B (que agora redesenha só o que mudou). **Nenhum patch em arquivo do
OBS.**

**Tech Stack:** C++17, Qt 6.11.1, libobs/frontend 32.2.2 (`obs_hotkey_register_frontend`, `obs_frontend_*`,
`proc_handler_*`, `gs_texture_set_image`), obs-websocket 5.7.4 (`TriggerHotkeyByName`, fornecedor de teste), Win32
(`CreateWindowExW` em camadas com `LWA_ALPHA` 1/255, `SetCapture`/`ReleaseCapture`, `SetForegroundWindow`,
`SetWindowRgn`, `SetTimer`, `UpdateLayeredWindowIndirect` + `prcDirty`, `SetWindowDisplayAffinity`,
`DisableProcessWindowsGhosting`, `SetThreadDpiAwarenessContext`), nlohmann_json 3.11, doctest. Testes ao vivo por
`tools/voltz-ws.ps1`, `tools/voltz-uia.ps1` e P/Invoke **só de leitura** (`FindWindowEx`, `EnumWindows`,
`GetWindowLongPtr`, `GetWindowDisplayAffinity`, `GetGUIThreadInfo`, `IsHungAppWindow`).

**Spec:** `docs/superpowers/specs/2026-10-15-voltz-studio-base-gravacao-design.md` §3 (annotate/: OverlayWindow,
Toolbar), §4.3 (Ajustes → Anotações/Atalhos — vão para o 4D, ver abaixo), §6.2 (atalho global Ctrl+Shift+D, barra, Esc
sai, ferramentas, cores, espessura, desfazer, limpar, "somem após", clicável só no modo desenho), §6.3 e §8 (aviso de
"queimadas"), §9 (lógica testada no núcleo) · **Backlog:** `docs/voltz/backlog-plano-4.md` ("Próximo: Plano 4C",
"Pendências e limites do 4B") e o fim de `docs/superpowers/plans/2026-10-16-voltz-plano-4b-anotacoes.md` · **Modelo de
formato:** `docs/superpowers/plans/2026-10-16-voltz-plano-4b-anotacoes.md`.

**Base deste plano:** commit `39bf223cb` (branch `voltz`, publicado) + o commit que adiciona este arquivo. Contagem de
testes na base: **153** casos.

## Divisão do restante do Plano 4 (e o porquê)

O que sobrou do Plano 4 depois do 4B passa de 20 tasks e mistura três naturezas: **(a)** a máquina de estados e a
captura de entrada (o risco central: uma janela que recebe o mouse e o teclado do usuário), com a barra, os atalhos e o
desempenho que o desenho com o mouse a 60 Hz exige; **(b)** a ferramenta de **texto** (caixa de digitação, fonte
tipográfica que o plugin — sem Qt — também precisa desenhar, `"ferr":"texto"` no `anotacoes.jsonl`); **(c)** Ajustes ›
Anotações, Ajustes › Atalhos e o desenho em **todos os monitores** (coordenadas fora da captura, uma sobreposição por
monitor). (a) é o que precisa sair primeiro e sozinho: é onde mora o risco de travar o PC do usuário, e cada saída
garantida precisa estar provada antes de qualquer outra coisa ser desenhada por cima. (b) e (c) não mudam a segurança da
entrada e dependem de (a) pronto. Por isso:

- **Plano 4C (este arquivo):** (a) inteiro — núcleo da sessão de desenho com todas as saídas, janela de entrada em thread
  própria (o único arquivo que captura entrada), barra de ferramentas, atalhos (desenhar, desfazer, limpar), botão
  "Desenhar", redesenho parcial com medidas, aviso de "queimadas", fantasma desligado, riscos herdados do 4B. O modo
  desenho fica **só sobre o monitor da Tela** (os outros monitores continuam livres para o usuário; clicar neles sai do
  modo por perda de foco).
- **Plano 4D (próximo, descrito no fim):** texto, Ajustes › Anotações e › Atalhos, desenho e sobreposição em todos os
  monitores, barra arrastável.

**Fora do 4C, com o porquê:**
- **Ferramenta de texto:** 4D (exige rasterizar fonte tipográfica também no plugin e uma caixa de digitação que pega o
  teclado de outro jeito; ver o fim do arquivo).
- **Ajustes › Anotações e Ajustes › Atalhos:** 4D. No 4C o estilo é escolhido na barra (e salvo), os atalhos se trocam em
  Configurações → Atalhos (modo clássico), como o do holofote no 4A, e o botão "Desenhar" mostra o atalho atual.
- **Seletor de cor completo (`QColorDialog`):** 4D (em Ajustes). Um diálogo modal tiraria o foco da janela de entrada —
  e perda de foco **sai** do modo desenho. A barra tem 4 cores rápidas + 8 a mais, sem diálogo.
- **Desenhar em monitores que não são o da Tela:** 4D (traços fora da captura não têm coordenada no `anotacoes.jsonl`).
- **Rasterizar a fonte fora da thread gráfica:** só se a medição do 4C pedir (a Task 8 mede; o backlog registra).

## Descobertas que decidem o desenho (conferidas no código 32.2.2 e na API do Windows)

- **Atalhos do OBS são lidos por sondagem e roteados à UI:** o libobs lê as teclas na thread de atalhos e o
  `OBSBasic::HotkeyTriggered` enfileira o callback na thread da UI (`frontend/widgets/OBSBasic_Hotkeys.cpp:76-87`). O
  padrão `HotkeyFocusType = NeverDisableHotkeys` (`frontend/OBSApp.cpp:390`, `:1229-1243`) deixa o Ctrl+Shift+D valer
  com qualquer janela em primeiro plano, inclusive a de entrada do desenho. Com "desligar atalhos com o app em foco" o
  mesmo atalho não sai do modo — Esc, clique direito e o botão Sair continuam. `TriggerHotkeyByName` chama o callback
  na thread do websocket com o lock dos atalhos preso (Armadilha do `CLAUDE.md`): os callbacks só enfileiram.
- **`OBSApp::notify` reaplica a afinidade em janelas Qt `Tool`** no `QEvent::Show` (`frontend/OBSApp.cpp:1551-1587`),
  exceto com a propriedade `isOBSProjectorWindow` no `QWindow` (`frontend/widgets/OBSBasic.cpp:2219`). ⇒ A barra (Qt)
  recebe a propriedade antes do primeiro `show()` e a afinidade é posta por nós.
- **`HTTRANSPARENT` só passa o clique para janelas da mesma thread** (documentação do `WM_NCHITTEST`). A janela de
  entrada vive em outra thread que a barra ⇒ a barra fica clicável por um **buraco** na região da janela de entrada
  (`SetWindowRgn`), não por hit-test.
- **Dono (owner) de outra thread liga as filas de entrada** das duas threads (como um `AttachThreadInput` implícito) ⇒
  nem a janela de entrada nem a barra têm dono de outra thread (a barra é Qt sem pai; a janela de entrada não tem dono).
- **`SetWindowLongPtr(GWL_EXSTYLE)` manda `WM_STYLECHANGING/ED` síncronos** à thread dona da janela; `ShowWindowAsync` e
  `SWP_ASYNCWINDOWPOS` só **postam**. ⇒ Um vigia em outra thread **não consegue** tornar transparente uma janela cuja
  thread travou (ou trava junto, ou nada acontece). Por isso a janela de entrada mora numa **thread própria** que não faz
  nada além dela: se a thread da UI travar, a janela continua respondendo, e o vigia é a própria thread de entrada (lê o
  pulso da UI no tique de 100 ms e sai).
- **Janela fantasma:** quando a thread dona de uma janela de topo para de ler mensagens por ~5 s, o Windows pode pôr no
  lugar dela uma janela "Ghost" (classe `Ghost`) com a última imagem — e essa janela **recebe cliques**. Para a
  sobreposição do 4B (que cobre um monitor inteiro, na thread da UI) isso bloquearia o monitor. ⇒
  `DisableProcessWindowsGhosting()` no começo da casca (vale para o processo; trade-off na seção de decisões).
- **Foco:** um processo em segundo plano não consegue `SetForegroundWindow` (regra de primeiro plano do Windows). Pelo
  atalho, o Voltz normalmente está em segundo plano ⇒ a janela de entrada aparece no topo mas **sem ativar**; o primeiro
  clique nela a ativa (e já começa o traço). Pelo botão "Desenhar", o Voltz está em primeiro plano e ela já nasce ativa.
  `WM_ACTIVATE` precisa passar pelo `DefWindowProc` para o foco do teclado ir para a janela (senão o Esc não chega).
- **Raw Input:** o `LiveMouse` registra o mouse com `RIDEV_INPUTSINK` e `hwndTarget` próprio (`frontend/voltz/project/
  LiveMouse.cpp:253-258`) e o remove ao parar (`:305-310`). O modo desenho **não registra nada** (lê `WM_MOUSEMOVE`/
  `WM_LBUTTONDOWN` da própria janela) ⇒ não há registro para disputar nem `RIDEV_REMOVE` novo. Com `hwndTarget` dado, o
  `WM_INPUT` continua indo para a janela do `LiveMouse` com o Voltz em primeiro plano.
- **Janela em camadas com `LWA_ALPHA` = 1/255:** invisível a olho e ainda recebe o mouse (um pixel de alfa 0 deixaria o
  clique passar). Com `WDA_EXCLUDEFROMCAPTURE` nem o 1/255 entra na captura.
- **`QScreen::name()` no Windows é o nome GDI do monitor** (`\\.\DISPLAY1`), o mesmo `MONITORINFOEX::szDevice` que o
  `findMonitor` já percorre (`frontend/voltz/project/MonitorLocator.cpp:24-47`) ⇒ a barra vai para o monitor da Tela
  pelo nome, em coordenadas lógicas do Qt, sem conta de DPI à mão.
- **Textura dinâmica do D3D11 é mapeada com `D3D11_MAP_WRITE_DISCARD`** (`libobs-d3d11/d3d11-subsystem.cpp:2755`): o
  `gs_texture_set_image` sempre reenvia a textura inteira. ⇒ Na fonte, o ganho do retângulo sujo é na **rasterização**
  (CPU); o envio continua inteiro, no máximo uma vez por quadro. Na sobreposição, `UpdateLayeredWindowIndirect` com
  `prcDirty` atualiza só a região.
- **`OBSStudioAPI::on_event` percorre os callbacks de trás para frente** (`frontend/OBSStudioAPI.cpp:752`): o
  `DrawModeController` (criado depois da `FrontendBridge`) pode tirar o próprio callback durante o EXIT disparado pela
  ponte.

---

## Decisões técnicas (e o porquê)

| Tema | Decisão | Por quê |
|---|---|---|
| Divisão | 4C = sessão de desenho, entrada, barra, atalhos, desempenho; 4D = texto, Ajustes › Anotações/Atalhos, todos os monitores | Ver acima: o risco da entrada sai provado antes do resto. |
| Máquina de estados | `DrawSession` no núcleo (`draw-session`): entrada (mouse em pixels do desktop, teclas, ativação, captura, tique com o pulso da UI) → comandos `Begin/Extend/End/Undo/Exit`; relógio vem em cada entrada; depois de sair ignora tudo; toda saída termina antes o traço em andamento | Toda regra de saída testada com doctest e relógio falso; o Win32 só traduz mensagens. |
| Saídas garantidas | Esc · clique direito · o mesmo atalho · botão Sair da barra · perda de ativação (`WA_INACTIVE`) · captura perdida (`WM_CANCELMODE`/`WM_CAPTURECHANGED`) · Alt+F4 (`WM_CLOSE`) · `WM_DISPLAYCHANGE` · Tela/monitor trocado (conferido a cada 1 s) · **30 s sem entrada** · **vigia: UI sem pulsar por 1 s** (pulso nunca dado = sai) · saída do app | Spec §6.2 (Esc) + backlog do 4B; nenhuma depende da thread da UI, exceto atalho/botão/alvo (que não são necessárias para destravar o PC). |
| Nunca sozinho | O modo não é salvo (sempre desligado ao abrir, log `desligado ao abrir`); só liga por `DrawTrigger::Hotkey` (callback do atalho, `pressed`) ou `DrawTrigger::Button`; `checkDrawEntry` recusa saindo/`VOLTZ_TEST_LIVE`/sem alvo; `DrawInputWindow::start` recusa `VOLTZ_TEST_LIVE` de novo antes de criar a thread | Teste automático não consegue ligá-lo nem por engano (duas travas independentes). |
| Janela de entrada | `DrawInputWindow`: thread própria (`voltz-draw-input`, Per-Monitor v2), janela `VoltzDrawInput` `WS_POPUP`, `WS_EX_LAYERED | TOOLWINDOW | TOPMOST` (sem `TRANSPARENT`/`NOACTIVATE`), `LWA_ALPHA` 1, `WDA_EXCLUDEFROMCAPTURE` quando suportado, **só sobre o monitor da Tela**, cursor em cruz, sem dono; `SetCapture` só entre botão apertado e solto; saída: solta o mouse, esconde, devolve o primeiro plano a quem o tinha (se ainda era nosso) | Se a UI travar, a janela continua viva e sai sozinha; outros monitores ficam livres. |
| Exceção de segurança | `SetCapture`, `ReleaseCapture`, `SetForegroundWindow` (e qualquer outra API de foco/captura) **só** em `frontend/voltz/annotate/DrawInputWindow.cpp`; injeção/bloqueio (`SendInput`, `SetCursorPos`, `mouse_event`, `keybd_event`, `BlockInput`, `ClipCursor`, `RegisterHotKey`) em lugar nenhum; observação passiva já existente só onde já estava (`SetWindowsHookEx` no `CursorTracker.cpp` do 3A, `RegisterRawInputDevices` no `LiveMouse.cpp` do 4A) | Uma lista de exceções de um arquivo só: quem revisa a captura de entrada lê um arquivo de ~300 linhas. |
| Canal entrada → UI | `DrawShared` (mutex): fila de `DrawCommand`, `wake` chamado sob o mutex (posta um `drain` na UI, um por vez), retângulo do buraco da barra; pulso da UI num atômico | A thread de entrada nunca espera a UI; o shutdown anula `wake` antes de o controlador morrer. |
| Barra | `DrawToolbar` Qt sem pai: `Qt::Tool | Frameless | StaysOnTop | WindowDoesNotAcceptFocus` + `WS_EX_NOACTIVATE`, `isOBSProjectorWindow`, `WDA_EXCLUDEFROMCAPTURE` (quando suportado e não queimado), no alto e ao centro do monitor da Tela; buraco na região da janela de entrada; 6 ferramentas · 4 cores + 8 · 4 espessuras · desfazer · limpar · somem após (nunca → 3 → 5 → 10 s) · sair; aviso de "queimadas"; QSS próprio `#voltzDrawToolbar` | Clicar na barra não tira a ativação da janela de entrada (o Esc segue valendo); sem diálogo modal. |
| Atalhos | `VoltzStudio.Draw` (Ctrl+Shift+D), `VoltzStudio.UndoAnnotation` e `VoltzStudio.ClearAnnotations` (sem tecla padrão — os atalhos do OBS não consomem a tecla e Ctrl+Shift+Z/X iriam também para o app em uso), carregados do perfil como `VoltzStudio.AddMarker`; dentro do modo desenho, Ctrl+Z desfaz (só na janela de entrada) | Mesmo padrão do 4A; sem conflito global novo. |
| Desempenho | Retângulo sujo: `strokeBounds`/`appendDirtyRect` (peças novas da caneta; forma antiga ∪ nova) + `redrawRegion` (limpa a região e redesenha os traços que a tocam, recortados) — **byte a byte igual** a redesenhar tudo (doctest); sobreposição com `UpdateLayeredWindowIndirect` + `prcDirty`; fonte com `DirtyHistory` por versão; medidas (parciais/completas, média/máximo em µs) no estado de teste, no log do modo desenho e no log do plugin a cada 600 redesenhos | O desenho a 60 Hz não pode limpar 15 MB a cada ponto; a igualdade exata garante que o 4B continua certo. |
| Fantasma | `DisableProcessWindowsGhosting()` no construtor da casca (log `[voltz] janelas fantasma do Windows desligadas no processo`) | O fantasma de uma janela que cobre o monitor receberia cliques. Custo: com o Voltz travado, a janela dele não vira fantasma (não dá para arrastá-la; o Gerenciador de Tarefas continua). |
| `ensureClickThrough` | Se o `WS_EX_TOPMOST` sumiu, `SetWindowPos(HWND_TOPMOST, …, SWP_NOACTIVATE)` (o bit sozinho não muda a ordem Z) | Pendência do 4B. |
| Cliques no modo desenho | `LiveMouse::setClicksSuppressed(true)` enquanto o modo está ligado: o halo segue o mouse, mas cada traço não começa com uma onda | Gravação limpa; nenhum registro de Raw Input muda. |
| Testes seguros | Ver a seção abaixo | — |

---

## Segurança dos testes (o ponto central deste plano)

O modo desenho captura o mouse e o teclado. O PC é do usuário e ele o usa durante os testes. Cada regra abaixo tem um
mecanismo concreto no código ou no script:

1. **Teste automático nunca liga o modo desenho.** `checkDrawEntry` (núcleo, testado) recusa com `VOLTZ_TEST_LIVE` antes
   de procurar alvo; `DrawInputWindow::start` recusa de novo antes de criar a thread (defesa em profundidade); a recusa
   vira log (`[voltz] modo desenho: recusado (teste)`) e estado (`desenho_recusa: "teste"`). O único gatilho usado em
   teste é `TriggerHotkeyByName "VoltzStudio.Draw"` — **só** para provar a recusa — e sempre com o vigia rodando. O botão
   "Desenhar" nunca é acionado por UI Automation.
2. **Nada em teste toma foco, captura o mouse ou cobre a área visível.** Scripts não usam `SendInput`, `SetCursorPos`,
   `mouse_event`, `keybd_event`, `SendKeys`, `SetForegroundWindow`, `BlockInput`, `ClipCursor` nem
   `[System.Windows.Forms.Cursor]::Position` (grep nas Tasks 8 e 9); o único P/Invoke que escreve é `PostMessage(WM_CLOSE)`
   na janela da casca do PID lançado, guardada na abertura.
3. **`Assert-DrawSafe` depois de cada ação e um vigia a cada 200 ms** (job em segundo plano), só com leitura: (a)
   sobreposições `VoltzAnnotationOverlay` do PID por `FindWindowEx` (título nulo de verdade, em C#) ∪ `EnumWindows` —
   listas divergentes no assert = BLOCKED — com `WS_EX` `0x080800A8`, fora do primeiro plano, dentro do retângulo de
   teste, fora do desktop virtual e afinidade `0x11`; (b) **nenhuma** janela `VoltzDrawInput` do PID; (c) **nenhuma**
   janela visível de título `VoltzDrawToolbar`; (d) `GetGUIThreadInfo` em cada thread do PID: nenhuma janela Voltz Win32
   (`VoltzAnnotationOverlay`/`VoltzDrawInput`) ativa, com o foco do teclado ou com o mouse capturado. Qualquer falha:
   `Stop-Process -Id` do PID lançado e o teste falha (BLOCKED).
4. **Sobreposição de teste fora do desktop virtual** (`VOLTZ_TEST_OVERLAY_RECT` à direita de todo o desktop, conferido
   contra o desktop virtual e cada monitor antes de abrir o app); desenho conferido no buffer (`ProbeOverlay`), nunca
   por captura de tela.
5. **A barra é testada sem janela:** o pedido `ToolbarImage` cria uma `DrawToolbar` que nunca é mostrada e a desenha por
   `QWidget::grab()` (fora da tela); a resposta diz se nasceu janela nativa (`janela_nativa`, tem de ser `false`).
6. **A camada Win32 que captura entrada é verificada só pelo usuário**, com o roteiro manual do fim deste arquivo e o
   `tools/voltz-draw-demo.ps1` (abre o Voltz portátil **sem** `VOLTZ_TEST_LIVE`; o usuário é quem liga o modo). O que é
   provado automaticamente e o que fica manual está na tabela abaixo.
7. **Demais regras (iguais ao 4B):** capturas só por `GetSourceScreenshot` de cena sintética (`Assert-NoRealCapture`) ou
   `PrintWindow` da casca lançada; nunca a tela inteira; nunca webcam/captura real (`VOLTZ_TEST_PRESET_STANDINS`); nunca
   quadros de vídeo; nunca `Stop-Process -Name`/`taskkill /IM`; não abrir o Explorer; nunca tocar em `%APPDATA%\Voltz
   Studio`, `%APPDATA%\obs-studio` nem no que já existe em `%USERPROFILE%\Videos`; config portátil com backup e
   restauração em `try/finally` (o smoke confere por hash).
8. **Travar a UI de propósito** (fase C do smoke) só com `VOLTZ_DEBUG_FREEZE_UI=1`, que registra o atalho
   `VoltzStudio.DebugFreezeUi` (8 s); a casca fica parada 8 s, nada é clicado nela, e a conferência é só leitura
   (`IsHungAppWindow`, janelas `Ghost`).
9. **Se algum passo parecer exigir ligar o modo desenho num teste automático, pare e reporte** — não improvise.

| Provado automaticamente | Só pelo roteiro manual do usuário |
|---|---|
| Toda regra da `DrawSession` (gesto, conversão para a captura, cada saída, inatividade, vigia, Ctrl+Z, ordem End→Exit) — doctest | Que a janela Win32 traduz de verdade cada mensagem (Esc, clique direito, Alt+Tab, tecla Windows, clique em outro monitor, Alt+F4) |
| Recusa com `VOLTZ_TEST_LIVE` no app vivo (atalho real do OBS por `TriggerHotkeyByName`), nenhuma janela de entrada/barra nasce | Desenhar com o mouse/caneta/touchpad em cada ferramenta, cor, espessura |
| Retângulo sujo byte a byte igual ao desenho inteiro (doctest) e usado de fato (contadores ao vivo, sobreposição 2560×1440 fora da tela) | Fluidez com o mouse real a 60 Hz e mais (medidas no log ao sair do modo) |
| Estilo da barra salvo no `user.ini` e usado nos traços (pedido de teste) | Clicar na barra sem perder o Esc (buraco na região + sem ativação) |
| Barra desenhada fora da tela sem janela nativa (imagem conferida pelo executor) | Barra no monitor certo, por cima da janela de entrada, fora da captura |
| UI travada 8 s: a casca fica "travada" para o Windows e nenhuma janela `Ghost` cobre a casca ou a sobreposição | Vigia: com o modo ligado e a UI travada, em ~1 s o clique volta a passar no monitor |
| Grep: APIs de foco/captura só no arquivo de exceção; nenhuma injeção no código nem nos scripts | Halo: sem onda ao desenhar, onda de volta depois (Raw Input intacto) |

---

## Convenções

**Diretório de trabalho:** a raiz do repositório (`voltz-studio`), branch `voltz`. Leia o `CLAUDE.md` inteiro antes
(seções **Decisões** e **Armadilhas**, inclusive as de sobreposição e testes). Base deste plano: commit `39bf223cb`.

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
- **SMOKE ANOTAÇÕES:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-annotations.ps1 -OutDir "<scratchpad>\smoke-anotacoes"`
- **SMOKE DESENHO** (Task 8): `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-draw.ps1`
- **FORMATAR:** `"$CLANGFMT" -i <arquivos .cpp/.hpp novos ou alterados em frontend/voltz/ ou plugins/voltz-sources/>`
  (nunca nos arquivos do OBS; nunca em `.effect`/`.ini`/`.ps1`/`.qss`)
- **GREP DE SEGURANÇA** (Tasks 5, 7, 8 e 10; copie como está):
```bash
# A) foco/captura ativa: só no arquivo de exceção
grep -rnE "SetCapture|ReleaseCapture|SetForegroundWindow|SetFocus\(|SetActiveWindow|BringWindowToTop|AttachThreadInput|LockSetForegroundWindow|AllowSetForegroundWindow|grabMouse|grabKeyboard" frontend/voltz plugins/voltz-sources | grep -v "^frontend/voltz/annotate/DrawInputWindow.cpp:"; echo "foco-fora-da-excecao=$?"
# B) injeção/bloqueio de entrada: em lugar nenhum
grep -rnE "SendInput|SetCursorPos|mouse_event|keybd_event|BlockInput|ClipCursor|RegisterHotKey" frontend/voltz plugins/voltz-sources; echo "injecao=$?"
# C) observação passiva já existente: só onde já estava (3A e 4A)
grep -rln "SetWindowsHookEx" frontend/voltz plugins/voltz-sources; grep -rln "RegisterRawInputDevices" frontend/voltz plugins/voltz-sources
# D) no arquivo de exceção, só estas chamadas e nesta quantidade
grep -oE "SetCapture\(|ReleaseCapture\(|SetForegroundWindow\(" frontend/voltz/annotate/DrawInputWindow.cpp | sort | uniq -c
```
  Expected: `foco-fora-da-excecao=1`; `injecao=1`; em C, exatamente `frontend/voltz/project/CursorTracker.cpp` e
  `frontend/voltz/project/LiveMouse.cpp`; em D, `1 ReleaseCapture(`, `1 SetCapture(` e `2 SetForegroundWindow(`.
  (Antes da Task 5 o arquivo de exceção não existe: D sai vazio e o grep D reclama — só A, B e C valem.)

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
>   `size_t`, `%lld` + `static_cast<long long>`, `%lu` para `DWORD`/`GetLastError()` no `blog`, `%u` para `uint32_t`.
> - O alvo `obs-studio` compila com `QT_NO_CONTEXTLESS_CONNECT`: todo `connect` com lambda leva objeto de contexto.
> - Sinais/callbacks do libobs, do websocket e da thread de entrada chegam **fora** da thread da UI → só atômicos/mutex
>   ali; o resto via `QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)`.
> - QSS da casca: prefixo `#voltzShell`; a barra (janela própria) usa o prefixo `#voltzDrawToolbar` no QSS dela. Ícones
>   no código sempre por `phosphorIcon()`/`phosphorToggleIcon()`; ícone novo = SVG **oficial** do Phosphor 2.1.1
>   baixado do unpkg com versão exata.
> - Textos novos nos **dois** locales da casca (`frontend/data/locale/voltz-pt-BR.ini` e `voltz-en-US.ini`, UTF-8 sem
>   BOM), com paridade:
>   `diff <(cut -d= -f1 frontend/data/locale/voltz-en-US.ini | sort) <(cut -d= -f1 frontend/data/locale/voltz-pt-BR.ini | sort)` → sem saída.
>   (O plugin não ganha texto novo neste plano; a paridade dele continua:
>   `diff <(cut -d= -f1 plugins/voltz-sources/data/locale/en-US.ini | sort) <(cut -d= -f1 plugins/voltz-sources/data/locale/pt-BR.ini | sort)`.)
> - Textos de `blog` e literais C em ASCII (sem acento), como no resto do código Voltz.
> - **Não** commitar `build-compile.log` nem `docs/voltz/backlog-plano-*.md` fora da Task 10. **Push só na Task 10**,
>   nunca `--force`.
>
> **Regras de segurança dos testes ao vivo (releia "Segurança dos testes" antes de cada teste):** nunca mexer no
> mouse/teclado do usuário; nunca ligar o modo desenho; traços só por pedidos de teste; sobreposição de teste sempre fora
> do desktop virtual e conferida por `Assert-DrawSafe` depois de cada ação, com o vigia rodando; app só pelo PID lançado
> e fechado por `WM_CLOSE` na casca guardada; cenas só sintéticas; só metadados de vídeo; config portátil com backup;
> `Vídeos` intocada. PowerShell 5.1 lê `.ps1` sem BOM como ANSI: script do scratchpad com acento precisa de **BOM**;
> scripts versionados em `tools/` ficam em ASCII e montam acentos com `[char]0xF3` etc.

**Commits:** mensagem em PT-BR terminando com a linha `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

**Bloco de teste (PowerShell 5.1)** — copie para um `.ps1` **com BOM** no scratchpad, ajuste as variáveis marcadas e
ponha os passos da task no `try`. Ele guarda a config portátil, cria uma limpa (obs-websocket na porta 4455 sem senha,
pt-BR, sem confirmação ao sair, `HideOBSWindowsFromCapture=false`), liga os modos de teste, põe a sobreposição de teste
**fora do desktop virtual** (recusa se cruzar algum monitor), abre o app guardando a janela da casca, liga o **vigia**
(job a cada 200 ms) e, no fim, fecha o app por `WM_CLOSE` na casca guardada (só o PID lançado), confere que nenhuma
janela Voltz ficou e restaura tudo. **Valide dentro do `try`, depois de `Close-App`: o `finally` apaga a config.**
```powershell
$root = "C:\Users\Cassio\Documents\Projetos IA\voltz-studio"
$rundir = Join-Path $root "build_x64\rundir\RelWithDebInfo"; $bin = Join-Path $rundir "bin\64bit"
$sp = "C:\Users\Cassio\AppData\Local\Temp\claude\C--Users-Cassio-Documents-Projetos-IA-voltz-studio\eee63625-8704-4453-920b-073461dee2a9\scratchpad"
$cfg = Join-Path $rundir "config"; $bak = Join-Path $rundir "config.4cbak"; $voltzCfg = Join-Path $cfg "Voltz Studio"
if (Test-Path $bak) { throw "backup anterior em $bak - restaure (Move-Item) antes de continuar" }
if (@(Get-NetTCPConnection -LocalPort 4455 -State Listen -ErrorAction SilentlyContinue).Count -gt 0) { throw "porta 4455 ocupada (outro OBS/Voltz?)" }
# ---------- ajuste por task ----------
$ovW = 640; $ovH = 360       # tamanho do retangulo de teste da sobreposicao (sempre FORA do desktop virtual)
$freezeUi = $false           # VOLTZ_DEBUG_FREEZE_UI: atalho VoltzStudio.DebugFreezeUi (trava a UI por 8 s)
$voltzIni = "NextProjectName=Teste 4C`r`n"   # linhas da secao [Voltz]
# -------------------------------------
$probeSource = @'
using System; using System.Runtime.InteropServices; using System.Collections.Generic; using System.Text;
public static class VoltzProbe {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
 [StructLayout(LayoutKind.Sequential)] public struct GUITHREADINFO { public int cbSize; public int flags; public IntPtr hwndActive; public IntPtr hwndFocus; public IntPtr hwndCapture; public IntPtr hwndMenuOwner; public IntPtr hwndMoveSize; public IntPtr hwndCaret; public RECT rcCaret; }
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")] public static extern IntPtr GetWindowLongPtr(IntPtr h, int index);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool GetWindowDisplayAffinity(IntPtr h, out uint affinity);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
 [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint threadId, ref GUITHREADINFO info);
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder sb, int n);
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder sb, int n);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
 public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
 public static string ClassOf(IntPtr h) { var sb = new StringBuilder(256); GetClassName(h, sb, 256); return sb.ToString(); }
 public static string TitleOf(IntPtr h) { var sb = new StringBuilder(256); GetWindowText(h, sb, 256); return sb.ToString(); }
 public static uint OwnerOf(IntPtr h) { uint owner; GetWindowThreadProcessId(h, out owner); return owner; }
 // Janelas da classe no PID: FindWindowEx (titulo NULO de verdade) UNIAO EnumWindows; divergent = as listas diferem
 public static List<long> OfClass(uint pid, string cls, out bool divergent) {
   var byFind = new HashSet<long>(); IntPtr h = IntPtr.Zero;
   while ((h = FindWindowEx(IntPtr.Zero, h, cls, null)) != IntPtr.Zero) { if (OwnerOf(h) == pid) byFind.Add(h.ToInt64()); }
   var byEnum = new HashSet<long>();
   EnumWindows((w, l) => { if (OwnerOf(w) == pid && ClassOf(w) == cls) byEnum.Add(w.ToInt64()); return true; }, IntPtr.Zero);
   divergent = !byFind.SetEquals(byEnum); byFind.UnionWith(byEnum); return new List<long>(byFind); }
 public static List<long> VisibleTitled(uint pid, string title) { var list = new List<long>();
   EnumWindows((w, l) => { if (OwnerOf(w) == pid && IsWindowVisible(w) && TitleOf(w) == title) list.Add(w.ToInt64()); return true; }, IntPtr.Zero);
   return list; }
 // Janelas "Ghost" (fantasma do Windows, de qualquer processo) com o mesmo retangulo de uma das janelas dadas
 public static int GhostsOver(long[] windows) { var rects = new List<RECT>();
   foreach (long w in windows) { RECT r; if (GetWindowRect(new IntPtr(w), out r)) rects.Add(r); }
   int count = 0;
   EnumWindows((h, l) => { if (ClassOf(h) == "Ghost") { RECT g; if (GetWindowRect(h, out g)) { foreach (RECT r in rects) { if (r.L == g.L && r.T == g.T && r.R == g.R && r.B == g.B) { count++; break; } } } } return true; }, IntPtr.Zero);
   return count; }
 // Alguma thread com uma janela Voltz Win32 ativa, com o foco do teclado ou com o mouse capturado? "" = nenhuma
 public static string VoltzHolder(int[] threads) {
   foreach (int t in threads) { GUITHREADINFO gi = new GUITHREADINFO(); gi.cbSize = Marshal.SizeOf(typeof(GUITHREADINFO));
     if (!GetGUIThreadInfo((uint)t, ref gi)) continue;
     IntPtr[] held = { gi.hwndActive, gi.hwndFocus, gi.hwndCapture }; string[] names = { "ativa", "foco", "captura" };
     for (int i = 0; i < 3; i++) { if (held[i] == IntPtr.Zero) continue; string c = ClassOf(held[i]);
       if (c == "VoltzAnnotationOverlay" || c == "VoltzDrawInput") return names[i] + " (" + c + ", thread " + t + ")"; } }
   return ""; }
}
'@
Add-Type $probeSource
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
$P = [VoltzProbe]; [void]$P::SetProcessDPIAware()
$vx = $P::GetSystemMetrics(76); $vy = $P::GetSystemMetrics(77); $vw = $P::GetSystemMetrics(78); $vh = $P::GetSystemMetrics(79)
function Crosses([int]$l, [int]$t, [int]$r, [int]$b, [int]$L2, [int]$T2, [int]$R2, [int]$B2) { return ($l -lt $R2 -and $r -gt $L2 -and $t -lt $B2 -and $b -gt $T2) }
# Retangulo de teste A DIREITA de todo o desktop virtual (ninguem ve, nada fica por cima de janela alguma)
$ovX = $vx + $vw + 200; $ovY = $vy; $overlayRect = "{0},{1},{2},{3}" -f $ovX, $ovY, $ovW, $ovH
if (Crosses $ovX $ovY ($ovX + $ovW) ($ovY + $ovH) $vx $vy ($vx + $vw) ($vy + $vh)) { throw "retangulo de teste cruza o desktop virtual" }
foreach ($s in [System.Windows.Forms.Screen]::AllScreens) { $b = $s.Bounds; if (Crosses $ovX $ovY ($ovX + $ovW) ($ovY + $ovH) $b.Left $b.Top $b.Right $b.Bottom) { throw "retangulo de teste sobre $($s.DeviceName)" } }
if (Test-Path $cfg) { Move-Item $cfg $bak }
function New-CleanConfig { if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
  $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"; New-Item -ItemType Directory -Force $wsDir | Out-Null
  [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}')
  $ini = "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[BasicWindow]`r`nHideOBSWindowsFromCapture=false`r`n[Voltz]`r`n" + $voltzIni
  [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), $ini, (New-Object Text.UTF8Encoding $false)) }
New-CleanConfig
$env:VOLTZ_TEST_LIVE = "1"; $env:VOLTZ_TEST_PRESET_STANDINS = "1"; $env:VOLTZ_TEST_OVERLAY_RECT = $overlayRect
if ($freezeUi) { $env:VOLTZ_DEBUG_FREEZE_UI = "1" } else { Remove-Item Env:VOLTZ_DEBUG_FREEZE_UI -ErrorAction SilentlyContinue }
Remove-Item Env:VOLTZ_TEST_ANNOT_BURNED, Env:VOLTZ_TEST_SCREEN_LAYERS -ErrorAction SilentlyContinue
$uia = Join-Path $root "tools\voltz-uia.ps1"; $ws = Join-Path $root "tools\voltz-ws.ps1"
function Ws([string]$json, [switch]$Quiet) { $f = Join-Path $sp "req.json"
  [IO.File]::WriteAllText($f, $json, (New-Object Text.UTF8Encoding $false))
  $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $f
  if (-not $Quiet) { $lines | ForEach-Object { Write-Host $_ } }
  return ,@($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json }) }
function Uia([string]$name) { & powershell -ExecutionPolicy Bypass -File $uia -ProcessId $script:proc.Id -Name $name | Out-Host; return ($LASTEXITCODE -eq 0) }
function Vendor([string]$type, [string]$dataJson = '{}', [string]$vendorName = 'voltz-teste') {
  $r = Ws ('[{"requestType":"CallVendorRequest","requestData":{"vendorName":"' + $vendorName + '","requestType":"' + $type + '","requestData":' + $dataJson + '}}]') -Quiet
  if ($r.Count -lt 1 -or -not $r[0].ok) { throw "pedido de teste $vendorName/$type falhou" }
  return $r[0].data.responseData }
function VendorShell([string]$type, [string]$dataJson = '{}') { return (Vendor $type $dataJson 'voltz-teste-casca') }
function Shell-State { Start-Sleep -Milliseconds 400; return ((VendorShell "AnnotationState").estado | ConvertFrom-Json) }
function Feed-State { return ((Vendor "AnnotationFeedState").estado | ConvertFrom-Json) }
function Stroke-Json([string]$tool, [string]$color, [int]$width, [string]$points, [switch]$Open) {
  $req = [ordered]@{ ferr = $tool; cor = $color; esp = $width; pts = $points }; if ($Open) { $req.aberto = $true }
  return (ConvertTo-Json $req -Compress) }
function Kill-Launched([string]$why) { if ($script:proc -and -not $script:proc.HasExited) { Stop-Process -Id $script:proc.Id -Force }; throw "BLOCKED $why - app (PID lancado) encerrado" }
# Seguranca (so leitura). Qualquer falha encerra o PID lancado e aborta.
$script:overlays = @(); $script:asserts = 0
function Assert-DrawSafe([string]$where, [switch]$Quiet) {
  $procId = [uint32]$script:proc.Id; $div = $false
  $list = $P::OfClass($procId, "VoltzAnnotationOverlay", [ref]$div)
  if ($div) { Kill-Launched "[$where] FindWindowEx e EnumWindows divergem nas sobreposicoes" }
  foreach ($id in $list) { $h = [IntPtr][Int64]$id; if ($script:overlays -notcontains $id) { $script:overlays += $id }
    $ex = $P::GetWindowLongPtr($h, -20).ToInt64(); $r = New-Object VoltzProbe+RECT; [void]$P::GetWindowRect($h, [ref]$r)
    [uint32]$aff = 99; [void]$P::GetWindowDisplayAffinity($h, [ref]$aff); $bad = @()
    if (($ex -band 0x080800A8) -ne 0x080800A8) { $bad += ("estilo 0x{0:X}" -f $ex) }
    if ($P::GetForegroundWindow() -eq $h) { $bad += "primeiro plano" }
    if ($r.L -lt $ovX -or $r.T -lt $ovY -or $r.R -gt ($ovX + $ovW) -or $r.B -gt ($ovY + $ovH)) { $bad += "fora do retangulo de teste" }
    if (Crosses $r.L $r.T $r.R $r.B $vx $vy ($vx + $vw) ($vy + $vh)) { $bad += "cruza o desktop virtual" }
    if ($aff -ne 0x11) { $bad += ("afinidade 0x{0:X}" -f $aff) }
    if ($bad.Count -gt 0) { Kill-Launched ("[{0}] sobreposicao insegura hwnd={1}: {2}" -f $where, $id, ($bad -join '; ')) } }
  $divIn = $false; $inputs = $P::OfClass($procId, "VoltzDrawInput", [ref]$divIn)
  if ($inputs.Count -gt 0 -or $divIn) { Kill-Launched "[$where] janela de entrada do modo desenho em teste" }
  if ($P::VisibleTitled($procId, "VoltzDrawToolbar").Count -gt 0) { Kill-Launched "[$where] barra do modo desenho visivel em teste" }
  $holder = $P::VoltzHolder([int[]]@((Get-Process -Id $script:proc.Id).Threads | ForEach-Object { $_.Id }))
  if ($holder) { Kill-Launched "[$where] janela Voltz com $holder" }
  $script:asserts++; if (-not $Quiet) { Write-Host ("assert[{0}] ok: {1} sobreposicao(oes)" -f $where, $list.Count) } }
# Vigia (job em outro processo): a mesma conferencia a cada 200 ms; falha = encerra o PID e escreve BLOCKED
$watchdog = { param([int]$TargetPid, [int]$OvX, [int]$OvY, [int]$OvW, [int]$OvH, [string]$ProbeSource, [string]$StopFile, [string]$LogFile)
  Add-Type $ProbeSource; $W = [VoltzProbe]; [void]$W::SetProcessDPIAware()
  $vx = $W::GetSystemMetrics(76); $vy = $W::GetSystemMetrics(77); $vw = $W::GetSystemMetrics(78); $vh = $W::GetSystemMetrics(79); $rounds = 0
  while ($true) { if (Test-Path -LiteralPath $StopFile) { break }
    $p = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue; if (-not $p -or $p.HasExited) { break }
    $bad = @(); $div = $false
    foreach ($id in $W::OfClass([uint32]$TargetPid, "VoltzAnnotationOverlay", [ref]$div)) { $h = [IntPtr][Int64]$id
      $ex = $W::GetWindowLongPtr($h, -20).ToInt64(); $r = New-Object VoltzProbe+RECT; [void]$W::GetWindowRect($h, [ref]$r)
      [uint32]$aff = 99; [void]$W::GetWindowDisplayAffinity($h, [ref]$aff)
      if (($ex -band 0x080800A8) -ne 0x080800A8) { $bad += ("estilo 0x{0:X}" -f $ex) }
      if ($W::GetForegroundWindow() -eq $h) { $bad += "primeiro plano" }
      if ($r.L -lt $OvX -or $r.T -lt $OvY -or $r.R -gt ($OvX + $OvW) -or $r.B -gt ($OvY + $OvH)) { $bad += "fora do retangulo" }
      if ($r.L -lt ($vx + $vw) -and $r.R -gt $vx -and $r.T -lt ($vy + $vh) -and $r.B -gt $vy) { $bad += "cruza o desktop virtual" }
      if ($aff -ne 0x11) { $bad += ("afinidade 0x{0:X}" -f $aff) } }
    $divIn = $false; if ($W::OfClass([uint32]$TargetPid, "VoltzDrawInput", [ref]$divIn).Count -gt 0) { $bad += "janela de entrada do modo desenho" }
    if ($W::VisibleTitled([uint32]$TargetPid, "VoltzDrawToolbar").Count -gt 0) { $bad += "barra visivel" }
    $holder = $W::VoltzHolder([int[]]@($p.Threads | ForEach-Object { $_.Id })); if ($holder) { $bad += "janela Voltz com $holder" }
    if ($bad.Count -gt 0) { Stop-Process -Id $TargetPid -Force; Add-Content -LiteralPath $LogFile ("BLOCKED vigia: " + ($bad -join '; ')); return }
    $rounds++; Start-Sleep -Milliseconds 200 }
  Add-Content -LiteralPath $LogFile ("vigia: {0} conferencias sem problema" -f $rounds) }
$wdLog = Join-Path $sp "vigia-4c.txt"; $wdStop = Join-Path $sp "vigia-4c.stop"; Remove-Item $wdLog, $wdStop -ErrorAction SilentlyContinue
function Start-App { $script:proc = Start-Process (Join-Path $bin "VoltzStudio.exe") -ArgumentList '--portable','--multi' -WorkingDirectory $bin -PassThru
  $script:job = Start-Job -ScriptBlock $watchdog -ArgumentList $script:proc.Id, $ovX, $ovY, $ovW, $ovH, $probeSource, $wdStop, $wdLog
  Start-Sleep 15; $script:shell = [IntPtr]::Zero
  for ($i = 0; $i -lt 30 -and $script:shell -eq [IntPtr]::Zero; $i++) { $script:proc.Refresh(); $h = $script:proc.MainWindowHandle
    if ($h -ne [IntPtr]::Zero -and $P::OwnerOf($h) -eq $script:proc.Id -and $P::ClassOf($h) -notmatch '^Voltz(AnnotationOverlay|DrawInput)$') { $script:shell = $h } else { Start-Sleep -Milliseconds 500 } }
  $owners = @(Get-NetTCPConnection -LocalPort 4455 -State Listen -ErrorAction SilentlyContinue | ForEach-Object { [int]$_.OwningProcess })
  if (-not ($owners -contains $script:proc.Id)) { Kill-Launched "a porta 4455 nao e do app lancado" }
  Assert-DrawSafe "abertura" }
# Fecha SO o PID lancado: WM_CLOSE na casca guardada (o MainWindowHandle pode virar a sobreposicao, que ignora WM_CLOSE)
function Close-App { if ($script:proc -and -not $script:proc.HasExited) { $procId = $script:proc.Id
    $ok = ($script:shell -ne [IntPtr]::Zero) -and $P::IsWindow($script:shell) -and ($P::OwnerOf($script:shell) -eq $procId)
    if ($ok) { [void]$P::PostMessage($script:shell, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) } else { [void]$script:proc.CloseMainWindow() }
    if (-not $script:proc.WaitForExit(30000)) { Write-Host "FECHAMENTO FORCADO"; Stop-Process -Id $procId -Force } }
  if ($script:job) { New-Item -ItemType File -Force $wdStop | Out-Null; [void](Wait-Job $script:job -Timeout 20); Remove-Job $script:job -Force; $script:job = $null
    Write-Host ("vigia: " + ((Get-Content -LiteralPath $wdLog -ErrorAction SilentlyContinue) -join ' ')) }
  $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
  if ($log) { Copy-Item $log.FullName "$sp\ultimo-log.txt" -Force } }
function LogLines([string]$pattern) { if (Test-Path "$sp\ultimo-log.txt") { Select-String -LiteralPath "$sp\ultimo-log.txt" -Encoding UTF8 -SimpleMatch -Pattern $pattern | ForEach-Object { $_.Line } } }
function Assert-NoRealCapture { $inputs = @((Ws '[{"requestType":"GetInputList"}]' -Quiet)[0].data.inputs)
  $real = @($inputs | Where-Object { $_.inputKind -match 'monitor_capture|window_capture|game_capture|dshow_input' })
  if ($real.Count -gt 0) { throw ("captura real presente - abortando: " + (($real | ForEach-Object { $_.inputName }) -join ', ')) } }
function Scene-Image([string]$scene) { Assert-NoRealCapture
  $r = Ws ('[{"requestType":"GetSourceScreenshot","requestData":{"sourceName":' + (ConvertTo-Json $scene) + ',"imageFormat":"png","imageWidth":640,"imageHeight":360}}]') -Quiet
  if ($r.Count -lt 1 -or -not $r[0].ok) { throw "GetSourceScreenshot falhou para '$scene'" }
  $bytes = [Convert]::FromBase64String(($r[0].data.imageData -replace '^data:image/png;base64,', ''))
  return [System.Drawing.Bitmap]::FromStream((New-Object IO.MemoryStream (,$bytes))) }
function Near($c, [int]$r, [int]$g, [int]$b, [int]$tol = 24) { ([Math]::Abs($c.R - $r) -le $tol) -and ([Math]::Abs($c.G - $g) -le $tol) -and ([Math]::Abs($c.B - $b) -le $tol) }
$proc = $null; $script:job = $null
try {
  Start-App
  # Preset com substitutas: Uia "Só tela / Só câmera — Aplicar"; Start-Sleep 3; Assert-NoRealCapture; e
  # [void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Só tela"}}]')
  # ... passos da task (Assert-DrawSafe depois de cada acao; validacao no fim: Close-App; LogLines ...) ...
} finally {
  Close-App
  $left = @($script:overlays | Where-Object { $P::IsWindow([IntPtr][Int64]$_) })
  if ($left.Count -gt 0) { Write-Host "FALHOU janela Voltz orfa: $($left -join ',')" } else { Write-Host "ok: nenhuma sobreposicao ficou ($($script:overlays.Count) vista(s), $($script:asserts) asserts)" }
  Remove-Item Env:VOLTZ_TEST_LIVE, Env:VOLTZ_TEST_PRESET_STANDINS, Env:VOLTZ_TEST_OVERLAY_RECT, Env:VOLTZ_DEBUG_FREEZE_UI -ErrorAction SilentlyContinue
  Start-Sleep 3
  if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
  if (Test-Path $bak) { Move-Item $bak $cfg }
}
```

**Aplicar o preset "Só tela / Só câmera"** (primeira abertura com config limpa = página Presets): `Uia "Só tela / Só
câmera — Aplicar"`; depois `Start-Sleep 3`, `Assert-NoRealCapture` e
`[void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Só tela"}}]')`.

**Cores e pixels de referência (iguais ao 4B):** Tela substituta = `#2A2A30` → (42,42,48); lima `#D4F53C` →
(212,245,60). A Tela 2560×1440 cabe no canvas 1920×1080 com escala 0,75 e a `Scene-Image` tem 640×360 ⇒ pixel (x,y) da
captura ≈ (x/4, y/4) na imagem. Sobreposição de teste 640×360 = escala 0,25; 2560×1440 = escala 1. Lima opaco no buffer =
`#FFD4F53C`; buffer limpo = `#00000000`.

---

## Mapa de arquivos

**Novos — núcleo (`frontend/voltz/core/`, sem Qt/libobs) e testes**
| Arquivo | Responsabilidade |
|---|---|
| `core/draw-session.hpp/.cpp` | `DrawExit`, `DrawRefusal`, `checkDrawEntry`, `DrawInput`, `DrawCommand`, `DrawSession` (gesto + saídas garantidas, relógio injetado) |
| `tests/test-draw-session.cpp` | doctest da sessão (11 casos) |
| `tests/test-annotation-dirty.cpp` | doctest do retângulo sujo (6 casos) |

**Novos — frontend (alvo `obs-studio`) e ferramentas**
| Arquivo | Responsabilidade |
|---|---|
| `annotate/DrawInputWindow.hpp/.cpp` | thread de entrada + janela `VoltzDrawInput` (**único arquivo com captura/foco**), `DrawShared`, `takeDrawCommands` |
| `annotate/DrawToolbar.hpp/.cpp` | barra do modo desenho (Qt, sem foco, fora da captura, buraco na janela de entrada) |
| `annotate/DrawModeController.hpp/.cpp` | liga/desliga (atalho/botão), recusas, pulso da UI, barra, atalhos Draw/Undo/Clear/DebugFreezeUi, drena os comandos |
| `shell/HotkeyBinding.hpp/.cpp` | `loadHotkeyBinding` (perfil `[Hotkeys]`, padrão Ctrl+Shift+tecla ou nenhum) e `hotkeyShortcutText` |
| `shell/icons/regular/{pen,highlighter-circle,line-segment,arrow-up-right,rectangle,circle,arrow-counter-clockwise,palette,timer}.svg`, `shell/icons/fill/{pen,highlighter-circle,line-segment,arrow-up-right,rectangle,circle,pencil-simple}-fill.svg` | ícones Phosphor 2.1.1 |
| `tools/voltz-smoke-draw.ps1` | smoke do 4C (recusa, estilo, barra fora da tela, redesenho parcial com medidas, UI travada sem fantasma) |
| `tools/voltz-draw-demo.ps1` | demonstração MANUAL do modo desenho (o usuário liga e desenha; sem `VOLTZ_TEST_LIVE`) |

**Modificados (Voltz):** `frontend/voltz/core/annotation-raster.hpp/.cpp`, `frontend/voltz/core/live-protocol.hpp`,
`frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`, `plugins/voltz-sources/annotation-feed.hpp/.cpp`,
`plugins/voltz-sources/annotation-source.cpp`, `frontend/voltz/annotate/OverlayWindow.hpp/.cpp`,
`frontend/voltz/annotate/AnnotationController.hpp/.cpp`, `frontend/voltz/annotate/AnnotationTestVendor.hpp/.cpp`,
`frontend/voltz/project/MonitorLocator.hpp/.cpp`, `frontend/voltz/project/LiveMouse.hpp/.cpp`,
`frontend/voltz/shell/RecordPage.hpp/.cpp`, `frontend/voltz/shell/VoltzShell.hpp/.cpp`,
`frontend/voltz/shell/voltz-shell.qrc`, `frontend/voltz/shell/voltz-shell.qss`, `frontend/cmake/feature-voltz.cmake`,
`frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`, `tools/voltz-annot-demo.ps1`; na Task
10: `docs/voltz/backlog-plano-4.md`, `CLAUDE.md`, `AGENTS.md`, `docs/HISTORICO.md`.

**Modificados (OBS upstream):** nenhum. (`plugins/CMakeLists.txt` e `docs/voltz/upstream-patches.md` não mudam; a Task
10 confere.)

**Contagem de testes:** 153 (base) → 164 (Task 1) → 170 (Task 2).

---
### Task 1: Núcleo — sessão do modo desenho (`draw-session`) com todas as saídas garantidas

**Files:**
- Create: `frontend/voltz/core/draw-session.hpp`, `frontend/voltz/core/draw-session.cpp`
- Create: `frontend/voltz/tests/test-draw-session.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

- [ ] **Step 1: Escrever o teste que falha**

Criar `frontend/voltz/tests/test-draw-session.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/draw-session.hpp>

#include <cstdint>
#include <string>
#include <vector>

using voltz::DrawCommand;
using voltz::DrawCommandType;
using voltz::DrawExit;
using voltz::DrawInput;
using voltz::DrawInputKind;
using voltz::DrawRefusal;
using voltz::DrawSession;
using voltz::StrokePoint;

namespace {

constexpr int64_t kMs = 1000000;
constexpr int64_t kStart = 1000 * kMs;
// Monitor 1000×500 em (100,50) capturado em 2000×1000: cada pixel do monitor vale 2 da captura.
const voltz::CaptureMapping kMapping{voltz::MonitorRect{100, 50, 1000, 500}, 2000, 1000};

DrawInput at(DrawInputKind kind, int64_t tNs, int x = 0, int y = 0)
{
	DrawInput input;
	input.kind = kind;
	input.tNs = tNs;
	input.x = x;
	input.y = y;
	input.uiHeartbeatNs = tNs;
	return input;
}

DrawInput tick(int64_t tNs, int64_t heartbeatNs)
{
	DrawInput input;
	input.kind = DrawInputKind::Tick;
	input.tNs = tNs;
	input.uiHeartbeatNs = heartbeatNs;
	return input;
}

// "begin(1000,500) extend(1020,510) end exit:esc"
std::string describe(const std::vector<DrawCommand> &commands)
{
	std::string text;
	for (const DrawCommand &command : commands) {
		if (!text.empty()) {
			text += ' ';
		}
		switch (command.type) {
		case DrawCommandType::Begin:
			text += "begin";
			break;
		case DrawCommandType::Extend:
			text += "extend";
			break;
		case DrawCommandType::End:
			text += "end";
			break;
		case DrawCommandType::Undo:
			text += "undo";
			break;
		case DrawCommandType::Exit:
			text += std::string("exit:") + voltz::drawExitName(command.exit);
			break;
		}
		for (const StrokePoint &point : command.points) {
			text += "(" + std::to_string(point.x) + "," + std::to_string(point.y) + ")";
		}
	}
	return text;
}

} // namespace

TEST_CASE("checkDrawEntry: saindo vence teste, que vence sem alvo; nomes das recusas")
{
	CHECK(voltz::checkDrawEntry(false, false, true) == DrawRefusal::None);
	CHECK(voltz::checkDrawEntry(true, true, false) == DrawRefusal::ShuttingDown);
	CHECK(voltz::checkDrawEntry(false, true, true) == DrawRefusal::TestMode);
	CHECK(voltz::checkDrawEntry(false, true, false) == DrawRefusal::TestMode);
	CHECK(voltz::checkDrawEntry(false, false, false) == DrawRefusal::NoTarget);
	CHECK(std::string(voltz::drawRefusalName(DrawRefusal::None)).empty());
	CHECK(std::string(voltz::drawRefusalName(DrawRefusal::ShuttingDown)) == "saindo");
	CHECK(std::string(voltz::drawRefusalName(DrawRefusal::TestMode)) == "teste");
	CHECK(std::string(voltz::drawRefusalName(DrawRefusal::NoTarget)) == "sem_alvo");
}

TEST_CASE("drawExitName: um nome para cada saída")
{
	const DrawExit exits[] = {DrawExit::Escape,    DrawExit::RightClick, DrawExit::Hotkey, DrawExit::Button,
				  DrawExit::FocusLost, DrawExit::Idle,       DrawExit::Watchdog, DrawExit::Target,
				  DrawExit::Close,     DrawExit::Shutdown};
	const char *names[] = {"esc",         "clique_direito", "atalho", "botao", "foco",
			       "inatividade", "vigia",          "alvo",   "fechar", "saida"};
	for (int i = 0; i < 10; i++) {
		CHECK(std::string(voltz::drawExitName(exits[i])) == names[i]);
	}
	CHECK(std::string(voltz::drawExitName(DrawExit::None)).empty());
}

TEST_CASE("DrawSession: arrastar com o botão esquerdo vira Begin, Extend e End em pixels da captura")
{
	DrawSession session(kMapping, kStart);
	CHECK(describe(session.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300))) == "begin(1000,500)");
	CHECK(session.drawing());
	CHECK(describe(session.handle(at(DrawInputKind::Move, kStart + 2 * kMs, 610, 305))) == "extend(1020,510)");
	CHECK(describe(session.handle(at(DrawInputKind::LeftUp, kStart + 3 * kMs, 620, 310))) ==
	      "extend(1040,520) end");
	CHECK_FALSE(session.drawing());
	CHECK(session.active());
	CHECK(session.exitReason() == DrawExit::None);
}

TEST_CASE("DrawSession: mover sem botão não desenha; ponto repetido não repete; arrastar para fora fica na borda")
{
	DrawSession session(kMapping, kStart);
	CHECK(session.handle(at(DrawInputKind::Move, kStart + 1 * kMs, 600, 300)).empty());
	CHECK(describe(session.handle(at(DrawInputKind::LeftDown, kStart + 2 * kMs, 600, 300))) == "begin(1000,500)");
	CHECK(session.handle(at(DrawInputKind::Move, kStart + 3 * kMs, 600, 300)).empty());
	CHECK(describe(session.handle(at(DrawInputKind::Move, kStart + 4 * kMs, 5000, -40))) == "extend(1998,0)");
	CHECK(describe(session.handle(at(DrawInputKind::LeftUp, kStart + 5 * kMs, 5000, -40))) == "end");
}

TEST_CASE("DrawSession: apertar fora do monitor não começa traço")
{
	DrawSession session(kMapping, kStart);
	CHECK(session.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 50, 300)).empty());
	CHECK(session.handle(at(DrawInputKind::LeftDown, kStart + 2 * kMs, 1100, 300)).empty());
	CHECK_FALSE(session.drawing());
	CHECK(session.handle(at(DrawInputKind::Move, kStart + 3 * kMs, 600, 300)).empty());
	CHECK(session.handle(at(DrawInputKind::LeftUp, kStart + 4 * kMs, 600, 300)).empty());
}

TEST_CASE("DrawSession: Esc termina o traço e sai; depois disso tudo é ignorado")
{
	DrawSession session(kMapping, kStart);
	session.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300));
	CHECK(describe(session.handle(at(DrawInputKind::Escape, kStart + 2 * kMs))) == "end exit:esc");
	CHECK_FALSE(session.active());
	CHECK_FALSE(session.drawing());
	CHECK(session.exitReason() == DrawExit::Escape);
	CHECK(session.handle(at(DrawInputKind::LeftDown, kStart + 3 * kMs, 600, 300)).empty());
	CHECK(session.handle(tick(kStart + 60000 * kMs, 0)).empty());
	CHECK(session.requestExit(DrawExit::Hotkey).empty());
	CHECK(session.exitReason() == DrawExit::Escape);
}

TEST_CASE("DrawSession: clique direito, perda de foco, captura perdida, Alt+F4 e troca de monitores saem")
{
	const DrawInputKind kinds[] = {DrawInputKind::RightDown, DrawInputKind::Deactivated, DrawInputKind::CaptureLost,
				       DrawInputKind::Close, DrawInputKind::TargetChanged};
	const char *expected[] = {"exit:clique_direito", "exit:foco", "exit:foco", "exit:fechar", "exit:alvo"};
	for (int i = 0; i < 5; i++) {
		DrawSession idle(kMapping, kStart);
		CHECK(describe(idle.handle(at(kinds[i], kStart + 1 * kMs, 600, 300))) == expected[i]);
		CHECK_FALSE(idle.active());
		DrawSession drawing(kMapping, kStart);
		drawing.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300));
		CHECK(describe(drawing.handle(at(kinds[i], kStart + 2 * kMs, 600, 300))) ==
		      std::string("end ") + expected[i]);
	}
}

TEST_CASE("DrawSession: 30 s sem entrada saem; qualquer entrada reinicia a conta")
{
	DrawSession fresh(kMapping, kStart);
	CHECK(fresh.handle(tick(kStart + 29999 * kMs, kStart + 29999 * kMs)).empty());
	CHECK(describe(fresh.handle(tick(kStart + 30000 * kMs, kStart + 30000 * kMs))) == "exit:inatividade");

	DrawSession busy(kMapping, kStart);
	busy.handle(at(DrawInputKind::OtherKey, kStart + 10000 * kMs));
	busy.handle(at(DrawInputKind::Activated, kStart + 20000 * kMs));
	CHECK(busy.handle(tick(kStart + 49999 * kMs, kStart + 49999 * kMs)).empty());
	busy.handle(at(DrawInputKind::Move, kStart + 49999 * kMs, 600, 300));
	CHECK(busy.handle(tick(kStart + 79998 * kMs, kStart + 79998 * kMs)).empty());
	CHECK(describe(busy.handle(tick(kStart + 79999 * kMs, kStart + 79999 * kMs))) == "exit:inatividade");
}

TEST_CASE("DrawSession: vigia sai com a UI 1 s sem pulsar, mesmo com entrada recente, e com pulso nunca dado")
{
	DrawSession session(kMapping, kStart);
	session.handle(at(DrawInputKind::Move, kStart + 5000 * kMs, 600, 300));
	CHECK(session.handle(tick(kStart + 5000 * kMs, kStart + 4001 * kMs)).empty());
	CHECK(describe(session.handle(tick(kStart + 5000 * kMs, kStart + 4000 * kMs))) == "exit:vigia");

	DrawSession never(kMapping, kStart);
	CHECK(describe(never.handle(tick(kStart + 100 * kMs, 0))) == "exit:vigia");

	DrawSession both(kMapping, kStart);
	both.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300));
	CHECK(describe(both.handle(tick(kStart + 40000 * kMs, kStart))) == "end exit:vigia");
}

TEST_CASE("DrawSession: saída pedida de fora termina o traço; pedir de novo não repete")
{
	DrawSession session(kMapping, kStart);
	session.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300));
	CHECK(describe(session.requestExit(DrawExit::Hotkey)) == "end exit:atalho");
	CHECK(session.requestExit(DrawExit::Shutdown).empty());
	CHECK(session.exitReason() == DrawExit::Hotkey);

	DrawSession other(kMapping, kStart);
	CHECK(other.requestExit(DrawExit::None).empty());
	CHECK(other.active());
	CHECK(describe(other.requestExit(DrawExit::Button)) == "exit:botao");
}

TEST_CASE("DrawSession: Ctrl+Z desfaz (termina antes o traço em andamento, que não continua)")
{
	DrawSession session(kMapping, kStart);
	session.handle(at(DrawInputKind::LeftDown, kStart + 1 * kMs, 600, 300));
	CHECK(describe(session.handle(at(DrawInputKind::Undo, kStart + 2 * kMs))) == "end undo");
	CHECK(session.handle(at(DrawInputKind::Move, kStart + 3 * kMs, 700, 300)).empty());
	CHECK(session.handle(at(DrawInputKind::LeftUp, kStart + 4 * kMs, 700, 300)).empty());
	CHECK(describe(session.handle(at(DrawInputKind::Undo, kStart + 5 * kMs))) == "undo");
	CHECK(session.active());
}
```

- [ ] **Step 2: Ligar ao CMake e ver falhar**

Em `frontend/voltz/tests/CMakeLists.txt`, depois de `    test-disk-space.cpp`, acrescentar `    test-draw-session.cpp`.
Em `frontend/voltz/core/CMakeLists.txt`: em `PRIVATE`, depois de `    disk-space.cpp`, acrescentar `    draw-session.cpp`;
em `PUBLIC`, depois de `    disk-space.hpp`, acrescentar `    draw-session.hpp`.
Run: CONFIGURAR; COMPILAR TESTES.
Expected: FALHA (`draw-session.hpp`/`draw-session.cpp` não existem).

- [ ] **Step 3: Cabeçalho**

Criar `frontend/voltz/core/draw-session.hpp`:
```cpp
#pragma once

#include "annotation-model.hpp"
#include "cursor-track.hpp"

#include <cstdint>
#include <vector>

namespace voltz {

// Por que o modo desenho acabou (log "[voltz] modo desenho: desligado (<nome>)" e estado de teste).
enum class DrawExit { None, Escape, RightClick, Hotkey, Button, FocusLost, Idle, Watchdog, Target, Close, Shutdown };

// "esc" | "clique_direito" | "atalho" | "botao" | "foco" | "inatividade" | "vigia" | "alvo" | "fechar" | "saida";
// "" para None.
const char *drawExitName(DrawExit exit);

// Por que o modo desenho não ligou.
enum class DrawRefusal { None, ShuttingDown, TestMode, NoTarget };

// "saindo" | "teste" | "sem_alvo"; "" para None.
const char *drawRefusalName(DrawRefusal refusal);

// O modo desenho só liga por ação do usuário e nunca: com o app saindo, com VOLTZ_TEST_LIVE (teste automático não
// consegue ligá-lo) ou sem monitor capturado onde desenhar — nessa ordem.
DrawRefusal checkDrawEntry(bool shuttingDown, bool testMode, bool hasTarget);

constexpr int64_t kDrawIdleNs = 30LL * 1000000000LL; // 30 s sem entrada do usuário
constexpr int64_t kDrawHeartbeatNs = 1000000000LL;   // thread da UI sem pulsar por 1 s

enum class DrawInputKind {
	Move,          // o mouse moveu (x, y)
	LeftDown,      // botão esquerdo desceu (x, y)
	LeftUp,        // botão esquerdo subiu (x, y)
	RightDown,     // botão direito desceu
	Escape,        // Esc
	Undo,          // Ctrl+Z
	OtherKey,      // outra tecla (só conta como atividade)
	Activated,     // a janela de entrada ficou ativa
	Deactivated,   // perdeu a ativação (Alt+Tab, tecla Windows, clique em outra janela ou monitor)
	CaptureLost,   // alguém tirou a captura do mouse (WM_CANCELMODE, WM_CAPTURECHANGED)
	Close,         // WM_CLOSE (Alt+F4)
	TargetChanged, // os monitores mudaram (WM_DISPLAYCHANGE)
	Tick,          // tique da thread de entrada (~100 ms), com o último pulso da UI
};

struct DrawInput {
	DrawInputKind kind = DrawInputKind::Tick;
	int64_t tNs = 0;           // relógio do libobs (os_gettime_ns)
	int x = 0;                 // pixels físicos do desktop (eventos de mouse)
	int y = 0;
	int64_t uiHeartbeatNs = 0; // Tick: último pulso da thread da UI (0 = nunca pulsou)
};

enum class DrawCommandType { Begin, Extend, End, Undo, Exit };

struct DrawCommand {
	DrawCommandType type = DrawCommandType::End;
	std::vector<StrokePoint> points; // Begin: 1 ponto; Extend: os pontos novos (pixels da captura)
	DrawExit exit = DrawExit::None;  // Exit: o motivo
};

// Uma sessão do modo desenho, na thread de entrada: traduz a entrada em comandos de traço e decide TODAS as saídas
// garantidas (Esc, clique direito, perda de ativação/captura, Alt+F4, troca de monitores, 30 s parado, vigia da UI).
// Sem Qt nem Win32; o tempo vem em cada entrada (relógio injetável nos testes). Toda saída termina antes o traço em
// andamento; depois de sair, ignora tudo.
class DrawSession {
public:
	DrawSession(const CaptureMapping &mapping, int64_t startNs, int64_t idleNs = kDrawIdleNs,
		    int64_t heartbeatNs = kDrawHeartbeatNs);

	std::vector<DrawCommand> handle(const DrawInput &input);
	// Saída pedida de fora (o mesmo atalho, botão Sair da barra, saída do app). Vazio se já saiu ou `reason` é None.
	std::vector<DrawCommand> requestExit(DrawExit reason);

	bool active() const { return exit_ == DrawExit::None; }
	bool drawing() const { return drawing_; }
	DrawExit exitReason() const { return exit_; }

private:
	std::vector<DrawCommand> finish(DrawExit reason);
	// Ponto do desktop preso ao monitor e convertido para a captura (arrastar para fora fica na borda).
	StrokePoint clampedPoint(int x, int y) const;

	CaptureMapping mapping_;
	int64_t lastInputNs_ = 0;
	int64_t idleNs_ = kDrawIdleNs;
	int64_t heartbeatNs_ = kDrawHeartbeatNs;
	bool drawing_ = false;
	StrokePoint last_;
	DrawExit exit_ = DrawExit::None;
};

} // namespace voltz
```

- [ ] **Step 4: Implementação**

Criar `frontend/voltz/core/draw-session.cpp`:
```cpp
#include "draw-session.hpp"

#include <algorithm>
#include <utility>

namespace voltz {

namespace {

DrawCommand command(DrawCommandType type, std::vector<StrokePoint> points = {}, DrawExit exit = DrawExit::None)
{
	DrawCommand out;
	out.type = type;
	out.points = std::move(points);
	out.exit = exit;
	return out;
}

} // namespace

const char *drawExitName(DrawExit exit)
{
	switch (exit) {
	case DrawExit::Escape:
		return "esc";
	case DrawExit::RightClick:
		return "clique_direito";
	case DrawExit::Hotkey:
		return "atalho";
	case DrawExit::Button:
		return "botao";
	case DrawExit::FocusLost:
		return "foco";
	case DrawExit::Idle:
		return "inatividade";
	case DrawExit::Watchdog:
		return "vigia";
	case DrawExit::Target:
		return "alvo";
	case DrawExit::Close:
		return "fechar";
	case DrawExit::Shutdown:
		return "saida";
	case DrawExit::None:
		break;
	}
	return "";
}

const char *drawRefusalName(DrawRefusal refusal)
{
	switch (refusal) {
	case DrawRefusal::ShuttingDown:
		return "saindo";
	case DrawRefusal::TestMode:
		return "teste";
	case DrawRefusal::NoTarget:
		return "sem_alvo";
	case DrawRefusal::None:
		break;
	}
	return "";
}

DrawRefusal checkDrawEntry(bool shuttingDown, bool testMode, bool hasTarget)
{
	if (shuttingDown) {
		return DrawRefusal::ShuttingDown;
	}
	if (testMode) {
		return DrawRefusal::TestMode;
	}
	if (!hasTarget) {
		return DrawRefusal::NoTarget;
	}
	return DrawRefusal::None;
}

DrawSession::DrawSession(const CaptureMapping &mapping, int64_t startNs, int64_t idleNs, int64_t heartbeatNs)
	: mapping_(mapping),
	  lastInputNs_(startNs),
	  idleNs_(idleNs),
	  heartbeatNs_(heartbeatNs)
{
}

StrokePoint DrawSession::clampedPoint(int x, int y) const
{
	const MonitorRect &rect = mapping_.monitor;
	if (rect.width <= 0 || rect.height <= 0) {
		return StrokePoint{};
	}
	const int64_t dx = std::clamp<int64_t>(static_cast<int64_t>(x) - rect.left, 0, rect.width - 1);
	const int64_t dy = std::clamp<int64_t>(static_cast<int64_t>(y) - rect.top, 0, rect.height - 1);
	const CapturePoint point =
		mapToCapture(mapping_, static_cast<int>(rect.left + dx), static_cast<int>(rect.top + dy));
	return StrokePoint{point.x, point.y};
}

std::vector<DrawCommand> DrawSession::finish(DrawExit reason)
{
	std::vector<DrawCommand> out;
	if (drawing_) {
		out.push_back(command(DrawCommandType::End));
		drawing_ = false;
	}
	exit_ = reason;
	out.push_back(command(DrawCommandType::Exit, {}, reason));
	return out;
}

std::vector<DrawCommand> DrawSession::requestExit(DrawExit reason)
{
	if (!active() || reason == DrawExit::None) {
		return {};
	}
	return finish(reason);
}

std::vector<DrawCommand> DrawSession::handle(const DrawInput &input)
{
	if (!active()) {
		return {};
	}
	switch (input.kind) {
	case DrawInputKind::Tick:
		/* o vigia vem antes da inatividade: com a UI parada, sair é o que destrava o usuário */
		if (input.tNs - input.uiHeartbeatNs >= heartbeatNs_) {
			return finish(DrawExit::Watchdog);
		}
		if (input.tNs - lastInputNs_ >= idleNs_) {
			return finish(DrawExit::Idle);
		}
		return {};
	case DrawInputKind::Escape:
		return finish(DrawExit::Escape);
	case DrawInputKind::RightDown:
		return finish(DrawExit::RightClick);
	case DrawInputKind::Deactivated:
	case DrawInputKind::CaptureLost:
		return finish(DrawExit::FocusLost);
	case DrawInputKind::Close:
		return finish(DrawExit::Close);
	case DrawInputKind::TargetChanged:
		return finish(DrawExit::Target);
	default:
		break;
	}

	lastInputNs_ = std::max(lastInputNs_, input.tNs);
	std::vector<DrawCommand> out;
	switch (input.kind) {
	case DrawInputKind::LeftDown: {
		if (drawing_) {
			break;
		}
		const CapturePoint point = mapToCapture(mapping_, input.x, input.y);
		if (!point.inside) {
			break;
		}
		drawing_ = true;
		last_ = StrokePoint{point.x, point.y};
		out.push_back(command(DrawCommandType::Begin, {last_}));
		break;
	}
	case DrawInputKind::Move:
		if (drawing_) {
			const StrokePoint point = clampedPoint(input.x, input.y);
			if (!(point == last_)) {
				last_ = point;
				out.push_back(command(DrawCommandType::Extend, {point}));
			}
		}
		break;
	case DrawInputKind::LeftUp:
		if (drawing_) {
			const StrokePoint point = clampedPoint(input.x, input.y);
			if (!(point == last_)) {
				last_ = point;
				out.push_back(command(DrawCommandType::Extend, {point}));
			}
			out.push_back(command(DrawCommandType::End));
			drawing_ = false;
		}
		break;
	case DrawInputKind::Undo:
		/* desfazer tira o traço em andamento: o botão segue apertado, mas nada mais vai para ele */
		if (drawing_) {
			out.push_back(command(DrawCommandType::End));
			drawing_ = false;
		}
		out.push_back(command(DrawCommandType::Undo));
		break;
	default:
		break; /* Activated, OtherKey: só atividade */
	}
	return out;
}

} // namespace voltz
```

- [ ] **Step 5: Rodar e ver passar**

```bash
"$CLANGFMT" -i frontend/voltz/core/draw-session.hpp frontend/voltz/core/draw-session.cpp frontend/voltz/tests/test-draw-session.cpp
```
Run: COMPILAR TESTES; RODAR TESTES; `build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe | tail -3`.
Expected: `100% tests passed`; `test cases: 164 | 164 passed`.

- [ ] **Step 6: Commit**

```bash
git add frontend/voltz/core/draw-session.hpp frontend/voltz/core/draw-session.cpp frontend/voltz/tests/test-draw-session.cpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): sessao do modo desenho no nucleo (gesto, saidas garantidas, inatividade, vigia, recusa em teste)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 2: Núcleo — retângulo sujo (`strokeBounds`, `redrawRegion`, `appendDirtyRect`, `DirtyHistory`)

**Files:**
- Modify: `frontend/voltz/core/annotation-raster.hpp`, `frontend/voltz/core/annotation-raster.cpp`
- Create: `frontend/voltz/tests/test-annotation-dirty.cpp`
- Modify: `frontend/voltz/tests/CMakeLists.txt`

- [ ] **Step 1: Escrever o teste que falha**

Criar `frontend/voltz/tests/test-annotation-dirty.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/annotation-raster.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

using voltz::AnnotationTool;
using voltz::PixelRect;
using voltz::RgbColor;
using voltz::Stroke;
using voltz::StrokePoint;

namespace {

const RgbColor kLime{0xD4, 0xF5, 0x3C};
const RgbColor kRed{0xFF, 0x4D, 0x4D};
const RgbColor kBlue{0x4D, 0xA3, 0xFF};

struct Image {
	Image(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0) {}
	voltz::RasterImage view() { return voltz::RasterImage{pixels.data(), width, height, width * 4}; }
	int width;
	int height;
	std::vector<uint8_t> pixels;
};

Stroke makeStroke(int id, AnnotationTool tool, RgbColor color, int width, const std::vector<StrokePoint> &points)
{
	Stroke stroke;
	stroke.id = id;
	stroke.tool = tool;
	stroke.color = color;
	stroke.width = width;
	voltz::appendStrokePoints(stroke, points);
	return stroke;
}

void redrawAll(Image &image, const std::vector<const Stroke *> &strokes, double scaleX, double scaleY)
{
	voltz::clearRaster(image.view());
	for (const Stroke *stroke : strokes) {
		voltz::rasterizeStroke(image.view(), *stroke, scaleX, scaleY);
	}
}

bool inside(const PixelRect &rect, int x, int y)
{
	return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

} // namespace

TEST_CASE("uniteRects/intersectRects: vazios não contam; sobreposição")
{
	CHECK(PixelRect{}.empty());
	CHECK_FALSE((PixelRect{0, 0, 1, 1}).empty());
	CHECK(voltz::uniteRects(PixelRect{0, 0, 10, 10}, PixelRect{5, -2, 20, 4}) == PixelRect{0, -2, 20, 10});
	CHECK(voltz::uniteRects(PixelRect{}, PixelRect{1, 2, 3, 4}) == PixelRect{1, 2, 3, 4});
	CHECK(voltz::uniteRects(PixelRect{1, 2, 3, 4}, PixelRect{5, 5, 5, 9}) == PixelRect{1, 2, 3, 4});
	CHECK(voltz::uniteRects(PixelRect{}, PixelRect{}).empty());
	CHECK(voltz::intersectRects(PixelRect{0, 0, 10, 10}, PixelRect{5, 5, 20, 20}) == PixelRect{5, 5, 10, 10});
	CHECK(voltz::intersectRects(PixelRect{0, 0, 10, 10}, PixelRect{10, 0, 20, 10}).empty());
	CHECK(voltz::intersectRects(PixelRect{0, 0, 10, 10}, PixelRect{10, 0, 20, 10}) == PixelRect{});
}

TEST_CASE("strokeBounds: cobre todo pixel desenhado (caneta, marca-texto, seta, elipse; escala 1 e 0,25)")
{
	const std::vector<Stroke> strokes = {
		makeStroke(1, AnnotationTool::Pen, kLime, 6, {{20, 20}, {80, 60}, {140, 30}}),
		makeStroke(2, AnnotationTool::Highlighter, kRed, 8, {{30, 200}, {250, 210}}),
		makeStroke(3, AnnotationTool::Arrow, kBlue, 5, {{50, 250}, {350, 120}}),
		makeStroke(4, AnnotationTool::Ellipse, kLime, 4, {{200, 40}, {380, 140}}),
	};
	for (double scale : {1.0, 0.25}) {
		for (const Stroke &stroke : strokes) {
			Image image(400, 300);
			voltz::rasterizeStroke(image.view(), stroke, scale, scale);
			const PixelRect bounds = voltz::strokeBounds(stroke, scale, scale);
			int drawn = 0;
			int outside = 0;
			for (int y = 0; y < image.height; y++) {
				for (int x = 0; x < image.width; x++) {
					const size_t alpha = (static_cast<size_t>(y) * static_cast<size_t>(image.width) +
							      static_cast<size_t>(x)) *
								     4 +
							     3;
					if (image.pixels[alpha] != 0) {
						drawn++;
						if (!inside(bounds, x, y)) {
							outside++;
						}
					}
				}
			}
			CHECK(drawn > 0);
			CHECK(outside == 0);
		}
	}
}

TEST_CASE("rasterizeStrokeClipped: fora do recorte intacto; dentro igual ao desenho inteiro")
{
	const Stroke stroke = makeStroke(1, AnnotationTool::Highlighter, kRed, 10, {{10, 10}, {200, 150}, {390, 290}});
	Image full(400, 300);
	voltz::rasterizeStroke(full.view(), stroke, 1.0, 1.0);
	Image clipped(400, 300);
	const PixelRect clip{100, 80, 260, 200};
	voltz::rasterizeStrokeClipped(clipped.view(), stroke, 1.0, 1.0, clip);
	int mismatches = 0;
	int touchedOutside = 0;
	int drawnInside = 0;
	for (int y = 0; y < 300; y++) {
		for (int x = 0; x < 400; x++) {
			const size_t index = (static_cast<size_t>(y) * 400 + static_cast<size_t>(x)) * 4;
			for (size_t c = 0; c < 4; c++) {
				if (inside(clip, x, y)) {
					mismatches += clipped.pixels[index + c] != full.pixels[index + c] ? 1 : 0;
				} else {
					touchedOutside += clipped.pixels[index + c] != 0 ? 1 : 0;
				}
			}
			drawnInside += inside(clip, x, y) && clipped.pixels[index + 3] != 0 ? 1 : 0;
		}
	}
	CHECK(drawnInside > 0);
	CHECK(mismatches == 0);
	CHECK(touchedOutside == 0);
}

TEST_CASE("redrawRegion depois de acrescentar pontos = redesenhar tudo (caneta, marca-texto, seta, retângulo)")
{
	const double scales[][2] = {{1.0, 1.0}, {0.25, 0.25}, {0.75, 0.5}};
	const AnnotationTool tools[] = {AnnotationTool::Pen, AnnotationTool::Highlighter, AnnotationTool::Arrow,
					AnnotationTool::Rectangle};
	for (const auto &scale : scales) {
		for (AnnotationTool tool : tools) {
			const Stroke below = makeStroke(1, AnnotationTool::Highlighter, kRed, 6, {{20, 30}, {300, 200}});
			Stroke stroke = makeStroke(2, tool, kLime, 5, {{40, 40}, {120, 90}});
			const Stroke above = makeStroke(3, AnnotationTool::Pen, kBlue, 3, {{100, 20}, {110, 280}});
			const std::vector<const Stroke *> list{&below, &stroke, &above};
			Image incremental(400, 300);
			redrawAll(incremental, list, scale[0], scale[1]);
			const voltz::StrokeTail tail = voltz::strokeTail(stroke);
			voltz::appendStrokePoints(stroke, {{200, 150}, {260, 90}});
			const PixelRect dirty = voltz::appendDirtyRect(tail, stroke, scale[0], scale[1]);
			CHECK_FALSE(dirty.empty());
			voltz::redrawRegion(incremental.view(), list, scale[0], scale[1], dirty);
			Image full(400, 300);
			redrawAll(full, list, scale[0], scale[1]);
			CHECK(incremental.pixels == full.pixels);
		}
	}
	/* ponto repetido na caneta: nada muda */
	Stroke pen = makeStroke(9, AnnotationTool::Pen, kLime, 4, {{10, 10}, {20, 20}});
	const voltz::StrokeTail tail = voltz::strokeTail(pen);
	voltz::appendStrokePoints(pen, {{20, 20}});
	CHECK(voltz::appendDirtyRect(tail, pen, 1.0, 1.0).empty());
}

TEST_CASE("redrawRegion depois de desfazer = redesenhar tudo sem o traço")
{
	for (double scale : {1.0, 0.25}) {
		const Stroke a = makeStroke(1, AnnotationTool::Highlighter, kRed, 8, {{30, 60}, {370, 80}});
		const Stroke b = makeStroke(2, AnnotationTool::Pen, kLime, 6, {{50, 20}, {120, 200}, {300, 260}});
		const Stroke c = makeStroke(3, AnnotationTool::Arrow, kBlue, 4, {{390, 10}, {150, 150}});
		Image image(400, 300);
		redrawAll(image, {&a, &b, &c}, scale, scale);
		voltz::redrawRegion(image.view(), {&a, &c}, scale, scale, voltz::strokeBounds(b, scale, scale));
		Image expected(400, 300);
		redrawAll(expected, {&a, &c}, scale, scale);
		CHECK(image.pixels == expected.pixels);
	}
}

TEST_CASE("DirtyHistory: união desde a versão conhecida; 0, lacuna e 'tudo' pedem tudo; capacidade")
{
	voltz::DirtyHistory history;
	history.record(2, PixelRect{0, 0, 10, 10}, false);
	history.record(3, PixelRect{20, 20, 30, 30}, false);
	PixelRect out{1, 1, 2, 2};
	CHECK(history.since(1, 3, out));
	CHECK(out == PixelRect{0, 0, 30, 30});
	CHECK(history.since(2, 3, out));
	CHECK(out == PixelRect{20, 20, 30, 30});
	CHECK(history.since(3, 3, out));
	CHECK(out.empty());
	CHECK_FALSE(history.since(0, 3, out));
	CHECK_FALSE(history.since(4, 3, out));
	history.record(4, PixelRect{}, true);
	CHECK_FALSE(history.since(3, 4, out));
	history.record(6, PixelRect{1, 1, 2, 2}, false); /* a versão 5 não foi registrada */
	CHECK_FALSE(history.since(4, 6, out));
	CHECK(history.since(5, 6, out));
	const uint64_t last = 6 + voltz::DirtyHistory::kCapacity;
	for (uint64_t version = 7; version <= last; version++) {
		history.record(version, PixelRect{0, 0, 1, 1}, false);
	}
	CHECK(history.since(6, last, out));
	CHECK(out == PixelRect{0, 0, 1, 1});
	CHECK_FALSE(history.since(5, last, out));
}
```
Em `frontend/voltz/tests/CMakeLists.txt`, depois de `    test-annotation-model.cpp`, acrescentar
`    test-annotation-dirty.cpp`.

- [ ] **Step 2: Ver falhar**

Run: CONFIGURAR; COMPILAR TESTES.
Expected: FALHA (`PixelRect`, `strokeBounds`, `redrawRegion`, `DirtyHistory` não declarados).

- [ ] **Step 3: Cabeçalho**

Em `frontend/voltz/core/annotation-raster.hpp`: trocar `#include <cstdint>` por
```cpp
#include <cstddef>
#include <cstdint>
#include <deque>
```
e acrescentar, logo antes de `} // namespace voltz`:
```cpp

// Retângulo de pixels [left, right) × [top, bottom); vazio quando left >= right ou top >= bottom.
struct PixelRect {
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;
	bool empty() const { return left >= right || top >= bottom; }
};

inline bool operator==(const PixelRect &a, const PixelRect &b)
{
	return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

// União (o menor retângulo que contém os dois; vazio não conta) e interseção (vazia = PixelRect{}).
PixelRect uniteRects(const PixelRect &a, const PixelRect &b);
PixelRect intersectRects(const PixelRect &a, const PixelRect &b);

// Todo pixel que rasterizeStroke pode tocar (pixels da imagem, com a escala dada), sem cortar na imagem. Vazio se o
// traço não desenha nada.
PixelRect strokeBounds(const Stroke &stroke, double scaleX, double scaleY);

// rasterizeStroke só dentro de `clip` (cortado na imagem): fora dele nada muda; dentro, o mesmo resultado.
void rasterizeStrokeClipped(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY,
			    const PixelRect &clip);

// Limpa `region` (cortada na imagem) e redesenha nela, na ordem, os traços que a tocam: o mesmo que limpar e redesenhar
// a imagem inteira, só que dentro de `region` (Plano 4C: sobreposição e fonte redesenham só o que mudou).
void redrawRegion(const RasterImage &image, const std::vector<const Stroke *> &strokes, double scaleX, double scaleY,
		  const PixelRect &region);

// O que appendDirtyRect precisa do traço ANTES de acrescentar pontos: quantos pontos tinha e, nas formas, os pontos.
struct StrokeTail {
	size_t count = 0;
	std::vector<StrokePoint> shapePoints;
};

StrokeTail strokeTail(const Stroke &stroke);

// Região que muda quando `after` é o traço de `before` com pontos acrescentados (appendStrokePoints): caneta e
// marca-texto, só as peças novas (do último ponto antigo em diante; vazio se nada entrou); formas, a forma antiga ∪ a nova.
PixelRect appendDirtyRect(const StrokeTail &before, const Stroke &after, double scaleX, double scaleY);

// Regiões que mudaram, por versão (AnnotationFeed do plugin): quem copia os traços de vez em quando pergunta o que mudou
// desde a versão que já tem. Guarda as últimas kCapacity versões.
class DirtyHistory {
public:
	static constexpr size_t kCapacity = 64;

	// `version` cresce de 1 em 1; `full` = mudou tudo (limpar, queimadas).
	void record(uint64_t version, const PixelRect &rect, bool full);
	// União das regiões das versões em (known, current]. Falso = refazer a imagem inteira (known 0 ou à frente,
	// versão que já saiu do histórico ou não foi registrada, mudança "tudo").
	bool since(uint64_t known, uint64_t current, PixelRect &out) const;

private:
	struct Entry {
		uint64_t version = 0;
		PixelRect rect;
		bool full = false;
	};
	std::deque<Entry> entries_;
};
```

- [ ] **Step 4: Implementação**

Em `frontend/voltz/core/annotation-raster.cpp`: trocar `#include <cstddef>` por
```cpp
#include <cstddef>
#include <iterator>
```
e trocar a função `rasterizeStroke` inteira (do `void rasterizeStroke(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY)`
até o `}` dela, logo antes de `} // namespace voltz`) por:
```cpp
namespace {

int boundCoordinate(double value)
{
	return static_cast<int>(std::clamp(value, -1.0e9, 1.0e9));
}

// A caixa que o rasterizador usa (com as margens de suavização): [floor(mín), ceil(máx) + 1). Vazia sem peças.
PixelRect shapeBounds(const StrokeShape &shape)
{
	if (shape.capsules.empty() && shape.triangles.empty()) {
		return PixelRect{};
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
	return PixelRect{boundCoordinate(std::floor(minX)), boundCoordinate(std::floor(minY)),
			 boundCoordinate(std::ceil(maxX) + 1.0), boundCoordinate(std::ceil(maxY) + 1.0)};
}

// O rasterizador do 4B limitado a `clip` (e à imagem). Dentro do limite o resultado é o mesmo do desenho inteiro: a
// cobertura de cada pixel não depende do limite.
void rasterizeShapeClipped(const RasterImage &image, const Stroke &stroke, const StrokeShape &shape,
			   const PixelRect &clip)
{
	const PixelRect box = intersectRects(intersectRects(shapeBounds(shape), clip),
					     PixelRect{0, 0, image.width, image.height});
	if (box.empty()) {
		return;
	}
	const int left = box.left;
	const int right = box.right;
	const int top = box.top;
	const int bottom = box.bottom;

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

} // namespace

PixelRect uniteRects(const PixelRect &a, const PixelRect &b)
{
	if (a.empty()) {
		return b.empty() ? PixelRect{} : b;
	}
	if (b.empty()) {
		return a;
	}
	return PixelRect{std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right),
			 std::max(a.bottom, b.bottom)};
}

PixelRect intersectRects(const PixelRect &a, const PixelRect &b)
{
	const PixelRect out{std::max(a.left, b.left), std::max(a.top, b.top), std::min(a.right, b.right),
			    std::min(a.bottom, b.bottom)};
	return out.empty() ? PixelRect{} : out;
}

PixelRect strokeBounds(const Stroke &stroke, double scaleX, double scaleY)
{
	return shapeBounds(strokeShape(stroke, scaleX, scaleY));
}

void rasterizeStroke(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY)
{
	rasterizeStrokeClipped(image, stroke, scaleX, scaleY, PixelRect{0, 0, image.width, image.height});
}

void rasterizeStrokeClipped(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY,
			    const PixelRect &clip)
{
	if (!image.pixels || image.width <= 0 || image.height <= 0) {
		return;
	}
	rasterizeShapeClipped(image, stroke, strokeShape(stroke, scaleX, scaleY), clip);
}

void redrawRegion(const RasterImage &image, const std::vector<const Stroke *> &strokes, double scaleX, double scaleY,
		  const PixelRect &region)
{
	if (!image.pixels || image.width <= 0 || image.height <= 0) {
		return;
	}
	const PixelRect area = intersectRects(region, PixelRect{0, 0, image.width, image.height});
	if (area.empty()) {
		return;
	}
	for (int y = area.top; y < area.bottom; y++) {
		std::fill_n(image.pixels + static_cast<size_t>(y) * static_cast<size_t>(image.stride) +
				    static_cast<size_t>(area.left) * 4,
			    static_cast<size_t>(area.right - area.left) * 4, uint8_t{0});
	}
	for (const Stroke *stroke : strokes) {
		if (!stroke) {
			continue;
		}
		const StrokeShape shape = strokeShape(*stroke, scaleX, scaleY);
		if (!intersectRects(shapeBounds(shape), area).empty()) {
			rasterizeShapeClipped(image, *stroke, shape, area);
		}
	}
}

StrokeTail strokeTail(const Stroke &stroke)
{
	StrokeTail tail;
	tail.count = stroke.points.size();
	if (isShapeTool(stroke.tool)) {
		tail.shapePoints = stroke.points;
	}
	return tail;
}

PixelRect appendDirtyRect(const StrokeTail &before, const Stroke &after, double scaleX, double scaleY)
{
	if (isShapeTool(after.tool)) {
		Stroke old;
		old.tool = after.tool;
		old.width = after.width;
		old.points = before.shapePoints;
		return uniteRects(strokeBounds(old, scaleX, scaleY), strokeBounds(after, scaleX, scaleY));
	}
	if (after.points.size() <= before.count) {
		return PixelRect{};
	}
	Stroke added;
	added.tool = after.tool;
	added.width = after.width;
	const size_t from = before.count > 0 ? before.count - 1 : 0;
	added.points.assign(after.points.begin() + static_cast<std::ptrdiff_t>(from), after.points.end());
	return strokeBounds(added, scaleX, scaleY);
}

void DirtyHistory::record(uint64_t version, const PixelRect &rect, bool full)
{
	entries_.push_back(Entry{version, rect, full});
	while (entries_.size() > kCapacity) {
		entries_.pop_front();
	}
}

bool DirtyHistory::since(uint64_t known, uint64_t current, PixelRect &out) const
{
	out = PixelRect{};
	if (known == current) {
		return true;
	}
	if (known == 0 || known > current) {
		return false;
	}
	uint64_t expected = known + 1;
	for (const Entry &entry : entries_) {
		if (entry.version <= known) {
			continue;
		}
		if (entry.version > current) {
			break;
		}
		if (entry.version != expected || entry.full) {
			return false;
		}
		out = uniteRects(out, entry.rect);
		expected++;
	}
	return expected == current + 1;
}
```
(As funções internas do 4B — `scaledPoint`, `addCapsule`, `capsuleDistance`, `triangleDepth`, `pixelSpan`,
`blendChannel` — e `strokeShape`, `clearRaster`, `strokeRenderWidth`, `strokeOpacity` ficam como estão; o corpo do
desenho é o do 4B, só que limitado pela caixa `box`.)

- [ ] **Step 5: Rodar e ver passar (inclusive os 8 casos do rasterizador do 4B)**

```bash
"$CLANGFMT" -i frontend/voltz/core/annotation-raster.hpp frontend/voltz/core/annotation-raster.cpp frontend/voltz/tests/test-annotation-dirty.cpp
```
Run: COMPILAR TESTES; RODAR TESTES; `build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe | tail -3`.
Expected: `100% tests passed`; `test cases: 170 | 170 passed` (os testes de `test-annotation-raster.cpp` seguem
passando: o desenho inteiro é o recorte do tamanho da imagem).

- [ ] **Step 6: Commit**

```bash
git add frontend/voltz/core/annotation-raster.hpp frontend/voltz/core/annotation-raster.cpp frontend/voltz/tests/test-annotation-dirty.cpp frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): retangulo sujo no rasterizador (strokeBounds, redrawRegion, appendDirtyRect, DirtyHistory), igual ao desenho inteiro

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 3: Plugin — `AnnotationFeed` com regiões por versão e fonte que redesenha só o que mudou

**Files:**
- Modify: `plugins/voltz-sources/annotation-feed.hpp`, `plugins/voltz-sources/annotation-feed.cpp`
- Modify: `plugins/voltz-sources/annotation-source.cpp`

Sem teste unitário próprio (a regra é a do núcleo, testada na Task 2); medido ao vivo na Task 4 (pedido
`AnnotationFeedState`) e no smoke da Task 8.

- [ ] **Step 1: Cabeçalho do feed**

Trocar o conteúdo de `plugins/voltz-sources/annotation-feed.hpp` por:
```cpp
#pragma once

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/annotation-raster.hpp>

#include <obs.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace voltz {

// Traços ao vivo das anotações, um estado só no processo. Escrito pelos procedimentos voltz_annot_* (thread da UI do
// frontend) e pelos pedidos de teste (thread do obs-websocket); lido pelas fontes Voltz Anotações na thread gráfica, que
// só copiam os traços quando a versão muda. Cada mudança registra a região que mudou (pixels da captura, DirtyHistory):
// a fonte redesenha só ela (Plano 4C) e mede cada redesenho. Trechos curtos sob um mutex; o JSON é lido fora dele.
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
	// Copia os traços para `out` e devolve true se a versão mudou desde `knownVersion` (0 = nunca copiou); `dirty` = a
	// região que mudou desde ela e `full` = redesenhar tudo. `version` e `burned` sempre preenchidos.
	bool snapshotIfChanged(uint64_t knownVersion, std::vector<Stroke> &out, uint64_t &version, bool &burned,
			       PixelRect &dirty, bool &full);
	// Tempo de um redesenho da fonte (thread gráfica). A cada 600 redesenhos vai um resumo para o log.
	void recordRaster(int64_t elapsedNs, bool partial);
	// {"tracos":N,"versao":V,"queimadas":false,"mostradas":M,"raster_us_ultimo":U,"raster_us_max":X,"parciais":P,
	//  "completas":C,"ids":[...]} (pedido de teste AnnotationFeedState).
	std::string describe();

	// Fontes Voltz Anotações sendo mostradas (show/hide do libobs; o destroy desconta o que o hide não descontou).
	void addShowing(int delta) { showing_.fetch_add(delta); }
	int showing() const { return showing_.load(); }

private:
	AnnotationFeed() = default;
	// Sob o mutex: nova versão com a região que mudou.
	void changed(const PixelRect &rect, bool full);

	std::mutex mutex_;
	std::vector<Stroke> strokes_;
	uint64_t version_ = 1;
	bool burned_ = false;
	DirtyHistory history_;
	int64_t rasterLastNs_ = 0;
	int64_t rasterMaxNs_ = 0;
	uint32_t partialRasters_ = 0;
	uint32_t fullRasters_ = 0;
	std::atomic<int> showing_{0};
};

// Registra voltz_annot_begin, voltz_annot_extend, voltz_annot_remove, voltz_annot_clear e voltz_annot_burned
// (live-protocol.hpp).
void registerAnnotationProcs(proc_handler_t *handler);

} // namespace voltz
```

- [ ] **Step 2: Implementação do feed**

Em `plugins/voltz-sources/annotation-feed.cpp`: trocar `#include <cstdio>` por
```cpp
#include <algorithm>
#include <cstdio>
```
e trocar tudo de `bool AnnotationFeed::begin(int id, const std::string &json)` até o fim de `std::string AnnotationFeed::describe()`
(inclusive) por:
```cpp
void AnnotationFeed::changed(const PixelRect &rect, bool full)
{
	version_++;
	history_.record(version_, rect, full);
}

bool AnnotationFeed::begin(int id, const std::string &json)
{
	Stroke stroke;
	if (!strokeFromJson(json, stroke)) {
		return false;
	}
	stroke.id = id;
	const PixelRect area = strokeBounds(stroke, 1.0, 1.0);
	std::lock_guard<std::mutex> lock(mutex_);
	for (Stroke &existing : strokes_) {
		if (existing.id == id) {
			const PixelRect old = strokeBounds(existing, 1.0, 1.0);
			existing = std::move(stroke);
			changed(uniteRects(old, area), false);
			return true;
		}
	}
	strokes_.push_back(std::move(stroke));
	changed(area, false);
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
			const StrokeTail tail = strokeTail(stroke);
			appendStrokePoints(stroke, points);
			changed(appendDirtyRect(tail, stroke, 1.0, 1.0), false);
			return;
		}
	}
}

void AnnotationFeed::remove(int id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto it = strokes_.begin(); it != strokes_.end(); ++it) {
		if (it->id == id) {
			const PixelRect area = strokeBounds(*it, 1.0, 1.0);
			strokes_.erase(it);
			changed(area, false);
			return;
		}
	}
}

void AnnotationFeed::clear()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!strokes_.empty()) {
		strokes_.clear();
		changed(PixelRect{}, true);
	}
}

void AnnotationFeed::setBurned(bool burned)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (burned_ != burned) {
		burned_ = burned;
		changed(PixelRect{}, true);
		blog(LOG_INFO, "[voltz-sources] anotacoes %s",
		     burned ? "queimadas: a fonte nao desenha" : "desenhadas pela fonte");
	}
}

bool AnnotationFeed::snapshotIfChanged(uint64_t knownVersion, std::vector<Stroke> &out, uint64_t &version,
				       bool &burned, PixelRect &dirty, bool &full)
{
	std::lock_guard<std::mutex> lock(mutex_);
	version = version_;
	burned = burned_;
	dirty = PixelRect{};
	full = false;
	if (knownVersion == version_) {
		return false;
	}
	full = !history_.since(knownVersion, version_, dirty);
	out = strokes_;
	return true;
}

void AnnotationFeed::recordRaster(int64_t elapsedNs, bool partial)
{
	uint32_t partials = 0;
	uint32_t total = 0;
	int64_t maxNs = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		rasterLastNs_ = elapsedNs;
		rasterMaxNs_ = std::max(rasterMaxNs_, elapsedNs);
		if (partial) {
			partialRasters_++;
		} else {
			fullRasters_++;
		}
		partials = partialRasters_;
		total = partialRasters_ + fullRasters_;
		maxNs = rasterMaxNs_;
	}
	if (total % 600 == 0) {
		blog(LOG_INFO, "[voltz-sources] anotacoes: %u redesenho(s) (%u parcial(is)), ultimo %lld us, max %lld us",
		     total, partials, static_cast<long long>(elapsedNs / 1000), static_cast<long long>(maxNs / 1000));
	}
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
	char buffer[320];
	std::snprintf(buffer, sizeof(buffer),
		      "{\"tracos\":%zu,\"versao\":%llu,\"queimadas\":%s,\"mostradas\":%d,\"raster_us_ultimo\":%lld,"
		      "\"raster_us_max\":%lld,\"parciais\":%u,\"completas\":%u,\"ids\":[",
		      strokes_.size(), static_cast<unsigned long long>(version_), burned_ ? "true" : "false",
		      showing_.load(), static_cast<long long>(rasterLastNs_ / 1000),
		      static_cast<long long>(rasterMaxNs_ / 1000), partialRasters_, fullRasters_);
	return std::string(buffer) + ids + "]}";
}
```

- [ ] **Step 3: Fonte com redesenho parcial**

Em `plugins/voltz-sources/annotation-source.cpp`: depois de `#include <obs-module.h>` acrescentar
`#include <util/platform.h>`; trocar a função `rebuild` inteira e a função `annotationsRender` inteira por:
```cpp
// Refaz a imagem (CPU, núcleo) e manda para a textura. Só quando a versão do feed ou o tamanho muda; com a textura do
// mesmo tamanho e uma região conhecida, redesenha só a região (o envio continua inteiro: textura dinâmica do D3D11 é
// mapeada com WRITE_DISCARD).
void rebuild(AnnotationSource *ctx, uint32_t width, uint32_t height, uint64_t version, bool full,
	     const PixelRect &dirty)
{
	const int64_t start = static_cast<int64_t>(os_gettime_ns());
	const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
	const bool partial = !full && ctx->texture && ctx->textureWidth == width && ctx->textureHeight == height &&
			     ctx->pixels.size() == bytes;
	if (partial && dirty.empty()) {
		ctx->hasStrokes = !ctx->strokes.empty();
		ctx->builtVersion = version;
		return;
	}
	ctx->pixels.resize(bytes);
	const RasterImage image{ctx->pixels.data(), static_cast<int>(width), static_cast<int>(height),
				static_cast<int>(width) * 4};
	std::vector<const Stroke *> strokes;
	strokes.reserve(ctx->strokes.size());
	for (const Stroke &stroke : ctx->strokes) {
		strokes.push_back(&stroke);
	}
	if (partial) {
		redrawRegion(image, strokes, 1.0, 1.0, dirty);
	} else {
		clearRaster(image);
		for (const Stroke *stroke : strokes) {
			rasterizeStroke(image, *stroke, 1.0, 1.0);
		}
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
	AnnotationFeed::instance().recordRaster(static_cast<int64_t>(os_gettime_ns()) - start, partial);
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
	PixelRect dirty;
	bool full = true;
	const bool changed = AnnotationFeed::instance().snapshotIfChanged(resized ? 0 : ctx->builtVersion, ctx->strokes,
									  version, burned, dirty, full);
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
		rebuild(ctx, width, height, version, full || resized, dirty);
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
```

- [ ] **Step 4: Compilar o plugin**

```bash
"$CLANGFMT" -i plugins/voltz-sources/annotation-feed.hpp plugins/voltz-sources/annotation-feed.cpp plugins/voltz-sources/annotation-source.cpp
```
Run: COMPILAR PLUGIN (segundo plano).
Expected: `exit=0`, nenhuma linha de erro.

- [ ] **Step 5: Commit**

```bash
git add plugins/voltz-sources/annotation-feed.hpp plugins/voltz-sources/annotation-feed.cpp plugins/voltz-sources/annotation-source.cpp
git commit -m "feat(voltz-sources): Voltz Anotacoes redesenha so a regiao que mudou (DirtyHistory por versao) e mede cada redesenho

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 4: Sobreposição e `AnnotationController` com retângulo sujo, estilo salvo, alvo do desenho e estado do modo

**Files:**
- Modify: `frontend/voltz/annotate/OverlayWindow.hpp`, `frontend/voltz/annotate/OverlayWindow.cpp`
- Modify: `frontend/voltz/annotate/AnnotationController.hpp`, `frontend/voltz/annotate/AnnotationController.cpp`
- Modify: `frontend/voltz/annotate/AnnotationTestVendor.hpp`, `frontend/voltz/annotate/AnnotationTestVendor.cpp`
- Modify: `frontend/voltz/project/MonitorLocator.hpp`, `frontend/voltz/project/MonitorLocator.cpp`

- [ ] **Step 1: `MonitorLocator` devolve o nome GDI do monitor**

Em `frontend/voltz/project/MonitorLocator.hpp`, dentro de `struct MonitorDescription`, depois de
`	double scale = 1.0; // DPI efetivo / 96`, acrescentar:
```cpp
	std::string deviceName; // nome GDI ("\\.\DISPLAY1"): o mesmo de QScreen::name() no Windows
```
Em `frontend/voltz/project/MonitorLocator.cpp`: em `struct MonitorSearch`, depois de `	RECT rect = {};`, acrescentar
`	std::string device;`; em `matchMonitor`, trocar
```cpp
	if (match) {
		search->handle = handle;
		search->rect = *rect;
	}
```
por
```cpp
	if (match) {
		search->handle = handle;
		search->rect = *rect;
		search->device = info.szDevice;
	}
```
e em `findMonitor`, depois de `	result.id = monitorId;`, acrescentar `	result.deviceName = search.device;`.

- [ ] **Step 2: `OverlayWindow` — cabeçalho**

Em `frontend/voltz/annotate/OverlayWindow.hpp`: depois de `#include <voltz/core/annotation-model.hpp>` acrescentar
`#include <voltz/core/annotation-raster.hpp>`; no fim do comentário da classe (depois de
`// UpdateLayeredWindow. Thread da UI (as mensagens da janela chegam pelo laço de eventos do Qt).`) acrescentar:
```cpp
// Plano 4C: redesenha só a região que mudou (UpdateLayeredWindowIndirect com prcDirty), mede cada desenho e, se o
// "sempre no topo" sumir, devolve-o (o bit no estilo sozinho não muda a ordem Z). Continua sem modo clicável: a entrada
// do modo desenho é outra janela (DrawInputWindow).
```
em `struct Info`, depois de `		MonitorRect rect;`, acrescentar:
```cpp
		uint32_t partialRenders = 0;
		uint32_t fullRenders = 0;
		int64_t partialTotalNs = 0;
		int64_t partialMaxNs = 0;
		int64_t fullMaxNs = 0;
```
trocar a declaração de `render` (e o comentário acima dela) por:
```cpp
	// Desenha `strokes` (pixels da captura captureWidth×captureHeight, esticados para o retângulo) e mostra sem ativar;
	// sem traços, limpa e esconde. `dirty` (pixels do buffer) = só essa região mudou — vale com o buffer do mesmo tamanho
	// e a janela já visível; senão desenha tudo. Região vazia = nada a fazer.
	void render(const std::vector<const Stroke *> &strokes, int captureWidth, int captureHeight,
		    const PixelRect *dirty = nullptr);
```
e, nos membros privados, depois de `	bool visible_ = false;`, acrescentar:
```cpp
	uint32_t partialRenders_ = 0;
	uint32_t fullRenders_ = 0;
	int64_t partialTotalNs_ = 0;
	int64_t partialMaxNs_ = 0;
	int64_t fullMaxNs_ = 0;
```

- [ ] **Step 3: `OverlayWindow` — implementação**

Em `frontend/voltz/annotate/OverlayWindow.cpp`: depois de `#include <util/base.h>` acrescentar
`#include <util/platform.h>`; trocar `#include <cstddef>` por
```cpp
#include <algorithm>
#include <cstddef>
```
trocar a função `ensureClickThrough` inteira por:
```cpp
void OverlayWindow::ensureClickThrough()
{
#ifdef _WIN32
	HWND hwnd = static_cast<HWND>(hwnd_);
	const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
	if ((style & kClickThroughStyle) != kClickThroughStyle) {
		SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style | kClickThroughStyle);
		blog(LOG_WARNING, "[voltz] anotacoes: estilo da sobreposicao refeito (transparente a cliques)");
	}
	if ((style & WS_EX_TOPMOST) == 0) {
		/* o bit no estilo não muda a ordem Z: só o SetWindowPos com HWND_TOPMOST devolve o "sempre no topo" */
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		blog(LOG_WARNING, "[voltz] anotacoes: sobreposicao de volta ao topo");
	}
#endif
}
```
trocar a função `render` inteira por:
```cpp
void OverlayWindow::render(const std::vector<const Stroke *> &strokes, int captureWidth, int captureHeight,
			   const PixelRect *dirty)
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
	const bool sameBuffer = bits_ && bufferWidth_ == rect_.width && bufferHeight_ == rect_.height;
	const bool partial = dirty && sameBuffer && visible_;
	if (!ensureBuffer(rect_.width, rect_.height)) {
		blog(LOG_WARNING, "[voltz] anotacoes: sem memoria para a sobreposicao %dx%d", rect_.width,
		     rect_.height);
		hide();
		return;
	}
	PixelRect area{0, 0, bufferWidth_, bufferHeight_};
	if (partial) {
		area = intersectRects(*dirty, area);
		if (area.empty()) {
			return; /* nada mudou na imagem */
		}
	}
	const int64_t start = static_cast<int64_t>(os_gettime_ns());
	const RasterImage image{bits_, bufferWidth_, bufferHeight_, bufferWidth_ * 4};
	const double scaleX = static_cast<double>(bufferWidth_) / static_cast<double>(captureWidth);
	const double scaleY = static_cast<double>(bufferHeight_) / static_cast<double>(captureHeight);
	if (partial) {
		redrawRegion(image, strokes, scaleX, scaleY, area);
	} else {
		clearRaster(image);
		for (const Stroke *stroke : strokes) {
			rasterizeStroke(image, *stroke, scaleX, scaleY);
		}
	}
	GdiFlush();
	POINT position{rect_.left, rect_.top};
	SIZE size{bufferWidth_, bufferHeight_};
	POINT source{0, 0};
	BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
	RECT dirtyRect{area.left, area.top, area.right, area.bottom};
	UPDATELAYEREDWINDOWINFO update = {};
	update.cbSize = sizeof(update);
	update.pptDst = &position;
	update.psize = &size;
	update.hdcSrc = static_cast<HDC>(memoryDc_);
	update.pptSrc = &source;
	update.pblend = &blend;
	update.dwFlags = ULW_ALPHA;
	update.prcDirty = partial ? &dirtyRect : nullptr;
	if (!UpdateLayeredWindowIndirect(hwnd, &update)) {
		blog(LOG_WARNING, "[voltz] anotacoes: UpdateLayeredWindowIndirect falhou (%lu)", GetLastError());
		return;
	}
	const int64_t elapsed = static_cast<int64_t>(os_gettime_ns()) - start;
	if (partial) {
		partialRenders_++;
		partialTotalNs_ += elapsed;
		partialMaxNs_ = std::max(partialMaxNs_, elapsed);
	} else {
		fullRenders_++;
		fullMaxNs_ = std::max(fullMaxNs_, elapsed);
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
	(void)dirty;
#endif
}
```
e, em `OverlayWindow::info()`, logo depois de `	info.rect = rect_;`, acrescentar:
```cpp
	info.partialRenders = partialRenders_;
	info.fullRenders = fullRenders_;
	info.partialTotalNs = partialTotalNs_;
	info.partialMaxNs = partialMaxNs_;
	info.fullMaxNs = fullMaxNs_;
```

- [ ] **Step 4: Conferência de segurança da sobreposição (continua obrigatória)**

```bash
grep -nE "SetCapture|SetForegroundWindow|SetFocus|SetActiveWindow|RegisterHotKey|SetWindowsHookEx|SendInput|SetCursorPos|mouse_event|keybd_event" frontend/voltz/annotate/OverlayWindow.cpp; echo "achados=$?"
grep -nE "&\s*~" frontend/voltz/annotate/OverlayWindow.cpp; echo "achados=$?"
```
Expected: `achados=1` nas duas.

- [ ] **Step 5: `AnnotationController` — cabeçalho**

Trocar o conteúdo de `frontend/voltz/annotate/AnnotationController.hpp` por:
```cpp
#pragma once

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/annotation-raster.hpp>
#include <voltz/core/cursor-track.hpp>
#include <voltz/core/stroke-store.hpp>

#include <QObject>
#include <QTimer>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace voltz {

class OverlayWindow;

// Anotações ao vivo (spec §6.2). Dono do StrokeStore (traços em pixels da captura da Tela compartilhada); repassa cada
// mudança ao plugin voltz-sources (procedimentos voltz_annot_*, desenhados pela fonte Voltz Anotações no programa), à
// sobreposição do monitor da Tela (OverlayWindow: transparente a cliques, fora da captura por WDA_EXCLUDEFROMCAPTURE) e a
// quem grava (ProjectRecorder → anotacoes.jsonl, pelos sinais). "Queimadas" (Windows < 10 2004 ou afinidade recusada,
// spec §6.3): a sobreposição fica sem afinidade (aparece na captura), a fonte não desenha (não duplica) e o manifesto
// marca anotacoes_queimadas. "Somem após N s" pelo estilo (user.ini [Voltz] AnnotationStyle). Plano 4C: os traços
// chegam do modo desenho (DrawModeController) e, com VOLTZ_TEST_LIVE, do fornecedor de teste; a sobreposição redesenha
// só a região que mudou; o estilo escolhido na barra é salvo; drawTarget diz onde o modo desenho pode desenhar.
// Thread da UI.
class AnnotationController : public QObject {
	Q_OBJECT

public:
	explicit AnnotationController(QObject *parent = nullptr);
	~AnnotationController() override;

	// EXIT do frontend, antes de o libobs descarregar o plugin: grava o estilo pendente, destrói a sobreposição, para os
	// temporizadores e os pedidos de teste; depois disso nada mais é enviado ao plugin.
	void shutdown();

	// Começa um traço (termina antes o que estiver em andamento). Devolve o id (0 se desligado).
	int beginStroke(AnnotationTool tool, const RgbColor &color, int width, const StrokePoint &first);
	void extendStroke(const std::vector<StrokePoint> &points);
	void endStroke();
	void undo();
	void clear();
	// Ferramenta, cor e espessura do próximo traço e "somem após" (limitados). Valem na hora (styleChanged); o user.ini
	// é gravado 400 ms depois da última mudança.
	void setStyle(const AnnotationStyle &style);
	// Onde o modo desenho pode desenhar: o monitor da Tela compartilhada (captura de monitor) e o tamanho da captura;
	// nunca o retângulo de teste. `deviceName` = nome GDI do monitor. Falso sem captura de monitor.
	bool drawTarget(CaptureMapping &mapping, std::string &deviceName) const;
	// Estado do modo desenho no AnnotationState (testes): ligado, último motivo de recusa e de saída ("" = nenhum).
	void setDrawStatus(bool active, const char *refusal, const char *exit);
	// "N parcial(is) (media X us, max Y us), C completa(s) (max Z us)" da sobreposição, para o log do modo desenho.
	std::string overlayStatsText() const;
	// Só testes: guarda os pixels da sobreposição nesses pontos (pixels da janela) para o estado de teste.
	void probeOverlay(const std::vector<StrokePoint> &points);
	// Só testes (VOLTZ_TEST_LIVE): publica o estado para o pedido AnnotationState (inclui o tamanho atual da captura).
	void publishTestState();

	const std::vector<LiveStroke> &visibleStrokes() const { return store_.visible(); }
	const AnnotationStyle &style() const { return style_; }
	bool burned() const { return burned_; }

signals:
	// Direto (mesma thread): o ProjectRecorder escreve o diário na hora.
	void strokeEnded(const voltz::LiveStroke &stroke);
	void strokesRemoved(const std::vector<voltz::RemovedStroke> &removed);
	void burnedChanged(bool burned);
	void styleChanged();

private:
	void removed(const std::vector<RemovedStroke> &strokes, bool all);
	void scheduleExpiry();
	void expire();
	int64_t lifetimeNs() const;
	void saveStyle();
	// Recalcula o alvo da sobreposição (monitor da Tela) e redesenha se os traços (`strokesChanged`) ou o alvo mudaram.
	// `dirty` = região do buffer da sobreposição que mudou (nullopt = tudo); só vale se o alvo não mudou.
	void refreshOverlay(bool strokesChanged, const std::optional<PixelRect> &dirty = std::nullopt);
	bool overlayTarget(MonitorRect &rect, int &captureWidth, int &captureHeight) const;
	// Escala captura → buffer da sobreposição de agora; falso sem sobreposição ou sem alvo.
	bool overlayScale(double &scaleX, double &scaleY) const;
	std::vector<const Stroke *> strokePointers() const;
	void markBurned(const char *why);

	StrokeStore store_;
	AnnotationStyle style_;
	std::unique_ptr<OverlayWindow> overlay_;
	MonitorRect overlayRect_;
	int captureWidth_ = 0;
	int captureHeight_ = 0;
	QTimer expiry_;
	QTimer overlayPoll_;
	QTimer saveTimer_;
	std::vector<std::string> samples_;
	std::string drawRefusal_;
	std::string drawExit_;
	bool drawActive_ = false;
	bool burned_ = false;
	bool testMode_ = false;
	bool shutdown_ = false;
	int overlayState_ = -1; // -1 desconhecido, 0 sem alvo, 1 com alvo (só para o log)
};

} // namespace voltz
```

- [ ] **Step 6: `AnnotationController` — implementação**

Em `frontend/voltz/annotate/AnnotationController.cpp`:

1. Trocar `#include <algorithm>` por
```cpp
#include <algorithm>
#include <cstdio>
```
2. Depois de `constexpr int64_t kMaxTimerMs = 3600000;` acrescentar `constexpr int kStyleSaveDelayMs = 400;`.
3. No construtor, depois de `	connect(&overlayPoll_, &QTimer::timeout, this, [this]() { refreshOverlay(false); });`, acrescentar:
```cpp
	saveTimer_.setSingleShot(true);
	saveTimer_.setInterval(kStyleSaveDelayMs);
	connect(&saveTimer_, &QTimer::timeout, this, [this]() { saveStyle(); });
```
4. Em `shutdown()`, logo antes de `	expiry_.stop();`, acrescentar:
```cpp
	if (saveTimer_.isActive()) {
		saveStyle();
	}
```
5. Trocar as funções `beginStroke` e `extendStroke` inteiras por:
```cpp
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
	const Stroke &stroke = store_.visible().back().stroke;
	sendBegin(stroke);
	std::optional<PixelRect> dirty;
	double scaleX = 0.0;
	double scaleY = 0.0;
	if (overlayScale(scaleX, scaleY)) {
		dirty = strokeBounds(stroke, scaleX, scaleY);
	}
	refreshOverlay(true, dirty);
	return id;
}

void AnnotationController::extendStroke(const std::vector<StrokePoint> &points)
{
	if (shutdown_ || points.empty() || store_.openId() == 0) {
		return;
	}
	const StrokeTail tail = strokeTail(store_.visible().back().stroke);
	if (!store_.extend(points)) {
		return;
	}
	sendExtend(store_.openId(), points);
	std::optional<PixelRect> dirty;
	double scaleX = 0.0;
	double scaleY = 0.0;
	if (overlayScale(scaleX, scaleY)) {
		dirty = appendDirtyRect(tail, store_.visible().back().stroke, scaleX, scaleY);
	}
	refreshOverlay(true, dirty);
}
```
6. Trocar a função `removed` inteira por:
```cpp
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
	std::optional<PixelRect> dirty;
	double scaleX = 0.0;
	double scaleY = 0.0;
	if (overlayScale(scaleX, scaleY)) {
		PixelRect area;
		for (const RemovedStroke &stroke : strokes) {
			area = uniteRects(area, strokeBounds(stroke.live.stroke, scaleX, scaleY));
		}
		dirty = area;
	}
	emit strokesRemoved(strokes);
	scheduleExpiry();
	refreshOverlay(true, dirty);
	blog(LOG_INFO, "[voltz] anotacoes: %d traco(s) removido(s), %d na tela", static_cast<int>(strokes.size()),
	     static_cast<int>(store_.visible().size()));
}
```
7. Trocar a função `refreshOverlay` inteira por:
```cpp
void AnnotationController::refreshOverlay(bool strokesChanged, const std::optional<PixelRect> &dirty)
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
			blog(LOG_INFO, "[voltz] anotacoes: sobreposicao em (%d,%d %dx%d) para a captura %dx%d",
			     rect.left, rect.top, rect.width, rect.height, captureWidth, captureHeight);
		} else {
			blog(LOG_INFO, "[voltz] anotacoes: sem captura de monitor; sem sobreposicao");
		}
	}
	const std::vector<const Stroke *> strokes = strokePointers();
	if (!hasTarget) {
		if (overlay_) {
			overlay_->render({}, 0, 0);
		}
		/* Sem alvo agora (Tela trocada, captura reiniciando com tamanho 0): esquece o alvo, para redesenhar quando ele
		 * voltar, e continua conferindo enquanto houver traço — senão a sobreposição ficaria escondida até o próximo
		 * traço. */
		overlayRect_ = MonitorRect{};
		captureWidth_ = 0;
		captureHeight_ = 0;
		if (strokes.empty()) {
			overlayPoll_.stop();
		} else if (!overlayPoll_.isActive()) {
			overlayPoll_.start();
		}
		publishTestState();
		return;
	}
	const bool moved = !sameRect(rect, overlayRect_) || captureWidth != captureWidth_ ||
			   captureHeight != captureHeight_;
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
	bool created = false;
	if (!overlay_) {
		auto overlay = std::make_unique<OverlayWindow>();
		if (!overlay->create(rect)) {
			blog(LOG_WARNING, "[voltz] anotacoes: a sobreposicao nao foi criada");
			publishTestState();
			return;
		}
		overlay_ = std::move(overlay);
		created = true;
		if (!burned_ && !overlay_->excludeFromCapture()) {
			markBurned("afinidade recusada");
		}
		blog(LOG_INFO, "[voltz] anotacoes: sobreposicao criada (%s)",
		     burned_ ? "visivel na captura" : "fora da captura");
	} else {
		overlay_->setRect(rect);
	}
	/* só a região que mudou, se o alvo é o mesmo; senão (criada, movida, tamanho novo) a imagem inteira */
	const bool partial = !moved && !created && dirty.has_value();
	overlay_->render(strokes, captureWidth, captureHeight, partial ? &*dirty : nullptr);
	if (strokes.empty()) {
		overlayPoll_.stop();
	} else if (!overlayPoll_.isActive()) {
		overlayPoll_.start();
	}
	publishTestState();
}

bool AnnotationController::overlayScale(double &scaleX, double &scaleY) const
{
	if (!overlay_ || overlayRect_.width <= 0 || overlayRect_.height <= 0 || captureWidth_ <= 0 ||
	    captureHeight_ <= 0) {
		return false;
	}
	scaleX = static_cast<double>(overlayRect_.width) / static_cast<double>(captureWidth_);
	scaleY = static_cast<double>(overlayRect_.height) / static_cast<double>(captureHeight_);
	return true;
}

std::vector<const Stroke *> AnnotationController::strokePointers() const
{
	std::vector<const Stroke *> out;
	out.reserve(store_.visible().size());
	for (const LiveStroke &live : store_.visible()) {
		out.push_back(&live.stroke);
	}
	return out;
}

void AnnotationController::setStyle(const AnnotationStyle &style)
{
	if (shutdown_) {
		return;
	}
	const int fadeBefore = style_.fadeSeconds;
	style_ = clampAnnotationStyle(style);
	saveTimer_.start();
	if (style_.fadeSeconds != fadeBefore) {
		blog(LOG_INFO, "[voltz] anotacoes: somem apos %d s", style_.fadeSeconds);
		scheduleExpiry();
	}
	emit styleChanged();
	publishTestState();
}

void AnnotationController::saveStyle()
{
	saveTimer_.stop();
	config_set_string(App()->GetUserConfig(), "Voltz", "AnnotationStyle", annotationStyleToJson(style_).c_str());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

bool AnnotationController::drawTarget(CaptureMapping &mapping, std::string &deviceName) const
{
	OBSSource screen = findSharedSource(SharedSource::Screen);
	if (!screen || strcmp(obs_source_get_unversioned_id(screen), "monitor_capture") != 0) {
		return false;
	}
	const int captureWidth = static_cast<int>(obs_source_get_width(screen));
	const int captureHeight = static_cast<int>(obs_source_get_height(screen));
	if (captureWidth <= 0 || captureHeight <= 0) {
		return false;
	}
	OBSDataAutoRelease settings = obs_source_get_settings(screen);
	const MonitorDescription monitor = findMonitor(obs_data_get_string(settings, "monitor_id"));
	if (!monitor.found) {
		return false;
	}
	mapping = CaptureMapping{monitor.rect, captureWidth, captureHeight};
	deviceName = monitor.deviceName;
	return true;
}

void AnnotationController::setDrawStatus(bool active, const char *refusal, const char *exit)
{
	drawActive_ = active;
	drawRefusal_ = refusal ? refusal : "";
	drawExit_ = exit ? exit : "";
	publishTestState();
}

std::string AnnotationController::overlayStatsText() const
{
	const OverlayWindow::Info info = overlay_ ? overlay_->info() : OverlayWindow::Info{};
	const long long average =
		info.partialRenders ? static_cast<long long>(info.partialTotalNs / info.partialRenders / 1000) : 0;
	char text[192];
	std::snprintf(text, sizeof(text), "%u parcial(is) (media %lld us, max %lld us), %u completa(s) (max %lld us)",
		      info.partialRenders, average, static_cast<long long>(info.partialMaxNs / 1000), info.fullRenders,
		      static_cast<long long>(info.fullMaxNs / 1000));
	return text;
}
```
8. Trocar a função `publishTestState` inteira por:
```cpp
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
	overlay.insert(QStringLiteral("parciais"), static_cast<qint64>(info.partialRenders));
	overlay.insert(QStringLiteral("completas"), static_cast<qint64>(info.fullRenders));
	overlay.insert(QStringLiteral("parcial_us_media"),
		       info.partialRenders ? static_cast<qint64>(info.partialTotalNs / info.partialRenders / 1000)
					   : qint64(0));
	overlay.insert(QStringLiteral("parcial_us_max"), static_cast<qint64>(info.partialMaxNs / 1000));
	overlay.insert(QStringLiteral("completa_us_max"), static_cast<qint64>(info.fullMaxNs / 1000));
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
	state.insert(QStringLiteral("modo_desenho"), drawActive_);
	state.insert(QStringLiteral("desenho_recusa"), QString::fromStdString(drawRefusal_));
	state.insert(QStringLiteral("desenho_saida"), QString::fromStdString(drawExit_));
	AnnotationTestVendor::publish(QJsonDocument(state).toJson(QJsonDocument::Compact).toStdString());
}
```
(As demais funções — construtor no resto, `endStroke`, `undo`, `clear`, `lifetimeNs`, `scheduleExpiry`, `expire`,
`overlayTarget`, `markBurned`, `probeOverlay` e os auxiliares do topo — ficam como estão.)

- [ ] **Step 7: Pedido de teste `SetAnnotationStyle`**

Em `frontend/voltz/annotate/AnnotationTestVendor.hpp`, no comentário da classe, depois da linha de `ProbeOverlay`,
acrescentar:
```cpp
//   SetAnnotationStyle {estilo:"{...}"} — AnnotationController::setStyle (o que a barra do modo desenho faz);
```
Em `frontend/voltz/annotate/AnnotationTestVendor.cpp`: trocar o `kRequests` por
```cpp
constexpr const char *kRequests[] = {"InjectStroke", "ExtendStroke",    "EndStroke",
				     "UndoStroke",   "ClearStrokes",    "ProbeOverlay",
				     "AnnotationState", "SetAnnotationStyle"};
```
depois da função `probeOverlay` (antes de `void annotationState(`), acrescentar:
```cpp
void setAnnotationStyle(obs_data_t *request, obs_data_t *response, void *)
{
	AnnotationStyle style;
	const bool parsed = annotationStyleFromJson(obs_data_get_string(request, "estilo"), style);
	const bool ok = parsed && post([style](AnnotationController *controller) { controller->setStyle(style); });
	obs_data_set_bool(response, "ok", ok);
}
```
e em `AnnotationTestVendor::start`, depois do registro de `"AnnotationState"`, acrescentar:
```cpp
	obs_websocket_vendor_register_request(gVendor, "SetAnnotationStyle", setAnnotationStyle, nullptr);
```

- [ ] **Step 8: Compilar**

```bash
"$CLANGFMT" -i frontend/voltz/annotate/OverlayWindow.hpp frontend/voltz/annotate/OverlayWindow.cpp frontend/voltz/annotate/AnnotationController.hpp frontend/voltz/annotate/AnnotationController.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp frontend/voltz/project/MonitorLocator.hpp frontend/voltz/project/MonitorLocator.cpp
```
Run: COMPILAR APP (segundo plano).
Expected: `exit=0`, nenhuma linha de erro.

- [ ] **Step 9: Ao vivo — redesenho parcial medido, estilo salvo (bloco de teste)**

Bloco de teste com `$ovW = 2560; $ovH = 1440` (fora do desktop virtual; escala 1 na sobreposição), passos no `try`:
```powershell
  [void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
  [void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Só tela"}}]')
  # onda de 602 pontos: 2 no InjectStroke aberto + 60 ExtendStroke de 10 pontos, em 3 lotes
  $pts = @(); for ($i = 0; $i -le 601; $i++) { $pts += ,@([int](200 + $i * 3.6), [int](700 + 200 * [Math]::Sin($i / 40.0))) }
  function Pts($list) { "[" + (($list | ForEach-Object { "[{0},{1}]" -f $_[0], $_[1] }) -join ",") + "]" }
  [void](VendorShell "InjectStroke" (Stroke-Json "caneta" "#D4F53C" 8 (Pts $pts[0..1]) -Open)); Start-Sleep -Milliseconds 300
  for ($batch = 0; $batch -lt 3; $batch++) { $reqs = @()
    for ($k = 0; $k -lt 20; $k++) { $j = $batch * 20 + $k
      $reqs += @{ requestType = "CallVendorRequest"; requestData = @{ vendorName = "voltz-teste-casca"; requestType = "ExtendStroke"; requestData = @{ pts = (Pts $pts[(2 + 10 * $j)..(11 + 10 * $j)]) } } } }
    [void](Ws (ConvertTo-Json -InputObject $reqs -Depth 6 -Compress) -Quiet); Start-Sleep -Milliseconds 300; Assert-DrawSafe "lote $batch" }
  [void](VendorShell "EndStroke")
  $probe = "[[{0},{1}],[{2},{3}]]" -f $pts[0][0], $pts[0][1], $pts[300][0], $pts[300][1]
  [void](VendorShell "ProbeOverlay" (ConvertTo-Json @{ pts = $probe } -Compress))
  $st = Shell-State; $feed = Feed-State; $ov = $st.sobreposicao
  Write-Host ("sobreposicao: {0} parciais (media {1} us, max {2} us), {3} completa(s) (max {4} us); amostras {5}" -f $ov.parciais, $ov.parcial_us_media, $ov.parcial_us_max, $ov.completas, $ov.completa_us_max, ($st.amostras -join ','))
  Write-Host ("fonte: {0} parciais, {1} completas, ultimo {2} us, max {3} us" -f $feed.parciais, $feed.completas, $feed.raster_us_ultimo, $feed.raster_us_max)
  [void](VendorShell "SetAnnotationStyle" (ConvertTo-Json @{ estilo = '{"ferr":"seta","cor":"#4DA3FF","esp":8,"somem_apos_s":2}' } -Compress))
  [void](VendorShell "InjectStroke" '{"pts":"[[400,400],[1200,800]]"}')
  $st2 = Shell-State; Assert-DrawSafe "estilo"; Write-Host "ultimo: $($st2.ultimo)"
  Start-Sleep 3; $st3 = Shell-State; Write-Host "na tela depois de 3 s: $(@($st3.visiveis).Count) (a seta sumiu; a onda tambem: terminou ha mais de 2 s)"
  Close-App
  Write-Host (Get-Content -Raw -Encoding UTF8 (Join-Path $voltzCfg "user.ini") | Select-String -Pattern 'AnnotationStyle=.*' -AllMatches).Matches.Value
```
Expected: `sobreposicao: 61 parciais (...), 1 completa(s) (...)` (o começo cria a janela = completa; cada um dos 60
acréscimos = parcial; ±1); a média parcial bem menor que o máximo completo; amostras `#FFD4F53C,#FFD4F53C`; a fonte com
`parciais` ≥ 1 e `completas` ≥ 1; `ultimo` com `"ferr":"seta","cor":"#4DA3FF","esp":8`; `na tela depois de 3 s: 0`; a
linha `AnnotationStyle={"ferr":"seta","cor":"#4DA3FF","esp":8,"somem_apos_s":2}`; `vigia: N conferencias sem problema`;
`ok: nenhuma sobreposicao ficou`. Anote os tempos no relatório da task.

- [ ] **Step 10: Regressão do 4B**

Run: SMOKE ANOTAÇÕES (`-OutDir "<scratchpad>\smoke-anotacoes"`).
Expected: última linha `SMOKE ANOTACOES OK` (as amostras do buffer na escala 0,25 e o programa continuam iguais: o
redesenho parcial é byte a byte o desenho inteiro).

- [ ] **Step 11: Commit**

```bash
git add frontend/voltz/annotate/OverlayWindow.hpp frontend/voltz/annotate/OverlayWindow.cpp frontend/voltz/annotate/AnnotationController.hpp frontend/voltz/annotate/AnnotationController.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp frontend/voltz/project/MonitorLocator.hpp frontend/voltz/project/MonitorLocator.cpp
git commit -m "feat(voltz): sobreposicao redesenha so o retangulo sujo (prcDirty) e volta ao topo; estilo das anotacoes salvo; alvo do modo desenho

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 5: `DrawInputWindow` — a janela de entrada numa thread própria (único arquivo com captura/foco) e `HotkeyBinding`

**Files:**
- Create: `frontend/voltz/annotate/DrawInputWindow.hpp`, `frontend/voltz/annotate/DrawInputWindow.cpp`
- Create: `frontend/voltz/shell/HotkeyBinding.hpp`, `frontend/voltz/shell/HotkeyBinding.cpp`
- Modify: `frontend/cmake/feature-voltz.cmake`

Sem teste automático da janela (ela captura entrada: só o roteiro manual a exercita — ver "Segurança dos testes"). A
lógica que ela segue é a `DrawSession` da Task 1. Esta task só compila e confere o código por grep.

- [ ] **Step 1: Cabeçalho da janela de entrada**

Criar `frontend/voltz/annotate/DrawInputWindow.hpp`:
```cpp
#pragma once

#include <voltz/core/cursor-track.hpp>
#include <voltz/core/draw-session.hpp>

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace voltz {

// Estado dividido entre a thread de entrada do modo desenho e a thread da UI.
struct DrawShared {
	std::atomic<int64_t> uiHeartbeatNs{0}; // a UI escreve a cada 250 ms; a thread de entrada lê no tique (vigia)
	std::mutex mutex;                      // protege os campos abaixo
	std::vector<DrawCommand> commands;     // thread de entrada → UI (DrawModeController::drain)
	bool wakePending = false;              // já há um drain a caminho da UI
	void (*wake)(void *context) = nullptr; // chamado SOB o mutex; anulado no shutdown (depois disso ninguém é acordado)
	void *wakeContext = nullptr;
	MonitorRect hole; // retângulo da barra (pixels físicos do desktop) que a janela de entrada deixa livre
};

// Thread da UI: pega os comandos que chegaram e limpa o aviso de "acordar".
std::vector<DrawCommand> takeDrawCommands(DrawShared &shared);

struct DrawInputHandle;

// A janela que RECEBE o desenho (spec §6.2, "clicável apenas em modo desenho"). Este arquivo .cpp é o ÚNICO do Voltz
// que pega o mouse ou o primeiro plano (conferido por grep; ver o Plano 4C). Janela Win32 "VoltzDrawInput" numa
// thread própria, só sobre o monitor da Tela: em camadas com alfa 1/255 (invisível, mas recebe o mouse), sempre no topo,
// fora da barra de tarefas, fora da captura quando o Windows suporta, cursor em cruz, sem dono. A thread não faz nada
// além dela: roda a DrawSession (núcleo), que decide todas as saídas — inclusive o vigia (a UI parou de pulsar por 1 s)
// —, e entrega os comandos pela fila do DrawShared sem esperar a UI. Ao sair: solta o mouse, esconde a janela, destrói e
// devolve o primeiro plano a quem o tinha (se ainda era nosso). Recusa com VOLTZ_TEST_LIVE antes de criar a thread.
class DrawInputWindow {
public:
	struct Options {
		CaptureMapping mapping;          // monitor da Tela (pixels físicos) → captura
		bool excludeFromCapture = false; // WDA_EXCLUDEFROMCAPTURE (só com SetDisplayAffinitySupported)
	};

	// Thread da UI. Cria a thread e espera a janela nascer (até 2 s); nullptr = não ligou (motivo no log).
	static std::unique_ptr<DrawInputWindow> start(std::shared_ptr<DrawShared> shared, const Options &options);
	// Pede a saída (Shutdown) e espera a thread até 2 s; se não acabar, ela fica destacada e sai sozinha.
	~DrawInputWindow();
	DrawInputWindow(const DrawInputWindow &) = delete;
	DrawInputWindow &operator=(const DrawInputWindow &) = delete;

	// Assíncronos (postam para a janela; nunca esperam a thread de entrada).
	void requestExit(DrawExit reason);
	void holeChanged();
	// Espera a thread terminar (até timeoutMs). Falso = ainda viva.
	bool wait(int timeoutMs);
	// A janela ficou em primeiro plano ao nascer (pelo botão "Desenhar"); pelo atalho, o primeiro clique a ativa.
	bool activated() const;

private:
	DrawInputWindow() = default;

	std::shared_ptr<DrawInputHandle> handle_;
	std::thread thread_;
	std::future<void> finished_;
};

} // namespace voltz
```

- [ ] **Step 2: Implementação da janela de entrada**

Criar `frontend/voltz/annotate/DrawInputWindow.cpp`:
```cpp
#include "DrawInputWindow.hpp"

#include <voltz/core/live-protocol.hpp>

#include <util/base.h>
#include <util/platform.h>
#include <util/threading.h>

#include <chrono>
#include <iterator>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#endif

namespace voltz {

struct DrawInputHandle {
	std::atomic<void *> hwnd{nullptr}; // HWND da janela de entrada (nulo antes de nascer e depois de destruída)
	std::atomic<bool> cancel{false};   // start() desistiu: a thread sai logo depois de criar a janela
	std::atomic<bool> activated{false};
};

std::vector<DrawCommand> takeDrawCommands(DrawShared &shared)
{
	std::lock_guard<std::mutex> lock(shared.mutex);
	shared.wakePending = false;
	std::vector<DrawCommand> out;
	out.swap(shared.commands);
	return out;
}

#ifdef _WIN32
namespace {

constexpr UINT kExitMessage = WM_APP + 1; // wParam = DrawExit
constexpr UINT kHoleMessage = WM_APP + 2;
constexpr UINT_PTR kTickTimer = 1;
constexpr UINT kTickMs = 100;
const wchar_t *kInputClass = L"VoltzDrawInput";

int64_t nowNs()
{
	return static_cast<int64_t>(os_gettime_ns());
}

// Tudo que a janela precisa, só na thread de entrada.
struct InputState {
	InputState(std::shared_ptr<DrawShared> sharedState, const CaptureMapping &mapping)
		: shared(std::move(sharedState)),
		  session(mapping, nowNs()),
		  rect(mapping.monitor)
	{
	}

	std::shared_ptr<DrawShared> shared;
	DrawSession session;
	MonitorRect rect;
	HWND hwnd = nullptr;
	HWND previousForeground = nullptr;
	bool capturing = false;
	bool releasing = false;
	bool foregroundAtExit = false;
};

void releaseMouse(InputState &state)
{
	if (!state.capturing) {
		return;
	}
	state.releasing = true;
	ReleaseCapture();
	state.releasing = false;
	state.capturing = false;
}

// Entrega os comandos à UI sem esperar. Se a sessão acabou (Exit é sempre o último): solta o mouse, esconde a janela e
// encerra o laço desta thread — tudo aqui, sem depender da UI.
void deliver(InputState &state, std::vector<DrawCommand> commands)
{
	if (commands.empty()) {
		return;
	}
	if (commands.back().type == DrawCommandType::Exit && state.hwnd) {
		state.foregroundAtExit = GetForegroundWindow() == state.hwnd;
		releaseMouse(state);
		ShowWindow(state.hwnd, SW_HIDE);
		PostQuitMessage(0);
	}
	std::lock_guard<std::mutex> lock(state.shared->mutex);
	std::vector<DrawCommand> &queue = state.shared->commands;
	queue.insert(queue.end(), std::make_move_iterator(commands.begin()), std::make_move_iterator(commands.end()));
	if (!state.shared->wakePending && state.shared->wake) {
		state.shared->wakePending = true;
		state.shared->wake(state.shared->wakeContext);
	}
}

DrawInput eventAt(DrawInputKind kind)
{
	DrawInput input;
	input.kind = kind;
	input.tNs = nowNs();
	return input;
}

// Coordenadas do cliente (pixels físicos; a janela começa no canto do monitor) → desktop.
DrawInput pointerAt(DrawInputKind kind, const InputState &state, LPARAM lParam)
{
	DrawInput input = eventAt(kind);
	input.x = state.rect.left + GET_X_LPARAM(lParam);
	input.y = state.rect.top + GET_Y_LPARAM(lParam);
	return input;
}

// Região da janela = o monitor inteiro menos o retângulo da barra (o clique ali vai para a barra, que é de outra
// thread: HTTRANSPARENT não serviria).
void applyHole(InputState &state)
{
	MonitorRect hole;
	{
		std::lock_guard<std::mutex> lock(state.shared->mutex);
		hole = state.shared->hole;
	}
	HRGN region = CreateRectRgn(0, 0, state.rect.width, state.rect.height);
	if (!region) {
		return;
	}
	if (hole.width > 0 && hole.height > 0) {
		const int left = hole.left - state.rect.left;
		const int top = hole.top - state.rect.top;
		HRGN cut = CreateRectRgn(left, top, left + hole.width, top + hole.height);
		if (cut) {
			CombineRgn(region, region, cut, RGN_DIFF);
			DeleteObject(cut);
		}
	}
	if (!SetWindowRgn(state.hwnd, region, TRUE)) {
		DeleteObject(region); /* deu certo = a região passa a ser do sistema */
	}
}

LRESULT CALLBACK inputProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto *state = reinterpret_cast<InputState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (!state) {
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}
	switch (message) {
	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT) {
			SetCursor(LoadCursorW(nullptr, IDC_CROSS));
			return TRUE;
		}
		break;
	case WM_LBUTTONDOWN:
		/* o mouse fica com esta janela só entre apertar e soltar (arrastar para fora do monitor continua o traço) */
		if (!state->capturing) {
			SetCapture(hwnd);
			state->capturing = true;
		}
		deliver(*state, state->session.handle(pointerAt(DrawInputKind::LeftDown, *state, lParam)));
		return 0;
	case WM_MOUSEMOVE:
		deliver(*state, state->session.handle(pointerAt(DrawInputKind::Move, *state, lParam)));
		return 0;
	case WM_LBUTTONUP:
		deliver(*state, state->session.handle(pointerAt(DrawInputKind::LeftUp, *state, lParam)));
		releaseMouse(*state);
		return 0;
	case WM_RBUTTONDOWN:
		deliver(*state, state->session.handle(pointerAt(DrawInputKind::RightDown, *state, lParam)));
		return 0;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN: {
		DrawInputKind kind = DrawInputKind::OtherKey;
		if (wParam == VK_ESCAPE) {
			kind = DrawInputKind::Escape;
		} else if (wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
			kind = DrawInputKind::Undo;
		}
		deliver(*state, state->session.handle(eventAt(kind)));
		if (message == WM_SYSKEYDOWN) {
			break; /* Alt+F4: o DefWindowProc manda WM_CLOSE, que também sai */
		}
		return 0;
	}
	case WM_ACTIVATE:
		deliver(*state, state->session.handle(eventAt(LOWORD(wParam) == WA_INACTIVE ? DrawInputKind::Deactivated
											       : DrawInputKind::Activated)));
		break; /* o DefWindowProc põe o foco do teclado na janela ativada: Esc e Ctrl+Z chegam aqui */
	case WM_CANCELMODE:
		deliver(*state, state->session.handle(eventAt(DrawInputKind::CaptureLost)));
		break; /* o DefWindowProc solta a captura */
	case WM_CAPTURECHANGED:
		if (!state->releasing && reinterpret_cast<HWND>(lParam) != hwnd) {
			state->capturing = false;
			deliver(*state, state->session.handle(eventAt(DrawInputKind::CaptureLost)));
		}
		return 0;
	case WM_TIMER:
		if (wParam == kTickTimer) {
			DrawInput input = eventAt(DrawInputKind::Tick);
			input.uiHeartbeatNs = state->shared->uiHeartbeatNs.load();
			deliver(*state, state->session.handle(input));
			return 0;
		}
		break;
	case WM_DISPLAYCHANGE:
		deliver(*state, state->session.handle(eventAt(DrawInputKind::TargetChanged)));
		return 0;
	case WM_CLOSE:
		deliver(*state, state->session.handle(eventAt(DrawInputKind::Close)));
		return 0;
	case WM_ENDSESSION:
		if (wParam) {
			deliver(*state, state->session.requestExit(DrawExit::Shutdown));
		}
		return 0;
	case WM_DPICHANGED:
		return 0; /* o retângulo é sempre o do monitor, em pixels físicos */
	case kExitMessage:
		deliver(*state, state->session.requestExit(static_cast<DrawExit>(wParam)));
		return 0;
	case kHoleMessage:
		applyHole(*state);
		return 0;
	default:
		break;
	}
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool registerInputClass()
{
	static const bool registered = []() {
		WNDCLASSEXW windowClass = {};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = inputProc;
		windowClass.hInstance = GetModuleHandleW(nullptr);
		windowClass.lpszClassName = kInputClass;
		return RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	}();
	return registered;
}

void inputThreadMain(std::shared_ptr<DrawShared> shared, DrawInputWindow::Options options,
		     std::shared_ptr<DrawInputHandle> handle, std::promise<bool> ready, std::promise<void> done)
{
	os_set_thread_name("voltz-draw-input");
	/* pixels físicos em qualquer escala, como o resto do Voltz (MonitorLocator, sobreposição) */
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	InputState state(std::move(shared), options.mapping);
	const MonitorRect rect = state.rect;
	state.previousForeground = GetForegroundWindow();
	HWND hwnd = nullptr;
	if (registerInputClass()) {
		/* Sem os bits de "transparente a cliques" e "não ativar", de propósito: é a janela que recebe o desenho. Sem
		 * dono: uma dona de outra thread ligaria as filas de entrada das duas threads. */
		hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kInputClass, L"Voltz Desenho",
				       WS_POPUP, rect.left, rect.top, rect.width, rect.height, nullptr, nullptr,
				       GetModuleHandleW(nullptr), nullptr);
	}
	if (!hwnd) {
		blog(LOG_WARNING, "[voltz] modo desenho: CreateWindowEx falhou (%lu)", GetLastError());
		ready.set_value(false);
		done.set_value();
		return;
	}
	state.hwnd = hwnd;
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
	/* alfa 1/255: invisível, mas recebe o mouse (um pixel de alfa 0 deixaria o clique passar) */
	SetLayeredWindowAttributes(hwnd, 0, 1, LWA_ALPHA);
	if (options.excludeFromCapture) {
		SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
	}
	applyHole(state);
	SetTimer(hwnd, kTickTimer, kTickMs, nullptr);
	handle->hwnd.store(hwnd);
	ShowWindow(hwnd, SW_SHOW);
	/* Só consegue se o Voltz já está em primeiro plano (botão "Desenhar"); pelo atalho, o primeiro clique ativa. */
	const bool foreground = SetForegroundWindow(hwnd) != FALSE || GetForegroundWindow() == hwnd;
	handle->activated.store(foreground);
	ready.set_value(true);
	if (handle->cancel.load()) {
		deliver(state, state.session.requestExit(DrawExit::Shutdown));
	}

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
		DispatchMessageW(&msg);
	}

	/* Fim, tudo nesta thread (nada espera a UI). */
	if (state.session.active()) {
		deliver(state, state.session.requestExit(DrawExit::Shutdown));
	}
	releaseMouse(state);
	KillTimer(hwnd, kTickTimer);
	handle->hwnd.store(nullptr);
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
	DestroyWindow(hwnd);
	state.hwnd = nullptr;
	/* Devolve o primeiro plano a quem o tinha, só se ainda era nosso na saída (Esc, atalho, vigia...). Na perda de
	 * ativação o usuário já foi para outra janela: não mexe. */
	if (state.foregroundAtExit && state.previousForeground && IsWindow(state.previousForeground)) {
		SetForegroundWindow(state.previousForeground);
	}
	done.set_value();
}

} // namespace
#endif

std::unique_ptr<DrawInputWindow> DrawInputWindow::start(std::shared_ptr<DrawShared> shared, const Options &options)
{
	/* Defesa em profundidade: o DrawModeController já recusa com VOLTZ_TEST_LIVE; aqui nem a thread nasce. */
	if (live::testModeEnabled()) {
		blog(LOG_WARNING, "[voltz] modo desenho: janela de entrada recusada (%s)", live::kTestEnv);
		return nullptr;
	}
#ifdef _WIN32
	const CaptureMapping &mapping = options.mapping;
	if (!shared || mapping.monitor.width <= 0 || mapping.monitor.height <= 0 || mapping.captureWidth <= 0 ||
	    mapping.captureHeight <= 0) {
		return nullptr;
	}
	std::unique_ptr<DrawInputWindow> window(new DrawInputWindow());
	window->handle_ = std::make_shared<DrawInputHandle>();
	std::promise<bool> ready;
	std::future<bool> started = ready.get_future();
	std::promise<void> done;
	window->finished_ = done.get_future();
	window->thread_ = std::thread(inputThreadMain, std::move(shared), options, window->handle_, std::move(ready),
				      std::move(done));
	if (started.wait_for(std::chrono::seconds(2)) != std::future_status::ready || !started.get()) {
		blog(LOG_WARNING, "[voltz] modo desenho: a janela de entrada nao nasceu");
		window->handle_->cancel.store(true);
		return nullptr; /* o destrutor pede a saída e espera */
	}
	return window;
#else
	(void)shared;
	(void)options;
	return nullptr;
#endif
}

DrawInputWindow::~DrawInputWindow()
{
	requestExit(DrawExit::Shutdown);
	if (!wait(2000) && thread_.joinable()) {
		handle_->cancel.store(true);
		blog(LOG_WARNING, "[voltz] modo desenho: a thread de entrada nao terminou em 2 s (destacada)");
		thread_.detach();
	}
}

void DrawInputWindow::requestExit(DrawExit reason)
{
#ifdef _WIN32
	void *hwnd = handle_ ? handle_->hwnd.load() : nullptr;
	if (hwnd) {
		PostMessageW(static_cast<HWND>(hwnd), kExitMessage, static_cast<WPARAM>(reason), 0);
	}
#else
	(void)reason;
#endif
}

void DrawInputWindow::holeChanged()
{
#ifdef _WIN32
	void *hwnd = handle_ ? handle_->hwnd.load() : nullptr;
	if (hwnd) {
		PostMessageW(static_cast<HWND>(hwnd), kHoleMessage, 0, 0);
	}
#endif
}

bool DrawInputWindow::wait(int timeoutMs)
{
	if (!thread_.joinable()) {
		return true;
	}
	if (finished_.valid() &&
	    finished_.wait_for(std::chrono::milliseconds(timeoutMs)) != std::future_status::ready) {
		return false;
	}
	thread_.join();
	return true;
}

bool DrawInputWindow::activated() const
{
	return handle_ && handle_->activated.load();
}

} // namespace voltz
```

- [ ] **Step 3: `HotkeyBinding`**

Criar `frontend/voltz/shell/HotkeyBinding.hpp`:
```cpp
#pragma once

#include <obs.h>

#include <QString>

namespace voltz {

// Teclas do atalho `name` a partir do perfil, no formato do OBS ([Hotkeys] <name> = {"bindings":[...]}, o mesmo do
// OBSBasic::InitHotkeys / OBSBasicSettings::SaveHotkeySettings). Nada salvo: Ctrl+Shift+`defaultKey` (ex.
// "OBS_KEY_D"), ou sem tecla se `defaultKey` é nulo. Lista vazia salva = sem atalho. Thread da UI.
void loadHotkeyBinding(obs_hotkey_id id, const char *name, const char *defaultKey);

// Teclas do atalho em texto ("Ctrl + Shift + D"; vários separados por ", "); vazio sem atalho.
QString hotkeyShortcutText(obs_hotkey_id id);

} // namespace voltz
```
Criar `frontend/voltz/shell/HotkeyBinding.cpp`:
```cpp
#include "HotkeyBinding.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>
#include <util/config-file.h>
#include <util/dstr.h>

#include <QStringList>

namespace voltz {

void loadHotkeyBinding(obs_hotkey_id id, const char *name, const char *defaultKey)
{
	if (id == OBS_INVALID_HOTKEY_ID) {
		return;
	}
	config_t *profile = obs_frontend_get_profile_config();
	const char *json = profile ? config_get_string(profile, "Hotkeys", name) : nullptr;
	OBSDataArrayAutoRelease bindings;
	if (json && *json) {
		OBSDataAutoRelease data = obs_data_create_from_json(json);
		if (data) {
			bindings = obs_data_get_array(data, "bindings");
		}
	} else {
		bindings = obs_data_array_create();
		if (defaultKey) {
			OBSDataAutoRelease combo = obs_data_create();
			obs_data_set_bool(combo, "control", true);
			obs_data_set_bool(combo, "shift", true);
			obs_data_set_string(combo, "key", defaultKey);
			obs_data_array_push_back(bindings, combo);
		}
	}
	obs_hotkey_load(id, bindings);
}

QString hotkeyShortcutText(obs_hotkey_id id)
{
	struct Search {
		obs_hotkey_id id;
		QStringList keys;
	} search{id, {}};
	if (id == OBS_INVALID_HOTKEY_ID) {
		return QString();
	}
	obs_enum_hotkey_bindings(
		[](void *data, size_t, obs_hotkey_binding_t *binding) {
			auto *found = static_cast<Search *>(data);
			if (obs_hotkey_binding_get_hotkey_id(binding) == found->id) {
				struct dstr text = {};
				obs_key_combination_to_str(obs_hotkey_binding_get_key_combination(binding), &text);
				if (text.array && *text.array) {
					found->keys.append(QString::fromUtf8(text.array));
				}
				dstr_free(&text);
			}
			return true;
		},
		&search);
	return search.keys.join(QStringLiteral(", "));
}

} // namespace voltz
```
(`MouseStyleStore` e `ProjectRecorder` seguem com o código deles; migrar para este auxiliar fica para o 4D, junto com
Ajustes › Atalhos.)

- [ ] **Step 4: Compilar**

Em `frontend/cmake/feature-voltz.cmake`, na lista de `target_sources(obs-studio PRIVATE ...)`: depois de
`    voltz/annotate/AnnotationTestVendor.hpp`, acrescentar
```cmake
    voltz/annotate/DrawInputWindow.cpp
    voltz/annotate/DrawInputWindow.hpp
```
e depois de `    voltz/shell/FrontendBridge.hpp`, acrescentar
```cmake
    voltz/shell/HotkeyBinding.cpp
    voltz/shell/HotkeyBinding.hpp
```
```bash
"$CLANGFMT" -i frontend/voltz/annotate/DrawInputWindow.hpp frontend/voltz/annotate/DrawInputWindow.cpp frontend/voltz/shell/HotkeyBinding.hpp frontend/voltz/shell/HotkeyBinding.cpp
```
Run: CONFIGURAR; COMPILAR APP (segundo plano).
Expected: `exit=0`, nenhuma linha de erro. (Se `SetThreadDpiAwarenessContext`/`DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`
não forem declarados, o `WINVER` do alvo está abaixo de `0x0605`: pare e reporte — não defina `WINVER` à mão.)

- [ ] **Step 5: Grep de segurança (obrigatório)**

Run: GREP DE SEGURANÇA (Convenções).
Expected: `foco-fora-da-excecao=1`; `injecao=1`; C lista só `CursorTracker.cpp` e `LiveMouse.cpp`; D mostra
`1 ReleaseCapture(`, `1 SetCapture(`, `2 SetForegroundWindow(`. Conferir também que o arquivo de exceção recusa o modo
de teste antes de criar a thread e não registra Raw Input nem gancho:
```bash
grep -n "testModeEnabled" frontend/voltz/annotate/DrawInputWindow.cpp
grep -nE "RegisterRawInputDevices|SetWindowsHookEx|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT" frontend/voltz/annotate/DrawInputWindow.cpp; echo "achados=$?"
```
Expected: uma linha com `if (live::testModeEnabled()) {` dentro de `DrawInputWindow::start`, **antes** de
`std::thread`; a segunda busca sem nenhuma linha e `achados=1`.

- [ ] **Step 6: Commit**

```bash
git add frontend/voltz/annotate/DrawInputWindow.hpp frontend/voltz/annotate/DrawInputWindow.cpp frontend/voltz/shell/HotkeyBinding.hpp frontend/voltz/shell/HotkeyBinding.cpp frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): janela de entrada do modo desenho em thread propria (saidas pela DrawSession, vigia local, recusa em teste)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 6: `DrawToolbar` — barra do modo desenho (sem foco, fora da captura), ícones, textos e pedido `ToolbarImage`

**Files:**
- Create: `frontend/voltz/annotate/DrawToolbar.hpp`, `frontend/voltz/annotate/DrawToolbar.cpp`
- Create: 9 SVGs em `frontend/voltz/shell/icons/regular/` e 7 em `frontend/voltz/shell/icons/fill/` (Phosphor 2.1.1)
- Modify: `frontend/voltz/shell/voltz-shell.qrc`, `frontend/cmake/feature-voltz.cmake`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`
- Modify: `frontend/voltz/annotate/AnnotationTestVendor.hpp`, `frontend/voltz/annotate/AnnotationTestVendor.cpp`

- [ ] **Step 1: Ícones oficiais (Phosphor 2.1.1)**

```bash
base="https://unpkg.com/@phosphor-icons/core@2.1.1/assets"
for n in pen highlighter-circle line-segment arrow-up-right rectangle circle arrow-counter-clockwise palette timer; do
  curl -fsSL "$base/regular/$n.svg" -o "frontend/voltz/shell/icons/regular/$n.svg" || echo "FALHOU $n"
done
for n in pen highlighter-circle line-segment arrow-up-right rectangle circle pencil-simple; do
  curl -fsSL "$base/fill/$n-fill.svg" -o "frontend/voltz/shell/icons/fill/$n-fill.svg" || echo "FALHOU $n-fill"
done
head -c 80 frontend/voltz/shell/icons/regular/pen.svg; echo; head -c 80 frontend/voltz/shell/icons/fill/pencil-simple-fill.svg; echo
```
Expected: nenhum `FALHOU`; os dois começam com `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" fill="currentColor">`.
Em `frontend/voltz/shell/voltz-shell.qrc`, logo antes de `  </qresource>`, acrescentar:
```xml
    <file>icons/regular/pen.svg</file>
    <file>icons/regular/highlighter-circle.svg</file>
    <file>icons/regular/line-segment.svg</file>
    <file>icons/regular/arrow-up-right.svg</file>
    <file>icons/regular/rectangle.svg</file>
    <file>icons/regular/circle.svg</file>
    <file>icons/regular/arrow-counter-clockwise.svg</file>
    <file>icons/regular/palette.svg</file>
    <file>icons/regular/timer.svg</file>
    <file>icons/fill/pen-fill.svg</file>
    <file>icons/fill/highlighter-circle-fill.svg</file>
    <file>icons/fill/line-segment-fill.svg</file>
    <file>icons/fill/arrow-up-right-fill.svg</file>
    <file>icons/fill/rectangle-fill.svg</file>
    <file>icons/fill/circle-fill.svg</file>
    <file>icons/fill/pencil-simple-fill.svg</file>
```
Conferir: `grep -c "<file>" frontend/voltz/shell/voltz-shell.qrc` → `100`.

- [ ] **Step 2: Textos (casca, os dois locales)**

No fim de `frontend/data/locale/voltz-pt-BR.ini`, acrescentar:
```ini
Voltz.Draw.Toolbar="Barra de desenho"
Voltz.Draw.Tool.Pen="Caneta"
Voltz.Draw.Tool.Highlighter="Marca-texto"
Voltz.Draw.Tool.Line="Linha"
Voltz.Draw.Tool.Arrow="Seta"
Voltz.Draw.Tool.Rectangle="Retângulo"
Voltz.Draw.Tool.Ellipse="Elipse"
Voltz.Draw.Color="Cor %1"
Voltz.Draw.MoreColors="Mais cores"
Voltz.Draw.Width="Espessura %1 px"
Voltz.Draw.Undo="Desfazer (Ctrl+Z)"
Voltz.Draw.Clear="Limpar tudo"
Voltz.Draw.Fade="Traços somem após: %1"
Voltz.Draw.Fade.Never="nunca"
Voltz.Draw.Fade.Seconds="%1 s"
Voltz.Draw.Exit="Sair do modo desenho (Esc, clique direito ou %1)"
Voltz.Draw.Burned="Neste Windows as anotações aparecem também na gravação da tela (tela.mkv): ele não consegue escondê-las da captura."
```
No fim de `frontend/data/locale/voltz-en-US.ini`, acrescentar:
```ini
Voltz.Draw.Toolbar="Drawing toolbar"
Voltz.Draw.Tool.Pen="Pen"
Voltz.Draw.Tool.Highlighter="Highlighter"
Voltz.Draw.Tool.Line="Line"
Voltz.Draw.Tool.Arrow="Arrow"
Voltz.Draw.Tool.Rectangle="Rectangle"
Voltz.Draw.Tool.Ellipse="Ellipse"
Voltz.Draw.Color="Color %1"
Voltz.Draw.MoreColors="More colors"
Voltz.Draw.Width="Width %1 px"
Voltz.Draw.Undo="Undo (Ctrl+Z)"
Voltz.Draw.Clear="Clear all"
Voltz.Draw.Fade="Strokes fade after: %1"
Voltz.Draw.Fade.Never="never"
Voltz.Draw.Fade.Seconds="%1 s"
Voltz.Draw.Exit="Exit draw mode (Esc, right-click or %1)"
Voltz.Draw.Burned="On this Windows the annotations also show up in the screen recording (tela.mkv): it cannot hide them from capture."
```
Conferir a paridade (Convenções) → sem saída.

- [ ] **Step 3: Cabeçalho da barra**

Criar `frontend/voltz/annotate/DrawToolbar.hpp`:
```cpp
#pragma once

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/cursor-track.hpp>

#include <QList>
#include <QPair>
#include <QWidget>

class QButtonGroup;
class QFrame;
class QLabel;
class QToolButton;

namespace voltz {

// Barra do modo desenho (spec §6.2): caneta, marca-texto, linha, seta, retângulo, elipse · 4 cores rápidas + 8 em "mais
// cores" · 4 espessuras · desfazer · limpar · "traços somem após" (nunca → 3 → 5 → 10 s) · sair. Janela Qt própria (sem
// pai: nada de dono de outra thread), sempre no topo, que NUNCA pega o foco (Qt::WindowDoesNotAcceptFocus +
// WS_EX_NOACTIVATE): a janela de entrada do desenho continua ativa e o Esc continua chegando nela. Com a propriedade
// isOBSProjectorWindow o OBSApp::notify não mexe na afinidade dela; fora da captura por WDA_EXCLUDEFROMCAPTURE quando o
// Windows suporta. Aviso visível quando as anotações estão "queimadas" (spec §6.3). A janela nativa só nasce no primeiro
// showOnScreen — o grab() do pedido de teste ToolbarImage não cria janela. Título nativo "VoltzDrawToolbar" (os testes a
// procuram para garantir que ela nunca aparece num teste automático). Thread da UI.
class DrawToolbar : public QWidget {
	Q_OBJECT

public:
	explicit DrawToolbar(QWidget *parent = nullptr);

	// Marca ferramenta, cor, espessura e "somem após" (não emite sinais).
	void setAnnotationStyle(const AnnotationStyle &style);
	void setBurned(bool burned);
	// Texto do atalho de desenhar na dica do botão Sair.
	void setExitHint(const QString &shortcut);
	// Mostra no alto e ao centro do monitor `screenName` (nome GDI, QScreen::name()), sem ativar.
	void showOnScreen(const QString &screenName, bool excludeFromCapture);
	// Põe de novo por cima (a janela de entrada acabou de ir para o topo), sem ativar.
	void raiseWithoutActivating();
	// Retângulo da janela em pixels físicos do desktop (o buraco na janela de entrada); vazio se não está visível.
	MonitorRect nativeRect() const;

signals:
	void toolChosen(voltz::AnnotationTool tool);
	void colorChosen(voltz::RgbColor color);
	void widthChosen(int width);
	void fadeChosen(int seconds);
	void undoClicked();
	void clearClicked();
	void exitClicked();
	void nativeRectChanged();

protected:
	void moveEvent(QMoveEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	QToolButton *makeButton(QWidget *parent, const QString &hint);
	QToolButton *addColor(const RgbColor &color);
	QFrame *separator();
	void markColor(const RgbColor &color);
	void updateFadeText();

	QFrame *panel_ = nullptr;
	QButtonGroup *tools_ = nullptr;
	QList<QPair<QToolButton *, RgbColor>> colors_;
	QList<QPair<QToolButton *, int>> widths_;
	QToolButton *moreColors_ = nullptr;
	QWidget *extraColors_ = nullptr;
	QToolButton *fade_ = nullptr;
	QToolButton *exit_ = nullptr;
	QLabel *burned_ = nullptr;
	AnnotationStyle style_;
};

} // namespace voltz
```

- [ ] **Step 4: Implementação da barra**

Criar `frontend/voltz/annotate/DrawToolbar.cpp`:
```cpp
#include "DrawToolbar.hpp"

#include <voltz/shell/Icons.hpp>

#include <OBSApp.hpp> // QTStr

#include <QAbstractButton>
#include <QButtonGroup>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMoveEvent>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <iterator>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "moc_DrawToolbar.cpp"

namespace voltz {

namespace {

constexpr int kTopMargin = 12;
constexpr int kFadeSteps[] = {0, 3, 5, 10};
constexpr int kWidths[] = {2, 4, 8, 16};
const RgbColor kQuickColors[] = {{0xD4, 0xF5, 0x3C}, {0xFF, 0x4D, 0x4D}, {0x4D, 0xA3, 0xFF}, {0xFF, 0xE1, 0x4D}};
const RgbColor kMoreColors[] = {{0xFF, 0xFF, 0xFF}, {0x09, 0x09, 0x0B}, {0xFF, 0x9F, 0x43}, {0x2E, 0xD5, 0x73},
				{0xA5, 0x5E, 0xEA}, {0xFF, 0x6B, 0x9D}, {0x22, 0xD3, 0xEE}, {0x9C, 0xA3, 0xAF}};

struct ToolEntry {
	AnnotationTool tool;
	const char *icon;
	const char *text;
};

const ToolEntry kTools[] = {
	{AnnotationTool::Pen, "pen", "Voltz.Draw.Tool.Pen"},
	{AnnotationTool::Highlighter, "highlighter-circle", "Voltz.Draw.Tool.Highlighter"},
	{AnnotationTool::Line, "line-segment", "Voltz.Draw.Tool.Line"},
	{AnnotationTool::Arrow, "arrow-up-right", "Voltz.Draw.Tool.Arrow"},
	{AnnotationTool::Rectangle, "rectangle", "Voltz.Draw.Tool.Rectangle"},
	{AnnotationTool::Ellipse, "circle", "Voltz.Draw.Tool.Ellipse"},
};

// QSS próprio: a barra é uma janela fora do #voltzShell e o tema global do OBS tem regras de QToolButton (altura máxima,
// cores) — tudo leva o prefixo #voltzDrawToolbar.
const char *kStyleSheet = R"(
#voltzDrawToolbar QFrame#drawPanel {
	background: #111114;
	border: 1px solid #2A2A30;
	border-radius: 12px;
}
#voltzDrawToolbar QToolButton {
	background: transparent;
	color: #F2F2F3;
	border: none;
	border-radius: 8px;
	padding: 0;
	margin: 0;
	min-width: 32px;
	min-height: 32px;
	max-height: 32px;
	font-family: "DM Sans", "Segoe UI";
	font-size: 12px;
}
#voltzDrawToolbar QToolButton:hover {
	background: #1F1F24;
}
#voltzDrawToolbar QToolButton:checked {
	background: rgba(212, 245, 60, 40);
}
#voltzDrawToolbar QToolButton#fadeButton {
	padding: 0 8px;
}
#voltzDrawToolbar QFrame#drawSeparator {
	background: #2A2A30;
	border: none;
	margin: 6px 4px;
}
#voltzDrawToolbar QLabel#drawBurned {
	background: transparent;
	color: #F5C83C;
	font-family: "DM Sans", "Segoe UI";
	font-size: 12px;
}
)";

QColor toQColor(const RgbColor &color)
{
	return QColor(color.r, color.g, color.b);
}

// Bolinha da cor (20 px lógicos desenhados a 2×); a escolhida ganha um anel lima por fora.
QIcon swatchIcon(const RgbColor &color, bool selected)
{
	QPixmap pixmap(40, 40);
	pixmap.setDevicePixelRatio(2.0);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	if (selected) {
		painter.setPen(QPen(palette::kAccent, 2.0));
		painter.setBrush(Qt::NoBrush);
		painter.drawEllipse(QRectF(1.0, 1.0, 18.0, 18.0));
	}
	painter.setPen(QPen(QColor(0x3A, 0x3A, 0x42), 1.0));
	painter.setBrush(toQColor(color));
	painter.drawEllipse(QRectF(4.5, 4.5, 11.0, 11.0));
	return QIcon(pixmap);
}

// Ponto do tamanho da espessura (3 a 15 px lógicos).
QIcon widthIcon(int width)
{
	QPixmap pixmap(40, 40);
	pixmap.setDevicePixelRatio(2.0);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	painter.setBrush(palette::kText);
	const double diameter = std::clamp(2.0 + width * 0.8, 3.0, 15.0);
	painter.drawEllipse(QRectF(10.0 - diameter / 2.0, 10.0 - diameter / 2.0, diameter, diameter));
	return QIcon(pixmap);
}

} // namespace

DrawToolbar::DrawToolbar(QWidget *parent)
	: QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus |
				  Qt::NoDropShadowWindowHint)
{
	setObjectName("voltzDrawToolbar");
	/* os testes procuram este título: a barra nunca pode estar visível num teste automático */
	setWindowTitle(QStringLiteral("VoltzDrawToolbar"));
	setAccessibleName(QTStr("Voltz.Draw.Toolbar"));
	setAttribute(Qt::WA_ShowWithoutActivating);
	setAttribute(Qt::WA_TranslucentBackground);
	setFocusPolicy(Qt::NoFocus);
	setStyleSheet(QString::fromUtf8(kStyleSheet));

	panel_ = new QFrame(this);
	panel_->setObjectName("drawPanel");
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->addWidget(panel_);
	auto *column = new QVBoxLayout(panel_);
	column->setContentsMargins(8, 6, 8, 6);
	column->setSpacing(4);
	auto *row = new QHBoxLayout();
	row->setSpacing(2);
	column->addLayout(row);

	tools_ = new QButtonGroup(this);
	tools_->setExclusive(true);
	for (const ToolEntry &entry : kTools) {
		QToolButton *button = makeButton(panel_, QTStr(entry.text));
		button->setCheckable(true);
		button->setIcon(phosphorToggleIcon(QString::fromLatin1(entry.icon), palette::kText, palette::kAccent));
		tools_->addButton(button, static_cast<int>(entry.tool));
		row->addWidget(button);
	}
	connect(tools_, &QButtonGroup::idClicked, this,
		[this](int id) { emit toolChosen(static_cast<AnnotationTool>(id)); });
	row->addWidget(separator());

	for (const RgbColor &color : kQuickColors) {
		row->addWidget(addColor(color));
	}
	moreColors_ = makeButton(panel_, QTStr("Voltz.Draw.MoreColors"));
	moreColors_->setCheckable(true);
	moreColors_->setIcon(phosphorIcon(QStringLiteral("palette"), palette::kText));
	row->addWidget(moreColors_);
	row->addWidget(separator());

	for (int width : kWidths) {
		QToolButton *button = makeButton(panel_, QTStr("Voltz.Draw.Width").arg(width));
		button->setCheckable(true);
		button->setIcon(widthIcon(width));
		connect(button, &QToolButton::clicked, this, [this, width]() { emit widthChosen(width); });
		widths_.append(qMakePair(button, width));
		row->addWidget(button);
	}
	row->addWidget(separator());

	QToolButton *undo = makeButton(panel_, QTStr("Voltz.Draw.Undo"));
	undo->setIcon(phosphorIcon(QStringLiteral("arrow-counter-clockwise"), palette::kText));
	connect(undo, &QToolButton::clicked, this, [this]() { emit undoClicked(); });
	row->addWidget(undo);
	QToolButton *clear = makeButton(panel_, QTStr("Voltz.Draw.Clear"));
	clear->setIcon(phosphorIcon(QStringLiteral("trash"), palette::kText));
	connect(clear, &QToolButton::clicked, this, [this]() { emit clearClicked(); });
	row->addWidget(clear);
	row->addWidget(separator());

	fade_ = makeButton(panel_, QString());
	fade_->setObjectName("fadeButton");
	fade_->setIcon(phosphorIcon(QStringLiteral("timer"), palette::kText));
	fade_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	connect(fade_, &QToolButton::clicked, this, [this]() {
		/* nunca → 3 s → 5 s → 10 s → nunca (valor fora da lista volta ao começo) */
		const int *current = std::find(std::begin(kFadeSteps), std::end(kFadeSteps), style_.fadeSeconds);
		const int *next = (current == std::end(kFadeSteps) || current + 1 == std::end(kFadeSteps))
					  ? std::begin(kFadeSteps)
					  : current + 1;
		emit fadeChosen(*next);
	});
	row->addWidget(fade_);
	row->addWidget(separator());

	exit_ = makeButton(panel_, QString());
	exit_->setIcon(phosphorIcon(QStringLiteral("x"), palette::kText));
	connect(exit_, &QToolButton::clicked, this, [this]() { emit exitClicked(); });
	row->addWidget(exit_);
	setExitHint(QStringLiteral("Ctrl + Shift + D"));

	extraColors_ = new QWidget(panel_);
	auto *extraRow = new QHBoxLayout(extraColors_);
	extraRow->setContentsMargins(0, 0, 0, 0);
	extraRow->setSpacing(2);
	for (const RgbColor &color : kMoreColors) {
		extraRow->addWidget(addColor(color));
	}
	extraRow->addStretch(1);
	extraColors_->hide();
	column->addWidget(extraColors_);
	connect(moreColors_, &QToolButton::toggled, this, [this](bool on) {
		extraColors_->setVisible(on);
		adjustSize();
	});

	burned_ = new QLabel(QTStr("Voltz.Draw.Burned"), panel_);
	burned_->setObjectName("drawBurned");
	burned_->setWordWrap(true);
	burned_->hide();
	column->addWidget(burned_);

	setAnnotationStyle(AnnotationStyle{});
}

QToolButton *DrawToolbar::makeButton(QWidget *parent, const QString &hint)
{
	auto *button = new QToolButton(parent);
	button->setFocusPolicy(Qt::NoFocus);
	button->setIconSize(QSize(20, 20));
	button->setAutoRaise(true);
	if (!hint.isEmpty()) {
		setButtonHint(button, hint);
	}
	return button;
}

QToolButton *DrawToolbar::addColor(const RgbColor &color)
{
	QToolButton *button =
		makeButton(panel_, QTStr("Voltz.Draw.Color").arg(QString::fromStdString(formatHexColor(color))));
	button->setIcon(swatchIcon(color, false));
	connect(button, &QToolButton::clicked, this, [this, color]() { emit colorChosen(color); });
	colors_.append(qMakePair(button, color));
	return button;
}

QFrame *DrawToolbar::separator()
{
	auto *line = new QFrame(panel_);
	line->setObjectName("drawSeparator");
	line->setFixedWidth(1);
	return line;
}

void DrawToolbar::setAnnotationStyle(const AnnotationStyle &style)
{
	style_ = style;
	if (QAbstractButton *button = tools_->button(static_cast<int>(style.tool))) {
		button->setChecked(true);
	}
	markColor(style.color);
	for (const auto &entry : widths_) {
		entry.first->setChecked(entry.second == style.width);
	}
	updateFadeText();
}

void DrawToolbar::markColor(const RgbColor &color)
{
	for (const auto &entry : colors_) {
		entry.first->setIcon(swatchIcon(entry.second, entry.second == color));
	}
}

void DrawToolbar::updateFadeText()
{
	const QString value = style_.fadeSeconds > 0 ? QTStr("Voltz.Draw.Fade.Seconds").arg(style_.fadeSeconds)
						     : QTStr("Voltz.Draw.Fade.Never");
	fade_->setText(value);
	setButtonHint(fade_, QTStr("Voltz.Draw.Fade").arg(value));
}

void DrawToolbar::setBurned(bool burned)
{
	burned_->setVisible(burned);
	adjustSize();
}

void DrawToolbar::setExitHint(const QString &shortcut)
{
	setButtonHint(exit_, QTStr("Voltz.Draw.Exit").arg(shortcut));
}

void DrawToolbar::showOnScreen(const QString &screenName, bool excludeFromCapture)
{
	QScreen *target = QGuiApplication::primaryScreen();
	for (QScreen *screen : QGuiApplication::screens()) {
		if (screen->name() == screenName) {
			target = screen;
			break;
		}
	}
	adjustSize();
	if (!testAttribute(Qt::WA_WState_Created)) {
		(void)winId(); /* cria a janela nativa agora: a propriedade abaixo tem de existir antes do primeiro show */
	}
	/* o OBSApp::notify não reaplica nela o "esconder da captura" do usuário (WDA_NONE com a opção desligada) */
	windowHandle()->setProperty("isOBSProjectorWindow", true);
	if (target) {
		windowHandle()->setScreen(target);
		const QRect area = target->geometry();
		move(area.x() + (area.width() - width()) / 2, area.y() + kTopMargin);
	}
	show();
#ifdef _WIN32
	HWND hwnd = reinterpret_cast<HWND>(winId());
	SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
			  GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
	SetWindowDisplayAffinity(hwnd, excludeFromCapture ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
#else
	(void)excludeFromCapture;
#endif
	raiseWithoutActivating();
	emit nativeRectChanged();
}

void DrawToolbar::raiseWithoutActivating()
{
#ifdef _WIN32
	if (testAttribute(Qt::WA_WState_Created) && isVisible()) {
		SetWindowPos(reinterpret_cast<HWND>(winId()), HWND_TOPMOST, 0, 0, 0, 0,
			     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}
#endif
}

MonitorRect DrawToolbar::nativeRect() const
{
	MonitorRect rect;
#ifdef _WIN32
	if (!testAttribute(Qt::WA_WState_Created) || !isVisible()) {
		return rect;
	}
	RECT window;
	if (GetWindowRect(reinterpret_cast<HWND>(winId()), &window)) {
		rect = MonitorRect{static_cast<int>(window.left), static_cast<int>(window.top),
				   static_cast<int>(window.right - window.left),
				   static_cast<int>(window.bottom - window.top)};
	}
#endif
	return rect;
}

void DrawToolbar::moveEvent(QMoveEvent *event)
{
	QWidget::moveEvent(event);
	if (isVisible()) {
		emit nativeRectChanged();
	}
}

void DrawToolbar::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if (isVisible()) {
		emit nativeRectChanged();
	}
}

} // namespace voltz
```

- [ ] **Step 5: Pedido de teste `ToolbarImage` (barra desenhada fora da tela, sem janela)**

Em `frontend/voltz/annotate/AnnotationTestVendor.hpp`, no comentário da classe, depois da linha de
`SetAnnotationStyle`, acrescentar:
```cpp
//   ToolbarImage → {png (base64), largura, altura, janela_nativa} — uma DrawToolbar NUNCA mostrada, desenhada por
//   QWidget::grab() com o estilo atual (janela_nativa tem de ser false: nenhuma janela nasce);
```
Em `frontend/voltz/annotate/AnnotationTestVendor.cpp`: depois de `#include "AnnotationController.hpp"` acrescentar
`#include "DrawToolbar.hpp"`; depois de `#include <QMetaObject>` acrescentar
```cpp
#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QPixmap>
```
trocar o `kRequests` por
```cpp
constexpr const char *kRequests[] = {"InjectStroke",    "ExtendStroke",       "EndStroke",
				     "UndoStroke",      "ClearStrokes",       "ProbeOverlay",
				     "AnnotationState", "SetAnnotationStyle", "ToolbarImage"};
```
depois da função `setAnnotationStyle`, acrescentar:
```cpp
void toolbarImage(obs_data_t *, obs_data_t *response, void *)
{
	struct Result {
		std::string png;
		int width = 0;
		int height = 0;
		bool native = true;
	};
	auto result = std::make_shared<Result>();
	auto done = std::make_shared<std::promise<void>>();
	std::future<void> ready = done->get_future();
	bool ok = post([result, done](AnnotationController *controller) {
		/* nunca mostrada: o grab() desenha fora da tela, sem criar janela nativa */
		DrawToolbar toolbar;
		toolbar.setAnnotationStyle(controller->style());
		toolbar.setBurned(controller->burned());
		toolbar.adjustSize();
		const QImage image = toolbar.grab().toImage();
		QByteArray bytes;
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, "PNG");
		result->png = bytes.toBase64().toStdString();
		result->width = image.width();
		result->height = image.height();
		result->native = toolbar.testAttribute(Qt::WA_WState_Created);
		done->set_value();
	});
	if (ok) {
		done.reset();
		ok = ready.wait_for(std::chrono::seconds(3)) == std::future_status::ready && result->width > 0;
	}
	if (ok) {
		obs_data_set_string(response, "png", result->png.c_str());
		obs_data_set_int(response, "largura", result->width);
		obs_data_set_int(response, "altura", result->height);
		obs_data_set_bool(response, "janela_nativa", result->native);
	}
	obs_data_set_bool(response, "ok", ok);
}
```
e em `AnnotationTestVendor::start`, depois do registro de `"SetAnnotationStyle"`, acrescentar:
```cpp
	obs_websocket_vendor_register_request(gVendor, "ToolbarImage", toolbarImage, nullptr);
```

- [ ] **Step 6: Compilar**

Em `frontend/cmake/feature-voltz.cmake`, depois de `    voltz/annotate/DrawInputWindow.hpp`, acrescentar
```cmake
    voltz/annotate/DrawToolbar.cpp
    voltz/annotate/DrawToolbar.hpp
```
```bash
"$CLANGFMT" -i frontend/voltz/annotate/DrawToolbar.hpp frontend/voltz/annotate/DrawToolbar.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp
```
Run: CONFIGURAR; COMPILAR APP (segundo plano).
Expected: `exit=0`, nenhuma linha de erro.

- [ ] **Step 7: Ao vivo — a barra desenhada fora da tela (bloco de teste)**

Bloco de teste (`$ovW = 640; $ovH = 360`), passos no `try`:
```powershell
  [void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
  [void](VendorShell "SetAnnotationStyle" (ConvertTo-Json @{ estilo = '{"ferr":"marca_texto","cor":"#FF4D4D","esp":8,"somem_apos_s":5}' } -Compress))
  Start-Sleep -Milliseconds 400
  $bar = VendorShell "ToolbarImage"; Assert-DrawSafe "barra"
  [IO.File]::WriteAllBytes((Join-Path $sp "4c-barra.png"), [Convert]::FromBase64String($bar.png))
  Write-Host ("barra {0}x{1}, janela nativa: {2}" -f $bar.largura, $bar.altura, $bar.janela_nativa)
  Close-App
```
Expected: `barra NxM, janela nativa: False` com N ≥ 600 e M ≥ 40 (pixels físicos); `vigia: ... sem problema`. Abrir
`<scratchpad>\4c-barra.png` (Read) e conferir: painel escuro arredondado; 6 ferramentas com o **marca-texto** marcado
(fundo lima translúcido, ícone cheio lima); 4 bolinhas de cor com a **vermelha** marcada (anel lima) e o botão de
paleta; 4 pontos de espessura com o de **8 px** marcado; desfazer, lixeira; relógio com "5 s"; X. Descreva a imagem no
relatório. Nenhuma janela da barra apareceu (o vigia teria encerrado o app).

- [ ] **Step 8: Commit**

```bash
git add frontend/voltz/annotate/DrawToolbar.hpp frontend/voltz/annotate/DrawToolbar.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp frontend/voltz/shell/voltz-shell.qrc frontend/voltz/shell/icons/regular/pen.svg frontend/voltz/shell/icons/regular/highlighter-circle.svg frontend/voltz/shell/icons/regular/line-segment.svg frontend/voltz/shell/icons/regular/arrow-up-right.svg frontend/voltz/shell/icons/regular/rectangle.svg frontend/voltz/shell/icons/regular/circle.svg frontend/voltz/shell/icons/regular/arrow-counter-clockwise.svg frontend/voltz/shell/icons/regular/palette.svg frontend/voltz/shell/icons/regular/timer.svg frontend/voltz/shell/icons/fill/pen-fill.svg frontend/voltz/shell/icons/fill/highlighter-circle-fill.svg frontend/voltz/shell/icons/fill/line-segment-fill.svg frontend/voltz/shell/icons/fill/arrow-up-right-fill.svg frontend/voltz/shell/icons/fill/rectangle-fill.svg frontend/voltz/shell/icons/fill/circle-fill.svg frontend/voltz/shell/icons/fill/pencil-simple-fill.svg frontend/cmake/feature-voltz.cmake frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): barra do modo desenho (ferramentas, cores, espessura, desfazer, limpar, somem apos, aviso de queimadas) e pedido ToolbarImage

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 7: `DrawModeController` e ligação na casca (atalhos, botão "Desenhar", pulso, cliques sem onda, fantasma desligado)

**Files:**
- Create: `frontend/voltz/annotate/DrawModeController.hpp`, `frontend/voltz/annotate/DrawModeController.cpp`
- Modify: `frontend/voltz/core/live-protocol.hpp`
- Modify: `frontend/voltz/project/LiveMouse.hpp`, `frontend/voltz/project/LiveMouse.cpp`
- Modify: `frontend/voltz/shell/RecordPage.hpp`, `frontend/voltz/shell/RecordPage.cpp`
- Modify: `frontend/voltz/shell/VoltzShell.hpp`, `frontend/voltz/shell/VoltzShell.cpp`
- Modify: `frontend/voltz/shell/voltz-shell.qss`, `frontend/cmake/feature-voltz.cmake`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`

- [ ] **Step 1: Variável de depuração no protocolo**

Em `frontend/voltz/core/live-protocol.hpp`, depois de `constexpr const char *kTestBurnedEnv = "VOLTZ_TEST_ANNOT_BURNED";`,
acrescentar:
```cpp
// Só depuração e testes (independe de VOLTZ_TEST_LIVE): registra o atalho "VoltzStudio.DebugFreezeUi" (Ctrl+Shift+F12),
// que trava a thread da UI por 8 s — para ver o vigia do modo desenho e a ausência de janela fantasma (Plano 4C).
constexpr const char *kDebugFreezeEnv = "VOLTZ_DEBUG_FREEZE_UI";
```

- [ ] **Step 2: `LiveMouse` — cliques sem onda durante o desenho**

Em `frontend/voltz/project/LiveMouse.hpp`, depois de `	bool running() const { return running_; }`, acrescentar:
```cpp
	// Modo desenho ligado: os cliques não fazem onda (cada traço começaria com uma); o halo segue o mouse. Nada muda no
	// registro de Raw Input. Qualquer thread.
	void setClicksSuppressed(bool suppressed) { clicksSuppressed_.store(suppressed); }
```
e, depois de `	std::atomic<unsigned long> threadId_{0};`, acrescentar `	std::atomic<bool> clicksSuppressed_{false};`.
Em `frontend/voltz/project/LiveMouse.cpp`, em `threadMain`, trocar
```cpp
					if (button != MouseButton::None) {
```
por
```cpp
					if (button != MouseButton::None && !clicksSuppressed_.load()) {
```

- [ ] **Step 3: Cabeçalho do controlador**

Criar `frontend/voltz/annotate/DrawModeController.hpp`:
```cpp
#pragma once

#include <voltz/core/cursor-track.hpp>
#include <voltz/core/draw-session.hpp>

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

namespace voltz {

class AnnotationController;
class DrawInputWindow;
class DrawToolbar;
struct DrawShared;

// Quem pediu o modo desenho: as duas únicas portas de entrada (ação explícita do usuário).
enum class DrawTrigger { Hotkey, Button };

// Modo desenho (spec §6.2) na thread da UI. Liga SÓ por ação do usuário — o atalho do OBS "VoltzStudio.Draw" (padrão
// Ctrl+Shift+D) ou o botão "Desenhar" —, nunca sozinho (não é salvo; desligado em toda abertura) e nunca com
// VOLTZ_TEST_LIVE. Ligado: pulso da UI a cada 250 ms (o vigia da thread de entrada sai se ele parar por 1 s), barra
// (DrawToolbar) no alto do monitor da Tela e a janela de entrada (DrawInputWindow, thread própria) sobre esse monitor.
// As saídas garantidas são decididas na thread de entrada (DrawSession); daqui saem o mesmo atalho, o botão Sair da
// barra, a troca da Tela/monitor (conferida a cada 1 s) e a saída do app. Drena os comandos de traço para o
// AnnotationController. Também registra "VoltzStudio.UndoAnnotation" e "VoltzStudio.ClearAnnotations" (sem tecla
// padrão) e, só com VOLTZ_DEBUG_FREEZE_UI, "VoltzStudio.DebugFreezeUi".
class DrawModeController : public QObject {
	Q_OBJECT

public:
	explicit DrawModeController(AnnotationController *annotations, QObject *parent = nullptr);
	~DrawModeController() override;

	// EXIT do frontend, ANTES do AnnotationController::shutdown: sai do modo (o traço em andamento termina), apaga a
	// barra e solta os atalhos.
	void shutdown();
	// Atalho ou botão: liga (se nada recusar) ou pede a saída.
	void toggle(DrawTrigger trigger);
	bool active() const { return input_ != nullptr; }
	// Teclas do atalho de desenhar ("Ctrl + Shift + D") ou "sem atalho".
	QString drawShortcutText() const;

signals:
	void activeChanged(bool active);
	// Mensagem para o usuário quando o modo não ligou (vazia nunca).
	void refused(const QString &message);

private:
	void enter(DrawTrigger trigger);
	void createToolbar();
	void beat();
	void drain();
	void finish(DrawExit reason);
	void updateHole();
	void registerHotkeys();
	void loadHotkeyBindings();
	void unregisterHotkeys();
	static void wake(void *context);
	static void handleEvent(enum obs_frontend_event event, void *data);
	static void handleDrawHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void handleUndoHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void handleClearHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void handleFreezeHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

	AnnotationController *annotations_;
	std::shared_ptr<DrawShared> shared_;
	std::unique_ptr<DrawInputWindow> input_;
	DrawToolbar *toolbar_ = nullptr; // sem pai (janela própria): apagada no shutdown
	CaptureMapping mapping_;
	QTimer heartbeat_;
	int beats_ = 0;
	obs_hotkey_id drawHotkey_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id undoHotkey_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id clearHotkey_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id freezeHotkey_ = OBS_INVALID_HOTKEY_ID;
	bool eventsRegistered_ = false;
	bool shutdown_ = false;
};

} // namespace voltz
```

- [ ] **Step 4: Implementação do controlador**

Criar `frontend/voltz/annotate/DrawModeController.cpp`:
```cpp
#include "DrawModeController.hpp"
#include "AnnotationController.hpp"
#include "DrawInputWindow.hpp"
#include "DrawToolbar.hpp"

#include <voltz/core/live-protocol.hpp>
#include <voltz/shell/HotkeyBinding.hpp>

#include <OBSApp.hpp>           // QTStr
#include <utility/platform.hpp> // SetDisplayAffinitySupported

#include <util/platform.h>

#include <QThread>

#include <mutex>
#include <string>
#include <vector>

#include "moc_DrawModeController.cpp"

namespace voltz {

namespace {

constexpr const char *kDrawHotkeyName = "VoltzStudio.Draw";
constexpr const char *kUndoHotkeyName = "VoltzStudio.UndoAnnotation";
constexpr const char *kClearHotkeyName = "VoltzStudio.ClearAnnotations";
constexpr const char *kFreezeHotkeyName = "VoltzStudio.DebugFreezeUi";
constexpr int kHeartbeatMs = 250;
constexpr int kTargetCheckBeats = 4; // confere a Tela/monitor a cada ~1 s
constexpr unsigned long kFreezeMs = 8000;

int64_t nowNs()
{
	return static_cast<int64_t>(os_gettime_ns());
}

bool sameMapping(const CaptureMapping &a, const CaptureMapping &b)
{
	return a.monitor.left == b.monitor.left && a.monitor.top == b.monitor.top &&
	       a.monitor.width == b.monitor.width && a.monitor.height == b.monitor.height &&
	       a.captureWidth == b.captureWidth && a.captureHeight == b.captureHeight;
}

} // namespace

DrawModeController::DrawModeController(AnnotationController *annotations, QObject *parent)
	: QObject(parent),
	  annotations_(annotations)
{
	heartbeat_.setInterval(kHeartbeatMs);
	connect(&heartbeat_, &QTimer::timeout, this, [this]() { beat(); });
	connect(annotations_, &AnnotationController::styleChanged, this, [this]() {
		if (toolbar_) {
			toolbar_->setAnnotationStyle(annotations_->style());
		}
	});
	connect(annotations_, &AnnotationController::burnedChanged, this, [this](bool burned) {
		if (toolbar_) {
			toolbar_->setBurned(burned);
		}
	});
	/* A casca nasce depois do OBSBasic::OBSInit: perfil e atalhos do OBS já estão carregados. */
	registerHotkeys();
	obs_frontend_add_event_callback(&DrawModeController::handleEvent, this);
	eventsRegistered_ = true;
	blog(LOG_INFO, "[voltz] modo desenho: desligado ao abrir (so liga pelo atalho ou pelo botao Desenhar)");
}

DrawModeController::~DrawModeController()
{
	shutdown();
}

void DrawModeController::shutdown()
{
	if (shutdown_) {
		return;
	}
	if (input_) {
		input_->requestExit(DrawExit::Shutdown);
		input_->wait(2000);
		drain(); /* o traço em andamento termina e o Exit chega aqui → finish */
	}
	if (input_ || shared_) {
		finish(DrawExit::Shutdown);
	}
	shutdown_ = true;
	unregisterHotkeys();
	if (eventsRegistered_) {
		/* seguro durante o EXIT: OBSStudioAPI::on_event percorre os callbacks de trás para frente */
		obs_frontend_remove_event_callback(&DrawModeController::handleEvent, this);
		eventsRegistered_ = false;
	}
	delete toolbar_;
	toolbar_ = nullptr;
}

void DrawModeController::toggle(DrawTrigger trigger)
{
	if (shutdown_) {
		return;
	}
	if (input_) {
		input_->requestExit(trigger == DrawTrigger::Hotkey ? DrawExit::Hotkey : DrawExit::Button);
		return;
	}
	enter(trigger);
}

void DrawModeController::enter(DrawTrigger trigger)
{
	const bool testMode = live::testModeEnabled();
	CaptureMapping mapping;
	std::string device;
	/* com VOLTZ_TEST_LIVE nem procura o alvo: a recusa vem antes de qualquer janela */
	const bool hasTarget = !shutdown_ && !testMode && annotations_->drawTarget(mapping, device);
	const DrawRefusal refusal = checkDrawEntry(shutdown_, testMode, hasTarget);
	if (refusal != DrawRefusal::None) {
		blog(LOG_INFO, "[voltz] modo desenho: recusado (%s)", drawRefusalName(refusal));
		annotations_->setDrawStatus(false, drawRefusalName(refusal), "");
		if (refusal == DrawRefusal::TestMode) {
			emit refused(QTStr("Voltz.Draw.Refused.Test"));
		} else if (refusal == DrawRefusal::NoTarget) {
			emit refused(QTStr("Voltz.Draw.Refused.NoTarget"));
		}
		return;
	}
	const bool exclude = SetDisplayAffinitySupported() && !annotations_->burned();
	shared_ = std::make_shared<DrawShared>();
	shared_->uiHeartbeatNs.store(nowNs());
	{
		std::lock_guard<std::mutex> lock(shared_->mutex);
		shared_->wake = &DrawModeController::wake;
		shared_->wakeContext = this;
	}
	mapping_ = mapping;
	beats_ = 0;
	heartbeat_.start();
	if (!toolbar_) {
		createToolbar();
	}
	toolbar_->setAnnotationStyle(annotations_->style());
	toolbar_->setBurned(annotations_->burned());
	toolbar_->setExitHint(drawShortcutText());
	toolbar_->showOnScreen(QString::fromStdString(device), exclude);
	{
		std::lock_guard<std::mutex> lock(shared_->mutex);
		shared_->hole = toolbar_->nativeRect();
	}
	DrawInputWindow::Options options;
	options.mapping = mapping;
	options.excludeFromCapture = exclude;
	input_ = DrawInputWindow::start(shared_, options);
	if (!input_) {
		blog(LOG_WARNING, "[voltz] modo desenho: recusado (janela)");
		heartbeat_.stop();
		{
			std::lock_guard<std::mutex> lock(shared_->mutex);
			shared_->wake = nullptr;
		}
		shared_.reset();
		toolbar_->hide();
		annotations_->setDrawStatus(false, "janela", "");
		emit refused(QTStr("Voltz.Draw.Refused.Window"));
		return;
	}
	/* a janela de entrada acabou de ir para o topo: a barra volta por cima (e tem o buraco na região dela) */
	toolbar_->raiseWithoutActivating();
	annotations_->setDrawStatus(true, "", "");
	blog(LOG_INFO,
	     "[voltz] modo desenho: ligado (%s; monitor %dx%d em %d,%d; captura %dx%d; ativa: %s; fora da captura: %s)",
	     trigger == DrawTrigger::Hotkey ? "atalho" : "botao", mapping.monitor.width, mapping.monitor.height,
	     mapping.monitor.left, mapping.monitor.top, mapping.captureWidth, mapping.captureHeight,
	     input_->activated() ? "sim" : "nao (o primeiro clique ativa)", exclude ? "sim" : "nao");
	emit activeChanged(true);
}

void DrawModeController::createToolbar()
{
	toolbar_ = new DrawToolbar(nullptr);
	connect(toolbar_, &DrawToolbar::toolChosen, this, [this](AnnotationTool tool) {
		AnnotationStyle style = annotations_->style();
		style.tool = tool;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::colorChosen, this, [this](RgbColor color) {
		AnnotationStyle style = annotations_->style();
		style.color = color;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::widthChosen, this, [this](int width) {
		AnnotationStyle style = annotations_->style();
		style.width = width;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::fadeChosen, this, [this](int seconds) {
		AnnotationStyle style = annotations_->style();
		style.fadeSeconds = seconds;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::undoClicked, this, [this]() { annotations_->undo(); });
	connect(toolbar_, &DrawToolbar::clearClicked, this, [this]() { annotations_->clear(); });
	connect(toolbar_, &DrawToolbar::exitClicked, this, [this]() {
		if (input_) {
			input_->requestExit(DrawExit::Button);
		}
	});
	connect(toolbar_, &DrawToolbar::nativeRectChanged, this, [this]() { updateHole(); });
}

void DrawModeController::updateHole()
{
	if (!shared_ || !toolbar_) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(shared_->mutex);
		shared_->hole = toolbar_->nativeRect();
	}
	if (input_) {
		input_->holeChanged();
	}
}

void DrawModeController::beat()
{
	if (!shared_) {
		return;
	}
	shared_->uiHeartbeatNs.store(nowNs());
	if (++beats_ % kTargetCheckBeats != 0 || !input_) {
		return;
	}
	CaptureMapping mapping;
	std::string device;
	if (!annotations_->drawTarget(mapping, device) || !sameMapping(mapping, mapping_)) {
		blog(LOG_INFO, "[voltz] modo desenho: a Tela ou o monitor mudou");
		input_->requestExit(DrawExit::Target);
	}
}

void DrawModeController::wake(void *context)
{
	/* Thread de entrada, SOB o mutex do DrawShared: o finish anula `wake` sob o mesmo mutex antes de este objeto
	 * morrer, e evento postado para objeto já destruído o Qt descarta. */
	auto *self = static_cast<DrawModeController *>(context);
	QMetaObject::invokeMethod(self, [self]() { self->drain(); }, Qt::QueuedConnection);
}

void DrawModeController::drain()
{
	if (!shared_) {
		return;
	}
	const std::vector<DrawCommand> commands = takeDrawCommands(*shared_);
	const AnnotationStyle style = annotations_->style();
	std::vector<StrokePoint> pending;
	auto flush = [this, &pending]() {
		if (!pending.empty()) {
			annotations_->extendStroke(pending);
			pending.clear();
		}
	};
	for (const DrawCommand &command : commands) {
		switch (command.type) {
		case DrawCommandType::Begin:
			flush();
			if (!command.points.empty()) {
				annotations_->beginStroke(style.tool, style.color, style.width, command.points.front());
			}
			break;
		case DrawCommandType::Extend:
			pending.insert(pending.end(), command.points.begin(), command.points.end());
			break;
		case DrawCommandType::End:
			flush();
			annotations_->endStroke();
			break;
		case DrawCommandType::Undo:
			flush();
			annotations_->undo();
			break;
		case DrawCommandType::Exit:
			flush();
			finish(command.exit);
			return;
		}
	}
	flush();
}

void DrawModeController::finish(DrawExit reason)
{
	heartbeat_.stop();
	if (shared_) {
		std::lock_guard<std::mutex> lock(shared_->mutex);
		shared_->wake = nullptr;
	}
	input_.reset(); /* a thread de entrada já saiu ou sai agora (espera até 2 s) */
	shared_.reset();
	if (toolbar_) {
		toolbar_->hide();
	}
	annotations_->setDrawStatus(false, "", drawExitName(reason));
	blog(LOG_INFO, "[voltz] modo desenho: desligado (%s); sobreposicao: %s", drawExitName(reason),
	     annotations_->overlayStatsText().c_str());
	emit activeChanged(false);
}

QString DrawModeController::drawShortcutText() const
{
	const QString text = hotkeyShortcutText(drawHotkey_);
	return text.isEmpty() ? QTStr("Voltz.Record.Draw.NoShortcut") : text;
}

void DrawModeController::registerHotkeys()
{
	drawHotkey_ = obs_hotkey_register_frontend(kDrawHotkeyName, QTStr("Voltz.Hotkey.Draw").toUtf8().constData(),
						   &DrawModeController::handleDrawHotkey, this);
	undoHotkey_ = obs_hotkey_register_frontend(kUndoHotkeyName,
						   QTStr("Voltz.Hotkey.UndoAnnotation").toUtf8().constData(),
						   &DrawModeController::handleUndoHotkey, this);
	clearHotkey_ = obs_hotkey_register_frontend(kClearHotkeyName,
						    QTStr("Voltz.Hotkey.ClearAnnotations").toUtf8().constData(),
						    &DrawModeController::handleClearHotkey, this);
	if (qEnvironmentVariableIsSet(live::kDebugFreezeEnv)) {
		freezeHotkey_ = obs_hotkey_register_frontend(kFreezeHotkeyName,
							     QTStr("Voltz.Hotkey.DebugFreezeUi").toUtf8().constData(),
							     &DrawModeController::handleFreezeHotkey, this);
		blog(LOG_WARNING, "[voltz] depuracao (%s): atalho %s ligado", live::kDebugFreezeEnv, kFreezeHotkeyName);
	}
	loadHotkeyBindings();
}

void DrawModeController::loadHotkeyBindings()
{
	/* Desfazer e limpar sem tecla padrão: atalhos do OBS não consomem a tecla (Ctrl+Shift+Z iria também para o app em
	 * uso). Dentro do modo desenho, Ctrl+Z desfaz pela janela de entrada. */
	loadHotkeyBinding(drawHotkey_, kDrawHotkeyName, "OBS_KEY_D");
	loadHotkeyBinding(undoHotkey_, kUndoHotkeyName, nullptr);
	loadHotkeyBinding(clearHotkey_, kClearHotkeyName, nullptr);
	loadHotkeyBinding(freezeHotkey_, kFreezeHotkeyName, "OBS_KEY_F12");
}

void DrawModeController::unregisterHotkeys()
{
	for (obs_hotkey_id *id : {&drawHotkey_, &undoHotkey_, &clearHotkey_, &freezeHotkey_}) {
		if (*id != OBS_INVALID_HOTKEY_ID) {
			obs_hotkey_unregister(*id);
			*id = OBS_INVALID_HOTKEY_ID;
		}
	}
}

void DrawModeController::handleEvent(enum obs_frontend_event event, void *data)
{
	if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
		static_cast<DrawModeController *>(data)->loadHotkeyBindings();
	}
}

// Teclado: rota do OBSBasic (thread da UI). obs-websocket TriggerHotkeyByName: thread do websocket com o lock dos
// atalhos preso. Os quatro só enfileiram.
void DrawModeController::handleDrawHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<DrawModeController *>(data);
	QMetaObject::invokeMethod(self, [self]() { self->toggle(DrawTrigger::Hotkey); }, Qt::QueuedConnection);
}

void DrawModeController::handleUndoHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<DrawModeController *>(data);
	QMetaObject::invokeMethod(self, [self]() { self->annotations_->undo(); }, Qt::QueuedConnection);
}

void DrawModeController::handleClearHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<DrawModeController *>(data);
	QMetaObject::invokeMethod(self, [self]() { self->annotations_->clear(); }, Qt::QueuedConnection);
}

void DrawModeController::handleFreezeHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<DrawModeController *>(data);
	QMetaObject::invokeMethod(
		self,
		[]() {
			blog(LOG_WARNING, "[voltz] depuracao: thread da UI travada por %lu ms (%s)", kFreezeMs,
			     live::kDebugFreezeEnv);
			QThread::msleep(kFreezeMs);
			blog(LOG_INFO, "[voltz] depuracao: thread da UI de volta");
		},
		Qt::QueuedConnection);
}

} // namespace voltz
```

- [ ] **Step 5: Botão "Desenhar" na tela Gravar**

Em `frontend/voltz/shell/RecordPage.hpp`: depois de `class ErrorBanner;` acrescentar `class DrawModeController;`;
trocar o construtor por
```cpp
	RecordPage(FrontendBridge *bridge, ProjectRecorder *recorder, DrawModeController *drawMode,
		   QWidget *parent = nullptr);
```
depois de `	void showMarkerToast(int number);` acrescentar
```cpp
	void showToast(const QString &text);
	void refreshDrawHint();
```
depois de `	ProjectRecorder *recorder_;` acrescentar `	DrawModeController *drawMode_;` e depois de
`	QPushButton *markerButton_ = nullptr;` acrescentar `	QPushButton *drawButton_ = nullptr;`. No comentário da
classe, trocar `projeto · Marcador · Pausar` por `projeto · Desenhar · Marcador · Pausar`.

Em `frontend/voltz/shell/RecordPage.cpp`:
1. Depois de `#include <voltz/project/ProjectRecorder.hpp>` acrescentar `#include <voltz/annotate/DrawModeController.hpp>`.
2. Trocar a assinatura e a lista de inicialização do construtor por:
```cpp
RecordPage::RecordPage(FrontendBridge *bridge, ProjectRecorder *recorder, DrawModeController *drawMode,
		       QWidget *parent)
	: QWidget(parent),
	  bridge_(bridge),
	  recorder_(recorder),
	  drawMode_(drawMode)
```
3. Logo antes de `	markerButton_ = new QPushButton(QTStr("Voltz.Record.Marker"), this);`, acrescentar:
```cpp
	drawButton_ = new QPushButton(QTStr("Voltz.Record.Draw"), this);
	drawButton_->setObjectName("drawButton");
	drawButton_->setCheckable(true);
	drawButton_->setIcon(phosphorToggleIcon(QStringLiteral("pencil-simple"), palette::kText, palette::kAccent));
	drawButton_->setIconSize(QSize(16, 16));
```
4. Trocar `	bar->addWidget(markerButton_);` por
```cpp
	bar->addWidget(drawButton_);
	bar->addWidget(markerButton_);
```
5. Logo depois de `	connect(markerButton_, &QPushButton::clicked, this, [this]() { recorder_->addMarker(); });`, acrescentar:
```cpp
	connect(drawButton_, &QPushButton::clicked, this, [this]() {
		drawMode_->toggle(DrawTrigger::Button);
		/* o botão reflete o estado de verdade (ligou, foi recusado ou vai sair) */
		const QSignalBlocker blocker(drawButton_);
		drawButton_->setChecked(drawMode_->active());
	});
	connect(drawMode_, &DrawModeController::activeChanged, this, [this](bool active) {
		const QSignalBlocker blocker(drawButton_);
		drawButton_->setChecked(active);
	});
	connect(drawMode_, &DrawModeController::refused, this, [this](const QString &message) { showToast(message); });
	refreshDrawHint();
```
6. Em `showEvent`, depois de `	refreshDiskInfo();`, acrescentar `	refreshDrawHint();` (o atalho pode ter mudado em
Configurações).
7. Trocar a função `showMarkerToast` inteira por:
```cpp
void RecordPage::showMarkerToast(int number)
{
	/* Mostra o tempo do cronômetro (líquido, como o usuário vê), não o t_ms bruto do marcadores.json. */
	const QString clock = QString::fromStdString(formatClock(recordingClock_.elapsedMs(monotonic_.elapsed())));
	showToast(QTStr("Voltz.Record.MarkerAdded").arg(QTStr("Voltz.Marker.Name").arg(number), clock));
}

void RecordPage::showToast(const QString &text)
{
	markerToast_->setText(text);
	markerToastTimer_.start();
}

void RecordPage::refreshDrawHint()
{
	drawButton_->setToolTip(QTStr("Voltz.Record.Draw.Hint").arg(drawMode_->drawShortcutText()));
}
```

Em `frontend/voltz/shell/voltz-shell.qss`, logo depois do bloco `#voltzShell QPushButton:disabled { ... }`, acrescentar:
```css
#voltzShell QPushButton#drawButton:checked {
	border-color: #D4F53C;
	color: #D4F53C;
}
```

- [ ] **Step 6: Casca — controlador, fantasma desligado, saída na ordem certa**

Em `frontend/voltz/shell/VoltzShell.hpp`: depois de `class AnnotationController;` acrescentar
`class DrawModeController;`; depois de `	AnnotationController *annotations_ = nullptr;` acrescentar
`	DrawModeController *drawMode_ = nullptr;`.

Em `frontend/voltz/shell/VoltzShell.cpp`:
1. Depois de `#include <voltz/annotate/AnnotationController.hpp>` acrescentar
`#include <voltz/annotate/DrawModeController.hpp>`; depois de `#include <QVBoxLayout>` acrescentar:
```cpp

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
```
2. No construtor, logo depois de `	setStyleSheet(shellStyleSheet());`, acrescentar:
```cpp
#ifdef _WIN32
	/* Nenhuma janela do processo vira "fantasma" quando a UI trava (> 5 s): o fantasma de uma janela que cobre o monitor
	 * (sobreposição das anotações) receberia os cliques do usuário. O modo desenho tem ainda o vigia de 1 s. */
	DisableProcessWindowsGhosting();
	blog(LOG_INFO, "[voltz] janelas fantasma do Windows desligadas no processo");
#endif
```
3. Trocar
```cpp
	/* Anotações (4B): só exibe; os traços vêm do fornecedor de teste até o modo desenho do 4C. */
	annotations_ = new AnnotationController(this);
	recorder_->setAnnotationController(annotations_);
```
por
```cpp
	/* Anotações: o modo desenho (atalho ou botão "Desenhar") entrega os traços ao AnnotationController. */
	annotations_ = new AnnotationController(this);
	recorder_->setAnnotationController(annotations_);
	drawMode_ = new DrawModeController(annotations_, this);
	connect(drawMode_, &DrawModeController::activeChanged, this,
		[this](bool active) { liveMouse_->setClicksSuppressed(active); });
```
4. Trocar `	pages_->addWidget(new RecordPage(bridge_, recorder_, pages_));` por
`	pages_->addWidget(new RecordPage(bridge_, recorder_, drawMode_, pages_));`.
5. No `connect(bridge_, &FrontendBridge::exiting, ...)`, trocar
```cpp
		annotations_->shutdown();
		liveMouse_->shutdown();
```
por
```cpp
		drawMode_->shutdown(); /* antes das anotações: o traço em andamento termina */
		annotations_->shutdown();
		liveMouse_->shutdown();
```

- [ ] **Step 7: Textos (os dois locales)**

No fim de `frontend/data/locale/voltz-pt-BR.ini`, acrescentar:
```ini
Voltz.Hotkey.Draw="Voltz Studio: ligar/desligar modo desenho"
Voltz.Hotkey.UndoAnnotation="Voltz Studio: desfazer último traço"
Voltz.Hotkey.ClearAnnotations="Voltz Studio: limpar anotações"
Voltz.Hotkey.DebugFreezeUi="Voltz Studio (depuração): travar a interface por 8 s"
Voltz.Record.Draw="Desenhar"
Voltz.Record.Draw.Hint="Desenhar na tela (%1). Saia com Esc, clique direito ou o mesmo atalho. Troque o atalho em Configurações → Atalhos (modo clássico)."
Voltz.Record.Draw.NoShortcut="sem atalho"
Voltz.Draw.Refused.Test="Modo desenho desligado no modo de teste."
Voltz.Draw.Refused.NoTarget="Para desenhar, a Tela precisa ser uma captura de monitor (aplique um preset)."
Voltz.Draw.Refused.Window="Não foi possível ligar o modo desenho (veja o log)."
```
No fim de `frontend/data/locale/voltz-en-US.ini`, acrescentar:
```ini
Voltz.Hotkey.Draw="Voltz Studio: toggle draw mode"
Voltz.Hotkey.UndoAnnotation="Voltz Studio: undo last stroke"
Voltz.Hotkey.ClearAnnotations="Voltz Studio: clear annotations"
Voltz.Hotkey.DebugFreezeUi="Voltz Studio (debug): freeze the interface for 8 s"
Voltz.Record.Draw="Draw"
Voltz.Record.Draw.Hint="Draw on screen (%1). Exit with Esc, right-click or the same shortcut. Change the shortcut in Settings → Hotkeys (classic mode)."
Voltz.Record.Draw.NoShortcut="no shortcut"
Voltz.Draw.Refused.Test="Draw mode is off in test mode."
Voltz.Draw.Refused.NoTarget="To draw, the Screen must be a display capture (apply a preset)."
Voltz.Draw.Refused.Window="Could not turn on draw mode (see the log)."
```
Conferir a paridade (Convenções) → sem saída.

- [ ] **Step 8: Compilar e conferir**

Em `frontend/cmake/feature-voltz.cmake`, depois de `    voltz/annotate/DrawInputWindow.hpp`, acrescentar
```cmake
    voltz/annotate/DrawModeController.cpp
    voltz/annotate/DrawModeController.hpp
```
```bash
"$CLANGFMT" -i frontend/voltz/annotate/DrawModeController.hpp frontend/voltz/annotate/DrawModeController.cpp frontend/voltz/core/live-protocol.hpp frontend/voltz/project/LiveMouse.hpp frontend/voltz/project/LiveMouse.cpp frontend/voltz/shell/RecordPage.hpp frontend/voltz/shell/RecordPage.cpp frontend/voltz/shell/VoltzShell.hpp frontend/voltz/shell/VoltzShell.cpp
```
Run: CONFIGURAR; COMPILAR APP (segundo plano); depois GREP DE SEGURANÇA (Convenções) e
```bash
grep -n "toggle(" frontend/voltz --include=*.cpp -r
```
Expected: `exit=0`; o grep de segurança como na Task 5; `toggle(` só em `DrawModeController.cpp` (definição e
`handleDrawHotkey`) e em `RecordPage.cpp` (clique do botão) — as duas únicas portas de entrada.

- [ ] **Step 9: Ao vivo — recusa com `VOLTZ_TEST_LIVE` (bloco de teste)**

Bloco de teste (`$ovW = 640; $ovH = 360`, `$freezeUi = $false`), passos no `try`:
```powershell
  [void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
  $st = Shell-State; Write-Host "modo_desenho ao abrir: $($st.modo_desenho)"
  for ($i = 1; $i -le 3; $i++) {
    $r = Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.Draw"}}]' -Quiet
    Start-Sleep -Milliseconds 500; Assert-DrawSafe "atalho $i"
    $st = Shell-State; Write-Host ("#{0}: ok={1} modo_desenho={2} recusa='{3}'" -f $i, $r[0].ok, $st.modo_desenho, $st.desenho_recusa) }
  Close-App
  LogLines "[voltz] modo desenho"; LogLines "[voltz] janelas fantasma"
```
Expected: `modo_desenho ao abrir: False`; três linhas `#i: ok=True modo_desenho=False recusa='teste'`; quatro
`assert[...] ok`; no log `[voltz] modo desenho: desligado ao abrir (...)` e três `[voltz] modo desenho: recusado (teste)`,
**nenhum** `ligado`; `[voltz] janelas fantasma do Windows desligadas no processo`; `vigia: N conferencias sem problema`.
(A janela de entrada nem é tentada: a linha `janela de entrada recusada` não aparece, porque o controlador recusa antes.)

- [ ] **Step 10: Ao vivo — UI travada sem fantasma (bloco de teste)**

Bloco de teste (`$ovW = 640; $ovH = 360`, `$freezeUi = $true`), passos no `try`:
```powershell
  [void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
  [void](VendorShell "InjectStroke" (Stroke-Json "linha" "#D4F53C" 16 "[[640,720],[1920,720]]"))
  $st = Shell-State; Assert-DrawSafe "traco"
  $watch = [long[]]@($script:shell.ToInt64(), [long]$st.sobreposicao.hwnd)
  [void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.DebugFreezeUi"}}]' -Quiet)
  $hung = $false; $ghosts = 0
  for ($i = 0; $i -lt 44; $i++) { Start-Sleep -Milliseconds 250
    if ($P::IsHungAppWindow($script:shell)) { $hung = $true }
    $ghosts = [Math]::Max($ghosts, $P::GhostsOver($watch)); Assert-DrawSafe "travada $i" -Quiet }
  Start-Sleep 2; $st = Shell-State
  Write-Host ("travou: {0}; fantasmas: {1}; tracos depois: {2}" -f $hung, $ghosts, @($st.visiveis).Count)
  Close-App
  LogLines "[voltz] depuracao"
```
Expected: `travou: True; fantasmas: 0; tracos depois: 1`; no log `[voltz] depuracao (VOLTZ_DEBUG_FREEZE_UI): atalho
VoltzStudio.DebugFreezeUi ligado`, `[voltz] depuracao: thread da UI travada por 8000 ms (...)` e `... de volta`; vigia
sem problema. (Durante os 8 s a janela do Voltz fica parada; não clique nela. Se `travou: False`, a casca estava
minimizada — repita com ela visível; se `fantasmas` > 0, pare e reporte.)

- [ ] **Step 11: Commit**

```bash
git add frontend/voltz/annotate/DrawModeController.hpp frontend/voltz/annotate/DrawModeController.cpp frontend/voltz/core/live-protocol.hpp frontend/voltz/project/LiveMouse.hpp frontend/voltz/project/LiveMouse.cpp frontend/voltz/shell/RecordPage.hpp frontend/voltz/shell/RecordPage.cpp frontend/voltz/shell/VoltzShell.hpp frontend/voltz/shell/VoltzShell.cpp frontend/voltz/shell/voltz-shell.qss frontend/cmake/feature-voltz.cmake frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): modo desenho na casca (atalho Ctrl+Shift+D e botao Desenhar, pulso da UI, barra, desfazer/limpar, cliques sem onda, fantasma desligado)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 8: Smoke do modo desenho (`tools/voltz-smoke-draw.ps1`) e grep de segurança de todo o código

**Files:**
- Create: `tools/voltz-smoke-draw.ps1`

- [ ] **Step 1: Escrever o smoke**

Criar `tools/voltz-smoke-draw.ps1` (ASCII puro):
```powershell
<#
  Teste de integracao do Plano 4C (modo desenho). O modo desenho NUNCA liga aqui.
  Seguranca (o PC e do usuario e esta em uso):
  - VOLTZ_TEST_LIVE=1 recusa o modo desenho duas vezes (DrawModeController e DrawInputWindow). O unico gatilho usado e
    TriggerHotkeyByName "VoltzStudio.Draw", so para provar a recusa. O botao de desenhar da casca nunca e acionado.
  - Nenhuma entrada e injetada; nada e posto em primeiro plano. O unico P/Invoke que escreve e PostMessage(WM_CLOSE) na
    janela da casca do PID lancado, guardada na abertura.
  - Assert-DrawSafe depois de cada acao e um vigia (job, 200 ms), so leitura: sobreposicoes VoltzAnnotationOverlay do PID
    (FindWindowEx com titulo nulo UNIAO EnumWindows; listas divergentes = BLOCKED) com WS_EX 0x080800A8, fora do
    primeiro plano, dentro do retangulo de teste, fora do desktop virtual, afinidade 0x11; NENHUMA janela VoltzDrawInput;
    NENHUMA janela visivel "VoltzDrawToolbar"; nenhuma thread do PID com janela Voltz Win32 ativa, com foco ou com o
    mouse capturado (GetGUIThreadInfo). Falha: o PID lancado e encerrado e o smoke falha (BLOCKED).
  - Sobreposicao de teste sempre fora do desktop virtual (VOLTZ_TEST_OVERLAY_RECT, conferido contra cada monitor antes de
    abrir o app); Tela e Camera de COR (VOLTZ_TEST_PRESET_STANDINS); Assert-NoRealCapture; render so por
    GetSourceScreenshot de cena sintetica; a barra so e desenhada fora da tela (ToolbarImage, sem janela).
  - Fase C trava a thread da UI por 8 s (VOLTZ_DEBUG_FREEZE_UI): a janela do Voltz fica parada; so leitura
    (IsHungAppWindow, janelas "Ghost").
  - Config portatil guardada e restaurada (conferida por hash); Videos conferida intacta; no fim, nenhuma janela Voltz.
  Fases: A (recusa, estilo, barra), B (redesenho parcial 2560x1440 com medidas), C (UI travada, sem fantasma).
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-draw.ps1
#>
param([string]$Config = "RelWithDebInfo")

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
$bak = Join-Path $rundir "config.smokedraw-bak"
$voltzCfg = Join-Path $cfg "Voltz Studio"
$logDir = Join-Path $env:TEMP "voltz-smoke-draw-logs"
$wsPort = 4455

$oldBaks = @(Get-ChildItem -LiteralPath $rundir -Directory -Force -Filter "config.*bak*" -ErrorAction SilentlyContinue)
if ($oldBaks.Count -gt 0) { Write-Host "FALHOU backup anterior em $rundir ($(($oldBaks | ForEach-Object { $_.Name }) -join ', ')): restaure ou apague antes"; exit 1 }
if (@(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue).Count -gt 0) {
    Write-Host "FALHOU a porta $wsPort ja esta em uso (outro OBS/Voltz aberto?)"; exit 1
}

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
# P/Invoke de LEITURA (nada que mova o mouse, clique, digite, ative ou mude janelas); PostMessage so para WM_CLOSE
$probeSource = @'
using System; using System.Runtime.InteropServices; using System.Collections.Generic; using System.Text;
public static class VoltzProbe {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
 [StructLayout(LayoutKind.Sequential)] public struct GUITHREADINFO { public int cbSize; public int flags; public IntPtr hwndActive; public IntPtr hwndFocus; public IntPtr hwndCapture; public IntPtr hwndMenuOwner; public IntPtr hwndMoveSize; public IntPtr hwndCaret; public RECT rcCaret; }
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")] public static extern IntPtr GetWindowLongPtr(IntPtr h, int index);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool GetWindowDisplayAffinity(IntPtr h, out uint affinity);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
 [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint threadId, ref GUITHREADINFO info);
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder sb, int n);
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder sb, int n);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
 public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
 public static string ClassOf(IntPtr h) { var sb = new StringBuilder(256); GetClassName(h, sb, 256); return sb.ToString(); }
 public static string TitleOf(IntPtr h) { var sb = new StringBuilder(256); GetWindowText(h, sb, 256); return sb.ToString(); }
 public static uint OwnerOf(IntPtr h) { uint owner; GetWindowThreadProcessId(h, out owner); return owner; }
 public static List<long> OfClass(uint pid, string cls, out bool divergent) {
   var byFind = new HashSet<long>(); IntPtr h = IntPtr.Zero;
   while ((h = FindWindowEx(IntPtr.Zero, h, cls, null)) != IntPtr.Zero) { if (OwnerOf(h) == pid) byFind.Add(h.ToInt64()); }
   var byEnum = new HashSet<long>();
   EnumWindows((w, l) => { if (OwnerOf(w) == pid && ClassOf(w) == cls) byEnum.Add(w.ToInt64()); return true; }, IntPtr.Zero);
   divergent = !byFind.SetEquals(byEnum); byFind.UnionWith(byEnum); return new List<long>(byFind); }
 public static List<long> VisibleTitled(uint pid, string title) { var list = new List<long>();
   EnumWindows((w, l) => { if (OwnerOf(w) == pid && IsWindowVisible(w) && TitleOf(w) == title) list.Add(w.ToInt64()); return true; }, IntPtr.Zero);
   return list; }
 public static int GhostsOver(long[] windows) { var rects = new List<RECT>();
   foreach (long w in windows) { RECT r; if (GetWindowRect(new IntPtr(w), out r)) rects.Add(r); }
   int count = 0;
   EnumWindows((h, l) => { if (ClassOf(h) == "Ghost") { RECT g; if (GetWindowRect(h, out g)) { foreach (RECT r in rects) { if (r.L == g.L && r.T == g.T && r.R == g.R && r.B == g.B) { count++; break; } } } } return true; }, IntPtr.Zero);
   return count; }
 public static string VoltzHolder(int[] threads) {
   foreach (int t in threads) { GUITHREADINFO gi = new GUITHREADINFO(); gi.cbSize = Marshal.SizeOf(typeof(GUITHREADINFO));
     if (!GetGUIThreadInfo((uint)t, ref gi)) continue;
     IntPtr[] held = { gi.hwndActive, gi.hwndFocus, gi.hwndCapture }; string[] names = { "ativa", "foco", "captura" };
     for (int i = 0; i < 3; i++) { if (held[i] == IntPtr.Zero) continue; string c = ClassOf(held[i]);
       if (c == "VoltzAnnotationOverlay" || c == "VoltzDrawInput") return names[i] + " (" + c + ", thread " + t + ")"; } }
   return ""; }
}
'@
Add-Type $probeSource
$P = [VoltzProbe]
[void]$P::SetProcessDPIAware()

$vx = $P::GetSystemMetrics(76); $vy = $P::GetSystemMetrics(77); $vw = $P::GetSystemMetrics(78); $vh = $P::GetSystemMetrics(79)
function Crosses([int]$l, [int]$t, [int]$r, [int]$b, [int]$L2, [int]$T2, [int]$R2, [int]$B2) { return ($l -lt $R2 -and $r -gt $L2 -and $t -lt $B2 -and $b -gt $T2) }
# Retangulo de teste A DIREITA de todo o desktop virtual; recusa se cruzar o desktop ou algum monitor
function Set-TestRect([int]$w, [int]$h) {
    $script:ovX = $vx + $vw + 200; $script:ovY = $vy; $script:ovW = $w; $script:ovH = $h
    $script:overlayRect = "{0},{1},{2},{3}" -f $script:ovX, $script:ovY, $w, $h
    if (Crosses $script:ovX $script:ovY ($script:ovX + $w) ($script:ovY + $h) $vx $vy ($vx + $vw) ($vy + $vh)) { throw "retangulo de teste $($script:overlayRect) cruza o desktop virtual" }
    foreach ($s in [System.Windows.Forms.Screen]::AllScreens) {
        $sb = $s.Bounds
        if (Crosses $script:ovX $script:ovY ($script:ovX + $w) ($script:ovY + $h) $sb.Left $sb.Top $sb.Right $sb.Bottom) { throw "retangulo de teste $($script:overlayRect) sobre $($s.DeviceName)" }
    }
    Write-Host "retangulo de teste da sobreposicao: $($script:overlayRect) (desktop virtual ($vx,$vy) ${vw}x${vh})"
}

$videosDir = [Environment]::GetFolderPath('MyVideos')
function Videos-List { return ,@(Get-ChildItem -Force -LiteralPath $videosDir | ForEach-Object { "$($_.Name)|$($_.LastWriteTimeUtc.Ticks)|$($_.Attributes)" }) }
$videosBefore = Videos-List
function Hash-Dir([string]$d) {
    if (-not (Test-Path -LiteralPath $d)) { return "(ausente)" }
    return (Get-ChildItem -Recurse -File -Force -LiteralPath $d | Sort-Object FullName | ForEach-Object { $_.FullName.Substring($d.Length) + "|" + (Get-FileHash -Algorithm MD5 -LiteralPath $_.FullName).Hash }) -join "`n"
}
$cfgHashBefore = Hash-Dir $cfg

$checks = [ordered]@{}
function Check([string]$name, [bool]$ok) { $script:checks[$name] = $ok }

function Ws([string]$json) {
    $file = Join-Path $env:TEMP "voltz-smoke-draw-req.json"
    [IO.File]::WriteAllText($file, $json, (New-Object Text.UTF8Encoding $false))
    $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $file -Port $wsPort
    Remove-Item -Force $file -ErrorAction SilentlyContinue
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
function Shell-State { Start-Sleep -Milliseconds 400; return ((VendorShell "AnnotationState").estado | ConvertFrom-Json) }
function Feed-State { return ((Vendor "AnnotationFeedState").estado | ConvertFrom-Json) }
function Stroke-Json([string]$tool, [string]$color, [int]$width, [string]$points, [switch]$Open) {
    $req = [ordered]@{ ferr = $tool; cor = $color; esp = $width; pts = $points }
    if ($Open) { $req.aberto = $true }
    return (ConvertTo-Json $req -Compress)
}
function Pts($list) { return "[" + (($list | ForEach-Object { "[{0},{1}]" -f $_[0], $_[1] }) -join ",") + "]" }

function Kill-Launched([string]$why) {
    if ($script:proc -and -not $script:proc.HasExited) { Stop-Process -Id $script:proc.Id -Force }
    throw "BLOCKED $why - app (PID lancado) encerrado"
}

$script:overlays = @()
$script:asserts = 0
function Assert-DrawSafe([string]$where, [switch]$Quiet) {
    $procId = [uint32]$script:proc.Id
    $div = $false
    $list = $P::OfClass($procId, "VoltzAnnotationOverlay", [ref]$div)
    if ($div) { Kill-Launched "[$where] FindWindowEx e EnumWindows divergem nas sobreposicoes" }
    foreach ($id in $list) {
        $h = [IntPtr][Int64]$id
        if ($script:overlays -notcontains $id) { $script:overlays += $id }
        $ex = $P::GetWindowLongPtr($h, -20).ToInt64()
        $r = New-Object VoltzProbe+RECT; [void]$P::GetWindowRect($h, [ref]$r)
        [uint32]$aff = 99; [void]$P::GetWindowDisplayAffinity($h, [ref]$aff)
        $bad = @()
        if (($ex -band 0x080800A8) -ne 0x080800A8) { $bad += ("estilo 0x{0:X}" -f $ex) }
        if ($P::GetForegroundWindow() -eq $h) { $bad += "primeiro plano" }
        if ($r.L -lt $script:ovX -or $r.T -lt $script:ovY -or $r.R -gt ($script:ovX + $script:ovW) -or $r.B -gt ($script:ovY + $script:ovH)) { $bad += "fora do retangulo de teste ($($r.L),$($r.T),$($r.R),$($r.B))" }
        if (Crosses $r.L $r.T $r.R $r.B $vx $vy ($vx + $vw) ($vy + $vh)) { $bad += "cruza o desktop virtual" }
        if ($aff -ne 0x11) { $bad += ("afinidade 0x{0:X}" -f $aff) }
        if ($bad.Count -gt 0) { Kill-Launched ("[{0}] sobreposicao insegura hwnd={1}: {2}" -f $where, $id, ($bad -join '; ')) }
    }
    $divIn = $false
    $inputs = $P::OfClass($procId, "VoltzDrawInput", [ref]$divIn)
    if ($inputs.Count -gt 0 -or $divIn) { Kill-Launched "[$where] janela de entrada do modo desenho em teste ($($inputs -join ','))" }
    if ($P::VisibleTitled($procId, "VoltzDrawToolbar").Count -gt 0) { Kill-Launched "[$where] barra do modo desenho visivel em teste" }
    $holder = $P::VoltzHolder([int[]]@((Get-Process -Id $script:proc.Id).Threads | ForEach-Object { $_.Id }))
    if ($holder) { Kill-Launched "[$where] janela Voltz com $holder" }
    $script:asserts++
    if (-not $Quiet) { Write-Host ("assert[{0}] ok: {1} sobreposicao(oes), nenhuma janela de entrada nem barra" -f $where, $list.Count) }
}

# Vigia (job = outro processo): a mesma conferencia a cada 200 ms; falha = encerra ESSE PID e escreve BLOCKED
$watchdog = {
    param([int]$TargetPid, [int]$OvX, [int]$OvY, [int]$OvW, [int]$OvH, [string]$ProbeSource, [string]$StopFile, [string]$LogFile)
    Add-Type $ProbeSource
    $W = [VoltzProbe]
    [void]$W::SetProcessDPIAware()
    $vx = $W::GetSystemMetrics(76); $vy = $W::GetSystemMetrics(77); $vw = $W::GetSystemMetrics(78); $vh = $W::GetSystemMetrics(79)
    $rounds = 0
    while ($true) {
        if (Test-Path -LiteralPath $StopFile) { break }
        $p = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
        if (-not $p -or $p.HasExited) { break }
        $bad = @()
        $div = $false
        foreach ($id in $W::OfClass([uint32]$TargetPid, "VoltzAnnotationOverlay", [ref]$div)) {
            $h = [IntPtr][Int64]$id
            $ex = $W::GetWindowLongPtr($h, -20).ToInt64()
            $r = New-Object VoltzProbe+RECT; [void]$W::GetWindowRect($h, [ref]$r)
            [uint32]$aff = 99; [void]$W::GetWindowDisplayAffinity($h, [ref]$aff)
            if (($ex -band 0x080800A8) -ne 0x080800A8) { $bad += ("estilo 0x{0:X}" -f $ex) }
            if ($W::GetForegroundWindow() -eq $h) { $bad += "primeiro plano" }
            if ($r.L -lt $OvX -or $r.T -lt $OvY -or $r.R -gt ($OvX + $OvW) -or $r.B -gt ($OvY + $OvH)) { $bad += "fora do retangulo de teste" }
            if ($r.L -lt ($vx + $vw) -and $r.R -gt $vx -and $r.T -lt ($vy + $vh) -and $r.B -gt $vy) { $bad += "cruza o desktop virtual" }
            if ($aff -ne 0x11) { $bad += ("afinidade 0x{0:X}" -f $aff) }
        }
        $divIn = $false
        if ($W::OfClass([uint32]$TargetPid, "VoltzDrawInput", [ref]$divIn).Count -gt 0) { $bad += "janela de entrada do modo desenho" }
        if ($W::VisibleTitled([uint32]$TargetPid, "VoltzDrawToolbar").Count -gt 0) { $bad += "barra visivel" }
        $holder = $W::VoltzHolder([int[]]@($p.Threads | ForEach-Object { $_.Id }))
        if ($holder) { $bad += "janela Voltz com $holder" }
        if ($bad.Count -gt 0) {
            Stop-Process -Id $TargetPid -Force
            Add-Content -LiteralPath $LogFile ("BLOCKED vigia: " + ($bad -join '; '))
            return
        }
        $rounds++
        Start-Sleep -Milliseconds 200
    }
    Add-Content -LiteralPath $LogFile ("vigia: {0} conferencias sem problema" -f $rounds)
}

function Assert-NoRealCapture {
    $inputs = @((Ws '[{"requestType":"GetInputList"}]')[0].data.inputs)
    $real = @($inputs | Where-Object { $_.inputKind -match 'monitor_capture|window_capture|game_capture|dshow_input' })
    if ($real.Count -gt 0) { throw ("captura real presente - abortando: " + (($real | ForEach-Object { $_.inputName }) -join ', ')) }
}
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

function New-CleanConfig {
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
    New-Item -ItemType Directory -Force $wsDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":' + $wsPort + '}')
    [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[BasicWindow]`r`nHideOBSWindowsFromCapture=false`r`n[Voltz]`r`nNextProjectName=Smoke desenho`r`n", (New-Object Text.UTF8Encoding $false))
}
function Set-TestEnv([bool]$freeze) {
    $env:VOLTZ_TEST_LIVE = "1"
    $env:VOLTZ_TEST_PRESET_STANDINS = "1"
    $env:VOLTZ_TEST_OVERLAY_RECT = $script:overlayRect
    if ($freeze) { $env:VOLTZ_DEBUG_FREEZE_UI = "1" } else { Remove-Item Env:VOLTZ_DEBUG_FREEZE_UI -ErrorAction SilentlyContinue }
    Remove-Item Env:VOLTZ_TEST_ANNOT_BURNED, Env:VOLTZ_TEST_SCREEN_LAYERS -ErrorAction SilentlyContinue
}

function Start-App([string]$tag) {
    if (@(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue).Count -gt 0) { throw "porta $wsPort ocupada antes de abrir o app ($tag)" }
    $script:proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
    $script:wdLog = Join-Path $logDir "vigia-$tag.txt"
    $script:wdStop = Join-Path $logDir "vigia-$tag.stop"
    $script:job = Start-Job -ScriptBlock $watchdog -ArgumentList $script:proc.Id, $script:ovX, $script:ovY, $script:ovW, $script:ovH, $probeSource, $script:wdStop, $script:wdLog
    Start-Sleep -Seconds 15
    if ($script:proc.HasExited) { return $false }
    $script:shell = [IntPtr]::Zero
    for ($i = 0; $i -lt 30 -and $script:shell -eq [IntPtr]::Zero; $i++) {
        $script:proc.Refresh()
        $h = $script:proc.MainWindowHandle
        if ($h -ne [IntPtr]::Zero -and $P::OwnerOf($h) -eq $script:proc.Id -and $P::ClassOf($h) -notmatch '^Voltz(AnnotationOverlay|DrawInput)$') { $script:shell = $h } else { Start-Sleep -Milliseconds 500 }
    }
    $owners = @(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue | ForEach-Object { [int]$_.OwningProcess })
    if (-not ($owners -contains $script:proc.Id)) { Kill-Launched "a porta $wsPort nao e do app lancado ($($owners -join ','))" }
    Write-Host "[$tag] PID lancado $($script:proc.Id); casca hwnd=$($script:shell.ToInt64()) classe '$($P::ClassOf($script:shell))'"
    Assert-DrawSafe "$tag abertura"
    return ($script:shell -ne [IntPtr]::Zero)
}

function Stop-Watchdog([string]$tag) {
    if (-not $script:job) { return }
    New-Item -ItemType File -Force $script:wdStop | Out-Null
    [void](Wait-Job $script:job -Timeout 20)
    Remove-Job $script:job -Force
    $script:job = $null
    Remove-Item -LiteralPath $script:wdStop -ErrorAction SilentlyContinue
    $text = if (Test-Path -LiteralPath $script:wdLog) { (Get-Content -LiteralPath $script:wdLog) -join ' ' } else { "(sem registro)" }
    Write-Host "[$tag] $text"
    Check "${tag}: vigia sem problema ($text)" ($text -like "vigia: * conferencias sem problema*" -and $text -notlike "*BLOCKED*")
}

$script:forced = 0
function Close-App([string]$tag) {
    $clean = $false
    if ($script:proc -and -not $script:proc.HasExited) {
        $procId = $script:proc.Id
        $shellOk = ($script:shell -ne [IntPtr]::Zero) -and $P::IsWindow($script:shell) -and ($P::OwnerOf($script:shell) -eq $procId) -and ($P::ClassOf($script:shell) -notmatch '^Voltz(AnnotationOverlay|DrawInput)$')
        if ($shellOk) { [void]$P::PostMessage($script:shell, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) } else { [void]$script:proc.CloseMainWindow() }
        $clean = $script:proc.WaitForExit(30000)
        if (-not $clean) { $script:forced++; Write-Host "[$tag] FECHAMENTO FORCADO (30 s)"; Stop-Process -Id $procId -Force; [void]$script:proc.WaitForExit(10000) }
        $div = $false
        $left = $P::OfClass([uint32]$procId, "VoltzAnnotationOverlay", [ref]$div).Count + $P::OfClass([uint32]$procId, "VoltzDrawInput", [ref]$div).Count
        $orphans = @($script:overlays | Where-Object { $P::IsWindow([IntPtr][Int64]$_) }).Count
        Check "${tag}: nenhuma janela Voltz depois de fechar (do PID $left, orfas $orphans)" ($left -eq 0 -and $orphans -eq 0)
    }
    Stop-Watchdog $tag
    $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
    if ($log) { Copy-Item $log.FullName (Join-Path $logDir "$tag.txt") -Force }
    if ($clean) { Check "${tag}: log - Number of memory leaks: 0" (LogHas $tag "Number of memory leaks: 0") }
    return $clean
}
function LogHas([string]$tag, [string]$text) {
    $file = Join-Path $logDir "$tag.txt"
    return [bool]((Test-Path $file) -and (Select-String -LiteralPath $file -SimpleMatch $text -Encoding UTF8 -Quiet))
}
function LogCount([string]$tag, [string]$text) {
    $file = Join-Path $logDir "$tag.txt"
    if (-not (Test-Path $file)) { return 0 }
    return @(Select-String -LiteralPath $file -SimpleMatch $text -Encoding UTF8).Count
}
function Apply-Preset([string]$tag) {
    Check "${tag}: preset 'So tela / So camera' (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    [void](Ws ('[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":' + (ConvertTo-Json $sceneScreen) + '}}]'))
    Assert-DrawSafe "$tag preset"
}

if (Test-Path $cfg) { Move-Item $cfg $bak }
if (Test-Path $logDir) { Remove-Item -Recurse -Force $logDir }
New-Item -ItemType Directory -Force $logDir | Out-Null
$proc = $null
$script:job = $null
try {
    # ---------------- Fase A: recusa, estilo, barra ----------------
    Set-TestRect 640 360
    New-CleanConfig
    Set-TestEnv $false
    Check "A: app aberto" (Start-App "A")
    Apply-Preset "A"
    $st = Shell-State
    Check "A: modo desenho desligado ao abrir" (-not $st.modo_desenho)
    for ($i = 1; $i -le 3; $i++) {
        $r = Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.Draw"}}]'
        Check "A: TriggerHotkeyByName VoltzStudio.Draw #$i aceito" ($r.Count -eq 1 -and $r[0].ok)
        Start-Sleep -Milliseconds 500
        Assert-DrawSafe "A atalho $i"
        $st = Shell-State
        Check "A: recusado #$i (modo_desenho $($st.modo_desenho), recusa '$($st.desenho_recusa)')" ((-not $st.modo_desenho) -and $st.desenho_recusa -eq "teste")
    }
    $style = '{"ferr":"seta","cor":"#4DA3FF","esp":8,"somem_apos_s":2}'
    [void](VendorShell "SetAnnotationStyle" (ConvertTo-Json @{ estilo = $style } -Compress))
    $st = Shell-State
    Check "A: estilo aplicado ($($st.estilo))" ($st.estilo -eq $style)
    [void](VendorShell "InjectStroke" '{"pts":"[[400,400],[1200,800]]"}')
    $st = Shell-State
    Assert-DrawSafe "A traco"
    $last = if ($st.ultimo) { $st.ultimo | ConvertFrom-Json } else { $null }
    Check "A: traco sem ferr/cor/esp usa o estilo (seta #4DA3FF 8)" ($null -ne $last -and $last.ferr -eq "seta" -and $last.cor -eq "#4DA3FF" -and $last.esp -eq 8)
    Start-Sleep -Seconds 3
    $st = Shell-State
    Check "A: somem apos 2 s (na tela: $(@($st.visiveis).Count))" (@($st.visiveis).Count -eq 0)
    $bar = VendorShell "ToolbarImage"
    if ($bar.png) { [IO.File]::WriteAllBytes((Join-Path $logDir "barra-A.png"), [Convert]::FromBase64String($bar.png)) }
    Check "A: barra desenhada fora da tela, sem janela nativa ($($bar.largura)x$($bar.altura), nativa $($bar.janela_nativa))" ($bar.largura -ge 400 -and $bar.altura -ge 36 -and -not $bar.janela_nativa)
    Assert-DrawSafe "A barra"
    Check "A: encerrou limpo" (Close-App "A")
    Check "A: log - desligado ao abrir" (LogHas "A" "[voltz] modo desenho: desligado ao abrir")
    Check "A: log - 3 recusas (teste)" ((LogCount "A" "[voltz] modo desenho: recusado (teste)") -ge 3)
    Check "A: log - o modo desenho nunca ligou" (-not (LogHas "A" "[voltz] modo desenho: ligado"))
    Check "A: log - janelas fantasma desligadas" (LogHas "A" "[voltz] janelas fantasma do Windows desligadas no processo")
    $ini = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $voltzCfg "user.ini")
    Check "A: user.ini com o AnnotationStyle da barra" ($ini -match [regex]::Escape("AnnotationStyle=$style"))

    # ---------------- Fase B: redesenho parcial (2560x1440, escala 1) e medidas ----------------
    Set-TestRect 2560 1440
    New-CleanConfig
    Set-TestEnv $false
    Check "B: app aberto" (Start-App "B")
    Apply-Preset "B"
    $pts = @()
    for ($i = 0; $i -le 601; $i++) { $pts += , @([int](200 + $i * 3.6), [int](700 + 200 * [Math]::Sin($i / 40.0))) }
    [void](VendorShell "InjectStroke" (Stroke-Json "caneta" "#D4F53C" 8 (Pts $pts[0..1]) -Open))
    Start-Sleep -Milliseconds 300
    for ($batch = 0; $batch -lt 3; $batch++) {
        $reqs = @()
        for ($k = 0; $k -lt 20; $k++) {
            $j = $batch * 20 + $k
            $reqs += @{ requestType = "CallVendorRequest"; requestData = @{ vendorName = "voltz-teste-casca"; requestType = "ExtendStroke"; requestData = @{ pts = (Pts $pts[(2 + 10 * $j)..(11 + 10 * $j)]) } } }
        }
        [void](Ws (ConvertTo-Json -InputObject $reqs -Depth 6 -Compress))
        Start-Sleep -Milliseconds 300
        Assert-DrawSafe "B lote $batch"
    }
    [void](VendorShell "EndStroke")
    $probe = "[[{0},{1}],[{2},{3}]]" -f $pts[0][0], $pts[0][1], $pts[300][0], $pts[300][1]
    [void](VendorShell "ProbeOverlay" (ConvertTo-Json @{ pts = $probe } -Compress))
    $st = Shell-State
    $feed = Feed-State
    $ov = $st.sobreposicao
    Write-Host ("B: sobreposicao 2560x1440: {0} parciais (media {1} us, max {2} us), {3} completa(s) (max {4} us)" -f $ov.parciais, $ov.parcial_us_media, $ov.parcial_us_max, $ov.completas, $ov.completa_us_max)
    Write-Host ("B: fonte: {0} parciais, {1} completas, ultimo {2} us, max {3} us" -f $feed.parciais, $feed.completas, $feed.raster_us_ultimo, $feed.raster_us_max)
    Check "B: sobreposicao redesenhou so o retangulo sujo ($($ov.parciais) parciais, $($ov.completas) completas)" ($ov.parciais -ge 60 -and $ov.completas -le 2)
    Check "B: parcial mais rapido que completo (media $($ov.parcial_us_media) us, completo max $($ov.completa_us_max) us)" ($ov.parcial_us_media -lt $ov.completa_us_max)
    Check "B: parcial abaixo de 100 ms (max $($ov.parcial_us_max) us)" ($ov.parcial_us_max -lt 100000)
    Check "B: fonte redesenhou parcialmente ($($feed.parciais) parciais, $($feed.completas) completas)" ($feed.parciais -ge 1 -and $feed.completas -ge 1)
    Check "B: buffer da sobreposicao com o traco ($($st.amostras -join ','))" (@($st.amostras).Count -eq 2 -and $st.amostras[0] -eq "#FFD4F53C" -and $st.amostras[1] -eq "#FFD4F53C")
    $img = Scene-Image $sceneScreen
    $c = $img.GetPixel([int]($pts[300][0] / 4), [int]($pts[300][1] / 4)); $img.Dispose()
    Check "B: programa com o traco ($c)" (Near $c 212 245 60 40)
    [void](VendorShell "UndoStroke")
    [void](VendorShell "ProbeOverlay" (ConvertTo-Json @{ pts = $probe } -Compress))
    $st = Shell-State
    Assert-DrawSafe "B desfazer"
    Check "B: desfazer limpa a sobreposicao ($($st.amostras -join ','), visivel $($st.sobreposicao.visivel))" ((-not $st.sobreposicao.visivel) -and $st.amostras[0] -eq "#00000000" -and $st.amostras[1] -eq "#00000000")
    Check "B: encerrou limpo" (Close-App "B")

    # ---------------- Fase C: UI travada 8 s, sem janela fantasma ----------------
    Set-TestRect 640 360
    New-CleanConfig
    Set-TestEnv $true
    Check "C: app aberto" (Start-App "C")
    Apply-Preset "C"
    [void](VendorShell "InjectStroke" (Stroke-Json "linha" "#D4F53C" 16 "[[640,720],[1920,720]]"))
    $st = Shell-State
    Assert-DrawSafe "C traco"
    Check "C: sobreposicao visivel (fora da area de trabalho)" ($st.sobreposicao.visivel)
    $watch = [long[]]@($script:shell.ToInt64(), [long]$st.sobreposicao.hwnd)
    $r = Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.DebugFreezeUi"}}]'
    Check "C: atalho de depuracao aceito" ($r.Count -eq 1 -and $r[0].ok)
    $hung = $false; $ghosts = 0
    for ($i = 0; $i -lt 44; $i++) {
        Start-Sleep -Milliseconds 250
        if ($P::IsHungAppWindow($script:shell)) { $hung = $true }
        $ghosts = [Math]::Max($ghosts, $P::GhostsOver($watch))
        Assert-DrawSafe "C travada $i" -Quiet
    }
    Check "C: a UI travou de verdade (IsHungAppWindow)" $hung
    Check "C: nenhuma janela fantasma sobre a casca ou a sobreposicao ($ghosts)" ($ghosts -eq 0)
    Start-Sleep -Seconds 2
    $st = Shell-State
    Check "C: a UI voltou ($(@($st.visiveis).Count) traco na tela)" (@($st.visiveis).Count -eq 1)
    Check "C: encerrou limpo" (Close-App "C")
    Check "C: log - atalho de depuracao ligado" (LogHas "C" "[voltz] depuracao (VOLTZ_DEBUG_FREEZE_UI): atalho VoltzStudio.DebugFreezeUi ligado")
    Check "C: log - UI travada 8000 ms e de volta" ((LogHas "C" "[voltz] depuracao: thread da UI travada por 8000 ms") -and (LogHas "C" "[voltz] depuracao: thread da UI de volta"))
} catch {
    Check "sem excecao ($($_.Exception.Message))" $false
} finally {
    if ($script:proc -and -not $script:proc.HasExited) { [void](Close-App "final") }
    if ($script:job) { Stop-Watchdog "final" }
    Remove-Item Env:VOLTZ_TEST_LIVE, Env:VOLTZ_TEST_PRESET_STANDINS, Env:VOLTZ_TEST_OVERLAY_RECT, Env:VOLTZ_DEBUG_FREEZE_UI -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 5
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    if (Test-Path $bak) { Move-Item $bak $cfg }
}

$orphans = @($script:overlays | Where-Object { $P::IsWindow([IntPtr][Int64]$_) })
Check "nenhuma sobreposicao orfa ($($script:overlays.Count) vista(s))" ($script:overlays.Count -ge 1 -and $orphans.Count -eq 0)
Check "seguranca: $($script:asserts) Assert-DrawSafe, $($script:forced) fechamento(s) forcado(s)" ($script:asserts -gt 0 -and $script:forced -eq 0)
$videosAfter = Videos-List
Check "Videos intacta (antes $($videosBefore.Count), depois $($videosAfter.Count))" (-not (Compare-Object $videosBefore $videosAfter))
$leftBaks = @(Get-ChildItem -LiteralPath $rundir -Directory -Force -Filter "config.*bak*" -ErrorAction SilentlyContinue)
Check "config portatil restaurada igual e sem sobra de backup" (-not (Test-Path $bak) -and $leftBaks.Count -eq 0 -and ($cfgHashBefore -eq (Hash-Dir $cfg)))
$failed = 0
foreach ($k in $checks.Keys) {
    if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
}
Write-Host "logs: $logDir"
if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE DESENHO OK"
```

- [ ] **Step 2: Conferir ASCII e segurança do script**

```bash
grep -nP '[^\x00-\x7F]' tools/voltz-smoke-draw.ps1; echo "nao-ascii=$?"
grep -nE "SendInput|SetCursorPos|mouse_event|keybd_event|SendKeys|Cursor\]::Position|SetForegroundWindow|BlockInput|ClipCursor|taskkill|Stop-Process -Name|explorer|Desenhar" tools/voltz-smoke-draw.ps1; echo "proibidos=$?"
```
Expected: `nao-ascii=1` e `proibidos=1` (nenhuma linha: nem o nome do botão "Desenhar" aparece no smoke).

- [ ] **Step 3: Rodar o smoke**

Run: SMOKE DESENHO.
Expected: todas as linhas `OK`, última `SMOKE DESENHO OK` (~2 min). Anote no relatório as duas linhas `B: sobreposicao
...` e `B: fonte ...` (as medidas) e confira `%TEMP%\voltz-smoke-draw-logs\barra-A.png` (Read). Se `B: fonte redesenhou
parcialmente` falhar com `parciais 0`, os lotes chegaram num quadro só: aumente o `Start-Sleep` entre lotes para 600 ms
e rode de novo. Se qualquer linha `BLOCKED` aparecer, **pare e reporte** (não afrouxe a conferência).

- [ ] **Step 4: Grep de segurança de todo o código**

Run: GREP DE SEGURANÇA (Convenções).
Expected: como na Task 5 (`foco-fora-da-excecao=1`, `injecao=1`, C só `CursorTracker.cpp` e `LiveMouse.cpp`, D com
`1 ReleaseCapture(`, `1 SetCapture(`, `2 SetForegroundWindow(`).

- [ ] **Step 5: Commit**

```bash
git add tools/voltz-smoke-draw.ps1
git commit -m "test(voltz): smoke do modo desenho (recusa em teste, estilo, barra fora da tela, retangulo sujo medido, UI travada sem fantasma)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 9: Demonstrações manuais — conserto do `-Close` da demo do 4B e `tools/voltz-draw-demo.ps1`

**Files:**
- Modify: `tools/voltz-annot-demo.ps1`
- Create: `tools/voltz-draw-demo.ps1`

- [ ] **Step 1: `voltz-annot-demo.ps1 -Close` apaga a config da demo quando não havia uma antes**

Em `tools/voltz-annot-demo.ps1`:
1. No comentário do topo, trocar
```
  -Close: fecha o Voltz aberto pelo -Launch (pelo PID salvo, com WM_CLOSE na janela da casca) e restaura a config
    portatil.
```
por
```
  -Close: fecha o Voltz aberto pelo -Launch (pelo PID salvo, com WM_CLOSE na janela da casca) e restaura a config
    portatil - ou apaga a da demonstracao (obs-websocket sem senha), se nao havia config portatil antes do -Launch.
```
2. Depois de `$pidFile = Join-Path $env:TEMP "voltz-annot-demo.pid"`, acrescentar
`$marker = Join-Path $cfg "voltz-annot-demo.marker"`.
3. Trocar a função `Restore-Config` inteira por:
```powershell
function Restore-Config {
    Start-Sleep -Seconds 5
    if (Test-Path $bak) {
        if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
        Move-Item $bak $cfg
    } elseif (Test-Path -LiteralPath $marker) {
        # nao havia config portatil antes do -Launch: a da demonstracao (websocket sem senha na 4455) sai inteira
        Remove-Item -Recurse -Force $cfg
    }
}
```
4. No `-Launch`, logo depois da linha que grava o `user.ini`
(`[IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), ...)`), acrescentar:
```powershell
        # marca: esta config e da demonstracao (o -Close a apaga se nao havia config portatil antes)
        Set-Content -LiteralPath $marker -Value "config criada pelo -Launch da demonstracao das anotacoes"
```

- [ ] **Step 2: Demonstração manual do modo desenho**

Criar `tools/voltz-draw-demo.ps1` (ASCII puro):
```powershell
<#
  Demonstracao MANUAL do modo desenho (Plano 4C): o usuario abre, liga o modo desenho e desenha. Nao e teste automatico.
  -Launch: guarda a config portatil (config.drawdemo-bak), cria uma limpa (pt-BR, sem confirmacao ao sair) e abre o
    Voltz portatil SEM nenhuma variavel de teste (com VOLTZ_TEST_LIVE o modo desenho e recusado). Com -Freeze, liga
    VOLTZ_DEBUG_FREEZE_UI: o atalho Ctrl+Shift+F12 trava a interface por 8 s (para ver o vigia do modo desenho). Na tela
    Presets, aplique VOCE MESMO um preset com a Tela real (ex.: Tutorial) e ligue o modo desenho com Ctrl+Shift+D ou com
    o botao "Desenhar" da tela Gravar.
  -Log: mostra as linhas do log do Voltz aberto sobre o modo desenho, as anotacoes e a depuracao.
  -Close: fecha o Voltz aberto pelo -Launch (PID salvo, WM_CLOSE na janela da casca guardada) e restaura a config
    portatil (ou apaga a da demonstracao, se nao havia uma antes).
  Seguranca: este script NAO liga o modo desenho, nao move o mouse, nao clica, nao digita e nao poe janela alguma em
  primeiro plano; so abre e fecha o Voltz que ele mesmo lancou (PID e executavel conferidos). O modo desenho sai com Esc,
  clique direito, o mesmo atalho, o botao Sair da barra, clique em outra janela ou monitor, Alt+Tab, tecla Windows, 30 s
  parado ou, com a interface travada, sozinho em ~1 s.
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Launch [-Freeze]
       powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Log
       powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Close
#>
param(
    [switch]$Launch,
    [switch]$Freeze,
    [switch]$Log,
    [switch]$Close,
    [string]$Config = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rundir = Join-Path $root "build_x64\rundir\$Config"
$bin = Join-Path $rundir "bin\64bit"
$exe = Join-Path $bin "VoltzStudio.exe"
$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.drawdemo-bak"
$marker = Join-Path $cfg "voltz-draw-demo.marker"
$pidFile = Join-Path $env:TEMP "voltz-draw-demo.pid"
$testVars = @("VOLTZ_TEST_LIVE", "VOLTZ_TEST_PRESET_STANDINS", "VOLTZ_TEST_OVERLAY_RECT", "VOLTZ_TEST_ANNOT_BURNED", "VOLTZ_TEST_SCREEN_LAYERS", "VOLTZ_DEBUG_FREEZE_UI")

# P/Invoke de leitura; a unica escrita e PostMessage(WM_CLOSE) na janela da casca do PID aberto pelo -Launch
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class VoltzDrawDemoWin {
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder sb, int n);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
 public static string ClassOf(IntPtr h) { var sb = new System.Text.StringBuilder(256); GetClassName(h, sb, 256); return sb.ToString(); }
 public static uint OwnerOf(IntPtr h) { uint owner; GetWindowThreadProcessId(h, out owner); return owner; } }
'@
$Win = [VoltzDrawDemoWin]

# PID e janela da casca salvos pelo -Launch; o processo precisa ser o VoltzStudio.exe deste build
function Launched {
    if (-not (Test-Path -LiteralPath $pidFile)) { return $null }
    $parts = ([string](Get-Content -LiteralPath $pidFile -TotalCount 1)).Split('|')
    $id = [int]$parts[0]
    $hwnd = if ($parts.Count -gt 1) { [IntPtr][Int64]$parts[1] } else { [IntPtr]::Zero }
    $p = Get-Process -Id $id -ErrorAction SilentlyContinue
    if (-not $p -or $p.HasExited -or $p.ProcessName -ne "VoltzStudio" -or $p.Path -ne $exe) { return $null }
    return @{ Process = $p; Shell = $hwnd }
}

function Restore-Config {
    Start-Sleep -Seconds 5
    if (Test-Path $bak) {
        if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
        Move-Item $bak $cfg
    } elseif (Test-Path -LiteralPath $marker) {
        # nao havia config portatil antes do -Launch: a da demonstracao sai inteira
        Remove-Item -Recurse -Force $cfg
    }
}

if ($Launch) {
    if (-not (Test-Path $exe)) { throw "executavel nao encontrado: $exe" }
    if (Test-Path $bak) { throw "ja existe $bak (rode -Close antes)" }
    if (Launched) { throw "a demonstracao ja esta aberta (rode -Close antes)" }
    if (Test-Path $cfg) { Move-Item $cfg $bak }
    # o Voltz herda o ambiente deste PowerShell: nenhuma variavel de teste vai junto (o modo desenho seria recusado)
    $saved = @{}
    foreach ($name in $testVars) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, "Process"); Remove-Item "Env:$name" -ErrorAction SilentlyContinue }
    $proc = $null
    try {
        $voltzCfg = Join-Path $cfg "Voltz Studio"
        New-Item -ItemType Directory -Force $voltzCfg | Out-Null
        [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n", (New-Object Text.UTF8Encoding $false))
        Set-Content -LiteralPath $marker -Value "config criada pelo -Launch da demonstracao do modo desenho"
        if ($Freeze) { $env:VOLTZ_DEBUG_FREEZE_UI = "1" }
        $proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
        # janela da casca para o WM_CLOSE do -Close (nunca a sobreposicao nem a janela de entrada)
        $shell = [IntPtr]::Zero
        for ($i = 0; $i -lt 60 -and $shell -eq [IntPtr]::Zero -and -not $proc.HasExited; $i++) {
            Start-Sleep -Milliseconds 500
            $proc.Refresh()
            $h = $proc.MainWindowHandle
            if ($h -ne [IntPtr]::Zero -and $Win::OwnerOf($h) -eq $proc.Id -and $Win::ClassOf($h) -notmatch '^Voltz(AnnotationOverlay|DrawInput)$') { $shell = $h }
        }
        if ($proc.HasExited) { throw "o Voltz fechou logo depois de abrir" }
        Set-Content -LiteralPath $pidFile -Value ("{0}|{1}" -f $proc.Id, $shell.ToInt64())
    } catch {
        if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
        Remove-Item -LiteralPath $pidFile -ErrorAction SilentlyContinue
        Restore-Config
        throw
    } finally {
        foreach ($name in $testVars) {
            if ($null -ne $saved[$name]) { Set-Item "Env:$name" $saved[$name] } else { Remove-Item "Env:$name" -ErrorAction SilentlyContinue }
        }
    }
    Write-Host "Voltz aberto (PID $($proc.Id)). Aplique um preset com a Tela real (ex.: Tutorial) e ligue o modo desenho com Ctrl+Shift+D ou o botao Desenhar."
    if ($Freeze) { Write-Host "Depuracao: Ctrl+Shift+F12 trava a interface por 8 s (o modo desenho deve sair sozinho em ~1 s)." }
    exit 0
}

if ($Log) {
    $logs = Join-Path $cfg "Voltz Studio\logs"
    $last = Get-ChildItem -LiteralPath $logs -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $last) { throw "nenhum log em $logs (rode -Launch antes)" }
    Select-String -LiteralPath $last.FullName -Encoding UTF8 -Pattern '\[voltz\] (modo desenho|anotacoes|janelas fantasma|depuracao)|\[voltz-sources\] anotacoes' | ForEach-Object { $_.Line }
    exit 0
}

if ($Close) {
    $app = Launched
    if ($app) {
        $p = $app.Process
        $shellOk = ($app.Shell -ne [IntPtr]::Zero) -and $Win::IsWindow($app.Shell) -and ($Win::OwnerOf($app.Shell) -eq $p.Id) -and ($Win::ClassOf($app.Shell) -notmatch '^Voltz(AnnotationOverlay|DrawInput)$')
        if ($shellOk) { [void]$Win::PostMessage($app.Shell, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) } else { [void]$p.CloseMainWindow() }
        if (-not $p.WaitForExit(30000)) { Write-Host "o Voltz nao fechou em 30 s: encerrando o PID $($p.Id)"; Stop-Process -Id $p.Id -Force }
    }
    Remove-Item -LiteralPath $pidFile -ErrorAction SilentlyContinue
    Restore-Config
    Write-Host "Voltz fechado e config portatil restaurada."
    exit 0
}

Write-Host "use -Launch [-Freeze], -Log ou -Close"
```

- [ ] **Step 3: Conferir ASCII e segurança dos dois scripts**

```bash
grep -nP '[^\x00-\x7F]' tools/voltz-draw-demo.ps1 tools/voltz-annot-demo.ps1; echo "nao-ascii=$?"
grep -nE "SendInput|SetCursorPos|mouse_event|keybd_event|SendKeys|Cursor\]::Position|SetForegroundWindow|BlockInput|ClipCursor|TriggerHotkey|taskkill|Stop-Process -Name|explorer" tools/voltz-draw-demo.ps1 tools/voltz-annot-demo.ps1; echo "proibidos=$?"
```
Expected: `nao-ascii=1` e `proibidos=1`.

- [ ] **Step 4: Conferir o `-Close` da demo do 4B sem config portátil antes (sem ligar nada)**

Sem nenhum Voltz/OBS aberto e **sem** config portátil (se `build_x64/rundir/RelWithDebInfo/config` existir, mova-a
antes para `config.closetest-bak` e devolva no fim):
```bash
powershell -ExecutionPolicy Bypass -File tools/voltz-annot-demo.ps1 -Launch
powershell -ExecutionPolicy Bypass -File tools/voltz-annot-demo.ps1 -Close
ls build_x64/rundir/RelWithDebInfo/ | grep -c "^config$"
```
Expected: `Voltz aberto (PID ...)`, depois `Voltz fechado e config portatil restaurada.` e `0` (a config da demo foi
apagada). Nenhum traço é desenhado neste passo (só abre e fecha).

- [ ] **Step 5: Commit**

```bash
git add tools/voltz-annot-demo.ps1 tools/voltz-draw-demo.ps1
git commit -m "feat(voltz): demo manual do modo desenho e -Close da demo das anotacoes apaga a config criada por ela

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: Verificação, registro, poda do `CLAUDE.md` e push

**Files:** `docs/voltz/backlog-plano-4.md`, `docs/voltz/upstream-patches.md` (conferência), `CLAUDE.md`, `AGENTS.md`,
`docs/HISTORICO.md`

- [ ] **Step 1:** COMPILAR TUDO (segundo plano), RODAR TESTES (esperado **170** casos), SMOKE, SMOKE GRAVAÇÃO, SMOKE
  PROJETO, SMOKE PROJETO `-ProgramOnly`, SMOKE MOUSE, SMOKE ANOTAÇÕES e SMOKE DESENHO (um de cada vez). Todos OK.
  GREP DE SEGURANÇA como na Task 5.
- [ ] **Step 2:** Captura final **só da janela** da casca (bloco de teste, `$ovW = 640; $ovH = 360`; preset "Só tela /
  Só câmera" por UIA; `Shot` = `PrintWindow` da casca guardada, como no 4B):
```powershell
  [void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
  $r = New-Object VoltzProbe+RECT; [void]$P::GetWindowRect($script:shell, [ref]$r)
  $b = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [System.Drawing.Graphics]::FromImage($b)
  Add-Type -Namespace V -Name Pw -MemberDefinition '[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);'
  $d = $g.GetHdc(); [void][V.Pw]::PrintWindow($script:shell, $d, 2); $g.ReleaseHdc($d); $g.Dispose(); $b.Save("$sp\4c-gravar.png"); $b.Dispose()
  Close-App
```
  A barra inferior mostra **Desenhar** (lápis) antes de **Marcador**; o botão está desmarcado (o modo nunca ligou).
  Descreva a imagem no relatório, junto com a `barra-A.png` do smoke. Nenhuma captura da tela inteira, nenhum quadro de
  vídeo, nenhuma captura da sobreposição.
- [ ] **Step 3:** Nenhum arquivo do OBS mudou:
  `git diff 39bf223cb..HEAD --stat -- . ':(exclude)frontend/voltz' ':(exclude)plugins/voltz-sources' ':(exclude)docs' ':(exclude)tools' ':(exclude)frontend/data/locale/voltz-pt-BR.ini' ':(exclude)frontend/data/locale/voltz-en-US.ini' ':(exclude)frontend/cmake/feature-voltz.cmake' ':(exclude)CLAUDE.md' ':(exclude)AGENTS.md'`
  → sem saída; `git diff 39bf223cb..HEAD -- plugins/CMakeLists.txt docs/voltz/upstream-patches.md` → sem saída.
- [ ] **Step 4: Backlog do Plano 4** — em `docs/voltz/backlog-plano-4.md`:
  1. Logo depois da seção `## Feito no Plano 4B` (antes de `## Próximo: Plano 4C (modo desenho)`), acrescentar:
```markdown
## Feito no Plano 4C
- **Núcleo:** `draw-session` — `DrawSession` (entrada → `Begin/Extend/End/Undo/Exit`, pixels do desktop → captura com a
  borda presa) com todas as saídas garantidas (Esc, clique direito, perda de ativação/captura, Alt+F4, troca de
  monitores, 30 s parado, vigia: UI 1 s sem pulsar ou pulso nunca dado) e `checkDrawEntry` (saindo > `VOLTZ_TEST_LIVE` >
  sem alvo); retângulo sujo no `annotation-raster` (`strokeBounds`, `rasterizeStrokeClipped`, `redrawRegion`,
  `appendDirtyRect`, `DirtyHistory`) byte a byte igual ao desenho inteiro.
- **Entrada:** `annotate/DrawInputWindow.cpp` — o único arquivo com captura/foco (`SetCapture`/`ReleaseCapture` entre
  apertar e soltar, `SetForegroundWindow` ao nascer e para devolver o primeiro plano) — janela `VoltzDrawInput` numa
  thread própria (Per-Monitor v2), alfa 1/255, sempre no topo, fora da captura, só sobre o monitor da Tela, sem dono,
  buraco na região para a barra; recusa `VOLTZ_TEST_LIVE` de novo antes de criar a thread.
- **Casca:** `DrawModeController` (atalho `VoltzStudio.Draw` Ctrl+Shift+D, `VoltzStudio.UndoAnnotation` e
  `VoltzStudio.ClearAnnotations` sem tecla padrão, botão **Desenhar** na tela Gravar, pulso de 250 ms, alvo conferido a
  cada 1 s, log com as medidas ao sair); `DrawToolbar` (6 ferramentas, 4+8 cores, 4 espessuras, desfazer, limpar,
  somem após nunca/3/5/10 s, sair; sem foco, `isOBSProjectorWindow`, fora da captura, aviso de "queimadas"); estilo
  salvo em `user.ini [Voltz] AnnotationStyle`; cliques sem onda no modo desenho; `DisableProcessWindowsGhosting`.
- **Desempenho:** sobreposição com `UpdateLayeredWindowIndirect` + `prcDirty`; fonte redesenha só a região (o envio da
  textura segue inteiro); medidas no estado de teste, no log do modo desenho e no log do plugin (a cada 600).
- **Riscos herdados resolvidos:** fantasma (desligado no processo + vigia local na thread de entrada), `ensureClickThrough`
  devolve o `WS_EX_TOPMOST` com `SetWindowPos`, Raw Input intocado (o modo desenho não registra nada), `-Close` da demo
  apaga a config que ela criou.
- Testes: 170 casos; `tools/voltz-smoke-draw.ps1` (fases A–C); `tools/voltz-draw-demo.ps1` (demo manual, `-Freeze`);
  pedidos novos `SetAnnotationStyle` e `ToolbarImage`; `VOLTZ_DEBUG_FREEZE_UI` (atalho `VoltzStudio.DebugFreezeUi`).
```
  2. Trocar a seção `## Próximo: Plano 4C (modo desenho)` inteira (título e parágrafo) por:
```markdown
## Próximo: Plano 4D (texto, Ajustes, todos os monitores)
Ferramenta de texto (`"ferr":"texto"`, fonte rasterizada também no plugin), Ajustes › Anotações (estilo padrão com
seletor de cor completo, "somem após", aviso de "queimadas") e Ajustes › Atalhos (`OBSHotkeyEdit` para marcador,
holofote, desenhar, desfazer, limpar; migrar `ProjectRecorder`/`MouseStyleStore` para `HotkeyBinding`), desenho e
sobreposição em todos os monitores, barra arrastável. Ver o fim de
`docs/superpowers/plans/2026-10-17-voltz-plano-4c-modo-desenho.md`.
```
  3. No fim do arquivo, acrescentar (preencha os números medidos onde está `<…>`, com o que o smoke e o roteiro
     mostrarem):
```markdown
## Pendências e limites do 4C
- O modo desenho só cobre o monitor da Tela compartilhada de captura de monitor; Tela de janela/outra captura → recusado
  ("sem_alvo"). Outros monitores continuam livres (clicar neles sai do modo por perda de ativação).
- Pelo atalho, o Voltz está em segundo plano e o Windows não deixa a janela de entrada ativar sozinha: o primeiro clique
  a ativa (e já desenha); até lá, Esc e Ctrl+Z vão para o app em uso (clique direito, o mesmo atalho e o botão Sair
  funcionam).
- Com "Desligar atalhos com o app em foco" (Configurações → Geral → Atalhos), o mesmo atalho não sai do modo desenho.
- Atalhos do OBS não consomem a tecla: Ctrl+Shift+D também chega ao app em uso. Desfazer/limpar globais sem tecla padrão.
- O vigia sai do modo quando a UI fica 1 s sem pulsar — inclusive em travadas legítimas (carregar coleção, iniciar
  gravação): o usuário liga de novo. A barra (thread da UI) fica parada até a UI voltar.
- `DisableProcessWindowsGhosting` vale para o processo todo: com o Voltz travado, a janela dele não vira fantasma (não
  dá para arrastá-la/minimizá-la até voltar; o Gerenciador de Tarefas funciona).
- A barra fica no alto e ao centro do monitor da Tela (não arrasta); sem diálogo de cor (4 + 8 cores); a região da
  janela de entrada tem um buraco retangular do tamanho da barra (os cantos arredondados não desenham).
- Medidas (smoke, sobreposição 2560×1440): parcial média `<…>` µs, máx. `<…>` µs; completa máx. `<…>` µs; fonte máx.
  `<…>` µs. O envio da textura da fonte continua inteiro (~15 MB por mudança, no máximo uma vez por quadro); rasterizar
  fora da thread gráfica só se o roteiro manual mostrar quadros perdidos.
- `VOLTZ_DEBUG_FREEZE_UI` (como `VOLTZ_TEST_LIVE`) fica no código de produção; avaliar guarda de compilação no Plano 5.
- Pendências do 4B que continuam: limite de número de traços, "somem após" sem animação, monitor trocado no meio da
  gravação não vira evento no `anotacoes.jsonl`, pedidos de teste da casca enfileirados (~400 ms).

## Testes manuais pendentes do 4C
- Todo o "Roteiro de teste manual do Plano 4C" do fim do plano (ligar/sair por cada caminho, cada ferramenta, barra sem
  roubar o Esc, vigia com `-Freeze`, halo sem onda ao desenhar e com onda depois, gravação com traços, escala 125/150% e
  monitor à esquerda, queimadas numa VM < 2004, textos em en-US).
```
  E, na seção `## Pendências e limites do 4B`, apagar os itens já resolvidos no 4C: `- Traços só entram por pedidos de
  teste ...`, `- A fonte e a sobreposição re-rasterizam a imagem inteira ...` (3 linhas), `- \`ensureClickThrough\` refaz
  os bits do estilo ...` (3 linhas), `- Janela "fantasma" do Windows: ...` (2 linhas) e `- \`tools/voltz-annot-demo.ps1
  -Close\` só restaura ...` (2 linhas).
- [ ] **Step 5: `CLAUDE.md` — atualização e poda** (depois `cp CLAUDE.md AGENTS.md`). O alvo é **≤ 12 000 bytes**
  (`wc -c CLAUDE.md`; hoje 11 942). Em `docs/HISTORICO.md`, acrescentar no fim a seção
  `## Poda do Plano 4C (texto que saiu do CLAUDE.md, 2026-10-17)` e mover para ela, **verbatim**, cada trecho trocado ou
  tirado abaixo (conferido ao escrever o plano: com as cinco trocas e as cinco saídas o arquivo fica com ~11 650 bytes):
  1. *Stack & estrutura*: trocar a linha
     ``    `voltz-smoke-mouse -OutDir <vazia>` · `voltz-smoke-annotations -OutDir <vazia>`; demo manual: `voltz-annot-demo`.``
     por
```markdown
    `voltz-smoke-mouse -OutDir <vazia>` · `voltz-smoke-annotations -OutDir <vazia>` · `voltz-smoke-draw`; demos manuais:
    `voltz-annot-demo`, `voltz-draw-demo` (modo desenho, sem variável de teste).
```
  2. *Estado atual*: trocar o item `- Plano 4B concluído: ...` (3 linhas) por:
```markdown
- Planos 4B–4C concluídos: anotações (núcleo, rasterizador com retângulo sujo, `anotacoes.jsonl`, fonte Voltz
  Anotações, sobreposição fora da captura) e modo desenho (Ctrl+Shift+D/botão Desenhar, janela de entrada em thread
  própria, saídas garantidas, barra). Testes: 170 casos + smokes de anotações e desenho.
```
  3. *Decisões*: depois do item `- Anotações: traços em pixels da captura da Tela; ...` (3 linhas), acrescentar:
```markdown
- Modo desenho: só por ação do usuário (nunca com `VOLTZ_TEST_LIVE`), só no monitor da Tela; janela de entrada Win32 em
  thread própria (saídas e vigia de 1 s pela `DrawSession` do núcleo); captura/foco só em `annotate/DrawInputWindow.cpp`.
```
  4. *Armadilhas*: tirar (para o HISTORICO) os itens inteiros `- Grupo do OBS não se compartilha entre cenas ...`
     (2 linhas), `- Raw Input é por processo/uso: ...`, `- \`show\`/\`hide\` de fonte só no tick ...`,
     `- Item novo num grupo existente: ...` (3 linhas) e `- Fonte sem \`OBS_SOURCE_SRGB\` que mistura alfa: ...` (2 linhas);
     no lugar do primeiro deles, um só item:
```markdown
- Armadilhas arquivadas do 4A–4B (grupos: item novo/entre cenas; show/hide no tick; Raw Input por processo; sRGB da
  fonte): `docs/HISTORICO.md` → "Poda do Plano 4C".
```
     e acrescentar no fim das Armadilhas:
```markdown
- `HTTRANSPARENT` só passa o clique a janelas da mesma thread; dono de outra thread liga as filas de entrada;
  `SetWindowLongPtr(GWL_EXSTYLE)` espera a thread dona. Teste nunca liga o modo desenho (só prova a recusa).
```
  5. *Próximos passos* → trocar os dois itens por:
```markdown
1. Usuário roda o roteiro manual do 4C; depois escrever e executar o Plano 4D (texto, Ajustes › Anotações/Atalhos,
   todos os monitores) a partir do fim do plano 4C e do `docs/voltz/backlog-plano-4.md`.
2. Depois: Plano 5 (marca visual + instalador + CI).
```
  Conferir `wc -c CLAUDE.md` ≤ 12000 (se passar, mover também, verbatim, o item `- PowerShell 5.1: ...` (3 linhas) e
  citá-lo no item de armadilhas arquivadas; conferir de novo). Depois `cp CLAUDE.md AGENTS.md` e
  `diff CLAUDE.md AGENTS.md` → sem saída. Em `docs/HISTORICO.md`, acrescentar ainda `## Plano 4C (2026-10-17)` com 3–4
  linhas (sessão de desenho no núcleo com saídas garantidas, janela de entrada em thread própria como única exceção de
  captura/foco, barra e atalhos, retângulo sujo medido; 170 casos + smoke de desenho; roteiro manual e pendências em
  backlog-plano-4.md).
- [ ] **Step 6:** Commit e push:
```bash
git add docs/voltz/backlog-plano-4.md docs/HISTORICO.md CLAUDE.md AGENTS.md
git commit -m "docs(voltz): registro do Plano 4C (backlog, memoria podada)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push origin voltz
```
Expected: push sem `--force`, `voltz -> voltz`.

---
## Roteiro de teste manual do Plano 4C (para o usuário, depois da Task 10)

Nada aqui é automatizado: é o usuário quem liga o modo desenho, desenha e usa o PC. Em qualquer momento, se algo
prender o mouse ou o teclado: **Esc**, **clique direito**, **Ctrl+Shift+D**, **Alt+Tab** ou esperar **30 s** — e, se a
interface do Voltz travar, o modo sai sozinho em ~1 s. Depois de cada passo, `tools/voltz-draw-demo.ps1 -Log` mostra o
motivo da saída (`desligado (esc)`, `(clique_direito)`, `(atalho)`, `(botao)`, `(foco)`, `(fechar)`, `(inatividade)`,
`(vigia)`, `(alvo)`).

1. **Preparar:** `powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Launch` → na tela Presets aplicar
   **Tutorial** (monitor e webcam reais). Abrir o Bloco de Notas no monitor capturado e deixá-lo em primeiro plano.
2. **Ligar pelo atalho com outro app em foco:** Ctrl+Shift+D → a barra aparece no alto e ao centro do monitor da Tela;
   sobre esse monitor o cursor vira **cruz**; o botão "Desenhar" da casca fica marcado; os outros monitores continuam
   normais. `-Log`: `ligado (atalho; ... ativa: nao (o primeiro clique ativa) ...)`. Clicar e arrastar já desenha
   (caneta lima).
3. **Cada saída** (ligar de novo entre uma e outra; depois de cada uma, clicar e digitar no Bloco de Notas funciona):
   Esc (depois de ter clicado ao menos uma vez) → `esc` e o foco volta ao Bloco de Notas · clique direito →
   `clique_direito` · Ctrl+Shift+D → `atalho` · botão **X** da barra → `botao` · Alt+Tab → `foco` · tecla Windows →
   `foco` · clicar numa janela de **outro monitor** → `foco` · Alt+F4 → `fechar` (o Voltz **não** fecha) · 30 s parado →
   `inatividade`.
4. **Pelo botão:** com a casca visível, clicar **Desenhar** → `ativa: sim`; o Esc sai **sem** clique antes. Clicar
   Desenhar de novo (se a casca estiver em outro monitor) → `botao`.
5. **Ferramentas e barra:** caneta, marca-texto (translúcido, sem "contas" onde o traço se cruza), linha, seta,
   retângulo, elipse; as 4 cores e as 8 de "mais cores"; as 4 espessuras; desfazer pelo botão e por **Ctrl+Z** (também
   no meio de um traço); limpar; "somem após" em 3 s (os traços somem sozinhos). Arrastar para fora do monitor: o traço
   fica preso à borda. **Clicar nos botões da barra não tira a cruz nem o Esc** (depois de clicar na barra, o Esc ainda
   sai); a barra não aparece na barra de tarefas nem no Alt+Tab.
6. **Gravação:** gravar ~20 s desenhando com várias ferramentas. No player: `programa.mp4` **com** os traços e **sem**
   a barra; `tela.mkv` **sem** traços e sem barra; nenhuma onda de clique no começo dos traços. `anotacoes.jsonl` com
   uma linha por traço em pixels da captura.
7. **Halo e Raw Input:** com o halo visível na prévia — clicar fora do modo desenho → onda; desenhar no modo desenho →
   sem onda; sair e clicar → a onda volta (o registro de Raw Input do mouse ao vivo continua valendo).
8. **Vigia e fantasma:** `-Close`, depois `-Launch -Freeze`, aplicar Tutorial, desenhar 2 traços e ligar o modo desenho.
   Ctrl+Shift+F12 (trava a interface 8 s): em **~1 s** a cruz some e o clique volta a passar no monitor (clique numa
   janela embaixo); a barra fica parada no topo até a interface voltar e então some; **nenhuma** janela
   "(Não está respondendo)" cobre o monitor; os traços continuam visíveis e o clique passa por eles. `-Log`:
   `depuracao: thread da UI travada por 8000 ms`, `desligado (vigia)`. Repetir o Ctrl+Shift+F12 **fora** do modo desenho,
   com traços na tela: o clique passa pelos traços o tempo todo.
9. **Escala e posição:** Windows a 125% e 150%, e um monitor à **esquerda** do principal como Tela: o traço sai
   exatamente sob a ponta da cruz, alinhado com a prévia; a barra fica centrada no monitor certo.
10. **Desempenho:** desenhar sem parar por ~30 s com traços longos (caneta e marca-texto), até dezenas de traços na
    tela: a cruz não atrasa, o sistema segue responsivo, a prévia acompanha. Sair e rodar `-Log`: anotar a linha
    `desligado (...); sobreposicao: N parcial(is) (media X us, max Y us), ...` e, se aparecer,
    `[voltz-sources] anotacoes: 600 redesenho(s) ...` — os números vão para o backlog (pendências do 4C).
11. **Nunca liga sozinho:** fechar (`-Close`) e abrir (`-Launch`) duas vezes: o modo desenho começa desligado
    (`-Log`: `desligado ao abrir`) e só liga pelo atalho ou pelo botão.
12. **`-Close`:** o Voltz fecha (com ou sem o modo desenho ligado) e a config portátil volta a ser a de antes — ou some,
    se não havia uma.
13. (Se houver uma VM com Windows 10 anterior ao 2004) **queimadas:** a barra mostra o aviso amarelo; os traços aparecem
    uma vez só no programa.
14. **en-US:** trocar o idioma e conferir os textos da barra ("Pen", "Highlighter", ...) e do botão ("Draw").

---

## Auto-revisão (feita ao escrever)

- **Cobertura da spec (parte do 4C):** §6.2 "atalho global (padrão Ctrl+Shift+D) abre a barra flutuante e ativa o modo
  desenho; Esc sai" (Tasks 1, 5, 7); "caneta, marca-texto, linha, seta, retângulo, elipse" (barra, Task 6; o núcleo do
  4B já desenha); "4 cores rápidas + seletor" (4 + 8 sem diálogo, Task 6 — seletor completo no 4D, com o porquê);
  "espessura, desfazer, limpar tudo, somem após N s" (Tasks 4, 6, 7); "clicável apenas em modo desenho" (janela de
  entrada só existe no modo; sobreposição do 4B continua sempre transparente — Tasks 4, 5); §6.3/§8 "o app avisa" em
  Windows < 2004 (aviso na barra, Task 6); §3 Toolbar (Task 6); §9 lógica no núcleo testada (Tasks 1–2). Backlog do 4B:
  saídas garantidas (Task 1, 5), barra Qt com `isOBSProjectorWindow` + `WDA_EXCLUDEFROMCAPTURE` (Task 6), atalhos
  Draw/limpar/desfazer (Task 7), aviso de queimadas (Task 6), fantasma (Task 7 + smoke C), raster inteiro (Tasks 2–4 +
  smoke B), `ensureClickThrough` sem `TOPMOST` (Task 4), Raw Input por processo (decisão + grep + roteiro 7), demo
  `-Close` (Task 9). **Fora do 4C, com motivo e destino (4D):** texto, Ajustes › Anotações/Atalhos (§4.3), todos os
  monitores, seletor de cor completo, barra arrastável.
- **Segurança:** nenhum passo liga o modo desenho; o único gatilho em teste prova a recusa (dupla) com o vigia de
  `VoltzDrawInput`/barra/`GetGUIThreadInfo`; a barra só é desenhada sem janela (`janela_nativa=false`); scripts sem
  injeção nem primeiro plano (grep nas Tasks 8 e 9); código com APIs de captura/foco só no arquivo de exceção, com
  contagem (grep nas Tasks 5, 7, 8, 10); a fase de UI travada só lê (`IsHungAppWindow`, `Ghost`); capturas só da casca e
  de cena sintética; config portátil com backup e hash.
- **Placeholders:** nenhum "TBD". Os únicos `<…>` são números a medir na Task 10 (backlog), com a origem dita; onde só a
  execução decide há critério objetivo (contadores de parciais/completas, amostras `#FFD4F53C`/`#00000000`, linhas de
  log exatas, `janela_nativa=false`, `fantasmas: 0`).
- **Consistência de nomes:** núcleo `DrawExit` (`None, Escape, RightClick, Hotkey, Button, FocusLost, Idle, Watchdog,
  Target, Close, Shutdown`), `drawExitName`, `DrawRefusal` (`None, ShuttingDown, TestMode, NoTarget`),
  `drawRefusalName`, `checkDrawEntry`, `kDrawIdleNs`, `kDrawHeartbeatNs`, `DrawInputKind`, `DrawInput` (`kind, tNs, x,
  y, uiHeartbeatNs`), `DrawCommandType` (`Begin, Extend, End, Undo, Exit`), `DrawCommand` (`type, points, exit`),
  `DrawSession` (`handle, requestExit, active, drawing, exitReason`); `PixelRect` (`empty`), `uniteRects`,
  `intersectRects`, `strokeBounds`, `rasterizeStrokeClipped`, `redrawRegion`, `StrokeTail` (`count, shapePoints`),
  `strokeTail`, `appendDirtyRect`, `DirtyHistory` (`kCapacity, record, since`); plugin `AnnotationFeed`
  (`snapshotIfChanged(..., dirty, full)`, `recordRaster`, `describe` com `raster_us_ultimo/raster_us_max/parciais/
  completas`); `OverlayWindow::render(const std::vector<const Stroke *> &, int, int, const PixelRect *)`, `Info`
  (`partialRenders, fullRenders, partialTotalNs, partialMaxNs, fullMaxNs`); `AnnotationController` (`setStyle`,
  `drawTarget`, `setDrawStatus`, `overlayStatsText`, sinal `styleChanged`; estado `modo_desenho/desenho_recusa/
  desenho_saida` e `sobreposicao.parciais/completas/parcial_us_media/parcial_us_max/completa_us_max`);
  `MonitorDescription::deviceName`; `DrawShared` (`uiHeartbeatNs, mutex, commands, wakePending, wake, wakeContext,
  hole`), `takeDrawCommands`, `DrawInputWindow` (`Options{mapping, excludeFromCapture}`, `start, requestExit,
  holeChanged, wait, activated`), classe de janela `VoltzDrawInput`; `DrawToolbar` (`setAnnotationStyle, setBurned,
  setExitHint, showOnScreen, raiseWithoutActivating, nativeRect`; sinais `toolChosen, colorChosen, widthChosen,
  fadeChosen, undoClicked, clearClicked, exitClicked, nativeRectChanged`; título `VoltzDrawToolbar`); `DrawTrigger`
  (`Hotkey, Button`), `DrawModeController` (`shutdown, toggle, active, drawShortcutText`; sinais `activeChanged,
  refused`); `loadHotkeyBinding`, `hotkeyShortcutText`; `LiveMouse::setClicksSuppressed`; atalhos
  `VoltzStudio.Draw`/`UndoAnnotation`/`ClearAnnotations`/`DebugFreezeUi`; `live::kDebugFreezeEnv` =
  `VOLTZ_DEBUG_FREEZE_UI`; pedidos `SetAnnotationStyle`, `ToolbarImage` (`png, largura, altura, janela_nativa`);
  locales `Voltz.Draw.*`, `Voltz.Hotkey.{Draw,UndoAnnotation,ClearAnnotations,DebugFreezeUi}`,
  `Voltz.Record.Draw{,.Hint,.NoShortcut}` — iguais no código, no bloco de teste, no smoke e nas demos.
- **Contagem de testes:** 153 (base) → 164 (Task 1: 11 casos) → 170 (Task 2: 6 casos).
- **Conferido ao escrever (fora do repositório, numa cópia no scratchpad):** o código do núcleo das Tasks 1–2 compilado
  com `/W3 /WX /permissive-` e os 11 + 6 casos novos passando junto com os 8 do rasterizador do 4B (37 casos dos
  arquivos tocados); as edições das Tasks 3–7 aplicadas numa cópia de `frontend/voltz` e `plugins/voltz-sources` e os
  13 `.cpp` alterados/novos compilados com os includes e definições reais do alvo `obs-studio` (moc gerado,
  `QT_NO_CONTEXTLESS_CONNECT`) — sem link nem execução; os 4 scripts PowerShell analisados pelo parser do PowerShell 5.1
  e o `Add-Type` da sonda compilado; o GREP DE SEGURANÇA rodado na cópia dá exatamente o esperado; o `CLAUDE.md` com as
  trocas da Task 10 fica com ~11 650 bytes.

---

## Plano 4D (próximo, a escrever depois do 4C) — texto, Ajustes e todos os monitores

- **Ferramenta de texto:** clique no modo desenho abre uma caixa de digitação **na thread de entrada** (Win32, sem Qt:
  o teclado já vai para a janela de entrada); Enter confirma, Esc cancela (e não sai do modo na primeira vez). O traço
  `"ferr":"texto"` ganha `"texto"` e `"tam"` no `anotacoes.jsonl` (`strokeToJsonLine`/`strokeFromJson`); o texto vira
  imagem por um rasterizador de glifos no núcleo (fonte embutida, ex. DM Sans via stb_truetype, a mesma no plugin e na
  sobreposição — o plugin não tem Qt). `DrawSession` ganha o estado "digitando" com as mesmas saídas garantidas.
- **Ajustes › Anotações:** estilo padrão (ferramenta, cor com `QColorDialog` — fora do modo desenho —, espessura),
  "somem após", aviso de "queimadas", texto do atalho. **Ajustes › Atalhos:** `OBSHotkeyEdit` (ou equivalente) para
  marcador, holofote, desenhar, desfazer, limpar; `ProjectRecorder` e `MouseStyleStore` passam a usar `HotkeyBinding`.
- **Todos os monitores:** uma sobreposição por monitor (só exibição) e a janela de entrada sobre o desktop virtual; traço
  fora do monitor da Tela fica só na sobreposição (coordenadas do desktop) e não entra no `anotacoes.jsonl` nem no
  programa; barra arrastável (posição salva por monitor).
- **Desempenho:** rasterizar a fonte fora da thread gráfica só se o roteiro manual do 4C mostrar quadros perdidos.
- **Testes:** núcleo (texto no jsonl, rasterizador de glifos, estado "digitando" e saídas, coordenadas multimonitor);
  integração com o modo desligado e a sobreposição fora do desktop virtual, como no 4C; a interação real só no roteiro
  manual do usuário.
