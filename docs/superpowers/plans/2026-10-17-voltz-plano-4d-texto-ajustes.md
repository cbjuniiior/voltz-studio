# Voltz Studio — Plano 4D: texto nas anotações, seletor de cor completo, Ajustes › Anotações e Ajustes › Atalhos — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** O modo desenho ganha a ferramenta **Texto** (spec §6.2): com o T da barra, o clique marca onde o texto começa e
o usuário digita (acentos, Shift+Enter quebra a linha, Backspace apaga, Enter confirma, Esc cancela só o texto); o texto
aparece ao vivo no programa e na sobreposição (com o cursor de digitação só na sobreposição), vai para o
`anotacoes.jsonl` (`"ferr":"texto"`, `"texto"`, `"tam"`, `"fonte"`) e é desenhado **pelo mesmo código e com os mesmos
bytes de fonte** na sobreposição (frontend) e na fonte Voltz Anotações (plugin, sem Qt). A barra ganha tamanho e negrito
do texto, o **seletor de cor completo** (sem diálogo, sem foco) e uma **linha de dica** (as dicas flutuantes do Qt não
aparecem numa janela que nunca fica ativa). Ajustes ganha as seções **Anotações** (estilo com que o modo desenho começa,
"somem após", estado de "queimadas", prévia com o rasterizador do núcleo) e **Atalhos** (marcador, holofote, desenhar,
desfazer, limpar — e o de depuração, quando ligado — com o campo de teclas do próprio OBS, validação, conflitos,
"Limpar" e "Restaurar padrão"); marcador e holofote passam a carregar o atalho pelo mesmo `HotkeyBinding`.

**Architecture:** As decisões ficam no núcleo, puras e testadas: modelo e JSON do texto (`annotation-model`), UTF-8 e
limites (`annotation-text`), layout sem fonte real (`text-layout`, métricas por interface), fonte embutida e
rasterização (`annotation-font` + `annotation-raster`: FreeType das deps do OBS e DM Sans Regular/Bold embutidas no
`voltz-core` por um `embed-fonts.cmake` — o `voltz-core` é ligado ao `obs-studio` **e** ao plugin, então os dois desenham
com o mesmo código e os mesmos bytes), digitação na `DrawSession` (`TextBegin/TextEdit/TextCommit/TextCancel`, com as
mesmas saídas garantidas) e atalhos (`hotkey-bindings`: tabela dos atalhos do Voltz, formato do perfil, regra de
combinação, conflitos). Na casca, a janela de entrada só passa a traduzir teclas (`TranslateMessage` → `WM_CHAR`, sem
nenhuma API de foco nova: o arquivo de exceção do 4C continua com as mesmas chamadas e contagens), o
`AnnotationController` manda o texto inteiro ao plugin a cada mudança (o `AnnotationFeed` já troca o traço de mesmo id)
e desenha o cursor só na sobreposição; Ajustes reaproveita o `OBSHotkeyEdit` do OBS sem patch e pausa os atalhos do
OBS enquanto o campo tem o foco. **Nenhum patch em arquivo do OBS.**

**Tech Stack:** C++17, Qt 6.11.1, libobs/frontend 32.2.2 (`obs_hotkey_load_bindings`, `obs_enum_hotkey_bindings`,
`obs_enum_hotkeys`, `obs_key_to_name`/`obs_key_from_name`, `obs_key_combination_to_str`, `obs_frontend_get_profile_config`,
`OBSHotkeyEdit`, `OBSApp::DisableHotkeys`/`UpdateHotkeyFocusSetting`), FreeType 2.13.3 (estático, das deps
`obs-deps-2026-07-15-x64`; `FT_New_Memory_Face`, `FT_Get_Advance`, `FT_Load_Glyph` sem hinting, `FT_Outline_Translate`,
`FT_Render_Glyph`), DM Sans (OFL, já empacotada em `frontend/voltz/shell/fonts`), Win32 (`TranslateMessage`, `WM_CHAR`
com substitutos UTF-16, `WM_SYSCHAR`, `IDC_IBEAM`), nlohmann_json 3.11, doctest. Testes ao vivo por
`tools/voltz-ws.ps1`, `tools/voltz-uia.ps1` (só botões, por Invoke) e UI Automation **só de leitura** (nome e valor).

**Spec:** `docs/superpowers/specs/2026-10-15-voltz-studio-base-gravacao-design.md` §4.3 (Ajustes → Mouse, Anotações,
Atalhos), §6.2 (ferramentas "…, texto", "4 cores rápidas + seletor", `anotacoes.jsonl` em coordenadas de `tela.mkv`, a
fonte Voltz Anotações desenha os mesmos traços), §6.3 (queimadas: o app avisa), §9 (lógica no núcleo testada; serialização
de traços) · **Backlog:** `docs/voltz/backlog-plano-4.md` ("Próximo: Plano 4D", "Pendências e limites do 4C") e o fim de
`docs/superpowers/plans/2026-10-17-voltz-plano-4c-modo-desenho.md` · **Modelo de formato:** o plano 4C.

**Base deste plano:** commit `a6d2655e4` (branch `voltz`, publicado) + o commit que adiciona este arquivo. Contagem de
testes na base: **170** casos (1108 asserções).

## Divisão do restante do Plano 4 (e o porquê)

O que o fim do plano 4C deixou para o "4D" passa de 20 tasks e mistura duas naturezas: **(a)** o que é **conteúdo e
configuração** do modo desenho que já existe — texto (modelo, fonte no plugin, digitação), seletor de cor, dicas da
barra, Ajustes › Anotações e Ajustes › Atalhos — e **(b)** o que muda a **geometria e a segurança da entrada** — desenhar
e mostrar traços em todos os monitores (uma janela de entrada que cobre mais de um monitor, traços fora da captura,
sobreposição por monitor) e a barra arrastável com posição por monitor. (b) mexe de novo no arquivo de exceção
(`DrawInputWindow.cpp`) e no que cobre a tela do usuário; não dá para provar automaticamente sem um segundo monitor e
precisa de um teste de geometria próprio. (a) não muda a entrada — a janela de entrada só passa a traduzir teclas — e é
o que o usuário mais sente no dia a dia (escrever na tela e trocar atalhos sem abrir o modo clássico). Por isso:

- **Plano 4D (este arquivo, 12 tasks):** (a) inteiro — texto ponta a ponta, seletor de cor completo, linha de dica,
  Ajustes › Anotações, Ajustes › Atalhos (com a migração do marcador e do holofote para o `HotkeyBinding`).
- **Plano 4E (próximo, descrito no fim):** (b) — todos os monitores e barra arrastável (posição por monitor, buraco que
  acompanha), com a geometria no núcleo e monitores falsos fora do desktop virtual nos testes.

**Fora do 4D, com o porquê:**
- **Todos os monitores e barra arrastável:** 4E (acima). No 4D o modo desenho continua só sobre o monitor da Tela e a
  barra continua no alto e ao centro dele.
- **Sair do modo desenho ao começar/parar a gravação:** **não** (nem no 4E). Pelo botão Gravar o clique na casca já tira
  a ativação da janela de entrada (o modo sai por `foco`); pelo atalho de gravar, quem desenha quer continuar
  desenhando — sair no meio do traço seria a surpresa. Registrado no backlog como decisão.
- **O(N²) do `redrawRegion`:** não. A medição do 4C (sobreposição 2560×1440) deu parcial média 121 µs e máximo 273 µs; o
  texto acrescenta glifos à região, e o smoke do texto mede de novo (Task 10) com um limite objetivo (< 100 ms).
- **Editar um texto já confirmado, cursor no meio, seleção, colar (Ctrl+V), quebra automática:** fora (o editor da etapa
  3 pode reescrever o texto pelo `anotacoes.jsonl`); desfazer e escrever de novo resolve no ao vivo.
- **Kerning, ligaduras e escritas complexas (HarfBuzz):** fora. O layout é proporcional e simples (avanço por
  caractere); o português e o inglês saem certos. Caractere que a DM Sans não tem (emoji, CJK) vira o retângulo
  `.notdef` — limite registrado.
- **Desligar as anotações por completo em Ajustes:** fora. O modo desenho só liga por ação explícita do usuário e a fonte
  Voltz Anotações pode ser escondida por cena (olho na lista de Fontes): uma terceira chave global criaria um estado a
  mais em que o atalho "não faz nada", sem ganho de segurança.

## Descobertas que decidem o desenho (conferidas no código 32.2.2, nas deps e no Windows)

- **FreeType já está nas deps, estático:** `.deps/obs-deps-2026-07-15-x64/lib/freetype.lib` (FreeType 2.13.3, 1,8 MB,
  sem DLL; zlib interno — `FT_CONFIG_OPTION_USE_ZLIB` sem `SYSTEM_ZLIB`) com cabeçalhos em `include/freetype2`. O
  `plugins/text-freetype2/CMakeLists.txt:10` faz `find_package(Freetype REQUIRED)` e o módulo do CMake cria
  `Freetype::Freetype` (UNKNOWN IMPORTED). Não há stb_truetype no repositório nem nas deps.
- **Alvo importado só existe no diretório do `find_package` e abaixo**, e o `find_dependencies` do OBS
  (`cmake/common/helpers_common.cmake:157`) aborta com `$<LINK_ONLY:...>` (linha 146: "unsupported generator
  expression") — é o que um `target_link_libraries(voltz-core PRIVATE ...)` gera numa biblioteca estática (por isso o
  nlohmann entra só pelos includes). ⇒ `find_package(Freetype 2.13 REQUIRED)` no `frontend/cmake/feature-voltz.cmake`
  (antes do `add_subdirectory(voltz/core)`, visível para o `obs-studio`, o núcleo, os testes e o plugin) e o `voltz-core`
  liga **PUBLIC**. Conferido num projeto mínimo (NMake) no scratchpad com a mesma árvore (frontend → core, testes e
  plugin em `add_subdirectory`) e o `find_dependencies` copiado do OBS: ele acha `Voltz::core;Freetype::Freetype`, o
  executável, os testes e o módulo ligam e abrem a fonte embutida (`unitsPerEm=1000`).
- **DM Sans estática:** `frontend/voltz/shell/fonts/DMSans-Regular.ttf` (56 344 B) e `DMSans-Bold.ttf` (56 268 B), 1000
  unidades por em, OFL (`OFL.txt` ao lado) — embutir em binário é permitido pela licença. Um `file(READ ... HEX)` +
  `string(REGEX REPLACE)` do CMake gera o `.cpp` com os bytes em 0,3 s (conferido com o CMake 3.31 do VS).
- **A thread de entrada não traduz teclas:** o laço de `annotate/DrawInputWindow.cpp:404` só faz `DispatchMessageW`;
  sem `TranslateMessage` não há `WM_CHAR` (nem acento por tecla morta). Com ele: `WM_CHAR` chega também para Backspace,
  Enter, Esc e Ctrl+letra (controles, que a sessão recusa), caractere fora do plano básico chega em **duas** metades
  UTF-16, e `WM_SYSCHAR` (Alt+letra) numa janela sem menu faz o `DefWindowProc` tocar o aviso sonoro. A janela de
  entrada já é a ativa (foco do teclado) depois do primeiro clique — **nenhuma** API de foco nova é necessária.
- **O plugin já troca um traço inteiro:** `AnnotationFeed::begin` com um id que já existe substitui o traço e registra a
  região antiga ∪ nova (`plugins/voltz-sources/annotation-feed.cpp:73`). ⇒ cada tecla manda o traço de texto inteiro
  pelo `voltz_annot_begin`; nenhum procedimento novo, nenhuma mudança no plugin além do núcleo que ele já liga.
- **O diário registra o que some aberto:** `AnnotationJournal::strokeRemoved` (`annotation-journal.cpp:48`) escreve a
  linha completa (`t_fim` = `t_limpo`) de um traço que ainda não tinha linha. ⇒ texto cancelado **com** algo (apareceu
  no programa) fica registrado; texto cancelado **vazio** sai sem sinal (`StrokeStore::discardOpen`), sem linha.
- **O campo de teclas do OBS é reaproveitável:** `OBSHotkeyEdit(QWidget *, obs_key_combination_t, OBSBasicSettings *)`
  (`frontend/settings/OBSHotkeyEdit.hpp`) só usa o `settings` no aviso de duplicado (`CreateDupeIcon`,
  `OBSHotkeyEdit.cpp:212-214`, chamado só por `UpdateDuplicationState`) ⇒ com `nullptr` funciona se o Voltz nunca chamar
  `UpdateDuplicationState`. O `OBSHotkeyWidget` grava só no libobs; quem grava no perfil é
  `OBSBasicSettings::SaveHotkeySettings` (`OBSBasicSettings.cpp:3723`): `[Hotkeys] <nome> = {"bindings":[...]}` com o
  JSON do `obs_hotkey_save` (`libobs/obs-hotkey.c:618-645`: `shift`/`control`/`alt`/`command` só quando ligados, depois
  `key`), lido por `load_binding` (`obs-hotkey.c:452-482`).
- **Os atalhos do OBS disparam enquanto a tecla é gravada:** o OBS não pausa nada nas Configurações; com
  `NeverDisableHotkeys`, apertar Ctrl+Shift+D no campo ligaria o modo desenho. O OBS já pausa os atalhos na janela de
  adicionar fonte com `App()->DisableHotkeys()` e devolve com `App()->UpdateHotkeyFocusSetting()`
  (`frontend/dialogs/OBSBasicSourceSelect.cpp:288,293`) ⇒ o mesmo par, no foco do campo.
- **Dicas flutuantes do Qt não aparecem em janela inativa** (a menos de `Qt::WA_AlwaysShowToolTips`, que criaria uma
  janela de dica a mais, de outra classe, fora do controle de afinidade da barra) ⇒ a dica vai numa linha **dentro** da
  barra, que já está fora da captura.
- **`QColorDialog::getColor` é modal** (o `MouseSettingsSection` usa, `MouseSettings.cpp:228`): na barra ele pegaria a
  ativação e a janela de entrada sairia do modo (`foco`) ⇒ seletor desenhado dentro da barra. Em Ajustes (fora do modo
  desenho), o `QColorDialog` serve.
- **Testes e smokes que mudam:** `tests/test-annotation-model.cpp` usa `"texto"` como ferramenta desconhecida e confere o
  JSON exato do estilo; `tools/voltz-smoke-draw.ps1:361` confere o `AnnotationStyle` exato no estado e no `user.ini` —
  os dois passam a ter `"tam"` e `"fonte"`.

---

## Decisões técnicas (e o porquê)

| Tema | Decisão | Por quê |
|---|---|---|
| Divisão | 4D = texto, seletor de cor, linha de dica, Ajustes › Anotações/Atalhos; 4E = todos os monitores e barra arrastável | Ver acima: 4D não mexe na segurança da entrada; 4E sim. |
| Texto no plugin | **FreeType das deps + DM Sans embutida no `voltz-core`**; layout e rasterização no núcleo; o plugin usa o mesmo núcleo | Mesmo código e mesmos bytes nos dois lados (byte a byte igual com a mesma escala); o plugin não tem Qt. Alternativas descartadas: **bitmap do frontend pelo feed** (o plugin redesenha em outra escala e o `anotacoes.jsonl` precisaria do texto de qualquer jeito; um canal binário novo frontend → plugin); **stb_truetype** (mais um código de terceiro a baixar e auditar, quando o FreeType já está nas deps, na licença e no build do OBS); **DirectWrite** (só Windows, COM no núcleo, que precisa ficar puro e testável). |
| Fontes | Só as duas embutidas: `dm_sans` (Regular) e `dm_sans_negrito` (Bold, padrão); tamanho = "em" em pixels da captura, 12–200 (padrão 48; barra: 24, 36, 48, 72) | Fonte do sistema não estaria igual no plugin nem no editor de outro PC; DM Sans é a fonte da marca e já vem no app. |
| Layout | `layoutText` com `GlyphMetrics` por interface (testado com métricas falsas); avanço sem hinting em unidades da fonte, sem kerning/ligaduras; linhas por `'\n'`; âncora = canto de cima à esquerda; cursor no fim da última linha | Testável sem fonte; proporcional ao tamanho (sobreposição com outra escala sai igual em proporção). |
| Rasterização | Glifo do FreeType sem hinting, contorno deslocado pela fração da posição, maior cobertura por pixel (como os outros traços), mistura src-over opaca; corte no `strokeBounds` do texto (caixa do layout + 0,3 em + 2 px) | Mesmo resultado do redesenho parcial e do inteiro (doctest byte a byte); o retângulo nunca perde tinta (a raster corta nele). |
| `anotacoes.jsonl` | `"texto"`, `"tam"`, `"fonte"` depois de `"pts"` só no texto; `"pts"` = a âncora; `"esp"` continua (formato da spec) | Leitores antigos ignoram campos novos; o editor reescreve o texto. |
| Digitação | Na `DrawSession` (núcleo): `Char`/`Backspace`/`Enter{shift}` → `TextEdit` com o texto inteiro; Enter confirma; Esc e Ctrl+Z cancelam **só** o texto; clicar de novo confirma e começa outro; **toda saída confirma** (vazio = cancela); limites 280 caracteres e 12 linhas | Mesma máquina de estados, as mesmas saídas garantidas, testada com relógio falso. |
| Ao vivo | O texto aparece no programa enquanto é digitado (como a caneta); o cursor só na sobreposição (traço extra que não vai ao plugin nem ao diário) | É o que um apresentador espera; o cursor não é conteúdo do vídeo. |
| Foco do teclado | Nenhuma API nova: `TranslateMessage` no laço da thread de entrada; `WM_SYSCHAR` engolido; cursor em I com a ferramenta texto | O arquivo de exceção mantém exatamente 1 `SetCapture`, 1 `ReleaseCapture`, 2 `SetForegroundWindow` (grep D). |
| Privacidade | O texto digitado **nunca** vai para o log (só tamanho e quantidade de caracteres) | É conteúdo do usuário; grep F e checagem no smoke. |
| Seletor de cor | `ColorPicker` (quadrado saturação × brilho + faixa de matiz) dentro de "mais cores", aberto pelo conta-gotas; escolhe ao soltar; o botão mostra a cor fora da paleta | Sem diálogo e sem foco: a barra nunca fica ativa. |
| Dicas | Linha de dica dentro da barra: a dica do botão sob o mouse ou, sem botão, como sair (ou como digitar, com o texto) | Fica fora da captura com a barra; nenhuma janela nova. |
| Ajustes › Anotações | Edita o **mesmo** `AnnotationStyle` da barra (salvo em `user.ini [Voltz] AnnotationStyle`): ferramenta, cor (`QColorDialog`), espessura, tamanho e fonte do texto, "somem após"; prévia pelo rasterizador do núcleo; estado (fora da captura / queimadas / fonte) e o atalho de desenhar | Um estilo só: o que se escolhe na barra é o que Ajustes mostra, e vice-versa. |
| Atalhos | Tabela dos atalhos do Voltz no núcleo (`hotkey-bindings`: nome, tecla padrão, texto, depuração); `HotkeyBinding` carrega/grava o perfil pelo núcleo (JSON do OBS); marcador e holofote migrados | Um lugar só para os padrões; leitura/gravação testadas sem libobs. |
| Ajustes › Atalhos | `OBSHotkeyEdit` sem `OBSBasicSettings`; grava na hora só combinação aceita (`checkVoltzHotkeyCombo`: tecla + Ctrl/Alt/Windows; sozinhas só F1–F24 e mouse lateral), trocando a **primeira** tecla (as outras do modo clássico ficam); conflitos com qualquer atalho; "Limpar" e "Restaurar padrão"; o campo só ganha foco por clique e, com o foco, pausa os atalhos do OBS | "D" sozinho ligaria o modo desenho no meio de qualquer digitação (os atalhos do OBS não consomem a tecla); gravar Ctrl+Shift+D não pode ligar o modo; Tab nunca pausa os atalhos sem querer. |
| Pendências do 4C | Dicas: resolvidas pela linha de dica. Sair ao gravar/parar: não (decisão acima). O(N²): não (medição). Barra arrastável e seletor completo: seletor no 4D, barra no 4E | Ver "Fora do 4D". |

---

## Segurança dos testes (igual ao 4C, mais o texto e os atalhos)

O PC é do usuário e ele o usa durante os testes. Cada regra tem um mecanismo concreto no código ou no script:

1. **Teste automático nunca liga o modo desenho.** As duas travas do 4C continuam (`checkDrawEntry` recusa com
   `VOLTZ_TEST_LIVE`; `DrawInputWindow::start` recusa de novo antes de criar a thread). O único gatilho usado em teste é
   `TriggerHotkeyByName "VoltzStudio.Draw"`, **só para provar a recusa** — também depois de limpar e de restaurar o
   atalho em Ajustes › Atalhos. O botão "Desenhar" nunca é acionado por UI Automation.
2. **Nada é digitado.** O texto entra só pelos pedidos de teste da casca (`InjectText`, `TypeText`, `CommitText`,
   `CancelText`), que chamam o `AnnotationController` direto na thread da UI — o mesmo caminho dos comandos da
   `DrawSession`. A tradução de teclas da janela de entrada (`WM_CHAR`, teclas mortas, substitutos, Shift+Enter) e a
   própria digitação são verificadas **só pelo roteiro manual do usuário**; as regras dela estão no doctest (Task 4).
3. **Nenhum campo de texto da casca recebe foco.** Em Ajustes só se acionam **botões** por UI Automation/Invoke ("Ajustes",
   "Limpar atalho — …", "Restaurar padrão — …"); o resto é leitura (`UiaExists`, `UiaValue`: nome, `ValuePattern`,
   `RangeValuePattern`). O campo de atalho só ganha foco por clique (`Qt::ClickFocus`) e o smoke confere no log que os
   atalhos **nunca** foram pausados (`[voltz] atalhos: editando` ausente).
4. **Nada em teste toma foco, captura o mouse ou cobre a área visível.** Scripts sem `SendInput`, `SetCursorPos`,
   `mouse_event`, `keybd_event`, `SendKeys`, `SetForegroundWindow`, `BlockInput`, `ClipCursor`,
   `[System.Windows.Forms.Cursor]::Position` (grep nas Tasks 10 e 11); o único P/Invoke que escreve é
   `PostMessage(WM_CLOSE)` na janela da casca do PID lançado, guardada na abertura.
5. **`Assert-DrawSafe` depois de cada ação e um vigia a cada 200 ms** (job), iguais aos do 4C: sobreposições do PID dentro
   do retângulo de teste, fora do desktop virtual, transparentes a cliques (`0x080800A8`), fora do primeiro plano, com
   afinidade `0x11`; **nenhuma** janela `VoltzDrawInput`; **nenhuma** barra `VoltzDrawToolbar` visível; nenhuma thread
   do PID com janela Voltz Win32 ativa, com foco ou com o mouse capturado. Falha: `Stop-Process -Id` do PID lançado e o
   teste falha (BLOCKED).
6. **Sobreposição de teste fora do desktop virtual** (`VOLTZ_TEST_OVERLAY_RECT` à direita de todo o desktop, conferido
   contra o desktop virtual e cada monitor antes de abrir o app); o texto e o cursor conferidos no buffer
   (`ProbeOverlay`), nunca por captura de tela. O desktop do usuário é um monitor só (0,0) 2560×1080: nada do 4D
   depende de um segundo monitor (o 4E terá monitores falsos fora do desktop).
7. **A barra é testada sem janela:** `ToolbarImage` (agora também com a ferramenta texto e o seletor aberto) desenha uma
   `DrawToolbar` nunca mostrada por `QWidget::grab()`; a resposta diz `janela_nativa` (tem de ser `false`).
8. **A fonte é testada no núcleo:** glifos, retângulos e igualdade byte a byte pelo doctest, com a mesma fonte embutida
   que o app usa (nada de fonte do sistema).
9. **Demais regras (iguais ao 4C):** capturas só por `GetSourceScreenshot` de cena sintética (`Assert-NoRealCapture`) ou
   `PrintWindow` da casca lançada; nunca a tela inteira; nunca webcam/captura real (`VOLTZ_TEST_PRESET_STANDINS`); nunca
   quadros de vídeo (só o `anotacoes.jsonl` e metadados); nunca `Stop-Process -Name`/`taskkill /IM`; não abrir o
   Explorer; nunca tocar em `%APPDATA%\Voltz Studio`, `%APPDATA%\obs-studio` nem no que já existe em
   `%USERPROFILE%\Videos`; config portátil com backup e restauração em `try/finally` (o smoke confere por hash);
   gravações só em `-OutDir`/pasta do scratchpad.
10. **Se algum passo parecer exigir ligar o modo desenho, digitar ou focar um campo num teste automático, pare e
    reporte** — não improvise.

| Provado automaticamente | Só pelo roteiro manual do usuário |
|---|---|
| Texto no modelo e no `anotacoes.jsonl` (texto, tam, fonte, âncora), UTF-8, limites, estilo com tamanho/fonte — doctest | Digitar de verdade na janela de entrada: `WM_CHAR`, teclas mortas (´ + a), AltGr, Shift+Enter, Backspace, emoji |
| Layout (métricas falsas) e fonte embutida: glifos, retângulo cobre a tinta, parcial = inteiro byte a byte, cursor — doctest | Cursor em I sobre o monitor da Tela com a ferramenta texto; clique na barra sem perder a digitação |
| Digitação na `DrawSession`: começar, editar, Enter/Shift+Enter, Esc e Ctrl+Z só no texto, clicar de novo, toda saída confirma, inatividade — doctest | Texto no monitor real alinhado ao clique em 100/125/150%; texto ao vivo na prévia enquanto digita |
| Texto ao vivo pelos pedidos de teste: buffer da sobreposição (letras e cursor), programa (`GetSourceScreenshot`), fonte do plugin, `anotacoes.jsonl` com acentos e quebra, cancelar com e sem texto, log sem o conteúdo | Seletor de cor: arrastar no quadrado e na faixa sem perder o Esc; linha de dica ao passar o mouse |
| Barra com a ferramenta texto e o seletor aberto, desenhada fora da tela, sem janela nativa | Ajustes › Anotações: escolher cor no diálogo, sliders, prévia, o modo desenho começa com o estilo |
| Atalhos: tabela, JSON do perfil, regra de combinação, conflitos — doctest; "Limpar" e "Restaurar padrão" por UIA mudam o atalho ao vivo e o perfil; o modo desenho continua recusado | Gravar uma tecla no campo de atalho (atalhos pausados enquanto ele tem o foco), aviso de combinação inválida e de conflito, o atalho novo funcionando com outro app em foco |
| Ajustes › Anotações e › Atalhos existem e acompanham o estilo (UIA só leitura) | Configurações → Atalhos do modo clássico mostrando a mesma tecla; textos em en-US |
| Grep: foco/captura só no arquivo de exceção (mesmas contagens); pausar atalhos só no campo de atalho; texto fora do log; nenhuma injeção | — |

---

## Convenções

**Diretório de trabalho:** a raiz do repositório (`voltz-studio`), branch `voltz`. Leia o `CLAUDE.md` inteiro antes
(seções **Decisões** e **Armadilhas**, inclusive as de sobreposição, atalhos e testes). Base deste plano: commit
`a6d2655e4`.

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
  (contagem: `build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe | tail -3` → `test cases: N | N passed`)
- **SMOKE:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1`
- **SMOKE GRAVAÇÃO:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-record.ps1`
- **SMOKE PROJETO:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-project.ps1 -OutDir "<scratchpad>\smoke-projeto"`
- **SMOKE MOUSE:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-mouse.ps1 -OutDir "<scratchpad>\smoke-mouse"`
- **SMOKE ANOTAÇÕES:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-annotations.ps1 -OutDir "<scratchpad>\smoke-anotacoes"`
- **SMOKE DESENHO:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-draw.ps1`
- **SMOKE TEXTO** (Task 10): `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-text.ps1 -OutDir "<scratchpad>\smoke-texto"`
- **FORMATAR:** `"$CLANGFMT" -i <arquivos .cpp/.hpp novos ou alterados em frontend/voltz/ ou plugins/voltz-sources/>`
  (nunca nos arquivos do OBS; nunca em `.cmake`/`.ini`/`.ps1`/`.qss`/`.qrc`; nunca no `annotation-font-data.cpp`
  gerado no `build_x64`)
- **GREP DE SEGURANÇA** (Tasks 5, 8, 10 e 12; copie como está):
```bash
# A) foco/captura ativa: só no arquivo de exceção
grep -rnE "SetCapture|ReleaseCapture|SetForegroundWindow|SetFocus\(|SetActiveWindow|BringWindowToTop|AttachThreadInput|LockSetForegroundWindow|AllowSetForegroundWindow|grabMouse|grabKeyboard" frontend/voltz plugins/voltz-sources | grep -v "^frontend/voltz/annotate/DrawInputWindow.cpp:"; echo "foco-fora-da-excecao=$?"
# B) injeção/bloqueio de entrada: em lugar nenhum
grep -rnE "SendInput|SetCursorPos|mouse_event|keybd_event|BlockInput|ClipCursor|RegisterHotKey" frontend/voltz plugins/voltz-sources; echo "injecao=$?"
# C) observação passiva já existente: só onde já estava (3A e 4A)
grep -rln "SetWindowsHookEx" frontend/voltz plugins/voltz-sources; grep -rln "RegisterRawInputDevices" frontend/voltz plugins/voltz-sources
# D) no arquivo de exceção, só estas chamadas e nesta quantidade (as mesmas do 4C)
grep -oE "SetCapture\(|ReleaseCapture\(|SetForegroundWindow\(" frontend/voltz/annotate/DrawInputWindow.cpp | sort | uniq -c
# E) (4D) pausar os atalhos do OBS: só no campo de atalho de Ajustes › Atalhos, uma vez cada
grep -rlE "DisableHotkeys|UpdateHotkeyFocusSetting" frontend/voltz plugins/voltz-sources; grep -oE "DisableHotkeys\(\)|UpdateHotkeyFocusSetting\(\)" frontend/voltz/shell/HotkeySettings.cpp | sort | uniq -c
# F) (4D) o texto digitado nunca vai para o log (só a quantidade de caracteres, por textCharCount)
grep -rn -A2 "blog(" frontend/voltz plugins/voltz-sources | grep -E "\.text\b|command\.text" | grep -v "textCharCount("; echo "texto-no-log=$?"
```
  Expected: `foco-fora-da-excecao=1`; `injecao=1`; em C, exatamente `frontend/voltz/project/CursorTracker.cpp` e
  `frontend/voltz/project/LiveMouse.cpp`; em D, `1 ReleaseCapture(`, `1 SetCapture(` e `2 SetForegroundWindow(`; em E,
  só `frontend/voltz/shell/HotkeySettings.cpp` e depois `1 DisableHotkeys()` e `1 UpdateHotkeyFocusSetting()`;
  `texto-no-log=1`. (Antes da Task 8 o `HotkeySettings.cpp` não existe: E sai vazio e o segundo grep reclama — só A, B,
  C, D e F valem.)

`<scratchpad>` = `C:\Users\Cassio\AppData\Local\Temp\claude\C--Users-Cassio-Documents-Projetos-IA-voltz-studio\eee63625-8704-4453-920b-073461dee2a9\scratchpad`.

> **Regras fixas:**
> - **Sem patch no OBS.** `plugins/CMakeLists.txt` não muda (o plugin entra pelo `feature-voltz.cmake`); o
>   `OBSHotkeyEdit` é usado como está (`#include <settings/OBSHotkeyEdit.hpp>`). Se algo se mostrar impossível sem mexer
>   em arquivo do OBS, pare e reporte; se o controlador autorizar, a mudança vai dentro de `#ifdef VOLTZ_STUDIO` (ou com
>   comentário "Voltz Studio" no CMake) e é registrada em `docs/voltz/upstream-patches.md` **no mesmo commit**.
> - Builds longos **em segundo plano**, nunca dois ao mesmo tempo. `MSB4018`/`ZERO_CHECK`/"Touch" no `build_x86` →
>   rode de novo. Arquivo novo (código, locale, ícone no `.qrc`, `.cmake`) exige **CONFIGURAR** antes de compilar.
> - **Warnings como erro** (`/W3` + `CMAKE_COMPILE_WARNING_AS_ERROR`) valem para tudo, inclusive plugin, `voltz-core`,
>   o `.cpp` gerado das fontes e testes: casts explícitos em conversões numéricas (`double`→`int`, `double`→`float` nas
>   chamadas `QColor::fromHsvF`, `size_t`→`int`); `%zu` para `size_t` no `blog`, `%lld` + `static_cast<long long>`.
> - O alvo `obs-studio` compila com `QT_NO_CONTEXTLESS_CONNECT`: todo `connect` com lambda leva objeto de contexto.
> - Sinais/callbacks do libobs, do websocket e da thread de entrada chegam **fora** da thread da UI → só atômicos/mutex
>   ali (o `textTool` do `DrawShared` é atômico); o resto via `QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)`.
> - QSS da casca: prefixo `#voltzShell`; a barra (janela própria) usa `#voltzDrawToolbar` no QSS dela. Ícones no código
>   sempre por `phosphorIcon()`/`phosphorToggleIcon()`; ícone novo = SVG **oficial** do Phosphor 2.1.1 baixado do unpkg
>   com versão exata.
> - Textos novos nos **dois** locales da casca (`frontend/data/locale/voltz-pt-BR.ini` e `voltz-en-US.ini`, UTF-8 sem
>   BOM, fim de linha CRLF como o resto do arquivo), com paridade:
>   `diff <(cut -d= -f1 frontend/data/locale/voltz-en-US.ini | sort) <(cut -d= -f1 frontend/data/locale/voltz-pt-BR.ini | sort)` → sem saída.
>   O plugin não ganha texto novo neste plano.
> - Textos de `blog` e literais C em ASCII (sem acento), como no resto do código Voltz; **nunca** o texto digitado no log.
> - **Não** commitar `build-compile.log` nem `docs/voltz/backlog-plano-*.md` fora da Task 12. **Push só na Task 12**,
>   nunca `--force`.
> - Os trechos "trocar/por" deste plano foram gerados a partir do código já formatado: aplique-os em ordem (cada `old`
>   é único no arquivo naquele momento) e rode o clang-format no fim da task. Se um `old` não for encontrado, pare e
>   compare com o arquivo — não reescreva de memória.
>
> **Regras de segurança dos testes ao vivo (releia "Segurança dos testes" antes de cada teste):** nunca mexer no
> mouse/teclado do usuário; nunca ligar o modo desenho; nunca focar um campo de texto; texto e traços só por pedidos de
> teste; sobreposição de teste sempre fora do desktop virtual e conferida por `Assert-DrawSafe` depois de cada ação, com o
> vigia rodando; app só pelo PID lançado e fechado por `WM_CLOSE` na casca guardada; cenas só sintéticas; só metadados
> de vídeo; config portátil com backup; `Vídeos` intocada. PowerShell 5.1 lê `.ps1` sem BOM como ANSI: script do
> scratchpad com acento precisa de **BOM**; scripts versionados em `tools/` ficam em ASCII e montam acentos com
> `[char]0xF3` etc.

**Commits:** mensagem em PT-BR terminando com a linha `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

**Bloco de teste (PowerShell 5.1)** — o do 4C, com os nomes do 4D e ajudantes de leitura a mais (`Hotkeys`,
`BindingsText`, `Text-Json`, `Grid`, `Lime-Count`, `Profile-Ini`, `UiaFind`, `UiaExists`, `UiaValue`). Copie para um
`.ps1` **com BOM** no scratchpad, ajuste as variáveis marcadas e ponha os passos da task no `try`. Ele guarda a config
portátil, cria uma limpa (obs-websocket na porta 4455 sem senha, pt-BR, sem confirmação ao sair,
`HideOBSWindowsFromCapture=false`), liga os modos de teste, põe a sobreposição de teste **fora do desktop virtual**
(recusa se cruzar algum monitor), abre o app guardando a janela da casca, liga o **vigia** (job a cada 200 ms) e, no fim,
fecha o app por `WM_CLOSE` na casca guardada (só o PID lançado), confere que nenhuma janela Voltz ficou e restaura tudo.
**Valide dentro do `try`, depois de `Close-App`: o `finally` apaga a config.**
```powershell
$root = "C:\Users\Cassio\Documents\Projetos IA\voltz-studio"
$rundir = Join-Path $root "build_x64\rundir\RelWithDebInfo"; $bin = Join-Path $rundir "bin\64bit"
$sp = "C:\Users\Cassio\AppData\Local\Temp\claude\C--Users-Cassio-Documents-Projetos-IA-voltz-studio\eee63625-8704-4453-920b-073461dee2a9\scratchpad"
$cfg = Join-Path $rundir "config"; $bak = Join-Path $rundir "config.4dbak"; $voltzCfg = Join-Path $cfg "Voltz Studio"
if (Test-Path $bak) { throw "backup anterior em $bak - restaure (Move-Item) antes de continuar" }
if (@(Get-NetTCPConnection -LocalPort 4455 -State Listen -ErrorAction SilentlyContinue).Count -gt 0) { throw "porta 4455 ocupada (outro OBS/Voltz?)" }
# ---------- ajuste por task ----------
$ovW = 640; $ovH = 360       # tamanho do retangulo de teste da sobreposicao (sempre FORA do desktop virtual)
$freezeUi = $false           # VOLTZ_DEBUG_FREEZE_UI: atalho VoltzStudio.DebugFreezeUi (trava a UI por 8 s)
$voltzIni = "NextProjectName=Teste 4D`r`n"   # linhas da secao [Voltz]
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
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
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
$wdLog = Join-Path $sp "vigia-4d.txt"; $wdStop = Join-Path $sp "vigia-4d.stop"; Remove-Item $wdLog, $wdStop -ErrorAction SilentlyContinue
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
# --- ajudantes do 4D (so leitura na UI; nada e focado, digitado nem clicado fora de botoes por UIA/Invoke) ---
function Hotkeys { Start-Sleep -Milliseconds 300; return ((VendorShell "HotkeyState").atalhos | ConvertFrom-Json) }
function BindingsText($hotkeys, [string]$name) { return (ConvertTo-Json -InputObject $hotkeys.$name -Compress -Depth 5) }
function Text-Json([string]$points, [string]$text, [switch]$Open) {
  $req = [ordered]@{ pts = $points; texto = $text }; if ($Open) { $req.aberto = $true }
  return (ConvertTo-Json $req -Compress) }
function Grid([int]$x0, [int]$x1, [int]$dx, [int]$y0, [int]$y1, [int]$dy) { $pts = @()
  for ($y = $y0; $y -le $y1; $y += $dy) { for ($x = $x0; $x -le $x1; $x += $dx) { $pts += "[$x,$y]" } }
  return "[" + ($pts -join ",") + "]" }
function Lime-Count($img, [int]$x0, [int]$x1, [int]$y0, [int]$y1) { $n = 0
  for ($y = $y0; $y -lt $y1; $y++) { for ($x = $x0; $x -lt $x1; $x++) { if (Near $img.GetPixel($x, $y) 212 245 60 40) { $n++ } } }
  return $n }
function Profile-Ini { $ini = Get-ChildItem -LiteralPath (Join-Path $voltzCfg "basic\profiles") -Recurse -Filter basic.ini -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $ini) { return "" }; return [IO.File]::ReadAllText($ini.FullName, [Text.Encoding]::UTF8) }
# Elemento da UI Automation por nome (e tipo, opcional) numa janela do PID lancado; $null se nao achar. So leitura.
function UiaFind([string]$name, $type = $null) { $AE = [System.Windows.Automation.AutomationElement]
  $cond = New-Object System.Windows.Automation.PropertyCondition($AE::NameProperty, $name)
  if ($type) { $cond = New-Object System.Windows.Automation.AndCondition($cond, (New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $type))) }
  $pidCond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $script:proc.Id)
  foreach ($win in @($AE::RootElement.FindAll([System.Windows.Automation.TreeScope]::Children, $pidCond))) {
    $el = $win.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $cond); if ($null -ne $el) { return $el } }
  return $null }
function UiaExists([string]$name) { return ($null -ne (UiaFind $name)) }
# Valor atual (so leitura): ValuePattern (caixa de selecao) ou RangeValuePattern (slider)
function UiaValue([string]$name, $type) { $el = UiaFind $name $type; if ($null -eq $el) { return $null }
  $p = $null
  if ($el.TryGetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern, [ref]$p)) { return $p.Current.Value }
  if ($el.TryGetCurrentPattern([System.Windows.Automation.RangeValuePattern]::Pattern, [ref]$p)) { return $p.Current.Value }
  return $null }
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

**Cores e pixels de referência (iguais ao 4C):** Tela substituta = `#2A2A30` → (42,42,48); lima `#D4F53C` →
(212,245,60). A Tela 2560×1440 cabe no canvas 1920×1080 com escala 0,75 e a `Scene-Image` tem 640×360 ⇒ pixel (x,y) da
captura ≈ (x/4, y/4) na imagem. Sobreposição de teste 2560×1440 = escala 1 (o buffer tem os pixels da captura). Lima
opaco no buffer = `#FFD4F53C`; buffer limpo = `#00000000`. Texto de referência: âncora (400,300), tamanho 96, negrito ⇒
as maiúsculas ocupam ~x 400–700 e y ~330–395 da captura (≈ x 100–175, y 82–99 na `Scene-Image`).

---

## Mapa de arquivos

**Novos — núcleo (`frontend/voltz/core/`, sem Qt/libobs) e testes**
| Arquivo | Responsabilidade |
|---|---|
| `core/annotation-text.hpp/.cpp` | UTF-8 (`decodeUtf8`/`encodeUtf8`), caractere aceito, `appendTextCodepoint`/`popTextCodepoint` com limites (280 caracteres, 12 linhas), `sanitizeAnnotationText` |
| `core/text-layout.hpp/.cpp` | `GlyphMetrics` (interface), `PlacedGlyph`, `TextLayout`, `layoutText` (linhas, avanços, caixa, cursor) |
| `core/embed-fonts.cmake` | gera `annotation-font-data.cpp` (no `build_x64`) com os bytes das duas DM Sans |
| `core/annotation-font.hpp/.cpp` | FreeType + fontes embutidas (mutex), `embeddedFontAvailable`, `embeddedFontMetrics`, `renderGlyph`, `textCaretStroke` |
| `core/hotkey-bindings.hpp/.cpp` | `KeyCombo`, tabela `voltzHotkeys`, `defaultHotkeyBindings`, JSON do perfil, `checkVoltzHotkeyCombo`, `replaceFirstBinding`, `hotkeyConflicts` |
| `tests/test-annotation-text.cpp` | doctest do texto no modelo, UTF-8, estilo e `StrokeStore` (8 casos) |
| `tests/test-text-layout.cpp` | doctest do layout com métricas falsas (5 casos) |
| `tests/test-annotation-font.cpp` | doctest da fonte embutida e da rasterização do texto (6 casos) |
| `tests/test-draw-text.cpp` | doctest da digitação na `DrawSession` (8 casos) |
| `tests/test-hotkey-bindings.cpp` | doctest dos atalhos (6 casos) |

**Novos — frontend (alvo `obs-studio`) e ferramentas**
| Arquivo | Responsabilidade |
|---|---|
| `annotate/ColorPicker.hpp/.cpp` | seletor de cor completo da barra (sem diálogo, sem foco) |
| `shell/HotkeySettings.hpp/.cpp` | Ajustes › Atalhos (`VoltzHotkeyEdit` = `OBSHotkeyEdit` com os atalhos pausados no foco) |
| `shell/AnnotationSettings.hpp/.cpp` | Ajustes › Anotações (`AnnotationSettingsSection`, `AnnotationPreview`) |
| `shell/icons/regular/{text-b,eyedropper}.svg`, `shell/icons/fill/text-b-fill.svg` | ícones Phosphor 2.1.1 |
| `tools/voltz-smoke-text.ps1` | smoke do 4D (texto ao vivo e gravado; Ajustes › Atalhos/Anotações; barra com o texto e o seletor) |

**Modificados (Voltz):** `frontend/voltz/core/annotation-model.hpp/.cpp`, `core/annotation-raster.hpp/.cpp`,
`core/stroke-store.hpp/.cpp`, `core/draw-session.hpp/.cpp`, `core/CMakeLists.txt`, `tests/CMakeLists.txt`,
`tests/test-annotation-model.cpp`, `annotate/AnnotationController.hpp/.cpp`, `annotate/AnnotationTestVendor.hpp/.cpp`,
`annotate/DrawInputWindow.hpp/.cpp`, `annotate/DrawModeController.cpp`, `annotate/DrawToolbar.hpp/.cpp`,
`shell/HotkeyBinding.hpp/.cpp`, `shell/MouseStyleStore.cpp`, `shell/SettingsPage.hpp/.cpp`, `shell/VoltzShell.cpp`,
`shell/voltz-shell.qrc`, `shell/voltz-shell.qss`, `project/ProjectRecorder.cpp`, `frontend/cmake/feature-voltz.cmake`,
`frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`, `tools/voltz-smoke-draw.ps1`,
`tools/voltz-draw-demo.ps1`; na Task 12: `docs/voltz/backlog-plano-4.md`, `CLAUDE.md`, `AGENTS.md`,
`docs/HISTORICO.md`.

**Modificados (OBS upstream):** nenhum. (`plugins/CMakeLists.txt`, `plugins/voltz-sources/*` e
`docs/voltz/upstream-patches.md` não mudam; a Task 12 confere. O plugin ganha o texto só pelo `voltz-core`.)

**Contagem de testes:** 170 (base) → 178 (Task 1) → 183 (Task 2) → 189 (Task 3) → 197 (Task 4) → 203 (Task 7).

---

### Task 1: Núcleo — texto no modelo, UTF-8 e estilo (`annotation-model`, `annotation-text`, `StrokeStore`)

**Files:**
- Create: `frontend/voltz/core/annotation-text.hpp`, `frontend/voltz/core/annotation-text.cpp`
- Create: `frontend/voltz/tests/test-annotation-text.cpp`
- Modify (substituir): `frontend/voltz/core/annotation-model.hpp`, `frontend/voltz/core/annotation-model.cpp`
- Modify: `frontend/voltz/core/stroke-store.hpp`, `frontend/voltz/core/stroke-store.cpp`,
  `frontend/voltz/tests/test-annotation-model.cpp`, `frontend/voltz/core/CMakeLists.txt`,
  `frontend/voltz/tests/CMakeLists.txt`

O texto é um traço como os outros (`AnnotationTool::Text`, `"ferr":"texto"`): `points` guarda só a âncora (canto de cima
à esquerda da caixa, pixels da captura), `text` é UTF-8 com linhas por `'\n'`, `textSize` é o "em" em pixels da captura e
`textFont` uma das duas fontes embutidas. O estilo ganha tamanho e fonte do texto (`"tam"`, `"fonte"`). O `StrokeStore`
ganha o texto em andamento (começa **vazio**; o texto e o estilo dele mudam enquanto está aberto; um texto que fica vazio
sai sem registro).

- [ ] **Step 1: Escrever os testes que falham**

Criar `frontend/voltz/tests/test-annotation-text.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/annotation-text.hpp>
#include <voltz/core/stroke-store.hpp>

#include <algorithm>
#include <string>
#include <vector>

using voltz::AnnotationTool;
using voltz::RgbColor;
using voltz::Stroke;
using voltz::StrokePoint;
using voltz::TextFont;

namespace {

// "Olá" em UTF-8 escrito byte a byte (o arquivo de teste não depende da codificação do compilador).
const std::string kOla = "Ol\xC3\xA1";

} // namespace

TEST_CASE("texto: ferramenta e fonte (nomes, ida e volta); texto não é forma")
{
	AnnotationTool tool = AnnotationTool::Pen;
	CHECK(std::string(voltz::toolToString(AnnotationTool::Text)) == "texto");
	CHECK(voltz::toolFromString("texto", tool));
	CHECK(tool == AnnotationTool::Text);
	CHECK_FALSE(voltz::isShapeTool(AnnotationTool::Text));
	CHECK(voltz::isShapeTool(AnnotationTool::Line));
	CHECK(std::string(voltz::textFontToString(TextFont::Regular)) == "dm_sans");
	CHECK(std::string(voltz::textFontToString(TextFont::Bold)) == "dm_sans_negrito");
	TextFont font = TextFont::Regular;
	CHECK(voltz::textFontFromString("dm_sans_negrito", font));
	CHECK(font == TextFont::Bold);
	CHECK_FALSE(voltz::textFontFromString("arial", font));
	CHECK(font == TextFont::Bold);
	CHECK(voltz::clampTextSize(1) == voltz::kTextSizeMin);
	CHECK(voltz::clampTextSize(999) == voltz::kTextSizeMax);
}

TEST_CASE("strokeToJsonLine/strokeFromJson: texto com tam e fonte; campos de texto só no texto")
{
	Stroke stroke;
	stroke.id = 9;
	stroke.tool = AnnotationTool::Text;
	stroke.points = {{100, 200}};
	stroke.text = kOla + "\nmundo";
	stroke.textSize = 36;
	stroke.textFont = TextFont::Regular;
	stroke.startMs = 10;
	stroke.endMs = 20;
	const std::string line = voltz::strokeToJsonLine(stroke);
	CHECK(line == "{\"id\":9,\"ferr\":\"texto\",\"cor\":\"#D4F53C\",\"esp\":4,\"pts\":[[100,200]],\"texto\":\"" +
			      kOla + "\\nmundo\",\"tam\":36,\"fonte\":\"dm_sans\",\"t_ini\":10,\"t_fim\":20}");
	Stroke parsed;
	REQUIRE(voltz::strokeFromJson(line, parsed));
	CHECK(parsed.tool == AnnotationTool::Text);
	CHECK(parsed.text == stroke.text);
	CHECK(parsed.textSize == 36);
	CHECK(parsed.textFont == TextFont::Regular);
	REQUIRE(parsed.points.size() == 1);
	CHECK(parsed.points[0] == StrokePoint{100, 200});

	/* tolerância: vários pontos = só a âncora; tamanho limitado; fonte desconhecida = negrito; controles saem */
	REQUIRE(voltz::strokeFromJson(
		R"({"id":1,"ferr":"texto","pts":[[1,2],[3,4]],"texto":"a\u0007b\r\nc","tam":5000,"fonte":"x"})",
		parsed));
	CHECK(parsed.points.size() == 1);
	CHECK(parsed.text == "ab\nc");
	CHECK(parsed.textSize == voltz::kTextSizeMax);
	CHECK(parsed.textFont == TextFont::Bold);

	/* noutra ferramenta, "texto"/"tam"/"fonte" não entram nem saem */
	REQUIRE(voltz::strokeFromJson(R"({"id":2,"ferr":"linha","pts":[[0,0],[5,5]],"texto":"x","tam":30})", parsed));
	CHECK(parsed.text.empty());
	CHECK(parsed.textSize == voltz::kTextSizeDefault);
	CHECK(voltz::strokeToJsonLine(parsed).find("texto") == std::string::npos);
}

TEST_CASE("appendStrokePoints: texto guarda só a âncora")
{
	Stroke text;
	text.tool = AnnotationTool::Text;
	voltz::appendStrokePoints(text, {{5, 6}, {7, 8}});
	voltz::appendStrokePoints(text, {{9, 9}});
	REQUIRE(text.points.size() == 1);
	CHECK(text.points[0] == StrokePoint{5, 6});
}

TEST_CASE("AnnotationStyle: tamanho e fonte do texto no JSON, com limites")
{
	voltz::AnnotationStyle style;
	style.tool = AnnotationTool::Text;
	style.textSize = 80;
	style.textFont = TextFont::Regular;
	CHECK(voltz::annotationStyleToJson(style) ==
	      R"({"ferr":"texto","cor":"#D4F53C","esp":4,"somem_apos_s":0,"tam":80,"fonte":"dm_sans"})");
	voltz::AnnotationStyle parsed;
	REQUIRE(voltz::annotationStyleFromJson(voltz::annotationStyleToJson(style), parsed));
	CHECK(parsed.tool == AnnotationTool::Text);
	CHECK(parsed.textSize == 80);
	CHECK(parsed.textFont == TextFont::Regular);
	REQUIRE(voltz::annotationStyleFromJson(R"({"tam":2,"fonte":"comic"})", parsed));
	CHECK(parsed.textSize == voltz::kTextSizeMin);
	CHECK(parsed.textFont == TextFont::Bold);
	/* estilo de antes do 4D (sem tam/fonte): padrão */
	REQUIRE(voltz::annotationStyleFromJson(R"({"ferr":"seta","esp":8})", parsed));
	CHECK(parsed.textSize == voltz::kTextSizeDefault);
	CHECK(parsed.textFont == TextFont::Bold);
}

TEST_CASE("UTF-8: ida e volta; inválidos viram U+FFFD")
{
	const std::u32string codes = voltz::decodeUtf8(kOla + "\xF0\x9F\x98\x80"); // "Olá😀"
	REQUIRE(codes.size() == 4);
	CHECK(codes[2] == U'\u00E1');
	CHECK(codes[3] == U'\U0001F600');
	CHECK(voltz::encodeUtf8(codes) == kOla + "\xF0\x9F\x98\x80");
	CHECK(voltz::decodeUtf8("\x80") == std::u32string(1, voltz::kReplacementChar));         // continuação solta
	CHECK(voltz::decodeUtf8("\xC3") == std::u32string(1, voltz::kReplacementChar));         // curta
	CHECK(voltz::decodeUtf8("\xC0\xAF") == std::u32string(1, voltz::kReplacementChar));     // longa demais
	CHECK(voltz::decodeUtf8("\xED\xA0\x80") == std::u32string(1, voltz::kReplacementChar)); // substituto
	CHECK(voltz::decodeUtf8("\xC3"
				"a") == std::u32string{voltz::kReplacementChar, U'a'}); // curta, segue
	CHECK(voltz::encodeUtf8(std::u32string(1, char32_t(0xD800))) == "\xEF\xBF\xBD");
	CHECK(voltz::textCharCount(kOla) == 3);
}

TEST_CASE("appendTextCodepoint/popTextCodepoint: controles recusados; limites de caracteres e de linhas")
{
	std::string text;
	CHECK(voltz::appendTextCodepoint(text, U'O'));
	CHECK(voltz::appendTextCodepoint(text, U'\u00E1'));
	CHECK(text == "O\xC3\xA1");
	CHECK_FALSE(voltz::appendTextCodepoint(text, U'\b'));
	CHECK_FALSE(voltz::appendTextCodepoint(text, U'\r'));
	CHECK_FALSE(voltz::appendTextCodepoint(text, char32_t(0x1B)));
	CHECK_FALSE(voltz::appendTextCodepoint(text, char32_t(0x7F)));
	CHECK_FALSE(voltz::appendTextCodepoint(text, char32_t(0x85)));
	CHECK(voltz::popTextCodepoint(text));
	CHECK(text == "O");
	CHECK(voltz::popTextCodepoint(text));
	CHECK_FALSE(voltz::popTextCodepoint(text));

	std::string lines;
	for (size_t i = 0; i + 1 < voltz::kMaxTextLines; i++) {
		CHECK(voltz::appendTextCodepoint(lines, U'\n'));
	}
	CHECK_FALSE(voltz::appendTextCodepoint(lines, U'\n')); // a 13ª linha não entra
	CHECK(voltz::appendTextCodepoint(lines, U'x'));

	std::string full;
	for (size_t i = 0; i < voltz::kMaxTextChars; i++) {
		REQUIRE(voltz::appendTextCodepoint(full, U'a'));
	}
	CHECK_FALSE(voltz::appendTextCodepoint(full, U'b'));
	CHECK(voltz::textCharCount(full) == voltz::kMaxTextChars);
}

TEST_CASE("sanitizeAnnotationText: quebras, controles e cortes")
{
	CHECK(voltz::sanitizeAnnotationText("a\r\nb\rc\td") == "a\nb\ncd");
	CHECK(voltz::sanitizeAnnotationText("x\xFFy") == "x\xEF\xBF\xBDy");
	std::string many;
	for (int i = 0; i < 20; i++) {
		many += "l\n";
	}
	const std::string cut = voltz::sanitizeAnnotationText(many);
	CHECK(std::count(cut.begin(), cut.end(), '\n') == static_cast<long>(voltz::kMaxTextLines) - 1);
	CHECK(voltz::textCharCount(voltz::sanitizeAnnotationText(std::string(500, 'z'))) == voltz::kMaxTextChars);
}

TEST_CASE("StrokeStore: texto — beginText vazio, setOpenText, setOpenTextStyle, discardOpen")
{
	voltz::StrokeStore store;
	const int id = store.beginText(RgbColor{1, 2, 3}, 5000, TextFont::Regular, StrokePoint{10, 20}, 100);
	REQUIRE(id == 1);
	const Stroke &open = store.visible().back().stroke;
	CHECK(open.tool == AnnotationTool::Text);
	CHECK(open.text.empty());
	CHECK(open.textSize == voltz::kTextSizeMax);
	CHECK(open.textFont == TextFont::Regular);
	CHECK(store.beginText(RgbColor{}, 48, TextFont::Bold, StrokePoint{}, 110) == 0); // já há um em andamento
	CHECK(store.setOpenText(kOla + "\x07"));
	CHECK(store.visible().back().stroke.text == kOla);
	CHECK(store.setOpenTextStyle(RgbColor{9, 8, 7}, 3, TextFont::Bold));
	CHECK(store.visible().back().stroke.color == RgbColor{9, 8, 7});
	CHECK(store.visible().back().stroke.textSize == voltz::kTextSizeMin);
	CHECK(store.visible().back().stroke.textFont == TextFont::Bold);
	CHECK(store.extend({{1, 1}})); // a âncora não muda
	CHECK(store.visible().back().stroke.points.size() == 1);
	CHECK(store.discardOpen());
	CHECK(store.visible().empty());
	CHECK_FALSE(store.discardOpen());
	CHECK_FALSE(store.setOpenText("x")); // sem texto em andamento

	/* caneta em andamento: setOpenText recusa; depois de terminar, nada mais muda */
	REQUIRE(store.begin(AnnotationTool::Pen, RgbColor{}, 4, StrokePoint{0, 0}, 200) == 2);
	CHECK_FALSE(store.setOpenText("x"));
	CHECK_FALSE(store.setOpenTextStyle(RgbColor{}, 48, TextFont::Bold));
	REQUIRE(store.end(210) != nullptr);
	CHECK_FALSE(store.discardOpen());
	CHECK(store.visible().size() == 1);
}
```

Em `frontend/voltz/tests/test-annotation-model.cpp` (o texto deixou de ser ferramenta desconhecida e o estilo tem tamanho
e fonte), na ordem:
1. trocar:
```cpp
	AnnotationTool unchanged = AnnotationTool::Arrow;
	CHECK_FALSE(voltz::toolFromString("texto", unchanged));
	CHECK(unchanged == AnnotationTool::Arrow);
```
por:
```cpp
	AnnotationTool unchanged = AnnotationTool::Arrow;
	CHECK_FALSE(voltz::toolFromString("pincel", unchanged));
	CHECK(unchanged == AnnotationTool::Arrow);
```
2. trocar:
```cpp
	CHECK_FALSE(voltz::strokeFromJson(R"({"ferr":"seta"})", untouched));
	CHECK_FALSE(voltz::strokeFromJson(R"({"id":1,"ferr":"texto"})", untouched));
	CHECK_FALSE(voltz::strokeFromJson("[1,2]", untouched));
```
por:
```cpp
	CHECK_FALSE(voltz::strokeFromJson(R"({"ferr":"seta"})", untouched));
	CHECK_FALSE(voltz::strokeFromJson(R"({"id":1,"ferr":"pincel"})", untouched));
	CHECK_FALSE(voltz::strokeFromJson("[1,2]", untouched));
```
3. trocar:
```cpp
	const voltz::AnnotationStyle style;
	CHECK(voltz::annotationStyleToJson(style) == R"({"ferr":"caneta","cor":"#D4F53C","esp":4,"somem_apos_s":0})");
	voltz::AnnotationStyle parsed;
```
por:
```cpp
	const voltz::AnnotationStyle style;
	CHECK(voltz::annotationStyleToJson(style) ==
	      R"({"ferr":"caneta","cor":"#D4F53C","esp":4,"somem_apos_s":0,"tam":48,"fonte":"dm_sans_negrito"})");
	voltz::AnnotationStyle parsed;
```

- [ ] **Step 2: Ligar ao CMake e ver falhar**

Em `frontend/voltz/core/CMakeLists.txt`:
1. trocar:
```cmake
    annotation-raster.cpp
    atomic-file.cpp
```
por:
```cmake
    annotation-raster.cpp
    annotation-text.cpp
    atomic-file.cpp
```
2. trocar:
```cmake
    annotation-raster.hpp
    atomic-file.hpp
```
por:
```cmake
    annotation-raster.hpp
    annotation-text.hpp
    atomic-file.hpp
```

Em `frontend/voltz/tests/CMakeLists.txt`:
trocar:
```cmake
    test-annotation-raster.cpp
    test-atomic-file.cpp
```
por:
```cmake
    test-annotation-raster.cpp
    test-annotation-text.cpp
    test-atomic-file.cpp
```

Run: CONFIGURAR; COMPILAR TESTES.
Expected: FALHA (`annotation-text.hpp` não existe).

- [ ] **Step 3: UTF-8 e limites do texto**

Criar `frontend/voltz/core/annotation-text.hpp`:
```cpp
#pragma once

#include <cstddef>
#include <string>

namespace voltz {

// Texto das anotações (Plano 4D): UTF-8, linhas separadas por '\n'. Limites do que se digita no modo desenho e do que
// se lê do anotacoes.jsonl.
constexpr size_t kMaxTextChars = 280; // caracteres (códigos Unicode), contando os '\n'
constexpr size_t kMaxTextLines = 12;
constexpr char32_t kReplacementChar = 0xFFFD;

// UTF-8 → códigos. Sequência inválida (byte solto, curta, longa demais, substituto, > U+10FFFF) vira U+FFFD.
std::u32string decodeUtf8(const std::string &text);
// Códigos → UTF-8. Código inválido (substituto, > U+10FFFF) vira U+FFFD.
std::string encodeUtf8(const std::u32string &text);

// Caractere aceito no texto: '\n' ou imprimível (não é controle C0/C1 nem DEL, nem substituto, nem > U+10FFFF).
bool isTextCodepoint(char32_t codepoint);
// Caracteres (códigos) do texto em UTF-8.
size_t textCharCount(const std::string &text);
// Acrescenta `codepoint` no fim se for aceito e couber (kMaxTextChars caracteres, kMaxTextLines linhas). Falso = nada
// mudou.
bool appendTextCodepoint(std::string &text, char32_t codepoint);
// Tira o último caractere. Falso se o texto está vazio.
bool popTextCodepoint(std::string &text);
// Texto vindo de fora (anotacoes.jsonl, pedido de teste): UTF-8 válido, "\r\n" e "\r" viram '\n', sem os outros
// controles, cortado em kMaxTextChars caracteres e kMaxTextLines linhas.
std::string sanitizeAnnotationText(const std::string &text);

} // namespace voltz
```

Criar `frontend/voltz/core/annotation-text.cpp`:
```cpp
#include "annotation-text.hpp"

#include <algorithm>
#include <cstdint>

namespace voltz {

namespace {

bool validCodepoint(char32_t codepoint)
{
	return codepoint <= 0x10FFFF && !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
}

void appendEncoded(std::string &out, char32_t codepoint)
{
	if (!validCodepoint(codepoint)) {
		codepoint = kReplacementChar;
	}
	if (codepoint < 0x80) {
		out.push_back(static_cast<char>(codepoint));
	} else if (codepoint < 0x800) {
		out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
	} else if (codepoint < 0x10000) {
		out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
	} else {
		out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
	}
}

size_t lineCount(const std::u32string &text)
{
	return static_cast<size_t>(std::count(text.begin(), text.end(), U'\n')) + 1;
}

} // namespace

std::u32string decodeUtf8(const std::string &text)
{
	std::u32string out;
	out.reserve(text.size());
	size_t i = 0;
	while (i < text.size()) {
		const auto lead = static_cast<uint8_t>(text[i]);
		size_t length = 0;
		char32_t codepoint = 0;
		char32_t minimum = 0;
		if (lead < 0x80) {
			out.push_back(lead);
			i++;
			continue;
		} else if ((lead & 0xE0) == 0xC0) {
			length = 2;
			codepoint = lead & 0x1Fu;
			minimum = 0x80;
		} else if ((lead & 0xF0) == 0xE0) {
			length = 3;
			codepoint = lead & 0x0Fu;
			minimum = 0x800;
		} else if ((lead & 0xF8) == 0xF0) {
			length = 4;
			codepoint = lead & 0x07u;
			minimum = 0x10000;
		} else {
			out.push_back(kReplacementChar); /* byte de continuação solto ou inválido */
			i++;
			continue;
		}
		size_t used = 1;
		bool ok = true;
		for (; used < length; used++) {
			if (i + used >= text.size() || (static_cast<uint8_t>(text[i + used]) & 0xC0) != 0x80) {
				ok = false;
				break;
			}
			codepoint = (codepoint << 6) | (static_cast<uint8_t>(text[i + used]) & 0x3Fu);
		}
		if (!ok || codepoint < minimum || !validCodepoint(codepoint)) {
			out.push_back(kReplacementChar);
			i += ok ? length : used; /* sequência curta: recomeça no byte que quebrou */
			continue;
		}
		out.push_back(codepoint);
		i += length;
	}
	return out;
}

std::string encodeUtf8(const std::u32string &text)
{
	std::string out;
	out.reserve(text.size());
	for (const char32_t codepoint : text) {
		appendEncoded(out, codepoint);
	}
	return out;
}

bool isTextCodepoint(char32_t codepoint)
{
	if (codepoint == U'\n') {
		return true;
	}
	if (codepoint < 0x20 || (codepoint >= 0x7F && codepoint <= 0x9F)) {
		return false;
	}
	return validCodepoint(codepoint);
}

size_t textCharCount(const std::string &text)
{
	return decodeUtf8(text).size();
}

bool appendTextCodepoint(std::string &text, char32_t codepoint)
{
	if (!isTextCodepoint(codepoint)) {
		return false;
	}
	const std::u32string current = decodeUtf8(text);
	if (current.size() >= kMaxTextChars) {
		return false;
	}
	if (codepoint == U'\n' && lineCount(current) >= kMaxTextLines) {
		return false;
	}
	appendEncoded(text, codepoint);
	return true;
}

bool popTextCodepoint(std::string &text)
{
	if (text.empty()) {
		return false;
	}
	std::u32string current = decodeUtf8(text);
	current.pop_back();
	text = encodeUtf8(current);
	return true;
}

std::string sanitizeAnnotationText(const std::string &text)
{
	const std::u32string input = decodeUtf8(text);
	std::u32string out;
	out.reserve(std::min(input.size(), kMaxTextChars));
	size_t lines = 1;
	for (size_t i = 0; i < input.size() && out.size() < kMaxTextChars; i++) {
		char32_t codepoint = input[i];
		if (codepoint == U'\r') {
			if (i + 1 < input.size() && input[i + 1] == U'\n') {
				continue; /* "\r\n": o '\n' seguinte conta */
			}
			codepoint = U'\n';
		}
		if (!isTextCodepoint(codepoint)) {
			continue;
		}
		if (codepoint == U'\n') {
			if (lines >= kMaxTextLines) {
				break;
			}
			lines++;
		}
		out.push_back(codepoint);
	}
	return encodeUtf8(out);
}

} // namespace voltz
```

- [ ] **Step 4: Modelo do traço e do estilo com o texto**

Substituir o conteúdo de `frontend/voltz/core/annotation-model.hpp` por:
```cpp
#pragma once

#include "mouse-style.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Desenho na tela (spec §6.2). Ferramentas do Plano 4B; o texto entra no 4D.
enum class AnnotationTool { Pen, Highlighter, Line, Arrow, Rectangle, Ellipse, Text };

// "caneta" | "marca_texto" | "linha" | "seta" | "retangulo" | "elipse" | "texto" (campo "ferr" do anotacoes.jsonl).
const char *toolToString(AnnotationTool tool);
// Falso (e `out` intacto) para texto desconhecido.
bool toolFromString(const std::string &text, AnnotationTool &out);
// Linha, seta, retângulo e elipse: só o primeiro e o último ponto contam. (O texto guarda só o primeiro.)
bool isShapeTool(AnnotationTool tool);

// Fonte do texto: as duas embutidas no voltz-core (DM Sans, OFL), as mesmas na sobreposição e na fonte do plugin.
// "dm_sans" | "dm_sans_negrito" (campo "fonte").
enum class TextFont { Regular, Bold };
const char *textFontToString(TextFont font);
// Falso (e `out` intacto) para nome desconhecido.
bool textFontFromString(const std::string &text, TextFont &out);

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
// Tamanho do texto: altura do "em" em pixels da captura (como o font-size do CSS).
constexpr int kTextSizeMin = 12;
constexpr int kTextSizeMax = 200;
constexpr int kTextSizeDefault = 48;

int clampStrokeWidth(int width);
int clampTextSize(int size);

// Um traço. Tempos em ms relativos ao t0 do projeto (spec §5.5); -1 = ainda não aconteceu. Texto (Plano 4D): `points`
// = só a âncora (canto de cima à esquerda da caixa do texto, pixels da captura), `text` em UTF-8 (linhas separadas por
// '\n'; annotation-text.hpp), `textSize` e `textFont`; `width` não conta.
struct Stroke {
	int id = 0;
	AnnotationTool tool = AnnotationTool::Pen;
	RgbColor color{0xD4, 0xF5, 0x3C};
	int width = 4;
	std::vector<StrokePoint> points;
	std::string text;
	int textSize = kTextSizeDefault;
	TextFont textFont = TextFont::Bold;
	int64_t startMs = 0;
	int64_t endMs = -1;
	int64_t clearedMs = -1;
};

// Acrescenta pontos. Caneta/marca-texto: no fim, sem repetir o último e até kMaxStrokePoints. Formas: o primeiro ponto
// fica e o segundo passa a ser o último recebido (no máximo 2 pontos). Texto: só o primeiro ponto (a âncora) conta.
void appendStrokePoints(Stroke &stroke, const std::vector<StrokePoint> &points);

// [[x,y],[x,y]] compacto.
std::string pointsToJson(const std::vector<StrokePoint> &points);
// Falso (e `out` intacto) se não for uma lista; itens que não são [número, número] são ignorados.
bool pointsFromJson(const std::string &json, std::vector<StrokePoint> &out);

// Linha do anotacoes.jsonl, sem "\n" (spec §6.2):
// {"id":7,"ferr":"seta","cor":"#D4F53C","esp":4,"pts":[[x,y],...],"t_ini":1200,"t_fim":1850,"t_limpo":9000}
// t_fim e t_limpo ficam de fora quando -1. Texto: "texto", "tam" e "fonte" logo depois de "pts":
// {"id":9,"ferr":"texto","cor":"#D4F53C","esp":4,"pts":[[x,y]],"texto":"Olá","tam":48,"fonte":"dm_sans_negrito",...}
std::string strokeToJsonLine(const Stroke &stroke);
// Leitura tolerante: precisa de objeto com "id" inteiro e "ferr" conhecido; cor inválida = lima; espessura limitada;
// tempos ausentes = 0 (t_ini) e -1 (t_fim, t_limpo). Texto: "texto" limpo (sanitizeAnnotationText), "tam" limitado,
// "fonte" desconhecida = negrito; nas outras ferramentas esses campos são ignorados. Falso deixa `out` intacto.
bool strokeFromJson(const std::string &json, Stroke &out);

// Estilo das anotações (user.ini [Voltz] AnnotationStyle): ferramenta, cor e espessura do próximo traço, "traços somem
// após N s" (0 = nunca) e tamanho/fonte do texto.
// {"ferr":"caneta","cor":"#D4F53C","esp":4,"somem_apos_s":0,"tam":48,"fonte":"dm_sans_negrito"}
struct AnnotationStyle {
	AnnotationTool tool = AnnotationTool::Pen;
	RgbColor color{0xD4, 0xF5, 0x3C};
	int width = 4;
	int fadeSeconds = 0;
	int textSize = kTextSizeDefault;
	TextFont textFont = TextFont::Bold;
};

AnnotationStyle clampAnnotationStyle(AnnotationStyle style);
std::string annotationStyleToJson(const AnnotationStyle &style);
// `out` recebe o padrão + o que vier válido (limitado). Falso só para JSON inválido ou que não é objeto (aí `out` fica
// no padrão).
bool annotationStyleFromJson(const std::string &json, AnnotationStyle &out);

} // namespace voltz
```

Substituir o conteúdo de `frontend/voltz/core/annotation-model.cpp` por:
```cpp
#include "annotation-model.hpp"
#include "annotation-text.hpp"

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
	{AnnotationTool::Text, "texto"},
};

struct FontName {
	TextFont font;
	const char *name;
};

constexpr FontName kFontNames[] = {{TextFont::Regular, "dm_sans"}, {TextFont::Bold, "dm_sans_negrito"}};

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

// "tam" e "fonte" (traço de texto e estilo).
void readTextFields(const Json &object, int &size, TextFont &font)
{
	int64_t value = 0;
	if (readInt64(object, "tam", value)) {
		size = clampTextSize(toIntClamped(value));
	}
	const auto fontIt = object.find("fonte");
	if (fontIt != object.end() && fontIt->is_string()) {
		textFontFromString(fontIt->get<std::string>(), font);
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
	return tool == AnnotationTool::Line || tool == AnnotationTool::Arrow || tool == AnnotationTool::Rectangle ||
	       tool == AnnotationTool::Ellipse;
}

const char *textFontToString(TextFont font)
{
	for (const FontName &entry : kFontNames) {
		if (entry.font == font) {
			return entry.name;
		}
	}
	return "dm_sans_negrito";
}

bool textFontFromString(const std::string &text, TextFont &out)
{
	for (const FontName &entry : kFontNames) {
		if (text == entry.name) {
			out = entry.font;
			return true;
		}
	}
	return false;
}

int clampStrokeWidth(int width)
{
	return std::clamp(width, kStrokeWidthMin, kStrokeWidthMax);
}

int clampTextSize(int size)
{
	return std::clamp(size, kTextSizeMin, kTextSizeMax);
}

void appendStrokePoints(Stroke &stroke, const std::vector<StrokePoint> &points)
{
	if (points.empty()) {
		return;
	}
	if (stroke.tool == AnnotationTool::Text) {
		if (stroke.points.empty()) {
			stroke.points.push_back(points.front());
		}
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
	if (stroke.tool == AnnotationTool::Text) {
		json["texto"] = stroke.text;
		json["tam"] = stroke.textSize;
		json["fonte"] = textFontToString(stroke.textFont);
	}
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
		std::vector<StrokePoint> points;
		readPoints(*pointsIt, points);
		if (tool == AnnotationTool::Text) {
			appendStrokePoints(stroke, points); /* só a âncora */
		} else {
			stroke.points = std::move(points);
		}
	}
	if (tool == AnnotationTool::Text) {
		const auto textIt = root.find("texto");
		if (textIt != root.end() && textIt->is_string()) {
			stroke.text = sanitizeAnnotationText(textIt->get<std::string>());
		}
		readTextFields(root, stroke.textSize, stroke.textFont);
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
	style.textSize = clampTextSize(style.textSize);
	return style;
}

std::string annotationStyleToJson(const AnnotationStyle &style)
{
	Json json = Json::object();
	json["ferr"] = toolToString(style.tool);
	json["cor"] = formatHexColor(style.color);
	json["esp"] = style.width;
	json["somem_apos_s"] = style.fadeSeconds;
	json["tam"] = style.textSize;
	json["fonte"] = textFontToString(style.textFont);
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
	readTextFields(root, out.textSize, out.textFont);
	out = clampAnnotationStyle(out);
	return true;
}

} // namespace voltz
```

(Em relação ao arquivo de antes: `Text` em `AnnotationTool` e na tabela de nomes; `TextFont` com os nomes `dm_sans` e
`dm_sans_negrito`; `isShapeTool` agora lista as quatro formas em vez de "tudo que não é caneta/marca-texto"; o texto
guarda só o primeiro ponto em `appendStrokePoints`; `"texto"`, `"tam"` e `"fonte"` no JSON do traço — só no texto — e no
do estilo; `readTextFields` comum aos dois. O resto é igual.)

- [ ] **Step 5: `StrokeStore` com o texto em andamento**

Em `frontend/voltz/core/stroke-store.hpp`:
1. trocar:
```cpp
#include <cstdint>
#include <vector>
```
por:
```cpp
#include <cstdint>
#include <string>
#include <vector>
```
2. trocar:
```cpp
	bool extend(const std::vector<StrokePoint> &points);
	// Termina o traço em andamento; nullptr se não há. O ponteiro vale até a próxima mudança.
```
por:
```cpp
	bool extend(const std::vector<StrokePoint> &points);
	// Texto (Plano 4D): começa um traço de texto VAZIO com a âncora (canto de cima à esquerda da caixa), tamanho
	// limitado e fonte. 0 se já há um em andamento.
	int beginText(const RgbColor &color, int textSize, TextFont font, const StrokePoint &anchor, int64_t nowNs);
	// Troca o texto do traço de texto em andamento (sanitizeAnnotationText). Falso sem traço de texto em andamento.
	bool setOpenText(const std::string &text);
	// Cor, tamanho (limitado) e fonte do texto em andamento (mudados na barra durante a digitação). Falso sem texto em
	// andamento.
	bool setOpenTextStyle(const RgbColor &color, int textSize, TextFont font);
	// Tira o traço em andamento SEM devolvê-lo (texto cancelado que ficou vazio: nada apareceu, nada a registrar).
	// Falso se não há traço em andamento.
	bool discardOpen();
	// Termina o traço em andamento; nullptr se não há. O ponteiro vale até a próxima mudança.
```

Em `frontend/voltz/core/stroke-store.cpp`:
1. trocar:
```cpp
#include "stroke-store.hpp"

#include <algorithm>
```
por:
```cpp
#include "stroke-store.hpp"
#include "annotation-text.hpp"

#include <algorithm>
```
2. trocar:
```cpp
	appendStrokePoints(visible_.back().stroke, points);
	return true;
```
por:
```cpp
	appendStrokePoints(visible_.back().stroke, points);
	return true;
}

int StrokeStore::beginText(const RgbColor &color, int textSize, TextFont font, const StrokePoint &anchor, int64_t nowNs)
{
	const int id = begin(AnnotationTool::Text, color, kStrokeWidthMin, anchor, nowNs);
	if (id != 0) {
		Stroke &stroke = visible_.back().stroke;
		stroke.textSize = clampTextSize(textSize);
		stroke.textFont = font;
	}
	return id;
}

bool StrokeStore::setOpenText(const std::string &text)
{
	if (visible_.empty() || !visible_.back().open() || visible_.back().stroke.tool != AnnotationTool::Text) {
		return false;
	}
	visible_.back().stroke.text = sanitizeAnnotationText(text);
	return true;
}

bool StrokeStore::setOpenTextStyle(const RgbColor &color, int textSize, TextFont font)
{
	if (visible_.empty() || !visible_.back().open() || visible_.back().stroke.tool != AnnotationTool::Text) {
		return false;
	}
	Stroke &stroke = visible_.back().stroke;
	stroke.color = color;
	stroke.textSize = clampTextSize(textSize);
	stroke.textFont = font;
	return true;
}

bool StrokeStore::discardOpen()
{
	if (visible_.empty() || !visible_.back().open()) {
		return false;
	}
	visible_.pop_back();
	return true;
```

- [ ] **Step 6: Rodar e ver passar**

```bash
"$CLANGFMT" -i frontend/voltz/core/annotation-text.hpp frontend/voltz/core/annotation-text.cpp frontend/voltz/core/annotation-model.hpp frontend/voltz/core/annotation-model.cpp frontend/voltz/core/stroke-store.hpp frontend/voltz/core/stroke-store.cpp frontend/voltz/tests/test-annotation-text.cpp frontend/voltz/tests/test-annotation-model.cpp
git diff --stat
```
Run: COMPILAR TESTES; RODAR TESTES; `build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe | tail -3`.
Expected: `100% tests passed`; `test cases: 178 | 178 passed` (1497 asserções). O clang-format não muda nada (o código
acima já está formatado).

- [ ] **Step 7: Commit**

```bash
git add frontend/voltz/core/annotation-text.hpp frontend/voltz/core/annotation-text.cpp frontend/voltz/core/annotation-model.hpp frontend/voltz/core/annotation-model.cpp frontend/voltz/core/stroke-store.hpp frontend/voltz/core/stroke-store.cpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/CMakeLists.txt frontend/voltz/tests/test-annotation-text.cpp frontend/voltz/tests/test-annotation-model.cpp
git commit -m "feat(voltz): texto no modelo das anotacoes (ferramenta, anotacoes.jsonl, estilo, UTF-8 e limites, texto em andamento no StrokeStore)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 2: Núcleo — layout do texto sem fonte real (`text-layout`)

**Files:**
- Create: `frontend/voltz/core/text-layout.hpp`, `frontend/voltz/core/text-layout.cpp`
- Create: `frontend/voltz/tests/test-text-layout.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

O layout só precisa de medidas em unidades da fonte (interface `GlyphMetrics`): a fonte embutida (Task 3) e os testes
(fonte falsa, monoespaçada) dão as medidas. Âncora no canto de cima à esquerda; primeira linha de base `ascender` abaixo
dela; cada linha seguinte `lineHeight` abaixo; cada caractere avança o seu `advance`; sem kerning nem arredondamento
(quem desenha arredonda) — por isso o resultado é proporcional ao tamanho.

- [ ] **Step 1: Escrever o teste que falha**

Criar `frontend/voltz/tests/test-text-layout.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/text-layout.hpp>

#include <string>

using doctest::Approx;
using voltz::TextLayout;

namespace {

// Fonte falsa: em de 1000 unidades, ascender 800, descender -200, linhas a cada 1200; "W" 900, espaço 250, o resto 500.
class FakeMetrics : public voltz::GlyphMetrics {
public:
	int unitsPerEm() const override { return 1000; }
	int ascender() const override { return 800; }
	int descender() const override { return -200; }
	int lineHeight() const override { return 1200; }
	int advance(char32_t codepoint) const override
	{
		if (codepoint == U'W') {
			return 900;
		}
		return codepoint == U' ' ? 250 : 500;
	}
};

const FakeMetrics kMetrics;

} // namespace

TEST_CASE("layoutText: uma linha — origens, largura, altura e cursor")
{
	const TextLayout layout = voltz::layoutText("AW b", 10.0, kMetrics); // escala 0,01 px por unidade
	REQUIRE(layout.glyphs.size() == 4);
	CHECK(layout.pixelSize == Approx(10.0));
	CHECK(layout.glyphs[0].x == Approx(0.0));
	CHECK(layout.glyphs[1].x == Approx(5.0));
	CHECK(layout.glyphs[2].x == Approx(14.0));
	CHECK(layout.glyphs[3].x == Approx(16.5));
	CHECK(layout.glyphs[3].codepoint == U'b');
	for (const voltz::PlacedGlyph &glyph : layout.glyphs) {
		CHECK(glyph.baseline == Approx(8.0));
	}
	CHECK(layout.lines == 1);
	CHECK(layout.width == Approx(21.5));
	CHECK(layout.height == Approx(10.0));
	CHECK(layout.lineAdvance == Approx(12.0));
	CHECK(layout.caretX == Approx(21.5));
	CHECK(layout.caretTop == Approx(0.0));
	CHECK(layout.caretBottom == Approx(10.0));
}

TEST_CASE("layoutText: várias linhas, linha vazia e '\\n' no fim (cursor na linha nova)")
{
	const TextLayout layout = voltz::layoutText("ab\n\nW\n", 100.0, kMetrics);
	REQUIRE(layout.glyphs.size() == 3);
	CHECK(layout.lines == 4);
	CHECK(layout.glyphs[1].x == Approx(50.0));
	CHECK(layout.glyphs[1].baseline == Approx(80.0));
	CHECK(layout.glyphs[2].x == Approx(0.0));
	CHECK(layout.glyphs[2].baseline == Approx(80.0 + 2 * 120.0));
	CHECK(layout.width == Approx(100.0)); // "ab" = 50 + 50; "W" = 90
	CHECK(layout.height == Approx(3 * 120.0 + 100.0));
	CHECK(layout.caretX == Approx(0.0));
	CHECK(layout.caretTop == Approx(3 * 120.0));
	CHECK(layout.caretBottom == Approx(3 * 120.0 + 100.0));
}

TEST_CASE("layoutText: tudo proporcional ao tamanho")
{
	const TextLayout small = voltz::layoutText("Wa\nb", 24.0, kMetrics);
	const TextLayout big = voltz::layoutText("Wa\nb", 48.0, kMetrics);
	REQUIRE(small.glyphs.size() == big.glyphs.size());
	for (size_t i = 0; i < small.glyphs.size(); i++) {
		CHECK(big.glyphs[i].x == Approx(2.0 * small.glyphs[i].x));
		CHECK(big.glyphs[i].baseline == Approx(2.0 * small.glyphs[i].baseline));
	}
	CHECK(big.width == Approx(2.0 * small.width));
	CHECK(big.height == Approx(2.0 * small.height));
	CHECK(big.caretX == Approx(2.0 * small.caretX));
}

TEST_CASE("layoutText: texto vazio é uma caixa de uma linha com o cursor na âncora; tamanho inválido = vazio")
{
	const TextLayout empty = voltz::layoutText("", 50.0, kMetrics);
	CHECK(empty.glyphs.empty());
	CHECK(empty.lines == 1);
	CHECK(empty.width == Approx(0.0));
	CHECK(empty.height == Approx(50.0));
	CHECK(empty.caretX == Approx(0.0));
	CHECK(empty.caretTop == Approx(0.0));
	CHECK(empty.caretBottom == Approx(50.0));
	const TextLayout invalid = voltz::layoutText("abc", 0.0, kMetrics);
	CHECK(invalid.glyphs.empty());
	CHECK(invalid.pixelSize == Approx(0.0));
}

TEST_CASE("layoutText: UTF-8 — acento é um caractere; byte inválido vira U+FFFD")
{
	const TextLayout layout = voltz::layoutText("\xC3\xA1\xFF", 10.0, kMetrics); // "á" + byte inválido
	REQUIRE(layout.glyphs.size() == 2);
	CHECK(layout.glyphs[0].codepoint == U'\u00E1');
	CHECK(layout.glyphs[1].codepoint == U'\uFFFD');
	CHECK(layout.glyphs[1].x == Approx(5.0));
	CHECK(layout.width == Approx(10.0));
}
```

- [ ] **Step 2: Ligar ao CMake e ver falhar**

Em `frontend/voltz/core/CMakeLists.txt`:
1. trocar:
```cmake
    svg-recolor.cpp
    time-format.cpp
```
por:
```cmake
    svg-recolor.cpp
    text-layout.cpp
    time-format.cpp
```
2. trocar:
```cmake
    svg-recolor.hpp
    time-format.hpp
```
por:
```cmake
    svg-recolor.hpp
    text-layout.hpp
    time-format.hpp
```

Em `frontend/voltz/tests/CMakeLists.txt`:
trocar:
```cmake
    test-svg-recolor.cpp
    test-time-format.cpp
```
por:
```cmake
    test-svg-recolor.cpp
    test-text-layout.cpp
    test-time-format.cpp
```

Run: CONFIGURAR; COMPILAR TESTES.
Expected: FALHA (`text-layout.hpp` não existe).

- [ ] **Step 3: Implementação**

Criar `frontend/voltz/core/text-layout.hpp`:
```cpp
#pragma once

#include <string>
#include <vector>

namespace voltz {

// Medidas de uma fonte em unidades da fonte — só o que o layout precisa. A de verdade é a da fonte embutida
// (annotation-font: DM Sans pelo FreeType); os testes usam uma falsa.
class GlyphMetrics {
public:
	virtual ~GlyphMetrics() = default;
	virtual int unitsPerEm() const = 0;
	virtual int ascender() const = 0;                  // > 0: acima da linha de base
	virtual int descender() const = 0;                 // <= 0: abaixo da linha de base
	virtual int lineHeight() const = 0;                // distância entre duas linhas de base
	virtual int advance(char32_t codepoint) const = 0; // avanço horizontal do caractere (sem kerning)
};

// Um caractere posicionado: origem (na linha de base) em pixels, relativa à âncora do texto.
struct PlacedGlyph {
	char32_t codepoint = 0;
	double x = 0.0;
	double baseline = 0.0;
};

// Texto montado, em pixels relativos à âncora (canto de cima à esquerda da caixa; y cresce para baixo).
struct TextLayout {
	std::vector<PlacedGlyph> glyphs; // sem os '\n'
	double pixelSize = 0.0;
	double lineAdvance = 0.0; // de uma linha de base à seguinte
	int lines = 1;
	double width = 0.0;  // a linha mais larga
	double height = 0.0; // do topo da primeira linha (âncora) ao pé da última (ascender - descender na última)
	// Cursor de digitação: depois do último caractere, na última linha (do topo ao pé dela).
	double caretX = 0.0;
	double caretTop = 0.0;
	double caretBottom = 0.0;
};

// Monta `text` (UTF-8, linhas separadas por '\n') com o "em" de `pixelSize` pixels: a primeira linha de base fica
// `ascender` abaixo da âncora e cada linha seguinte `lineHeight` abaixo da anterior; cada caractere avança o seu
// `advance`. Sem quebra automática, sem kerning, sem arredondar (quem desenha arredonda). Métricas inválidas
// (unitsPerEm <= 0) ou pixelSize <= 0 → layout vazio com pixelSize 0.
TextLayout layoutText(const std::string &text, double pixelSize, const GlyphMetrics &metrics);

} // namespace voltz
```

Criar `frontend/voltz/core/text-layout.cpp`:
```cpp
#include "text-layout.hpp"
#include "annotation-text.hpp"

#include <algorithm>

namespace voltz {

TextLayout layoutText(const std::string &text, double pixelSize, const GlyphMetrics &metrics)
{
	TextLayout layout;
	const int units = metrics.unitsPerEm();
	if (units <= 0 || pixelSize <= 0.0) {
		return layout;
	}
	const double scale = pixelSize / static_cast<double>(units);
	const double ascender = static_cast<double>(metrics.ascender()) * scale;
	const double descender = static_cast<double>(metrics.descender()) * scale;
	layout.pixelSize = pixelSize;
	layout.lineAdvance = static_cast<double>(metrics.lineHeight()) * scale;
	double x = 0.0;
	double baseline = ascender;
	for (const char32_t codepoint : decodeUtf8(text)) {
		if (codepoint == U'\n') {
			x = 0.0;
			baseline += layout.lineAdvance;
			layout.lines++;
			continue;
		}
		layout.glyphs.push_back(PlacedGlyph{codepoint, x, baseline});
		x += static_cast<double>(metrics.advance(codepoint)) * scale;
		layout.width = std::max(layout.width, x);
	}
	layout.height = baseline - descender;
	layout.caretX = x;
	layout.caretTop = baseline - ascender;
	layout.caretBottom = baseline - descender;
	return layout;
}

} // namespace voltz
```

- [ ] **Step 4: Rodar e ver passar**

```bash
"$CLANGFMT" -i frontend/voltz/core/text-layout.hpp frontend/voltz/core/text-layout.cpp frontend/voltz/tests/test-text-layout.cpp
```
Run: COMPILAR TESTES; RODAR TESTES; `build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe | tail -3`.
Expected: `test cases: 183 | 183 passed` (1550 asserções).

- [ ] **Step 5: Commit**

```bash
git add frontend/voltz/core/text-layout.hpp frontend/voltz/core/text-layout.cpp frontend/voltz/tests/test-text-layout.cpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): layout do texto das anotacoes no nucleo (metricas por interface, linhas, caixa e cursor)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 3: Núcleo — fonte embutida (FreeType + DM Sans) e rasterização do texto

**Files:**
- Create: `frontend/voltz/core/embed-fonts.cmake`, `frontend/voltz/core/annotation-font.hpp`,
  `frontend/voltz/core/annotation-font.cpp`, `frontend/voltz/tests/test-annotation-font.cpp`
- Modify: `frontend/voltz/core/annotation-raster.hpp`, `frontend/voltz/core/annotation-raster.cpp`,
  `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`, `frontend/cmake/feature-voltz.cmake`

As duas DM Sans da casca viram um `.cpp` gerado no `build_x64` (bytes da fonte; nada versionado além do script) e são
abertas pelo FreeType das deps (`FT_New_Memory_Face`), uma vez por binário, sob um mutex: o `obs-studio` (sobreposição)
e o `voltz-sources` (fonte Voltz Anotações, na thread gráfica enquanto a UI chama `voltz_annot_begin`) têm cada um a
sua cópia, com os mesmos bytes e o mesmo código. Sem hinting: o glifo só depende do tamanho em pixels e da fração da
posição. O texto entra no rasterizador do 4B/4C pelos mesmos pontos (`strokeBounds`, `rasterizeStrokeClipped`,
`redrawRegion`): a sobreposição e a fonte do plugin desenham texto sem mudar uma linha.

- [ ] **Step 1: Escrever o teste que falha**

Criar `frontend/voltz/tests/test-annotation-font.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/annotation-font.hpp>
#include <voltz/core/annotation-raster.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using voltz::AnnotationTool;
using voltz::PixelRect;
using voltz::RgbColor;
using voltz::Stroke;
using voltz::StrokePoint;
using voltz::TextFont;

namespace {

const RgbColor kLime{0xD4, 0xF5, 0x3C};
const RgbColor kBlue{0x4D, 0xA3, 0xFF};

struct Image {
	Image(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0) {}
	voltz::RasterImage view() { return voltz::RasterImage{pixels.data(), width, height, width * 4}; }
	uint8_t alpha(int x, int y) const
	{
		return pixels[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4 + 3];
	}
	int inked() const
	{
		int count = 0;
		for (size_t i = 3; i < pixels.size(); i += 4) {
			count += pixels[i] != 0 ? 1 : 0;
		}
		return count;
	}
	int width;
	int height;
	std::vector<uint8_t> pixels;
};

Stroke makeText(int id, const std::string &text, StrokePoint anchor, int size, TextFont font = TextFont::Bold,
		RgbColor color = kLime)
{
	Stroke stroke;
	stroke.id = id;
	stroke.tool = AnnotationTool::Text;
	stroke.color = color;
	stroke.points = {anchor};
	stroke.text = text;
	stroke.textSize = size;
	stroke.textFont = font;
	return stroke;
}

void redrawAll(Image &image, const std::vector<const Stroke *> &strokes, double scale)
{
	voltz::clearRaster(image.view());
	for (const Stroke *stroke : strokes) {
		voltz::rasterizeStroke(image.view(), *stroke, scale, scale);
	}
}

} // namespace

TEST_CASE("fontes embutidas: as duas abrem; medidas da DM Sans")
{
	for (const TextFont font : {TextFont::Regular, TextFont::Bold}) {
		REQUIRE(voltz::embeddedFontAvailable(font));
		const voltz::GlyphMetrics &metrics = voltz::embeddedFontMetrics(font);
		CHECK(metrics.unitsPerEm() == 1000);
		CHECK(metrics.ascender() > 700);
		CHECK(metrics.descender() < -150);
		CHECK(metrics.lineHeight() >= metrics.ascender() - metrics.descender());
		CHECK(metrics.advance(U'W') > metrics.advance(U'i'));
		CHECK(metrics.advance(U'\u00E1') == metrics.advance(U'a')); // "á" existe e avança como "a"
	}
	CHECK(voltz::embeddedFontMetrics(TextFont::Bold).advance(U'a') >
	      voltz::embeddedFontMetrics(TextFont::Regular).advance(U'a'));
}

TEST_CASE("renderGlyph: 'A' tem tinta acima da linha de base; espaço não desenha; a fração desloca")
{
	voltz::GlyphBitmap glyph;
	REQUIRE(voltz::renderGlyph(TextFont::Bold, U'A', 40.0, 100.0, 200.0, glyph));
	CHECK(glyph.width > 10);
	CHECK(glyph.height > 20);
	CHECK(glyph.top >= 200 - 40);
	CHECK(glyph.top + glyph.height <= 201);
	CHECK(glyph.left >= 99);
	int full = 0;
	for (const uint8_t value : glyph.coverage) {
		full += value == 255 ? 1 : 0;
	}
	CHECK(full > 50);
	CHECK_FALSE(voltz::renderGlyph(TextFont::Bold, U' ', 40.0, 100.0, 200.0, glyph));
	voltz::GlyphBitmap whole;
	voltz::GlyphBitmap half;
	REQUIRE(voltz::renderGlyph(TextFont::Regular, U'l', 40.0, 10.0, 50.0, whole));
	REQUIRE(voltz::renderGlyph(TextFont::Regular, U'l', 40.0, 10.5, 50.0, half));
	CHECK(whole.coverage != half.coverage); // meio pixel para a direita muda a suavização
	CHECK_FALSE(voltz::renderGlyph(TextFont::Bold, U'A', 0.0, 0.0, 0.0, glyph));
}

TEST_CASE("texto: strokeBounds cobre todo pixel desenhado (escala 1 e 0,25); texto vazio não desenha")
{
	const Stroke text = makeText(1, "Ola\xC3\xA1 gj\nQ\xC3\x87y", {40, 30}, 64);
	for (const double scale : {1.0, 0.25}) {
		Image image(800, 400);
		voltz::rasterizeStroke(image.view(), text, scale, scale);
		const PixelRect bounds = voltz::strokeBounds(text, scale, scale);
		REQUIRE_FALSE(bounds.empty());
		int outside = 0;
		for (int y = 0; y < image.height; y++) {
			for (int x = 0; x < image.width; x++) {
				if (image.alpha(x, y) != 0 &&
				    !(x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)) {
					outside++;
				}
			}
		}
		CHECK(outside == 0);
		CHECK(image.inked() > (scale == 1.0 ? 2000 : 100));
	}
	const Stroke empty = makeText(2, "", {10, 10}, 48);
	CHECK(voltz::strokeBounds(empty, 1.0, 1.0).empty());
	Image blank(100, 100);
	voltz::rasterizeStroke(blank.view(), empty, 1.0, 1.0);
	CHECK(blank.inked() == 0);
}

TEST_CASE("texto: rasterizeStrokeClipped e redrawRegion iguais ao desenho inteiro (byte a byte)")
{
	const Stroke text = makeText(1, "Voltz\nStudio", {100, 80}, 72);
	Stroke pen;
	pen.id = 2;
	pen.tool = AnnotationTool::Pen;
	pen.color = kBlue;
	pen.width = 8;
	voltz::appendStrokePoints(pen, {{60, 60}, {400, 300}, {420, 90}});
	const std::vector<const Stroke *> strokes{&text, &pen};

	Image whole(640, 360);
	redrawAll(whole, strokes, 1.0);
	REQUIRE(whole.inked() > 0);

	/* recorte: fora dele intacto, dentro igual */
	Image clipped(640, 360);
	const PixelRect clip{150, 100, 300, 200};
	voltz::rasterizeStrokeClipped(clipped.view(), text, 1.0, 1.0, clip);
	Image textOnly(640, 360);
	voltz::rasterizeStroke(textOnly.view(), text, 1.0, 1.0);
	for (int y = 0; y < 360; y++) {
		for (int x = 0; x < 640; x++) {
			const bool in = x >= clip.left && x < clip.right && y >= clip.top && y < clip.bottom;
			if (!(in ? clipped.alpha(x, y) == textOnly.alpha(x, y) : clipped.alpha(x, y) == 0)) {
				FAIL("recorte diferente em (" << x << "," << y << ")");
			}
		}
	}

	/* o texto mudou (digitação): redesenhar a região antiga ∪ nova = redesenhar tudo */
	Stroke typed = text;
	typed.text = "Voltz!\nStudio 4D";
	Image partial = whole;
	const PixelRect region =
		voltz::uniteRects(voltz::strokeBounds(text, 1.0, 1.0), voltz::strokeBounds(typed, 1.0, 1.0));
	voltz::redrawRegion(partial.view(), {&typed, &pen}, 1.0, 1.0, region);
	Image expected(640, 360);
	redrawAll(expected, {&typed, &pen}, 1.0);
	CHECK(partial.pixels == expected.pixels);
}

TEST_CASE("texto: miolo da letra opaco na cor; negrito tem mais tinta que regular")
{
	Image regular(400, 120);
	Image bold(400, 120);
	voltz::rasterizeStroke(regular.view(), makeText(1, "HHHH", {10, 10}, 80, TextFont::Regular), 1.0, 1.0);
	voltz::rasterizeStroke(bold.view(), makeText(1, "HHHH", {10, 10}, 80, TextFont::Bold), 1.0, 1.0);
	CHECK(bold.inked() > regular.inked());
	int opaque = 0;
	int wrongColor = 0;
	for (size_t i = 0; i < bold.pixels.size(); i += 4) {
		if (bold.pixels[i + 3] == 255) {
			opaque++;
			wrongColor += (bold.pixels[i] != kLime.b || bold.pixels[i + 1] != kLime.g ||
				       bold.pixels[i + 2] != kLime.r)
					      ? 1
					      : 0;
		}
	}
	CHECK(opaque > 500);
	CHECK(wrongColor == 0);
}

TEST_CASE("textCaretStroke: no fim da última linha, na cor do texto; vazio na âncora")
{
	const Stroke text = makeText(5, "ab\ncd", {100, 50}, 48, TextFont::Bold, kBlue);
	const Stroke caret = voltz::textCaretStroke(text);
	CHECK(caret.tool == AnnotationTool::Pen);
	CHECK(caret.color == kBlue);
	CHECK(caret.width == 3);
	REQUIRE(caret.points.size() == 2);
	CHECK(caret.points[0].x == caret.points[1].x);
	CHECK(caret.points[0].x > 100 + 30); // depois de "cd"
	CHECK(caret.points[0].y > 50 + 48);  // segunda linha
	CHECK(caret.points[1].y > caret.points[0].y + 30);

	const Stroke empty = voltz::textCaretStroke(makeText(6, "", {10, 20}, 48));
	REQUIRE(empty.points.size() == 2);
	CHECK(empty.points[0].x <= 12);
	CHECK(empty.points[0].y == 20);
	Stroke pen;
	CHECK(voltz::textCaretStroke(pen).points.empty());
}
```

- [ ] **Step 2: FreeType no CMake, fontes embutidas e ver falhar**

Criar `frontend/voltz/core/embed-fonts.cmake`:
```cmake
# Gera o .cpp com os bytes das duas fontes do texto das anotações (DM Sans Regular e Bold, OFL), embutidas no voltz-core:
# a sobreposição (obs-studio) e a fonte Voltz Anotações (plugin) desenham o texto com os mesmos bytes.
# Uso (add_custom_command do CMakeLists.txt): cmake -DOUTPUT=<.cpp> -DREGULAR=<.ttf> -DBOLD=<.ttf> -P embed-fonts.cmake
cmake_minimum_required(VERSION 3.28)

foreach(required IN ITEMS OUTPUT REGULAR BOLD)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "embed-fonts.cmake: falta -D${required}=...")
  endif()
endforeach()

function(voltz_embed_font path symbol out_var)
  file(READ "${path}" hex HEX)
  string(LENGTH "${hex}" length)
  if(length LESS 1024)
    message(FATAL_ERROR "embed-fonts.cmake: fonte vazia ou pequena demais: ${path}")
  endif()
  # 16 bytes por linha
  string(REPEAT "[0-9a-f]" 32 chunk)
  string(REGEX REPLACE "(${chunk})" "\\1\n" hex "${hex}")
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
  set(
    ${out_var}
    "extern const unsigned char ${symbol}[];\nconst unsigned char ${symbol}[] = {\n${bytes}\n};\nextern const size_t ${symbol}Size;\nconst size_t ${symbol}Size = sizeof(${symbol});\n"
    PARENT_SCOPE
  )
endfunction()

voltz_embed_font("${REGULAR}" kDmSansRegular regular)
voltz_embed_font("${BOLD}" kDmSansBold bold)

file(
  WRITE "${OUTPUT}.tmp"
  "// Gerado pelo CMake (frontend/voltz/core/embed-fonts.cmake) a partir de frontend/voltz/shell/fonts. NAO editar.\n#include <cstddef>\n\nnamespace voltz::fontdata {\n\n${regular}\n${bold}\n} // namespace voltz::fontdata\n"
)
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
```

Em `frontend/cmake/feature-voltz.cmake` (o `find_package` fica aqui, antes do núcleo: o alvo importado só é visível no
diretório dele e abaixo):
trocar:
```cmake
set(VOLTZ_VERSION "${VOLTZ_VERSION_MAJOR}.${VOLTZ_VERSION_MINOR}.${VOLTZ_VERSION_PATCH}")

add_subdirectory(voltz/core)
```
por:
```cmake
set(VOLTZ_VERSION "${VOLTZ_VERSION_MAJOR}.${VOLTZ_VERSION_MINOR}.${VOLTZ_VERSION_PATCH}")

# FreeType das deps do OBS para o texto das anotações (voltz-core, Plano 4D). Aqui, e não no voltz-core: o alvo importado
# Freetype::Freetype só é visível no diretório em que nasce e abaixo dele — daqui valem o obs-studio, o voltz-core, os
# testes e o plugin voltz-sources.
find_package(Freetype 2.13 REQUIRED)

add_subdirectory(voltz/core)
```

Em `frontend/voltz/core/CMakeLists.txt`:
1. trocar:
```cmake
  PRIVATE
    annotation-journal.cpp
```
por:
```cmake
  PRIVATE
    annotation-font.cpp
    annotation-journal.cpp
```
2. trocar:
```cmake
  PUBLIC
    annotation-journal.hpp
```
por:
```cmake
  PUBLIC
    annotation-font.hpp
    annotation-journal.hpp
```
3. trocar:
```cmake
)

# Inclui a partir de frontend/ para usar <voltz/core/...>
```
por:
```cmake
)

# Texto das anotações (Plano 4D): as duas DM Sans (Regular e Bold, OFL) da casca embutidas no voltz-core — a sobreposição
# (obs-studio) e a fonte Voltz Anotações (plugin) desenham o texto com os mesmos bytes — e lidas pelo FreeType das deps
# do OBS (o mesmo do text-freetype2; o find_package fica no feature-voltz.cmake, visível para todos os alvos que ligam o
# voltz-core). Ligado PUBLIC: o find_dependencies do OBS aceita o alvo Freetype::Freetype na interface (um PRIVATE
# viraria $<LINK_ONLY:...>, que ele recusa — ver o nlohmann abaixo).
set(_voltz_fonts_dir "${CMAKE_CURRENT_SOURCE_DIR}/../shell/fonts")
set(_voltz_fonts_cpp "${CMAKE_CURRENT_BINARY_DIR}/annotation-font-data.cpp")
add_custom_command(
  OUTPUT "${_voltz_fonts_cpp}"
  COMMAND
    "${CMAKE_COMMAND}" "-DOUTPUT=${_voltz_fonts_cpp}" "-DREGULAR=${_voltz_fonts_dir}/DMSans-Regular.ttf"
    "-DBOLD=${_voltz_fonts_dir}/DMSans-Bold.ttf" -P "${CMAKE_CURRENT_SOURCE_DIR}/embed-fonts.cmake"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/embed-fonts.cmake"
    "${_voltz_fonts_dir}/DMSans-Regular.ttf"
    "${_voltz_fonts_dir}/DMSans-Bold.ttf"
  COMMENT "Voltz: embutindo as fontes do texto das anotacoes"
  VERBATIM
)
target_sources(voltz-core PRIVATE "${_voltz_fonts_cpp}")
target_link_libraries(voltz-core PUBLIC Freetype::Freetype)

# Inclui a partir de frontend/ para usar <voltz/core/...>
```

Em `frontend/voltz/tests/CMakeLists.txt`:
trocar:
```cmake
    test-annotation-dirty.cpp
    test-annotation-raster.cpp
```
por:
```cmake
    test-annotation-dirty.cpp
    test-annotation-font.cpp
    test-annotation-raster.cpp
```

Run: CONFIGURAR; COMPILAR TESTES.
Expected: o CONFIGURAR passa, sem erro do `find_dependencies` do OBS (`... is an unsupported generator expression ...`
seria o sinal de um `PRIVATE`/`$<LINK_ONLY:>` — não pode aparecer); o build roda o passo `Voltz: embutindo as fontes do
texto das anotacoes`, gera `build_x64/frontend/voltz/core/annotation-font-data.cpp` (~580 KB) e FALHA
(`annotation-font.hpp` não existe).

- [ ] **Step 3: Fonte embutida**

Criar `frontend/voltz/core/annotation-font.hpp`:
```cpp
#pragma once

#include "annotation-model.hpp"
#include "text-layout.hpp"

#include <cstdint>
#include <vector>

namespace voltz {

// As duas fontes do texto das anotações (DM Sans Regular e Bold, OFL) embutidas no voltz-core e lidas pelo FreeType
// (o mesmo das deps do OBS). Cada binário que liga o voltz-core (obs-studio e o plugin voltz-sources) tem a sua cópia:
// a sobreposição e a fonte Voltz Anotações desenham o texto com o mesmo código e os mesmos bytes. Sem hinting: o
// desenho só depende do tamanho em pixels, e o layout é proporcional ao tamanho. Thread-safe (um mutex por binário: no
// plugin a thread gráfica desenha enquanto a UI calcula retângulos).

// Falso se a fonte não abriu (aí o texto não é desenhado e os retângulos dele ficam vazios; quem usa avisa no log).
bool embeddedFontAvailable(TextFont font);
// Medidas da fonte para o layoutText (uma fonte que não abriu devolve unitsPerEm 0: layout vazio).
const GlyphMetrics &embeddedFontMetrics(TextFont font);

// Cobertura (0..255) de um caractere, pixels inteiros da imagem.
struct GlyphBitmap {
	int left = 0; // coluna do primeiro pixel
	int top = 0;  // linha do primeiro pixel
	int width = 0;
	int height = 0;
	std::vector<uint8_t> coverage; // width × height, de cima para baixo
};

// Rasteriza `codepoint` com o "em" de `pixelSize` pixels e a origem em (originX, baseline) — pixels da imagem, frações
// valem (o contorno é deslocado antes de rasterizar). Caractere sem desenho (espaço) ou fonte que não abriu → falso.
// Caractere que a fonte não tem: o glifo .notdef (um retângulo).
bool renderGlyph(TextFont font, char32_t codepoint, double pixelSize, double originX, double baseline,
		 GlyphBitmap &out);

// Cursor de digitação de um traço de texto (só a sobreposição o desenha): um traço de caneta vertical no fim do texto,
// na cor dele, em pixels da captura (espessura max(2, tam/16)). Sem pontos se `text` não é um traço de texto.
Stroke textCaretStroke(const Stroke &text);

} // namespace voltz
```

Criar `frontend/voltz/core/annotation-font.cpp`:
```cpp
#include "annotation-font.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_OUTLINE_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <mutex>

namespace voltz {

namespace fontdata {
// annotation-font-data.cpp, gerado pelo embed-fonts.cmake
extern const unsigned char kDmSansRegular[];
extern const size_t kDmSansRegularSize;
extern const unsigned char kDmSansBold[];
extern const size_t kDmSansBoldSize;
} // namespace fontdata

namespace {

constexpr double kMaxGlyphPixelSize = 2000.0;
constexpr double kMaxGlyphOrigin = 100000000.0;

int fontIndex(TextFont font)
{
	return font == TextFont::Regular ? 0 : 1;
}

class EmbeddedFonts;

// Medidas de uma das fontes: lidas uma vez ao abrir; o avanço pergunta à fonte (com o mutex).
class FaceMetrics : public GlyphMetrics {
public:
	int unitsPerEm() const override { return units_; }
	int ascender() const override { return ascender_; }
	int descender() const override { return descender_; }
	int lineHeight() const override { return lineHeight_; }
	int advance(char32_t codepoint) const override;

	void set(EmbeddedFonts *owner, int index, int units, int ascender, int descender, int lineHeight)
	{
		owner_ = owner;
		index_ = index;
		units_ = units;
		ascender_ = ascender;
		descender_ = descender;
		lineHeight_ = lineHeight;
	}

private:
	EmbeddedFonts *owner_ = nullptr;
	int index_ = 0;
	int units_ = 0; // 0 = a fonte não abriu: layout vazio
	int ascender_ = 0;
	int descender_ = 0;
	int lineHeight_ = 0;
};

// Biblioteca FreeType e as duas fontes, abertas na primeira vez que alguém precisa (estático da função: thread-safe) e
// fechadas na saída do binário.
class EmbeddedFonts {
public:
	EmbeddedFonts()
	{
		if (FT_Init_FreeType(&library_) != 0) {
			library_ = nullptr;
			return;
		}
		open(0, fontdata::kDmSansRegular, fontdata::kDmSansRegularSize);
		open(1, fontdata::kDmSansBold, fontdata::kDmSansBoldSize);
	}

	~EmbeddedFonts()
	{
		for (FT_Face &face : faces_) {
			if (face) {
				FT_Done_Face(face);
				face = nullptr;
			}
		}
		if (library_) {
			FT_Done_FreeType(library_);
		}
	}

	EmbeddedFonts(const EmbeddedFonts &) = delete;
	EmbeddedFonts &operator=(const EmbeddedFonts &) = delete;

	bool available(int index) const { return faces_[index] != nullptr; }
	const FaceMetrics &metrics(int index) const { return metrics_[index]; }

	int advance(int index, char32_t codepoint)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		FT_Face face = faces_[index];
		if (!face) {
			return 0;
		}
		FT_Fixed advance = 0;
		const FT_UInt glyph = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
		if (FT_Get_Advance(face, glyph, FT_LOAD_NO_SCALE, &advance) != 0) {
			return 0;
		}
		return static_cast<int>(advance);
	}

	bool render(int index, char32_t codepoint, double pixelSize, double originX, double baseline, GlyphBitmap &out)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		FT_Face face = faces_[index];
		if (!face || !(pixelSize > 0.0) || !std::isfinite(originX) || !std::isfinite(baseline)) {
			return false;
		}
		const double size = std::min(pixelSize, kMaxGlyphPixelSize);
		const auto charSize = static_cast<FT_F26Dot6>(std::max(1L, std::lround(size * 64.0)));
		if (FT_Set_Char_Size(face, 0, charSize, 72, 72) != 0) {
			return false;
		}
		const FT_UInt glyph = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
		if (FT_Load_Glyph(face, glyph, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0) {
			return false;
		}
		FT_GlyphSlot slot = face->glyph;
		if (slot->format != FT_GLYPH_FORMAT_OUTLINE || slot->outline.n_points == 0) {
			return false; /* espaço e afins: nada a desenhar */
		}
		/* a parte inteira vai para a posição do bitmap; a fração desloca o contorno (y do FreeType cresce para cima) */
		const double baseX = std::floor(std::clamp(originX, -kMaxGlyphOrigin, kMaxGlyphOrigin));
		const double baseY = std::floor(std::clamp(baseline, -kMaxGlyphOrigin, kMaxGlyphOrigin));
		const auto shiftX = static_cast<FT_Pos>(std::lround((originX - baseX) * 64.0));
		const auto shiftY = static_cast<FT_Pos>(std::lround((baseline - baseY) * 64.0));
		FT_Outline_Translate(&slot->outline, shiftX, -shiftY);
		if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) {
			return false;
		}
		const FT_Bitmap &bitmap = slot->bitmap;
		if (bitmap.width == 0 || bitmap.rows == 0 || bitmap.pixel_mode != FT_PIXEL_MODE_GRAY ||
		    bitmap.pitch < 0) {
			return false;
		}
		out.left = static_cast<int>(baseX) + slot->bitmap_left;
		out.top = static_cast<int>(baseY) - slot->bitmap_top;
		out.width = static_cast<int>(bitmap.width);
		out.height = static_cast<int>(bitmap.rows);
		out.coverage.resize(static_cast<size_t>(out.width) * static_cast<size_t>(out.height));
		for (int y = 0; y < out.height; y++) {
			std::memcpy(out.coverage.data() + static_cast<size_t>(y) * static_cast<size_t>(out.width),
				    bitmap.buffer + static_cast<size_t>(y) * static_cast<size_t>(bitmap.pitch),
				    static_cast<size_t>(out.width));
		}
		return true;
	}

private:
	void open(int index, const unsigned char *data, size_t size)
	{
		FT_Face face = nullptr;
		if (FT_New_Memory_Face(library_, data, static_cast<FT_Long>(size), 0, &face) != 0) {
			return;
		}
		if (!FT_IS_SCALABLE(face) || face->units_per_EM == 0) {
			FT_Done_Face(face);
			return;
		}
		faces_[index] = face;
		metrics_[index].set(this, index, face->units_per_EM, face->ascender, face->descender, face->height);
	}

	std::mutex mutex_;
	FT_Library library_ = nullptr;
	FT_Face faces_[2] = {nullptr, nullptr};
	FaceMetrics metrics_[2];
};

EmbeddedFonts &embeddedFonts()
{
	static EmbeddedFonts fonts;
	return fonts;
}

int FaceMetrics::advance(char32_t codepoint) const
{
	return owner_ ? owner_->advance(index_, codepoint) : 0;
}

} // namespace

bool embeddedFontAvailable(TextFont font)
{
	return embeddedFonts().available(fontIndex(font));
}

const GlyphMetrics &embeddedFontMetrics(TextFont font)
{
	return embeddedFonts().metrics(fontIndex(font));
}

bool renderGlyph(TextFont font, char32_t codepoint, double pixelSize, double originX, double baseline, GlyphBitmap &out)
{
	return embeddedFonts().render(fontIndex(font), codepoint, pixelSize, originX, baseline, out);
}

Stroke textCaretStroke(const Stroke &text)
{
	Stroke caret;
	caret.id = text.id;
	caret.tool = AnnotationTool::Pen;
	caret.color = text.color;
	if (text.tool != AnnotationTool::Text || text.points.empty()) {
		return caret;
	}
	const int size = clampTextSize(text.textSize);
	const TextLayout layout = layoutText(text.text, static_cast<double>(size), embeddedFontMetrics(text.textFont));
	if (layout.pixelSize <= 0.0) {
		return caret;
	}
	caret.width = clampStrokeWidth(std::max(2, size / 16));
	const StrokePoint anchor = text.points.front();
	const int x = anchor.x + static_cast<int>(std::lround(layout.caretX)) + caret.width / 2;
	const int top = anchor.y + static_cast<int>(std::lround(layout.caretTop));
	const int bottom = anchor.y + static_cast<int>(std::lround(layout.caretBottom)) - 1;
	caret.points = {StrokePoint{x, top}, StrokePoint{x, std::max(top, bottom)}};
	return caret;
}

} // namespace voltz
```

- [ ] **Step 4: O texto no rasterizador**

Em `frontend/voltz/core/annotation-raster.hpp` (só comentários):
1. trocar:
```cpp
// seta: haste + ponta cheia (comprimento max(12, 4×esp), limitado ao tamanho da seta; largura = comprimento);
// retângulo: 4 lados; elipse: polígono fechado inscrito no retângulo primeiro/último (24 a 180 lados, ~8 px por lado).
StrokeShape strokeShape(const Stroke &stroke, double scaleX, double scaleY);
```
por:
```cpp
// seta: haste + ponta cheia (comprimento max(12, 4×esp), limitado ao tamanho da seta; largura = comprimento);
// retângulo: 4 lados; elipse: polígono fechado inscrito no retângulo primeiro/último (24 a 180 lados, ~8 px por lado);
// texto: nenhuma peça (os glifos vêm da fonte embutida, annotation-font).
StrokeShape strokeShape(const Stroke &stroke, double scaleX, double scaleY);
```
2. trocar:
```cpp
// mistura: o marca-texto translúcido não escurece nas junções. Bordas suavizadas em ~1 px; o que sai da imagem é cortado.
void rasterizeStroke(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY);
```
por:
```cpp
// mistura: o marca-texto translúcido não escurece nas junções. Bordas suavizadas em ~1 px; o que sai da imagem é cortado.
// Texto (Plano 4D): os glifos da DM Sans embutida (FreeType, sem hinting) com a âncora × escala e o "em" × a escala
// média, opacos na cor do traço; o mesmo código na sobreposição e na fonte do plugin.
void rasterizeStroke(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY);
```
3. trocar:
```cpp
// Todo pixel que rasterizeStroke pode tocar (pixels da imagem, com a escala dada), sem cortar na imagem. Vazio se o
// traço não desenha nada.
PixelRect strokeBounds(const Stroke &stroke, double scaleX, double scaleY);
```
por:
```cpp
// Todo pixel que rasterizeStroke pode tocar (pixels da imagem, com a escala dada), sem cortar na imagem. Vazio se o
// traço não desenha nada. Texto: a caixa do layout com folga de 0,3 "em" + 2 px (o texto é cortado nela).
PixelRect strokeBounds(const Stroke &stroke, double scaleX, double scaleY);
```

Em `frontend/voltz/core/annotation-raster.cpp` — a mistura da máscara sai do `rasterizeShapeClipped` para um
`blendMask` comum (mesmo resultado, byte a byte) e o texto ganha `textPlacement`, `textBounds` e
`rasterizeTextClipped`, usados por `strokeBounds`, `rasterizeStrokeClipped` e `redrawRegion`:
1. trocar:
```cpp
#include "annotation-raster.hpp"

#include <algorithm>
```
por:
```cpp
#include "annotation-raster.hpp"
#include "annotation-font.hpp"
#include "text-layout.hpp"

#include <algorithm>
```
2. trocar:
```cpp
constexpr double kArrowHeadPerWidth = 4.0;

struct Point {
```
por:
```cpp
constexpr double kArrowHeadPerWidth = 4.0;
// Folga do retângulo do texto em relação à caixa do layout, em "em" (+2 px): acentos acima do ascender, caudas abaixo do
// descender, letras que passam do avanço e a suavização.
constexpr double kTextInkMargin = 0.3;

struct Point {
```
3. trocar:
```cpp
		break;
	}
	}
	return shape;
```
por:
```cpp
		break;
	}
	case AnnotationTool::Text:
		break; /* o texto não é feito de peças: rasterizeTextClipped */
	}
	return shape;
```
4. trocar:
```cpp
			 boundCoordinate(std::ceil(maxX) + 1.0), boundCoordinate(std::ceil(maxY) + 1.0)};
}
```
por:
```cpp
			 boundCoordinate(std::ceil(maxX) + 1.0), boundCoordinate(std::ceil(maxY) + 1.0)};
}

// Mistura (src-over, pré-multiplicada) a máscara de cobertura (box, linha a linha) na imagem, na cor e opacidade dadas.
void blendMask(const RasterImage &image, const PixelRect &box, const std::vector<uint8_t> &mask, const RgbColor &color,
	       double opacity)
{
	const int maskWidth = box.right - box.left;
	const int blue = color.b;
	const int green = color.g;
	const int red = color.r;
	for (int y = box.top; y < box.bottom; y++) {
		uint8_t *row = image.pixels + static_cast<size_t>(y) * static_cast<size_t>(image.stride);
		const uint8_t *maskRow =
			mask.data() + static_cast<size_t>(y - box.top) * static_cast<size_t>(maskWidth);
		for (int x = box.left; x < box.right; x++) {
			const int value = maskRow[x - box.left];
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
```
5. trocar:
```cpp
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
```
por:
```cpp
	}

	blendMask(image, box, mask, stroke.color, strokeOpacity(stroke));
}

// Texto: a âncora (canto do pixel) vai com a escala de cada eixo; o "em" escala pela média, como a espessura dos traços.
bool textPlacement(const Stroke &stroke, double scaleX, double scaleY, double &anchorX, double &anchorY,
		   double &pixelSize)
{
	if (stroke.tool != AnnotationTool::Text || stroke.points.empty() || stroke.text.empty() || scaleX <= 0.0 ||
	    scaleY <= 0.0) {
		return false;
	}
	anchorX = static_cast<double>(stroke.points.front().x) * scaleX;
	anchorY = static_cast<double>(stroke.points.front().y) * scaleY;
	pixelSize = static_cast<double>(clampTextSize(stroke.textSize)) * (scaleX + scaleY) / 2.0;
	return true;
}

// A caixa do layout com uma folga para o que a letra desenha fora dela (acentos, caudas, suavização). O texto nunca
// desenha fora disso (rasterizeTextClipped corta aqui): é o strokeBounds do texto.
PixelRect textBounds(const Stroke &stroke, double scaleX, double scaleY)
{
	double anchorX = 0.0;
	double anchorY = 0.0;
	double pixelSize = 0.0;
	if (!textPlacement(stroke, scaleX, scaleY, anchorX, anchorY, pixelSize)) {
		return PixelRect{};
	}
	const TextLayout layout = layoutText(stroke.text, pixelSize, embeddedFontMetrics(stroke.textFont));
	if (layout.glyphs.empty()) {
		return PixelRect{};
	}
	const double margin = std::ceil(pixelSize * kTextInkMargin) + 2.0;
	return PixelRect{boundCoordinate(std::floor(anchorX - margin)), boundCoordinate(std::floor(anchorY - margin)),
			 boundCoordinate(std::ceil(anchorX + layout.width + margin)),
			 boundCoordinate(std::ceil(anchorY + layout.height + margin))};
}

// Os glifos do texto (FreeType, annotation-font) numa máscara com a maior cobertura de cada pixel, limitada a `clip`,
// à imagem e ao textBounds; mistura como os outros traços. A cobertura de um pixel não depende do recorte.
void rasterizeTextClipped(const RasterImage &image, const Stroke &stroke, double scaleX, double scaleY,
			  const PixelRect &clip)
{
	double anchorX = 0.0;
	double anchorY = 0.0;
	double pixelSize = 0.0;
	if (!textPlacement(stroke, scaleX, scaleY, anchorX, anchorY, pixelSize)) {
		return;
	}
	const PixelRect box = intersectRects(intersectRects(textBounds(stroke, scaleX, scaleY), clip),
					     PixelRect{0, 0, image.width, image.height});
	if (box.empty()) {
		return;
	}
	const TextLayout layout = layoutText(stroke.text, pixelSize, embeddedFontMetrics(stroke.textFont));
	const int maskWidth = box.right - box.left;
	std::vector<uint8_t> mask(static_cast<size_t>(maskWidth) * static_cast<size_t>(box.bottom - box.top), 0);
	GlyphBitmap glyph;
	for (const PlacedGlyph &placed : layout.glyphs) {
		const double originX = anchorX + placed.x;
		const double baseline = anchorY + placed.baseline;
		/* pula, sem rasterizar, a letra que nem com folga toca a caixa: só desempenho, o resultado é o mesmo */
		const PixelRect reach{boundCoordinate(std::floor(originX - pixelSize)),
				      boundCoordinate(std::floor(baseline - 1.5 * pixelSize)),
				      boundCoordinate(std::ceil(originX + 2.0 * pixelSize)),
				      boundCoordinate(std::ceil(baseline + pixelSize))};
		if (intersectRects(reach, box).empty()) {
			continue;
		}
		if (!renderGlyph(stroke.textFont, placed.codepoint, pixelSize, originX, baseline, glyph)) {
			continue;
		}
		const PixelRect area = intersectRects(
			PixelRect{glyph.left, glyph.top, glyph.left + glyph.width, glyph.top + glyph.height}, box);
		for (int y = area.top; y < area.bottom; y++) {
			const uint8_t *source = glyph.coverage.data() +
						static_cast<size_t>(y - glyph.top) * static_cast<size_t>(glyph.width);
			uint8_t *target =
				mask.data() + static_cast<size_t>(y - box.top) * static_cast<size_t>(maskWidth);
			for (int x = area.left; x < area.right; x++) {
				uint8_t &cell = target[x - box.left];
				cell = std::max(cell, source[x - glyph.left]);
			}
		}
	}
	blendMask(image, box, mask, stroke.color, 1.0);
}

} // namespace
```
6. trocar:
```cpp
{
	return shapeBounds(strokeShape(stroke, scaleX, scaleY));
```
por:
```cpp
{
	if (stroke.tool == AnnotationTool::Text) {
		return textBounds(stroke, scaleX, scaleY);
	}
	return shapeBounds(strokeShape(stroke, scaleX, scaleY));
```
7. trocar:
```cpp
			    const PixelRect &clip)
{
	if (!image.pixels || image.width <= 0 || image.height <= 0) {
		return;
	}
	rasterizeShapeClipped(image, stroke, strokeShape(stroke, scaleX, scaleY), clip);
```
por:
```cpp
			    const PixelRect &clip)
{
	if (!image.pixels || image.width <= 0 || image.height <= 0) {
		return;
	}
	if (stroke.tool == AnnotationTool::Text) {
		rasterizeTextClipped(image, stroke, scaleX, scaleY, clip);
		return;
	}
	rasterizeShapeClipped(image, stroke, strokeShape(stroke, scaleX, scaleY), clip);
```
8. trocar:
```cpp
		if (!stroke) {
			continue;
```
por:
```cpp
		if (!stroke) {
			continue;
		}
		if (stroke->tool == AnnotationTool::Text) {
			if (!intersectRects(textBounds(*stroke, scaleX, scaleY), area).empty()) {
				rasterizeTextClipped(image, *stroke, scaleX, scaleY, area);
			}
			continue;
```

- [ ] **Step 5: Rodar e ver passar**

```bash
"$CLANGFMT" -i frontend/voltz/core/annotation-font.hpp frontend/voltz/core/annotation-font.cpp frontend/voltz/core/annotation-raster.hpp frontend/voltz/core/annotation-raster.cpp frontend/voltz/tests/test-annotation-font.cpp
```
Run: COMPILAR TESTES; RODAR TESTES; `build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe | tail -3`.
Expected: `test cases: 189 | 189 passed` (1602 asserções; os 6 casos novos e os 6+8 do retângulo sujo e do rasterizador
do 4B/4C continuam passando: o `blendMask` não mudou nenhum pixel).

- [ ] **Step 6: O app e o plugin ligam com o FreeType**

Run: COMPILAR TUDO (segundo plano).
Expected: `exit=0`, nenhum `error`. Conferir o arquivo gerado e os dois binários religados:
```bash
grep -c "kDmSans" build_x64/frontend/voltz/core/annotation-font-data.cpp
ls -la build_x64/rundir/RelWithDebInfo/obs-plugins/64bit/voltz-sources.dll build_x64/rundir/RelWithDebInfo/bin/64bit/VoltzStudio.exe
ls build_x64/rundir/RelWithDebInfo/bin/64bit | grep -ci freetype
```
Expected: `8` (4 linhas por fonte), os dois binários com data de agora e maiores que antes (FreeType + as duas fontes,
~112 KB de fonte em cada) e `0` (FreeType estático: nenhum DLL novo).

- [ ] **Step 7: Commit**

```bash
git add frontend/voltz/core/embed-fonts.cmake frontend/voltz/core/annotation-font.hpp frontend/voltz/core/annotation-font.cpp frontend/voltz/core/annotation-raster.hpp frontend/voltz/core/annotation-raster.cpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/CMakeLists.txt frontend/voltz/tests/test-annotation-font.cpp frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): texto das anotacoes rasterizado no nucleo (FreeType das deps, DM Sans embutida, mesmo desenho na sobreposicao e no plugin)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 4: Núcleo — digitação na `DrawSession` (texto com as mesmas saídas garantidas)

**Files:**
- Create: `frontend/voltz/tests/test-draw-text.cpp`
- Modify: `frontend/voltz/core/draw-session.hpp`, `frontend/voltz/core/draw-session.cpp`,
  `frontend/voltz/tests/CMakeLists.txt`

A sessão ganha a digitação: com a ferramenta texto (`DrawInput::textTool` no clique), o clique dentro do monitor começa
um texto (`TextBegin` com a âncora em pixels da captura); `Char`, `Backspace` e `Enter{shift}` editam e cada edição manda
o texto **inteiro** (`TextEdit`); Enter confirma (`TextCommit`; vazio = `TextCancel`); Esc e Ctrl+Z cancelam só o texto
(o Esc seguinte sai do modo); clicar de novo confirma e começa outro texto — ou o traço, se a ferramenta mudou; **toda
saída confirma antes** (vazio = cancela) e o `Exit` continua o último comando. Digitar conta como atividade.

- [ ] **Step 1: Escrever o teste que falha**

Criar `frontend/voltz/tests/test-draw-text.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/annotation-text.hpp>
#include <voltz/core/draw-session.hpp>

#include <cstdint>
#include <string>
#include <vector>

using voltz::DrawCommand;
using voltz::DrawCommandType;
using voltz::DrawExit;
using voltz::DrawInput;
using voltz::DrawInputKind;
using voltz::DrawSession;
using voltz::StrokePoint;

namespace {

constexpr int64_t kMs = 1000000;
constexpr int64_t kStart = 1000 * kMs;
// Monitor 1000×500 em (100,50) capturado em 2000×1000 (o mesmo do test-draw-session.cpp).
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

DrawInput click(int64_t tNs, int x, int y, bool textTool)
{
	DrawInput input = at(DrawInputKind::LeftDown, tNs, x, y);
	input.textTool = textTool;
	return input;
}

DrawInput typed(char32_t codepoint, int64_t tNs)
{
	DrawInput input = at(DrawInputKind::Char, tNs);
	input.codepoint = codepoint;
	return input;
}

DrawInput enter(int64_t tNs, bool shift)
{
	DrawInput input = at(DrawInputKind::Enter, tNs);
	input.shift = shift;
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

// "textbegin(200,100) edit:'Oi' commit exit:esc"
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
		case DrawCommandType::TextBegin:
			text += "textbegin";
			break;
		case DrawCommandType::TextEdit:
			text += "edit:'" + command.text + "'";
			break;
		case DrawCommandType::TextCommit:
			text += "commit";
			break;
		case DrawCommandType::TextCancel:
			text += "cancel";
			break;
		}
		for (const StrokePoint &point : command.points) {
			text += "(" + std::to_string(point.x) + "," + std::to_string(point.y) + ")";
		}
	}
	return text;
}

// Começa a digitação em (200,100) da captura (= (200,100) do desktop: (100+50, 50+25)).
DrawSession typingSession()
{
	DrawSession session(kMapping, kStart);
	REQUIRE(describe(session.handle(click(kStart, 200, 100, true))) == "textbegin(200,100)");
	REQUIRE(session.typing());
	return session;
}

} // namespace

TEST_CASE("texto: clique com a ferramenta texto começa a digitação na âncora; cada caractere manda o texto inteiro")
{
	DrawSession session = typingSession();
	CHECK_FALSE(session.drawing());
	CHECK(describe(session.handle(typed(U'O', kStart + 10 * kMs))) == "edit:'O'");
	CHECK(describe(session.handle(typed(U'i', kStart + 20 * kMs))) == "edit:'Oi'");
	CHECK(describe(session.handle(typed(U'\u00E1', kStart + 30 * kMs))) == "edit:'Oi\xC3\xA1'");
	/* arrastar e soltar com a ferramenta texto não desenha nada */
	CHECK(session.handle(at(DrawInputKind::Move, kStart + 40 * kMs, 300, 200)).empty());
	CHECK(session.handle(at(DrawInputKind::LeftUp, kStart + 50 * kMs, 300, 200)).empty());
	CHECK(session.typing());
}

TEST_CASE("texto: Backspace, Shift+Enter (nova linha) e Enter confirma; texto vazio confirma como cancelado")
{
	DrawSession session = typingSession();
	session.handle(typed(U'a', kStart + kMs));
	session.handle(typed(U'b', kStart + 2 * kMs));
	CHECK(describe(session.handle(at(DrawInputKind::Backspace, kStart + 3 * kMs))) == "edit:'a'");
	CHECK(describe(session.handle(enter(kStart + 4 * kMs, true))) == "edit:'a\n'");
	CHECK(describe(session.handle(typed(U'c', kStart + 5 * kMs))) == "edit:'a\nc'");
	CHECK(describe(session.handle(enter(kStart + 6 * kMs, false))) == "commit");
	CHECK_FALSE(session.typing());
	CHECK(session.active());
	/* Enter, Backspace e caracteres sem digitação: nada */
	CHECK(session.handle(enter(kStart + 7 * kMs, false)).empty());
	CHECK(session.handle(typed(U'x', kStart + 8 * kMs)).empty());
	CHECK(session.handle(at(DrawInputKind::Backspace, kStart + 9 * kMs)).empty());

	DrawSession empty = typingSession();
	CHECK(empty.handle(at(DrawInputKind::Backspace, kStart + kMs)).empty()); // nada a apagar
	CHECK(describe(empty.handle(enter(kStart + 2 * kMs, false))) == "cancel");
}

TEST_CASE("texto: Esc cancela só a digitação (sem sair); o Esc seguinte sai")
{
	DrawSession session = typingSession();
	session.handle(typed(U'x', kStart + kMs));
	CHECK(describe(session.handle(at(DrawInputKind::Escape, kStart + 2 * kMs))) == "cancel");
	CHECK(session.active());
	CHECK_FALSE(session.typing());
	CHECK(describe(session.handle(at(DrawInputKind::Escape, kStart + 3 * kMs))) == "exit:esc");
	CHECK_FALSE(session.active());
}

TEST_CASE("texto: Ctrl+Z durante a digitação cancela só o texto; depois desfaz o último traço")
{
	DrawSession session = typingSession();
	session.handle(typed(U'x', kStart + kMs));
	CHECK(describe(session.handle(at(DrawInputKind::Undo, kStart + 2 * kMs))) == "cancel");
	CHECK(describe(session.handle(at(DrawInputKind::Undo, kStart + 3 * kMs))) == "undo");
}

TEST_CASE("texto: clicar de novo confirma e começa outro; com outra ferramenta confirma e começa o traço")
{
	DrawSession session = typingSession();
	session.handle(typed(U'A', kStart + kMs));
	CHECK(describe(session.handle(click(kStart + 2 * kMs, 600, 300, true))) == "commit textbegin(1000,500)");
	CHECK(describe(session.handle(click(kStart + 3 * kMs, 700, 300, true))) == "cancel textbegin(1200,500)");
	session.handle(typed(U'B', kStart + 4 * kMs));
	CHECK(describe(session.handle(click(kStart + 5 * kMs, 700, 300, false))) == "commit begin(1200,500)");
	CHECK(session.drawing());
	CHECK_FALSE(session.typing());
	/* com o botão ainda apertado, outro clique (com a ferramenta texto) não começa nada */
	CHECK(session.handle(click(kStart + 6 * kMs, 710, 300, true)).empty());
	/* clique fora do monitor com a ferramenta texto: nada (o texto em andamento continua) */
	DrawSession outside = typingSession();
	outside.handle(typed(U'z', kStart + kMs));
	CHECK(outside.handle(click(kStart + 2 * kMs, 50, 20, true)).empty());
	CHECK(outside.typing());
}

TEST_CASE("texto: toda saída confirma antes (ou cancela se vazio), e o Exit vem por último")
{
	const DrawInputKind exits[] = {DrawInputKind::RightDown, DrawInputKind::Deactivated, DrawInputKind::CaptureLost,
				       DrawInputKind::Close, DrawInputKind::TargetChanged};
	const char *names[] = {"clique_direito", "foco", "foco", "fechar", "alvo"};
	for (int i = 0; i < 5; i++) {
		DrawSession session = typingSession();
		session.handle(typed(U'k', kStart + kMs));
		CHECK(describe(session.handle(at(exits[i], kStart + 2 * kMs))) ==
		      std::string("commit exit:") + names[i]);
	}
	DrawSession empty = typingSession();
	CHECK(describe(empty.requestExit(DrawExit::Hotkey)) == "cancel exit:atalho");
	DrawSession watchdog = typingSession();
	watchdog.handle(typed(U'k', kStart + kMs));
	CHECK(describe(watchdog.handle(tick(kStart + 2000 * kMs, kStart))) == "commit exit:vigia");
}

TEST_CASE("texto: digitar conta como atividade (sem saída por inatividade); controles e limites não editam")
{
	DrawSession session = typingSession();
	int64_t t = kStart;
	for (int i = 0; i < 4; i++) {
		t += 20000 * kMs; // 20 s entre teclas
		CHECK(describe(session.handle(typed(U'a', t))).rfind("edit:", 0) == 0);
		CHECK(session.handle(tick(t + 100 * kMs, t + 100 * kMs)).empty());
	}
	CHECK(session.active());
	CHECK(session.handle(typed(U'\b', t + kMs)).empty());
	CHECK(session.handle(typed(char32_t(0x1B), t + kMs)).empty());
	/* as teclas recusadas também contam como atividade (t + 1 ms) */
	const int64_t idle = t + kMs + voltz::kDrawIdleNs;
	CHECK(describe(session.handle(tick(idle, idle))) == "commit exit:inatividade");

	DrawSession full = typingSession();
	int64_t now = kStart;
	for (size_t i = 0; i < voltz::kMaxTextChars; i++) {
		now += kMs;
		full.handle(typed(U'x', now));
	}
	CHECK(full.handle(typed(U'y', now + kMs)).empty());
	CHECK(describe(full.handle(at(DrawInputKind::Backspace, now + 2 * kMs))).size() ==
	      std::string("edit:''").size() + voltz::kMaxTextChars - 1);
}

TEST_CASE("texto: comandos de traço continuam iguais sem a ferramenta texto")
{
	DrawSession session(kMapping, kStart);
	CHECK(describe(session.handle(click(kStart, 200, 100, false))) == "begin(200,100)");
	CHECK(session.handle(typed(U'a', kStart + kMs)).empty()); // digitar durante o traço não faz nada
	CHECK(describe(session.handle(at(DrawInputKind::LeftUp, kStart + 2 * kMs, 210, 100))) == "extend(220,100) end");
	CHECK(describe(session.handle(at(DrawInputKind::Escape, kStart + 3 * kMs))) == "exit:esc");
}
```

- [ ] **Step 2: Ligar ao CMake e ver falhar**

Em `frontend/voltz/tests/CMakeLists.txt`:
trocar:
```cmake
    test-draw-session.cpp
    test-iso-encoders.cpp
```
por:
```cmake
    test-draw-session.cpp
    test-draw-text.cpp
    test-iso-encoders.cpp
```

Run: CONFIGURAR; COMPILAR TESTES.
Expected: FALHA (`DrawInputKind::Char`, `DrawCommandType::TextBegin`, `DrawInput::textTool` não existem).

- [ ] **Step 3: Cabeçalho**

Em `frontend/voltz/core/draw-session.hpp`:
1. trocar:
```cpp
#include <cstdint>
#include <vector>
```
por:
```cpp
#include <cstdint>
#include <string>
#include <vector>
```
2. trocar:
```cpp
	OtherKey,      // outra tecla (só conta como atividade)
	Activated,     // a janela de entrada ficou ativa
```
por:
```cpp
	OtherKey,      // outra tecla (só conta como atividade)
	Char,          // caractere digitado (codepoint; WM_CHAR, já com acentos e substitutos juntados)
	Backspace,     // apagar o último caractere
	Enter,         // Enter (shift = Shift+Enter: nova linha)
	Activated,     // a janela de entrada ficou ativa
```
3. trocar:
```cpp
	int64_t uiHeartbeatNs = 0; // Tick: último pulso da thread da UI (0 = nunca pulsou)
};

enum class DrawCommandType { Begin, Extend, End, Undo, Exit };

struct DrawCommand {
	DrawCommandType type = DrawCommandType::End;
	std::vector<StrokePoint> points; // Begin: 1 ponto; Extend: os pontos novos (pixels da captura)
	DrawExit exit = DrawExit::None;  // Exit: o motivo
};
```
por:
```cpp
	int64_t uiHeartbeatNs = 0; // Tick: último pulso da thread da UI (0 = nunca pulsou)
	bool textTool = false;     // LeftDown: a ferramenta escolhida é o texto (o clique começa a digitação)
	char32_t codepoint = 0;    // Char
	bool shift = false;        // Enter: Shift apertado
};

// Begin/Extend/End/Undo: traço; TextBegin (1 ponto: a âncora), TextEdit (o texto inteiro, UTF-8), TextCommit (o texto
// fica), TextCancel (o texto sai): digitação (Plano 4D); Exit: fim da sessão (sempre o último).
enum class DrawCommandType { Begin, Extend, End, Undo, Exit, TextBegin, TextEdit, TextCommit, TextCancel };

struct DrawCommand {
	DrawCommandType type = DrawCommandType::End;
	std::vector<StrokePoint> points; // Begin/TextBegin: 1 ponto; Extend: os pontos novos (pixels da captura)
	DrawExit exit = DrawExit::None;  // Exit: o motivo
	std::string text;                // TextEdit: o texto inteiro até agora
};
```
4. trocar:
```cpp
// Sem Qt nem Win32; o tempo vem em cada entrada (relógio injetável nos testes). Toda saída termina antes o traço em
// andamento; depois de sair, ignora tudo.
class DrawSession {
```
por:
```cpp
// Sem Qt nem Win32; o tempo vem em cada entrada (relógio injetável nos testes). Toda saída termina antes o traço em
// andamento e confirma o texto que está sendo digitado (vazio = cancelado); depois de sair, ignora tudo.
// Texto (Plano 4D): com a ferramenta texto, o clique começa a digitação na âncora; caracteres, Backspace e Shift+Enter
// editam (limites do annotation-text); Enter confirma; Esc e Ctrl+Z cancelam só o texto (o Esc seguinte sai); clicar
// de novo confirma e começa outro texto (ou o traço, se a ferramenta mudou). Digitar conta como atividade.
class DrawSession {
```
5. trocar:
```cpp
	bool drawing() const { return drawing_; }
	DrawExit exitReason() const { return exit_; }
```
por:
```cpp
	bool drawing() const { return drawing_; }
	bool typing() const { return typing_; }
	DrawExit exitReason() const { return exit_; }
```
6. trocar:
```cpp
	std::vector<DrawCommand> finish(DrawExit reason);
	// Ponto do desktop preso ao monitor e convertido para a captura (arrastar para fora fica na borda).
```
por:
```cpp
	std::vector<DrawCommand> finish(DrawExit reason);
	// Fecha a digitação: TextCommit (texto com algo) ou TextCancel (vazio) em `out`. Nada sem digitação.
	void closeText(std::vector<DrawCommand> &out);
	// Ponto do desktop preso ao monitor e convertido para a captura (arrastar para fora fica na borda).
```
7. trocar:
```cpp
	bool drawing_ = false;
	StrokePoint last_;
```
por:
```cpp
	bool drawing_ = false;
	bool typing_ = false;
	std::string text_;
	StrokePoint last_;
```

- [ ] **Step 4: Implementação**

Em `frontend/voltz/core/draw-session.cpp`:
1. trocar:
```cpp
#include "draw-session.hpp"

#include <algorithm>
```
por:
```cpp
#include "draw-session.hpp"
#include "annotation-text.hpp"

#include <algorithm>
```
2. trocar:
```cpp
	out.exit = exit;
	return out;
```
por:
```cpp
	out.exit = exit;
	return out;
}

DrawCommand textEdit(const std::string &text)
{
	DrawCommand out = command(DrawCommandType::TextEdit);
	out.text = text;
	return out;
```
3. trocar:
```cpp
}

std::vector<DrawCommand> DrawSession::finish(DrawExit reason)
```
por:
```cpp
}

void DrawSession::closeText(std::vector<DrawCommand> &out)
{
	if (!typing_) {
		return;
	}
	out.push_back(command(text_.empty() ? DrawCommandType::TextCancel : DrawCommandType::TextCommit));
	typing_ = false;
	text_.clear();
}

std::vector<DrawCommand> DrawSession::finish(DrawExit reason)
```
4. trocar:
```cpp
	}
	exit_ = reason;
```
por:
```cpp
	}
	closeText(out);
	exit_ = reason;
```
5. trocar:
```cpp
	case DrawInputKind::Escape:
		return finish(DrawExit::Escape);
```
por:
```cpp
	case DrawInputKind::Escape:
		if (typing_) {
			/* o primeiro Esc só cancela o texto (conta como atividade); o seguinte sai */
			lastInputNs_ = std::max(lastInputNs_, input.tNs);
			typing_ = false;
			text_.clear();
			return {command(DrawCommandType::TextCancel)};
		}
		return finish(DrawExit::Escape);
```
6. trocar:
```cpp
		if (!point.inside) {
			break;
```
por:
```cpp
		if (!point.inside) {
			break;
		}
		/* clicar de novo confirma o texto que estava sendo digitado */
		closeText(out);
		if (input.textTool) {
			typing_ = true;
			text_.clear();
			out.push_back(command(DrawCommandType::TextBegin, {StrokePoint{point.x, point.y}}));
			break;
```
7. trocar:
```cpp
	case DrawInputKind::Undo:
		/* desfazer tira o traço em andamento: o botão segue apertado, mas nada mais vai para ele */
```
por:
```cpp
	case DrawInputKind::Undo:
		if (typing_) {
			/* durante a digitação, desfazer tira só o texto em andamento */
			typing_ = false;
			text_.clear();
			out.push_back(command(DrawCommandType::TextCancel));
			break;
		}
		/* desfazer tira o traço em andamento: o botão segue apertado, mas nada mais vai para ele */
```
8. trocar:
```cpp
		out.push_back(command(DrawCommandType::Undo));
		break;
```
por:
```cpp
		out.push_back(command(DrawCommandType::Undo));
		break;
	case DrawInputKind::Char:
		if (typing_ && appendTextCodepoint(text_, input.codepoint)) {
			out.push_back(textEdit(text_));
		}
		break;
	case DrawInputKind::Backspace:
		if (typing_ && popTextCodepoint(text_)) {
			out.push_back(textEdit(text_));
		}
		break;
	case DrawInputKind::Enter:
		if (!typing_) {
			break;
		}
		if (input.shift) {
			if (appendTextCodepoint(text_, U'\n')) {
				out.push_back(textEdit(text_));
			}
			break;
		}
		closeText(out);
		break;
```

- [ ] **Step 5: Rodar e ver passar**

```bash
"$CLANGFMT" -i frontend/voltz/core/draw-session.hpp frontend/voltz/core/draw-session.cpp frontend/voltz/tests/test-draw-text.cpp
```
Run: COMPILAR TESTES; RODAR TESTES; `build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe | tail -3`.
Expected: `test cases: 197 | 197 passed` (1692 asserções; os 11 casos do `test-draw-session.cpp` do 4C continuam
iguais — o `describe` dele não conhece os comandos novos, que nunca aparecem sem a ferramenta texto).

- [ ] **Step 6: Commit**

```bash
git add frontend/voltz/core/draw-session.hpp frontend/voltz/core/draw-session.cpp frontend/voltz/tests/test-draw-text.cpp frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): digitacao do texto na sessao do modo desenho (editar, confirmar, cancelar so o texto, toda saida confirma)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Casca — texto ao vivo (`AnnotationController`, janela de entrada, modo desenho e pedidos de teste)

**Files:**
- Modify: `frontend/voltz/annotate/AnnotationController.hpp`, `frontend/voltz/annotate/AnnotationController.cpp`
- Modify: `frontend/voltz/annotate/DrawInputWindow.hpp`, `frontend/voltz/annotate/DrawInputWindow.cpp`
- Modify: `frontend/voltz/annotate/DrawModeController.cpp`
- Modify: `frontend/voltz/annotate/AnnotationTestVendor.hpp`, `frontend/voltz/annotate/AnnotationTestVendor.cpp`

O `AnnotationController` ganha `beginText`/`updateText`/`commitText`/`cancelText`: o texto em andamento é o último traço
do `StrokeStore`, vai **inteiro** ao plugin a cada mudança (`voltz_annot_begin` com o mesmo id: o `AnnotationFeed` troca
o traço) e a sobreposição redesenha só o antes ∪ o depois, com o **cursor** (um traço de caneta de `textCaretStroke`)
que só ela desenha — não vai ao plugin nem ao diário. `endStroke` confirma o texto aberto (o `finish` do modo desenho e a
saída do app passam por ele); `undo` durante a digitação cancela só o texto; `setStyle` durante a digitação muda a cor, o
tamanho e a fonte do texto aberto. O conteúdo digitado nunca vai para o log. A janela de entrada passa a traduzir teclas
(`TranslateMessage` → `WM_CHAR`, com as duas metades de um caractere fora do plano básico juntadas), manda Backspace e
Enter (com Shift) à sessão, engole `WM_SYSCHAR` e mostra o cursor em I com a ferramenta texto — **sem nenhuma API de
foco ou captura nova** (o grep D continua com as mesmas contagens do 4C). O `DrawShared` ganha o atômico `textTool`
(escrito pela UI ao mudar o estilo, lido pela thread de entrada no clique e no cursor).

- [ ] **Step 1: `AnnotationController` — cabeçalho**

Em `frontend/voltz/annotate/AnnotationController.hpp`:
1. trocar:
```cpp
#include <cstdint>
#include <memory>
```
por:
```cpp
#include <cstdint>
#include <initializer_list>
#include <memory>
```
2. trocar:
```cpp
// só a região que mudou; o estilo escolhido na barra é salvo; drawTarget diz onde o modo desenho pode desenhar.
// Thread da UI.
class AnnotationController : public QObject {
```
por:
```cpp
// só a região que mudou; o estilo escolhido na barra é salvo; drawTarget diz onde o modo desenho pode desenhar.
// Plano 4D: texto (beginText/updateText/commitText/cancelText) — o texto em andamento aparece ao vivo no programa e na
// sobreposição, que desenha também o cursor de digitação (só ela: o cursor não é traço). O conteúdo digitado nunca vai
// para o log. Thread da UI.
class AnnotationController : public QObject {
```
3. trocar:
```cpp
	void extendStroke(const std::vector<StrokePoint> &points);
	void endStroke();
	void undo();
	void clear();
	// Ferramenta, cor e espessura do próximo traço e "somem após" (limitados). Valem na hora (styleChanged); o user.ini
	// é gravado 400 ms depois da última mudança.
	void setStyle(const AnnotationStyle &style);
```
por:
```cpp
	void extendStroke(const std::vector<StrokePoint> &points);
	// Termina o traço em andamento; texto em andamento é confirmado (commitText).
	void endStroke();
	// Durante a digitação, desfazer cancela só o texto (cancelText).
	void undo();
	void clear();
	// Texto (Plano 4D). Começa um texto VAZIO na âncora (pixels da captura) com a cor, o tamanho e a fonte do estilo
	// (termina antes o traço em andamento); a sobreposição mostra o cursor. Devolve o id (0 se desligado).
	int beginText(const StrokePoint &anchor);
	// Troca o texto em andamento (limpo e limitado pelo núcleo): o plugin recebe o traço inteiro de novo e a sobreposição
	// redesenha o antes ∪ o depois (com o cursor).
	void updateText(const std::string &text);
	// Confirma o texto em andamento: vira um traço terminado (diário, "somem após"). Vazio = cancelText.
	void commitText();
	// Cancela o texto em andamento: vazio sai sem registro (nunca apareceu); com algo, sai como um desfazer.
	void cancelText();
	bool typingText() const;
	// Ferramenta, cor e espessura do próximo traço, "somem após" e tamanho/fonte do texto (limitados). Valem na hora
	// (styleChanged; o texto em andamento muda de cor, tamanho e fonte); o user.ini é gravado 400 ms depois da última
	// mudança.
	void setStyle(const AnnotationStyle &style);
```
4. trocar:
```cpp
private:
	void removed(const std::vector<RemovedStroke> &strokes, bool all);
	void scheduleExpiry();
```
por:
```cpp
private:
	// `extra` (cursor que saiu, por exemplo) entra na região redesenhada da sobreposição.
	void removed(const std::vector<RemovedStroke> &strokes, bool all, const Stroke *extra = nullptr);
	void scheduleExpiry();
```
5. trocar:
```cpp
	std::vector<const Stroke *> strokePointers() const;
	void markBurned(const char *why);
```
por:
```cpp
	std::vector<const Stroke *> strokePointers() const;
	// Os traços + o cursor de digitação (só na sobreposição).
	std::vector<const Stroke *> overlayStrokes() const;
	// Região do buffer da sobreposição que cobre esses traços (pixels da captura); nullopt sem sobreposição.
	std::optional<PixelRect> overlayBounds(std::initializer_list<const Stroke *> strokes) const;
	// O texto em andamento mudou (texto ou estilo): manda o traço inteiro ao plugin e redesenha antes ∪ depois.
	void textChanged(const Stroke &before, const Stroke &caretBefore);
	void markBurned(const char *why);
```
6. trocar:
```cpp
	AnnotationStyle style_;
	std::unique_ptr<OverlayWindow> overlay_;
```
por:
```cpp
	AnnotationStyle style_;
	Stroke caret_;        // cursor de digitação (pontos vazios = sem cursor)
	bool typing_ = false; // há um texto em andamento (o último traço do store_)
	std::unique_ptr<OverlayWindow> overlay_;
```

- [ ] **Step 2: `AnnotationController` — implementação**

Em `frontend/voltz/annotate/AnnotationController.cpp`:
1. trocar:
```cpp
#include "OverlayWindow.hpp"

#include <voltz/core/live-protocol.hpp>
```
por:
```cpp
#include "OverlayWindow.hpp"

#include <voltz/core/annotation-font.hpp>
#include <voltz/core/annotation-text.hpp>
#include <voltz/core/live-protocol.hpp>
```
2. trocar:
```cpp
	}
	const bool forced = testMode_ && qEnvironmentVariableIsSet(live::kTestBurnedEnv);
```
por:
```cpp
	}
	if (!embeddedFontAvailable(TextFont::Regular) || !embeddedFontAvailable(TextFont::Bold)) {
		blog(LOG_WARNING, "[voltz] anotacoes: a fonte do texto nao abriu (o texto nao aparece)");
	}
	const bool forced = testMode_ && qEnvironmentVariableIsSet(live::kTestBurnedEnv);
```
3. trocar:
```cpp
	}
	const int id = store_.begin(tool, color, width, first, nowNs());
```
por:
```cpp
	}
	if (tool == AnnotationTool::Text) {
		return beginText(first); /* texto pelo pedido de teste InjectStroke: começa vazio */
	}
	const int id = store_.begin(tool, color, width, first, nowNs());
```
4. trocar:
```cpp
	}
	const LiveStroke *ended = store_.end(nowNs());
```
por:
```cpp
	}
	if (typingText()) {
		commitText();
		return;
	}
	const LiveStroke *ended = store_.end(nowNs());
```
5. trocar:
```cpp
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
```
por:
```cpp
{
	if (shutdown_) {
		return;
	}
	if (typingText()) {
		cancelText();
		return;
	}
	removed(store_.undo(nowNs()), false);
}

void AnnotationController::clear()
{
	if (shutdown_) {
		return;
	}
	const Stroke caretBefore = caret_;
	typing_ = false;
	caret_ = Stroke{};
	removed(store_.clear(nowNs()), true, &caretBefore);
}

int AnnotationController::beginText(const StrokePoint &anchor)
{
	if (shutdown_) {
		return 0;
	}
	if (store_.openId() != 0) {
		endStroke();
	}
	const int id = store_.beginText(style_.color, style_.textSize, style_.textFont, anchor, nowNs());
	if (id == 0) {
		return 0;
	}
	const Stroke &stroke = store_.visible().back().stroke;
	sendBegin(stroke); /* vazio: o plugin só guarda o id, que o texto vai substituir */
	typing_ = true;
	caret_ = textCaretStroke(stroke);
	refreshOverlay(true, overlayBounds({&caret_}));
	blog(LOG_INFO, "[voltz] anotacoes: texto comecado (tam %d, fonte %s)", stroke.textSize,
	     textFontToString(stroke.textFont));
	return id;
}

bool AnnotationController::typingText() const
{
	return typing_ && store_.openId() != 0 && store_.visible().back().stroke.tool == AnnotationTool::Text;
}

void AnnotationController::textChanged(const Stroke &before, const Stroke &caretBefore)
{
	const Stroke &after = store_.visible().back().stroke;
	sendBegin(after); /* o plugin troca o traço de mesmo id inteiro */
	caret_ = textCaretStroke(after);
	refreshOverlay(true, overlayBounds({&before, &after, &caretBefore, &caret_}));
}

void AnnotationController::updateText(const std::string &text)
{
	if (shutdown_ || !typingText()) {
		return;
	}
	const Stroke before = store_.visible().back().stroke;
	const Stroke caretBefore = caret_;
	if (!store_.setOpenText(text) || store_.visible().back().stroke.text == before.text) {
		return;
	}
	textChanged(before, caretBefore);
}

void AnnotationController::commitText()
{
	if (shutdown_ || !typingText()) {
		return;
	}
	if (store_.visible().back().stroke.text.empty()) {
		cancelText();
		return;
	}
	const Stroke caretBefore = caret_;
	typing_ = false;
	caret_ = Stroke{};
	const LiveStroke *ended = store_.end(nowNs());
	if (!ended) {
		return;
	}
	const LiveStroke copy = *ended;
	emit strokeEnded(copy);
	scheduleExpiry();
	refreshOverlay(true, overlayBounds({&caretBefore}));
	blog(LOG_INFO, "[voltz] anotacoes: texto confirmado (%zu caractere(s))", textCharCount(copy.stroke.text));
}

void AnnotationController::cancelText()
{
	if (shutdown_ || !typingText()) {
		return;
	}
	const Stroke open = store_.visible().back().stroke;
	const Stroke caretBefore = caret_;
	typing_ = false;
	caret_ = Stroke{};
	if (open.text.empty()) {
		/* nunca apareceu (nem no programa): sai sem registro no diário */
		store_.discardOpen();
		sendRemove(open.id);
		refreshOverlay(true, overlayBounds({&caretBefore}));
	} else {
		removed(store_.undo(nowNs()), false, &caretBefore);
	}
	blog(LOG_INFO, "[voltz] anotacoes: texto cancelado");
}

void AnnotationController::removed(const std::vector<RemovedStroke> &strokes, bool all, const Stroke *extra)
{
	if (strokes.empty()) {
		if (extra && !extra->points.empty()) {
			refreshOverlay(true, overlayBounds({extra}));
		}
		return;
```
6. trocar:
```cpp
			area = uniteRects(area, strokeBounds(stroke.live.stroke, scaleX, scaleY));
		}
```
por:
```cpp
			area = uniteRects(area, strokeBounds(stroke.live.stroke, scaleX, scaleY));
		}
		if (extra) {
			area = uniteRects(area, strokeBounds(*extra, scaleX, scaleY));
		}
```
7. trocar:
```cpp
	}
	const std::vector<const Stroke *> strokes = strokePointers();
	if (!hasTarget) {
```
por:
```cpp
	}
	const std::vector<const Stroke *> strokes = overlayStrokes();
	if (!hasTarget) {
```
8. trocar:
```cpp
}

void AnnotationController::setStyle(const AnnotationStyle &style)
```
por:
```cpp
}

std::vector<const Stroke *> AnnotationController::overlayStrokes() const
{
	std::vector<const Stroke *> out = strokePointers();
	if (typingText() && !caret_.points.empty()) {
		out.push_back(&caret_); /* por cima de tudo, só na sobreposição */
	}
	return out;
}

std::optional<PixelRect> AnnotationController::overlayBounds(std::initializer_list<const Stroke *> strokes) const
{
	double scaleX = 0.0;
	double scaleY = 0.0;
	if (!overlayScale(scaleX, scaleY)) {
		return std::nullopt;
	}
	PixelRect area;
	for (const Stroke *stroke : strokes) {
		if (stroke) {
			area = uniteRects(area, strokeBounds(*stroke, scaleX, scaleY));
		}
	}
	return area;
}

void AnnotationController::setStyle(const AnnotationStyle &style)
```
9. trocar:
```cpp
		scheduleExpiry();
	}
```
por:
```cpp
		scheduleExpiry();
	}
	if (typingText()) {
		/* cor, tamanho e fonte escolhidos na barra durante a digitação valem para o texto em andamento */
		const Stroke before = store_.visible().back().stroke;
		const Stroke caretBefore = caret_;
		if (store_.setOpenTextStyle(style_.color, style_.textSize, style_.textFont)) {
			const Stroke &after = store_.visible().back().stroke;
			if (!(after.color == before.color) || after.textSize != before.textSize ||
			    after.textFont != before.textFont) {
				textChanged(before, caretBefore);
			}
		}
	}
```
10. trocar:
```cpp
	state.insert(QStringLiteral("amostras"), samples);
	state.insert(QStringLiteral("modo_desenho"), drawActive_);
```
por:
```cpp
	state.insert(QStringLiteral("amostras"), samples);
	state.insert(QStringLiteral("digitando"), typingText());
	state.insert(QStringLiteral("cursor_texto"), caret_.points.size() == 2 && typingText()
							     ? QJsonArray{caret_.points[0].x, caret_.points[0].y,
									  caret_.points[1].x, caret_.points[1].y}
							     : QJsonArray{});
	state.insert(QStringLiteral("modo_desenho"), drawActive_);
```

- [ ] **Step 3: Janela de entrada traduz as teclas (arquivo de exceção: nenhuma chamada de foco/captura nova)**

Em `frontend/voltz/annotate/DrawInputWindow.hpp`:
1. trocar:
```cpp
	std::atomic<int64_t> uiHeartbeatNs{0}; // a UI escreve a cada 250 ms; a thread de entrada lê no tique (vigia)
	// Protege os campos abaixo. A UI só o segura por instantes; a thread de entrada, com a janela viva, só por try_lock.
```
por:
```cpp
	std::atomic<int64_t> uiHeartbeatNs{0}; // a UI escreve a cada 250 ms; a thread de entrada lê no tique (vigia)
	// A ferramenta escolhida é o texto (a UI escreve ao mudar o estilo; a thread de entrada lê no clique e no cursor).
	std::atomic<bool> textTool{false};
	// Protege os campos abaixo. A UI só o segura por instantes; a thread de entrada, com a janela viva, só por try_lock.
```
2. trocar:
```cpp
// thread própria, só sobre o monitor da Tela: em camadas com alfa 1/255 (invisível, mas recebe o mouse), sempre no topo,
// fora da barra de tarefas, fora da captura quando o Windows suporta, cursor em cruz, sem dono. A thread não faz nada
// além dela: roda a DrawSession (núcleo), que decide todas as saídas — inclusive o vigia (a UI parou de pulsar por 1 s)
```
por:
```cpp
// thread própria, só sobre o monitor da Tela: em camadas com alfa 1/255 (invisível, mas recebe o mouse), sempre no topo,
// fora da barra de tarefas, fora da captura quando o Windows suporta, cursor em cruz (ou em I com a ferramenta texto),
// sem dono. Plano 4D: as teclas viram caracteres (TranslateMessage → WM_CHAR, com acentos e substitutos UTF-16
// juntados) para a digitação do texto, que é da DrawSession; nenhuma API de foco nova. A thread não faz nada
// além dela: roda a DrawSession (núcleo), que decide todas as saídas — inclusive o vigia (a UI parou de pulsar por 1 s)
```

Em `frontend/voltz/annotate/DrawInputWindow.cpp`:
1. trocar:
```cpp
	int64_t lastTickNs = 0;
};
```
por:
```cpp
	int64_t lastTickNs = 0;
	wchar_t highSurrogate = 0; // WM_CHAR: primeira metade de um caractere fora do plano básico (emoji)
};
```
2. trocar:
```cpp
		if (LOWORD(lParam) == HTCLIENT) {
			SetCursor(LoadCursorW(nullptr, IDC_CROSS));
			return TRUE;
```
por:
```cpp
		if (LOWORD(lParam) == HTCLIENT) {
			SetCursor(LoadCursorW(nullptr, state->shared->textTool.load() ? IDC_IBEAM : IDC_CROSS));
			return TRUE;
```
3. trocar:
```cpp
		break;
	case WM_LBUTTONDOWN:
		/* o mouse fica com esta janela só entre apertar e soltar (arrastar para fora do monitor continua o traço) */
```
por:
```cpp
		break;
	case WM_LBUTTONDOWN: {
		/* o mouse fica com esta janela só entre apertar e soltar (arrastar para fora do monitor continua o traço) */
```
4. trocar:
```cpp
		}
		deliver(*state, state->session.handle(pointerAt(DrawInputKind::LeftDown, *state, lParam)));
		return 0;
	case WM_MOUSEMOVE:
```
por:
```cpp
		}
		DrawInput input = pointerAt(DrawInputKind::LeftDown, *state, lParam);
		input.textTool = state->shared->textTool.load();
		deliver(*state, state->session.handle(input));
		return 0;
	}
	case WM_MOUSEMOVE:
```
5. trocar:
```cpp
			kind = DrawInputKind::Undo;
		}
		deliver(*state, state->session.handle(eventAt(kind)));
		if (message == WM_SYSKEYDOWN) {
```
por:
```cpp
			kind = DrawInputKind::Undo;
		} else if (wParam == VK_BACK) {
			kind = DrawInputKind::Backspace;
		} else if (wParam == VK_RETURN) {
			kind = DrawInputKind::Enter;
		}
		DrawInput input = eventAt(kind);
		input.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
		deliver(*state, state->session.handle(input));
		if (message == WM_SYSKEYDOWN) {
```
6. trocar:
```cpp
	}
	case WM_ACTIVATE:
```
por:
```cpp
	}
	case WM_CHAR: {
		/* o texto digitado é da DrawSession (que recusa controles como Backspace, Enter e Esc, já tratados acima);
		 * um caractere fora do plano básico chega em duas metades */
		const auto unit = static_cast<wchar_t>(wParam);
		char32_t codepoint = static_cast<char32_t>(unit);
		if (unit >= 0xD800 && unit <= 0xDBFF) {
			state->highSurrogate = unit;
			return 0;
		}
		if (unit >= 0xDC00 && unit <= 0xDFFF) {
			if (state->highSurrogate == 0) {
				return 0;
			}
			codepoint = 0x10000 + ((static_cast<char32_t>(state->highSurrogate) - 0xD800) << 10) +
				    (static_cast<char32_t>(unit) - 0xDC00);
		}
		state->highSurrogate = 0;
		DrawInput input = eventAt(DrawInputKind::Char);
		input.codepoint = codepoint;
		deliver(*state, state->session.handle(input));
		return 0;
	}
	case WM_SYSCHAR:
		return 0; /* Alt+letra: sem o aviso sonoro do DefWindowProc (a janela não tem menu) */
	case WM_ACTIVATE:
```
7. trocar:
```cpp
	while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
		DispatchMessageW(&msg);
```
por:
```cpp
	while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
		/* WM_KEYDOWN → WM_CHAR (acentos por teclas mortas inclusive): a digitação do texto */
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
```

- [ ] **Step 4: Modo desenho entrega o texto**

Em `frontend/voltz/annotate/DrawModeController.cpp`:
1. trocar:
```cpp
			toolbar_->setAnnotationStyle(annotations_->style());
		}
```
por:
```cpp
			toolbar_->setAnnotationStyle(annotations_->style());
		}
		if (shared_) {
			shared_->textTool.store(annotations_->style().tool == AnnotationTool::Text);
		}
```
2. trocar:
```cpp
	shared_->uiHeartbeatNs.store(nowNs());
	{
```
por:
```cpp
	shared_->uiHeartbeatNs.store(nowNs());
	shared_->textTool.store(annotations_->style().tool == AnnotationTool::Text);
	{
```
3. trocar:
```cpp
			break;
		case DrawCommandType::Exit:
```
por:
```cpp
			break;
		case DrawCommandType::TextBegin:
			flush();
			if (!command.points.empty()) {
				annotations_->beginText(command.points.front());
				if (toolbar_) {
					toolbar_->raiseWithoutActivating();
				}
			}
			break;
		case DrawCommandType::TextEdit:
			flush();
			annotations_->updateText(command.text);
			break;
		case DrawCommandType::TextCommit:
			flush();
			annotations_->commitText();
			break;
		case DrawCommandType::TextCancel:
			flush();
			annotations_->cancelText();
			break;
		case DrawCommandType::Exit:
```
4. trocar:
```cpp
	exitReason_ = reason;
	/* Nenhum traço fica aberto (a sessão sempre manda o End antes do Exit; isto cobre a thread que não o entregou).
	 * Sem traço aberto, não faz nada. */
	if (annotations_) {
```
por:
```cpp
	exitReason_ = reason;
	/* Nenhum traço fica aberto (a sessão sempre manda o End/TextCommit antes do Exit; isto cobre a thread que não o
	 * entregou — e confirma o texto em andamento). Sem traço aberto, não faz nada. */
	if (annotations_) {
```

- [ ] **Step 5: Pedidos de teste do texto**

Em `frontend/voltz/annotate/AnnotationTestVendor.hpp`:
trocar:
```cpp
//   SetAnnotationStyle {estilo:"{...}"} — AnnotationController::setStyle (o que a barra do modo desenho faz);
//   ToolbarImage {queimadas?} → {png (base64), largura, altura, janela_nativa} — uma DrawToolbar NUNCA mostrada,
```
por:
```cpp
//   SetAnnotationStyle {estilo:"{...}"} — AnnotationController::setStyle (o que a barra do modo desenho faz);
//   InjectText {pts:"[[x,y]]", texto, aberto?} — começa um texto (estilo atual) na âncora, troca o texto e (sem
//   "aberto") confirma; TypeText {texto} troca o texto em andamento (o que a digitação faz); CommitText, CancelText;
//   ToolbarImage {queimadas?} → {png (base64), largura, altura, janela_nativa} — uma DrawToolbar NUNCA mostrada,
```

Em `frontend/voltz/annotate/AnnotationTestVendor.cpp`:
1. trocar:
```cpp
obs_websocket_vendor gVendor = nullptr;  // só na thread da UI

constexpr const char *kRequests[] = {"InjectStroke",    "ExtendStroke",       "EndStroke",
				     "UndoStroke",      "ClearStrokes",       "ProbeOverlay",
				     "AnnotationState", "SetAnnotationStyle", "ToolbarImage"};

// Entrega `work` na thread da UI sem esperar; falso se o controlador já saiu (EXIT). O mutex é segurado enquanto o evento
```
por:
```cpp
obs_websocket_vendor gVendor = nullptr;  // só na thread da UI

constexpr const char *kRequests[] = {"InjectStroke", "ExtendStroke", "EndStroke",       "UndoStroke",
				     "ClearStrokes", "ProbeOverlay", "InjectText",      "TypeText",
				     "CommitText",   "CancelText",   "AnnotationState", "SetAnnotationStyle",
				     "ToolbarImage"};

// Entrega `work` na thread da UI sem esperar; falso se o controlador já saiu (EXIT). O mutex é segurado enquanto o evento
```
2. trocar:
```cpp
			  post([points](AnnotationController *controller) { controller->probeOverlay(points); }));
}
```
por:
```cpp
			  post([points](AnnotationController *controller) { controller->probeOverlay(points); }));
}

void injectText(obs_data_t *request, obs_data_t *response, void *)
{
	const std::vector<StrokePoint> points = pointsOf(request);
	const std::string text = obs_data_get_string(request, "texto");
	const bool open = obs_data_get_bool(request, "aberto");
	const bool ok = !points.empty() && post([points, text, open](AnnotationController *controller) {
		if (!controller->beginText(points.front())) {
			return;
		}
		controller->updateText(text);
		if (!open) {
			controller->commitText();
		}
	});
	obs_data_set_bool(response, "ok", ok);
}

void typeText(obs_data_t *request, obs_data_t *response, void *)
{
	const std::string text = obs_data_get_string(request, "texto");
	obs_data_set_bool(response, "ok",
			  post([text](AnnotationController *controller) { controller->updateText(text); }));
}

void commitText(obs_data_t *, obs_data_t *response, void *)
{
	obs_data_set_bool(response, "ok", post([](AnnotationController *controller) { controller->commitText(); }));
}

void cancelText(obs_data_t *, obs_data_t *response, void *)
{
	obs_data_set_bool(response, "ok", post([](AnnotationController *controller) { controller->cancelText(); }));
}
```
3. trocar:
```cpp
	obs_websocket_vendor_register_request(gVendor, "ProbeOverlay", probeOverlay, nullptr);
	obs_websocket_vendor_register_request(gVendor, "AnnotationState", annotationState, nullptr);
```
por:
```cpp
	obs_websocket_vendor_register_request(gVendor, "ProbeOverlay", probeOverlay, nullptr);
	obs_websocket_vendor_register_request(gVendor, "InjectText", injectText, nullptr);
	obs_websocket_vendor_register_request(gVendor, "TypeText", typeText, nullptr);
	obs_websocket_vendor_register_request(gVendor, "CommitText", commitText, nullptr);
	obs_websocket_vendor_register_request(gVendor, "CancelText", cancelText, nullptr);
	obs_websocket_vendor_register_request(gVendor, "AnnotationState", annotationState, nullptr);
```

- [ ] **Step 6: Formatar, compilar e grep de segurança**

```bash
"$CLANGFMT" -i frontend/voltz/annotate/AnnotationController.hpp frontend/voltz/annotate/AnnotationController.cpp frontend/voltz/annotate/DrawInputWindow.hpp frontend/voltz/annotate/DrawInputWindow.cpp frontend/voltz/annotate/DrawModeController.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp
```
Run: COMPILAR APP (segundo plano); RODAR TESTES (197); GREP DE SEGURANÇA (A, B, C, D, F).
Expected: `exit=0`; `test cases: 197 | 197 passed`; grep como em Convenções — em especial D **igual ao 4C**
(`1 ReleaseCapture(`, `1 SetCapture(`, `2 SetForegroundWindow(`) e `texto-no-log=1`.

- [ ] **Step 7: Teste ao vivo (o modo desenho NÃO liga; o texto entra só pelos pedidos de teste)**

Bloco de teste com `$ovW = 2560; $ovH = 1440` (escala 1: o buffer da sobreposição tem os pixels da captura). No `try`, no lugar do comentário (depois do `Start-App`):
```powershell
  [void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
  [void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Só tela"}}]')
  $style = '{"ferr":"texto","cor":"#D4F53C","esp":4,"somem_apos_s":0,"tam":96,"fonte":"dm_sans_negrito"}'
  [void](VendorShell "SetAnnotationStyle" (ConvertTo-Json @{ estilo = $style } -Compress))
  [void](VendorShell "InjectText" (Text-Json "[[400,300]]" "Voltz" -Open)); $st = Shell-State; Assert-DrawSafe "texto aberto"
  "1) estilo=$($st.estilo) digitando=$($st.digitando) cursor=$(@($st.cursor_texto) -join ',') ultimo=$($st.ultimo)"
  $caret = @($st.cursor_texto)
  [void](VendorShell "ProbeOverlay" (ConvertTo-Json @{ pts = (Grid 400 760 6 310 400 6) } -Compress)); $st = Shell-State
  "2) letras no buffer: $(@($st.amostras | Where-Object { $_ -eq '#FFD4F53C' }).Count) de $(@($st.amostras).Count)"
  [void](VendorShell "ProbeOverlay" (ConvertTo-Json @{ pts = ("[[{0},{1}],[1500,700]]" -f $caret[0], [int](($caret[1] + $caret[3]) / 2)) } -Compress)); $st = Shell-State
  "3) cursor e fundo: $($st.amostras -join ',')"
  [void](VendorShell "TypeText" '{"texto":"Voltz 4D"}'); $st = Shell-State; Assert-DrawSafe "digitando"
  "4) texto=$(($st.ultimo | ConvertFrom-Json).texto) parciais=$($st.sobreposicao.parciais) completas=$($st.sobreposicao.completas)"
  $feed = Feed-State; "5) plugin: $($feed.tracos) traco(s)"
  $img = Scene-Image "Só tela"; "6) programa: $(Lime-Count $img 95 300 72 110) pixels lima"; $img.Save("$sp\4d-programa-texto.png"); $img.Dispose()
  [void](VendorShell "CommitText"); $st = Shell-State; Assert-DrawSafe "confirmado"
  "7) digitando=$($st.digitando) aberto=$($st.aberto) cursor=$(@($st.cursor_texto).Count) na tela=$(@($st.visiveis).Count)"
  [void](VendorShell "InjectText" (Text-Json "[[400,700]]" "" -Open)); [void](VendorShell "CancelText")
  [void](VendorShell "InjectText" (Text-Json "[[400,900]]" "x" -Open)); [void](VendorShell "CancelText")
  $st = Shell-State; Assert-DrawSafe "cancelados"
  "8) na tela=$(@($st.visiveis).Count) digitando=$($st.digitando)"
  [void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.Draw"}}]'); $st = Shell-State; Assert-DrawSafe "recusa"
  "9) modo=$($st.modo_desenho) recusa=$($st.desenho_recusa)"
  Close-App
  LogLines "[voltz] anotacoes: texto"
  "privacidade: $(@(LogLines 'Voltz 4D').Count) linha(s) com o texto"
```
Expected (o vigia sem problema e nenhum `BLOCKED`):
1. `estilo` = o `$style`; `digitando=True`; `cursor` com 4 números; `ultimo` com `"ferr":"texto"`, `"texto":"Voltz"`,
   `"tam":96`, `"fonte":"dm_sans_negrito"` e `"pts":[[400,300]]`.
2. `≥ 20` amostras lima (as letras estão no buffer).
3. `#FFD4F53C,#00000000` (o cursor está no buffer; longe do texto, nada).
4. `texto=Voltz 4D`; `parciais ≥ 2`, `completas ≤ 1` (cada mudança redesenhou só a região).
5. `1 traco(s)` (o plugin trocou o traço, não acumulou).
6. `≥ 40` pixels lima na região do texto (a fonte do plugin desenhou com a fonte embutida). Abra
   `4d-programa-texto.png` e descreva: "Voltz 4D" lima, negrito, no alto à esquerda da Tela cinza, sem cursor.
7. `digitando=False aberto=0 cursor=0 na tela=1`.
8. `na tela=1 digitando=False` (os dois cancelados saíram).
9. `modo=False recusa=teste`.
Log: três `[voltz] anotacoes: texto comecado (tam 96, fonte dm_sans_negrito)`, um `texto confirmado (8 caractere(s))`
e dois `texto cancelado`; `privacidade: 0 linha(s) com o texto`.

- [ ] **Step 8: Commit**

```bash
git add frontend/voltz/annotate/AnnotationController.hpp frontend/voltz/annotate/AnnotationController.cpp frontend/voltz/annotate/DrawInputWindow.hpp frontend/voltz/annotate/DrawInputWindow.cpp frontend/voltz/annotate/DrawModeController.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp
git commit -m "feat(voltz): texto ao vivo no modo desenho (digitacao pela janela de entrada sem API de foco nova, cursor so na sobreposicao, texto inteiro ao plugin, pedidos de teste)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 6: Barra — ferramenta Texto, tamanhos e negrito, seletor de cor completo e linha de dica

**Files:**
- Create: `frontend/voltz/annotate/ColorPicker.hpp`, `frontend/voltz/annotate/ColorPicker.cpp`
- Create: `frontend/voltz/shell/icons/regular/text-b.svg`, `frontend/voltz/shell/icons/fill/text-b-fill.svg`,
  `frontend/voltz/shell/icons/regular/eyedropper.svg` (Phosphor 2.1.1)
- Modify (substituir): `frontend/voltz/annotate/DrawToolbar.hpp`, `frontend/voltz/annotate/DrawToolbar.cpp`
- Modify: `frontend/voltz/annotate/DrawModeController.cpp`, `frontend/voltz/annotate/AnnotationTestVendor.hpp`,
  `frontend/voltz/annotate/AnnotationTestVendor.cpp`, `frontend/voltz/shell/voltz-shell.qrc`,
  `frontend/cmake/feature-voltz.cmake`, `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`

A barra ganha o T (texto, ícone `text-t` que já está no `.qrc`); com ele, as 4 espessuras dão lugar a 4 tamanhos ("A"
de tamanhos crescentes: 24, 36, 48 e 72 px da captura) e ao negrito (`text-b`). "Mais cores" ganha o botão de cor
personalizada (conta-gotas; mostra a cor quando ela não é da paleta) que abre o `ColorPicker` logo abaixo — tudo na
mesma janela sem foco. A linha de dica, embaixo, mostra a dica do botão sob o mouse ou, sem botão, como sair (ou como
digitar, com o texto). O `ToolbarImage` aceita `"ferramenta"` e `"seletor"` para desenhar esses estados sem janela.

- [ ] **Step 1: Ícones oficiais (Phosphor 2.1.1)**

```bash
base="https://unpkg.com/@phosphor-icons/core@2.1.1/assets"
curl -fsSL "$base/regular/text-b.svg" -o frontend/voltz/shell/icons/regular/text-b.svg || echo "FALHOU text-b"
curl -fsSL "$base/fill/text-b-fill.svg" -o frontend/voltz/shell/icons/fill/text-b-fill.svg || echo "FALHOU text-b-fill"
curl -fsSL "$base/regular/eyedropper.svg" -o frontend/voltz/shell/icons/regular/eyedropper.svg || echo "FALHOU eyedropper"
head -c 80 frontend/voltz/shell/icons/regular/text-b.svg; echo; head -c 80 frontend/voltz/shell/icons/regular/eyedropper.svg; echo
```
Expected: nenhum `FALHOU`; os dois começam com `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" fill="currentColor">`.
Em `frontend/voltz/shell/voltz-shell.qrc`:
trocar:
```xml
    <file>icons/fill/pencil-simple-fill.svg</file>
  </qresource>
```
por:
```xml
    <file>icons/fill/pencil-simple-fill.svg</file>
    <file>icons/regular/text-b.svg</file>
    <file>icons/fill/text-b-fill.svg</file>
    <file>icons/regular/eyedropper.svg</file>
  </qresource>
```

Conferir: `grep -c "<file>" frontend/voltz/shell/voltz-shell.qrc` → `103`.

- [ ] **Step 2: Textos (casca, os dois locales)**

Em `frontend/data/locale/voltz-pt-BR.ini`,
no fim do arquivo (UTF-8 sem BOM, fim de linha CRLF como o resto), acrescentar:
```ini
Voltz.Draw.Tool.Text="Texto"
Voltz.Draw.TextSize="Tamanho do texto %1 px"
Voltz.Draw.Bold="Negrito"
Voltz.Draw.CustomColor="Cor personalizada (seletor completo)"
Voltz.Draw.Hint.Exit="Esc, clique direito ou %1 saem do modo desenho."
Voltz.Draw.Hint.Text="Clique onde o texto começa e digite. Enter confirma · Shift+Enter quebra a linha · Esc cancela."
```

Em `frontend/data/locale/voltz-en-US.ini`,
no fim do arquivo (UTF-8 sem BOM, fim de linha CRLF como o resto), acrescentar:
```ini
Voltz.Draw.Tool.Text="Text"
Voltz.Draw.TextSize="Text size %1 px"
Voltz.Draw.Bold="Bold"
Voltz.Draw.CustomColor="Custom color (full picker)"
Voltz.Draw.Hint.Exit="Esc, right-click or %1 exit draw mode."
Voltz.Draw.Hint.Text="Click where the text starts and type. Enter confirms · Shift+Enter breaks the line · Esc cancels."
```

Paridade (Convenções) → sem saída.

- [ ] **Step 3: Seletor de cor**

Criar `frontend/voltz/annotate/ColorPicker.hpp`:
```cpp
#pragma once

#include <voltz/core/mouse-style.hpp>

#include <QImage>
#include <QWidget>

namespace voltz {

// Seletor de cor completo da barra do modo desenho (Plano 4D): quadrado saturação × brilho e faixa de matiz, desenhados
// à mão. Sem diálogo — um QColorDialog é modal, pegaria o foco e a janela de entrada sairia do modo desenho (perda de
// ativação) — e sem foco nunca (a barra nunca fica ativa). Apertar e arrastar escolhe; colorPicked ao soltar (uma
// mudança de estilo por escolha). Thread da UI.
class ColorPicker : public QWidget {
	Q_OBJECT

public:
	explicit ColorPicker(QWidget *parent = nullptr);

	// Marca a cor (sem sinal).
	void setColor(const RgbColor &color);
	RgbColor color() const;
	QSize sizeHint() const override;

signals:
	void colorPicked(voltz::RgbColor color);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	enum class Drag { None, Square, Hue };

	QRectF squareRect() const;
	QRectF hueRect() const;
	void pickAt(const QPointF &position);

	double hue_ = 0.2;
	double saturation_ = 0.75;
	double value_ = 0.96;
	Drag drag_ = Drag::None;
	QImage square_; // cache do quadrado para a matiz squareHue_
	double squareHue_ = -1.0;
};

} // namespace voltz
```

Criar `frontend/voltz/annotate/ColorPicker.cpp`:
```cpp
#include "ColorPicker.hpp"

#include <QColor>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

#include "moc_ColorPicker.cpp"

namespace voltz {

namespace {

constexpr int kWidth = 216;
constexpr int kSquareHeight = 96;
constexpr int kGap = 8;
constexpr int kHueHeight = 14;

QColor toQColor(const RgbColor &color)
{
	return QColor(color.r, color.g, color.b);
}

} // namespace

ColorPicker::ColorPicker(QWidget *parent) : QWidget(parent)
{
	setObjectName("drawColorPicker");
	setFocusPolicy(Qt::NoFocus);
	setAttribute(Qt::WA_OpaquePaintEvent, false);
	setCursor(Qt::CrossCursor);
	setFixedSize(sizeHint());
}

QSize ColorPicker::sizeHint() const
{
	return QSize(kWidth, kSquareHeight + kGap + kHueHeight);
}

QRectF ColorPicker::squareRect() const
{
	return QRectF(0.0, 0.0, kWidth, kSquareHeight);
}

QRectF ColorPicker::hueRect() const
{
	return QRectF(0.0, kSquareHeight + kGap, kWidth, kHueHeight);
}

void ColorPicker::setColor(const RgbColor &color)
{
	const QColor hsv = toQColor(color).toHsv();
	if (hsv.hsvHueF() >= 0.0) {
		hue_ = hsv.hsvHueF(); /* cinza não tem matiz: fica a anterior */
	}
	saturation_ = hsv.hsvSaturationF();
	value_ = hsv.valueF();
	update();
}

RgbColor ColorPicker::color() const
{
	const QColor rgb =
		QColor::fromHsvF(static_cast<float>(hue_), static_cast<float>(saturation_), static_cast<float>(value_))
			.toRgb();
	return RgbColor{static_cast<uint8_t>(rgb.red()), static_cast<uint8_t>(rgb.green()),
			static_cast<uint8_t>(rgb.blue())};
}

void ColorPicker::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QRectF square = squareRect();
	if (square_.isNull() || squareHue_ != hue_) {
		/* saturação na horizontal (0 → 1), brilho na vertical (1 → 0) */
		square_ = QImage(kWidth, kSquareHeight, QImage::Format_RGB32);
		for (int y = 0; y < kSquareHeight; y++) {
			const double value = 1.0 - static_cast<double>(y) / (kSquareHeight - 1);
			for (int x = 0; x < kWidth; x++) {
				const double saturation = static_cast<double>(x) / (kWidth - 1);
				square_.setPixelColor(x, y,
						      QColor::fromHsvF(static_cast<float>(hue_),
								       static_cast<float>(saturation),
								       static_cast<float>(value)));
			}
		}
		squareHue_ = hue_;
	}
	QPainterPath clip;
	clip.addRoundedRect(square, 6.0, 6.0);
	painter.save();
	painter.setClipPath(clip);
	painter.drawImage(square.topLeft(), square_);
	painter.restore();

	const QRectF hue = hueRect();
	QLinearGradient gradient(hue.topLeft(), hue.topRight());
	for (int i = 0; i <= 6; i++) {
		gradient.setColorAt(i / 6.0, QColor::fromHsvF(static_cast<float>(i % 6) / 6.0f, 1.0f, 1.0f));
	}
	painter.setPen(Qt::NoPen);
	painter.setBrush(gradient);
	painter.drawRoundedRect(hue, kHueHeight / 2.0, kHueHeight / 2.0);

	/* marcas: anel no ponto escolhido do quadrado e na matiz */
	const QPointF mark(square.left() + saturation_ * square.width(),
			   square.top() + (1.0 - value_) * square.height());
	painter.setBrush(Qt::NoBrush);
	painter.setPen(QPen(QColor(0, 0, 0, 160), 3.0));
	painter.drawEllipse(mark, 6.0, 6.0);
	painter.setPen(QPen(Qt::white, 1.5));
	painter.drawEllipse(mark, 6.0, 6.0);
	const QPointF hueMark(hue.left() + hue_ * hue.width(), hue.center().y());
	painter.setPen(QPen(QColor(0, 0, 0, 160), 3.0));
	painter.drawEllipse(hueMark, kHueHeight / 2.0, kHueHeight / 2.0);
	painter.setPen(QPen(Qt::white, 1.5));
	painter.drawEllipse(hueMark, kHueHeight / 2.0, kHueHeight / 2.0);
}

void ColorPicker::pickAt(const QPointF &position)
{
	if (drag_ == Drag::Square) {
		const QRectF square = squareRect();
		saturation_ = std::clamp((position.x() - square.left()) / square.width(), 0.0, 1.0);
		value_ = std::clamp(1.0 - (position.y() - square.top()) / square.height(), 0.0, 1.0);
	} else if (drag_ == Drag::Hue) {
		const QRectF hue = hueRect();
		hue_ = std::clamp((position.x() - hue.left()) / hue.width(), 0.0, 0.9999);
	}
	update();
}

void ColorPicker::mousePressEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton) {
		QWidget::mousePressEvent(event);
		return;
	}
	const QPointF position = event->position();
	if (squareRect().contains(position)) {
		drag_ = Drag::Square;
	} else if (hueRect().adjusted(0.0, -kGap / 2.0, 0.0, 0.0).contains(position)) {
		drag_ = Drag::Hue;
	} else {
		drag_ = Drag::None;
		return;
	}
	pickAt(position);
}

void ColorPicker::mouseMoveEvent(QMouseEvent *event)
{
	if (drag_ != Drag::None) {
		pickAt(event->position());
	}
}

void ColorPicker::mouseReleaseEvent(QMouseEvent *event)
{
	if (drag_ == Drag::None || event->button() != Qt::LeftButton) {
		return;
	}
	pickAt(event->position());
	drag_ = Drag::None;
	emit colorPicked(color());
}

} // namespace voltz
```

- [ ] **Step 4: Barra**

Substituir o conteúdo de `frontend/voltz/annotate/DrawToolbar.hpp` por:
```cpp
#pragma once

#include <voltz/core/annotation-model.hpp>
#include <voltz/core/cursor-track.hpp>

#include <QList>
#include <QPair>
#include <QString>
#include <QWidget>

class QButtonGroup;
class QFrame;
class QLabel;
class QScreen;
class QToolButton;

namespace voltz {

class ColorPicker;

// Barra do modo desenho (spec §6.2): caneta, marca-texto, linha, seta, retângulo, elipse, texto · 4 cores rápidas + 8 em
// "mais cores" + cor personalizada (seletor completo, sem diálogo) · 4 espessuras (ou, com o texto, 4 tamanhos e
// negrito) · desfazer · limpar · "traços somem após" (nunca → 3 → 5 → 10 s) · sair · linha de dica (a dica do botão sob
// o mouse; as dicas flutuantes do Qt não aparecem numa janela que nunca fica ativa). Janela Qt própria (sem pai: nada de
// dono de outra thread), sempre no topo, que NUNCA pega o foco (Qt::WindowDoesNotAcceptFocus + WS_EX_NOACTIVATE): a
// janela de entrada do desenho continua ativa e o Esc continua chegando nela. Com a propriedade isOBSProjectorWindow o
// OBSApp::notify não mexe na afinidade dela; fora da captura por WDA_EXCLUDEFROMCAPTURE quando o Windows suporta. Aviso
// visível quando as anotações estão "queimadas" (spec §6.3). A janela nativa só nasce no primeiro showOnScreen — o
// grab() do pedido de teste ToolbarImage não cria janela. Título nativo "VoltzDrawToolbar" (os testes a procuram para
// garantir que ela nunca aparece num teste automático). Thread da UI.
class DrawToolbar : public QWidget {
	Q_OBJECT

public:
	explicit DrawToolbar(QWidget *parent = nullptr);

	// Marca ferramenta, cor, espessura, tamanho/fonte do texto e "somem após" (não emite sinais).
	void setAnnotationStyle(const AnnotationStyle &style);
	void setBurned(bool burned);
	// Texto do atalho de desenhar na dica do botão Sair e na linha de dica.
	void setExitHint(const QString &shortcut);
	// Abre (ou fecha) "mais cores" com o seletor completo (o botão de cor personalizada faz o mesmo).
	void showColorPicker(bool show);
	// Mostra no alto e ao centro do monitor de nome GDI `deviceName` ("\\.\DISPLAYn", MonitorDescription::deviceName;
	// a QScreen sai do screenForMonitor, não do QScreen::name(), que no Qt 6 é o nome amigável), sem ativar. Monitor não
	// encontrado → tela principal.
	void showOnScreen(const QString &deviceName, bool excludeFromCapture);
	// Põe de novo por cima (a janela de entrada acabou de ir para o topo), sem ativar.
	void raiseWithoutActivating();
	// Retângulo da janela em pixels físicos do desktop (o buraco na janela de entrada); vazio se não está visível.
	MonitorRect nativeRect() const;

signals:
	void toolChosen(voltz::AnnotationTool tool);
	void colorChosen(voltz::RgbColor color);
	void widthChosen(int width);
	void textSizeChosen(int size);
	void textFontChosen(voltz::TextFont font);
	void fadeChosen(int seconds);
	void undoClicked();
	void clearClicked();
	void exitClicked();
	void nativeRectChanged();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void moveEvent(QMoveEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	QToolButton *makeButton(QWidget *parent, const QString &hint);
	QToolButton *addColor(QWidget *parent, const RgbColor &color);
	QFrame *separator(QWidget *parent);
	void markColor(const RgbColor &color);
	void updateFadeText();
	void updateHint();
	// adjustSize e, se visível, de volta ao alto e ao centro do monitor em que está (aviso de "queimadas", "mais
	// cores", troca entre espessuras e tamanhos do texto).
	void resizeAndRecenter();
	void placeOn(QScreen *screen);

	QFrame *panel_ = nullptr;
	QButtonGroup *tools_ = nullptr;
	QList<QPair<QToolButton *, RgbColor>> colors_;
	QList<QPair<QToolButton *, int>> widths_;
	QList<QPair<QToolButton *, int>> textSizes_;
	QWidget *widthGroup_ = nullptr;
	QWidget *textGroup_ = nullptr;
	QToolButton *bold_ = nullptr;
	QToolButton *moreColors_ = nullptr;
	QWidget *extraColors_ = nullptr;
	QToolButton *customColor_ = nullptr;
	ColorPicker *picker_ = nullptr;
	QToolButton *fade_ = nullptr;
	QToolButton *exit_ = nullptr;
	QLabel *burned_ = nullptr;
	QLabel *hint_ = nullptr;
	QString hovered_; // dica do botão sob o mouse ("" = a dica padrão)
	QString shortcut_;
	AnnotationStyle style_;
};

} // namespace voltz
```

Substituir o conteúdo de `frontend/voltz/annotate/DrawToolbar.cpp` por:
```cpp
#include "DrawToolbar.hpp"
#include "ColorPicker.hpp"

#include <voltz/project/MonitorLocator.hpp>
#include <voltz/shell/Icons.hpp>

#include <OBSApp.hpp> // QTStr

#include <util/base.h>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMoveEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QResizeEvent>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <cmath>
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
constexpr int kTextSizes[] = {24, 36, 48, 72};
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
	{AnnotationTool::Text, "text-t", "Voltz.Draw.Tool.Text"},
};

// Cores da barra que não estão na paleta da casca.
const QColor kPanel(0x11, 0x11, 0x14);
const QColor kBorder(0x2A, 0x2A, 0x30);
const QColor kHover(0x1F, 0x1F, 0x24);

// QSS próprio: a barra é uma janela fora do #voltzShell e o tema global do OBS tem regras de QToolButton (altura máxima,
// cores) — tudo leva o prefixo #voltzDrawToolbar. %1 painel · %2 borda/separador/apertado · %3 hover · %4 texto ·
// %5 lima translúcido (marcado) · %6 lima mais forte (marcado e apertado) · %7 aviso · %8 texto secundário.
const char *kStyleSheet = R"(
#voltzDrawToolbar QFrame#drawPanel {
	background: %1;
	border: 1px solid %2;
	border-radius: 12px;
}
#voltzDrawToolbar QToolButton {
	background: transparent;
	color: %4;
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
	background: %3;
}
#voltzDrawToolbar QToolButton:pressed {
	background: %2;
}
#voltzDrawToolbar QToolButton:checked {
	background: %5;
}
#voltzDrawToolbar QToolButton:checked:pressed {
	background: %6;
}
#voltzDrawToolbar QToolButton#fadeButton {
	padding: 0 8px;
}
#voltzDrawToolbar QFrame#drawSeparator {
	background: %2;
	border: none;
	margin: 6px 4px;
}
#voltzDrawToolbar QLabel#drawBurned {
	background: transparent;
	color: %7;
	font-family: "DM Sans", "Segoe UI";
	font-size: 12px;
}
#voltzDrawToolbar QLabel#drawHint {
	background: transparent;
	color: %8;
	font-family: "DM Sans", "Segoe UI";
	font-size: 11px;
	padding: 0 4px;
}
)";

QString rgba(const QColor &color, int alpha)
{
	return QStringLiteral("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
}

QString toolbarStyleSheet()
{
	return QString::fromUtf8(kStyleSheet)
		.arg(kPanel.name(), kBorder.name(), kHover.name(), palette::kText.name(), rgba(palette::kAccent, 40),
		     rgba(palette::kAccent, 80), palette::kWarn.name(), palette::kMuted.name());
}

QColor toQColor(const RgbColor &color)
{
	return QColor(color.r, color.g, color.b);
}

bool inPalette(const RgbColor &color)
{
	return std::find(std::begin(kQuickColors), std::end(kQuickColors), color) != std::end(kQuickColors) ||
	       std::find(std::begin(kMoreColors), std::end(kMoreColors), color) != std::end(kMoreColors);
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
	painter.setPen(QPen(kBorder.lighter(140), 1.0));
	painter.setBrush(toQColor(color));
	painter.drawEllipse(QRectF(4.5, 4.5, 11.0, 11.0));
	return QIcon(pixmap);
}

// Traço horizontal da espessura (2 → 1,5 · 4 → 3 · 8 → 4,5 · 16 → 6 px lógicos): não se confunde com as bolinhas de cor.
QIcon widthIcon(int width)
{
	QPixmap pixmap(40, 40);
	pixmap.setDevicePixelRatio(2.0);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	const double thickness = std::clamp(1.5 * std::log2(static_cast<double>(std::max(width, 1))), 1.5, 6.5);
	painter.setPen(QPen(palette::kText, thickness, Qt::SolidLine, Qt::RoundCap));
	painter.drawLine(QPointF(4.0, 10.0), QPointF(16.0, 10.0));
	return QIcon(pixmap);
}

// "A" do tamanho do texto: 8, 11, 14 e 17 px lógicos (posição na lista, não o tamanho real).
QIcon textSizeIcon(int index)
{
	QPixmap pixmap(40, 40);
	pixmap.setDevicePixelRatio(2.0);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	QFont font(QStringLiteral("DM Sans"));
	font.setPixelSize(8 + 3 * index);
	font.setBold(true);
	painter.setFont(font);
	painter.setPen(palette::kText);
	painter.drawText(QRectF(0.0, 0.0, 20.0, 20.0), Qt::AlignCenter, QStringLiteral("A"));
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
	setStyleSheet(toolbarStyleSheet());

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
		button->setIcon(voltz::phosphorToggleIcon(QString::fromLatin1(entry.icon), voltz::palette::kText,
							  voltz::palette::kAccent));
		tools_->addButton(button, static_cast<int>(entry.tool));
		row->addWidget(button);
	}
	connect(tools_, &QButtonGroup::idClicked, this,
		[this](int id) { emit toolChosen(static_cast<AnnotationTool>(id)); });
	row->addWidget(separator(panel_));

	for (const RgbColor &color : kQuickColors) {
		row->addWidget(addColor(panel_, color));
	}
	moreColors_ = makeButton(panel_, QTStr("Voltz.Draw.MoreColors"));
	moreColors_->setCheckable(true);
	moreColors_->setIcon(voltz::phosphorIcon(QStringLiteral("palette"), voltz::palette::kText));
	row->addWidget(moreColors_);
	row->addWidget(separator(panel_));

	/* espessuras (traços) ou tamanhos + negrito (texto): um grupo de cada vez, conforme a ferramenta */
	widthGroup_ = new QWidget(panel_);
	auto *widthRow = new QHBoxLayout(widthGroup_);
	widthRow->setContentsMargins(0, 0, 0, 0);
	widthRow->setSpacing(2);
	for (int width : kWidths) {
		QToolButton *button = makeButton(widthGroup_, QTStr("Voltz.Draw.Width").arg(width));
		button->setCheckable(true);
		button->setIcon(widthIcon(width));
		connect(button, &QToolButton::clicked, this, [this, width]() { emit widthChosen(width); });
		widths_.append(qMakePair(button, width));
		widthRow->addWidget(button);
	}
	row->addWidget(widthGroup_);
	textGroup_ = new QWidget(panel_);
	auto *textRow = new QHBoxLayout(textGroup_);
	textRow->setContentsMargins(0, 0, 0, 0);
	textRow->setSpacing(2);
	for (int i = 0; i < static_cast<int>(std::size(kTextSizes)); i++) {
		const int size = kTextSizes[i];
		QToolButton *button = makeButton(textGroup_, QTStr("Voltz.Draw.TextSize").arg(size));
		button->setCheckable(true);
		button->setIcon(textSizeIcon(i));
		connect(button, &QToolButton::clicked, this, [this, size]() { emit textSizeChosen(size); });
		textSizes_.append(qMakePair(button, size));
		textRow->addWidget(button);
	}
	bold_ = makeButton(textGroup_, QTStr("Voltz.Draw.Bold"));
	bold_->setCheckable(true);
	bold_->setIcon(
		voltz::phosphorToggleIcon(QStringLiteral("text-b"), voltz::palette::kText, voltz::palette::kAccent));
	connect(bold_, &QToolButton::clicked, this,
		[this](bool on) { emit textFontChosen(on ? TextFont::Bold : TextFont::Regular); });
	textRow->addWidget(bold_);
	textGroup_->hide();
	row->addWidget(textGroup_);
	row->addWidget(separator(panel_));

	QToolButton *undo = makeButton(panel_, QTStr("Voltz.Draw.Undo"));
	undo->setIcon(voltz::phosphorIcon(QStringLiteral("arrow-counter-clockwise"), voltz::palette::kText));
	connect(undo, &QToolButton::clicked, this, [this]() { emit undoClicked(); });
	row->addWidget(undo);
	QToolButton *clear = makeButton(panel_, QTStr("Voltz.Draw.Clear"));
	clear->setIcon(voltz::phosphorIcon(QStringLiteral("trash"), voltz::palette::kText));
	connect(clear, &QToolButton::clicked, this, [this]() { emit clearClicked(); });
	row->addWidget(clear);
	row->addWidget(separator(panel_));

	fade_ = makeButton(panel_, QString());
	fade_->setObjectName("fadeButton");
	fade_->setIcon(voltz::phosphorIcon(QStringLiteral("timer"), voltz::palette::kText));
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
	row->addWidget(separator(panel_));

	exit_ = makeButton(panel_, QString());
	exit_->setIcon(voltz::phosphorIcon(QStringLiteral("x"), voltz::palette::kText));
	connect(exit_, &QToolButton::clicked, this, [this]() { emit exitClicked(); });
	row->addWidget(exit_);

	/* "mais cores": as 8 cores, a cor personalizada e, aberto por ela, o seletor completo */
	extraColors_ = new QWidget(panel_);
	auto *extraColumn = new QVBoxLayout(extraColors_);
	extraColumn->setContentsMargins(0, 0, 0, 0);
	extraColumn->setSpacing(6);
	auto *extraRow = new QHBoxLayout();
	extraRow->setSpacing(2);
	for (const RgbColor &color : kMoreColors) {
		extraRow->addWidget(addColor(extraColors_, color));
	}
	customColor_ = makeButton(extraColors_, QTStr("Voltz.Draw.CustomColor"));
	customColor_->setCheckable(true);
	extraRow->addWidget(customColor_);
	extraRow->addStretch(1);
	extraColumn->addLayout(extraRow);
	picker_ = new ColorPicker(extraColors_);
	picker_->hide();
	extraColumn->addWidget(picker_, 0, Qt::AlignLeft);
	extraColors_->hide();
	column->addWidget(extraColors_);
	connect(moreColors_, &QToolButton::toggled, this, [this](bool on) {
		extraColors_->setVisible(on);
		resizeAndRecenter();
	});
	connect(customColor_, &QToolButton::toggled, this, [this](bool on) {
		picker_->setVisible(on);
		resizeAndRecenter();
	});
	connect(picker_, &ColorPicker::colorPicked, this, [this](RgbColor color) { emit colorChosen(color); });

	burned_ = new QLabel(QTStr("Voltz.Draw.Burned"), panel_);
	burned_->setObjectName("drawBurned");
	burned_->setWordWrap(true);
	burned_->hide();
	column->addWidget(burned_);

	/* linha de dica: o que o botão sob o mouse faz ou, sem botão, como sair (ou digitar) */
	hint_ = new QLabel(panel_);
	hint_->setObjectName("drawHint");
	hint_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	column->addWidget(hint_);

	setExitHint(QStringLiteral("Ctrl + Shift + D"));
	setAnnotationStyle(AnnotationStyle{});
}

QToolButton *DrawToolbar::makeButton(QWidget *parent, const QString &hint)
{
	auto *button = new QToolButton(parent);
	button->setFocusPolicy(Qt::NoFocus);
	button->setIconSize(QSize(20, 20));
	button->setAutoRaise(true);
	button->installEventFilter(this); /* a dica vai para a linha de dica */
	if (!hint.isEmpty()) {
		voltz::setButtonHint(button, hint);
	}
	return button;
}

QToolButton *DrawToolbar::addColor(QWidget *parent, const RgbColor &color)
{
	QToolButton *button =
		makeButton(parent, QTStr("Voltz.Draw.Color").arg(QString::fromStdString(voltz::formatHexColor(color))));
	button->setIcon(swatchIcon(color, false));
	connect(button, &QToolButton::clicked, this, [this, color]() { emit colorChosen(color); });
	colors_.append(qMakePair(button, color));
	return button;
}

QFrame *DrawToolbar::separator(QWidget *parent)
{
	auto *line = new QFrame(parent);
	line->setObjectName("drawSeparator");
	line->setFixedWidth(1);
	return line;
}

bool DrawToolbar::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
		auto *button = qobject_cast<QToolButton *>(watched);
		hovered_ = (event->type() == QEvent::Enter && button) ? button->toolTip() : QString();
		updateHint();
	}
	return QWidget::eventFilter(watched, event);
}

void DrawToolbar::setAnnotationStyle(const AnnotationStyle &style)
{
	style_ = style;
	if (QAbstractButton *button = tools_->button(static_cast<int>(style.tool))) {
		button->setChecked(true);
	}
	markColor(style.color);
	picker_->setColor(style.color);
	for (const auto &entry : widths_) {
		entry.first->setChecked(entry.second == style.width);
	}
	for (const auto &entry : textSizes_) {
		entry.first->setChecked(entry.second == style.textSize);
	}
	bold_->setChecked(style.textFont == TextFont::Bold);
	const bool text = style.tool == AnnotationTool::Text;
	if (textGroup_->isHidden() == text) {
		widthGroup_->setVisible(!text);
		textGroup_->setVisible(text);
		resizeAndRecenter();
	}
	updateFadeText();
	updateHint();
}

void DrawToolbar::markColor(const RgbColor &color)
{
	for (const auto &entry : colors_) {
		entry.first->setIcon(swatchIcon(entry.second, entry.second == color));
	}
	/* cor fora da paleta: o botão da cor personalizada mostra a cor, marcada; senão, o conta-gotas */
	const bool custom = !inPalette(color);
	customColor_->setIcon(custom ? swatchIcon(color, true)
				     : voltz::phosphorIcon(QStringLiteral("eyedropper"), voltz::palette::kText));
	/* a cor marcada é uma das 8 extras (ou a personalizada): abre a linha delas para a marca ficar à vista */
	const bool extra = std::find(std::begin(kMoreColors), std::end(kMoreColors), color) != std::end(kMoreColors);
	if ((extra || custom) && !moreColors_->isChecked()) {
		moreColors_->setChecked(true);
	}
}

void DrawToolbar::updateFadeText()
{
	const QString value = style_.fadeSeconds > 0 ? QTStr("Voltz.Draw.Fade.Seconds").arg(style_.fadeSeconds)
						     : QTStr("Voltz.Draw.Fade.Never");
	fade_->setText(value);
	voltz::setButtonHint(fade_, QTStr("Voltz.Draw.Fade").arg(value));
}

void DrawToolbar::updateHint()
{
	if (!hovered_.isEmpty()) {
		hint_->setText(hovered_);
	} else if (style_.tool == AnnotationTool::Text) {
		hint_->setText(QTStr("Voltz.Draw.Hint.Text"));
	} else {
		hint_->setText(QTStr("Voltz.Draw.Hint.Exit").arg(shortcut_));
	}
}

void DrawToolbar::showColorPicker(bool show)
{
	moreColors_->setChecked(show || moreColors_->isChecked());
	customColor_->setChecked(show);
}

void DrawToolbar::setBurned(bool burned)
{
	burned_->setVisible(burned);
	resizeAndRecenter();
}

void DrawToolbar::resizeAndRecenter()
{
	adjustSize();
	if (isVisible()) {
		placeOn(windowHandle() ? windowHandle()->screen() : screen());
	}
}

void DrawToolbar::placeOn(QScreen *target)
{
	if (target) {
		const QRect area = target->geometry();
		move(area.x() + (area.width() - width()) / 2, area.y() + kTopMargin);
	}
}

void DrawToolbar::setExitHint(const QString &shortcut)
{
	shortcut_ = shortcut;
	voltz::setButtonHint(exit_, QTStr("Voltz.Draw.Exit").arg(shortcut));
	updateHint();
}

void DrawToolbar::showOnScreen(const QString &deviceName, bool excludeFromCapture)
{
	/* pelo HMONITOR (screenForMonitor): no Qt 6 o QScreen::name() é o nome amigável, não o "\\.\DISPLAYn" */
	QScreen *target = voltz::screenForMonitor(deviceName.toStdString());
	if (!target) {
		blog(LOG_WARNING,
		     "[voltz] barra do modo desenho: monitor '%s' nao encontrado entre as telas do Qt; vai para a "
		     "tela principal",
		     deviceName.toUtf8().constData());
		target = QGuiApplication::primaryScreen();
	}
	adjustSize();
	if (!testAttribute(Qt::WA_WState_Created)) {
		(void)winId(); /* cria a janela nativa agora: a propriedade abaixo tem de existir antes do primeiro show */
	}
	/* o OBSApp::notify não reaplica nela o "esconder da captura" do usuário (WDA_NONE com a opção desligada) */
	windowHandle()->setProperty("isOBSProjectorWindow", true);
#ifdef _WIN32
	/* sem ativar e fora da captura ANTES do primeiro show: nenhum quadro ativo nem capturado */
	const auto applyNativeStyle = [this]() {
		HWND hwnd = reinterpret_cast<HWND>(winId());
		const LONG_PTR wanted = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
		const LONG_PTR current = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
		if ((current & wanted) != wanted) {
			SetWindowLongPtrW(hwnd, GWL_EXSTYLE, current | wanted);
		}
	};
	applyNativeStyle();
	if (!SetWindowDisplayAffinity(reinterpret_cast<HWND>(winId()),
				      excludeFromCapture ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE)) {
		blog(LOG_WARNING, "[voltz] barra do modo desenho: SetWindowDisplayAffinity falhou (%lu)",
		     GetLastError());
	}
#else
	(void)excludeFromCapture;
#endif
	if (target) {
		windowHandle()->setScreen(target);
		placeOn(target);
	}
	show();
#ifdef _WIN32
	applyNativeStyle(); /* o Qt pode refazer o estilo estendido no show: confere de novo */
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

void DrawToolbar::paintEvent(QPaintEvent *event)
{
	/* alfa 1/255 em toda a janela (invisível): os cantos arredondados do painel não deixam o clique atravessar a barra
	 * (e o buraco da janela de entrada) até o app de baixo, o que tiraria o Voltz do modo desenho */
	QPainter painter(this);
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.fillRect(rect(), QColor(0, 0, 0, 1));
	painter.end();
	QWidget::paintEvent(event);
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

(Em relação ao 4C: o T em `kTools`; `kTextSizes`, `textSizeIcon`, `widthGroup_`/`textGroup_` (um de cada vez, conforme a
ferramenta), `bold_`; `customColor_` + `picker_` na linha de "mais cores" (`inPalette` decide o ícone);
`hint_` + `eventFilter` (Enter/Leave dos botões) + `updateHint`; `showColorPicker`; sinais `textSizeChosen` e
`textFontChosen`; `makeButton`/`addColor`/`separator` recebem o pai; `%8` (texto secundário) no QSS para `#drawHint`.
Comportamento de janela, foco, afinidade, posição e buraco: igual.)

- [ ] **Step 5: Modo desenho e pedido de teste**

Em `frontend/voltz/annotate/DrawModeController.cpp`:
trocar:
```cpp
		style.width = width;
		annotations_->setStyle(style);
```
por:
```cpp
		style.width = width;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::textSizeChosen, this, [this](int size) {
		AnnotationStyle style = annotations_->style();
		style.textSize = size;
		annotations_->setStyle(style);
	});
	connect(toolbar_, &DrawToolbar::textFontChosen, this, [this](TextFont font) {
		AnnotationStyle style = annotations_->style();
		style.textFont = font;
		annotations_->setStyle(style);
```

Em `frontend/voltz/annotate/AnnotationTestVendor.hpp`:
trocar:
```cpp
//   "aberto") confirma; TypeText {texto} troca o texto em andamento (o que a digitação faz); CommitText, CancelText;
//   ToolbarImage {queimadas?} → {png (base64), largura, altura, janela_nativa} — uma DrawToolbar NUNCA mostrada,
//   desenhada por QWidget::grab() com o estilo atual e o aviso de "queimadas" se o controlador está queimado ou se
//   "queimadas" vem true (só no desenho; nada muda no controlador) (janela_nativa tem de ser false: nenhuma janela nasce);
```
por:
```cpp
//   "aberto") confirma; TypeText {texto} troca o texto em andamento (o que a digitação faz); CommitText, CancelText;
//   ToolbarImage {queimadas?, ferramenta?, seletor?} → {png (base64), largura, altura, janela_nativa} — uma DrawToolbar
//   NUNCA mostrada, desenhada por QWidget::grab() com o estilo atual (a ferramenta trocada por "ferramenta", ex.
//   "texto", e o seletor de cor aberto com "seletor":true) e o aviso de "queimadas" se o controlador está queimado ou se
//   "queimadas" vem true (só no desenho; nada muda no controlador) (janela_nativa tem de ser false: nenhuma janela nasce);
```

Em `frontend/voltz/annotate/AnnotationTestVendor.cpp`:
1. trocar:
```cpp
	const bool forceBurned = obs_data_get_bool(request, "queimadas");
	struct Result {
```
por:
```cpp
	const bool forceBurned = obs_data_get_bool(request, "queimadas");
	const bool picker = obs_data_get_bool(request, "seletor");
	const std::string toolText = obs_data_get_string(request, "ferramenta");
	struct Result {
```
2. trocar:
```cpp
	std::future<void> ready = done->get_future();
	bool ok = post([result, done, forceBurned](AnnotationController *controller) {
		/* nunca mostrada: o grab() desenha fora da tela, sem criar janela nativa */
		DrawToolbar toolbar;
		toolbar.setAnnotationStyle(controller->style());
		toolbar.setBurned(forceBurned || controller->burned());
		toolbar.adjustSize();
```
por:
```cpp
	std::future<void> ready = done->get_future();
	bool ok = post([result, done, forceBurned, picker, toolText](AnnotationController *controller) {
		/* nunca mostrada: o grab() desenha fora da tela, sem criar janela nativa */
		DrawToolbar toolbar;
		AnnotationStyle style = controller->style();
		toolFromString(toolText, style.tool);
		toolbar.setAnnotationStyle(style);
		toolbar.setBurned(forceBurned || controller->burned());
		toolbar.showColorPicker(picker);
		toolbar.adjustSize();
```

Em `frontend/cmake/feature-voltz.cmake`:
trocar:
```cmake
    voltz/annotate/AnnotationTestVendor.hpp
    voltz/annotate/DrawInputWindow.cpp
```
por:
```cmake
    voltz/annotate/AnnotationTestVendor.hpp
    voltz/annotate/ColorPicker.cpp
    voltz/annotate/ColorPicker.hpp
    voltz/annotate/DrawInputWindow.cpp
```

- [ ] **Step 6: Formatar e compilar**

```bash
"$CLANGFMT" -i frontend/voltz/annotate/ColorPicker.hpp frontend/voltz/annotate/ColorPicker.cpp frontend/voltz/annotate/DrawToolbar.hpp frontend/voltz/annotate/DrawToolbar.cpp frontend/voltz/annotate/DrawModeController.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp
```
Run: CONFIGURAR; COMPILAR APP (segundo plano); RODAR TESTES; GREP DE SEGURANÇA (A–D, F).
Expected: `exit=0`; `test cases: 197 | 197 passed` (a contagem não muda nesta task); grep como em Convenções (o
`ColorPicker` não usa `grabMouse`: o arrasto usa o "grab" implícito do Qt dentro da própria janela da barra).

- [ ] **Step 7: Barra desenhada fora da tela (sem janela)**

Bloco de teste com `$ovW = 640; $ovH = 360`. No `try`, no lugar do comentário (depois do `Start-App`):
```powershell
  foreach ($case in @(@{ n = "padrao"; j = '{}' }, @{ n = "texto"; j = '{"ferramenta":"texto"}' }, @{ n = "seletor"; j = '{"seletor":true}' }, @{ n = "texto-seletor-queimadas"; j = '{"ferramenta":"texto","seletor":true,"queimadas":true}' })) {
    $bar = VendorShell "ToolbarImage" $case.j
    [IO.File]::WriteAllBytes("$sp\4d-barra-$($case.n).png", [Convert]::FromBase64String($bar.png))
    "$($case.n): $($bar.largura)x$($bar.altura) nativa=$($bar.janela_nativa)"
    Assert-DrawSafe "barra $($case.n)"
  }
  Close-App
```
Expected: quatro linhas com `nativa=False`; `padrao` com largura ≥ 400 e altura ≥ 50 (a linha de dica); `seletor` e
`texto-seletor-queimadas` com altura ≥ 150. Abra as quatro imagens e descreva-as no relatório:
- `padrao`: 7 ferramentas (o T por último), 4 cores + paleta, 4 espessuras, desfazer, limpar, "nunca", X; embaixo, em
  cinza, "Esc, clique direito ou Ctrl + Shift + D saem do modo desenho.";
- `texto`: o T marcado (lima); no lugar das espessuras, quatro "A" crescentes e o B do negrito marcado; dica "Clique onde o
  texto começa e digite. …";
- `seletor`: segunda linha com as 8 cores e o conta-gotas marcado; abaixo, o quadrado de saturação × brilho e a faixa de
  matiz, cada um com o anel da cor atual;
- `texto-seletor-queimadas`: os dois e o aviso amarelo de "queimadas".
Nenhuma barra aparece na tela durante o teste (o vigia confere).

- [ ] **Step 8: Commit**

```bash
git add frontend/voltz/annotate/ColorPicker.hpp frontend/voltz/annotate/ColorPicker.cpp frontend/voltz/annotate/DrawToolbar.hpp frontend/voltz/annotate/DrawToolbar.cpp frontend/voltz/annotate/DrawModeController.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp frontend/voltz/shell/voltz-shell.qrc frontend/voltz/shell/icons/regular/text-b.svg frontend/voltz/shell/icons/fill/text-b-fill.svg frontend/voltz/shell/icons/regular/eyedropper.svg frontend/cmake/feature-voltz.cmake frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): barra do modo desenho com texto (tamanhos e negrito), seletor de cor completo sem dialogo e linha de dica

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Núcleo — atalhos do Voltz (`hotkey-bindings`)

**Files:**
- Create: `frontend/voltz/core/hotkey-bindings.hpp`, `frontend/voltz/core/hotkey-bindings.cpp`
- Create: `frontend/voltz/tests/test-hotkey-bindings.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

Um lugar só para os atalhos do Voltz: nome, tecla padrão (sempre Ctrl+Shift+tecla, os mesmos de antes: D, M, H e F12 de
depuração; desfazer e limpar sem tecla), texto do locale e se é de depuração — na ordem de Ajustes › Atalhos. O formato
do perfil é o do OBS (lido e escrito sem libobs, testado). A regra de combinação vale para os atalhos **globais** do
Voltz: precisa de uma tecla e de Ctrl, Alt ou Windows; sozinhas (ou só com Shift), só F1–F24 e os botões laterais do
mouse. Ajustes troca só a primeira tecla de um atalho (as outras, vindas do modo clássico, ficam) e avisa conflitos.

- [ ] **Step 1: Escrever o teste que falha**

Criar `frontend/voltz/tests/test-hotkey-bindings.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/hotkey-bindings.hpp>

#include <set>
#include <string>
#include <vector>

using voltz::HotkeyComboCheck;
using voltz::KeyCombo;
using voltz::NamedBinding;

namespace {

KeyCombo combo(const char *key, bool control = false, bool shift = false, bool alt = false, bool command = false)
{
	KeyCombo out;
	out.key = key;
	out.control = control;
	out.shift = shift;
	out.alt = alt;
	out.command = command;
	return out;
}

} // namespace

TEST_CASE("atalhos do Voltz: tabela com nomes únicos e os padrões de antes (Ctrl+Shift+D/M/H; desfazer e limpar sem)")
{
	std::set<std::string> names;
	for (const voltz::VoltzHotkey &hotkey : voltz::voltzHotkeys()) {
		CHECK(std::string(hotkey.name).rfind("VoltzStudio.", 0) == 0);
		CHECK(std::string(hotkey.localeKey).rfind("Voltz.Hotkey.", 0) == 0);
		names.insert(hotkey.name);
	}
	CHECK(names.size() == voltz::voltzHotkeys().size());
	CHECK(voltz::voltzHotkeys().size() == 6);
	CHECK(std::string(voltz::voltzHotkeys().front().name) == "VoltzStudio.Draw");
	const std::vector<KeyCombo> draw = voltz::defaultHotkeyBindings("VoltzStudio.Draw");
	REQUIRE(draw.size() == 1);
	CHECK(draw[0] == combo("OBS_KEY_D", true, true));
	CHECK(voltz::defaultHotkeyBindings("VoltzStudio.AddMarker")[0].key == "OBS_KEY_M");
	CHECK(voltz::defaultHotkeyBindings("VoltzStudio.ToggleSpotlight")[0].key == "OBS_KEY_H");
	CHECK(voltz::defaultHotkeyBindings("VoltzStudio.DebugFreezeUi")[0].key == "OBS_KEY_F12");
	CHECK(voltz::defaultHotkeyBindings("VoltzStudio.UndoAnnotation").empty());
	CHECK(voltz::defaultHotkeyBindings("VoltzStudio.ClearAnnotations").empty());
	CHECK(voltz::defaultHotkeyBindings("OBSBasic.StartRecording").empty());
	REQUIRE(voltz::findVoltzHotkey("VoltzStudio.DebugFreezeUi") != nullptr);
	CHECK(voltz::findVoltzHotkey("VoltzStudio.DebugFreezeUi")->debug);
	CHECK_FALSE(voltz::findVoltzHotkey("VoltzStudio.Draw")->debug);
	CHECK(voltz::findVoltzHotkey("VoltzStudio.Nada") == nullptr);
}

TEST_CASE("hotkeyBindingsFromJson/ToJson: formato do OBS, ida e volta, tolerância")
{
	std::vector<KeyCombo> bindings;
	REQUIRE(voltz::hotkeyBindingsFromJson(
		R"({"bindings":[{"control":true,"shift":true,"key":"OBS_KEY_D"},{"alt":true,"command":true,"key":"OBS_KEY_F5"}]})",
		bindings));
	REQUIRE(bindings.size() == 2);
	CHECK(bindings[0] == combo("OBS_KEY_D", true, true));
	CHECK(bindings[1] == combo("OBS_KEY_F5", false, false, true, true));
	CHECK(voltz::hotkeyBindingsToJson(bindings) ==
	      R"({"bindings":[{"shift":true,"control":true,"key":"OBS_KEY_D"},{"alt":true,"command":true,"key":"OBS_KEY_F5"}]})");
	std::vector<KeyCombo> again;
	REQUIRE(voltz::hotkeyBindingsFromJson(voltz::hotkeyBindingsToJson(bindings), again));
	CHECK(again == bindings);

	/* lista vazia = sem atalho (válido); itens vazios ou que não são objeto saem; só modificador fica */
	REQUIRE(voltz::hotkeyBindingsFromJson(R"({"bindings":[]})", again));
	CHECK(again.empty());
	REQUIRE(voltz::hotkeyBindingsFromJson(
		R"({"bindings":[1,{},{"key":"OBS_KEY_NONE"},{"control":"sim","shift":true}]})", again));
	REQUIRE(again.size() == 1);
	CHECK(again[0] == combo("", false, true));
	CHECK(voltz::hotkeyBindingsToJson(again) == R"({"bindings":[{"shift":true,"key":"OBS_KEY_NONE"}]})");
	CHECK(voltz::hotkeyBindingsToJson({}) == R"({"bindings":[]})");

	CHECK_FALSE(voltz::hotkeyBindingsFromJson("lixo", again));
	CHECK_FALSE(voltz::hotkeyBindingsFromJson("[]", again));
	CHECK_FALSE(voltz::hotkeyBindingsFromJson(R"({"teclas":[]})", again));
	CHECK(again.empty());
}

TEST_CASE("checkVoltzHotkeyCombo: precisa de tecla e de Ctrl, Alt ou Windows; F1–F24 e mouse lateral sozinhos")
{
	CHECK(voltz::checkVoltzHotkeyCombo(KeyCombo{}) == HotkeyComboCheck::Empty);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("", true, true)) == HotkeyComboCheck::NeedsKey);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_NONE", true)) == HotkeyComboCheck::NeedsKey);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_D")) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_D", false, true)) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_ESCAPE")) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_MOUSE3")) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_D", true)) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_D", false, false, true)) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_D", false, false, false, true)) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_F1")) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_F24", false, true)) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_F25")) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_FN")) == HotkeyComboCheck::NeedsModifier);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_MOUSE4")) == HotkeyComboCheck::Ok);
	CHECK(voltz::checkVoltzHotkeyCombo(combo("OBS_KEY_MOUSE29")) == HotkeyComboCheck::Ok);
}

TEST_CASE("replaceFirstBinding: troca a primeira, mantém as outras (sem repetir), vazia tira só a primeira")
{
	const KeyCombo a = combo("OBS_KEY_A", true);
	const KeyCombo b = combo("OBS_KEY_B", true);
	const KeyCombo c = combo("OBS_KEY_C", true);
	CHECK(voltz::replaceFirstBinding({}, a) == std::vector<KeyCombo>{a});
	CHECK(voltz::replaceFirstBinding({a, b}, c) == std::vector<KeyCombo>{c, b});
	CHECK(voltz::replaceFirstBinding({a, b}, b) == std::vector<KeyCombo>{b});
	CHECK(voltz::replaceFirstBinding({a, b}, KeyCombo{}) == std::vector<KeyCombo>{b});
	CHECK(voltz::replaceFirstBinding({a}, KeyCombo{}).empty());
}

TEST_CASE("hotkeyConflicts: mesma combinação em outro atalho; o próprio e as vazias não contam")
{
	const KeyCombo d = combo("OBS_KEY_D", true, true);
	const KeyCombo r = combo("OBS_KEY_R", true);
	const std::vector<NamedBinding> all = {{"VoltzStudio.Draw", d},
					       {"OBSBasic.StartRecording", r},
					       {"OBSBasic.Screenshot", d},
					       {"OBSBasic.Screenshot", d},
					       {"OBSBasic.Vazio", KeyCombo{}}};
	CHECK(voltz::hotkeyConflicts("VoltzStudio.Draw", {d}, all) == std::vector<std::string>{"OBSBasic.Screenshot"});
	CHECK(voltz::hotkeyConflicts("VoltzStudio.AddMarker", {r, d}, all) ==
	      std::vector<std::string>{"VoltzStudio.Draw", "OBSBasic.StartRecording", "OBSBasic.Screenshot"});
	CHECK(voltz::hotkeyConflicts("VoltzStudio.Draw", {KeyCombo{}}, all).empty());
	CHECK(voltz::hotkeyConflicts("VoltzStudio.Draw", {}, all).empty());
}

TEST_CASE("KeyCombo: OBS_KEY_NONE vale como sem tecla; igualdade compara modificadores e tecla")
{
	CHECK_FALSE(combo("OBS_KEY_NONE").hasKey());
	CHECK(combo("OBS_KEY_NONE").empty());
	CHECK(combo("OBS_KEY_NONE", true) == combo("", true));
	CHECK_FALSE(combo("OBS_KEY_D", true) == combo("OBS_KEY_D", true, true));
	CHECK_FALSE(combo("OBS_KEY_D", true) == combo("OBS_KEY_E", true));
	CHECK(combo("OBS_KEY_D", true).hasModifier());
}
```

- [ ] **Step 2: Ligar ao CMake e ver falhar**

Em `frontend/voltz/core/CMakeLists.txt`:
1. trocar:
```cmake
    draw-session.cpp
    iso-encoders.cpp
```
por:
```cmake
    draw-session.cpp
    hotkey-bindings.cpp
    iso-encoders.cpp
```
2. trocar:
```cmake
    draw-session.hpp
    iso-encoders.hpp
```
por:
```cmake
    draw-session.hpp
    hotkey-bindings.hpp
    iso-encoders.hpp
```

Em `frontend/voltz/tests/CMakeLists.txt`:
trocar:
```cmake
    test-draw-text.cpp
    test-iso-encoders.cpp
```
por:
```cmake
    test-draw-text.cpp
    test-hotkey-bindings.cpp
    test-iso-encoders.cpp
```

Run: CONFIGURAR; COMPILAR TESTES.
Expected: FALHA (`hotkey-bindings.hpp` não existe).

- [ ] **Step 3: Implementação**

Criar `frontend/voltz/core/hotkey-bindings.hpp`:
```cpp
#pragma once

#include <string>
#include <vector>

namespace voltz {

// Uma combinação de teclas no formato do OBS (perfil [Hotkeys] <nome> = {"bindings":[...]}, o mesmo do obs_hotkey_save
// e do OBSBasicSettings): modificadores ligados e o nome da tecla ("OBS_KEY_D", "OBS_KEY_F12", "OBS_KEY_MOUSE4").
struct KeyCombo {
	bool shift = false;
	bool control = false;
	bool alt = false;
	bool command = false; // tecla Windows
	std::string key;      // "" ou "OBS_KEY_NONE" = só modificadores

	bool hasKey() const;
	bool hasModifier() const { return shift || control || alt || command; }
	bool empty() const { return !hasKey() && !hasModifier(); }
};

bool operator==(const KeyCombo &a, const KeyCombo &b);

// Atalhos do Voltz (obs_hotkey_register_frontend): nome, tecla padrão (sempre Ctrl+Shift+tecla) ou nenhuma, texto do
// locale e se só existe em depuração (VOLTZ_DEBUG_FREEZE_UI). Na ordem de Ajustes › Atalhos.
struct VoltzHotkey {
	const char *name;
	const char *defaultKey; // nullptr = sem tecla padrão
	const char *localeKey;
	bool debug;
};

const std::vector<VoltzHotkey> &voltzHotkeys();
// nullptr se `name` não é um atalho do Voltz.
const VoltzHotkey *findVoltzHotkey(const std::string &name);
// Teclas padrão (Ctrl+Shift+defaultKey) ou nenhuma (sem padrão ou nome desconhecido).
std::vector<KeyCombo> defaultHotkeyBindings(const std::string &name);

// {"bindings":[{"control":true,"shift":true,"key":"OBS_KEY_D"},...]} → combinações (itens vazios ou que não são objeto
// saem). Falso para JSON inválido, que não é objeto ou sem a lista "bindings" (aí `out` fica vazio).
bool hotkeyBindingsFromJson(const std::string &json, std::vector<KeyCombo> &out);
// Combinações → o mesmo formato (só os modificadores ligados, na ordem shift, control, alt, command; depois "key").
std::string hotkeyBindingsToJson(const std::vector<KeyCombo> &bindings);

enum class HotkeyComboCheck { Ok, Empty, NeedsKey, NeedsModifier };

// Pode ser atalho do Voltz? Precisa de uma tecla (não só modificadores) e de Ctrl, Alt ou Windows; sem eles (ou só com
// Shift) só F1–F24 e os botões laterais do mouse (MOUSE4 em diante). Os atalhos do OBS não consomem a tecla: "D"
// sozinho (ou Shift+D) ligaria o modo desenho no meio de qualquer digitação. Vazia = Empty (tirar o atalho é válido).
HotkeyComboCheck checkVoltzHotkeyCombo(const KeyCombo &combo);

// Troca a primeira combinação pela nova (ou a acrescenta, se não há nenhuma); as outras ficam — vieram das
// Configurações do OBS (modo clássico), que aceitam várias. Nova vazia = tira só a primeira.
std::vector<KeyCombo> replaceFirstBinding(const std::vector<KeyCombo> &bindings, const KeyCombo &combo);

struct NamedBinding {
	std::string name; // nome do atalho no OBS ("VoltzStudio.Draw", "OBSBasic.StartRecording", ...)
	KeyCombo combo;
};

// Nomes dos OUTROS atalhos (não `name`) que usam alguma das combinações não vazias de `bindings`, sem repetir, na ordem
// em que aparecem em `all`.
std::vector<std::string> hotkeyConflicts(const std::string &name, const std::vector<KeyCombo> &bindings,
					 const std::vector<NamedBinding> &all);

} // namespace voltz
```

Criar `frontend/voltz/core/hotkey-bindings.cpp`:
```cpp
#include "hotkey-bindings.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace voltz {

namespace {

using Json = nlohmann::ordered_json;

// "OBS_KEY_F12" → 12 com prefixo "OBS_KEY_F"; 0 se não for prefixo + número.
int numberAfter(const std::string &key, const char *prefix)
{
	const std::string start(prefix);
	if (key.size() <= start.size() || key.compare(0, start.size(), start) != 0) {
		return 0;
	}
	const std::string digits = key.substr(start.size());
	if (digits.size() > 3 || !std::all_of(digits.begin(), digits.end(), [](char c) {
		    return std::isdigit(static_cast<unsigned char>(c)) != 0;
	    })) {
		return 0;
	}
	return std::atoi(digits.c_str());
}

bool readFlag(const Json &object, const char *key)
{
	const auto it = object.find(key);
	return it != object.end() && it->is_boolean() && it->get<bool>();
}

} // namespace

bool KeyCombo::hasKey() const
{
	return !key.empty() && key != "OBS_KEY_NONE";
}

bool operator==(const KeyCombo &a, const KeyCombo &b)
{
	return a.shift == b.shift && a.control == b.control && a.alt == b.alt && a.command == b.command &&
	       (a.hasKey() ? a.key : std::string()) == (b.hasKey() ? b.key : std::string());
}

const std::vector<VoltzHotkey> &voltzHotkeys()
{
	static const std::vector<VoltzHotkey> hotkeys = {
		{"VoltzStudio.Draw", "OBS_KEY_D", "Voltz.Hotkey.Draw", false},
		{"VoltzStudio.UndoAnnotation", nullptr, "Voltz.Hotkey.UndoAnnotation", false},
		{"VoltzStudio.ClearAnnotations", nullptr, "Voltz.Hotkey.ClearAnnotations", false},
		{"VoltzStudio.AddMarker", "OBS_KEY_M", "Voltz.Hotkey.AddMarker", false},
		{"VoltzStudio.ToggleSpotlight", "OBS_KEY_H", "Voltz.Hotkey.ToggleSpotlight", false},
		{"VoltzStudio.DebugFreezeUi", "OBS_KEY_F12", "Voltz.Hotkey.DebugFreezeUi", true},
	};
	return hotkeys;
}

const VoltzHotkey *findVoltzHotkey(const std::string &name)
{
	for (const VoltzHotkey &hotkey : voltzHotkeys()) {
		if (name == hotkey.name) {
			return &hotkey;
		}
	}
	return nullptr;
}

std::vector<KeyCombo> defaultHotkeyBindings(const std::string &name)
{
	const VoltzHotkey *hotkey = findVoltzHotkey(name);
	if (!hotkey || !hotkey->defaultKey) {
		return {};
	}
	KeyCombo combo;
	combo.control = true;
	combo.shift = true;
	combo.key = hotkey->defaultKey;
	return {combo};
}

bool hotkeyBindingsFromJson(const std::string &json, std::vector<KeyCombo> &out)
{
	out.clear();
	const Json root = Json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		return false;
	}
	const auto list = root.find("bindings");
	if (list == root.end() || !list->is_array()) {
		return false;
	}
	for (const Json &item : *list) {
		if (!item.is_object()) {
			continue;
		}
		KeyCombo combo;
		combo.shift = readFlag(item, "shift");
		combo.control = readFlag(item, "control");
		combo.alt = readFlag(item, "alt");
		combo.command = readFlag(item, "command");
		const auto key = item.find("key");
		if (key != item.end() && key->is_string()) {
			combo.key = key->get<std::string>();
		}
		if (!combo.empty()) {
			out.push_back(combo);
		}
	}
	return true;
}

std::string hotkeyBindingsToJson(const std::vector<KeyCombo> &bindings)
{
	Json list = Json::array();
	for (const KeyCombo &combo : bindings) {
		if (combo.empty()) {
			continue;
		}
		Json item = Json::object();
		if (combo.shift) {
			item["shift"] = true;
		}
		if (combo.control) {
			item["control"] = true;
		}
		if (combo.alt) {
			item["alt"] = true;
		}
		if (combo.command) {
			item["command"] = true;
		}
		item["key"] = combo.hasKey() ? combo.key : std::string("OBS_KEY_NONE");
		list.push_back(item);
	}
	Json root = Json::object();
	root["bindings"] = list;
	return root.dump();
}

HotkeyComboCheck checkVoltzHotkeyCombo(const KeyCombo &combo)
{
	if (combo.empty()) {
		return HotkeyComboCheck::Empty;
	}
	if (!combo.hasKey()) {
		return HotkeyComboCheck::NeedsKey;
	}
	if (combo.control || combo.alt || combo.command) {
		return HotkeyComboCheck::Ok;
	}
	const int function = numberAfter(combo.key, "OBS_KEY_F");
	const int mouse = numberAfter(combo.key, "OBS_KEY_MOUSE");
	if ((function >= 1 && function <= 24) || mouse >= 4) {
		return HotkeyComboCheck::Ok;
	}
	return HotkeyComboCheck::NeedsModifier;
}

std::vector<KeyCombo> replaceFirstBinding(const std::vector<KeyCombo> &bindings, const KeyCombo &combo)
{
	std::vector<KeyCombo> out;
	if (!combo.empty()) {
		out.push_back(combo);
	}
	for (size_t i = 1; i < bindings.size(); i++) {
		if (!bindings[i].empty() && !(bindings[i] == combo)) {
			out.push_back(bindings[i]);
		}
	}
	return out;
}

std::vector<std::string> hotkeyConflicts(const std::string &name, const std::vector<KeyCombo> &bindings,
					 const std::vector<NamedBinding> &all)
{
	std::vector<std::string> out;
	for (const NamedBinding &other : all) {
		if (other.name == name || other.combo.empty() ||
		    std::find(out.begin(), out.end(), other.name) != out.end()) {
			continue;
		}
		for (const KeyCombo &combo : bindings) {
			if (!combo.empty() && combo == other.combo) {
				out.push_back(other.name);
				break;
			}
		}
	}
	return out;
}

} // namespace voltz
```

- [ ] **Step 4: Rodar e ver passar**

```bash
"$CLANGFMT" -i frontend/voltz/core/hotkey-bindings.hpp frontend/voltz/core/hotkey-bindings.cpp frontend/voltz/tests/test-hotkey-bindings.cpp
```
Run: COMPILAR TESTES; RODAR TESTES; `build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe | tail -3`.
Expected: `test cases: 203 | 203 passed` (1768 asserções).

- [ ] **Step 5: Commit**

```bash
git add frontend/voltz/core/hotkey-bindings.hpp frontend/voltz/core/hotkey-bindings.cpp frontend/voltz/tests/test-hotkey-bindings.cpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): atalhos do Voltz no nucleo (tabela com os padroes, formato do perfil do OBS, regra de combinacao e conflitos)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 8: Casca — atalhos num lugar só e Ajustes › Atalhos

**Files:**
- Create: `frontend/voltz/shell/HotkeySettings.hpp`, `frontend/voltz/shell/HotkeySettings.cpp`
- Modify (substituir): `frontend/voltz/shell/HotkeyBinding.hpp`, `frontend/voltz/shell/HotkeyBinding.cpp`
- Modify: `frontend/voltz/annotate/DrawModeController.cpp`, `frontend/voltz/shell/MouseStyleStore.cpp`,
  `frontend/voltz/project/ProjectRecorder.cpp`, `frontend/voltz/annotate/AnnotationTestVendor.hpp`,
  `frontend/voltz/annotate/AnnotationTestVendor.cpp`, `frontend/voltz/shell/SettingsPage.hpp`,
  `frontend/voltz/shell/SettingsPage.cpp`, `frontend/voltz/shell/voltz-shell.qss`, `frontend/cmake/feature-voltz.cmake`,
  `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`

`HotkeyBinding` passa a carregar **e** gravar pelo núcleo (padrões da tabela, JSON do perfil) e a ler os atalhos vivos
(`currentHotkeyBindings`, `findHotkey`, `allHotkeyBindings`, `hotkeyDescription`); o marcador (`ProjectRecorder`) e o
holofote (`MouseStyleStore`) deixam o código próprio de leitura do perfil. Ajustes ganha a seção **Atalhos**, entre Mouse
e Sobre: uma linha por atalho do Voltz registrado (o de depuração só com `VOLTZ_DEBUG_FREEZE_UI`), com o `OBSHotkeyEdit`
do OBS (sem `OBSBasicSettings`), "Limpar atalho" e "Restaurar padrão" (nomes acessíveis "Limpar atalho — <atalho>" e
"Restaurar padrão — <atalho>", usados pelo smoke), aviso de combinação inválida e de conflito. Com o foco no campo, os
atalhos do OBS ficam pausados (log `[voltz] atalhos: editando (atalhos pausados)` e `... edicao terminada (atalhos de
volta)`); o campo só ganha foco por clique. Os textos que mandavam trocar o atalho "em Configurações → Atalhos (modo
clássico)" passam a apontar para Ajustes › Atalhos. O pedido de teste `HotkeyState` devolve as teclas de agora.

- [ ] **Step 1: `HotkeyBinding` pelo núcleo**

Substituir o conteúdo de `frontend/voltz/shell/HotkeyBinding.hpp` por:
```cpp
#pragma once

#include <voltz/core/hotkey-bindings.hpp>

#include <obs.h>

#include <QString>

#include <vector>

namespace voltz {

// Atalhos do Voltz no libobs e no perfil (Plano 4D: um lugar só para o marcador, o holofote e o modo desenho). Formato
// do OBS: perfil [Hotkeys] <nome> = {"bindings":[...]} (o do OBSBasic::InitHotkeys e do OBSBasicSettings, que continua
// editando os mesmos atalhos no modo clássico). Thread da UI.

// Carrega as teclas do atalho `name` (VoltzStudio.*) do perfil. Nada salvo: as teclas padrão da tabela do núcleo
// (voltzHotkeys). Lista vazia salva = sem atalho. JSON inválido = padrão, com aviso no log.
void loadHotkeyBinding(obs_hotkey_id id, const char *name);
// Grava `bindings` no atalho (vale na hora) e no perfil (config_save_safe); log "[voltz] atalhos: <nome> = <teclas>".
void saveHotkeyBinding(obs_hotkey_id id, const char *name, const std::vector<KeyCombo> &bindings);
// As teclas do atalho agora (obs_enum_hotkey_bindings), na ordem do libobs.
std::vector<KeyCombo> currentHotkeyBindings(obs_hotkey_id id);
// O atalho registrado com esse nome (OBS_INVALID_HOTKEY_ID se não há: atalho de depuração sem a variável).
obs_hotkey_id findHotkey(const char *name);
// Todas as teclas de todos os atalhos registrados (do OBS e do Voltz), com o nome de cada um (para os conflitos).
std::vector<NamedBinding> allHotkeyBindings();
// Descrição do atalho registrado com esse nome ("" se não há).
QString hotkeyDescription(const char *name);

KeyCombo fromObsCombination(obs_key_combination_t combination);
obs_key_combination_t toObsCombination(const KeyCombo &combo);
// "Ctrl + Shift + D" (texto do OBS, no idioma e no teclado do usuário); vazio para combinação vazia.
QString keyComboText(const KeyCombo &combo);
// Teclas do atalho em texto ("Ctrl + Shift + D"; vários separados por ", "); vazio sem atalho.
QString hotkeyShortcutText(obs_hotkey_id id);

} // namespace voltz
```

Substituir o conteúdo de `frontend/voltz/shell/HotkeyBinding.cpp` por:
```cpp
#include "HotkeyBinding.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>
#include <util/config-file.h>
#include <util/dstr.h>

#include <QStringList>

#include <cstring>
#include <string>

namespace voltz {

namespace {

std::vector<obs_key_combination_t> toObsCombinations(const std::vector<KeyCombo> &bindings)
{
	std::vector<obs_key_combination_t> out;
	for (const KeyCombo &combo : bindings) {
		if (!combo.empty()) {
			out.push_back(toObsCombination(combo));
		}
	}
	return out;
}

} // namespace

KeyCombo fromObsCombination(obs_key_combination_t combination)
{
	KeyCombo combo;
	combo.shift = (combination.modifiers & INTERACT_SHIFT_KEY) != 0;
	combo.control = (combination.modifiers & INTERACT_CONTROL_KEY) != 0;
	combo.alt = (combination.modifiers & INTERACT_ALT_KEY) != 0;
	combo.command = (combination.modifiers & INTERACT_COMMAND_KEY) != 0;
	if (combination.key != OBS_KEY_NONE) {
		const char *name = obs_key_to_name(combination.key);
		combo.key = name ? name : "";
	}
	return combo;
}

obs_key_combination_t toObsCombination(const KeyCombo &combo)
{
	obs_key_combination_t combination = {};
	combination.modifiers = (combo.shift ? INTERACT_SHIFT_KEY : 0u) | (combo.control ? INTERACT_CONTROL_KEY : 0u) |
				(combo.alt ? INTERACT_ALT_KEY : 0u) | (combo.command ? INTERACT_COMMAND_KEY : 0u);
	combination.key = combo.hasKey() ? obs_key_from_name(combo.key.c_str()) : OBS_KEY_NONE;
	return combination;
}

void loadHotkeyBinding(obs_hotkey_id id, const char *name)
{
	if (id == OBS_INVALID_HOTKEY_ID || !name) {
		return;
	}
	config_t *profile = obs_frontend_get_profile_config();
	const char *json = profile ? config_get_string(profile, "Hotkeys", name) : nullptr;
	std::vector<KeyCombo> bindings;
	if (!json || !*json) {
		bindings = defaultHotkeyBindings(name);
	} else if (!hotkeyBindingsFromJson(json, bindings)) {
		blog(LOG_WARNING, "[voltz] atalhos: %s invalido no perfil; usando o padrao", name);
		bindings = defaultHotkeyBindings(name);
	}
	std::vector<obs_key_combination_t> combinations = toObsCombinations(bindings);
	obs_hotkey_load_bindings(id, combinations.data(), combinations.size());
}

void saveHotkeyBinding(obs_hotkey_id id, const char *name, const std::vector<KeyCombo> &bindings)
{
	if (id == OBS_INVALID_HOTKEY_ID || !name) {
		return;
	}
	std::vector<obs_key_combination_t> combinations = toObsCombinations(bindings);
	obs_hotkey_load_bindings(id, combinations.data(), combinations.size());
	const std::string json = hotkeyBindingsToJson(bindings);
	config_t *profile = obs_frontend_get_profile_config();
	if (profile) {
		config_set_string(profile, "Hotkeys", name, json.c_str());
		config_save_safe(profile, "tmp", nullptr);
	}
	blog(LOG_INFO, "[voltz] atalhos: %s = %s", name, json.c_str());
}

std::vector<KeyCombo> currentHotkeyBindings(obs_hotkey_id id)
{
	struct Search {
		obs_hotkey_id id;
		std::vector<KeyCombo> combos;
	} search{id, {}};
	if (id == OBS_INVALID_HOTKEY_ID) {
		return {};
	}
	obs_enum_hotkey_bindings(
		[](void *data, size_t, obs_hotkey_binding_t *binding) {
			auto *found = static_cast<Search *>(data);
			if (obs_hotkey_binding_get_hotkey_id(binding) == found->id) {
				found->combos.push_back(
					fromObsCombination(obs_hotkey_binding_get_key_combination(binding)));
			}
			return true;
		},
		&search);
	return search.combos;
}

obs_hotkey_id findHotkey(const char *name)
{
	struct Search {
		const char *name;
		obs_hotkey_id id;
	} search{name, OBS_INVALID_HOTKEY_ID};
	if (!name) {
		return OBS_INVALID_HOTKEY_ID;
	}
	obs_enum_hotkeys(
		[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey) {
			auto *found = static_cast<Search *>(data);
			const char *current = obs_hotkey_get_name(hotkey);
			if (current && std::strcmp(current, found->name) == 0) {
				found->id = id;
				return false;
			}
			return true;
		},
		&search);
	return search.id;
}

std::vector<NamedBinding> allHotkeyBindings()
{
	std::vector<NamedBinding> all;
	obs_enum_hotkey_bindings(
		[](void *data, size_t, obs_hotkey_binding_t *binding) {
			auto *out = static_cast<std::vector<NamedBinding> *>(data);
			const obs_hotkey_t *hotkey = obs_hotkey_binding_get_hotkey(binding);
			const char *name = hotkey ? obs_hotkey_get_name(hotkey) : nullptr;
			out->push_back(NamedBinding{
				name ? name : "", fromObsCombination(obs_hotkey_binding_get_key_combination(binding))});
			return true;
		},
		&all);
	return all;
}

QString hotkeyDescription(const char *name)
{
	struct Search {
		const char *name;
		QString description;
	} search{name, {}};
	if (!name) {
		return {};
	}
	obs_enum_hotkeys(
		[](void *data, obs_hotkey_id, obs_hotkey_t *hotkey) {
			auto *found = static_cast<Search *>(data);
			const char *current = obs_hotkey_get_name(hotkey);
			if (current && std::strcmp(current, found->name) == 0) {
				const char *description = obs_hotkey_get_description(hotkey);
				found->description = QString::fromUtf8(description ? description : current);
				return false;
			}
			return true;
		},
		&search);
	return search.description;
}

QString keyComboText(const KeyCombo &combo)
{
	if (combo.empty()) {
		return {};
	}
	struct dstr text = {};
	obs_key_combination_to_str(toObsCombination(combo), &text);
	const QString out = QString::fromUtf8(text.array ? text.array : "");
	dstr_free(&text);
	return out;
}

QString hotkeyShortcutText(obs_hotkey_id id)
{
	QStringList keys;
	for (const KeyCombo &combo : currentHotkeyBindings(id)) {
		const QString text = keyComboText(combo);
		if (!text.isEmpty()) {
			keys.append(text);
		}
	}
	return keys.join(QStringLiteral(", "));
}

} // namespace voltz
```

- [ ] **Step 2: Modo desenho, holofote e marcador usam o mesmo carregamento**

Em `frontend/voltz/annotate/DrawModeController.cpp`:
trocar:
```cpp
{
	/* Desfazer e limpar sem tecla padrão: atalhos do OBS não consomem a tecla (Ctrl+Shift+Z iria também para o app em
	 * uso). Dentro do modo desenho, Ctrl+Z desfaz pela janela de entrada. */
	voltz::loadHotkeyBinding(drawHotkey_, kDrawHotkeyName, "OBS_KEY_D");
	voltz::loadHotkeyBinding(undoHotkey_, kUndoHotkeyName, nullptr);
	voltz::loadHotkeyBinding(clearHotkey_, kClearHotkeyName, nullptr);
	voltz::loadHotkeyBinding(freezeHotkey_, kFreezeHotkeyName, "OBS_KEY_F12");
}
```
por:
```cpp
{
	/* Padrões na tabela do núcleo (hotkey-bindings): Ctrl+Shift+D e Ctrl+Shift+F12; desfazer e limpar sem tecla —
	 * atalhos do OBS não consomem a tecla (Ctrl+Shift+Z iria também para o app em uso). Dentro do modo desenho, Ctrl+Z
	 * desfaz pela janela de entrada. */
	voltz::loadHotkeyBinding(drawHotkey_, kDrawHotkeyName);
	voltz::loadHotkeyBinding(undoHotkey_, kUndoHotkeyName);
	voltz::loadHotkeyBinding(clearHotkey_, kClearHotkeyName);
	voltz::loadHotkeyBinding(freezeHotkey_, kFreezeHotkeyName);
}
```

Em `frontend/voltz/shell/MouseStyleStore.cpp`:
1. trocar:
```cpp
#include "MouseStyleStore.hpp"

#include <OBSApp.hpp> // QTStr, App()
```
por:
```cpp
#include "MouseStyleStore.hpp"
#include "HotkeyBinding.hpp"

#include <OBSApp.hpp> // QTStr, App()
```
2. trocar:
```cpp
#include <util/config-file.h>
#include <util/dstr.h>

#include <QStringList>

#include <string>
```
por:
```cpp
#include <util/config-file.h>

#include <string>
```
3. trocar:
```cpp
{
	struct Search {
		obs_hotkey_id id;
		QStringList keys;
	} search{spotlightHotkey_, {}};
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
```
por:
```cpp
{
	return voltz::hotkeyShortcutText(spotlightHotkey_);
}
```
4. trocar:
```cpp
	}
	/* Mesmo formato do OBS (perfil [Hotkeys] <nome> = {"bindings":[...]}). Nada salvo: Ctrl+Shift+H. Lista vazia
	 * salva = sem atalho. */
	config_t *profile = obs_frontend_get_profile_config();
	const char *json = profile ? config_get_string(profile, "Hotkeys", kSpotlightHotkeyName) : nullptr;
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
		obs_data_set_string(combo, "key", "OBS_KEY_H");
		obs_data_array_push_back(bindings, combo);
	}
	obs_hotkey_load(spotlightHotkey_, bindings);
}
```
por:
```cpp
	}
	/* perfil [Hotkeys] (formato do OBS); nada salvo: Ctrl+Shift+H (tabela do núcleo, hotkey-bindings) */
	voltz::loadHotkeyBinding(spotlightHotkey_, kSpotlightHotkeyName);
}
```

Em `frontend/voltz/project/ProjectRecorder.cpp`:
1. trocar:
```cpp
#include <voltz/core/project-naming.hpp>
#include <voltz/shell/MouseStyleStore.hpp>
```
por:
```cpp
#include <voltz/core/project-naming.hpp>
#include <voltz/shell/HotkeyBinding.hpp>
#include <voltz/shell/MouseStyleStore.hpp>
```
2. trocar:
```cpp
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
```
por:
```cpp
	}
	/* perfil [Hotkeys] (formato do OBS); nada salvo: Ctrl+Shift+M (tabela do núcleo, hotkey-bindings) */
	voltz::loadHotkeyBinding(markerHotkey_, kMarkerHotkeyName);
}
```

- [ ] **Step 3: Seção Ajustes › Atalhos**

Criar `frontend/voltz/shell/HotkeySettings.hpp`:
```cpp
#pragma once

#include <voltz/core/hotkey-bindings.hpp>

#include <QFrame>
#include <QList>

#include <vector>

class QLabel;
class QToolButton;

namespace voltz {

class VoltzHotkeyEdit;

// Ajustes › Atalhos (spec §4.3, Plano 4D): os atalhos do Voltz registrados (desenhar, desfazer, limpar, marcador,
// holofote e, com VOLTZ_DEBUG_FREEZE_UI, o de depuração), cada um com o campo de teclas do OBS (OBSHotkeyEdit,
// reaproveitado sem patch), "Limpar" e "Restaurar padrão". Grava na hora (perfil [Hotkeys], o mesmo das Configurações do
// OBS, que continuam editando os mesmos atalhos no modo clássico) só combinações aceitas pelo checkVoltzHotkeyCombo do
// núcleo, trocando a primeira tecla do atalho (as outras, vindas do modo clássico, ficam); mostra os conflitos com
// qualquer outro atalho. Enquanto o campo está ativo os atalhos do OBS ficam pausados (senão apertar Ctrl+Shift+D para
// gravá-lo ligaria o modo desenho). Os rótulos ficam em formLabels() para o SettingsPage alinhar as colunas. Thread da UI.
class HotkeySettingsSection : public QFrame {
public:
	explicit HotkeySettingsSection(QWidget *parent = nullptr);

	const QList<QLabel *> &formLabels() const { return formLabels_; }
	// Relê os atalhos (podem ter mudado nas Configurações do OBS ou ao trocar de perfil).
	void reload();

private:
	struct Row {
		const char *name = nullptr;
		QString title;
		VoltzHotkeyEdit *edit = nullptr;
		QToolButton *clear = nullptr;
		QToolButton *reset = nullptr;
		QLabel *status = nullptr;
	};

	// O campo mudou (a pessoa apertou teclas): grava se a combinação vale; senão explica.
	void keyChanged(size_t index);
	void save(size_t index, const std::vector<KeyCombo> &bindings);
	// Campo, conflitos e "+N" a partir do atalho de agora.
	void refreshRow(size_t index);

	std::vector<Row> rows_;
	QList<QLabel *> formLabels_;
	bool loading_ = false;
};

} // namespace voltz
```

Criar `frontend/voltz/shell/HotkeySettings.cpp`:
```cpp
#include "HotkeySettings.hpp"

#include "HotkeyBinding.hpp"
#include "Icons.hpp"

#include <OBSApp.hpp> // App(), QTStr
#include <settings/OBSHotkeyEdit.hpp>

#include <QFocusEvent>
#include <QGridLayout>
#include <QLabel>
#include <QStringList>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>
#include <string>
#include <utility>

namespace voltz {

// O campo de teclas do OBS (o mesmo das Configurações → Atalhos), sem OBSBasicSettings: `settings` nulo só seria usado
// pelo aviso de atalho duplicado do OBS (UpdateDuplicationState), que o Voltz nunca chama. Enquanto ele tem o foco, os
// atalhos do OBS ficam pausados (OBSApp::DisableHotkeys, como a janela de adicionar fonte do OBS faz) e voltam ao sair
// (UpdateHotkeyFocusSetting) — ou ao ser destruído.
class VoltzHotkeyEdit : public OBSHotkeyEdit {
public:
	VoltzHotkeyEdit(QWidget *parent, obs_key_combination_t original) : OBSHotkeyEdit(parent, original, nullptr)
	{
		setObjectName("hotkeyEdit");
		/* só o clique dá o foco (nunca o Tab nem a troca de página): pausar os atalhos é sempre um gesto explícito */
		setFocusPolicy(Qt::ClickFocus);
	}

	~VoltzHotkeyEdit() override { resume(); }

	std::function<void()> focusLeft;

protected:
	void focusInEvent(QFocusEvent *event) override
	{
		OBSHotkeyEdit::focusInEvent(event);
		if (!paused_) {
			paused_ = true;
			App()->DisableHotkeys();
			blog(LOG_INFO, "[voltz] atalhos: editando (atalhos pausados)");
		}
	}

	void focusOutEvent(QFocusEvent *event) override
	{
		resume();
		OBSHotkeyEdit::focusOutEvent(event);
		if (focusLeft) {
			focusLeft();
		}
	}

private:
	void resume()
	{
		if (paused_) {
			paused_ = false;
			App()->UpdateHotkeyFocusSetting();
			blog(LOG_INFO, "[voltz] atalhos: edicao terminada (atalhos de volta)");
		}
	}

	bool paused_ = false;
};

namespace {

// "Voltz Studio: ligar/desligar modo desenho" → "Ligar/desligar modo desenho"
QString shortTitle(const QString &description)
{
	QString title = description.contains(QStringLiteral(": ")) ? description.section(QStringLiteral(": "), 1)
								   : description;
	if (!title.isEmpty()) {
		title[0] = title[0].toUpper();
	}
	return title;
}

void setWarn(QLabel *label, bool warn)
{
	if (label->property("warn").toBool() != warn) {
		label->setProperty("warn", warn);
		label->style()->unpolish(label);
		label->style()->polish(label);
	}
}

} // namespace

HotkeySettingsSection::HotkeySettingsSection(QWidget *parent) : QFrame(parent)
{
	setObjectName("panel");
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(16, 14, 16, 16);
	layout->setSpacing(10);
	auto *title = new QLabel(QTStr("Voltz.Settings.Hotkeys").toUpper(), this);
	title->setObjectName("panelTitle");
	layout->addWidget(title);
	auto *hint = new QLabel(QTStr("Voltz.Hotkeys.Hint"), this);
	hint->setObjectName("muted");
	hint->setWordWrap(true);
	layout->addWidget(hint);

	auto *grid = new QGridLayout();
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setHorizontalSpacing(16);
	grid->setVerticalSpacing(4);
	grid->setColumnStretch(1, 1);
	int gridRow = 0;
	/* só os registrados: o de depuração existe só com VOLTZ_DEBUG_FREEZE_UI */
	rows_.reserve(voltzHotkeys().size());
	for (const VoltzHotkey &hotkey : voltzHotkeys()) {
		if (findHotkey(hotkey.name) == OBS_INVALID_HOTKEY_ID) {
			continue;
		}
		const size_t index = rows_.size();
		Row row;
		row.name = hotkey.name;
		row.title = shortTitle(QTStr(hotkey.localeKey));
		auto *label = new QLabel(row.title, this);
		formLabels_.append(label);
		row.edit = new VoltzHotkeyEdit(this, obs_key_combination_t{0, OBS_KEY_NONE});
		row.edit->setAccessibleName(QTStr("Voltz.Hotkeys.Edit") + QStringLiteral(" \u2014 ") + row.title);
		row.clear = new QToolButton(this);
		row.clear->setObjectName("iconButton");
		row.clear->setIcon(phosphorIcon(QStringLiteral("x"), palette::kText));
		setButtonHint(row.clear, QTStr("Voltz.Hotkeys.Clear"), row.title);
		row.reset = new QToolButton(this);
		row.reset->setObjectName("iconButton");
		row.reset->setIcon(phosphorIcon(QStringLiteral("arrow-counter-clockwise"), palette::kText));
		setButtonHint(row.reset, QTStr("Voltz.Hotkeys.Default"), row.title);
		row.status = new QLabel(this);
		row.status->setObjectName("hotkeyStatus");
		row.status->setWordWrap(true);
		row.status->hide();
		grid->addWidget(label, gridRow, 0, Qt::AlignLeft | Qt::AlignVCenter);
		grid->addWidget(row.edit, gridRow, 1);
		grid->addWidget(row.clear, gridRow, 2);
		grid->addWidget(row.reset, gridRow, 3);
		gridRow++;
		grid->addWidget(row.status, gridRow, 1, 1, 3);
		gridRow++;

		connect(row.edit, &OBSHotkeyEdit::KeyChanged, this, [this, index]() { keyChanged(index); });
		connect(row.clear, &QToolButton::clicked, this, [this, index]() {
			save(index,
			     replaceFirstBinding(currentHotkeyBindings(findHotkey(rows_[index].name)), KeyCombo{}));
		});
		connect(row.reset, &QToolButton::clicked, this,
			[this, index]() { save(index, defaultHotkeyBindings(rows_[index].name)); });
		row.edit->focusLeft = [this, index]() {
			refreshRow(index);
		}; /* combinação incompleta: volta à salva */
		rows_.push_back(row);
	}
	layout->addLayout(grid);
	reload();
}

void HotkeySettingsSection::reload()
{
	for (size_t i = 0; i < rows_.size(); i++) {
		refreshRow(i);
	}
}

void HotkeySettingsSection::refreshRow(size_t index)
{
	Row &row = rows_[index];
	const std::vector<KeyCombo> bindings = currentHotkeyBindings(findHotkey(row.name));
	loading_ = true;
	row.edit->original = bindings.empty() ? obs_key_combination_t{0, OBS_KEY_NONE}
					      : toObsCombination(bindings.front());
	row.edit->ResetKey();
	loading_ = false;
	row.clear->setEnabled(!bindings.empty());
	row.reset->setEnabled(!(bindings == defaultHotkeyBindings(row.name)));

	QStringList notes;
	if (bindings.size() > 1) {
		notes.append(QTStr("Voltz.Hotkeys.More").arg(static_cast<int>(bindings.size() - 1)));
	}
	QStringList others;
	for (const std::string &name : hotkeyConflicts(row.name, bindings, allHotkeyBindings())) {
		const QString description = hotkeyDescription(name.c_str());
		others.append(description.isEmpty() ? QString::fromStdString(name) : description);
	}
	if (!others.isEmpty()) {
		notes.append(QTStr("Voltz.Hotkeys.Conflict").arg(others.join(QStringLiteral(", "))));
	}
	row.status->setText(notes.join(QStringLiteral(" ")));
	setWarn(row.status, !others.isEmpty());
	row.status->setVisible(!notes.isEmpty());
}

void HotkeySettingsSection::keyChanged(size_t index)
{
	if (loading_) {
		return;
	}
	Row &row = rows_[index];
	const KeyCombo combo = fromObsCombination(row.edit->key);
	switch (checkVoltzHotkeyCombo(combo)) {
	case HotkeyComboCheck::Ok:
		save(index, replaceFirstBinding(currentHotkeyBindings(findHotkey(row.name)), combo));
		return;
	case HotkeyComboCheck::NeedsKey:
		row.status->setText(QTStr("Voltz.Hotkeys.NeedsKey"));
		break;
	case HotkeyComboCheck::NeedsModifier:
		row.status->setText(QTStr("Voltz.Hotkeys.NeedsModifier"));
		break;
	case HotkeyComboCheck::Empty:
		return;
	}
	/* não grava: a pessoa continua apertando teclas ou sai do campo (que volta ao atalho salvo) */
	setWarn(row.status, true);
	row.status->setVisible(true);
}

void HotkeySettingsSection::save(size_t index, const std::vector<KeyCombo> &bindings)
{
	const char *name = rows_[index].name;
	saveHotkeyBinding(findHotkey(name), name, bindings);
	reload(); /* os conflitos das outras linhas podem ter mudado */
}

} // namespace voltz
```

Em `frontend/voltz/shell/SettingsPage.hpp`:
1. trocar:
```cpp
class FrontendBridge;
class MouseSettingsSection;
class MouseStyleStore;

// Tela "Ajustes": Gravação, Captura, Mouse (destaque do mouse) e Sobre.
// Aplica cada mudança na hora; trava a seção Gravação enquanto grava.
```
por:
```cpp
class FrontendBridge;
class HotkeySettingsSection;
class MouseSettingsSection;
class MouseStyleStore;

// Tela "Ajustes": Gravação, Captura, Mouse (destaque do mouse), Atalhos e Sobre.
// Aplica cada mudança na hora; trava a seção Gravação enquanto grava.
```
2. trocar:
```cpp
	MouseSettingsSection *mouse_ = nullptr;
	// Rótulos dos formulários de todas as seções: recebem a mesma largura para alinhar as colunas dos campos.
```
por:
```cpp
	MouseSettingsSection *mouse_ = nullptr;
	HotkeySettingsSection *hotkeys_ = nullptr;
	// Rótulos dos formulários de todas as seções: recebem a mesma largura para alinhar as colunas dos campos.
```

Em `frontend/voltz/shell/SettingsPage.cpp`:
1. trocar:
```cpp
#include "FrontendBridge.hpp"
#include "MouseSettings.hpp"
```
por:
```cpp
#include "FrontendBridge.hpp"
#include "HotkeySettings.hpp"
#include "MouseSettings.hpp"
```
2. trocar:
```cpp
	sections_->addWidget(mouse_);
	sections_->addWidget(buildAboutSection());
```
por:
```cpp
	sections_->addWidget(mouse_);
	hotkeys_ = new HotkeySettingsSection(content);
	formLabels_.append(hotkeys_->formLabels());
	sections_->addWidget(hotkeys_);
	sections_->addWidget(buildAboutSection());
```
3. trocar:
```cpp
	mouse_->reload();
	{
```
por:
```cpp
	mouse_->reload();
	hotkeys_->reload(); /* podem ter mudado nas Configurações do OBS ou com a troca de perfil */
	{
```

Em `frontend/voltz/shell/voltz-shell.qss`:
trocar:
```css
	text-align: left;
}
```
por:
```css
	text-align: left;
}

/* Ajustes › Atalhos (Plano 4D): o campo de teclas do OBS (OBSHotkeyEdit) e o aviso de cada atalho */
#voltzShell QLineEdit#hotkeyEdit {
	background: #18181C;
	color: #F2F2F3;
	border: 1px solid #1F1F24;
	border-radius: 8px;
	padding: 5px 10px;
	min-width: 200px;
}

#voltzShell QLineEdit#hotkeyEdit:focus {
	border-color: #D4F53C;
}

#voltzShell QLabel#hotkeyStatus {
	color: #82828C;
}

#voltzShell QLabel#hotkeyStatus[warn="true"] {
	color: #F5C83C;
}
```

Em `frontend/cmake/feature-voltz.cmake`:
trocar:
```cmake
    voltz/shell/HotkeyBinding.hpp
    voltz/shell/Icons.cpp
```
por:
```cmake
    voltz/shell/HotkeyBinding.hpp
    voltz/shell/HotkeySettings.cpp
    voltz/shell/HotkeySettings.hpp
    voltz/shell/Icons.cpp
```

- [ ] **Step 4: Pedido de teste `HotkeyState`**

Em `frontend/voltz/annotate/AnnotationTestVendor.hpp`:
trocar:
```cpp
//   AnnotationState → {"estado": texto JSON publicado pelo AnnotationController (republicado na thread da UI antes de
//   responder, até 2 s), "ok"}.
// O callback roda na thread do websocket, sem lock do obs-websocket: os pedidos que mudam algo são ENFILEIRADOS para a
```
por:
```cpp
//   AnnotationState → {"estado": texto JSON publicado pelo AnnotationController (republicado na thread da UI antes de
//   responder, até 2 s), "ok"};
//   HotkeyState → {"atalhos": texto JSON {"VoltzStudio.Draw":{"bindings":[...]},...} com as teclas de agora de cada
//   atalho do Voltz registrado (lidas na thread da UI, até 2 s)}.
// O callback roda na thread do websocket, sem lock do obs-websocket: os pedidos que mudam algo são ENFILEIRADOS para a
```

Em `frontend/voltz/annotate/AnnotationTestVendor.cpp`:
1. trocar:
```cpp
#include <voltz/core/annotation-model.hpp>
#include <voltz/core/live-protocol.hpp>

#include <obs-websocket-api.h>
```
por:
```cpp
#include <voltz/core/annotation-model.hpp>
#include <voltz/core/hotkey-bindings.hpp>
#include <voltz/core/live-protocol.hpp>
#include <voltz/shell/HotkeyBinding.hpp>

#include <obs-websocket-api.h>
```
2. trocar:
```cpp
				     "CommitText",   "CancelText",   "AnnotationState", "SetAnnotationStyle",
				     "ToolbarImage"};

// Entrega `work` na thread da UI sem esperar; falso se o controlador já saiu (EXIT). O mutex é segurado enquanto o evento
```
por:
```cpp
				     "CommitText",   "CancelText",   "AnnotationState", "SetAnnotationStyle",
				     "ToolbarImage", "HotkeyState"};

// Entrega `work` na thread da UI sem esperar; falso se o controlador já saiu (EXIT). O mutex é segurado enquanto o evento
```
3. trocar:
```cpp
}

void annotationState(obs_data_t *, obs_data_t *response, void *)
```
por:
```cpp
}

void hotkeyState(obs_data_t *, obs_data_t *response, void *)
{
	auto text = std::make_shared<std::string>();
	auto done = std::make_shared<std::promise<void>>();
	std::future<void> ready = done->get_future();
	bool ok = post([text, done](AnnotationController *) {
		std::string json = "{";
		for (const VoltzHotkey &hotkey : voltzHotkeys()) {
			const obs_hotkey_id id = findHotkey(hotkey.name);
			if (id == OBS_INVALID_HOTKEY_ID) {
				continue;
			}
			if (json.size() > 1) {
				json += ',';
			}
			json += std::string("\"") + hotkey.name +
				"\":" + hotkeyBindingsToJson(currentHotkeyBindings(id));
		}
		*text = json + "}";
		done->set_value();
	});
	if (ok) {
		done.reset();
		ok = ready.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
	}
	if (ok) {
		obs_data_set_string(response, "atalhos", text->c_str());
	}
	obs_data_set_bool(response, "ok", ok);
}

void annotationState(obs_data_t *, obs_data_t *response, void *)
```
4. trocar:
```cpp
	obs_websocket_vendor_register_request(gVendor, "ToolbarImage", toolbarImage, nullptr);
	blog(LOG_WARNING, "[voltz] modo de teste (%s): pedidos '%s' ligados", live::kTestEnv, live::kTestVendorShell);
```
por:
```cpp
	obs_websocket_vendor_register_request(gVendor, "ToolbarImage", toolbarImage, nullptr);
	obs_websocket_vendor_register_request(gVendor, "HotkeyState", hotkeyState, nullptr);
	blog(LOG_WARNING, "[voltz] modo de teste (%s): pedidos '%s' ligados", live::kTestEnv, live::kTestVendorShell);
```

- [ ] **Step 5: Textos (os dois locales)**

Em `frontend/data/locale/voltz-pt-BR.ini`,
trocar a linha
```ini
Voltz.Mouse.Shortcut="Atalho do holofote durante a gravação: %1. Troque em Configurações → Atalhos (modo clássico)."
```
por
```ini
Voltz.Mouse.Shortcut="Atalho do holofote durante a gravação: %1. Troque em Ajustes › Atalhos."
```
trocar a linha
```ini
Voltz.Record.Draw.Hint="Desenhar na tela (%1). Saia com Esc, clique direito ou o mesmo atalho. Troque o atalho em Configurações → Atalhos (modo clássico)."
```
por
```ini
Voltz.Record.Draw.Hint="Desenhar na tela (%1). Saia com Esc, clique direito ou o mesmo atalho. Troque o atalho em Ajustes › Atalhos."
```
no fim do arquivo (UTF-8 sem BOM, fim de linha CRLF como o resto), acrescentar:
```ini
Voltz.Settings.Hotkeys="Atalhos"
Voltz.Hotkeys.Hint="Valem com qualquer app em primeiro plano. Clique no campo e aperte a combinação, com Ctrl, Alt ou a tecla Windows (F1 a F24 valem sozinhas). Enquanto o campo está ativo, os atalhos ficam pausados."
Voltz.Hotkeys.Edit="Atalho"
Voltz.Hotkeys.Clear="Limpar atalho"
Voltz.Hotkeys.Default="Restaurar padrão"
Voltz.Hotkeys.NeedsKey="Falta a tecla: segure Ctrl, Alt ou Windows e aperte uma tecla."
Voltz.Hotkeys.NeedsModifier="Use Ctrl, Alt ou a tecla Windows junto: os atalhos não consomem a tecla, e sozinha ela também iria para o app em uso."
Voltz.Hotkeys.Conflict="Também usado por: %1"
Voltz.Hotkeys.More="(+%1 em Configurações → Atalhos, no modo clássico)"
```

Em `frontend/data/locale/voltz-en-US.ini`,
trocar a linha
```ini
Voltz.Mouse.Shortcut="Spotlight shortcut while recording: %1. Change it in Settings → Hotkeys (classic mode)."
```
por
```ini
Voltz.Mouse.Shortcut="Spotlight shortcut while recording: %1. Change it in Settings › Hotkeys."
```
trocar a linha
```ini
Voltz.Record.Draw.Hint="Draw on screen (%1). Exit with Esc, right-click or the same shortcut. Change the shortcut in Settings → Hotkeys (classic mode)."
```
por
```ini
Voltz.Record.Draw.Hint="Draw on screen (%1). Exit with Esc, right-click or the same shortcut. Change the shortcut in Settings › Hotkeys."
```
no fim do arquivo (UTF-8 sem BOM, fim de linha CRLF como o resto), acrescentar:
```ini
Voltz.Settings.Hotkeys="Hotkeys"
Voltz.Hotkeys.Hint="They work with any app in the foreground. Click the field and press the combination, with Ctrl, Alt or the Windows key (F1 to F24 work alone). While the field is active, hotkeys are paused."
Voltz.Hotkeys.Edit="Hotkey"
Voltz.Hotkeys.Clear="Clear hotkey"
Voltz.Hotkeys.Default="Restore default"
Voltz.Hotkeys.NeedsKey="Missing the key: hold Ctrl, Alt or Windows and press a key."
Voltz.Hotkeys.NeedsModifier="Use Ctrl, Alt or the Windows key too: hotkeys don't consume the key, so alone it would also reach the app in use."
Voltz.Hotkeys.Conflict="Also used by: %1"
Voltz.Hotkeys.More="(+%1 in Settings → Hotkeys, in classic mode)"
```

Paridade (Convenções) → sem saída.

- [ ] **Step 6: Formatar, compilar e grep de segurança**

```bash
"$CLANGFMT" -i frontend/voltz/shell/HotkeyBinding.hpp frontend/voltz/shell/HotkeyBinding.cpp frontend/voltz/shell/HotkeySettings.hpp frontend/voltz/shell/HotkeySettings.cpp frontend/voltz/annotate/DrawModeController.cpp frontend/voltz/shell/MouseStyleStore.cpp frontend/voltz/project/ProjectRecorder.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp frontend/voltz/shell/SettingsPage.hpp frontend/voltz/shell/SettingsPage.cpp
```
Run: CONFIGURAR; COMPILAR APP (segundo plano); RODAR TESTES (203); GREP DE SEGURANÇA (A–F).
Expected: `exit=0`; `203 | 203 passed`; grep como em Convenções, agora com E: só `frontend/voltz/shell/HotkeySettings.cpp`,
`1 DisableHotkeys()` e `1 UpdateHotkeyFocusSetting()`. Nenhum código de leitura do `[Hotkeys]` fora do `HotkeyBinding`:
`grep -rn '"Hotkeys"' frontend/voltz` → só `frontend/voltz/shell/HotkeyBinding.cpp` (2 linhas: ler e gravar).

- [ ] **Step 7: Teste ao vivo (só botões por UIA; o campo de atalho nunca recebe foco)**

Bloco de teste com `$ovW = 640; $ovH = 360`. No `try`, no lugar do comentário (depois do `Start-App`):
```powershell
  $default = '{"bindings":[{"shift":true,"control":true,"key":"OBS_KEY_D"}]}'
  $hk = Hotkeys
  "1) desenhar=$(BindingsText $hk 'VoltzStudio.Draw') marcador=$(BindingsText $hk 'VoltzStudio.AddMarker') holofote=$(BindingsText $hk 'VoltzStudio.ToggleSpotlight') desfazer=$(BindingsText $hk 'VoltzStudio.UndoAnnotation') depuracao=$($hk.PSObject.Properties.Name -contains 'VoltzStudio.DebugFreezeUi')"
  [void](Uia "Ajustes"); Start-Sleep 1; Assert-DrawSafe "ajustes"
  "2) campo=$(UiaExists 'Atalho — Ligar/desligar modo desenho') limpar=$(UiaExists 'Limpar atalho — Ligar/desligar modo desenho') padrao=$(UiaExists 'Restaurar padrão — Adicionar marcador')"
  [void](Uia "Limpar atalho — Ligar/desligar modo desenho"); $hk = Hotkeys; $ini = Profile-Ini
  "3) desenhar=$(BindingsText $hk 'VoltzStudio.Draw') perfil=$($ini.Contains('VoltzStudio.Draw={"bindings":[]}'))"
  [void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.Draw"}}]'); $st = Shell-State; Assert-DrawSafe "sem atalho"
  "4) modo=$($st.modo_desenho) recusa=$($st.desenho_recusa)"
  [void](Uia "Restaurar padrão — Ligar/desligar modo desenho"); $hk = Hotkeys; $ini = Profile-Ini
  "5) desenhar=$(BindingsText $hk 'VoltzStudio.Draw') perfil=$($ini.Contains('VoltzStudio.Draw=' + $default))"
  Close-App
  LogLines "[voltz] atalhos:"
```
Expected:
1. `desenhar=` o `$default`; `marcador` com `OBS_KEY_M`, `holofote` com `OBS_KEY_H` (os padrões de antes, agora pela
   tabela); `desfazer={"bindings":[]}`; `depuracao=False` (sem `VOLTZ_DEBUG_FREEZE_UI`).
2. `campo=True limpar=True padrao=True` (o "Adicionar marcador" no nome do botão confere o título curto).
3. `desenhar={"bindings":[]} perfil=True` (gravou ao vivo e no `basic.ini` do perfil).
4. `modo=False recusa=teste` (sem tecla, o atalho continua existindo e o modo continua recusado).
5. `desenhar=` o `$default` e `perfil=True`.
Log: `[voltz] atalhos: VoltzStudio.Draw = {"bindings":[]}` e `[voltz] atalhos: VoltzStudio.Draw = ` + o `$default`;
**nenhuma** linha `atalhos: editando`.

- [ ] **Step 8: Commit**

```bash
git add frontend/voltz/shell/HotkeyBinding.hpp frontend/voltz/shell/HotkeyBinding.cpp frontend/voltz/shell/HotkeySettings.hpp frontend/voltz/shell/HotkeySettings.cpp frontend/voltz/annotate/DrawModeController.cpp frontend/voltz/shell/MouseStyleStore.cpp frontend/voltz/project/ProjectRecorder.cpp frontend/voltz/annotate/AnnotationTestVendor.hpp frontend/voltz/annotate/AnnotationTestVendor.cpp frontend/voltz/shell/SettingsPage.hpp frontend/voltz/shell/SettingsPage.cpp frontend/voltz/shell/voltz-shell.qss frontend/cmake/feature-voltz.cmake frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): Ajustes > Atalhos (campo de teclas do OBS, atalhos pausados na edicao, validacao, conflitos) e atalhos carregados num lugar so

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 9: Casca — Ajustes › Anotações

**Files:**
- Create: `frontend/voltz/shell/AnnotationSettings.hpp`, `frontend/voltz/shell/AnnotationSettings.cpp`
- Modify: `frontend/voltz/shell/SettingsPage.hpp`, `frontend/voltz/shell/SettingsPage.cpp`,
  `frontend/voltz/shell/VoltzShell.cpp`, `frontend/voltz/shell/voltz-shell.qss`, `frontend/cmake/feature-voltz.cmake`,
  `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`

A seção **Anotações** (entre Mouse e Atalhos) edita o mesmo `AnnotationStyle` da barra: ferramenta, cor (diálogo de cor
— fora do modo desenho), espessura, tamanho e fonte do texto, "traços somem após" (nunca, 3, 5, 10, 30, 60 s; um valor de
fora entra na lista), com a prévia desenhada pelo rasterizador do núcleo (um traço da ferramenta e "Aa" na fonte do
texto), o estado (fora da gravação da tela; "queimadas" ou fonte que não abriu, em amarelo) e o atalho de desenhar.
Cada mudança vale na hora (`setStyle`, gravado 400 ms depois); a seção acompanha a barra pelo `styleChanged`.

- [ ] **Step 1: Seção e prévia**

Criar `frontend/voltz/shell/AnnotationSettings.hpp`:
```cpp
#pragma once

#include <voltz/core/annotation-model.hpp>

#include <QFrame>
#include <QList>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

namespace voltz {

class AnnotationController;
class DrawModeController;

// Prévia do estilo das anotações com o rasterizador do núcleo (o mesmo da sobreposição e da fonte do plugin): um traço
// da ferramenta escolhida e "Aa" na fonte do texto, sobre o fundo da casca.
class AnnotationPreview : public QWidget {
public:
	explicit AnnotationPreview(QWidget *parent = nullptr);
	void setAnnotationStyle(const AnnotationStyle &style);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	AnnotationStyle style_;
};

// Ajustes › Anotações (spec §4.3, Plano 4D): o estilo com que o modo desenho começa — ferramenta, cor (QColorDialog: fora
// do modo desenho, um diálogo modal não atrapalha), espessura, tamanho e fonte do texto, "traços somem após" — com
// prévia; o estado das anotações (fora da gravação da tela ou "queimadas", e a fonte do texto) e o atalho de desenhar.
// Cada mudança vale na hora (AnnotationController::setStyle, a mesma da barra; o user.ini é gravado 400 ms depois). Os
// rótulos ficam em formLabels() para o SettingsPage alinhar as colunas. Thread da UI.
class AnnotationSettingsSection : public QFrame {
public:
	AnnotationSettingsSection(AnnotationController *annotations, DrawModeController *drawMode,
				  QWidget *parent = nullptr);

	const QList<QLabel *> &formLabels() const { return formLabels_; }
	// Relê o estilo, o estado e o atalho.
	void reload();

private:
	QLabel *formLabel(const QString &text);
	void pickColor();
	void commit();
	void updateValueLabels();
	void selectFade(int seconds);

	AnnotationController *annotations_;
	DrawModeController *drawMode_;
	QComboBox *tool_ = nullptr;
	QPushButton *color_ = nullptr;
	QSlider *width_ = nullptr;
	QLabel *widthValue_ = nullptr;
	QSlider *textSize_ = nullptr;
	QLabel *textSizeValue_ = nullptr;
	QComboBox *font_ = nullptr;
	QComboBox *fade_ = nullptr;
	AnnotationPreview *preview_ = nullptr;
	QLabel *status_ = nullptr;
	QLabel *shortcut_ = nullptr;
	RgbColor colorValue_;
	QList<QLabel *> formLabels_;
	bool loading_ = false;
};

} // namespace voltz
```

Criar `frontend/voltz/shell/AnnotationSettings.cpp`:
```cpp
#include "AnnotationSettings.hpp"

#include "Icons.hpp"

#include <voltz/annotate/AnnotationController.hpp>
#include <voltz/annotate/DrawModeController.hpp>
#include <voltz/core/annotation-font.hpp>
#include <voltz/core/annotation-raster.hpp>

#include <OBSApp.hpp> // QTStr

#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace voltz {

namespace {

constexpr int kFadeChoices[] = {0, 3, 5, 10, 30, 60};
// Texto da prévia, nunca maior que isto (pixels lógicos), para caber.
constexpr int kPreviewTextMax = 40;

struct ToolChoice {
	AnnotationTool tool;
	const char *text;
};

const ToolChoice kToolChoices[] = {
	{AnnotationTool::Pen, "Voltz.Draw.Tool.Pen"},
	{AnnotationTool::Highlighter, "Voltz.Draw.Tool.Highlighter"},
	{AnnotationTool::Line, "Voltz.Draw.Tool.Line"},
	{AnnotationTool::Arrow, "Voltz.Draw.Tool.Arrow"},
	{AnnotationTool::Rectangle, "Voltz.Draw.Tool.Rectangle"},
	{AnnotationTool::Ellipse, "Voltz.Draw.Tool.Ellipse"},
	{AnnotationTool::Text, "Voltz.Draw.Tool.Text"},
};

QColor toQColor(const RgbColor &color)
{
	return QColor(color.r, color.g, color.b);
}

RgbColor fromQColor(const QColor &color)
{
	return RgbColor{static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
			static_cast<uint8_t>(color.blue())};
}

QIcon swatch(const RgbColor &color)
{
	QPixmap pixmap(14, 14);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(QColor(0x3A, 0x3A, 0x42), 1.0));
	painter.setBrush(toQColor(color));
	painter.drawRoundedRect(QRectF(0.5, 0.5, 13.0, 13.0), 3.0, 3.0);
	return QIcon(pixmap);
}

void setWarn(QLabel *label, bool warn)
{
	if (label->property("warn").toBool() != warn) {
		label->setProperty("warn", warn);
		label->style()->unpolish(label);
		label->style()->polish(label);
	}
}

} // namespace

AnnotationPreview::AnnotationPreview(QWidget *parent) : QWidget(parent)
{
	setFixedSize(240, 120);
	setAccessibleName(QTStr("Voltz.Annot.Preview"));
}

void AnnotationPreview::setAnnotationStyle(const AnnotationStyle &style)
{
	style_ = style;
	update();
}

void AnnotationPreview::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(QColor(0x1F, 0x1F, 0x24), 1.0));
	painter.setBrush(QColor(0x09, 0x09, 0x0B));
	painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);

	/* pixels lógicos × devicePixelRatio: a prévia sai nítida a 125/150% */
	const double ratio = devicePixelRatioF();
	QImage image(static_cast<int>(std::lround(width() * ratio)), static_cast<int>(std::lround(height() * ratio)),
		     QImage::Format_ARGB32_Premultiplied); /* B, G, R, A na memória: o RasterImage do núcleo */
	image.fill(Qt::transparent);
	const RasterImage raster{image.bits(), image.width(), image.height(), static_cast<int>(image.bytesPerLine())};

	Stroke stroke;
	stroke.tool = style_.tool == AnnotationTool::Text ? AnnotationTool::Pen : style_.tool;
	stroke.color = style_.color;
	stroke.width = style_.width;
	if (stroke.tool == AnnotationTool::Pen || stroke.tool == AnnotationTool::Highlighter) {
		std::vector<StrokePoint> wave;
		for (int x = 18; x <= 130; x += 4) {
			wave.push_back(StrokePoint{x, static_cast<int>(60 + 22 * std::sin(x / 18.0))});
		}
		appendStrokePoints(stroke, wave);
	} else {
		appendStrokePoints(stroke, {StrokePoint{22, 96}, StrokePoint{126, 24}});
	}
	rasterizeStroke(raster, stroke, ratio, ratio);

	Stroke text;
	text.tool = AnnotationTool::Text;
	text.color = style_.color;
	text.textSize = std::min(style_.textSize, kPreviewTextMax);
	text.textFont = style_.textFont;
	text.text = "Aa";
	text.points = {StrokePoint{150, 60 - text.textSize / 2}};
	rasterizeStroke(raster, text, ratio, ratio);

	image.setDevicePixelRatio(ratio);
	painter.drawImage(QPointF(0.0, 0.0), image);
}

AnnotationSettingsSection::AnnotationSettingsSection(AnnotationController *annotations, DrawModeController *drawMode,
						     QWidget *parent)
	: QFrame(parent),
	  annotations_(annotations),
	  drawMode_(drawMode)
{
	setObjectName("panel");
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(16, 14, 16, 16);
	layout->setSpacing(10);
	auto *title = new QLabel(QTStr("Voltz.Settings.Annotations").toUpper(), this);
	title->setObjectName("panelTitle");
	layout->addWidget(title);
	auto *hint = new QLabel(QTStr("Voltz.Annot.Hint"), this);
	hint->setObjectName("muted");
	hint->setWordWrap(true);
	layout->addWidget(hint);

	tool_ = new QComboBox(this);
	tool_->setAccessibleName(QTStr("Voltz.Annot.Tool"));
	for (const ToolChoice &choice : kToolChoices) {
		tool_->addItem(QTStr(choice.text), static_cast<int>(choice.tool));
	}
	color_ = new QPushButton(this);
	color_->setObjectName("colorButton");
	color_->setAccessibleName(QTStr("Voltz.Annot.PickColor"));
	color_->setIconSize(QSize(14, 14));
	width_ = new QSlider(Qt::Horizontal, this);
	width_->setObjectName("mouseSlider");
	width_->setRange(kStrokeWidthMin, kStrokeWidthMax);
	width_->setAccessibleName(QTStr("Voltz.Annot.Width"));
	width_->setMinimumWidth(160);
	widthValue_ = new QLabel(this);
	textSize_ = new QSlider(Qt::Horizontal, this);
	textSize_->setObjectName("mouseSlider");
	textSize_->setRange(kTextSizeMin, kTextSizeMax);
	textSize_->setAccessibleName(QTStr("Voltz.Annot.TextSize"));
	textSize_->setMinimumWidth(160);
	textSizeValue_ = new QLabel(this);
	font_ = new QComboBox(this);
	font_->setAccessibleName(QTStr("Voltz.Annot.TextFont"));
	font_->addItem(QTStr("Voltz.Annot.Font.Bold"), static_cast<int>(TextFont::Bold));
	font_->addItem(QTStr("Voltz.Annot.Font.Regular"), static_cast<int>(TextFont::Regular));
	fade_ = new QComboBox(this);
	fade_->setAccessibleName(QTStr("Voltz.Annot.Fade"));
	for (int seconds : kFadeChoices) {
		fade_->addItem(seconds > 0 ? QTStr("Voltz.Annot.Fade.Seconds").arg(seconds)
					   : QTStr("Voltz.Annot.Fade.Never"),
			       seconds);
	}

	auto *grid = new QGridLayout();
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setHorizontalSpacing(16);
	grid->setVerticalSpacing(10);
	grid->setColumnStretch(1, 1);
	int row = 0;
	auto addRow = [this, grid, &row](const QString &label, QWidget *field) {
		grid->addWidget(formLabel(label), row, 0, Qt::AlignLeft | Qt::AlignVCenter);
		grid->addWidget(field, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
		row++;
	};
	auto addValueRow = [this, grid, &row](const QString &label, QWidget *field, QLabel *value) {
		value->setObjectName("muted");
		value->setMinimumWidth(56);
		auto *line = new QHBoxLayout();
		line->setSpacing(10);
		line->addWidget(field, 1);
		line->addWidget(value);
		grid->addWidget(formLabel(label), row, 0, Qt::AlignLeft | Qt::AlignVCenter);
		grid->addLayout(line, row, 1);
		row++;
	};
	addRow(QTStr("Voltz.Annot.Tool"), tool_);
	addRow(QTStr("Voltz.Annot.Color"), color_);
	addValueRow(QTStr("Voltz.Annot.Width"), width_, widthValue_);
	addValueRow(QTStr("Voltz.Annot.TextSize"), textSize_, textSizeValue_);
	addRow(QTStr("Voltz.Annot.TextFont"), font_);
	addRow(QTStr("Voltz.Annot.Fade"), fade_);

	preview_ = new AnnotationPreview(this);
	auto *columns = new QHBoxLayout();
	columns->setSpacing(24);
	columns->addLayout(grid, 1);
	columns->addWidget(preview_, 0, Qt::AlignTop);
	layout->addLayout(columns);

	status_ = new QLabel(this);
	status_->setObjectName("annotStatus");
	status_->setWordWrap(true);
	layout->addWidget(status_);
	shortcut_ = new QLabel(this);
	shortcut_->setObjectName("muted");
	shortcut_->setWordWrap(true);
	layout->addWidget(shortcut_);

	connect(tool_, &QComboBox::currentIndexChanged, this, [this]() { commit(); });
	connect(font_, &QComboBox::currentIndexChanged, this, [this]() { commit(); });
	connect(fade_, &QComboBox::currentIndexChanged, this, [this]() { commit(); });
	for (QSlider *slider : {width_, textSize_}) {
		connect(slider, &QSlider::valueChanged, this, [this]() {
			updateValueLabels();
			commit();
		});
	}
	connect(color_, &QPushButton::clicked, this, [this]() { pickColor(); });
	connect(annotations_, &AnnotationController::styleChanged, this, [this]() { reload(); });
	connect(annotations_, &AnnotationController::burnedChanged, this, [this](bool) { reload(); });
	reload();
}

QLabel *AnnotationSettingsSection::formLabel(const QString &text)
{
	auto *label = new QLabel(text, this);
	formLabels_.append(label);
	return label;
}

void AnnotationSettingsSection::pickColor()
{
	const QColor chosen = QColorDialog::getColor(toQColor(colorValue_), this, QTStr("Voltz.Annot.PickColor"));
	if (!chosen.isValid()) {
		return;
	}
	colorValue_ = fromQColor(chosen);
	commit();
}

void AnnotationSettingsSection::updateValueLabels()
{
	widthValue_->setText(QTStr("Voltz.Annot.Pixels").arg(width_->value()));
	textSizeValue_->setText(QTStr("Voltz.Annot.Pixels").arg(textSize_->value()));
}

void AnnotationSettingsSection::selectFade(int seconds)
{
	int index = fade_->findData(seconds);
	if (index < 0) {
		/* valor de antes (ou do user.ini) fora da lista: entra na lista */
		fade_->addItem(QTStr("Voltz.Annot.Fade.Seconds").arg(seconds), seconds);
		index = fade_->count() - 1;
	}
	fade_->setCurrentIndex(index);
}

void AnnotationSettingsSection::commit()
{
	if (loading_) {
		return;
	}
	AnnotationStyle style = annotations_->style();
	style.tool = static_cast<AnnotationTool>(tool_->currentData().toInt());
	style.color = colorValue_;
	style.width = width_->value();
	style.textSize = textSize_->value();
	style.textFont = static_cast<TextFont>(font_->currentData().toInt());
	style.fadeSeconds = fade_->currentData().toInt();
	annotations_->setStyle(style); /* styleChanged → reload */
}

void AnnotationSettingsSection::reload()
{
	loading_ = true;
	const AnnotationStyle &style = annotations_->style();
	tool_->setCurrentIndex(tool_->findData(static_cast<int>(style.tool)));
	colorValue_ = style.color;
	color_->setIcon(swatch(colorValue_));
	color_->setText(QString::fromStdString(formatHexColor(colorValue_)));
	width_->setValue(style.width);
	textSize_->setValue(style.textSize);
	font_->setCurrentIndex(font_->findData(static_cast<int>(style.textFont)));
	selectFade(style.fadeSeconds);
	updateValueLabels();
	preview_->setAnnotationStyle(style);
	const bool fontOk = embeddedFontAvailable(TextFont::Regular) && embeddedFontAvailable(TextFont::Bold);
	if (annotations_->burned()) {
		status_->setText(QTStr("Voltz.Annot.Status.Burned"));
	} else if (!fontOk) {
		status_->setText(QTStr("Voltz.Annot.Status.NoFont"));
	} else {
		status_->setText(QTStr("Voltz.Annot.Status.Ok"));
	}
	setWarn(status_, annotations_->burned() || !fontOk);
	shortcut_->setText(QTStr("Voltz.Annot.Shortcut").arg(drawMode_->drawShortcutText()));
	loading_ = false;
}

} // namespace voltz
```

- [ ] **Step 2: Na página de Ajustes**

Em `frontend/voltz/shell/SettingsPage.hpp`:
1. trocar:
```cpp
namespace voltz {

class DeviceSelector;
class ElidedLabel;
```
por:
```cpp
namespace voltz {

class AnnotationController;
class AnnotationSettingsSection;
class DeviceSelector;
class DrawModeController;
class ElidedLabel;
```
2. trocar:
```cpp
class MouseStyleStore;

// Tela "Ajustes": Gravação, Captura, Mouse (destaque do mouse), Atalhos e Sobre.
// Aplica cada mudança na hora; trava a seção Gravação enquanto grava.
```
por:
```cpp
class MouseStyleStore;

// Tela "Ajustes": Gravação, Captura, Mouse (destaque do mouse), Anotações, Atalhos e Sobre.
// Aplica cada mudança na hora; trava a seção Gravação enquanto grava.
```
3. trocar:
```cpp
public:
	explicit SettingsPage(FrontendBridge *bridge, MouseStyleStore *mouseStyles, QWidget *parent = nullptr);

protected:
```
por:
```cpp
public:
	SettingsPage(FrontendBridge *bridge, MouseStyleStore *mouseStyles, AnnotationController *annotations,
		     DrawModeController *drawMode, QWidget *parent = nullptr);

protected:
```
4. trocar:
```cpp
	MouseSettingsSection *mouse_ = nullptr;
	HotkeySettingsSection *hotkeys_ = nullptr;
```
por:
```cpp
	MouseSettingsSection *mouse_ = nullptr;
	AnnotationSettingsSection *annotations_ = nullptr;
	HotkeySettingsSection *hotkeys_ = nullptr;
```

Em `frontend/voltz/shell/SettingsPage.cpp`:
1. trocar:
```cpp
#include "SettingsPage.hpp"

#include "DeviceSelector.hpp"
```
por:
```cpp
#include "SettingsPage.hpp"

#include "AnnotationSettings.hpp"
#include "DeviceSelector.hpp"
```
2. trocar:
```cpp
};

SettingsPage::SettingsPage(FrontendBridge *bridge, MouseStyleStore *mouseStyles, QWidget *parent)
	: QWidget(parent),
```
por:
```cpp
};

SettingsPage::SettingsPage(FrontendBridge *bridge, MouseStyleStore *mouseStyles, AnnotationController *annotations,
			   DrawModeController *drawMode, QWidget *parent)
	: QWidget(parent),
```
3. trocar:
```cpp
	sections_->addWidget(mouse_);
	hotkeys_ = new HotkeySettingsSection(content);
```
por:
```cpp
	sections_->addWidget(mouse_);
	annotations_ = new AnnotationSettingsSection(annotations, drawMode, content);
	formLabels_.append(annotations_->formLabels());
	sections_->addWidget(annotations_);
	hotkeys_ = new HotkeySettingsSection(content);
```
4. trocar:
```cpp
	mouse_->reload();
	hotkeys_->reload(); /* podem ter mudado nas Configurações do OBS ou com a troca de perfil */
	{
```
por:
```cpp
	mouse_->reload();
	annotations_->reload(); /* o atalho de desenhar pode ter mudado */
	hotkeys_->reload();     /* podem ter mudado nas Configurações do OBS ou com a troca de perfil */
	{
```

Em `frontend/voltz/shell/VoltzShell.cpp`:
trocar:
```cpp
	pages_->addWidget(presets);
	pages_->addWidget(new SettingsPage(bridge_, mouseStyles_, pages_));

	auto *central = new QWidget(this);
```
por:
```cpp
	pages_->addWidget(presets);
	pages_->addWidget(new SettingsPage(bridge_, mouseStyles_, annotations_, drawMode_, pages_));

	auto *central = new QWidget(this);
```

Em `frontend/voltz/shell/voltz-shell.qss`:
trocar:
```css
	color: #F5C83C;
}
```
por:
```css
	color: #F5C83C;
}

/* Ajustes › Anotações (Plano 4D): estado das anotações (fora da gravação da tela; "queimadas" e fonte em amarelo) */
#voltzShell QLabel#annotStatus {
	color: #82828C;
}

#voltzShell QLabel#annotStatus[warn="true"] {
	color: #F5C83C;
}
```

Em `frontend/cmake/feature-voltz.cmake`:
trocar:
```cmake
    voltz/project/ProjectRecorder.hpp
    voltz/shell/AudioPanel.cpp
```
por:
```cmake
    voltz/project/ProjectRecorder.hpp
    voltz/shell/AnnotationSettings.cpp
    voltz/shell/AnnotationSettings.hpp
    voltz/shell/AudioPanel.cpp
```

- [ ] **Step 3: Textos (os dois locales)**

Em `frontend/data/locale/voltz-pt-BR.ini`,
no fim do arquivo (UTF-8 sem BOM, fim de linha CRLF como o resto), acrescentar:
```ini
Voltz.Settings.Annotations="Anotações"
Voltz.Annot.Hint="Estilo com que o modo desenho começa. A barra de desenho muda o mesmo estilo, que fica salvo."
Voltz.Annot.Tool="Ferramenta"
Voltz.Annot.Color="Cor"
Voltz.Annot.PickColor="Escolher a cor das anotações"
Voltz.Annot.Width="Espessura"
Voltz.Annot.TextSize="Tamanho do texto"
Voltz.Annot.TextFont="Fonte do texto"
Voltz.Annot.Font.Regular="DM Sans"
Voltz.Annot.Font.Bold="DM Sans Negrito"
Voltz.Annot.Fade="Traços somem após"
Voltz.Annot.Fade.Never="Nunca"
Voltz.Annot.Fade.Seconds="%1 s"
Voltz.Annot.Pixels="%1 px"
Voltz.Annot.Preview="Prévia das anotações"
Voltz.Annot.Status.Ok="As anotações aparecem no programa e ficam fora da gravação da tela (tela.mkv)."
Voltz.Annot.Status.Burned="Neste Windows as anotações também ficam na gravação da tela (tela.mkv): ele não consegue escondê-las da captura (requer o Windows 10 2004)."
Voltz.Annot.Status.NoFont="A fonte do texto não abriu: o texto não aparece (veja o log)."
Voltz.Annot.Shortcut="Modo desenho: %1 ou o botão Desenhar da tela Gravar. Troque o atalho em Atalhos, abaixo."
```

Em `frontend/data/locale/voltz-en-US.ini`,
no fim do arquivo (UTF-8 sem BOM, fim de linha CRLF como o resto), acrescentar:
```ini
Voltz.Settings.Annotations="Annotations"
Voltz.Annot.Hint="Style the draw mode starts with. The drawing toolbar changes the same style, which is saved."
Voltz.Annot.Tool="Tool"
Voltz.Annot.Color="Color"
Voltz.Annot.PickColor="Choose the annotation color"
Voltz.Annot.Width="Thickness"
Voltz.Annot.TextSize="Text size"
Voltz.Annot.TextFont="Text font"
Voltz.Annot.Font.Regular="DM Sans"
Voltz.Annot.Font.Bold="DM Sans Bold"
Voltz.Annot.Fade="Strokes fade after"
Voltz.Annot.Fade.Never="Never"
Voltz.Annot.Fade.Seconds="%1 s"
Voltz.Annot.Pixels="%1 px"
Voltz.Annot.Preview="Annotation preview"
Voltz.Annot.Status.Ok="Annotations show in the program and stay out of the screen recording (tela.mkv)."
Voltz.Annot.Status.Burned="On this Windows, annotations also end up in the screen recording (tela.mkv): it can't hide them from capture (requires Windows 10 2004)."
Voltz.Annot.Status.NoFont="The text font didn't open: text won't show (see the log)."
Voltz.Annot.Shortcut="Draw mode: %1 or the Draw button on the Record screen. Change the shortcut in Hotkeys, below."
```

Paridade (Convenções) → sem saída.

- [ ] **Step 4: Formatar e compilar**

```bash
"$CLANGFMT" -i frontend/voltz/shell/AnnotationSettings.hpp frontend/voltz/shell/AnnotationSettings.cpp frontend/voltz/shell/SettingsPage.hpp frontend/voltz/shell/SettingsPage.cpp frontend/voltz/shell/VoltzShell.cpp
```
Run: CONFIGURAR; COMPILAR APP (segundo plano); RODAR TESTES.
Expected: `exit=0`; `test cases: 203 | 203 passed` (a contagem não muda nesta task).

- [ ] **Step 5: Teste ao vivo (UIA só de leitura; o estilo muda pelo pedido de teste)**

Bloco de teste com `$ovW = 640; $ovH = 360`. No `try`, no lugar do comentário (depois do `Start-App`):
```powershell
  [void](Uia "Ajustes"); Start-Sleep 1; Assert-DrawSafe "ajustes"
  $slider = [System.Windows.Automation.ControlType]::Slider
  "1) previa=$(UiaExists 'Prévia das anotações') espessura=$(UiaValue 'Espessura' $slider) tamanho=$(UiaValue 'Tamanho do texto' $slider)"
  [void](VendorShell "SetAnnotationStyle" (ConvertTo-Json @{ estilo = '{"ferr":"seta","cor":"#4DA3FF","esp":8,"somem_apos_s":7,"tam":80,"fonte":"dm_sans"}' } -Compress))
  Start-Sleep 1
  "2) espessura=$(UiaValue 'Espessura' $slider) tamanho=$(UiaValue 'Tamanho do texto' $slider)"
  $r = New-Object VoltzProbe+RECT; [void]$P::GetWindowRect($script:shell, [ref]$r)
  $b = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [System.Drawing.Graphics]::FromImage($b)
  Add-Type -Namespace V -Name Pw -MemberDefinition '[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);'
  $d = $g.GetHdc(); [void][V.Pw]::PrintWindow($script:shell, $d, 2); $g.ReleaseHdc($d); $g.Dispose(); $b.Save("$sp\4d-ajustes.png"); $b.Dispose()
  Close-App
  Select-String -LiteralPath (Join-Path $voltzCfg "user.ini") -Encoding UTF8 -SimpleMatch "AnnotationStyle=" | ForEach-Object { $_.Line }
```
(Leia o `user.ini` antes de sair do `try`: o `finally` apaga a config.)
Expected:
1. `previa=True espessura=4 tamanho=48` (o estilo padrão).
2. `espessura=8 tamanho=80` (a seção acompanhou o estilo mudado por fora, como a barra faz).
`AnnotationStyle={"ferr":"seta","cor":"#4DA3FF","esp":8,"somem_apos_s":7,"tam":80,"fonte":"dm_sans"}`. Abra
`4d-ajustes.png` (só a janela da casca) e descreva o topo da página de Ajustes; as seções novas ficam abaixo, fora da
área visível sem rolar (a rolagem não é feita por teste: ela fica no roteiro manual).

- [ ] **Step 6: Commit**

```bash
git add frontend/voltz/shell/AnnotationSettings.hpp frontend/voltz/shell/AnnotationSettings.cpp frontend/voltz/shell/SettingsPage.hpp frontend/voltz/shell/SettingsPage.cpp frontend/voltz/shell/VoltzShell.cpp frontend/voltz/shell/voltz-shell.qss frontend/cmake/feature-voltz.cmake frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): Ajustes > Anotacoes (estilo com que o modo desenho comeca, previa pelo rasterizador do nucleo, estado e atalho)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: Smoke do texto e dos Ajustes (`tools/voltz-smoke-text.ps1`) e grep de segurança de todo o código

**Files:**
- Create: `tools/voltz-smoke-text.ps1`
- Modify: `tools/voltz-smoke-draw.ps1` (o estilo agora tem `"tam"` e `"fonte"`)

O smoke repete o padrão do `voltz-smoke-draw.ps1` (config portátil com backup e hash, sobreposição de teste fora do
desktop virtual, `Assert-DrawSafe` e vigia, só o PID lançado) em duas fases: **A** — texto ao vivo pelos pedidos de
teste (buffer da sobreposição com as letras e o cursor, programa por `GetSourceScreenshot`, fonte do plugin com um traço
só), uma gravação em `-OutDir` com quatro textos no `anotacoes.jsonl` (dois de antes com `t_ini` 0, um com acentos e
quebra de linha, um digitado gravando e um cancelado com `t_limpo`; o vazio não entra), medidas e o log sem o conteúdo
digitado; **B** — Ajustes por UI Automation só em botões (limpar e restaurar o atalho de desenhar, conferidos ao vivo e
no perfil, com o modo desenho sempre recusado), as seções novas presentes (só leitura) e a barra com o texto e o seletor
desenhada fora da tela.

- [ ] **Step 1: Script do smoke**

Criar `tools/voltz-smoke-text.ps1` (ASCII puro):
```powershell
<#
  Teste de integracao do Plano 4D (texto das anotacoes, Ajustes > Anotacoes e Ajustes > Atalhos). O modo desenho NUNCA
  liga aqui e nada e digitado: o texto entra so pelos pedidos de teste da casca (InjectText, TypeText, CommitText,
  CancelText), que chamam o AnnotationController direto.
  Seguranca (o PC e do usuario e esta em uso):
  - VOLTZ_TEST_LIVE=1 recusa o modo desenho duas vezes (DrawModeController e DrawInputWindow). O unico gatilho usado e
    TriggerHotkeyByName "VoltzStudio.Draw", so para provar a recusa (inclusive depois de limpar e restaurar o atalho).
  - Nenhuma entrada e injetada; nada e posto em primeiro plano; nenhum campo de texto da casca recebe foco (o campo de
    atalho nunca e acionado: so os botoes "Limpar atalho" e "Restaurar padrao", por UI Automation/Invoke). O unico
    P/Invoke que escreve e PostMessage(WM_CLOSE) na janela da casca do PID lancado, guardada na abertura.
  - Assert-DrawSafe depois de cada acao e um vigia (job, 200 ms), so leitura, iguais aos do voltz-smoke-draw.ps1:
    sobreposicoes do PID dentro do retangulo de teste (fora do desktop virtual), transparentes a cliques, fora do primeiro
    plano e da captura; NENHUMA janela VoltzDrawInput; NENHUMA barra "VoltzDrawToolbar" visivel; nenhuma thread do PID
    com janela Voltz Win32 ativa, com foco ou com o mouse capturado. Falha: o PID lancado e encerrado (BLOCKED).
  - Tela e Camera de COR (VOLTZ_TEST_PRESET_STANDINS); Assert-NoRealCapture; render so por GetSourceScreenshot de cena
    sintetica; a barra so e desenhada fora da tela (ToolbarImage, sem janela); so metadados da gravacao (nenhum quadro).
  - Config portatil guardada e restaurada (conferida por hash); -OutDir apagado no fim; Videos conferida intacta.
  Fases: A (texto ao vivo na sobreposicao e no programa, gravacao com o texto no anotacoes.jsonl, cancelar, privacidade
  do log), B (Ajustes: atalhos limpar/restaurar no perfil e ao vivo, secoes presentes, barra com o texto e o seletor).
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-text.ps1 [-OutDir <pasta vazia>] [-Keep]
#>
param(
    [string]$Config = "RelWithDebInfo",
    [string]$OutDir = (Join-Path $env:TEMP "voltz-smoke-text"),
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
$o = [string][char]0xF3; $a = [string][char]0xE2; $dash = [string][char]0x2014; $ccedil = [string][char]0xE7
$atilde = [string][char]0xE3; $eacute = [string][char]0xE9
$presetScreen = "S${o} tela / S${o} c${a}mera $dash Aplicar"
$sceneScreen = "S${o} tela"
$drawTitle = "Ligar/desligar modo desenho"
$clearDraw = "Limpar atalho $dash $drawTitle"
$resetDraw = "Restaurar padr${atilde}o $dash $drawTitle"
$editDraw = "Atalho $dash $drawTitle"
$textSizeName = "Tamanho do texto"
$previewName = "Pr${eacute}via das anota${ccedil}${o}es"
$accentText = "A${ccedil}${atilde}o 4D`nlinha 2"

$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.smoketext-bak"
$voltzCfg = Join-Path $cfg "Voltz Studio"
$logDir = Join-Path $env:TEMP "voltz-smoke-text-logs"
$wsPort = 4455

if ((Test-Path -LiteralPath $OutDir) -and @(Get-ChildItem -Force -LiteralPath $OutDir).Count -gt 0) {
    Write-Host "FALHOU -OutDir ja existe e nao esta vazio: $OutDir"; exit 1
}
$oldBaks = @(Get-ChildItem -LiteralPath $rundir -Directory -Force -Filter "config.*bak*" -ErrorAction SilentlyContinue)
if ($oldBaks.Count -gt 0) { Write-Host "FALHOU backup anterior em $rundir ($(($oldBaks | ForEach-Object { $_.Name }) -join ', ')): restaure ou apague antes"; exit 1 }
if (@(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue).Count -gt 0) {
    Write-Host "FALHOU a porta $wsPort ja esta em uso (outro OBS/Voltz aberto?)"; exit 1
}

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
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
$voltzVideos = Join-Path $videosDir "Voltz Studio"
$hadVoltzVideos = Test-Path -LiteralPath $voltzVideos
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
    $file = Join-Path $env:TEMP "voltz-smoke-text-req.json"
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
# So LEITURA: existe um elemento com esse nome numa janela do PID lancado? (nada e acionado nem focado)
function UiaExists([string]$name) {
    $AE = [System.Windows.Automation.AutomationElement]
    $pidCond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $script:proc.Id)
    $nameCond = New-Object System.Windows.Automation.PropertyCondition($AE::NameProperty, $name)
    foreach ($win in @($AE::RootElement.FindAll([System.Windows.Automation.TreeScope]::Children, $pidCond))) {
        if ($null -ne $win.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $nameCond)) { return $true }
    }
    return $false
}
function Vendor([string]$type, [string]$dataJson = '{}', [string]$vendorName = 'voltz-teste') {
    $r = Ws ('[{"requestType":"CallVendorRequest","requestData":{"vendorName":"' + $vendorName + '","requestType":"' + $type + '","requestData":' + $dataJson + '}}]')
    if ($r.Count -lt 1 -or -not $r[0].ok) { throw "pedido de teste $vendorName/$type falhou" }
    return $r[0].data.responseData
}
function VendorShell([string]$type, [string]$dataJson = '{}') { return (Vendor $type $dataJson 'voltz-teste-casca') }
function Shell-State { Start-Sleep -Milliseconds 400; return ((VendorShell "AnnotationState").estado | ConvertFrom-Json) }
function Feed-State { return ((Vendor "AnnotationFeedState").estado | ConvertFrom-Json) }
function Hotkeys { Start-Sleep -Milliseconds 300; return ((VendorShell "HotkeyState").atalhos | ConvertFrom-Json) }
function BindingsText($hotkeys, [string]$name) { return (ConvertTo-Json -InputObject $hotkeys.$name -Compress -Depth 5) }
function Text-Json([string]$points, [string]$text, [switch]$Open) {
    $req = [ordered]@{ pts = $points; texto = $text }
    if ($Open) { $req.aberto = $true }
    return (ConvertTo-Json $req -Compress)
}
# Pontos "[[x,y],...]" de uma grade (passo dx, dy) no retangulo [x0,x1] x [y0,y1]
function Grid([int]$x0, [int]$x1, [int]$dx, [int]$y0, [int]$y1, [int]$dy) {
    $pts = @()
    for ($y = $y0; $y -le $y1; $y += $dy) { for ($x = $x0; $x -le $x1; $x += $dx) { $pts += "[$x,$y]" } }
    return "[" + ($pts -join ",") + "]"
}

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
# Pixels quase lima na regiao [x0,x1) x [y0,y1) da imagem
function Lime-Count($img, [int]$x0, [int]$x1, [int]$y0, [int]$y1) {
    $n = 0
    for ($y = $y0; $y -lt $y1; $y++) { for ($x = $x0; $x -lt $x1; $x++) { if (Near $img.GetPixel($x, $y) 212 245 60 40) { $n++ } } }
    return $n
}

function New-CleanConfig([string]$voltzLines) {
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
    New-Item -ItemType Directory -Force $wsDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":' + $wsPort + '}')
    [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[BasicWindow]`r`nHideOBSWindowsFromCapture=false`r`n[Voltz]`r`n" + $voltzLines, (New-Object Text.UTF8Encoding $false))
}
function Set-TestEnv {
    $env:VOLTZ_TEST_LIVE = "1"
    $env:VOLTZ_TEST_PRESET_STANDINS = "1"
    $env:VOLTZ_TEST_OVERLAY_RECT = $script:overlayRect
    Remove-Item Env:VOLTZ_DEBUG_FREEZE_UI, Env:VOLTZ_TEST_ANNOT_BURNED, Env:VOLTZ_TEST_SCREEN_LAYERS -ErrorAction SilentlyContinue
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
function Apply-Preset([string]$tag) {
    Check "${tag}: preset 'So tela / So camera' (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    [void](Ws ('[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":' + (ConvertTo-Json $sceneScreen) + '}}]'))
    Assert-DrawSafe "$tag preset"
}
function Assert-Refused([string]$where) {
    $r = Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.Draw"}}]'
    Start-Sleep -Milliseconds 500
    Assert-DrawSafe "$where atalho"
    $st = Shell-State
    Check "${where}: TriggerHotkeyByName VoltzStudio.Draw recusado (modo $($st.modo_desenho), recusa '$($st.desenho_recusa)')" ($r.Count -eq 1 -and $r[0].ok -and (-not $st.modo_desenho) -and $st.desenho_recusa -eq "teste")
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
function Read-Jsonl([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return ,@() }
    return ,@(Get-Content -LiteralPath $path -Encoding UTF8 | Where-Object { $_ } | ForEach-Object { $_ | ConvertFrom-Json })
}
function Has([object]$obj, [string]$name) { return ($null -ne $obj) -and ($obj.PSObject.Properties.Name -contains $name) }
# basic.ini do perfil (o unico perfil da config limpa)
function Profile-Ini {
    $ini = Get-ChildItem -LiteralPath (Join-Path $voltzCfg "basic\profiles") -Recurse -Filter basic.ini -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $ini) { return "" }
    return [IO.File]::ReadAllText($ini.FullName, [Text.Encoding]::UTF8)
}

if (Test-Path $cfg) { Move-Item $cfg $bak }
if (Test-Path $logDir) { Remove-Item -Recurse -Force $logDir }
New-Item -ItemType Directory -Force $logDir | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null
$proc = $null
$script:job = $null
$textStyle = '{"ferr":"texto","cor":"#D4F53C","esp":4,"somem_apos_s":0,"tam":96,"fonte":"dm_sans_negrito"}'
try {
    # ---------------- Fase A: texto ao vivo (sobreposicao 2560x1440, escala 1) e gravacao ----------------
    Set-TestRect 2560 1440
    New-CleanConfig "NextProjectName=Smoke texto`r`n"
    Set-TestEnv
    Check "A: app aberto" (Start-App "A")
    Apply-Preset "A"
    Prepare-Recording
    Assert-Refused "A"
    [void](VendorShell "SetAnnotationStyle" (ConvertTo-Json @{ estilo = $textStyle } -Compress))
    $st = Shell-State
    Check "A: estilo do texto aplicado ($($st.estilo))" ($st.estilo -eq $textStyle)

    [void](VendorShell "InjectText" (Text-Json "[[400,300]]" "Voltz" -Open))
    $st = Shell-State
    Assert-DrawSafe "A texto aberto"
    $last = if ($st.ultimo) { $st.ultimo | ConvertFrom-Json } else { $null }
    Check "A: texto em andamento (digitando $($st.digitando), cursor $(@($st.cursor_texto) -join ','))" ($st.digitando -and @($st.cursor_texto).Count -eq 4)
    Check "A: traco de texto com o estilo (ferr texto, tam 96, negrito, ancora 400,300)" ($null -ne $last -and $last.ferr -eq "texto" -and $last.texto -eq "Voltz" -and $last.tam -eq 96 -and $last.fonte -eq "dm_sans_negrito" -and $last.pts[0][0] -eq 400 -and $last.pts[0][1] -eq 300)
    $caret = @($st.cursor_texto)
    $caretMid = [int](($caret[1] + $caret[3]) / 2)
    [void](VendorShell "ProbeOverlay" (ConvertTo-Json @{ pts = (Grid 400 760 6 310 400 6) } -Compress))
    $st = Shell-State
    $inked = @($st.amostras | Where-Object { $_ -eq "#FFD4F53C" }).Count
    Check "A: sobreposicao com as letras ($inked amostras lima de $(@($st.amostras).Count))" ($inked -ge 20)
    [void](VendorShell "ProbeOverlay" (ConvertTo-Json @{ pts = ("[[{0},{1}],[1500,700],[2000,1200]]" -f $caret[0], $caretMid) } -Compress))
    $st = Shell-State
    Check "A: cursor de digitacao so na sobreposicao ($($st.amostras -join ','))" ($st.amostras[0] -eq "#FFD4F53C" -and $st.amostras[1] -eq "#00000000" -and $st.amostras[2] -eq "#00000000")

    $partialsBefore = [int]$st.sobreposicao.parciais
    [void](VendorShell "TypeText" '{"texto":"Voltz Studio"}')
    $st = Shell-State
    Assert-DrawSafe "A digitando"
    $last = $st.ultimo | ConvertFrom-Json
    Check "A: TypeText troca o texto ($($last.texto)); redesenho parcial ($partialsBefore -> $($st.sobreposicao.parciais))" ($last.texto -eq "Voltz Studio" -and [int]$st.sobreposicao.parciais -gt $partialsBefore)
    $feed = Feed-State
    Check "A: o plugin tem o texto ($($feed.tracos) traco(s), ids $($feed.ids -join ','))" ($feed.tracos -eq 1)
    $img = Scene-Image $sceneScreen
    $lime = Lime-Count $img 95 300 72 110
    $img.Save((Join-Path $logDir "programa-texto.png")); $img.Dispose()
    Check "A: programa com o texto ($lime pixels lima na regiao do texto)" ($lime -ge 40)

    [void](VendorShell "CommitText")
    $st = Shell-State
    Assert-DrawSafe "A confirmado"
    Check "A: confirmar termina a digitacao (digitando $($st.digitando), aberto $($st.aberto), cursor $(@($st.cursor_texto).Count))" ((-not $st.digitando) -and $st.aberto -eq 0 -and @($st.cursor_texto).Count -eq 0)

    # acentos e duas linhas (conferidos no anotacoes.jsonl, lido do arquivo em UTF-8)
    [void](VendorShell "InjectText" (Text-Json "[[400,700]]" $accentText))
    $st = Shell-State
    Assert-DrawSafe "A acentos"
    Check "A: texto com acentos confirmado ($(@($st.visiveis).Count) na tela)" (@($st.visiveis).Count -eq 2 -and -not $st.digitando)

    # gravacao: os dois textos de antes (t_ini 0), um digitado gravando, um cancelado com algo (t_limpo) e um vazio
    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "A: StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 2
    [void](VendorShell "InjectText" (Text-Json "[[1400,300]]" "Aula" -Open))
    [void](VendorShell "TypeText" '{"texto":"Aula 4D"}')
    [void](VendorShell "CommitText")
    Start-Sleep -Milliseconds 500; Assert-DrawSafe "A gravando 1"
    [void](VendorShell "InjectText" (Text-Json "[[1400,600]]" "x" -Open))
    [void](VendorShell "CancelText")
    Start-Sleep -Milliseconds 500; Assert-DrawSafe "A gravando 2"
    [void](VendorShell "InjectText" (Text-Json "[[1400,900]]" "" -Open))
    [void](VendorShell "CancelText")
    $st = Shell-State
    Assert-DrawSafe "A gravando 3"
    Check "A: cancelados saem ($(@($st.visiveis).Count) na tela, digitando $($st.digitando))" (@($st.visiveis).Count -eq 3 -and -not $st.digitando)
    Start-Sleep -Seconds 1
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "A: StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 5
    $ov = $st.sobreposicao
    $feed = Feed-State
    Write-Host ("A: sobreposicao: {0} parciais (media {1} us, max {2} us), {3} completa(s) (max {4} us); fonte: max {5} us" -f $ov.parciais, $ov.parcial_us_media, $ov.parcial_us_max, $ov.completas, $ov.completa_us_max, $feed.raster_us_max)
    Check "A: redesenho do texto abaixo de 100 ms (sobreposicao max $($ov.parcial_us_max) us, fonte max $($feed.raster_us_max) us)" ([int64]$ov.parcial_us_max -lt 100000 -and [int64]$feed.raster_us_max -lt 100000)
    Check "A: encerrou limpo" (Close-App "A")
    $dirs = @(Get-ChildItem -LiteralPath $OutDir -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "*Smoke texto*" -and (Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz")) })
    Check "A: uma pasta de projeto" ($dirs.Count -eq 1)
    if ($dirs.Count -eq 1) {
        $lines = Read-Jsonl (Join-Path $dirs[0].FullName "anotacoes.jsonl")
        $texts = @($lines | Where-Object { $_.ferr -eq "texto" })
        $byText = @{}
        foreach ($l in $texts) { $byText[$l.texto] = $l }
        Check "A: anotacoes.jsonl com 4 textos ($(@($texts | ForEach-Object { $_.texto }) -join ' | '))" ($texts.Count -eq 4)
        Check "A: textos de antes da gravacao com t_ini 0" ($byText.ContainsKey("Voltz Studio") -and $byText["Voltz Studio"].t_ini -eq 0 -and $byText.ContainsKey($accentText) -and $byText[$accentText].t_ini -eq 0)
        Check "A: acentos e quebra de linha preservados" ($byText.ContainsKey($accentText) -and $byText[$accentText].tam -eq 96 -and $byText[$accentText].fonte -eq "dm_sans_negrito")
        Check "A: digitado gravando com t_ini > 0 e t_fim" ($byText.ContainsKey("Aula 4D") -and $byText["Aula 4D"].t_ini -gt 0 -and (Has $byText["Aula 4D"] "t_fim"))
        Check "A: cancelado com algo tem t_limpo; o vazio nao entra" ($byText.ContainsKey("x") -and (Has $byText["x"] "t_limpo") -and -not $byText.ContainsKey(""))
    }
    Check "A: log - texto comecado/confirmado/cancelado" ((LogHas "A" "[voltz] anotacoes: texto comecado (tam 96, fonte dm_sans_negrito)") -and (LogHas "A" "[voltz] anotacoes: texto confirmado (12 caractere(s))") -and (LogHas "A" "[voltz] anotacoes: texto cancelado"))
    Check "A: log sem o conteudo digitado (privacidade)" (-not (LogHas "A" "Aula 4D") -and -not (LogHas "A" "linha 2"))
    Check "A: log - o modo desenho nunca ligou" (-not (LogHas "A" "[voltz] modo desenho: ligado"))

    # ---------------- Fase B: Ajustes (atalhos e anotacoes) e a barra com o texto ----------------
    Set-TestRect 640 360
    New-CleanConfig "NextProjectName=Smoke texto B`r`n"
    Set-TestEnv
    Check "B: app aberto" (Start-App "B")
    $defaultDraw = '{"bindings":[{"shift":true,"control":true,"key":"OBS_KEY_D"}]}'
    $hk = Hotkeys
    Check "B: atalhos padrao ao vivo (desenhar $(BindingsText $hk 'VoltzStudio.Draw'))" ((BindingsText $hk "VoltzStudio.Draw") -eq $defaultDraw -and $hk.'VoltzStudio.AddMarker'.bindings[0].key -eq "OBS_KEY_M" -and $hk.'VoltzStudio.ToggleSpotlight'.bindings[0].key -eq "OBS_KEY_H" -and @($hk.'VoltzStudio.UndoAnnotation'.bindings).Count -eq 0 -and -not (Has $hk "VoltzStudio.DebugFreezeUi"))
    Check "B: Ajustes (UI Automation)" (Uia "Ajustes")
    Start-Sleep -Seconds 1
    Check "B: secoes Anotacoes e Atalhos presentes (so leitura)" ((UiaExists $textSizeName) -and (UiaExists $previewName) -and (UiaExists $editDraw))
    Assert-DrawSafe "B ajustes"
    Check "B: Limpar atalho de desenhar (UI Automation)" (Uia $clearDraw)
    $hk = Hotkeys
    $ini = Profile-Ini
    Check "B: atalho de desenhar limpo ao vivo e no perfil ($(BindingsText $hk 'VoltzStudio.Draw'))" ((BindingsText $hk "VoltzStudio.Draw") -eq '{"bindings":[]}' -and $ini.Contains('VoltzStudio.Draw={"bindings":[]}'))
    Assert-Refused "B sem atalho"
    Check "B: Restaurar padrao (UI Automation)" (Uia $resetDraw)
    $hk = Hotkeys
    $ini = Profile-Ini
    Check "B: atalho de desenhar de volta ao padrao ao vivo e no perfil" ((BindingsText $hk "VoltzStudio.Draw") -eq $defaultDraw -and $ini.Contains("VoltzStudio.Draw=$defaultDraw"))
    Assert-Refused "B restaurado"
    $bar = VendorShell "ToolbarImage" '{"ferramenta":"texto","seletor":true}'
    if ($bar.png) { [IO.File]::WriteAllBytes((Join-Path $logDir "barra-texto.png"), [Convert]::FromBase64String($bar.png)) }
    Check "B: barra com o texto e o seletor aberto, fora da tela e sem janela nativa ($($bar.largura)x$($bar.altura), nativa $($bar.janela_nativa))" ($bar.largura -ge 400 -and $bar.altura -ge 150 -and -not $bar.janela_nativa)
    Assert-DrawSafe "B barra"
    Check "B: encerrou limpo" (Close-App "B")
    Check "B: log - atalho limpo e restaurado" ((LogHas "B" '[voltz] atalhos: VoltzStudio.Draw = {"bindings":[]}') -and (LogHas "B" "[voltz] atalhos: VoltzStudio.Draw = $defaultDraw"))
    Check "B: log - o modo desenho nunca ligou" (-not (LogHas "B" "[voltz] modo desenho: ligado"))
    Check "B: log - os atalhos nunca foram pausados (campo de atalho nunca focado)" (-not (LogHas "B" "[voltz] atalhos: editando"))
} catch {
    Check "sem excecao ($($_.Exception.Message))" $false
} finally {
    if ($script:proc -and -not $script:proc.HasExited) { [void](Close-App "final") }
    if ($script:job) { Stop-Watchdog "final" }
    Remove-Item Env:VOLTZ_TEST_LIVE, Env:VOLTZ_TEST_PRESET_STANDINS, Env:VOLTZ_TEST_OVERLAY_RECT -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 5
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    if (Test-Path $bak) { Move-Item $bak $cfg }
    if (-not $Keep -and (Test-Path -LiteralPath $OutDir)) { Remove-Item -Recurse -Force -LiteralPath $OutDir; Write-Host "apagado: $OutDir" }
    if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) {
        Remove-Item -Force -LiteralPath $voltzVideos
        Write-Host "apagado (vazio): $voltzVideos"
    }
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
Write-Host "SMOKE TEXTO OK"
```

- [ ] **Step 2: O smoke do desenho com o estilo novo**

Em `tools/voltz-smoke-draw.ps1`:
trocar:
```powershell
    }
    $style = '{"ferr":"seta","cor":"#4DA3FF","esp":8,"somem_apos_s":2}'
    [void](VendorShell "SetAnnotationStyle" (ConvertTo-Json @{ estilo = $style } -Compress))
```
por:
```powershell
    }
    $style = '{"ferr":"seta","cor":"#4DA3FF","esp":8,"somem_apos_s":2,"tam":48,"fonte":"dm_sans_negrito"}'
    [void](VendorShell "SetAnnotationStyle" (ConvertTo-Json @{ estilo = $style } -Compress))
```

- [ ] **Step 3: Conferir os scripts sem rodar**

```bash
grep -nP '[^\x00-\x7F]' tools/voltz-smoke-text.ps1 tools/voltz-smoke-draw.ps1; echo "nao-ascii=$?"
grep -nE "SendInput|SetCursorPos|mouse_event|keybd_event|SendKeys|SendWait|Cursor\]::Position|SetForegroundWindow|SetFocus|BlockInput|ClipCursor|taskkill|Stop-Process -Name|explorer" tools/voltz-smoke-text.ps1; echo "proibidos=$?"
grep -c "Assert-DrawSafe" tools/voltz-smoke-text.ps1
```
```powershell
foreach ($n in "voltz-smoke-text.ps1", "voltz-smoke-draw.ps1") { $t = $null; $e = $null; [void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path "tools\$n"), [ref]$t, [ref]$e); "$n erros: $($e.Count)" }
```
Expected: `nao-ascii=1`; `proibidos=1`; `15` linhas com `Assert-DrawSafe`; `erros: 0` nos dois.

- [ ] **Step 4: Rodar o smoke do texto**

Run: COMPILAR APP (nada de C++ mudou desde a Task 9: só confirma `exit=0`); RODAR TESTES (`203 | 203 passed`, a
contagem não muda nesta task nem na 11). Sem nenhum Voltz/OBS aberto: SMOKE TEXTO.
Expected: todas as linhas `OK`, a última `SMOKE TEXTO OK`; entre elas `A: sobreposicao com as letras (... amostras lima
...)`, `A: cursor de digitacao so na sobreposicao (#FFD4F53C,#00000000,#00000000)`, `A: anotacoes.jsonl com 4 textos`,
`A: log sem o conteudo digitado (privacidade)`, `B: atalho de desenhar limpo ao vivo e no perfil`,
`B: log - os atalhos nunca foram pausados (campo de atalho nunca focado)`, `seguranca: N Assert-DrawSafe, 0
fechamento(s) forcado(s)`, `Videos intacta` e `config portatil restaurada igual`. Guarde a linha
`A: sobreposicao: N parciais (media X us, max Y us), ... ; fonte: max Z us` (vai para o backlog na Task 12). Abra
`%TEMP%\voltz-smoke-text-logs\barra-texto.png` e `programa-texto.png` e descreva-os.

- [ ] **Step 5: Rodar o smoke do desenho (estilo novo)**

Run: SMOKE DESENHO.
Expected: `SMOKE DESENHO OK` (em especial `A: estilo aplicado (...,"tam":48,"fonte":"dm_sans_negrito")` e
`A: user.ini com o AnnotationStyle da barra`).

- [ ] **Step 6: Grep de segurança de todo o código**

Run: GREP DE SEGURANÇA (A–F).
Expected: como em Convenções.

- [ ] **Step 7: Commit**

```bash
git add tools/voltz-smoke-text.ps1 tools/voltz-smoke-draw.ps1
git commit -m "test(voltz): smoke do texto e dos Ajustes (texto ao vivo e gravado, cursor so na sobreposicao, log sem o texto, atalhos limpar/restaurar, barra com o seletor)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 11: Demonstração manual ampliada (`tools/voltz-draw-demo.ps1`) e roteiro

**Files:**
- Modify: `tools/voltz-draw-demo.ps1`

A demo do 4C passa a explicar o texto, o seletor, a linha de dica e os Ajustes; o `-Log` mostra também as linhas
`[voltz] atalhos` (o texto digitado nunca aparece: só o tamanho); `-Hotkeys` (novo) mostra só leitura os atalhos do
Voltz gravados no perfil da config portátil.

- [ ] **Step 1: Demo**

Em `tools/voltz-draw-demo.ps1`:
1. trocar:
```powershell
<#
  Demonstracao MANUAL do modo desenho (Plano 4C): o usuario abre, liga o modo desenho e desenha. Nao e teste automatico.
  -Launch: guarda a config portatil (config.drawdemo-bak), cria uma limpa (pt-BR, sem confirmacao ao sair) e abre o
```
por:
```powershell
<#
  Demonstracao MANUAL do modo desenho (Planos 4C e 4D): o usuario abre, liga o modo desenho, desenha e escreve. Nao e
  teste automatico.
  -Launch: guarda a config portatil (config.drawdemo-bak), cria uma limpa (pt-BR, sem confirmacao ao sair) e abre o
```
2. trocar:
```powershell
    VOLTZ_DEBUG_FREEZE_UI: o atalho Ctrl+Shift+F12 trava a interface por 8 s (para ver o vigia do modo desenho).
  -Log: mostra as linhas do log do Voltz aberto sobre o modo desenho, a barra, as anotacoes e a depuracao (as medidas de
    redesenho aparecem na linha "modo desenho: desligado (...)", ao sair do modo).
  -Close: fecha o Voltz aberto pelo -Launch (PID salvo, WM_CLOSE na janela da casca guardada) e restaura a config
```
por:
```powershell
    VOLTZ_DEBUG_FREEZE_UI: o atalho Ctrl+Shift+F12 trava a interface por 8 s (para ver o vigia do modo desenho).
  -Log: mostra as linhas do log do Voltz aberto sobre o modo desenho, a barra, as anotacoes, os atalhos e a depuracao
    (as medidas de redesenho aparecem na linha "modo desenho: desligado (...)", ao sair do modo). O texto digitado nunca
    vai para o log: so o tamanho ("texto confirmado (N caractere(s))").
  -Hotkeys: mostra os atalhos do Voltz gravados no perfil da config portatil ([Hotkeys] VoltzStudio.* do basic.ini; so
    leitura). Atalho que nao aparece = ainda no padrao.
  -Close: fecha o Voltz aberto pelo -Launch (PID salvo, WM_CLOSE na janela da casca guardada) e restaura a config
```
3. trocar:
```powershell
       monitor da Tela; o cursor vira uma cruz so nesse monitor (os outros continuam livres).
    3. Desenhe com o botao esquerdo: caneta, marca-texto, linha, seta, retangulo e elipse; cores, espessura, desfazer
       (Ctrl+Z ou o botao), limpar e "tracos somem apos" ficam na barra.
    4. Rode -Log para ver o que o Voltz registrou e -Close para fechar e restaurar a config.

  COMO SAIR DO MODO DESENHO:
```
por:
```powershell
       monitor da Tela; o cursor vira uma cruz so nesse monitor (os outros continuam livres).
    3. Desenhe com o botao esquerdo: caneta, marca-texto, linha, seta, retangulo e elipse; cores (4 + 8 + a cor
       personalizada, com o seletor completo), espessura, desfazer (Ctrl+Z ou o botao), limpar e "tracos somem apos"
       ficam na barra. A linha de baixo da barra mostra o que cada botao faz.
    4. Texto: escolha a ferramenta Texto (T) na barra, clique onde o texto comeca e digite (acentos valem). Enter
       confirma, Shift+Enter quebra a linha, Backspace apaga, Esc (ou Ctrl+Z) cancela so o texto - o Esc seguinte sai do
       modo. Tamanho (4 botoes "A") e negrito ficam na barra no lugar das espessuras.
    5. Ajustes > Anotacoes (estilo com que o modo comeca, com previa) e Ajustes > Atalhos (clique no campo e aperte a
       combinacao; enquanto o campo esta ativo os atalhos ficam pausados).
    6. Rode -Log para ver o que o Voltz registrou, -Hotkeys para ver os atalhos gravados e -Close para fechar e
       restaurar a config.

  COMO SAIR DO MODO DESENHO:
```
4. trocar:
```powershell
       powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Log
       powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Close
```
por:
```powershell
       powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Log
       powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Hotkeys
       powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Close
```
5. trocar:
```powershell
    [switch]$Log,
    [switch]$Close,
```
por:
```powershell
    [switch]$Log,
    [switch]$Hotkeys,
    [switch]$Close,
```
6. trocar:
```powershell
    Write-Host "  2. Ligue o modo desenho com Ctrl+Shift+D ou com o botao Desenhar da tela Gravar e desenhe com o botao esquerdo."
    Write-Host "  Para sair do modo (sempre): clique direito, Ctrl+Shift+D de novo, o botao Sair (X) da barra ou 30 s parado."
```
por:
```powershell
    Write-Host "  2. Ligue o modo desenho com Ctrl+Shift+D ou com o botao Desenhar da tela Gravar e desenhe com o botao esquerdo."
    Write-Host "  3. Texto: ferramenta T na barra, clique e digite; Enter confirma, Shift+Enter quebra a linha, Esc cancela o texto."
    Write-Host "  Para sair do modo (sempre): clique direito, Ctrl+Shift+D de novo, o botao Sair (X) da barra ou 30 s parado."
```
7. trocar:
```powershell
    if ($Freeze) { Write-Host "  Depuracao: Ctrl+Shift+F12 trava a interface por 8 s (com o modo ligado, ele deve sair sozinho em ~1 s)." }
    Write-Host "  Depois: -Log mostra o que o Voltz registrou; -Close fecha o Voltz e restaura a config portatil."
    exit 0
```
por:
```powershell
    if ($Freeze) { Write-Host "  Depuracao: Ctrl+Shift+F12 trava a interface por 8 s (com o modo ligado, ele deve sair sozinho em ~1 s)." }
    Write-Host "  Depois: -Log mostra o que o Voltz registrou; -Hotkeys, os atalhos gravados; -Close fecha o Voltz e restaura a config portatil."
    exit 0
```
8. trocar:
```powershell
    Write-Host "log: $($last.FullName)"
    Select-String -LiteralPath $last.FullName -Encoding UTF8 -Pattern '\[voltz\] (modo desenho|barra do modo desenho|anotacoes|janelas fantasma|depuracao)|\[voltz-sources\] anotacoes' | ForEach-Object { $_.Line }
    exit 0
```
por:
```powershell
    Write-Host "log: $($last.FullName)"
    Select-String -LiteralPath $last.FullName -Encoding UTF8 -Pattern '\[voltz\] (modo desenho|barra do modo desenho|anotacoes|atalhos|janelas fantasma|depuracao)|\[voltz-sources\] anotacoes' | ForEach-Object { $_.Line }
    exit 0
}

if ($Hotkeys) {
    $profiles = Join-Path $cfg "Voltz Studio\basic\profiles"
    $ini = Get-ChildItem -LiteralPath $profiles -Recurse -Filter basic.ini -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $ini) { throw "nenhum perfil em $profiles (rode -Launch antes)" }
    Write-Host "perfil: $($ini.FullName)"
    $lines = @(Get-Content -LiteralPath $ini.FullName -Encoding UTF8 | Where-Object { $_ -like 'VoltzStudio.*' })
    if ($lines.Count -eq 0) { Write-Host "(nenhum atalho do Voltz gravado: todos no padrao)" } else { $lines | ForEach-Object { Write-Host $_ } }
    exit 0
```
9. trocar:
```powershell
}

Write-Host "use -Launch [-Freeze], -Log ou -Close (veja o comentario no topo do script)"
```
por:
```powershell
}

Write-Host "use -Launch [-Freeze], -Log, -Hotkeys ou -Close (veja o comentario no topo do script)"
```

- [ ] **Step 2: Conferir ASCII, segurança e sintaxe (sem abrir o Voltz)**

```bash
grep -nP '[^\x00-\x7F]' tools/voltz-draw-demo.ps1; echo "nao-ascii=$?"
grep -nE "SendInput|SetCursorPos|mouse_event|keybd_event|SendKeys|Cursor\]::Position|SetForegroundWindow|BlockInput|ClipCursor|TriggerHotkey|taskkill|Stop-Process -Name|explorer" tools/voltz-draw-demo.ps1; echo "proibidos=$?"
```
```powershell
$t = $null; $e = $null; [void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path "tools\voltz-draw-demo.ps1"), [ref]$t, [ref]$e); "erros: $($e.Count)"
powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Hotkeys
```
Expected: `nao-ascii=1`; `proibidos=1`; `erros: 0`; o `-Hotkeys` só lê: sem config portátil, termina com
`nenhum perfil em ...\config\Voltz Studio\basic\profiles (rode -Launch antes)`; com uma config portátil, lista as linhas
`VoltzStudio.*` do perfil (ou `(nenhum atalho do Voltz gravado: todos no padrao)`). **Não** rode o `-Launch` neste
passo: a demo abre o Voltz sem `VOLTZ_TEST_LIVE` (o modo desenho poderia ligar por um Ctrl+Shift+D do usuário) — ela é
do roteiro manual.

- [ ] **Step 3: Commit**

```bash
git add tools/voltz-draw-demo.ps1
git commit -m "feat(voltz): demo manual do modo desenho com texto, seletor de cor, Ajustes e -Hotkeys

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 12: Verificação, registro, poda do `CLAUDE.md` e push

**Files:** `docs/voltz/backlog-plano-4.md`, `docs/voltz/upstream-patches.md` (conferência), `CLAUDE.md`, `AGENTS.md`,
`docs/HISTORICO.md`

- [ ] **Step 1:** COMPILAR TUDO (segundo plano), RODAR TESTES (esperado **203** casos), SMOKE, SMOKE GRAVAÇÃO, SMOKE
  PROJETO, SMOKE PROJETO `-ProgramOnly`, SMOKE MOUSE, SMOKE ANOTAÇÕES, SMOKE DESENHO e SMOKE TEXTO (um de cada vez,
  nunca dois apps ao mesmo tempo). Todos OK. GREP DE SEGURANÇA (A–F) como em Convenções. Paridade dos locales → sem
  saída.
- [ ] **Step 2:** Imagens finais **só da janela** da casca e da barra fora da tela (bloco de teste,
  `$ovW = 640; $ovH = 360`; no `try`, no lugar do comentário, depois do `Start-App`):
```powershell
  [void](Uia "Só tela / Só câmera — Aplicar"); Start-Sleep 3; Assert-NoRealCapture
  $bar = VendorShell "ToolbarImage" '{"ferramenta":"texto","seletor":true}'
  [IO.File]::WriteAllBytes("$sp\4d-barra-final.png", [Convert]::FromBase64String($bar.png)); "barra nativa=$($bar.janela_nativa)"
  [void](Uia "Ajustes"); Start-Sleep 1; Assert-DrawSafe "ajustes"
  $r = New-Object VoltzProbe+RECT; [void]$P::GetWindowRect($script:shell, [ref]$r)
  $b = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [System.Drawing.Graphics]::FromImage($b)
  Add-Type -Namespace V -Name Pw -MemberDefinition '[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);'
  $d = $g.GetHdc(); [void][V.Pw]::PrintWindow($script:shell, $d, 2); $g.ReleaseHdc($d); $g.Dispose(); $b.Save("$sp\4d-ajustes-final.png"); $b.Dispose()
  Close-App
```
  `barra nativa=False`. Descreva no relatório a barra (T marcado, "A" e B, seletor aberto, linha de dica) e o topo dos
  Ajustes. Nenhuma captura da tela inteira, nenhum quadro de vídeo, nenhuma captura da sobreposição.
- [ ] **Step 3:** Nenhum arquivo do OBS mudou:
  `git diff a6d2655e4..HEAD --stat -- . ':(exclude)frontend/voltz' ':(exclude)plugins/voltz-sources' ':(exclude)docs' ':(exclude)tools' ':(exclude)frontend/data/locale/voltz-pt-BR.ini' ':(exclude)frontend/data/locale/voltz-en-US.ini' ':(exclude)frontend/cmake/feature-voltz.cmake' ':(exclude)CLAUDE.md' ':(exclude)AGENTS.md'`
  → sem saída; `git diff a6d2655e4..HEAD -- plugins/CMakeLists.txt plugins/voltz-sources docs/voltz/upstream-patches.md`
  → sem saída (o plugin ganhou o texto só pelo `voltz-core`).
- [ ] **Step 4: Backlog do Plano 4** — em `docs/voltz/backlog-plano-4.md`:
  1. Logo depois da seção `## Feito no Plano 4C` (antes de `## Próximo: Plano 4D (texto, Ajustes, todos os monitores)`),
     acrescentar:
```markdown
## Feito no Plano 4D
- **Núcleo:** texto no modelo (`"ferr":"texto"`, `"texto"`, `"tam"`, `"fonte"` no `anotacoes.jsonl`; âncora em
  `"pts"`), UTF-8 e limites (`annotation-text`: 280 caracteres, 12 linhas), estilo com tamanho/fonte do texto, texto em
  andamento no `StrokeStore`; layout com métricas por interface (`text-layout`); FreeType das deps + DM Sans Regular/Bold
  embutidas no `voltz-core` (`embed-fonts.cmake`, `annotation-font`) com o texto no rasterizador (mesmo desenho na
  sobreposição e na fonte do plugin, parcial = inteiro byte a byte); digitação na `DrawSession`
  (`TextBegin/TextEdit/TextCommit/TextCancel`, Esc e Ctrl+Z cancelam só o texto, toda saída confirma); atalhos
  (`hotkey-bindings`: tabela com os padrões, formato do perfil, regra de combinação, conflitos).
- **Casca:** texto ao vivo (`AnnotationController`: texto inteiro ao plugin a cada mudança, cursor só na sobreposição,
  conteúdo nunca no log); a janela de entrada traduz teclas (`TranslateMessage`/`WM_CHAR`, substitutos, `WM_SYSCHAR`
  engolido, cursor em I) sem nenhuma API de foco nova; barra com o T, tamanhos e negrito, seletor de cor completo
  (`ColorPicker`, sem diálogo) e linha de dica; Ajustes › Anotações (estilo com prévia pelo núcleo, estado, atalho) e
  Ajustes › Atalhos (`OBSHotkeyEdit` sem patch, atalhos pausados com o campo em foco, validação, conflitos, limpar e
  restaurar); marcador e holofote carregam o atalho pelo `HotkeyBinding`.
- Testes: 203 casos; `tools/voltz-smoke-text.ps1` (fases A–B); `tools/voltz-draw-demo.ps1 -Hotkeys`; pedidos novos
  `InjectText`, `TypeText`, `CommitText`, `CancelText`, `HotkeyState` e `ToolbarImage {ferramenta, seletor}`.
```
  2. Trocar a seção `## Próximo: Plano 4D (texto, Ajustes, todos os monitores)` inteira (título e parágrafo) por:
```markdown
## Próximo: Plano 4E (todos os monitores, barra arrastável)
Desenho e sobreposição em todos os monitores (traços fora da captura só na sobreposição, em pixels do desktop; nada no
`anotacoes.jsonl` nem no programa), janela de entrada sobre mais de um monitor, barra arrastável com posição salva por
monitor e o buraco acompanhando; geometria no núcleo e monitores falsos fora do desktop virtual nos testes. Ver o fim de
`docs/superpowers/plans/2026-10-17-voltz-plano-4d-texto-ajustes.md`.
```
  3. Em `## Pendências e limites do 4C`, apagar o item `- As dicas (tooltips) da barra não aparecem: ...` (2 linhas:
     resolvido pela linha de dica) e, no item `- A barra fica no alto e ao centro do monitor da Tela (não arrasta); ...`,
     trocar `sem diálogo de cor (4 + 8 cores); ` por nada (o seletor completo entrou no 4D). Acrescentar no fim dessa
     seção: `- Decisão (4D): o modo desenho não sai ao começar/parar a gravação (pelo botão Gravar já sai por foco; pelo
     atalho, quem desenha quer continuar).`
  4. No fim do arquivo, acrescentar (com os números do Step 4 da Task 10 no item das medidas):
```markdown
## Pendências e limites do 4D
- Texto sem kerning, ligaduras e escritas complexas (layout próprio, sem HarfBuzz); caractere que a DM Sans não tem
  (emoji, CJK) vira o retângulo `.notdef`. Só as duas fontes embutidas (DM Sans e DM Sans Negrito).
- Sem quebra automática, cursor só no fim, sem seleção nem colar (Ctrl+V não cola); até 280 caracteres e 12 linhas;
  texto confirmado não se edita (desfazer e escrever de novo). Ctrl+Z durante a digitação cancela o texto inteiro.
- IME (chinês, japonês): a composição usa a janela padrão do Windows, no canto; não testado.
- Pelo atalho sem clique antes (janela de entrada ainda não ativa), as teclas vão para o app em uso: o primeiro clique
  ativa e já começa o texto (limite do 4C).
- Sobreposição com escala ≠ 1 (monitor ≠ captura): o tamanho do texto usa a escala média; pode diferir ~1 px do programa.
- Ajustes › Atalhos edita só a primeira tecla de cada atalho (as outras, do modo clássico, ficam e aparecem como "+N");
  combinação sem Ctrl/Alt/Windows é recusada (sozinhas, só F1–F24 e mouse lateral); conflito só avisa; o campo só ganha
  foco por clique e, com o foco, **todos** os atalhos do OBS ficam pausados até sair do campo.
- O seletor de cor não tem campo hexadecimal nem lista de cores recentes; em Ajustes a cor vem do diálogo do Qt.
- A seção Anotações e a de Atalhos ficam abaixo de Mouse: a rolagem e a edição por teclado só foram vistas no roteiro.
- Medidas do smoke do texto (sobreposição 2560×1440, texto 96 px): parciais com média e máximo da linha
  `A: sobreposicao: ...` da Task 10 e o máximo da fonte do plugin (`fonte: max ... us`).
- Pendências do 4B/4C que continuam: limite de número de traços, "somem após" sem animação, monitor trocado no meio da
  gravação não vira evento, pedidos de teste da casca enfileirados (~400 ms), vigia sai em travadas legítimas, atalhos
  do OBS não consomem a tecla.

## Testes manuais pendentes do 4D
- Todo o "Roteiro de teste manual do Plano 4D" do fim do plano (digitar de verdade, teclas mortas e AltGr, Shift+Enter,
  Esc/Ctrl+Z só no texto, saídas com o texto aberto, seletor de cor arrastando, linha de dica, gravação com texto,
  Ajustes › Anotações e › Atalhos com o teclado, roda do mouse em Ajustes, atalho novo com outro app em foco, modo
  clássico, 125/150%, en-US).
```
- [ ] **Step 5: `CLAUDE.md` — atualização e poda** (depois `cp CLAUDE.md AGENTS.md`). O alvo é **≤ 12 000 bytes**
  (`wc -c CLAUDE.md`; hoje 11 993: a poda é obrigatória). Em `docs/HISTORICO.md`, acrescentar no fim a seção
  `## Poda do Plano 4D (texto que saiu do CLAUDE.md, 2026-10-17)` e mover para ela, **verbatim**, cada trecho trocado ou
  apagado abaixo (conferido ao escrever o plano: com estas trocas o arquivo fica com **11 629 bytes**). Em `CLAUDE.md`, na
  ordem:
1. trocar
```markdown
    `voltz-smoke-mouse -OutDir <vazia>` · `voltz-smoke-annotations -OutDir <vazia>` · `voltz-smoke-draw`; demos manuais:
    `voltz-annot-demo`, `voltz-draw-demo` (modo desenho, sem variável de teste).
```
por
```markdown
    `voltz-smoke-mouse -OutDir <vazia>` · `voltz-smoke-annotations -OutDir <vazia>` · `voltz-smoke-draw` ·
    `voltz-smoke-text -OutDir <vazia>`; demos manuais: `voltz-annot-demo`, `voltz-draw-demo` (desenho e texto, sem
    variável de teste; `-Hotkeys`).
```
2. trocar
```markdown
- Planos 4B–4C concluídos: anotações (núcleo, rasterizador com retângulo sujo, `anotacoes.jsonl`, fonte Voltz
  Anotações, sobreposição fora da captura) e modo desenho (Ctrl+Shift+D/botão Desenhar, janela de entrada em thread
  própria, saídas garantidas, barra). Testes: 170 casos + smokes de anotações e desenho.
```
por
```markdown
- Planos 4B–4D concluídos: anotações (núcleo, retângulo sujo, `anotacoes.jsonl`, fonte Voltz Anotações, sobreposição
  fora da captura), modo desenho (atalho/botão, entrada em thread própria, saídas garantidas, barra), texto, seletor de
  cor e Ajustes › Anotações/Atalhos. Testes: 203 casos + smokes de anotações, desenho e texto.
```
3. apagar (o texto vai para o HISTORICO, ver abaixo)
```markdown
- Logo: obturador de 3 lâminas lima inclinado sobre squircle preto; bolinha REC só ao gravar.
```
4. trocar
```markdown
  thread própria (saídas e vigia de 1 s pela `DrawSession` do núcleo); captura/foco só em `annotate/DrawInputWindow.cpp`.
```
por
```markdown
  thread própria (saídas e vigia de 1 s pela `DrawSession` do núcleo); captura/foco só em `annotate/DrawInputWindow.cpp`.
- Texto: FreeType das deps + DM Sans embutida no `voltz-core` (mesmos bytes na sobreposição e no plugin); digitação na
  `DrawSession` (WM_CHAR). Atalhos: tabela no núcleo (`hotkey-bindings`); Ajustes › Atalhos usa o `OBSHotkeyEdit`.
```
5. apagar (o texto vai para o HISTORICO, ver abaixo)
```markdown
- Widgets postos com `setItemWidget` não aparecem na UIA; o `SourcesPanel` usa uma fábrica `QAccessible` presa ao
  `objectName` `sourcesList` — não renomear.
```
6. apagar (o texto vai para o HISTORICO, ver abaixo)
```markdown
- Filhos de grupo emitem sinais pela fonte do grupo: ligar o `signal_handler` de cada grupo para atualizar as Fontes.
```
7. apagar (o texto vai para o HISTORICO, ver abaixo)
```markdown
- `config_set_default_*` deste libobs grava o padrão também como valor do usuário: "ainda no padrão" = comparar com
  `config_get_default_*` (`stillObsDefault()`), não `config_has_user_value`.
```
8. apagar (o texto vai para o HISTORICO, ver abaixo)
```markdown
- `WDA_EXCLUDEFROMCAPTURE` antes do Windows 10 2004 vira caixa preta: só com `SetDisplayAffinitySupported()`.
```
9. apagar (o texto vai para o HISTORICO, ver abaixo)
```markdown
- Qt 6.11: `QScreen::name()` é o nome amigável (repete em monitores iguais): achar por `HMONITOR` (`screenForMonitor`).
```
10. apagar (o texto vai para o HISTORICO, ver abaixo)
```markdown
- Thread de entrada nunca bloqueia no `DrawShared::mutex` com a janela viva (backlog + `try_lock`); sob ele só
  atribuição/swap. Gatilho enfileirado numa trava da UI chega depois do drain: recusa de 500 ms
  (`recusado (recente)`); o botão Desenhar lê o estado no `pressed`.
```
11. trocar
```markdown
- `proc_handler` não tem remoção:
```
por
```markdown
- Arquivadas do 4C (`setItemWidget`/UIA, sinais de filhos de grupo, `config_set_default_*`, WDA < 2004,
  `QScreen::name()`, mutex da entrada): `docs/HISTORICO.md` → "Poda do Plano 4D".
- `Freetype::Freetype` (importado) só existe no diretório do `find_package` e abaixo: fica no `feature-voltz.cmake`.
- Atalhos do OBS disparam enquanto se grava a tecla: o campo pausa (`DisableHotkeys`) e devolve ao perder o foco.
- `proc_handler` não tem remoção:
```
12. trocar
```markdown
1. Usuário roda o roteiro manual do 4C; depois escrever e executar o Plano 4D (texto, Ajustes › Anotações/Atalhos,
   todos os monitores) a partir do fim do plano 4C e do `docs/voltz/backlog-plano-4.md`.
```
por
```markdown
1. Usuário roda os roteiros manuais do 4C e do 4D; depois escrever e executar o Plano 4E (todos os monitores, barra
   arrastável) a partir do fim do plano 4D e do `docs/voltz/backlog-plano-4.md`.
```

  O texto que vai **verbatim** para a seção `## Poda do Plano 4D` do `docs/HISTORICO.md` (os trechos trocados e
  apagados, na ordem acima):
```markdown
    `voltz-smoke-mouse -OutDir <vazia>` · `voltz-smoke-annotations -OutDir <vazia>` · `voltz-smoke-draw`; demos manuais:
    `voltz-annot-demo`, `voltz-draw-demo` (modo desenho, sem variável de teste).

- Planos 4B–4C concluídos: anotações (núcleo, rasterizador com retângulo sujo, `anotacoes.jsonl`, fonte Voltz
  Anotações, sobreposição fora da captura) e modo desenho (Ctrl+Shift+D/botão Desenhar, janela de entrada em thread
  própria, saídas garantidas, barra). Testes: 170 casos + smokes de anotações e desenho.

- Logo: obturador de 3 lâminas lima inclinado sobre squircle preto; bolinha REC só ao gravar.

- Widgets postos com `setItemWidget` não aparecem na UIA; o `SourcesPanel` usa uma fábrica `QAccessible` presa ao
  `objectName` `sourcesList` — não renomear.

- Filhos de grupo emitem sinais pela fonte do grupo: ligar o `signal_handler` de cada grupo para atualizar as Fontes.

- `config_set_default_*` deste libobs grava o padrão também como valor do usuário: "ainda no padrão" = comparar com
  `config_get_default_*` (`stillObsDefault()`), não `config_has_user_value`.

- `WDA_EXCLUDEFROMCAPTURE` antes do Windows 10 2004 vira caixa preta: só com `SetDisplayAffinitySupported()`.

- Qt 6.11: `QScreen::name()` é o nome amigável (repete em monitores iguais): achar por `HMONITOR` (`screenForMonitor`).

- Thread de entrada nunca bloqueia no `DrawShared::mutex` com a janela viva (backlog + `try_lock`); sob ele só
  atribuição/swap. Gatilho enfileirado numa trava da UI chega depois do drain: recusa de 500 ms
  (`recusado (recente)`); o botão Desenhar lê o estado no `pressed`.

1. Usuário roda o roteiro manual do 4C; depois escrever e executar o Plano 4D (texto, Ajustes › Anotações/Atalhos,
   todos os monitores) a partir do fim do plano 4C e do `docs/voltz/backlog-plano-4.md`.
```

  Conferir `wc -c CLAUDE.md` ≤ 12000 (deve dar 11 629). Depois `cp CLAUDE.md AGENTS.md` e `diff CLAUDE.md AGENTS.md` →
  sem saída. Em `docs/HISTORICO.md`, acrescentar ainda `## Plano 4D (2026-10-17)` com 3–4 linhas (texto no núcleo com
  FreeType + DM Sans embutida e o mesmo desenho na sobreposição e no plugin; digitação na `DrawSession` sem API de foco
  nova; seletor de cor e linha de dica na barra; Ajustes › Anotações e › Atalhos com o `OBSHotkeyEdit`; 203 casos +
  smoke do texto; roteiro manual e pendências em `backlog-plano-4.md`).
- [ ] **Step 6:** Commit e push:
```bash
git add docs/voltz/backlog-plano-4.md docs/HISTORICO.md CLAUDE.md AGENTS.md
git commit -m "docs(voltz): registro do Plano 4D (backlog, memoria podada)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push origin voltz
```
Expected: push sem `--force`, `voltz -> voltz`.

---

## Roteiro de teste manual do Plano 4D (para o usuário, depois da Task 12)

Nada aqui é automatizado: é o usuário quem liga o modo desenho, digita e usa o PC. Em qualquer momento, se algo prender o
mouse ou o teclado: **clique direito**, **Ctrl+Shift+D**, o **X** da barra, **Alt+Tab** ou esperar **30 s** — e, se a
interface do Voltz travar, o modo sai sozinho em ~1 s. Lembre: com o texto aberto, o **primeiro Esc só cancela o texto**
(o segundo sai). Depois de cada passo, `tools/voltz-draw-demo.ps1 -Log` mostra o que o Voltz registrou (o texto digitado
nunca aparece: só `texto confirmado (N caractere(s))`).

1. **Preparar:** `powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Launch` → na tela Presets aplicar
   **Tutorial** (monitor e webcam reais). Abrir o Bloco de Notas no monitor capturado.
2. **Texto pelo botão:** clicar **Desenhar** na tela Gravar; na barra, escolher o **T** — sobre o monitor da Tela o
   cursor vira **I**; a linha de baixo da barra diz como digitar; no lugar das espessuras aparecem quatro "A" e o B. Clicar
   num ponto e digitar `Olá, ação, pão` (teclas mortas: ´ + a = á, ~ + a = ã; ç), um `/` com AltGr+Q, **Shift+Enter** e
   uma segunda linha; **Backspace** apaga o último caractere; **Alt+Enter** e **Alt+Backspace** não fazem nada no texto
   (não confirmam, não quebram a linha, não apagam, sem bipe). Enquanto digita: o texto aparece na **prévia** (programa)
   letra a letra; o cursor (barra vertical lima) só no monitor, nunca na prévia. **Enter** confirma: o cursor some. O texto
   começa exatamente onde foi o clique (canto de cima à esquerda).
3. **Cancelar só o texto:** clicar, digitar algo e apertar **Esc** → o texto some e o modo continua (cursor em I); **Esc**
   de novo → sai. Ligar de novo, clicar, digitar e apertar **Ctrl+Z** → o texto em andamento some; **Ctrl+Z** de novo →
   desfaz o texto anterior.
4. **Clicar de novo:** com um texto aberto, clicar em outro ponto → o primeiro fica (confirmado) e começa outro; escolher
   a **caneta** com um texto aberto e desenhar → o texto fica e o traço começa.
5. **Saídas com o texto aberto:** clique direito, Ctrl+Shift+D, o X da barra, Alt+Tab e 30 s parado — em todas o texto
   **fica** (confirmado); com o texto vazio ou **em branco** (só espaços e quebras de linha), nada fica — nem ao apertar
   Enter (`-Log`: `texto cancelado`). `-Log`: `texto confirmado (N caractere(s))`, nenhum conteúdo.
6. **Estilo durante a digitação:** com um texto aberto, clicar nos "A" (24/36/48/72; o 48 vem marcado), no B e numa
   cor → o texto em andamento muda na hora, sem perder o cursor; o Esc continua chegando (a barra nunca fica ativa).
7. **Pelo atalho sem clique antes:** com o Bloco de Notas em foco, Ctrl+Shift+D, T e digitar sem clicar → as letras vão
   para o Bloco de Notas (limite conhecido); clicar no monitor e digitar → texto do Voltz.
8. **Seletor de cor:** "mais cores" (paleta) → **conta-gotas** → aparece o quadrado e a faixa de matiz; arrastar no
   quadrado e na faixa (pode sair da área arrastando): a cor vale **ao soltar**; o conta-gotas passa a mostrar a cor
   escolhida, marcada; desenhar e escrever usam essa cor. Durante o arrasto o modo não sai e o Esc continua valendo.
9. **Linha de dica:** passar o mouse pelos botões da barra → a linha de baixo mostra o que cada um faz; nenhuma dica
   flutuante aparece; gravando, nem a barra nem a dica aparecem no vídeo.
10. **Gravação:** gravar ~20 s escrevendo dois textos (um com acento e duas linhas) e cancelando um com Esc. No player:
    `programa.mp4` **com** os textos (sem cursor, sem barra); `tela.mkv` sem nada. No `anotacoes.jsonl`: linhas com
    `"ferr":"texto"`, o texto com acentos e `\n`, `"tam"` e `"fonte"`; o cancelado com algo digitado tem `"t_limpo"`; um
    texto só com espaços confirmado com Enter não gera linha.
11. **Ajustes › Anotações:** rolar até a seção; trocar ferramenta, **cor** (diálogo), espessura, tamanho e fonte do texto
    e "somem após" → a **prévia** acompanha (traço e "Aa"); ligar o modo desenho → a barra começa com esse estilo; mudar
    na barra → a seção acompanha. O aviso de estado diz que as anotações ficam fora de `tela.mkv`.
12. **Ajustes › Atalhos:** clicar no campo de "Ligar/desligar modo desenho" e apertar **Ctrl+Alt+D** → gravado (o modo
    desenho **não** liga enquanto se aperta: `-Log` mostra `atalhos: editando (atalhos pausados)` e, ao clicar fora,
    `edicao terminada (atalhos de volta)`); a dica do botão Desenhar (tela Gravar) passa a dizer Ctrl + Alt + D; com o
    Bloco de Notas em foco, **Ctrl+Alt+D** liga o modo e **Ctrl+Shift+D** não. No campo, só **D** → aviso "Use Ctrl, Alt
    ou a tecla Windows junto…"; só **Ctrl** → "Falta a tecla…"; clicar fora → volta ao atalho salvo. Pôr **Ctrl+Shift+M**
    no desenhar → aviso amarelo "Também usado por: Voltz Studio: adicionar marcador" (e o mesmo na linha do marcador).
    "Restaurar padrão" volta a Ctrl+Shift+D; "Limpar atalho" deixa sem tecla (o botão Desenhar diz "sem atalho").
    `-Hotkeys` mostra as linhas `VoltzStudio.*` gravadas.
13. **Ajustes › Atalhos pelo teclado:** com o foco em Ajustes, **Tab** até o "Limpar atalho" de uma linha com tecla → o
    botão ganha a **borda lima** (o mesmo no "Restaurar padrão"); **Espaço** limpa só essa linha e o botão fica cinza;
    **Espaço** de novo **não** limpa nem restaura outra linha (o foco foi para a seção, não para o botão seguinte). O
    mesmo com o "Restaurar padrão". Clicar num campo de atalho (`-Log`: `editando (atalhos pausados)`) e depois no
    **fundo** da seção (fora de campo e botão) → o campo perde o foco e o `-Log` mostra `edicao terminada (atalhos de
    volta)`; o atalho de desenhar volta a funcionar.
14. **Roda do mouse em Ajustes:** com o mouse sobre uma caixa (ferramenta, fonte, "somem após" de Anotações; formato,
    qualidade, monitor, câmera, microfone) ou um slider (espessura, tamanho do texto, Mouse) **sem** ter clicado nela,
    girar a roda → a página **rola** e o valor **não** muda. Clicar na caixa/slider (fechar a lista com Esc, se abrir) e
    girar de novo com o mouse sobre ela → o valor muda. Nenhuma troca de monitor, câmera ou microfone sem querer.
15. **Modo clássico:** Ajustes → Abrir modo clássico → Configurações → Atalhos: as teclas do Voltz são as mesmas de
    Ajustes; mudar uma lá, aplicar e voltar a Ajustes → aparece a nova (e, com duas teclas lá, Ajustes mostra a primeira
    e "+1 em Configurações → Atalhos").
16. **Escala e posição:** Windows a 125% e 150% → o texto começa sob a ponta do cursor I e tem o mesmo tamanho relativo
    na prévia; a prévia de Ajustes nítida.
17. **en-US:** trocar o idioma: "Text", "Text size", "Bold", "Custom color", a linha de dica, "Annotations", "Hotkeys",
    "Clear hotkey", "Restore default".

---

## Auto-revisão (feita ao escrever)

- **Cobertura da spec e do backlog (parte do 4D):** §6.2 "texto" (Tasks 1–6: modelo, layout, fonte embutida, digitação,
  ao vivo, barra), "4 cores rápidas + seletor" (Task 6: seletor completo sem diálogo), "a fonte Voltz Anotações desenha os
  mesmos traços" (Task 3: o mesmo núcleo e os mesmos bytes no plugin; Task 5: texto inteiro pelo `voltz_annot_begin`),
  `anotacoes.jsonl` em coordenadas de `tela.mkv` (Task 1: âncora em pixels da captura); §4.3 Ajustes → Anotações (Task 9)
  e Atalhos (Tasks 7–8); §6.3 "o app avisa" (estado em Ajustes › Anotações, aviso na barra do 4C); §9 lógica no núcleo
  (Tasks 1–4 e 7: 33 casos novos). Backlog: `OBSHotkeyEdit` para marcador, holofote, desenhar, desfazer, limpar (Task 8);
  migração do `ProjectRecorder`/`MouseStyleStore` para o `HotkeyBinding` (Task 8); dicas fora da captura (Task 6, linha de
  dica); seletor de cor completo (Task 6). **Fora do 4D, com motivo e destino:** todos os monitores e barra arrastável
  (4E); sair ao gravar/parar (não); O(N²) (não, medido); editar texto confirmado, kerning, IME (limites registrados).
- **Segurança:** nenhum passo liga o modo desenho, digita ou foca um campo; o único gatilho em teste prova a recusa
  (também sem tecla e com a tecla restaurada); Ajustes só por botões (Invoke) e leitura (nome, valor); o campo de atalho
  só ganha foco por clique e o smoke confere que os atalhos nunca foram pausados; scripts sem injeção (grep nas Tasks 10 e
  11); código com foco/captura só no arquivo de exceção, com as **mesmas** contagens do 4C (grep D nas Tasks 5, 8, 10,
  12), pausa de atalhos só no campo de atalho (grep E) e texto fora do log (grep F + smoke); capturas só da casca, da barra
  sem janela e de cena sintética; config portátil com backup e hash; gravação só em `-OutDir`.
- **Placeholders:** nenhum "TBD". Os números das medidas do texto vêm do smoke (Task 10, Step 4) e vão ao backlog na Task
  12, com a origem dita; onde só a execução decide há critério objetivo (`#FFD4F53C`/`#00000000`, contagens mínimas de
  pixels, linhas de log exatas, `janela_nativa=False`, JSON exato do perfil).
- **Consistência de nomes:** núcleo `AnnotationTool::Text` (`"texto"`), `TextFont` (`Regular`/`Bold`, `"dm_sans"`/
  `"dm_sans_negrito"`), `textFontToString`, `textFontFromString`, `clampTextSize`, `kTextSizeMin/Max/Default`,
  `Stroke::text/textSize/textFont`, `AnnotationStyle::textSize/textFont`; `kMaxTextChars`, `kMaxTextLines`,
  `kReplacementChar`, `decodeUtf8`, `encodeUtf8`, `isTextCodepoint`, `textCharCount`, `appendTextCodepoint`,
  `popTextCodepoint`, `sanitizeAnnotationText`; `StrokeStore::beginText/setOpenText/setOpenTextStyle/discardOpen`;
  `GlyphMetrics`, `PlacedGlyph`, `TextLayout`, `layoutText`; `embeddedFontAvailable`, `embeddedFontMetrics`, `GlyphBitmap`,
  `renderGlyph`, `textCaretStroke`, `fontdata::kDmSansRegular/kDmSansBold(+Size)`; `DrawInputKind::Char/Backspace/Enter`,
  `DrawInput::textTool/codepoint/shift`, `DrawCommandType::TextBegin/TextEdit/TextCommit/TextCancel`,
  `DrawCommand::text`, `DrawSession::typing`; `KeyCombo`, `VoltzHotkey`, `voltzHotkeys`, `findVoltzHotkey`,
  `defaultHotkeyBindings`, `hotkeyBindingsFromJson/ToJson`, `HotkeyComboCheck`, `checkVoltzHotkeyCombo`,
  `replaceFirstBinding`, `NamedBinding`, `hotkeyConflicts`; casca `AnnotationController::beginText/updateText/
  commitText/cancelText/typingText` (estado `digitando`, `cursor_texto`), `DrawShared::textTool`, `ColorPicker`
  (`colorPicked`), `DrawToolbar` (`textSizeChosen`, `textFontChosen`, `showColorPicker`), `HotkeyBinding`
  (`loadHotkeyBinding(id, name)`, `saveHotkeyBinding`, `currentHotkeyBindings`, `findHotkey`, `allHotkeyBindings`,
  `hotkeyDescription`, `fromObsCombination`, `toObsCombination`, `keyComboText`, `hotkeyShortcutText`),
  `HotkeySettingsSection`/`VoltzHotkeyEdit`, `AnnotationSettingsSection`/`AnnotationPreview`; pedidos `InjectText`,
  `TypeText`, `CommitText`, `CancelText`, `HotkeyState`, `ToolbarImage {ferramenta, seletor}`; locales `Voltz.Draw.*`,
  `Voltz.Hotkeys.*`, `Voltz.Annot.*`, `Voltz.Settings.Hotkeys/Annotations` — iguais no código, no bloco de teste, no smoke
  e na demo.
- **Contagem de testes:** 170 (base) → 178 (Task 1: 8 casos) → 183 (Task 2: 5) → 189 (Task 3: 6) → 197 (Task 4: 8) →
  203 (Task 7: 6).
- **Conferido ao escrever (fora do repositório, numa cópia no scratchpad):**
  - todo o código do plano foi escrito numa cópia de `frontend/voltz`, `plugins/voltz-sources`, locales e `tools`, e o
    plano foi **gerado** dela: os trechos "trocar/por" são o diff entre os estados de antes e de depois de cada task,
    cada `old` conferido único no momento de aplicar, e aplicar as 11 tasks em ordem (com clang-format 19.1.5 no fim de
    cada uma) reproduz a cópia final arquivo por arquivo;
  - o núcleo e os testes compilados com `/W3 /WX /permissive- /utf-8` e o FreeType das deps em **cada** estado (Tasks 1,
    2, 3, 4, 7 e final): 178, 183, 189, 197 e 203 casos passando (1497, 1550, 1602, 1692 e 1768 asserções), o
    `annotation-font-data.cpp` gerado pelo `embed-fonts.cmake` com o CMake 3.31 do VS;
  - os `.cpp` do `obs-studio` alterados/novos compilados com os includes, definições e `moc` reais do alvo (inclusive
    `QT_NO_CONTEXTLESS_CONNECT`) nos estados das Tasks 5, 6, 8 e 9 e no final (16 arquivos); os do plugin
    (`annotation-feed`, `annotation-source`, `test-vendor`, `voltz-sources`) no final — sem link nem execução do app;
  - a montagem do CMake (`find_package(Freetype)` no `feature-voltz.cmake`, `voltz-core` PUBLIC, gerador das fontes,
    executável + testes + módulo em diretórios irmãos, `find_dependencies` copiado do OBS) num projeto mínimo com NMake:
    configura, compila, liga e abre a fonte embutida;
  - os três scripts PowerShell (`voltz-smoke-text.ps1`, `voltz-smoke-draw.ps1`, `voltz-draw-demo.ps1`) e o bloco de
    teste pelo parser do PowerShell 5.1 (0 erros) e o `Add-Type` da sonda compilado; ASCII puro nos três;
  - o GREP DE SEGURANÇA (A–F) na cópia final dá exatamente o esperado;
  - os ícones `text-b`, `text-b-fill` e `eyedropper` existem no unpkg do `@phosphor-icons/core@2.1.1` (HTTP 200, cabeçalho
    `<svg ... viewBox="0 0 256 256" fill="currentColor">`);
  - o `CLAUDE.md` com as trocas da Task 12 fica com 11 629 bytes (CRLF, como no disco);
  - a API real: `OBSHotkeyEdit` (construtor, `original`, `key`, `ResetKey`, sinal `KeyChanged`), `OBSApp::DisableHotkeys`
    e `UpdateHotkeyFocusSetting` (públicos), `obs_hotkey_load_bindings`, `obs_enum_hotkeys`, `obs_enum_hotkey_bindings`,
    `obs_hotkey_binding_get_hotkey`, `obs_hotkey_get_name/description`, `obs_key_to_name/from_name`,
    `obs_key_combination_to_str`, `INTERACT_*_KEY`, FreeType 2.13.3 (`FT_Get_Advance`, `FT_Outline_Translate`).

---

## Plano 4E (próximo, a escrever depois do 4D) — todos os monitores e barra arrastável

- **Escopo:** desenhar e escrever em qualquer monitor; a sobreposição mostra os traços em todos; a barra pode ser
  arrastada para qualquer monitor e lembra a posição por monitor.
- **Geometria no núcleo (doctest):** `DesktopLayout` (lista de monitores em pixels físicos, com o da Tela marcado);
  ponto do desktop → (monitor, pixel da captura **ou** pixel do desktop); região da janela de entrada = união dos
  monitores menos o buraco da barra (monitores de tamanhos e escalas diferentes, origem negativa, buracos entre eles);
  posição da barra presa ao monitor (`clampToolbarPosition`) e o JSON da posição por monitor (`user.ini [Voltz]
  DrawToolbarPos`, chave = nome GDI, posição relativa ao monitor em pixels lógicos).
- **O que vai para onde:** traço que começa no monitor da Tela continua como hoje (captura, plugin, `anotacoes.jsonl`;
  arrastar para fora prende na borda). Traço que começa **fora** dele é **só de exibição**: guardado num
  `StrokeStore` à parte em pixels do desktop, desenhado só na sobreposição daquele monitor, nunca no plugin nem no
  diário (não existe em `tela.mkv`); desfazer/limpar/"somem após" valem para os dois.
- **Janelas:** uma sobreposição por monitor (o `AnnotationController` passa a ter um conjunto, criado só com traço);
  janela de entrada sobre o retângulo que contém todos os monitores com a região = união dos monitores (nada fora deles
  fica coberto), ainda numa thread só e no mesmo arquivo de exceção (as mesmas chamadas e contagens). Como hoje os outros
  monitores ficam livres no modo desenho, desenhar em todos é uma opção de Ajustes › Anotações ("Desenhar em todos os
  monitores", desligada por padrão) — ligada, clicar em outro monitor desenha em vez de sair por `foco` (as outras saídas
  continuam).
- **Barra arrastável:** uma alça na barra (ícone `dots-six-vertical`); arrasto feito pela própria barra (sem o laço de
  mover do sistema, que poderia ativá-la), posição salva por monitor ao soltar, buraco da janela de entrada atualizado
  pelo `nativeRectChanged` que já existe; a barra vai para o monitor onde foi solta.
- **`screenForMonitor` por `HMONITOR`** (já existe) para a barra e para cada sobreposição; troca de monitores
  (`WM_DISPLAYCHANGE`) continua saindo do modo.
- **Testes sem segundo monitor:** a geometria toda no doctest; ao vivo, `VOLTZ_TEST_MONITORS="x,y,w,h;..."` (só com
  `VOLTZ_TEST_LIVE`) simula monitores **fora do desktop virtual**, com uma sobreposição de teste por monitor falso
  conferida pelo `Assert-DrawSafe` (todas fora do desktop, transparentes, fora da captura); a janela de entrada continua
  recusada em teste; o resto (desenhar em outro monitor real, arrastar a barra) só no roteiro manual de quem tiver dois
  monitores.
- **Pendências que podem entrar:** limite de número de traços (com os de exibição, a memória cresce por monitor);
  "somem após" com animação; guarda de compilação para `VOLTZ_TEST_LIVE`/`VOLTZ_DEBUG_FREEZE_UI` (ou no Plano 5).
