# Voltz Studio — Plano 4A: destaque do mouse, plugin das fontes Voltz e grupo "Tela Voltz" — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Destacar o mouse ao vivo no programa (spec §6.1): halo, onda no clique e holofote desenhados por uma fonte
OBS nova, "Voltz Mouse" (plugin `plugins/voltz-sources/`), alimentada pelo frontend com a posição e os cliques em
pixels da captura (as mesmas coordenadas do `cursor.jsonl`); cada cena de preset com tela passa a usar um grupo
"Tela Voltz" (captura + Voltz Mouse, spec §4.4) — inclusive as cenas que os usuários já têm, por migração — sem mudar a
fonte do ISO `tela.mkv`; Ajustes › Mouse com prévia ao vivo e estilos salvos; atalho do holofote; estilo usado no
`projeto.voltz → mouse`; campos desconhecidos dentro de `pausas[]`/`eventos[]` preservados.

**Architecture:** Lógica pura e testada no núcleo (`frontend/voltz/core/`): estilo do mouse e seu JSON
(`mouse-style`), geometria/tempo do halo, da onda e do holofote (`mouse-effects`), nomes do canal frontend → plugin
(`live-protocol.hpp`) e o manifesto com `mouse`. O plugin novo `voltz-sources` (entra pelo `feature-voltz.cmake`,
liga o `voltz-core`) registra a fonte `voltz_mouse_source` (tamanho = tamanho da captura "alvo", desenha com um
`.effect` próprio) e procedimentos no **proc handler global do libobs** (`voltz_mouse_position`, `voltz_mouse_click`,
`voltz_mouse_style`, `voltz_mouse_wanted`) que gravam num estado único protegido por mutex (`MouseFeed`); o render
copia um retrato desse estado por quadro. No frontend, `LiveMouse` (thread própria, **sem gancho**: posição por
`GetPhysicalCursorPos` a 60 Hz e cliques por Raw Input) só roda enquanto alguma Voltz Mouse está sendo mostrada;
`MouseStyleStore` guarda o estilo no `user.ini` e registra o atalho do holofote; o `PresetBuilder` monta e migra os
grupos "Tela Voltz". Testes nunca usam o mouse real: `VOLTZ_TEST_LIVE` liga pedidos de fornecedor do obs-websocket
("voltz-teste") para injetar mouse e ler o estado, e `VOLTZ_TEST_PRESET_STANDINS` faz os presets criarem fontes de
cor no lugar da captura de monitor e da webcam. **Nenhum patch em arquivo do OBS.**

**Tech Stack:** C++17, Qt 6.11.1, libobs/frontend 32.2.2 (`obs_source_info`, `gs_effect_*`, `gs_draw_quadf`,
`proc_handler_*`, grupos de cena, `obs_hotkey_register_frontend`), obs-websocket 5.7.4 (API de fornecedor
`obs-websocket-api.h`, `GetSourceScreenshot`), Win32 (`GetPhysicalCursorPos`, Raw Input `RIDEV_INPUTSINK`), nlohmann_json
3.11, doctest. Testes ao vivo por `tools/voltz-ws.ps1`, `tools/voltz-uia.ps1`, `ffprobe` (só metadados) e captura
**só da janela** do app.

**Spec:** `docs/superpowers/specs/2026-10-15-voltz-studio-base-gravacao-design.md` §3 (fontes Voltz), §4.3 (Ajustes →
Mouse/Atalhos), §4.4 (grupo "Tela Voltz"), §5.2 (`mouse` no manifesto), §6.1 (destaque do mouse), §8, §9, §11 risco 3 ·
**Backlog:** `docs/voltz/backlog-plano-3.md` ("Próximo", "Pendências e limites do 3A": campos desconhecidos em
`pausas[]`/`eventos[]`) · **Modelo de formato:** `docs/superpowers/plans/2026-10-16-voltz-plano-3b-iso.md`.

**Base deste plano:** commit `f05a6c518` (branch `voltz`, publicado) + o commit que adiciona este arquivo. Contagem
de testes na base: **107** casos.

## Divisão do Plano 4 (e o porquê)

O Plano 4 da spec (§6 inteiro) tem dois subsistemas que só se encontram no grupo "Tela Voltz": **(4A)** o que vive
dentro do OBS — plugin, fonte, canal frontend → fonte, grupo e migração — mais o destaque do mouse, que só depende
disso; **(4B)** o que vive no desktop — janelas de overlay topmost por monitor com `WDA_EXCLUDEFROMCAPTURE`, barra de
ferramentas, entrada de desenho, `StrokeStore`/`anotacoes.jsonl`, fonte "Voltz Anotações", atalhos de desenho e o caso
Windows < 10 2004 (`anotacoes_queimadas`). Juntos passariam de 20 tasks e misturariam dois riscos diferentes (fonte no
render do libobs × janela que captura a entrada do usuário). O 4A entrega software que funciona sozinho (o mouse
destacado no programa) e deixa pronto tudo o que o 4B reaproveita: o plugin (o 4B acrescenta a segunda fonte), o canal
(procedimentos no proc handler global + estado com mutex), o grupo com **versão de camadas** (o 4B sobe de 1 para 2 e
acrescenta a Voltz Anotações nos grupos existentes sem duplicar) e o caminho de teste por fornecedor do obs-websocket.
**Este arquivo é só o 4A**; o 4B está resumido no fim.

**Fora do 4A (vai para o 4B ou para `docs/voltz/backlog-plano-4.md` na Task 10), com o porquê:**
- **Desenho na tela, overlay, `anotacoes.jsonl`, fonte Voltz Anotações, `anotacoes_queimadas`** (spec §6.2, §6.3): Plano
  4B (ver fim).
- **Ajustes › Atalhos** (spec §4.3): vai para o 4B, onde estão quase todos os atalhos (desenho, limpar, ferramentas).
  No 4A o atalho do holofote é um atalho do OBS (`VoltzStudio.ToggleSpotlight`, como o `VoltzStudio.AddMarker` do 3A),
  trocado em Configurações → Atalhos do modo clássico; Ajustes › Mouse mostra a tecla atual.
- **Mudança de estilo no meio da gravação** (cor/tamanho): o manifesto guarda o estilo do início; só o holofote (que tem
  atalho) gera eventos `holofote_ligado`/`holofote_desligado`.
- **Voltz Mouse sobre captura de janela** e **trocar de monitor no meio da gravação no `cursor.jsonl`**: limites do 3A.
  Ao vivo, o `LiveMouse` refaz o mapeamento a cada 500 ms (o halo segue a troca de monitor da Tela); o `cursor.jsonl`
  continua com o monitor do início.

**Descobertas que decidem o desenho (conferidas no código 32.2.2):**
- **Plugin sem tocar no OBS:** `add_obs_plugin` (`cmake/common/helpers_common.cmake:455-506`) é só um
  `add_subdirectory`; quem instala é `set_target_properties_obs` (`cmake/windows/helpers.cmake`): módulo vai para
  `obs-plugins/64bit` e entra em `OBS_MODULES_ENABLED` (linha 161) antes de `target_install_resources` (linha 165), que
  copia `data/` para `data/obs-plugins/<alvo>` (linhas 302-344). O `obs-studio` depende de todo `OBS_MODULES_ENABLED`
  (linhas 38-42). `frontend/CMakeLists.txt` inclui `cmake/feature-voltz.cmake` na linha 67 e chama
  `set_target_properties_obs(obs-studio …)` na 118 ⇒ um `add_subdirectory(plugins/voltz-sources …)` dentro do
  `feature-voltz.cmake` (depois do `voltz/core`) registra o módulo a tempo, pode ligar o `Voltz::core` e não muda
  `plugins/CMakeLists.txt`. `OBS::websocket-api` (`plugins/obs-websocket/cmake/obs-websocket-api.cmake`) é definido
  sempre, antes do frontend (`CMakeLists.txt:31` plugins, `:35` frontend).
- **Canal entre módulos:** `proc_handler_call` (`libobs/callback/proc.c:111-127`) acha o procedimento sob o mutex do
  handler, **copia** a entrada e chama o callback **fora do lock, na thread de quem chama**; não existe
  `proc_handler_remove` (o frontend precisa parar de chamar antes do descarregamento dos módulos — o `EXIT` do frontend
  vem antes). Tipos aceitos na declaração: `int`, `float`, `bool`, `ptr`, `string` (`libobs/callback/decl.c:60-70`).
  É o mecanismo que o próprio obs-websocket usa para expor a API de fornecedor (`obs_websocket_api_get_ph` no handler
  global, `plugins/obs-websocket/lib/obs-websocket-api.h`).
- **Grupo não é compartilhável entre cenas:** o diálogo "Adicionar fonte existente" esconde grupos
  (`frontend/dialogs/OBSBasicSourceSelect.cpp:414-416`) e `resize_group` ajusta a posição de **um** item pai
  (`libobs/obs-scene.c:3483-3525`). ⇒ "Tela Voltz" = **um grupo por cena**, com as camadas (a mesma Tela e a mesma
  Voltz Mouse) compartilhadas. Nome repetido é renomeado pelo libobs para "Nome 2", "Nome 3"
  (`libobs/obs.c:2709-2757`); o plano gera o nome livre antes, sem o aviso no log.
- **Inserir grupo:** `obs_scene_insert_group(scene, name, items, n)` (`obs-scene.c:3535-3590`) exige itens diretos da
  cena, põe `items[0]` embaixo e cria o grupo na posição do **último** item; `resize_group` faz o tamanho do grupo = caixa
  dos filhos (e acompanha a fonte que muda de tamanho, `update_transforms_and_prune_sources`, `obs-scene.c:1008-1040`).
  `obs_sceneitem_get_order_position`/`obs_sceneitem_set_order_position` (`libobs/obs.h:1753`, `obs-scene.c`) devolvem o
  grupo ao lugar da Tela.
- **Fonte escondida do "Adicionar":** `OBS_SOURCE_CAP_DISABLED` some do seletor de fontes
  (`OBSBasicSourceSelect.cpp:512`) mas `obs_source_create`/carregar coleção continuam funcionando (é o que mantém o
  `color_source` v1 obsoleto, `plugins/image-source/color-source.c`). **O obs-websocket também a recusa:**
  `CreateInput` só aceita tipos de `GetInputKindList()`, que pula `OBS_SOURCE_CAP_DISABLED`
  (`plugins/obs-websocket/src/requesthandler/RequestHandler_Inputs.cpp:158-163`,
  `plugins/obs-websocket/src/utils/Obs_ArrayHelper.cpp:237-248`) ⇒ nos testes a Voltz Mouse é criada pelo pedido de
  teste `AddMouseSource` (ou pelo preset).
- **Render:** a cena reseta o blend antes de cada item (`scene_video_render`, `obs-scene.c:1059-1090`:
  `SRCALPHA/INVSRCALPHA`); `color_source` (`color-source.c:79-94`) usa `gs_enable_framebuffer_srgb` + cor linear
  (`vec4_from_rgba_srgb`) quando há transparência — o mesmo vale para o halo. `gs_draw_quadf(nullptr, 0, w, h)`
  (`libobs/graphics/graphics.c:1039-1076`) desenha um sprite sem textura com UV 0..1; `gs_effect_loop` cuida de
  begin/end da técnica.
- **Teste sem tela real:** `GetSourceScreenshot` (`plugins/obs-websocket/src/requesthandler/RequestHandler_Sources.cpp:28-80`
  e `:170-230`) renderiza a fonte/cena numa textura própria e devolve PNG em base64 — não é captura da tela nem quadro
  de vídeo gravado. `RemoveSceneItem` é "só cenas" (`RequestHandler_SceneItems.cpp:225-255`); itens de grupo se leem
  por `GetGroupSceneItemList`.
- **Fornecedor do obs-websocket:** registrar em `obs_module_post_load` (`obs-websocket-api.h`: "ALWAYS CALL ONLY VIA
  `obs_module_post_load()`"); callback `void (obs_data_t *req, obs_data_t *resp, void *priv)` na thread do websocket.
- **Coordenadas:** o 3A já converte pixels físicos do desktop → pixels da captura com `mapToCapture` +
  `CaptureMapping{monitor, largura, altura}` do `findMonitor` (`frontend/voltz/core/cursor-track.cpp`,
  `frontend/voltz/project/MonitorLocator.cpp`); o processo é DPI-aware por monitor (manifesto do OBS), então
  `GetPhysicalCursorPos` e `EnumDisplayMonitors` falam em pixels físicos — mesma origem do `cursor.jsonl`.

---

## Decisões técnicas (e o porquê)

| Tema | Decisão | Por quê |
|---|---|---|
| Divisão | 4A (OBS: plugin, Voltz Mouse, canal, Tela Voltz, Ajustes › Mouse) e 4B (desktop: overlay, desenho, anotações) | Ver seção acima. Cada um entrega software testável; o 4A deixa o canal e o versionamento de camadas prontos. |
| Plugin no build | `plugins/voltz-sources/` adicionado por `frontend/cmake/feature-voltz.cmake` (`add_subdirectory` com pasta binária própria), depois do `voltz/core` | Precisa ligar o `Voltz::core` (estilo, geometria); não muda `plugins/CMakeLists.txt` ⇒ zero patch no OBS. |
| Canal frontend → fontes | Procedimentos do plugin no **proc handler global** (`obs_get_proc_handler()`): `voltz_mouse_position(x,y,inside)`, `voltz_mouse_click(x,y,button,t_ns)`, `voltz_mouse_style(json)`, `voltz_mouse_wanted(out count)`; nomes/assinaturas em `voltz/core/live-protocol.hpp` | Mecanismo do próprio libobs entre módulos (o obs-websocket usa igual); sem ligar o executável ao DLL do plugin (API exportada), sem memória compartilhada e sem regravar `obs_data` da fonte (que salvaria a posição do mouse na coleção). Chamada síncrona, barata (busca linear numa lista curta). |
| Estado ao vivo | `MouseFeed` (singleton no plugin): posição, estilo e até 16 ondas sob **um mutex**; o render copia um `MouseSnapshot` por quadro; `showing` atômico | Escritores: thread do `LiveMouse` (posição/cliques), thread da UI (estilo), thread do websocket (testes); leitor: thread de render. Lock curto, sem alocação no caminho do gancho (não há gancho). |
| Leitura do mouse ao vivo | `LiveMouse`: thread própria com janela só de mensagens (`HWND_MESSAGE`); posição por `GetPhysicalCursorPos` a cada 16 ms (só envia se mudou); cliques por **Raw Input** `RIDEV_INPUTSINK` (botões trocados por `SM_SWAPBUTTON`); liga só com `voltz_mouse_wanted > 0` (consulta a cada 500 ms) | Um `WH_MOUSE_LL` ligado o tempo todo fica no caminho da entrada do Windows (processo travado = mouse do sistema lento); Raw Input só observa. O `CursorTracker` do 3A (gancho só durante a gravação) fica intocado — nada a regredir no `cursor.jsonl`. |
| Coordenadas | O frontend converte com `mapToCapture` + `CaptureMapping` do `findMonitor` da **Tela compartilhada** (`monitor_capture`), as mesmas funções do `cursor.jsonl`; o plugin recebe pixels da captura | Uma fonte de verdade para a conversão (DPI, multimonitor, origem negativa já testados no 3A). Tela que não é captura de monitor (ou monitor ausente) ⇒ `inside=false` (sem halo). |
| Tamanho da Voltz Mouse | Ajuste `alvo` = UUID da captura; no `video_tick` a fonte copia o tamanho do alvo (referência fraca, nova tentativa a cada 60 quadros) | "Mesmo tamanho da captura" (spec §4.4): no grupo, a camada cobre exatamente a Tela e o pixel (x,y) cai no mesmo lugar. |
| Desenho | `data/voltz-mouse.effect` com as técnicas `Circle` (disco/anel suavizado) e `Spotlight` (escurece fora do círculo); ordem holofote → ondas → halo; cor linear + `gs_enable_framebuffer_srgb(true)` como o `color_source` | Um sprite por forma, sem textura; borda suave de ~1 px. |
| Números do efeito | No núcleo (`mouse-effects`): halo = diâmetro `tamanho`, anel com espessura max(2, 12%); onda 450 ms, raio final = max(24, tamanho)·0,9, cresce de 30% a 100% com ease-out cúbico, alfa 0,9→0, espessura max(2, 12% do raio final); holofote com borda max(8, 12% do raio) | Testáveis com doctest; a prévia de Ajustes usa exatamente as mesmas contas. |
| Tela Voltz | **Um grupo por cena** ("Tela Voltz", "Tela Voltz 2", …) com a Tela compartilhada embaixo e a Voltz Mouse por cima, ambas em (0,0), escala 1, **travadas**; o grupo herda transformação, ordem, visibilidade e trava que a Tela tinha; recorte copiado para a camada | OBS não compartilha grupo entre cenas (ver Descobertas). Cena aninhada compartilhada apareceria na lista de cenas e reamostraria duas vezes. Camadas travadas evitam desalinhar mouse e tela. |
| ISO intacto (risco 3) | Nada muda no `IsoTrack`/`findIsoSource`: a fonte de papel continua sendo a **captura** pelo UUID (`ScreenSourceUuid`); grupo e Voltz Mouse nunca são fonte de papel; o log do ISO passa a dizer o nome e o tipo da fonte | O grupo é só do programa; o teste com fontes substitutas confere `tela.mkv` 2560×1440 e `fonte 'Tela' (color_source)` com a Tela dentro do grupo. |
| Migração | Uma vez por coleção: na abertura e no `SCENE_COLLECTION_CHANGED`, cada **item direto** da Tela compartilhada vira grupo; marca `voltz_tela_migrada` nas configurações privadas da Tela (salvas na coleção) e grava a coleção; o grupo leva `voltz_tela_camadas = 1` | Usuário que desagrupar não vê o grupo voltar; cenas próprias com a Tela compartilhada também ganham o destaque; o 4B usa a versão de camadas para acrescentar a Voltz Anotações sem duplicar. Sem o plugin, adia (não marca). |
| Fonte fora do "Adicionar" | `OBS_SOURCE_CAP_DISABLED` | A Voltz Mouse só faz sentido com `alvo`; quem cria é o preset/migração (nos testes, o pedido `AddMouseSource`, porque o `CreateInput` do websocket também a recusa). |
| Estilo | `user.ini [Voltz] MouseStyle` (JSON do núcleo = formato do `projeto.voltz → mouse`), global para todas as Voltz Mouse; gravação adiada 400 ms (arrastar slider não regrava o arquivo a cada passo); estilos salvos em `[Voltz] MouseStyles` (até 20, nome único) | Um estilo do usuário, não da coleção; o mesmo JSON nos três lugares (user.ini, procedimento, manifesto). |
| Manifesto | `mouse` escrito no início do projeto (estilo atual); eventos `holofote_ligado`/`holofote_desligado` quando o holofote muda gravando; campos desconhecidos dentro de `mouse`, `pausas[]` e `eventos[]` sobrevivem à reescrita | Spec §5.2/§6.1 ("a configuração usada vai para `projeto.voltz → mouse`"); pendência do 3A. |
| Atalho | `VoltzStudio.ToggleSpotlight` (atalho do frontend do OBS, padrão Ctrl+Shift+H, editável em Configurações → Atalhos) | Mesmo padrão do marcador do 3A; atalho do OBS é global (funciona com outro app em foco) e não "engole" a tecla. |
| Prévia em Ajustes | `MousePreview` (QPainter) que segue o mouse **dentro dela** e faz a onda ao clicar nela, com a geometria do núcleo; escala 1 px da prévia = 3 px da captura (mostra 540 px de altura) | Não depende de libobs nem da Tela; não precisa capturar nada; ao vivo de verdade. |
| Testes seguros | `VOLTZ_TEST_LIVE=1`: o plugin registra o fornecedor "voltz-teste" (`InjectMouse`, `MouseState`, `SetStyle`, `AddMouseSource`) e o `LiveMouse` **nunca** lê o mouse real; `VOLTZ_TEST_PRESET_STANDINS=1`: presets criam Tela 2560×1440 e Câmera 1280×720 de **cor sólida**; render conferido por `GetSourceScreenshot` de cena **só com fontes de cor + Voltz Mouse** (o bloco aborta se houver qualquer captura real) | Ninguém mexe no mouse do usuário, nenhuma janela captura entrada, nada da tela real é lido. Mesmo padrão de `VOLTZ_FAKE_FREE_BYTES`/`VOLTZ_TEST_ISO_*`. |
| Windows < 10 2004 / esconder da captura | Não se aplicam ao 4A: a Voltz Mouse é fonte do programa, não janela; o ISO não a contém; nada é "queimado" na tela crua | O caso `anotacoes_queimadas` e o overlay com `WDA_EXCLUDEFROMCAPTURE` são do 4B. |

**Respostas aos pontos obrigatórios:** (1) canal frontend → fontes: procedimentos no proc handler global + `MouseFeed`
com mutex (linhas "Canal" e "Estado ao vivo"); (2) grupo "Tela Voltz" sem quebrar o ISO nem as cenas existentes:
linhas "Tela Voltz", "ISO intacto", "Migração" (Tasks 6 e 7); (3) coordenadas: linha "Coordenadas" (mesma conversão do
`cursor.jsonl`, pixels físicos, DPI/multimonitor do 3A); (4) `WDA_EXCLUDEFROMCAPTURE`/`HideOBSWindowsFromCapture`: não
afetam o 4A; política do 4B descrita no fim; (5) atalhos: `VoltzStudio.ToggleSpotlight` no 4A; desenho/limpar/
ferramentas no 4B (fim do arquivo).

---

## Convenções

**Diretório de trabalho:** a raiz do repositório (`voltz-studio`), branch `voltz`. Leia o `CLAUDE.md` inteiro antes
(seções **Decisões** e **Armadilhas**). Base deste plano: commit `f05a6c518`.

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
- **SMOKE MOUSE** (Task 9): `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-mouse.ps1 -OutDir "<scratchpad>\smoke-mouse"`
- **FORMATAR:** `"$CLANGFMT" -i <arquivos .cpp/.hpp novos ou alterados em frontend/voltz/ ou plugins/voltz-sources/>`
  (nunca nos arquivos do OBS; nunca em `.effect`/`.ini`)

`<scratchpad>` = `C:\Users\Cassio\AppData\Local\Temp\claude\C--Users-Cassio-Documents-Projetos-IA-voltz-studio\eee63625-8704-4453-920b-073461dee2a9\scratchpad`.

> **Regras fixas:**
> - **Sem patch no OBS.** O plugin novo em `plugins/voltz-sources/` não é patch; `plugins/CMakeLists.txt` **não** muda
>   (o plugin entra pelo `feature-voltz.cmake`). Se algo se mostrar impossível sem mexer em arquivo do OBS, pare e
>   reporte; se o controlador autorizar, a mudança vai dentro de `#ifdef VOLTZ_STUDIO` (ou com comentário "Voltz
>   Studio" no CMake) e é registrada em `docs/voltz/upstream-patches.md` **no mesmo commit**.
> - Builds longos **em segundo plano**, nunca dois ao mesmo tempo. `MSB4018`/`ZERO_CHECK`/"Touch" no `build_x86` →
>   rode de novo. Arquivo novo (código, `data/`, `.effect`, locale, ícone no `.qrc`, pasta de CMake) exige
>   **CONFIGURAR** antes de compilar.
> - **Warnings como erro** (`/W3` + `CMAKE_COMPILE_WARNING_AS_ERROR`) valem para tudo, inclusive o plugin, o
>   `voltz-core` e os testes: casts explícitos em conversões numéricas (`double`→`float`/`int`, `int64_t`→`int`);
>   `%zu` para `size_t` e `%lld` + `static_cast<long long>` no `blog`.
> - O alvo `obs-studio` compila com `QT_NO_CONTEXTLESS_CONNECT`: todo `connect` com lambda leva objeto de contexto.
> - Sinais/callbacks do libobs (fonte, saída, atalho, websocket) chegam **fora** da thread da UI → só atômicos/mutex ali;
>   o resto via `QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)`.
> - QSS da casca: toda regra com prefixo `#voltzShell`; `setObjectName` **antes** de o widget entrar na janela. Ícones no
>   código sempre por `phosphorIcon()`; ícone novo = SVG **oficial** do Phosphor 2.1.1 baixado do unpkg com versão exata.
> - Textos novos nos **dois** locales da casca (`frontend/data/locale/voltz-pt-BR.ini` e `voltz-en-US.ini`, UTF-8 sem
>   BOM) e nos dois do plugin (`plugins/voltz-sources/data/locale/pt-BR.ini` e `en-US.ini`), com paridade:
>   `diff <(cut -d= -f1 frontend/data/locale/voltz-en-US.ini | sort) <(cut -d= -f1 frontend/data/locale/voltz-pt-BR.ini | sort)`
>   e o mesmo para o plugin → sem saída.
> - `clang-format -i` só nos arquivos de `frontend/voltz/` e `plugins/voltz-sources/`.
> - **Não** commitar `build-compile.log` nem `docs/voltz/backlog-plano-*.md` fora da Task 10. **Push só na Task 10**,
>   nunca `--force`.
>
> **Regras de segurança dos testes ao vivo (obrigatórias — releia antes de cada teste). O PC é do usuário e ele o está
> usando durante os testes:**
> - **Nunca** mova o mouse, clique nem digite na sessão do usuário: nada de `SendInput`, `SetCursorPos`, `mouse_event`,
>   `keybd_event`, `SendKeys`, `[System.Windows.Forms.Cursor]::Position`. UI Automation `Invoke`/`Toggle`/`Select` não
>   move o mouse e é permitido. O mouse nos testes vem **só** do pedido `InjectMouse` (fornecedor "voltz-teste").
> - **Nunca** deixe uma janela topmost capturando a entrada do mouse. O 4A não cria nenhuma: a janela do `LiveMouse` é
>   `HWND_MESSAGE` (invisível, sem entrada) e o Raw Input com `RIDEV_INPUTSINK` só **observa** (não consome cliques).
> - Testes automatizados rodam com `VOLTZ_TEST_LIVE=1` (o `LiveMouse` não lê o mouse real). A única exceção é o passo
>   da Task 5 que confere que o `LiveMouse` liga/desliga sem `VOLTZ_TEST_LIVE`: ali o app só **lê** a posição do cursor
>   (`GetPhysicalCursorPos`) e registra Raw Input por alguns segundos — nada é movido, clicado nem desenhado na tela real
>   (a Tela é de cor, então o mapeamento dá "fora").
> - **Nunca** capture a tela inteira — só o retângulo de uma janela do app via `PrintWindow` (bloco abaixo). Para ver
>   uma página inteira, `Resize-Shell` só redimensiona a janela da casca **lançada pelo teste** (`SWP_NOACTIVATE`: não
>   rouba o foco do usuário, não move a janela); nunca mexa em janelas de outros programas.
> - `GetSourceScreenshot` **só** de cenas compostas por fontes de cor + Voltz Mouse; a função `Scene-Image` do bloco
>   aborta se existir qualquer `monitor_capture`/`window_capture`/`game_capture`/`dshow_input` no app.
> - **Nunca capture nem inspecione quadros** de `programa.mp4`, `tela.mkv` ou `camera.mkv`: só metadados com
>   `ffprobe -show_entries` e o arquivo é apagado no fim (o bloco apaga `$out`).
> - **Nunca** ligue a webcam real nem grave o monitor real: presets **só** com `VOLTZ_TEST_PRESET_STANDINS=1` (o código
>   cria cor no lugar de `monitor_capture`/`dshow_input`); nunca crie `monitor_capture`, `window_capture`,
>   `game_capture` nem `dshow_input` pelo websocket nos testes do 4A.
> - **Nunca** encerre processos que você não iniciou: só pelo PID que você lançou (`Stop-Process -Id $proc.Id`); nunca
>   `taskkill /IM`, nunca `Stop-Process -Name`. Ler a lista de processos é permitido.
> - Não abra o Explorer. **Nunca** toque no que já existe em `%USERPROFILE%\Videos` (o bloco só apaga
>   `Vídeos\Voltz Studio` se foi criado pelo teste e ficou vazio).
> - App sempre com `--portable --multi`, com **backup e restauração** da config portátil
>   (`build_x64/rundir/RelWithDebInfo/config`) — os smokes antigos **apagam** essa pasta.
> - Gravações de teste vão para `<scratchpad>\gravacoes` (`SetProfileParameter`) e são apagadas no fim; o áudio global
>   é **mutado** antes de gravar. `HideOBSWindowsFromCapture=false` no `user.ini` portátil para o `PrintWindow`.
> - PowerShell 5.1 lê `.ps1` sem BOM como ANSI: script com acento (ex. `"Só tela / Só câmera — Aplicar"`) precisa ser
>   salvo **com BOM**; `Select-String` em log com caminho acentuado precisa de `-Encoding UTF8`. Scripts versionados em
>   `tools/` ficam em ASCII e montam acentos com `[char]0xF3` etc.

**Commits:** mensagem em PT-BR terminando com a linha `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

**Bloco de teste (PowerShell 5.1)** — copie para um `.ps1` **com BOM** no scratchpad, ajuste as variáveis marcadas e
coloque os passos da task no `try`. Ele guarda a config portátil, cria uma limpa (obs-websocket na porta 4455 sem
senha, pt-BR, sem confirmação ao sair, estilo de mouse de teste com halo de 200 px opaco), liga os modos de teste,
cria `gravacoes` no scratchpad e restaura/apaga tudo no fim. **Valide dentro do `try`, depois de `Close-App` +
`Save-Log`: o `finally` apaga `$out`.**
```powershell
$root = "C:\Users\Cassio\Documents\Projetos IA\voltz-studio"
$rundir = Join-Path $root "build_x64\rundir\RelWithDebInfo"; $bin = Join-Path $rundir "bin\64bit"
$sp = "C:\Users\Cassio\AppData\Local\Temp\claude\C--Users-Cassio-Documents-Projetos-IA-voltz-studio\eee63625-8704-4453-920b-073461dee2a9\scratchpad"
$out = Join-Path $sp "gravacoes"
$cfg = Join-Path $rundir "config"; $bak = Join-Path $rundir "config.4abak"
if (Test-Path $bak) { throw "backup anterior em $bak - restaure (Move-Item) antes de continuar" }
if (Test-Path $cfg) { Move-Item $cfg $bak }
$voltzCfg = Join-Path $cfg "Voltz Studio"
# ---------- ajuste por task ----------
$liveTest = $true    # VOLTZ_TEST_LIVE: pedidos "voltz-teste" ligados e o LiveMouse NUNCA le o mouse real
$standins = $false   # VOLTZ_TEST_PRESET_STANDINS: presets criam Tela/Camera de cor (nunca monitor/webcam)
$testStyle = '{"halo":{"ativo":true,"cor":"#D4F53C","tamanho":200,"opacidade":1.0,"forma":"cheio"},"clique":{"ativo":true,"cor":"#D4F53C","cor_direito":"#4DA3FF"},"holofote":{"ativo":false,"raio":220,"escurecer":0.55}}'
$voltzIni = "MouseStyle=$testStyle`r`nNextProjectName=Teste 4A`r`n"   # linhas da secao [Voltz]
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
if ($liveTest) { $env:VOLTZ_TEST_LIVE = "1" } else { Remove-Item Env:VOLTZ_TEST_LIVE -ErrorAction SilentlyContinue }
if ($standins) { $env:VOLTZ_TEST_PRESET_STANDINS = "1" } else { Remove-Item Env:VOLTZ_TEST_PRESET_STANDINS -ErrorAction SilentlyContinue }
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"; $hadVoltzVideos = Test-Path -LiteralPath $voltzVideos
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class W { [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f); }
'@
[void][W]::SetProcessDPIAware()
# Captura SO da janela do app (nunca da tela inteira)
function Shot([IntPtr]$h, [string]$n) { $r = New-Object W+RECT; [void][W]::GetWindowRect($h, [ref]$r)
  $b = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [System.Drawing.Graphics]::FromImage($b)
  $d = $g.GetHdc(); [void][W]::PrintWindow($h, $d, 2); $g.ReleaseHdc($d); $g.Dispose(); $b.Save("$sp\$n.png"); $b.Dispose() }
# Redimensiona SO a janela da casca lancada pelo teste (sem mover, sem ativar, sem mudar a ordem): para o PrintWindow
# ver uma pagina inteira de Ajustes. SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE = 0x2|0x4|0x10.
function Resize-Shell([int]$w, [int]$h) { [void][W]::SetWindowPos($script:shell, [IntPtr]::Zero, 0, 0, $w, $h, 0x16); Start-Sleep 1 }
$uia = Join-Path $root "tools\voltz-uia.ps1"; $ws = Join-Path $root "tools\voltz-ws.ps1"
# Pedidos ao obs-websocket: devolve as respostas (index/requestType/ok/data); -Quiet nao ecoa (screenshots sao longos)
function Ws([string]$json, [switch]$Quiet) { $f = Join-Path $sp "req.json"
  [IO.File]::WriteAllText($f, $json, (New-Object Text.UTF8Encoding $false))
  $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $f
  if (-not $Quiet) { $lines | ForEach-Object { Write-Host $_ } }
  return ,@($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json }) }
function Uia([string]$name) { & powershell -ExecutionPolicy Bypass -File $uia -ProcessId $script:proc.Id -Name $name | Out-Host; return ($LASTEXITCODE -eq 0) }
function Start-App { $script:proc = Start-Process (Join-Path $bin "VoltzStudio.exe") -ArgumentList '--portable','--multi' -WorkingDirectory $bin -PassThru
  Start-Sleep 15; $script:proc.Refresh(); $script:shell = $script:proc.MainWindowHandle }
function Close-App { if ($script:proc -and -not $script:proc.HasExited) { $script:proc.Refresh(); [void]$script:proc.CloseMainWindow()
  if (-not $script:proc.WaitForExit(30000)) { Stop-Process -Id $script:proc.Id -Force } } }
function Save-Log { $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
  if ($log) { Copy-Item $log.FullName "$sp\ultimo-log.txt" -Force } }
function LogLines([string]$pattern) { if (Test-Path "$sp\ultimo-log.txt") { Select-String -LiteralPath "$sp\ultimo-log.txt" -Encoding UTF8 -Pattern $pattern | ForEach-Object { $_.Line } } }
# Fornecedor de teste do plugin (so existe com VOLTZ_TEST_LIVE=1)
function Vendor([string]$type, [string]$dataJson = '{}') {
  $r = Ws ('[{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste","requestType":"' + $type + '","requestData":' + $dataJson + '}}]')
  if ($r.Count -lt 1 -or -not $r[0].ok) { throw "pedido de teste $type falhou" }
  return $r[0].data.responseData }
# Seguranca: nenhuma captura real no app (tela, janela, jogo, webcam)
function Assert-NoRealCapture { $inputs = @((Ws '[{"requestType":"GetInputList"}]' -Quiet)[0].data.inputs)
  $real = @($inputs | Where-Object { $_.inputKind -match 'monitor_capture|window_capture|game_capture|dshow_input' })
  if ($real.Count -gt 0) { throw ("captura real presente - abortando: " + (($real | ForEach-Object { $_.inputName }) -join ', ')) } }
# Render de uma cena SINTETICA (cor + Voltz Mouse) em 640x360 - nunca tela real nem quadro de video
function Scene-Image([string]$scene) { Assert-NoRealCapture
  $r = Ws ('[{"requestType":"GetSourceScreenshot","requestData":{"sourceName":' + (ConvertTo-Json $scene) + ',"imageFormat":"png","imageWidth":640,"imageHeight":360}}]') -Quiet
  if ($r.Count -lt 1 -or -not $r[0].ok) { throw "GetSourceScreenshot falhou para '$scene'" }
  $bytes = [Convert]::FromBase64String(($r[0].data.imageData -replace '^data:image/png;base64,', ''))
  return [System.Drawing.Bitmap]::FromStream((New-Object IO.MemoryStream (,$bytes))) }
function Near($c, [int]$r, [int]$g, [int]$b, [int]$tol = 24) { ([Math]::Abs($c.R - $r) -le $tol) -and ([Math]::Abs($c.G - $g) -le $tol) -and ([Math]::Abs($c.B - $b) -le $tol) }
# user.ini [Voltz] com o app FECHADO (o app regrava o arquivo ao sair)
function Set-VoltzIni([string]$key, [string]$value) { $path = Join-Path $voltzCfg "user.ini"
  $lines = [System.Collections.Generic.List[string]]@(Get-Content -LiteralPath $path -Encoding UTF8)
  $start = $lines.IndexOf("[Voltz]"); if ($start -lt 0) { $lines.Add("[Voltz]"); $start = $lines.Count - 1 }
  $end = $lines.Count; for ($i = $start + 1; $i -lt $lines.Count; $i++) { if ($lines[$i].StartsWith("[")) { $end = $i; break } }
  $done = $false; for ($i = $start + 1; $i -lt $end; $i++) { if ($lines[$i].StartsWith("$key=")) { $lines[$i] = "$key=$value"; $done = $true } }
  if (-not $done) { $lines.Insert($end, "$key=$value") }
  [IO.File]::WriteAllLines($path, $lines, (New-Object Text.UTF8Encoding $false)) }
function Get-VoltzIni([string]$key) { $line = Get-Content -LiteralPath (Join-Path $voltzCfg "user.ini") -Encoding UTF8 | Where-Object { $_.StartsWith("$key=") } | Select-Object -First 1
  if ($line) { return $line.Substring($key.Length + 1) } ; return $null }
function Project-Dirs { @(Get-ChildItem -LiteralPath $out -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz") } | Sort-Object CreationTime) }
function Read-Json([string]$path) { (Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json) | ForEach-Object { $_ } }
$ffprobe = (Get-Command ffprobe -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $ffprobe) { $ffprobe = Get-ChildItem (Join-Path $root ".deps") -Filter ffprobe.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName }
# So metadados (nunca quadros)
function Probe([string]$file) { (& $ffprobe -v error -show_entries "stream=codec_type,codec_name,width,height:format=duration" -of json $file) | Out-String | ConvertFrom-Json }
function Show-Project([System.IO.DirectoryInfo]$d) {
  $m = Read-Json (Join-Path $d.FullName "projeto.voltz")
  Write-Host ("== {0}: status={1} bruta={2} liquida={3}" -f $d.Name, $m.status, $m.duracao_ms, $m.duracao_liquida_ms)
  Write-Host ("arquivos: " + ($m.arquivos | ConvertTo-Json -Depth 6 -Compress))
  Write-Host ("mouse: " + ($m.mouse | ConvertTo-Json -Depth 6 -Compress))
  Write-Host ("eventos: " + (ConvertTo-Json -InputObject @($m.eventos) -Depth 4 -Compress))
  foreach ($f in @("programa.mp4", "tela.mkv", "camera.mkv")) {
    $path = Join-Path $d.FullName $f
    if (-not (Test-Path -LiteralPath $path)) { Write-Host "$f : ausente"; continue }
    $p = Probe $path
    Write-Host ("{0} : {1} | dur={2}" -f $f, (($p.streams | ForEach-Object { "$($_.codec_type)/$($_.codec_name) $($_.width)x$($_.height)" }) -join " | "), $p.format.duration) }
  return $m }
$proc = $null
try {
  Start-App
  # ... passos da task (validacao no fim: Close-App; Save-Log; ...) ...
} finally {
  Close-App
  Save-Log
  if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
  if (Test-Path $bak) { Move-Item $bak $cfg }
  Remove-Item Env:VOLTZ_TEST_LIVE, Env:VOLTZ_TEST_PRESET_STANDINS -ErrorAction SilentlyContinue
  if (Test-Path $out) { Remove-Item -Recurse -Force $out }   # apaga gravacoes de teste sem abrir
  if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) { Remove-Item -Force -LiteralPath $voltzVideos }
}
```

**Preparo de gravação** (dentro do `try`, depois de aplicar um preset com `$standins = $true`): aponta a pasta de
gravações para `$out` e muta o áudio global. (Aplicar preset já grava os padrões Voltz.)
```powershell
$r = Ws ('[{"requestType":"SetProfileParameter","requestData":{"parameterCategory":"SimpleOutput","parameterName":"FilePath","parameterValue":' + (ConvertTo-Json $out) + '}},{"requestType":"GetSpecialInputs"}]')
$special = $r[1].data
$mutes = @(); foreach ($n in @($special.desktop1, $special.desktop2, $special.mic1, $special.mic2, $special.mic3, $special.mic4)) { if ($n) { $mutes += @{ requestType = "SetInputMute"; requestData = @{ inputName = $n; inputMuted = $true } } } }
if ($mutes.Count -gt 0) { [void](Ws (ConvertTo-Json -InputObject $mutes -Depth 5 -Compress)) }
```

**Cores de referência nas imagens de `Scene-Image`:** Tela substituta = `#2A2A30` → (42,42,48); halo do estilo de teste
= `#D4F53C` → (212,245,60). Tolerância 24 por canal (`Near`).

---

## Mapa de arquivos

**Novos — núcleo (`frontend/voltz/core/`, sem Qt/libobs) e testes**
| Arquivo | Responsabilidade |
|---|---|
| `core/mouse-style.hpp/.cpp` | `MouseStyle` (halo/clique/holofote da spec §5.2), cor `#RRGGBB`, limites, JSON (mesmo formato do manifesto), estilos salvos com nome |
| `core/mouse-effects.hpp/.cpp` | geometria do halo, onda do clique no tempo (`rippleAt`), fila de ondas, geometria do holofote |
| `core/live-protocol.hpp` | nomes e assinaturas dos procedimentos frontend → plugin, id da fonte, ajuste `alvo`, códigos de botão, modo de teste |
| `tests/test-mouse-style.cpp`, `tests/test-mouse-effects.cpp` | doctest do acima |

**Novos — plugin (`plugins/voltz-sources/`, módulo do OBS)**
| Arquivo | Responsabilidade |
|---|---|
| `CMakeLists.txt`, `cmake/windows/obs-module.rc.in` | alvo `voltz-sources` (MODULE) ligado a `OBS::libobs`, `OBS::websocket-api`, `Voltz::core` |
| `voltz-sources.cpp` | `obs_module_load` (fonte + procedimentos) e `obs_module_post_load` (fornecedor de teste) |
| `mouse-feed.hpp/.cpp` | `MouseFeed` (estado ao vivo com mutex, retrato por quadro, contagem de fontes mostradas) e os 4 procedimentos |
| `mouse-source.hpp/.cpp` | fonte `voltz_mouse_source` ("Voltz Mouse"): tamanho do alvo, desenho com o `.effect` |
| `test-vendor.hpp/.cpp` | fornecedor "voltz-teste" do obs-websocket: `InjectMouse`, `MouseState`, `SetStyle`, `AddMouseSource` (só com `VOLTZ_TEST_LIVE`) |
| `data/voltz-mouse.effect` | técnicas `Circle` e `Spotlight` |
| `data/locale/en-US.ini`, `data/locale/pt-BR.ini` | nome do tipo de fonte |

**Novos — frontend (alvo `obs-studio`)**
| Arquivo | Responsabilidade |
|---|---|
| `shell/MouseStyleStore.hpp/.cpp` | estilo atual e salvos no `user.ini`, atalho `VoltzStudio.ToggleSpotlight`, sinais `styleChanged`/`spotlightToggled`/`savedStylesChanged` |
| `project/LiveMouse.hpp/.cpp` | thread do mouse ao vivo (posição + Raw Input) → procedimentos do plugin; liga/desliga conforme `voltz_mouse_wanted`; empurra o estilo |
| `shell/MouseSettings.hpp/.cpp` | seção Ajustes › Mouse (controles, estilos salvos, texto do atalho) |
| `shell/MousePreview.hpp/.cpp` | prévia ao vivo (QPainter) com a geometria do núcleo |
| `shell/icons/regular/cursor-click.svg` | ícone da Voltz Mouse na lista de Fontes (Phosphor 2.1.1) |
| `tools/voltz-smoke-mouse.ps1` | smoke do 4A (presets com substitutas, grupo, render, holofote, onda, gravação, migração) |

**Modificados (Voltz):** `frontend/cmake/feature-voltz.cmake`, `frontend/voltz/core/CMakeLists.txt`,
`frontend/voltz/core/project-manifest.hpp/.cpp`, `frontend/voltz/tests/CMakeLists.txt`,
`frontend/voltz/tests/test-project-manifest.cpp`, `frontend/voltz/shell/SharedSources.hpp/.cpp`,
`frontend/voltz/shell/PresetBuilder.hpp/.cpp`, `frontend/voltz/shell/FrontendBridge.hpp/.cpp`,
`frontend/voltz/shell/SourcesPanel.cpp`, `frontend/voltz/shell/VoltzShell.hpp/.cpp`,
`frontend/voltz/shell/SettingsPage.hpp/.cpp`, `frontend/voltz/shell/voltz-shell.qrc`,
`frontend/voltz/shell/voltz-shell.qss`, `frontend/voltz/project/ProjectRecorder.hpp/.cpp`,
`frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`, `tools/voltz-smoke-project.ps1`; na
Task 10: `docs/voltz/backlog-plano-4.md` (novo), `docs/voltz/backlog-plano-3.md`, `CLAUDE.md`, `AGENTS.md`,
`docs/HISTORICO.md`.

**Modificados (OBS upstream):** nenhum. (`plugins/CMakeLists.txt` e `docs/voltz/upstream-patches.md` não mudam; a
Task 10 confere.)

**Contagem de testes:** 107 (base) → 115 (Task 1) → 122 (Task 2) → 126 (Task 3).

---
### Task 1: Núcleo — estilo do mouse (`mouse-style`) e estilos salvos

**Files:**
- Create: `frontend/voltz/core/mouse-style.hpp`, `frontend/voltz/core/mouse-style.cpp`
- Create: `frontend/voltz/tests/test-mouse-style.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

- [ ] **Step 1: Escrever o teste que falha**

`frontend/voltz/tests/test-mouse-style.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/mouse-style.hpp>

#include <string>
#include <vector>

using voltz::HaloShape;
using voltz::MouseStyle;
using voltz::NamedMouseStyle;
using voltz::RgbColor;

TEST_CASE("estilo do mouse padrão é o da spec §5.2")
{
	const MouseStyle style;
	CHECK(voltz::mouseStyleToJson(style) ==
	      R"({"halo":{"ativo":true,"cor":"#D4F53C","tamanho":46,"opacidade":0.25,"forma":"cheio"},)"
	      R"("clique":{"ativo":true,"cor":"#D4F53C","cor_direito":"#4DA3FF"},)"
	      R"("holofote":{"ativo":false,"raio":220,"escurecer":0.55}})");
}

TEST_CASE("mouseStyleToJson e mouseStyleFromJson: ida e volta")
{
	MouseStyle style;
	style.halo.active = false;
	style.halo.color = RgbColor{0x12, 0xAB, 0xEF};
	style.halo.size = 80;
	style.halo.opacity = 0.6;
	style.halo.shape = HaloShape::Ring;
	style.click.active = false;
	style.click.color = RgbColor{1, 2, 3};
	style.click.rightColor = RgbColor{4, 5, 6};
	style.spotlight.active = true;
	style.spotlight.radius = 300;
	style.spotlight.dim = 0.7;
	MouseStyle back;
	REQUIRE(voltz::mouseStyleFromJson(voltz::mouseStyleToJson(style), back));
	CHECK_FALSE(back.halo.active);
	CHECK(back.halo.color == style.halo.color);
	CHECK(back.halo.size == 80);
	CHECK(back.halo.opacity == doctest::Approx(0.6));
	CHECK(back.halo.shape == HaloShape::Ring);
	CHECK_FALSE(back.click.active);
	CHECK(back.click.color == style.click.color);
	CHECK(back.click.rightColor == style.click.rightColor);
	CHECK(back.spotlight.active);
	CHECK(back.spotlight.radius == 300);
	CHECK(back.spotlight.dim == doctest::Approx(0.7));
}

TEST_CASE("mouseStyleFromJson tolera ausentes e tipos errados e limita as faixas")
{
	MouseStyle style;
	REQUIRE(voltz::mouseStyleFromJson(
		R"({"halo":{"tamanho":5,"opacidade":2,"cor":"verde","forma":"triangulo"},"holofote":{"raio":5000,"escurecer":0,"ativo":"sim"},"extra":1})",
		style));
	CHECK(style.halo.size == voltz::kHaloSizeMin);
	CHECK(style.halo.opacity == doctest::Approx(voltz::kHaloOpacityMax));
	CHECK(style.halo.color == voltz::HaloStyle{}.color);
	CHECK(style.halo.shape == HaloShape::Filled);
	CHECK(style.spotlight.radius == voltz::kSpotlightRadiusMax);
	CHECK(style.spotlight.dim == doctest::Approx(voltz::kSpotlightDimMin));
	CHECK_FALSE(style.spotlight.active);
	CHECK(style.click.active);

	MouseStyle big;
	big.halo.size = 999;
	big.halo.opacity = -1.0;
	big.spotlight.radius = 10;
	big.spotlight.dim = 1.0;
	big = voltz::clampMouseStyle(big);
	CHECK(big.halo.size == voltz::kHaloSizeMax);
	CHECK(big.halo.opacity == doctest::Approx(voltz::kHaloOpacityMin));
	CHECK(big.spotlight.radius == voltz::kSpotlightRadiusMin);
	CHECK(big.spotlight.dim == doctest::Approx(voltz::kSpotlightDimMax));
}

TEST_CASE("mouseStyleFromJson recusa JSON inválido e devolve o padrão")
{
	MouseStyle style;
	style.halo.size = 99;
	CHECK_FALSE(voltz::mouseStyleFromJson("", style));
	CHECK(style.halo.size == 46);
	style.halo.size = 99;
	CHECK_FALSE(voltz::mouseStyleFromJson("[1,2]", style));
	CHECK(style.halo.size == 46);
	CHECK_FALSE(voltz::mouseStyleFromJson("{", style));
}

TEST_CASE("cor #RRGGBB: lê maiúsculas e minúsculas, escreve maiúsculas, recusa o resto")
{
	RgbColor color;
	CHECK(voltz::parseHexColor("#d4f53c", color));
	CHECK(color.r == 0xD4);
	CHECK(color.g == 0xF5);
	CHECK(color.b == 0x3C);
	CHECK(voltz::formatHexColor(color) == "#D4F53C");
	const RgbColor before = color;
	CHECK_FALSE(voltz::parseHexColor("D4F53C", color));
	CHECK_FALSE(voltz::parseHexColor("#D4F53", color));
	CHECK_FALSE(voltz::parseHexColor("#D4F53G", color));
	CHECK(color == before);
	CHECK(voltz::formatHexColor(RgbColor{0, 10, 255}) == "#000AFF");
}

TEST_CASE("estilos salvos: JSON ida e volta; itens sem nome são ignorados")
{
	std::vector<NamedMouseStyle> styles(2);
	styles[0].name = "Aula";
	styles[0].style.halo.size = 60;
	styles[1].name = "Holofote forte";
	styles[1].style.spotlight.active = true;
	styles[1].style.spotlight.dim = 0.8;
	const std::string json = voltz::savedStylesToJson(styles);
	CHECK(json.rfind(R"([{"nome":"Aula","estilo":{"halo":)", 0) == 0);
	const std::vector<NamedMouseStyle> back = voltz::savedStylesFromJson(json);
	REQUIRE(back.size() == 2);
	CHECK(back[0].name == "Aula");
	CHECK(back[0].style.halo.size == 60);
	CHECK(back[1].name == "Holofote forte");
	CHECK(back[1].style.spotlight.active);
	CHECK(back[1].style.spotlight.dim == doctest::Approx(0.8));
	CHECK(voltz::savedStylesFromJson(R"([{"estilo":{}},{"nome":"  ","estilo":{}},3,{"nome":"Só nome"}])").size() == 1);
	CHECK(voltz::savedStylesFromJson("lixo").empty());
}

TEST_CASE("upsertSavedStyle troca pelo nome, ignora nome vazio e guarda no máximo 20")
{
	std::vector<NamedMouseStyle> styles;
	MouseStyle a;
	a.halo.size = 50;
	MouseStyle b;
	b.halo.size = 70;
	styles = voltz::upsertSavedStyle(styles, "  Aula ", a);
	styles = voltz::upsertSavedStyle(styles, "Outro", a);
	styles = voltz::upsertSavedStyle(styles, "Aula", b);
	REQUIRE(styles.size() == 2);
	CHECK(styles[0].name == "Aula");
	CHECK(styles[0].style.halo.size == 70);
	CHECK(styles[1].name == "Outro");
	CHECK(voltz::upsertSavedStyle(styles, "   ", a).size() == 2);
	for (int i = 0; i < 25; i++) {
		styles = voltz::upsertSavedStyle(styles, "E" + std::to_string(i), a);
	}
	CHECK(styles.size() == voltz::kMaxSavedStyles);
	CHECK(styles.front().name == "E5");
	CHECK(styles.back().name == "E24");
}

TEST_CASE("removeSavedStyle tira só o nome pedido")
{
	std::vector<NamedMouseStyle> styles(2);
	styles[0].name = "A";
	styles[1].name = "B";
	const std::vector<NamedMouseStyle> left = voltz::removeSavedStyle(styles, " A ");
	REQUIRE(left.size() == 1);
	CHECK(left[0].name == "B");
	CHECK(voltz::removeSavedStyle(styles, "Z").size() == 2);
}
```

- [ ] **Step 2: Registrar o teste e ver falhar**

Em `frontend/voltz/tests/CMakeLists.txt`, na lista de `target_sources`, acrescentar `test-mouse-style.cpp` depois de
`test-iso-encoders.cpp`:
```cmake
    test-iso-encoders.cpp
    test-mouse-style.cpp
    test-preset-layout.cpp
```
CONFIGURAR; COMPILAR TESTES.
Expected: FAIL com `Cannot open include file: 'voltz/core/mouse-style.hpp'` (C1083).

- [ ] **Step 3: Implementar o núcleo**

`frontend/voltz/core/mouse-style.hpp`:
```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

struct RgbColor {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
};

inline bool operator==(const RgbColor &a, const RgbColor &b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b;
}

// "#RRGGBB" (maiúsculas ou minúsculas) → cor. Falso (e `out` intacto) em qualquer outro formato.
bool parseHexColor(const std::string &text, RgbColor &out);
// Cor → "#RRGGBB" em maiúsculas.
std::string formatHexColor(const RgbColor &color);

enum class HaloShape { Filled, Ring };

// Limites de clampMouseStyle (os mesmos dos controles de Ajustes › Mouse).
constexpr int kHaloSizeMin = 12;
constexpr int kHaloSizeMax = 240;
constexpr double kHaloOpacityMin = 0.05;
constexpr double kHaloOpacityMax = 1.0;
constexpr int kSpotlightRadiusMin = 60;
constexpr int kSpotlightRadiusMax = 1000;
constexpr double kSpotlightDimMin = 0.1;
constexpr double kSpotlightDimMax = 0.9;
constexpr size_t kMaxSavedStyles = 20;

// Spec §5.2/§6.1. Tamanhos em pixels da captura (os do cursor.jsonl).
struct HaloStyle {
	bool active = true;
	RgbColor color{0xD4, 0xF5, 0x3C};
	int size = 46; // diâmetro
	double opacity = 0.25;
	HaloShape shape = HaloShape::Filled;
};

struct ClickStyle {
	bool active = true;
	RgbColor color{0xD4, 0xF5, 0x3C};
	RgbColor rightColor{0x4D, 0xA3, 0xFF};
};

struct SpotlightStyle {
	bool active = false;
	int radius = 220;
	double dim = 0.55; // "escurecer": opacidade do preto fora do círculo
};

struct MouseStyle {
	HaloStyle halo;
	ClickStyle click;
	SpotlightStyle spotlight;
};

MouseStyle clampMouseStyle(MouseStyle style);

// JSON compacto no formato do bloco "mouse" do projeto.voltz (spec §5.2):
// {"halo":{"ativo","cor","tamanho","opacidade","forma":"cheio|anel"},"clique":{"ativo","cor","cor_direito"},
//  "holofote":{"ativo","raio","escurecer"}}
std::string mouseStyleToJson(const MouseStyle &style);
// Leitura tolerante: `out` recebe o padrão + o que vier válido (já limitado). Falso só para JSON inválido ou que não é
// objeto (aí `out` fica no padrão).
bool mouseStyleFromJson(const std::string &json, MouseStyle &out);

struct NamedMouseStyle {
	std::string name;
	MouseStyle style;
};

// [{"nome":"Aula","estilo":{...}}]
std::string savedStylesToJson(const std::vector<NamedMouseStyle> &styles);
// Itens sem nome (ou só espaços) e itens que não são objeto são ignorados; JSON inválido → lista vazia.
std::vector<NamedMouseStyle> savedStylesFromJson(const std::string &json);
// Nome (sem espaços nas pontas) já salvo → troca o estilo; senão acrescenta no fim e, acima de kMaxSavedStyles, tira o
// mais antigo. Nome vazio → lista igual.
std::vector<NamedMouseStyle> upsertSavedStyle(std::vector<NamedMouseStyle> styles, const std::string &name,
					      const MouseStyle &style);
std::vector<NamedMouseStyle> removeSavedStyle(std::vector<NamedMouseStyle> styles, const std::string &name);

} // namespace voltz
```

`frontend/voltz/core/mouse-style.cpp`:
```cpp
#include "mouse-style.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace voltz {

namespace {

using Json = nlohmann::ordered_json;

int hexValue(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

const Json *child(const Json &object, const char *key)
{
	const auto it = object.find(key);
	return it != object.end() && it->is_object() ? &*it : nullptr;
}

void readBool(const Json &object, const char *key, bool &out)
{
	const auto it = object.find(key);
	if (it != object.end() && it->is_boolean()) {
		out = it->get<bool>();
	}
}

void readInt(const Json &object, const char *key, int &out)
{
	const auto it = object.find(key);
	if (it == object.end() || !it->is_number()) {
		return;
	}
	const double value = it->get<double>();
	/* fora do intervalo do int: vira um extremo e o clamp resolve */
	if (value > 1e9) {
		out = 1000000000;
	} else if (value < -1e9) {
		out = -1000000000;
	} else {
		out = static_cast<int>(std::lround(value));
	}
}

void readDouble(const Json &object, const char *key, double &out)
{
	const auto it = object.find(key);
	if (it != object.end() && it->is_number()) {
		out = it->get<double>();
	}
}

void readColor(const Json &object, const char *key, RgbColor &out)
{
	const auto it = object.find(key);
	if (it != object.end() && it->is_string()) {
		parseHexColor(it->get<std::string>(), out);
	}
}

// Os controles usam porcentagem inteira: 0,25 continua 0.25 no JSON.
double round2(double value)
{
	return std::round(value * 100.0) / 100.0;
}

std::string dump(const Json &json)
{
	return json.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string trimmed(const std::string &text)
{
	const size_t first = text.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return std::string();
	}
	const size_t last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}

Json styleJson(const MouseStyle &style)
{
	Json halo = Json::object();
	halo["ativo"] = style.halo.active;
	halo["cor"] = formatHexColor(style.halo.color);
	halo["tamanho"] = style.halo.size;
	halo["opacidade"] = round2(style.halo.opacity);
	halo["forma"] = style.halo.shape == HaloShape::Ring ? "anel" : "cheio";

	Json click = Json::object();
	click["ativo"] = style.click.active;
	click["cor"] = formatHexColor(style.click.color);
	click["cor_direito"] = formatHexColor(style.click.rightColor);

	Json spotlight = Json::object();
	spotlight["ativo"] = style.spotlight.active;
	spotlight["raio"] = style.spotlight.radius;
	spotlight["escurecer"] = round2(style.spotlight.dim);

	Json root = Json::object();
	root["halo"] = std::move(halo);
	root["clique"] = std::move(click);
	root["holofote"] = std::move(spotlight);
	return root;
}

MouseStyle styleFromObject(const Json &root)
{
	MouseStyle style;
	if (const Json *halo = child(root, "halo")) {
		readBool(*halo, "ativo", style.halo.active);
		readColor(*halo, "cor", style.halo.color);
		readInt(*halo, "tamanho", style.halo.size);
		readDouble(*halo, "opacidade", style.halo.opacity);
		const auto shape = halo->find("forma");
		if (shape != halo->end() && shape->is_string() && shape->get<std::string>() == "anel") {
			style.halo.shape = HaloShape::Ring;
		}
	}
	if (const Json *click = child(root, "clique")) {
		readBool(*click, "ativo", style.click.active);
		readColor(*click, "cor", style.click.color);
		readColor(*click, "cor_direito", style.click.rightColor);
	}
	if (const Json *spotlight = child(root, "holofote")) {
		readBool(*spotlight, "ativo", style.spotlight.active);
		readInt(*spotlight, "raio", style.spotlight.radius);
		readDouble(*spotlight, "escurecer", style.spotlight.dim);
	}
	return clampMouseStyle(style);
}

} // namespace

bool parseHexColor(const std::string &text, RgbColor &out)
{
	if (text.size() != 7 || text[0] != '#') {
		return false;
	}
	int digits[6];
	for (size_t i = 0; i < 6; i++) {
		digits[i] = hexValue(text[i + 1]);
		if (digits[i] < 0) {
			return false;
		}
	}
	out.r = static_cast<uint8_t>(digits[0] * 16 + digits[1]);
	out.g = static_cast<uint8_t>(digits[2] * 16 + digits[3]);
	out.b = static_cast<uint8_t>(digits[4] * 16 + digits[5]);
	return true;
}

std::string formatHexColor(const RgbColor &color)
{
	char text[8];
	std::snprintf(text, sizeof(text), "#%02X%02X%02X", static_cast<unsigned>(color.r),
		      static_cast<unsigned>(color.g), static_cast<unsigned>(color.b));
	return text;
}

MouseStyle clampMouseStyle(MouseStyle style)
{
	style.halo.size = std::clamp(style.halo.size, kHaloSizeMin, kHaloSizeMax);
	style.halo.opacity = std::clamp(style.halo.opacity, kHaloOpacityMin, kHaloOpacityMax);
	style.spotlight.radius = std::clamp(style.spotlight.radius, kSpotlightRadiusMin, kSpotlightRadiusMax);
	style.spotlight.dim = std::clamp(style.spotlight.dim, kSpotlightDimMin, kSpotlightDimMax);
	return style;
}

std::string mouseStyleToJson(const MouseStyle &style)
{
	return dump(styleJson(style));
}

bool mouseStyleFromJson(const std::string &json, MouseStyle &out)
{
	const Json root = Json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		out = MouseStyle{};
		return false;
	}
	out = styleFromObject(root);
	return true;
}

std::string savedStylesToJson(const std::vector<NamedMouseStyle> &styles)
{
	Json list = Json::array();
	for (const NamedMouseStyle &item : styles) {
		Json entry = Json::object();
		entry["nome"] = item.name;
		entry["estilo"] = styleJson(item.style);
		list.push_back(std::move(entry));
	}
	return dump(list);
}

std::vector<NamedMouseStyle> savedStylesFromJson(const std::string &json)
{
	std::vector<NamedMouseStyle> result;
	const Json root = Json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_array()) {
		return result;
	}
	for (const Json &entry : root) {
		if (!entry.is_object()) {
			continue;
		}
		const auto name = entry.find("nome");
		if (name == entry.end() || !name->is_string()) {
			continue;
		}
		NamedMouseStyle item;
		item.name = trimmed(name->get<std::string>());
		if (item.name.empty()) {
			continue;
		}
		if (const Json *style = child(entry, "estilo")) {
			item.style = styleFromObject(*style);
		}
		result.push_back(std::move(item));
		if (result.size() == kMaxSavedStyles) {
			break;
		}
	}
	return result;
}

std::vector<NamedMouseStyle> upsertSavedStyle(std::vector<NamedMouseStyle> styles, const std::string &name,
					      const MouseStyle &style)
{
	const std::string clean = trimmed(name);
	if (clean.empty()) {
		return styles;
	}
	for (NamedMouseStyle &item : styles) {
		if (item.name == clean) {
			item.style = clampMouseStyle(style);
			return styles;
		}
	}
	styles.push_back(NamedMouseStyle{clean, clampMouseStyle(style)});
	while (styles.size() > kMaxSavedStyles) {
		styles.erase(styles.begin());
	}
	return styles;
}

std::vector<NamedMouseStyle> removeSavedStyle(std::vector<NamedMouseStyle> styles, const std::string &name)
{
	const std::string clean = trimmed(name);
	styles.erase(std::remove_if(styles.begin(), styles.end(),
				    [&clean](const NamedMouseStyle &item) { return item.name == clean; }),
		     styles.end());
	return styles;
}

} // namespace voltz
```

Em `frontend/voltz/core/CMakeLists.txt`, acrescentar `mouse-style.cpp` em `PRIVATE` (depois de `iso-encoders.cpp`) e
`mouse-style.hpp` em `PUBLIC` (depois de `iso-encoders.hpp`):
```cmake
    iso-encoders.cpp
    mouse-style.cpp
    preset-layout.cpp
```
```cmake
    iso-encoders.hpp
    mouse-style.hpp
    preset-layout.hpp
```

- [ ] **Step 4: Testes passam**

CONFIGURAR; COMPILAR TESTES; RODAR TESTES.
Expected: `100% tests passed`; `voltz-core-tests.exe` → `test cases: 115 | 115 passed`.

- [ ] **Step 5: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/mouse-style.hpp frontend/voltz/core/mouse-style.cpp frontend/voltz/tests/test-mouse-style.cpp
git add frontend/voltz/core/mouse-style.hpp frontend/voltz/core/mouse-style.cpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/test-mouse-style.cpp frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): estilo do destaque do mouse no nucleo (JSON da spec, limites, estilos salvos)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Núcleo — geometria do halo, onda do clique, holofote e protocolo frontend → plugin

**Files:**
- Create: `frontend/voltz/core/mouse-effects.hpp`, `frontend/voltz/core/mouse-effects.cpp`,
  `frontend/voltz/core/live-protocol.hpp`
- Create: `frontend/voltz/tests/test-mouse-effects.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

- [ ] **Step 1: Escrever o teste que falha**

`frontend/voltz/tests/test-mouse-effects.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/live-protocol.hpp>
#include <voltz/core/mouse-effects.hpp>

#include <cstdint>
#include <string>
#include <vector>

using voltz::MouseButton;
using voltz::MouseStyle;
using voltz::Ripple;

TEST_CASE("haloGeometry: disco cheio e anel com espessura mínima de 2 px")
{
	voltz::HaloStyle halo;
	voltz::HaloGeometry geometry = voltz::haloGeometry(halo);
	CHECK(geometry.diameter == doctest::Approx(46.0));
	CHECK(geometry.innerRadius == doctest::Approx(0.0));
	halo.shape = voltz::HaloShape::Ring;
	geometry = voltz::haloGeometry(halo);
	CHECK(geometry.innerRadius == doctest::Approx(23.0 - 5.52));
	halo.size = 12;
	CHECK(voltz::haloGeometry(halo).innerRadius == doctest::Approx(4.0));
}

TEST_CASE("rippleAt: a onda cresce com ease-out, some aos 450 ms e tem raio mínimo")
{
	MouseStyle style;
	const voltz::RippleFrame start = voltz::rippleAt(style, 0);
	CHECK(start.visible);
	CHECK(start.radius == doctest::Approx(12.42));
	CHECK(start.thickness == doctest::Approx(4.968));
	CHECK(start.alpha == doctest::Approx(0.9));
	const voltz::RippleFrame half = voltz::rippleAt(style, voltz::kRippleDurationNs / 2);
	CHECK(half.radius == doctest::Approx(37.7775));
	CHECK(half.alpha == doctest::Approx(0.45));
	CHECK_FALSE(voltz::rippleAt(style, voltz::kRippleDurationNs).visible);
	style.halo.size = 12;
	CHECK(voltz::rippleAt(style, voltz::kRippleDurationNs - 1).radius == doctest::Approx(21.6));
}

TEST_CASE("rippleAt: clique desligado não desenha; idade negativa conta como zero")
{
	MouseStyle style;
	style.click.active = false;
	CHECK_FALSE(voltz::rippleAt(style, 0).visible);
	style.click.active = true;
	CHECK(voltz::rippleAt(style, -5000000).radius == doctest::Approx(12.42));
}

TEST_CASE("rippleColor: cor própria só no botão direito")
{
	const voltz::ClickStyle click;
	CHECK(voltz::rippleColor(click, MouseButton::Right) == click.rightColor);
	CHECK(voltz::rippleColor(click, MouseButton::Left) == click.color);
	CHECK(voltz::rippleColor(click, MouseButton::Middle) == click.color);
}

TEST_CASE("pushRipple guarda no máximo 16 e pruneRipples tira as ondas vencidas")
{
	std::vector<Ripple> ripples;
	for (int i = 0; i < 20; i++) {
		voltz::pushRipple(ripples, Ripple{i, 0, MouseButton::Left, static_cast<int64_t>(i) * 1000000});
	}
	REQUIRE(ripples.size() == voltz::kMaxRipples);
	CHECK(ripples.front().x == 4);
	CHECK(ripples.back().x == 19);
	voltz::pruneRipples(ripples, 19 * 1000000 + voltz::kRippleDurationNs - 5000000);
	REQUIRE(ripples.size() == 5);
	CHECK(ripples.front().x == 15);
}

TEST_CASE("spotlightGeometry: borda suave de 12% do raio, no mínimo 8 px")
{
	voltz::SpotlightStyle spotlight;
	const voltz::SpotlightGeometry geometry = voltz::spotlightGeometry(spotlight);
	CHECK(geometry.radius == doctest::Approx(220.0));
	CHECK(geometry.feather == doctest::Approx(26.4));
	CHECK(geometry.dim == doctest::Approx(0.55));
	spotlight.radius = 60;
	CHECK(voltz::spotlightGeometry(spotlight).feather == doctest::Approx(8.0));
}

TEST_CASE("protocolo frontend → plugin: códigos de botão e nomes")
{
	CHECK(voltz::live::buttonCode(MouseButton::Left) == 1);
	CHECK(voltz::live::buttonCode(MouseButton::Right) == 2);
	CHECK(voltz::live::buttonCode(MouseButton::Middle) == 3);
	CHECK(voltz::live::buttonCode(MouseButton::None) == 1);
	CHECK(voltz::live::buttonFromCode(2) == MouseButton::Right);
	CHECK(voltz::live::buttonFromCode(3) == MouseButton::Middle);
	CHECK(voltz::live::buttonFromCode(1) == MouseButton::Left);
	CHECK(voltz::live::buttonFromCode(99) == MouseButton::Left);
	CHECK(std::string(voltz::live::kMouseSourceId) == "voltz_mouse_source");
	CHECK(std::string(voltz::live::kDeclPosition).find(voltz::live::kProcPosition) != std::string::npos);
	CHECK(std::string(voltz::live::kDeclClick).find(voltz::live::kProcClick) != std::string::npos);
	CHECK(std::string(voltz::live::kDeclStyle).find(voltz::live::kProcStyle) != std::string::npos);
	CHECK(std::string(voltz::live::kDeclWanted).find(voltz::live::kProcWanted) != std::string::npos);
}
```

- [ ] **Step 2: Registrar o teste e ver falhar**

Em `frontend/voltz/tests/CMakeLists.txt`, acrescentar `test-mouse-effects.cpp` antes de `test-mouse-style.cpp`:
```cmake
    test-iso-encoders.cpp
    test-mouse-effects.cpp
    test-mouse-style.cpp
```
CONFIGURAR; COMPILAR TESTES.
Expected: FAIL com `Cannot open include file: 'voltz/core/live-protocol.hpp'` (C1083).

- [ ] **Step 3: Implementar**

`frontend/voltz/core/mouse-effects.hpp`:
```cpp
#pragma once

#include "cursor-track.hpp"
#include "mouse-style.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace voltz {

// Destaque do mouse (spec §6.1): contas usadas pela fonte Voltz Mouse (plugin) e pela prévia de Ajustes › Mouse.
// Unidades em pixels da captura; tempos no relógio do libobs (ns).
constexpr int64_t kRippleDurationNs = 450000000;
constexpr size_t kMaxRipples = 16;
constexpr int kRippleMinBase = 24;

// Halo: disco de diâmetro `tamanho`; no anel, espessura = max(2, 12% do diâmetro). innerRadius 0 = disco cheio.
struct HaloGeometry {
	double diameter = 0.0;
	double innerRadius = 0.0;
};
HaloGeometry haloGeometry(const HaloStyle &halo);

// Uma onda de clique, em pixels da captura.
struct Ripple {
	int x = 0;
	int y = 0;
	MouseButton button = MouseButton::Left;
	int64_t startNs = 0;
};

// Anel da onda `ageNs` depois do clique: raio final = max(24, tamanho do halo)·0,9; cresce de 30% a 100% com ease-out
// cúbico em 450 ms; alfa 0,9 → 0; espessura = max(2, 12% do raio final). Invisível com o clique desligado ou vencida.
struct RippleFrame {
	bool visible = false;
	double radius = 0.0;
	double thickness = 0.0;
	double alpha = 0.0;
};
RippleFrame rippleAt(const MouseStyle &style, int64_t ageNs);
RgbColor rippleColor(const ClickStyle &click, MouseButton button);
// Acrescenta no fim; acima de kMaxRipples descarta as mais antigas.
void pushRipple(std::vector<Ripple> &ripples, const Ripple &ripple);
// Tira as ondas com idade >= kRippleDurationNs.
void pruneRipples(std::vector<Ripple> &ripples, int64_t nowNs);

// Holofote: escurece (opacidade `dim`) tudo fora do círculo de raio `radius`, com borda suave de `feather`
// (max(8, 12% do raio)) centrada no raio.
struct SpotlightGeometry {
	double radius = 0.0;
	double feather = 0.0;
	double dim = 0.0;
};
SpotlightGeometry spotlightGeometry(const SpotlightStyle &spotlight);

} // namespace voltz
```

`frontend/voltz/core/mouse-effects.cpp`:
```cpp
#include "mouse-effects.hpp"

#include <algorithm>
#include <cstddef>

namespace voltz {

HaloGeometry haloGeometry(const HaloStyle &halo)
{
	HaloGeometry geometry;
	geometry.diameter = static_cast<double>(halo.size);
	if (halo.shape == HaloShape::Ring) {
		const double thickness = std::max(2.0, geometry.diameter * 0.12);
		geometry.innerRadius = std::max(0.0, geometry.diameter / 2.0 - thickness);
	}
	return geometry;
}

RippleFrame rippleAt(const MouseStyle &style, int64_t ageNs)
{
	RippleFrame frame;
	if (!style.click.active) {
		return frame;
	}
	const int64_t age = std::max<int64_t>(0, ageNs);
	if (age >= kRippleDurationNs) {
		return frame;
	}
	const double t = static_cast<double>(age) / static_cast<double>(kRippleDurationNs);
	const double remaining = 1.0 - t;
	const double ease = 1.0 - remaining * remaining * remaining;
	const double endRadius = static_cast<double>(std::max(kRippleMinBase, style.halo.size)) * 0.9;
	frame.visible = true;
	frame.radius = endRadius * (0.3 + 0.7 * ease);
	frame.thickness = std::max(2.0, endRadius * 0.12);
	frame.alpha = 0.9 * remaining;
	return frame;
}

RgbColor rippleColor(const ClickStyle &click, MouseButton button)
{
	return button == MouseButton::Right ? click.rightColor : click.color;
}

void pushRipple(std::vector<Ripple> &ripples, const Ripple &ripple)
{
	ripples.push_back(ripple);
	if (ripples.size() > kMaxRipples) {
		ripples.erase(ripples.begin(),
			      ripples.begin() + static_cast<std::ptrdiff_t>(ripples.size() - kMaxRipples));
	}
}

void pruneRipples(std::vector<Ripple> &ripples, int64_t nowNs)
{
	ripples.erase(std::remove_if(ripples.begin(), ripples.end(),
				     [nowNs](const Ripple &ripple) { return nowNs - ripple.startNs >= kRippleDurationNs; }),
		      ripples.end());
}

SpotlightGeometry spotlightGeometry(const SpotlightStyle &spotlight)
{
	SpotlightGeometry geometry;
	geometry.radius = static_cast<double>(spotlight.radius);
	geometry.feather = std::max(8.0, geometry.radius * 0.12);
	geometry.dim = spotlight.dim;
	return geometry;
}

} // namespace voltz
```

`frontend/voltz/core/live-protocol.hpp`:
```cpp
#pragma once

#include "cursor-track.hpp"

#include <cstdlib>

namespace voltz::live {

// Canal frontend → plugin voltz-sources: procedimentos no proc handler global do libobs (obs_get_proc_handler()),
// registrados pelo plugin no obs_module_load. Rodam na thread de quem chama; o plugin guarda tudo num estado com mutex.
// Coordenadas em pixels da captura alvo (as mesmas do cursor.jsonl).
constexpr const char *kMouseSourceId = "voltz_mouse_source";
// Ajuste da fonte Voltz Mouse: UUID da captura que ela cobre (dá o tamanho da fonte).
constexpr const char *kTargetSetting = "alvo";

constexpr const char *kProcPosition = "voltz_mouse_position";
constexpr const char *kDeclPosition = "void voltz_mouse_position(in int x, in int y, in bool inside)";
constexpr const char *kProcClick = "voltz_mouse_click";
constexpr const char *kDeclClick = "void voltz_mouse_click(in int x, in int y, in int button, in int t_ns)";
constexpr const char *kProcStyle = "voltz_mouse_style";
constexpr const char *kDeclStyle = "void voltz_mouse_style(in string json)";
constexpr const char *kProcWanted = "voltz_mouse_wanted";
constexpr const char *kDeclWanted = "void voltz_mouse_wanted(out int count)";

constexpr int kButtonLeft = 1;
constexpr int kButtonRight = 2;
constexpr int kButtonMiddle = 3;

inline int buttonCode(MouseButton button)
{
	switch (button) {
	case MouseButton::Right:
		return kButtonRight;
	case MouseButton::Middle:
		return kButtonMiddle;
	case MouseButton::Left:
	case MouseButton::None:
		break;
	}
	return kButtonLeft;
}

inline MouseButton buttonFromCode(int code)
{
	if (code == kButtonRight) {
		return MouseButton::Right;
	}
	if (code == kButtonMiddle) {
		return MouseButton::Middle;
	}
	return MouseButton::Left;
}

// Só testes: com VOLTZ_TEST_LIVE o plugin registra o fornecedor "voltz-teste" no obs-websocket (InjectMouse,
// MouseState, SetStyle) e o frontend não lê o mouse real.
constexpr const char *kTestEnv = "VOLTZ_TEST_LIVE";
constexpr const char *kTestVendor = "voltz-teste";

inline bool testModeEnabled()
{
	const char *value = std::getenv(kTestEnv);
	return value && *value;
}

} // namespace voltz::live
```

Em `frontend/voltz/core/CMakeLists.txt`: `mouse-effects.cpp` em `PRIVATE` antes de `mouse-style.cpp`; em `PUBLIC`,
`live-protocol.hpp` depois de `iso-encoders.hpp` e `mouse-effects.hpp` antes de `mouse-style.hpp`:
```cmake
    iso-encoders.cpp
    mouse-effects.cpp
    mouse-style.cpp
```
```cmake
    iso-encoders.hpp
    live-protocol.hpp
    mouse-effects.hpp
    mouse-style.hpp
```

- [ ] **Step 4: Testes passam**

CONFIGURAR; COMPILAR TESTES; RODAR TESTES.
Expected: `100% tests passed`; `test cases: 122 | 122 passed`.

- [ ] **Step 5: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/mouse-effects.hpp frontend/voltz/core/mouse-effects.cpp frontend/voltz/core/live-protocol.hpp frontend/voltz/tests/test-mouse-effects.cpp
git add frontend/voltz/core/mouse-effects.hpp frontend/voltz/core/mouse-effects.cpp frontend/voltz/core/live-protocol.hpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/test-mouse-effects.cpp frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): geometria do halo, onda do clique e holofote no nucleo; protocolo frontend-plugin

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Núcleo — manifesto com `mouse` e campos desconhecidos em `pausas[]`/`eventos[]`

**Files:**
- Modify: `frontend/voltz/core/project-manifest.hpp`, `frontend/voltz/core/project-manifest.cpp`
- Test: `frontend/voltz/tests/test-project-manifest.cpp`

- [ ] **Step 1: Escrever os testes que falham**

No fim de `frontend/voltz/tests/test-project-manifest.cpp` (os auxiliares `sample()`, `contains()` e `count()` já
existem no topo do arquivo):
```cpp
TEST_CASE("manifesto grava o bloco mouse da spec entre arquivos e eventos")
{
	ProjectManifest m = sample();
	m.hasMouse = true;
	const std::string json = voltz::serializeManifest(m);
	CHECK(contains(json, "\"mouse\": {"));
	CHECK(contains(json, "\"tamanho\": 46"));
	CHECK(contains(json, "\"forma\": \"cheio\""));
	CHECK(contains(json, "\"cor_direito\": \"#4DA3FF\""));
	CHECK(contains(json, "\"escurecer\": 0.55"));
	CHECK(json.find("\"arquivos\"") < json.find("\"mouse\""));
	CHECK(json.find("\"mouse\"") < json.find("\"eventos\""));
}

TEST_CASE("mouse volta do disco e campos desconhecidos dentro dele sobrevivem")
{
	const std::string text = R"({"formato":"voltz-projeto","versao":1,"nome":"X","status":"ok",
		"mouse":{"halo":{"ativo":false,"tamanho":80,"brilho":2},"zoom":{"nivel":1.5}}})";
	voltz::ManifestParse parsed = voltz::parseManifest(text);
	REQUIRE(parsed.ok);
	CHECK(parsed.manifest.hasMouse);
	CHECK_FALSE(parsed.manifest.mouse.halo.active);
	CHECK(parsed.manifest.mouse.halo.size == 80);
	CHECK(parsed.manifest.mouse.click.active);
	parsed.manifest.mouse.spotlight.active = true;
	const std::string rewritten = voltz::serializeManifest(parsed.manifest);
	CHECK(contains(rewritten, "\"brilho\": 2"));
	CHECK(contains(rewritten, "\"zoom\""));
	CHECK(contains(rewritten, "\"tamanho\": 80"));
	CHECK(contains(rewritten, "\"raio\": 220"));
	const voltz::ManifestParse again = voltz::parseManifest(rewritten);
	REQUIRE(again.ok);
	CHECK(again.manifest.mouse.spotlight.active);
}

TEST_CASE("manifesto sem mouse continua sem mouse ao reescrever")
{
	const std::string json = voltz::serializeManifest(sample());
	CHECK_FALSE(contains(json, "\"mouse\""));
	const voltz::ManifestParse parsed = voltz::parseManifest(json);
	REQUIRE(parsed.ok);
	CHECK_FALSE(parsed.manifest.hasMouse);
	CHECK_FALSE(contains(voltz::serializeManifest(parsed.manifest), "\"mouse\""));
}

TEST_CASE("campos desconhecidos dentro de pausas[] e eventos[] sobrevivem à reescrita")
{
	const std::string text = R"({"formato":"voltz-projeto","versao":1,"nome":"X","status":"gravando","duracao_ms":5000,
		"pausas":[{"inicio_ms":1000,"fim_ms":null,"origem_pausa":"atalho"}],
		"eventos":[{"t_ms":10,"tipo":"novo_evento","detalhe":{"a":1}},
		           {"t_ms":20,"tipo":"iso_falhou","faixa":"tela","motivo":"canvas","tentativas":3}]})";
	voltz::ManifestParse parsed = voltz::parseManifest(text);
	REQUIRE(parsed.ok);
	REQUIRE(parsed.manifest.events.size() == 2);
	CHECK(parsed.manifest.events[1].reason == "canvas");
	voltz::recoverManifest(parsed.manifest, 8000);
	const std::string rewritten = voltz::serializeManifest(parsed.manifest);
	CHECK(contains(rewritten, "\"origem_pausa\": \"atalho\""));
	CHECK(contains(rewritten, "\"fim_ms\": 8000"));
	CHECK(contains(rewritten, "\"detalhe\""));
	CHECK(contains(rewritten, "\"tentativas\": 3"));
	CHECK(contains(rewritten, "\"tipo\": \"recuperado\""));
	CHECK(count(rewritten, "\"tentativas\"") == 1);
}
```

- [ ] **Step 2: Ver falhar**

COMPILAR TESTES.
Expected: FAIL com `'hasMouse': is not a member of 'voltz::ProjectManifest'` (C2039).

- [ ] **Step 3: Modelo do manifesto**

Em `frontend/voltz/core/project-manifest.hpp`:
1. Depois de `#include "project-timeline.hpp"`, acrescentar `#include "mouse-style.hpp"`.
2. Trocar a struct `ManifestEvent` por:
```cpp
// eventos[]: { "t_ms", "tipo", "faixa"?, "codigo"?, "motivo"? } + campos que esta versão não conhece (extraJson, objeto
// JSON compacto), devolvidos ao reescrever.
struct ManifestEvent {
	int64_t tMs = 0;
	std::string type;
	std::string track;
	int code = 0;
	std::string reason;
	std::string extraJson;
};
```
3. Em `ProjectManifest`, entre `std::vector<ManifestEvent> events;` e o comentário de `preservedJson`, acrescentar:
```cpp
	// "mouse" (spec §5.2): estilo do destaque do mouse no início do projeto. hasMouse falso = bloco ausente.
	bool hasMouse = false;
	MouseStyle mouse;
	// Campos desconhecidos de cada pausas[i] (objeto JSON compacto ou vazio), na ordem de `pauses`.
	std::vector<std::string> pauseExtras;
```
4. Trocar o comentário de `preservedJson` por:
```cpp
	// JSON lido do disco. Ao reescrever, os campos que esta versão não conhece (no nível de cima, dentro de cada
	// arquivos.*, de "mouse", de cada pausa e de cada evento) voltam intactos; arquivo ausente no modelo sai de
	// "arquivos".
```

- [ ] **Step 4: Serialização e leitura**

Em `frontend/voltz/core/project-manifest.cpp`:
1. Acrescentar `#include <initializer_list>` aos includes de sistema.
2. No `namespace {`, depois de `readIso(...)`, acrescentar:
```cpp
// Campos de `item` fora de `known`, como objeto JSON compacto ("" se não houver).
std::string extrasOf(const Json &item, std::initializer_list<const char *> known)
{
	Json extra = Json::object();
	for (auto it = item.begin(); it != item.end(); ++it) {
		const bool isKnown =
			std::any_of(known.begin(), known.end(), [&it](const char *key) { return it.key() == key; });
		if (!isKnown) {
			extra[it.key()] = it.value();
		}
	}
	return extra.empty() ? std::string() : extra.dump(-1, ' ', false, Json::error_handler_t::replace);
}

// Devolve ao item os campos guardados por extrasOf, sem sobrescrever os que o modelo escreveu.
void appendExtras(Json &item, const std::string &extras)
{
	if (extras.empty()) {
		return;
	}
	const Json extra = Json::parse(extras, nullptr, false);
	if (!extra.is_object()) {
		return;
	}
	for (auto it = extra.begin(); it != extra.end(); ++it) {
		if (!item.contains(it.key())) {
			item[it.key()] = it.value();
		}
	}
}

// Funde `fresh` em `target`: objeto com objeto desce um nível; o resto é sobrescrito. Chaves só de `target` ficam.
void mergeInto(Json &target, const Json &fresh)
{
	for (auto it = fresh.begin(); it != fresh.end(); ++it) {
		const auto existing = target.find(it.key());
		if (it->is_object() && existing != target.end() && existing->is_object()) {
			mergeInto(*existing, *it);
		} else {
			target[it.key()] = it.value();
		}
	}
}
```
3. Em `serializeManifest`, trocar o laço das pausas:
```cpp
	Json pauses = Json::array();
	for (const PauseSpan &pause : m.pauses) {
		Json item = Json::object();
		item["inicio_ms"] = pause.startMs;
		item["fim_ms"] = pause.endMs < 0 ? Json(nullptr) : Json(pause.endMs);
		pauses.push_back(std::move(item));
	}
```
por:
```cpp
	Json pauses = Json::array();
	for (size_t i = 0; i < m.pauses.size(); i++) {
		const PauseSpan &pause = m.pauses[i];
		Json item = Json::object();
		item["inicio_ms"] = pause.startMs;
		item["fim_ms"] = pause.endMs < 0 ? Json(nullptr) : Json(pause.endMs);
		appendExtras(item, i < m.pauseExtras.size() ? m.pauseExtras[i] : std::string());
		pauses.push_back(std::move(item));
	}
```
4. Ainda em `serializeManifest`, logo **antes** de `Json events = Json::array();`, acrescentar:
```cpp
	if (m.hasMouse) {
		const Json fresh = Json::parse(mouseStyleToJson(m.mouse), nullptr, false);
		Json &mouse = root["mouse"];
		if (!mouse.is_object()) {
			mouse = Json::object();
		}
		mergeInto(mouse, fresh);
	}
```
5. No laço dos eventos, depois do bloco `if (!event.reason.empty()) { ... }`, acrescentar
   `appendExtras(item, event.extraJson);`.
6. Em `parseManifest`, trocar o laço das pausas por:
```cpp
	if (const Json *pauses = getChild(root, "pausas", true)) {
		for (const Json &item : *pauses) {
			if (!item.is_object()) {
				continue;
			}
			m.pauses.push_back(PauseSpan{getInt(item, "inicio_ms", 0), getInt(item, "fim_ms", -1)});
			m.pauseExtras.push_back(extrasOf(item, {"inicio_ms", "fim_ms"}));
		}
	}
```
7. Trocar o `push_back` dos eventos por:
```cpp
			m.events.push_back(ManifestEvent{getInt(item, "t_ms", 0), getString(item, "tipo"),
							 getString(item, "faixa"),
							 static_cast<int>(getInt(item, "codigo", 0)),
							 getString(item, "motivo"),
							 extrasOf(item, {"t_ms", "tipo", "faixa", "codigo", "motivo"})});
```
8. Depois do bloco `if (const Json *events = ...) { ... }` e antes de `m.preservedJson = text;`, acrescentar:
```cpp
	if (const Json *mouse = getChild(root, "mouse", false)) {
		m.hasMouse = mouseStyleFromJson(mouse->dump(), m.mouse);
	}
```

- [ ] **Step 5: Testes passam**

COMPILAR TESTES; RODAR TESTES.
Expected: `100% tests passed`; `test cases: 126 | 126 passed` (os casos antigos de manifesto continuam passando: o
"versão mais nova… sobrevivem" agora lê o `mouse` e o reescreve fundido, ainda com `"halo"`).

- [ ] **Step 6: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/project-manifest.hpp frontend/voltz/core/project-manifest.cpp frontend/voltz/tests/test-project-manifest.cpp
git add frontend/voltz/core/project-manifest.hpp frontend/voltz/core/project-manifest.cpp frontend/voltz/tests/test-project-manifest.cpp
git commit -m "feat(voltz): manifesto com o estilo do mouse e campos desconhecidos em pausas/eventos preservados

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 4: Plugin `voltz-sources` — fonte Voltz Mouse, estado ao vivo e fornecedor de teste

**Files:**
- Create: `plugins/voltz-sources/CMakeLists.txt`, `plugins/voltz-sources/cmake/windows/obs-module.rc.in`
- Create: `plugins/voltz-sources/voltz-sources.cpp`, `plugins/voltz-sources/mouse-feed.hpp`,
  `plugins/voltz-sources/mouse-feed.cpp`, `plugins/voltz-sources/mouse-source.hpp`,
  `plugins/voltz-sources/mouse-source.cpp`, `plugins/voltz-sources/test-vendor.hpp`,
  `plugins/voltz-sources/test-vendor.cpp`
- Create: `plugins/voltz-sources/data/voltz-mouse.effect`, `plugins/voltz-sources/data/locale/en-US.ini`,
  `plugins/voltz-sources/data/locale/pt-BR.ini`
- Modify: `frontend/cmake/feature-voltz.cmake`

- [ ] **Step 1: CMake do plugin (entra pelo `feature-voltz.cmake`)**

`plugins/voltz-sources/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.28...3.30)

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

target_link_libraries(voltz-sources PRIVATE OBS::libobs OBS::websocket-api Voltz::core)

if(OS_WINDOWS)
  configure_file(cmake/windows/obs-module.rc.in voltz-sources.rc)
  target_sources(voltz-sources PRIVATE voltz-sources.rc)
endif()

set_target_properties_obs(voltz-sources PROPERTIES FOLDER plugins PREFIX "")
```

`plugins/voltz-sources/cmake/windows/obs-module.rc.in`:
```
1 VERSIONINFO
FILEVERSION ${OBS_VERSION_MAJOR},${OBS_VERSION_MINOR},${OBS_VERSION_PATCH},0
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904B0"
    BEGIN
      VALUE "CompanyName", "${OBS_COMPANY_NAME}"
      VALUE "FileDescription", "Voltz Studio sources module"
      VALUE "FileVersion", "${OBS_VERSION_CANONICAL}"
      VALUE "ProductName", "${OBS_PRODUCT_NAME}"
      VALUE "ProductVersion", "${OBS_VERSION_CANONICAL}"
      VALUE "Comments", "${OBS_COMMENTS}"
      VALUE "LegalCopyright", "${OBS_LEGAL_COPYRIGHT}"
      VALUE "InternalName", "voltz-sources"
      VALUE "OriginalFilename", "voltz-sources"
    END
  END

  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x0409, 0x04B0
  END
END
```

Em `frontend/cmake/feature-voltz.cmake`, logo depois de `add_subdirectory(voltz/core)`:
```cmake
add_subdirectory(voltz/core)

# Plugin das fontes Voltz (Voltz Mouse). Usa o voltz-core, por isso entra aqui (depois dele) e não em
# plugins/CMakeLists.txt: nenhum arquivo do OBS muda.
add_subdirectory("${CMAKE_SOURCE_DIR}/plugins/voltz-sources" "${CMAKE_BINARY_DIR}/plugins/voltz-sources")
```

- [ ] **Step 2: Estado ao vivo e procedimentos (`MouseFeed`)**

`plugins/voltz-sources/mouse-feed.hpp`:
```cpp
#pragma once

#include <voltz/core/mouse-effects.hpp>
#include <voltz/core/mouse-style.hpp>

#include <obs.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace voltz {

// Retrato do estado ao vivo para desenhar um quadro.
struct MouseSnapshot {
	bool inside = false;
	int x = 0;
	int y = 0;
	MouseStyle style;
	std::vector<Ripple> ripples;
};

// Estado ao vivo do mouse, um só no processo. Escrito pelos procedimentos (thread do LiveMouse: posição e cliques;
// thread da UI: estilo) e pelos pedidos de teste (thread do obs-websocket); lido pelas fontes Voltz Mouse na thread
// de render. Trechos curtos sob um mutex; `showing` é atômico.
class MouseFeed {
public:
	static MouseFeed &instance();

	void setPosition(int x, int y, bool inside);
	void addClick(int x, int y, MouseButton button, int64_t tNs);
	// JSON do núcleo (mouseStyleFromJson). Falso (estilo anterior mantido) se inválido.
	bool setStyleJson(const std::string &json);
	// Copia o estado e descarta as ondas vencidas em `nowNs`.
	MouseSnapshot snapshot(int64_t nowNs);

	// Fontes Voltz Mouse sendo mostradas (show/hide do libobs): o frontend só lê o mouse quando > 0.
	void addShowing(int delta) { showing_.fetch_add(delta); }
	int showing() const { return showing_.load(); }

private:
	MouseFeed() = default;

	std::mutex mutex_;
	bool inside_ = false;
	int x_ = 0;
	int y_ = 0;
	MouseStyle style_;
	std::vector<Ripple> ripples_;
	std::atomic<int> showing_{0};
};

// Registra voltz_mouse_position, voltz_mouse_click, voltz_mouse_style e voltz_mouse_wanted (live-protocol.hpp).
void registerMouseProcs(proc_handler_t *handler);

} // namespace voltz
```

`plugins/voltz-sources/mouse-feed.cpp`:
```cpp
#include "mouse-feed.hpp"

#include <voltz/core/live-protocol.hpp>

namespace voltz {

namespace {

void procPosition(void *, calldata_t *params)
{
	MouseFeed::instance().setPosition(static_cast<int>(calldata_int(params, "x")),
					  static_cast<int>(calldata_int(params, "y")), calldata_bool(params, "inside"));
}

void procClick(void *, calldata_t *params)
{
	MouseFeed::instance().addClick(static_cast<int>(calldata_int(params, "x")),
				       static_cast<int>(calldata_int(params, "y")),
				       live::buttonFromCode(static_cast<int>(calldata_int(params, "button"))),
				       static_cast<int64_t>(calldata_int(params, "t_ns")));
}

void procStyle(void *, calldata_t *params)
{
	const char *json = calldata_string(params, "json");
	if (!json || !MouseFeed::instance().setStyleJson(json)) {
		blog(LOG_WARNING, "[voltz-sources] estilo do mouse invalido; mantido o anterior");
	}
}

void procWanted(void *, calldata_t *params)
{
	calldata_set_int(params, "count", MouseFeed::instance().showing());
}

} // namespace

MouseFeed &MouseFeed::instance()
{
	static MouseFeed feed;
	return feed;
}

void MouseFeed::setPosition(int x, int y, bool inside)
{
	std::lock_guard<std::mutex> lock(mutex_);
	x_ = x;
	y_ = y;
	inside_ = inside;
}

void MouseFeed::addClick(int x, int y, MouseButton button, int64_t tNs)
{
	std::lock_guard<std::mutex> lock(mutex_);
	pushRipple(ripples_, Ripple{x, y, button, tNs});
}

bool MouseFeed::setStyleJson(const std::string &json)
{
	MouseStyle style;
	if (!mouseStyleFromJson(json, style)) {
		return false;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	style_ = style;
	return true;
}

MouseSnapshot MouseFeed::snapshot(int64_t nowNs)
{
	std::lock_guard<std::mutex> lock(mutex_);
	pruneRipples(ripples_, nowNs);
	MouseSnapshot snap;
	snap.inside = inside_;
	snap.x = x_;
	snap.y = y_;
	snap.style = style_;
	snap.ripples = ripples_;
	return snap;
}

void registerMouseProcs(proc_handler_t *handler)
{
	proc_handler_add(handler, live::kDeclPosition, procPosition, nullptr);
	proc_handler_add(handler, live::kDeclClick, procClick, nullptr);
	proc_handler_add(handler, live::kDeclStyle, procStyle, nullptr);
	proc_handler_add(handler, live::kDeclWanted, procWanted, nullptr);
}

} // namespace voltz
```

- [ ] **Step 3: O efeito**

`plugins/voltz-sources/data/voltz-mouse.effect`:
```hlsl
// Voltz Mouse (plugin voltz-sources): círculo/anel suavizado e holofote. Unidades em pixels do sprite desenhado.
uniform float4x4 ViewProj;
uniform float4 color;
uniform float2 size_px;
uniform float inner_px;
uniform float2 center_px;
uniform float radius_px;
uniform float feather_px;

struct VertInOut {
	float4 pos : POSITION;
	float2 uv  : TEXCOORD0;
};

VertInOut VSDefault(VertInOut vert_in)
{
	VertInOut vert_out;
	vert_out.pos = mul(float4(vert_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv  = vert_in.uv;
	return vert_out;
}

// Disco (inner_px = 0) ou anel (inner_px > 0) inscrito no sprite; borda suavizada em ~1 px.
float4 PSCircle(VertInOut vert_in) : TARGET
{
	float2 p = (vert_in.uv - float2(0.5, 0.5)) * size_px;
	float r = length(p);
	float outer = size_px.x * 0.5;
	float coverage = saturate(outer - r + 0.5);
	float ring = saturate(r - inner_px + 0.5);
	coverage = coverage * lerp(1.0, ring, step(0.001, inner_px));
	return float4(color.rgb, color.a * coverage);
}

// Escurece tudo fora do círculo (center_px, radius_px), com borda suave de feather_px centrada no raio.
float4 PSSpotlight(VertInOut vert_in) : TARGET
{
	float2 p = vert_in.uv * size_px - center_px;
	float r = length(p);
	float coverage = saturate((r - radius_px) / max(feather_px, 1.0) + 0.5);
	return float4(color.rgb, color.a * coverage);
}

technique Circle
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader  = PSCircle(vert_in);
	}
}

technique Spotlight
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader  = PSSpotlight(vert_in);
	}
}
```

- [ ] **Step 4: A fonte Voltz Mouse**

`plugins/voltz-sources/mouse-source.hpp`:
```cpp
#pragma once

namespace voltz {

// Registra a fonte "voltz_mouse_source" (Voltz Mouse): tamanho = tamanho da captura do ajuste "alvo" (UUID);
// desenha holofote, ondas de clique e halo a partir do MouseFeed. Fica fora do "Adicionar fonte"
// (OBS_SOURCE_CAP_DISABLED): quem cria é o preset/migração do Voltz.
void registerMouseSource();

} // namespace voltz
```

`plugins/voltz-sources/mouse-source.cpp`:
```cpp
#include "mouse-source.hpp"
#include "mouse-feed.hpp"

#include <voltz/core/live-protocol.hpp>
#include <voltz/core/mouse-effects.hpp>

#include <obs-module.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <util/platform.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace voltz {

namespace {

constexpr int kTargetRetryTicks = 60;

struct MouseSource {
	obs_source_t *self = nullptr;
	gs_effect_t *effect = nullptr;
	gs_eparam_t *color = nullptr;
	gs_eparam_t *sizePx = nullptr;
	gs_eparam_t *innerPx = nullptr;
	gs_eparam_t *centerPx = nullptr;
	gs_eparam_t *radiusPx = nullptr;
	gs_eparam_t *featherPx = nullptr;
	// Alvo: escrito no update (thread de quem atualiza) e lido no tick (thread gráfica).
	std::mutex targetMutex;
	std::string targetUuid;
	obs_weak_source_t *target = nullptr;
	int retryTicks = 0;
	std::atomic<uint32_t> width{0};
	std::atomic<uint32_t> height{0};
};

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

const char *mouseName(void *)
{
	return obs_module_text("VoltzMouse");
}

void mouseUpdate(void *data, obs_data_t *settings)
{
	auto *ctx = static_cast<MouseSource *>(data);
	const std::string uuid = obs_data_get_string(settings, live::kTargetSetting);
	std::lock_guard<std::mutex> lock(ctx->targetMutex);
	if (uuid == ctx->targetUuid && ctx->target) {
		return;
	}
	ctx->targetUuid = uuid;
	ctx->retryTicks = 0;
	resolveTarget(ctx);
}

void *mouseCreate(obs_data_t *settings, obs_source_t *source)
{
	auto *ctx = new MouseSource();
	ctx->self = source;
	char *path = obs_module_file("voltz-mouse.effect");
	obs_enter_graphics();
	ctx->effect = path ? gs_effect_create_from_file(path, nullptr) : nullptr;
	if (ctx->effect) {
		ctx->color = gs_effect_get_param_by_name(ctx->effect, "color");
		ctx->sizePx = gs_effect_get_param_by_name(ctx->effect, "size_px");
		ctx->innerPx = gs_effect_get_param_by_name(ctx->effect, "inner_px");
		ctx->centerPx = gs_effect_get_param_by_name(ctx->effect, "center_px");
		ctx->radiusPx = gs_effect_get_param_by_name(ctx->effect, "radius_px");
		ctx->featherPx = gs_effect_get_param_by_name(ctx->effect, "feather_px");
	}
	obs_leave_graphics();
	bfree(path);
	if (!ctx->effect) {
		blog(LOG_WARNING, "[voltz-sources] voltz-mouse.effect nao carregou: a fonte nao desenha");
	}
	mouseUpdate(ctx, settings);
	return ctx;
}

void mouseDestroy(void *data)
{
	auto *ctx = static_cast<MouseSource *>(data);
	if (ctx->effect) {
		obs_enter_graphics();
		gs_effect_destroy(ctx->effect);
		obs_leave_graphics();
	}
	{
		std::lock_guard<std::mutex> lock(ctx->targetMutex);
		if (ctx->target) {
			obs_weak_source_release(ctx->target);
			ctx->target = nullptr;
		}
	}
	delete ctx;
}

void mouseDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, live::kTargetSetting, "");
}

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

// Cor do núcleo → vetor linear (o desenho roda com gs_enable_framebuffer_srgb, como o color_source com transparência).
struct vec4 linearColor(const RgbColor &rgb, double alpha)
{
	const uint32_t packed = 0xFF000000u | (static_cast<uint32_t>(rgb.b) << 16) |
				(static_cast<uint32_t>(rgb.g) << 8) | static_cast<uint32_t>(rgb.r);
	struct vec4 color;
	vec4_from_rgba_srgb(&color, packed);
	color.w = static_cast<float>(alpha);
	return color;
}

void drawCircle(MouseSource *ctx, double centerX, double centerY, double diameter, double innerRadius,
		const RgbColor &rgb, double alpha)
{
	if (diameter < 1.0 || alpha <= 0.0) {
		return;
	}
	const struct vec4 color = linearColor(rgb, alpha);
	struct vec2 size;
	vec2_set(&size, static_cast<float>(diameter), static_cast<float>(diameter));
	gs_effect_set_vec4(ctx->color, &color);
	gs_effect_set_vec2(ctx->sizePx, &size);
	gs_effect_set_float(ctx->innerPx, static_cast<float>(std::max(0.0, innerRadius)));
	gs_matrix_push();
	gs_matrix_translate3f(static_cast<float>(centerX - diameter / 2.0), static_cast<float>(centerY - diameter / 2.0),
			      0.0f);
	while (gs_effect_loop(ctx->effect, "Circle")) {
		gs_draw_quadf(nullptr, 0, static_cast<float>(diameter), static_cast<float>(diameter));
	}
	gs_matrix_pop();
}

void drawSpotlight(MouseSource *ctx, const MouseSnapshot &snap, uint32_t width, uint32_t height)
{
	const SpotlightGeometry geometry = spotlightGeometry(snap.style.spotlight);
	struct vec4 color;
	vec4_set(&color, 0.0f, 0.0f, 0.0f, static_cast<float>(geometry.dim));
	struct vec2 size;
	vec2_set(&size, static_cast<float>(width), static_cast<float>(height));
	struct vec2 center;
	vec2_set(&center, static_cast<float>(snap.x) + 0.5f, static_cast<float>(snap.y) + 0.5f);
	gs_effect_set_vec4(ctx->color, &color);
	gs_effect_set_vec2(ctx->sizePx, &size);
	gs_effect_set_vec2(ctx->centerPx, &center);
	gs_effect_set_float(ctx->radiusPx, static_cast<float>(geometry.radius));
	gs_effect_set_float(ctx->featherPx, static_cast<float>(geometry.feather));
	while (gs_effect_loop(ctx->effect, "Spotlight")) {
		gs_draw_quadf(nullptr, 0, static_cast<float>(width), static_cast<float>(height));
	}
}

void mouseRender(void *data, gs_effect_t *)
{
	auto *ctx = static_cast<MouseSource *>(data);
	const uint32_t width = ctx->width.load();
	const uint32_t height = ctx->height.load();
	if (!ctx->effect || !width || !height) {
		return;
	}
	const int64_t now = static_cast<int64_t>(os_gettime_ns());
	const MouseSnapshot snap = MouseFeed::instance().snapshot(now);
	if (!snap.inside && snap.ripples.empty()) {
		return;
	}
	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);
	if (snap.inside && snap.style.spotlight.active) {
		drawSpotlight(ctx, snap, width, height);
	}
	for (const Ripple &ripple : snap.ripples) {
		const RippleFrame frame = rippleAt(snap.style, now - ripple.startNs);
		if (frame.visible) {
			drawCircle(ctx, ripple.x + 0.5, ripple.y + 0.5, frame.radius * 2.0, frame.radius - frame.thickness,
				   rippleColor(snap.style.click, ripple.button), frame.alpha);
		}
	}
	if (snap.inside && snap.style.halo.active) {
		const HaloGeometry halo = haloGeometry(snap.style.halo);
		drawCircle(ctx, snap.x + 0.5, snap.y + 0.5, halo.diameter, halo.innerRadius, snap.style.halo.color,
			   snap.style.halo.opacity);
	}
	gs_enable_framebuffer_srgb(previous);
}

} // namespace

void registerMouseSource()
{
	obs_source_info info = {};
	info.id = live::kMouseSourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_SRGB | OBS_SOURCE_CAP_DISABLED;
	info.get_name = mouseName;
	info.create = mouseCreate;
	info.destroy = mouseDestroy;
	info.update = mouseUpdate;
	info.get_defaults = mouseDefaults;
	info.get_width = mouseWidth;
	info.get_height = mouseHeight;
	info.video_tick = mouseTick;
	info.video_render = mouseRender;
	info.show = [](void *) { MouseFeed::instance().addShowing(1); };
	info.hide = [](void *) { MouseFeed::instance().addShowing(-1); };
	info.icon_type = OBS_ICON_TYPE_CUSTOM;
	obs_register_source(&info);
}

} // namespace voltz
```

- [ ] **Step 5: Fornecedor de teste e entrada do módulo**

`plugins/voltz-sources/test-vendor.hpp`:
```cpp
#pragma once

namespace voltz {

// Só com VOLTZ_TEST_LIVE (chamar no obs_module_post_load): fornecedor "voltz-teste" do obs-websocket com
// InjectMouse {x,y,inside?,click?: "l"|"r"|"m"}, MouseState (x, y, inside, ripples, showing, style), SetStyle {style}
// e AddMouseSource {sceneName, targetName, name?} (o CreateInput do websocket recusa fonte OBS_SOURCE_CAP_DISABLED).
// Permite testar o desenho sem mexer no mouse do usuário.
void registerTestVendor();

} // namespace voltz
```

`plugins/voltz-sources/test-vendor.cpp`:
```cpp
#include "test-vendor.hpp"
#include "mouse-feed.hpp"

#include <voltz/core/live-protocol.hpp>
#include <voltz/core/mouse-style.hpp>

#include <cassert>

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

// Cria uma Voltz Mouse com alvo = UUID de `targetName` e a põe por cima na cena `sceneName`.
void addMouseSource(obs_data_t *request, obs_data_t *response, void *)
{
	obs_source_t *sceneSource = obs_get_source_by_name(obs_data_get_string(request, "sceneName"));
	obs_source_t *target = obs_get_source_by_name(obs_data_get_string(request, "targetName"));
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	bool ok = false;
	if (scene && target) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, live::kTargetSetting, obs_source_get_uuid(target));
		const char *name = obs_data_get_string(request, "name");
		obs_source_t *mouse =
			obs_source_create(live::kMouseSourceId, *name ? name : "Mouse teste", settings, nullptr);
		if (mouse) {
			ok = obs_scene_add(scene, mouse) != nullptr;
			obs_source_release(mouse);
		}
		obs_data_release(settings);
	}
	obs_source_release(target);
	obs_source_release(sceneSource);
	obs_data_set_bool(response, "ok", ok);
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
	blog(LOG_WARNING, "[voltz-sources] modo de teste (%s): pedidos '%s' ligados", live::kTestEnv,
	     live::kTestVendor);
}

} // namespace voltz
```

`plugins/voltz-sources/voltz-sources.cpp`:
```cpp
#include "mouse-feed.hpp"
#include "mouse-source.hpp"
#include "test-vendor.hpp"

#include <voltz/core/live-protocol.hpp>

#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("voltz-sources", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Voltz Studio: fontes Voltz (destaque do mouse)";
}

bool obs_module_load(void)
{
	voltz::registerMouseSource();
	voltz::registerMouseProcs(obs_get_proc_handler());
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

`plugins/voltz-sources/data/locale/en-US.ini` (UTF-8 sem BOM):
```ini
VoltzMouse="Voltz Mouse"
```
`plugins/voltz-sources/data/locale/pt-BR.ini` (UTF-8 sem BOM):
```ini
VoltzMouse="Voltz Mouse"
```

- [ ] **Step 6: Compilar e conferir o que foi instalado no rundir**

CONFIGURAR; COMPILAR APP (segundo plano). Depois:
```bash
ls build_x64/rundir/RelWithDebInfo/obs-plugins/64bit/voltz-sources.dll \
   build_x64/rundir/RelWithDebInfo/data/obs-plugins/voltz-sources/voltz-mouse.effect \
   build_x64/rundir/RelWithDebInfo/data/obs-plugins/voltz-sources/locale/pt-BR.ini
git status --short -- plugins/CMakeLists.txt
diff <(cut -d= -f1 plugins/voltz-sources/data/locale/en-US.ini | sort) <(cut -d= -f1 plugins/voltz-sources/data/locale/pt-BR.ini | sort)
```
Expected: `exit=0`; os três arquivos listados; `git status` e `diff` sem saída. Se o linker reclamar de
`Voltz::core`/`voltz-core` não encontrado, confira que a linha do `add_subdirectory` do plugin está **depois** de
`add_subdirectory(voltz/core)` no `feature-voltz.cmake`.

- [ ] **Step 7: Teste ao vivo — fonte criada à mão, render por `GetSourceScreenshot`**

Bloco de teste (`$liveTest = $true`, `$standins = $false`). No `try`, depois do `Start-App`:
```powershell
$kinds = (Ws '[{"requestType":"GetInputKindList","requestData":{"unversioned":false}}]' -Quiet)[0].data.inputKinds
"voltz_mouse_source fora da lista de tipos (CAP_DISABLED): " + (-not (@($kinds) -contains "voltz_mouse_source"))
[void](Ws ('[{"requestType":"CreateScene","requestData":{"sceneName":"Cena teste"}},' +
  '{"requestType":"CreateInput","requestData":{"sceneName":"Cena teste","inputName":"Tela substituta","inputKind":"color_source_v3","inputSettings":{"color":4281346602,"width":2560,"height":1440}}},' +
  '{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Cena teste"}}]'))
"AddMouseSource: " + ((Vendor "AddMouseSource" '{"sceneName":"Cena teste","targetName":"Tela substituta"}').ok)
"tipo da 'Mouse teste': " + ((@((Ws '[{"requestType":"GetInputList"}]' -Quiet)[0].data.inputs) | Where-Object { $_.inputName -eq "Mouse teste" }).inputKind)
Start-Sleep 2
"estado inicial: " + ((Vendor "MouseState") | ConvertTo-Json -Compress)
[void](Vendor "SetStyle" (ConvertTo-Json -Compress @{ style = $testStyle }))
# itens sem escala na cena 1920x1080: pixel (960,540) da captura = centro da imagem 640x360
[void](Vendor "InjectMouse" '{"x":960,"y":540}'); Start-Sleep -Milliseconds 300
$img = Scene-Image "Cena teste"; $center = $img.GetPixel(320, 180); $corner = $img.GetPixel(40, 40); $img.Save("$sp\t4-halo.png"); $img.Dispose()
"halo: centro=$center lima=$(Near $center 212 245 60) | canto=$corner tela=$(Near $corner 42 42 48)"
[void](Vendor "SetStyle" (ConvertTo-Json -Compress @{ style = ($testStyle -replace '"holofote":\{"ativo":false', '"holofote":{"ativo":true') }))
Start-Sleep -Milliseconds 300
$img = Scene-Image "Cena teste"; $dark = $img.GetPixel(40, 40); $center2 = $img.GetPixel(320, 180); $img.Save("$sp\t4-holofote.png"); $img.Dispose()
"holofote: canto=$dark (antes $corner) mais escuro=$($dark.R -le $corner.R - 8) | centro=$center2 lima=$(Near $center2 212 245 60)"
# clique + estado na MESMA conexao (cada Ws abre um powershell: ~1 s, mais que os 450 ms da onda)
$r = Ws '[{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste","requestType":"InjectMouse","requestData":{"x":960,"y":540,"click":"r"}}},{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste","requestType":"MouseState","requestData":{}}}]'
$r1 = $r[1].data.responseData.ripples
Start-Sleep 1; $r2 = (Vendor "MouseState").ripples
"ondas: logo depois=$r1 | 1 s depois=$r2"
[void](Vendor "InjectMouse" '{"x":0,"y":0,"inside":false}'); Start-Sleep -Milliseconds 300
$img = Scene-Image "Cena teste"; $outside = $img.GetPixel(320, 180); $img.Dispose()
"fora da captura: centro=$outside tela=$(Near $outside 42 42 48)"
"estado: " + ((Vendor "MouseState") | ConvertTo-Json -Compress)
Close-App; Save-Log
LogLines '\[voltz-sources\]|voltz-mouse|effect'
```
Critérios:
1. Log com `[voltz-sources] carregado` e `[voltz-sources] modo de teste (VOLTZ_TEST_LIVE): pedidos 'voltz-teste'
   ligados`; **sem** `voltz-mouse.effect nao carregou` nem erro de compilação de shader com `voltz-mouse` no texto.
2. `voltz_mouse_source fora da lista de tipos (CAP_DISABLED): True`; `AddMouseSource: True`; `tipo da 'Mouse teste':
   voltz_mouse_source`.
3. Estado inicial: `showing` ≥ 1 (a cena está no programa), `inside` falso, `ripples` 0.
4. `halo: … lima=True | … tela=True`; `holofote: … mais escuro=True | … lima=True` (o halo fica por cima do
   holofote); `ondas: logo depois=1 | 1 s depois=0`; `fora da captura: … tela=True`.
5. Abra `t4-halo.png` e `t4-holofote.png` (imagens sintéticas: cor + halo) e descreva: disco lima de ~67 px de
   diâmetro (200 px da captura ÷ 3) no centro sobre cinza-escuro; na segunda, o mesmo disco dentro de um círculo claro com o resto escurecido.

Se o halo não aparecer mas o resto passar, confira primeiro a `ViewProj`/ordem de translação no `drawCircle` (sprite
começa no canto superior esquerdo do círculo); se o shader não compilar, o log do libobs mostra a linha do `.effect`.

- [ ] **Step 8: Formatar e commitar**

```bash
"$CLANGFMT" -i plugins/voltz-sources/*.cpp plugins/voltz-sources/*.hpp
git add plugins/voltz-sources frontend/cmake/feature-voltz.cmake
git status --short
git commit -m "feat(voltz): plugin voltz-sources com a fonte Voltz Mouse (halo, onda, holofote) e canal ao vivo

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
Expected: `git status --short` antes do commit mostra só arquivos de `plugins/voltz-sources/` e o
`feature-voltz.cmake` (nenhum `build-compile.log`).

---
### Task 5: Frontend — `MouseStyleStore` (estilo + atalho do holofote) e `LiveMouse` (mouse ao vivo → plugin)

**Files:**
- Create: `frontend/voltz/shell/MouseStyleStore.hpp`, `frontend/voltz/shell/MouseStyleStore.cpp`
- Create: `frontend/voltz/project/LiveMouse.hpp`, `frontend/voltz/project/LiveMouse.cpp`
- Modify: `frontend/cmake/feature-voltz.cmake`, `frontend/voltz/shell/VoltzShell.hpp`,
  `frontend/voltz/shell/VoltzShell.cpp`, `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`

- [ ] **Step 1: `MouseStyleStore`**

`frontend/voltz/shell/MouseStyleStore.hpp`:
```cpp
#pragma once

#include <voltz/core/mouse-style.hpp>

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QObject>
#include <QString>
#include <QTimer>

#include <vector>

namespace voltz {

// Estilo do destaque do mouse (spec §6.1): o atual em user.ini [Voltz] MouseStyle e os salvos em [Voltz] MouseStyles
// (JSON do núcleo, mesmo formato do projeto.voltz → mouse). Avisa quem desenha ou registra (LiveMouse → plugin,
// Ajustes › Mouse, ProjectRecorder) e registra o atalho do OBS "VoltzStudio.ToggleSpotlight" (padrão Ctrl+Shift+H).
// Thread da UI.
class MouseStyleStore : public QObject {
	Q_OBJECT

public:
	explicit MouseStyleStore(QObject *parent = nullptr);
	~MouseStyleStore() override;

	const MouseStyle &style() const { return style_; }
	// Limita, aplica na hora (styleChanged; spotlightToggled se o holofote mudou) e grava no user.ini 400 ms depois
	// da última mudança (arrastar um slider não regrava o arquivo a cada passo).
	void setStyle(const MouseStyle &style);
	void toggleSpotlight();

	std::vector<NamedMouseStyle> savedStyles() const;
	void saveCurrentAs(const QString &name);
	bool applySaved(const QString &name);
	void removeSaved(const QString &name);

	// Teclas do atalho do holofote em texto (ex. "Ctrl + Shift + H"); vazio sem atalho.
	QString spotlightShortcutText() const;

signals:
	void styleChanged();
	void spotlightToggled(bool active);
	void savedStylesChanged();

private:
	static void handleEvent(enum obs_frontend_event event, void *data);
	static void handleHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	void loadHotkeyBinding();
	void unregisterHotkey();
	void save();
	void writeSaved(const std::vector<NamedMouseStyle> &styles);

	MouseStyle style_;
	QTimer saveTimer_;
	obs_hotkey_id spotlightHotkey_ = OBS_INVALID_HOTKEY_ID;
	bool registered_ = true;
};

} // namespace voltz
```

`frontend/voltz/shell/MouseStyleStore.cpp`:
```cpp
#include "MouseStyleStore.hpp"

#include <OBSApp.hpp> // QTStr, App()

#include <util/config-file.h>
#include <util/dstr.h>

#include <QStringList>

#include <string>

#include "moc_MouseStyleStore.cpp"

namespace voltz {

namespace {

constexpr const char *kSpotlightHotkeyName = "VoltzStudio.ToggleSpotlight";
constexpr int kSaveDelayMs = 400;

std::string readVoltz(const char *key)
{
	const char *value = config_get_string(App()->GetUserConfig(), "Voltz", key);
	return value ? value : "";
}

void writeVoltz(const char *key, const std::string &value)
{
	config_set_string(App()->GetUserConfig(), "Voltz", key, value.c_str());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

} // namespace

MouseStyleStore::MouseStyleStore(QObject *parent) : QObject(parent)
{
	const std::string json = readVoltz("MouseStyle");
	if (!json.empty() && !mouseStyleFromJson(json, style_)) {
		blog(LOG_WARNING, "[voltz] MouseStyle invalido no user.ini; usando o padrao");
	}
	saveTimer_.setSingleShot(true);
	saveTimer_.setInterval(kSaveDelayMs);
	connect(&saveTimer_, &QTimer::timeout, this, [this]() { save(); });
	/* A casca nasce depois do OBSBasic::OBSInit: perfil e atalhos do OBS já estão carregados. */
	spotlightHotkey_ = obs_hotkey_register_frontend(kSpotlightHotkeyName,
							QTStr("Voltz.Hotkey.ToggleSpotlight").toUtf8().constData(),
							&MouseStyleStore::handleHotkey, this);
	loadHotkeyBinding();
	obs_frontend_add_event_callback(&MouseStyleStore::handleEvent, this);
}

MouseStyleStore::~MouseStyleStore()
{
	if (saveTimer_.isActive()) {
		save();
	}
	if (registered_) {
		obs_frontend_remove_event_callback(&MouseStyleStore::handleEvent, this);
	}
	unregisterHotkey();
}

void MouseStyleStore::setStyle(const MouseStyle &style)
{
	const bool spotlightBefore = style_.spotlight.active;
	style_ = clampMouseStyle(style);
	saveTimer_.start();
	emit styleChanged();
	if (style_.spotlight.active != spotlightBefore) {
		blog(LOG_INFO, "[voltz] holofote: %s", style_.spotlight.active ? "ligado" : "desligado");
		emit spotlightToggled(style_.spotlight.active);
	}
}

void MouseStyleStore::toggleSpotlight()
{
	MouseStyle next = style_;
	next.spotlight.active = !next.spotlight.active;
	setStyle(next);
}

void MouseStyleStore::save()
{
	saveTimer_.stop();
	writeVoltz("MouseStyle", mouseStyleToJson(style_));
}

std::vector<NamedMouseStyle> MouseStyleStore::savedStyles() const
{
	return savedStylesFromJson(readVoltz("MouseStyles"));
}

void MouseStyleStore::writeSaved(const std::vector<NamedMouseStyle> &styles)
{
	writeVoltz("MouseStyles", savedStylesToJson(styles));
	emit savedStylesChanged();
}

void MouseStyleStore::saveCurrentAs(const QString &name)
{
	writeSaved(upsertSavedStyle(savedStyles(), name.toStdString(), style_));
}

bool MouseStyleStore::applySaved(const QString &name)
{
	const std::string wanted = name.trimmed().toStdString();
	for (const NamedMouseStyle &item : savedStyles()) {
		if (item.name == wanted) {
			setStyle(item.style);
			blog(LOG_INFO, "[voltz] estilo do mouse aplicado: %s", item.name.c_str());
			return true;
		}
	}
	return false;
}

void MouseStyleStore::removeSaved(const QString &name)
{
	writeSaved(removeSavedStyle(savedStyles(), name.toStdString()));
}

QString MouseStyleStore::spotlightShortcutText() const
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

void MouseStyleStore::handleEvent(enum obs_frontend_event event, void *data)
{
	auto *self = static_cast<MouseStyleStore *>(data);
	switch (event) {
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		self->loadHotkeyBinding();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		if (self->saveTimer_.isActive()) {
			self->save();
		}
		self->unregisterHotkey();
		/* Seguro durante o próprio evento: OBSStudioAPI::on_event percorre os callbacks de trás para frente */
		obs_frontend_remove_event_callback(&MouseStyleStore::handleEvent, self);
		self->registered_ = false;
		break;
	default:
		break;
	}
}

void MouseStyleStore::handleHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<MouseStyleStore *>(data);
	/* Teclado: thread de atalhos do libobs; obs-websocket TriggerHotkeyByName: thread do websocket. Só enfileira. */
	QMetaObject::invokeMethod(self, [self]() { self->toggleSpotlight(); }, Qt::QueuedConnection);
}

void MouseStyleStore::loadHotkeyBinding()
{
	if (spotlightHotkey_ == OBS_INVALID_HOTKEY_ID) {
		return;
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

void MouseStyleStore::unregisterHotkey()
{
	if (spotlightHotkey_ == OBS_INVALID_HOTKEY_ID) {
		return;
	}
	obs_hotkey_unregister(spotlightHotkey_);
	spotlightHotkey_ = OBS_INVALID_HOTKEY_ID;
}

} // namespace voltz
```

- [ ] **Step 2: `LiveMouse`**

`frontend/voltz/project/LiveMouse.hpp`:
```cpp
#pragma once

#include <voltz/core/cursor-track.hpp>

#include <QObject>
#include <QTimer>

#include <atomic>
#include <future>
#include <mutex>
#include <thread>

namespace voltz {

class MouseStyleStore;

// Alimenta as fontes Voltz Mouse (plugin voltz-sources) com a posição e os cliques do mouse em pixels da captura de
// monitor compartilhada (a Tela do preset) — a mesma conversão do cursor.jsonl. Pergunta ao plugin a cada 500 ms se
// alguma Voltz Mouse está sendo mostrada e só então liga uma thread própria, sem gancho: posição por
// GetPhysicalCursorPos a cada 16 ms e cliques por Raw Input (RIDEV_INPUTSINK) numa janela só de mensagens
// (HWND_MESSAGE: invisível, não recebe nem bloqueia entrada). Também empurra o estilo para o plugin. Com
// VOLTZ_TEST_LIVE a thread nunca liga (os testes injetam o mouse pelo obs-websocket). Thread da UI, menos threadMain.
class LiveMouse : public QObject {
public:
	explicit LiveMouse(MouseStyleStore *styles, QObject *parent = nullptr);
	~LiveMouse() override;

	// Para a consulta e a thread. Chamar no EXIT do frontend, antes de o libobs descarregar o plugin.
	void shutdown();
	bool running() const { return running_; }

private:
	void poll();
	void pushStyle();
	void refreshMapping();
	void start();
	void stop();
	void threadMain(std::promise<void> ready);
	CapturePoint currentPoint();

	MouseStyleStore *styles_;
	QTimer poll_;
	std::thread thread_;
	std::atomic<unsigned long> threadId_{0};
	std::mutex mutex_;
	CaptureMapping mapping_; // protegido por mutex_ (a UI escreve, a thread lê)
	int mappingState_ = -1;  // -1 desconhecido, 0 sem captura de monitor, 1 com captura (só para o log)
	bool running_ = false;
	bool testMode_ = false;
	bool shutdown_ = false;
};

} // namespace voltz
```

`frontend/voltz/project/LiveMouse.cpp`:
```cpp
#include "LiveMouse.hpp"
#include "MonitorLocator.hpp"

#include <voltz/core/live-protocol.hpp>
#include <voltz/core/mouse-style.hpp>
#include <voltz/shell/MouseStyleStore.hpp>
#include <voltz/shell/SharedSources.hpp>

#include <obs.hpp>
#include <util/platform.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace voltz {

namespace {

constexpr int kPollMs = 500;
constexpr unsigned long kTickMs = 16;
constexpr int64_t kPositionIntervalNs = 16000000;

int queryWanted()
{
	uint8_t stack[128];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	if (!proc_handler_call(obs_get_proc_handler(), live::kProcWanted, &params)) {
		return 0;
	}
	return static_cast<int>(calldata_int(&params, "count"));
}

void sendPosition(const CapturePoint &point)
{
	uint8_t stack[256];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_int(&params, "x", point.x);
	calldata_set_int(&params, "y", point.y);
	calldata_set_bool(&params, "inside", point.inside);
	proc_handler_call(obs_get_proc_handler(), live::kProcPosition, &params);
}

void sendClick(const CapturePoint &point, MouseButton button, int64_t tNs)
{
	uint8_t stack[256];
	calldata_t params;
	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_int(&params, "x", point.x);
	calldata_set_int(&params, "y", point.y);
	calldata_set_int(&params, "button", live::buttonCode(button));
	calldata_set_int(&params, "t_ns", tNs);
	proc_handler_call(obs_get_proc_handler(), live::kProcClick, &params);
}

void sendStyle(const std::string &json)
{
	calldata_t params;
	calldata_init(&params);
	calldata_set_string(&params, "json", json.c_str());
	if (!proc_handler_call(obs_get_proc_handler(), live::kProcStyle, &params)) {
		blog(LOG_WARNING, "[voltz] plugin voltz-sources ausente: estilo do mouse nao enviado");
	}
	calldata_free(&params);
}

#ifdef _WIN32
const wchar_t *kWindowClass = L"VoltzLiveMouse";

bool registerWindowClass()
{
	static bool registered = false;
	if (!registered) {
		WNDCLASSEXW windowClass = {};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = DefWindowProcW;
		windowClass.hInstance = GetModuleHandleW(nullptr);
		windowClass.lpszClassName = kWindowClass;
		registered = RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	}
	return registered;
}

// Botão que desceu neste pacote de Raw Input. O Raw Input fala em botões físicos: com SM_SWAPBUTTON o físico
// esquerdo é o botão secundário (direito).
MouseButton pressedButton(USHORT flags, bool swapped)
{
	if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) {
		return swapped ? MouseButton::Right : MouseButton::Left;
	}
	if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
		return swapped ? MouseButton::Left : MouseButton::Right;
	}
	if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
		return MouseButton::Middle;
	}
	return MouseButton::None;
}
#endif

} // namespace

LiveMouse::LiveMouse(MouseStyleStore *styles, QObject *parent) : QObject(parent), styles_(styles)
{
	testMode_ = live::testModeEnabled();
	if (testMode_) {
		blog(LOG_WARNING, "[voltz] mouse ao vivo: modo de teste (%s), o mouse real nao e lido", live::kTestEnv);
	}
	connect(styles_, &MouseStyleStore::styleChanged, this, [this]() { pushStyle(); });
	pushStyle();
	poll_.setInterval(kPollMs);
	connect(&poll_, &QTimer::timeout, this, [this]() { poll(); });
	poll_.start();
}

LiveMouse::~LiveMouse()
{
	shutdown();
}

void LiveMouse::shutdown()
{
	shutdown_ = true;
	poll_.stop();
	stop();
}

void LiveMouse::pushStyle()
{
	if (!shutdown_) {
		sendStyle(mouseStyleToJson(styles_->style()));
	}
}

void LiveMouse::poll()
{
	if (shutdown_) {
		return;
	}
	const int wanted = queryWanted();
	if (testMode_) {
		return;
	}
	if (wanted <= 0) {
		stop();
		return;
	}
	refreshMapping();
	if (!running_) {
		start();
	}
}

void LiveMouse::refreshMapping()
{
	CaptureMapping mapping;
	OBSSource screen = findSharedSource(SharedSource::Screen);
	if (screen && strcmp(obs_source_get_unversioned_id(screen), "monitor_capture") == 0) {
		OBSDataAutoRelease settings = obs_source_get_settings(screen);
		const MonitorDescription monitor = findMonitor(obs_data_get_string(settings, "monitor_id"));
		if (monitor.found) {
			mapping = CaptureMapping{monitor.rect, monitor.rect.width, monitor.rect.height};
		}
	}
	const int state = mapping.captureWidth > 0 ? 1 : 0;
	if (state != mappingState_) {
		mappingState_ = state;
		if (state == 1) {
			blog(LOG_INFO, "[voltz] mouse ao vivo: captura de monitor %dx%d", mapping.captureWidth,
			     mapping.captureHeight);
		} else {
			blog(LOG_INFO, "[voltz] mouse ao vivo: sem captura de monitor compartilhada (fora)");
		}
	}
	std::lock_guard<std::mutex> lock(mutex_);
	mapping_ = mapping;
}

void LiveMouse::start()
{
#ifdef _WIN32
	std::promise<void> ready;
	std::future<void> started = ready.get_future();
	thread_ = std::thread(&LiveMouse::threadMain, this, std::move(ready));
	started.wait();
	running_ = true;
#endif
}

void LiveMouse::stop()
{
#ifdef _WIN32
	if (!running_) {
		return;
	}
	running_ = false;
	/* WM_QUIT faz o laço da thread terminar; a fila já existe (criada antes de publicar o id). */
	const DWORD threadId = static_cast<DWORD>(threadId_.load());
	for (int attempt = 0; attempt < 200; attempt++) {
		if (PostThreadMessageW(threadId, WM_QUIT, 0, 0)) {
			break;
		}
		Sleep(5);
	}
	if (thread_.joinable()) {
		thread_.join();
	}
	sendPosition(CapturePoint{}); // some o halo
	blog(LOG_INFO, "[voltz] mouse ao vivo: desligado");
#endif
}

CapturePoint LiveMouse::currentPoint()
{
#ifdef _WIN32
	POINT cursor;
	if (!GetPhysicalCursorPos(&cursor)) {
		return CapturePoint{};
	}
	std::lock_guard<std::mutex> lock(mutex_);
	return mapToCapture(mapping_, cursor.x, cursor.y);
#else
	return CapturePoint{};
#endif
}

void LiveMouse::threadMain(std::promise<void> ready)
{
#ifdef _WIN32
	os_set_thread_name("voltz-mouse-live");
	MSG msg;
	/* cria a fila de mensagens desta thread antes de publicar o id (o PostThreadMessage do stop precisa dela) */
	PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
	threadId_.store(GetCurrentThreadId());
	HWND window = nullptr;
	if (registerWindowClass()) {
		window = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
					 GetModuleHandleW(nullptr), nullptr);
	}
	bool rawInput = false;
	if (window) {
		RAWINPUTDEVICE device = {};
		device.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
		device.usUsage = 0x02;     // HID_USAGE_GENERIC_MOUSE
		device.dwFlags = RIDEV_INPUTSINK;
		device.hwndTarget = window;
		rawInput = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
	}
	const bool swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
	blog(LOG_INFO, "[voltz] mouse ao vivo: ligado (cliques %s)", rawInput ? "por raw input" : "indisponiveis");
	ready.set_value();

	CapturePoint last;
	bool sent = false;
	int64_t lastSendNs = 0;
	bool quit = false;
	while (!quit) {
		MsgWaitForMultipleObjects(0, nullptr, FALSE, kTickMs, QS_ALLINPUT);
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				quit = true;
				break;
			}
			if (msg.message == WM_INPUT) {
				RAWINPUT raw;
				UINT size = sizeof(raw);
				if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg.lParam), RID_INPUT, &raw, &size,
						    sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1) &&
				    raw.header.dwType == RIM_TYPEMOUSE) {
					const MouseButton button = pressedButton(raw.data.mouse.usButtonFlags, swapped);
					if (button != MouseButton::None) {
						const CapturePoint point = currentPoint();
						if (point.inside) {
							sendClick(point, button, static_cast<int64_t>(os_gettime_ns()));
						}
					}
				}
			}
			DispatchMessageW(&msg); // WM_INPUT: o DefWindowProc libera o pacote
		}
		const int64_t now = static_cast<int64_t>(os_gettime_ns());
		if (quit || now - lastSendNs < kPositionIntervalNs) {
			continue;
		}
		lastSendNs = now;
		const CapturePoint point = currentPoint();
		if (!sent || point.inside != last.inside || point.x != last.x || point.y != last.y) {
			sendPosition(point);
			last = point;
			sent = true;
		}
	}
	if (rawInput) {
		RAWINPUTDEVICE device = {};
		device.usUsagePage = 0x01;
		device.usUsage = 0x02;
		device.dwFlags = RIDEV_REMOVE;
		device.hwndTarget = nullptr;
		RegisterRawInputDevices(&device, 1, sizeof(device));
	}
	if (window) {
		DestroyWindow(window);
	}
#else
	ready.set_value();
#endif
}

} // namespace voltz
```

- [ ] **Step 3: Ligar na casca, no CMake e nos locales**

Em `frontend/cmake/feature-voltz.cmake`, na lista do `target_sources(obs-studio …)`:
- depois de `voltz/project/IsoTrack.hpp`, acrescentar `voltz/project/LiveMouse.cpp` e `voltz/project/LiveMouse.hpp`;
- depois de `voltz/shell/Icons.hpp`, acrescentar `voltz/shell/MouseStyleStore.cpp` e `voltz/shell/MouseStyleStore.hpp`.

Em `frontend/voltz/shell/VoltzShell.hpp`, junto das declarações antecipadas (`class FrontendBridge;`), acrescentar
`class LiveMouse;` e `class MouseStyleStore;`; nos membros, depois de `ProjectRecorder *recorder_ = nullptr;`:
```cpp
	MouseStyleStore *mouseStyles_ = nullptr;
	LiveMouse *liveMouse_ = nullptr;
```

Em `frontend/voltz/shell/VoltzShell.cpp`:
1. Includes: depois de `#include "Icons.hpp"`, `#include "MouseStyleStore.hpp"`; depois de
   `#include <voltz/project/ProjectRecorder.hpp>`, `#include <voltz/project/LiveMouse.hpp>`.
2. Logo depois de `recorder_ = new ProjectRecorder(this);`:
```cpp
	mouseStyles_ = new MouseStyleStore(this);
	liveMouse_ = new LiveMouse(mouseStyles_, this);
```
3. No `connect(bridge_, &FrontendBridge::exiting, …)`, primeira linha do lambda:
```cpp
		/* antes de o libobs descarregar o plugin: nada de chamar os procedimentos dele depois daqui */
		liveMouse_->shutdown();
```

No fim de `frontend/data/locale/voltz-pt-BR.ini`:
```ini
Voltz.Hotkey.ToggleSpotlight="Voltz Studio: ligar/desligar holofote"
```
No fim de `frontend/data/locale/voltz-en-US.ini`:
```ini
Voltz.Hotkey.ToggleSpotlight="Voltz Studio: toggle spotlight"
```

- [ ] **Step 4: Compilar**

CONFIGURAR; COMPILAR APP (segundo plano). Paridade dos locales (comando das Regras fixas) sem saída.
Expected: `exit=0`.

- [ ] **Step 5: Teste ao vivo — estilo empurrado, atalho, liga/desliga do mouse ao vivo**

Bloco de teste (`$liveTest = $true`, `$standins = $false`). No `try`, depois do `Start-App`:
```powershell
# Execução 1: modo de teste (o mouse real nunca é lido). Prepara também as cenas da execução 2 (a Voltz Mouse só
# pode ser criada pelo pedido de teste AddMouseSource; ela fica salva na coleção).
[void](Ws ('[{"requestType":"CreateScene","requestData":{"sceneName":"Cena mouse"}},{"requestType":"CreateScene","requestData":{"sceneName":"Cena vazia"}},' +
  '{"requestType":"CreateInput","requestData":{"sceneName":"Cena mouse","inputName":"Tela substituta","inputKind":"color_source_v3","inputSettings":{"color":4281346602,"width":2560,"height":1440}}},' +
  '{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Cena vazia"}}]'))
"AddMouseSource: " + ((Vendor "AddMouseSource" '{"sceneName":"Cena mouse","targetName":"Tela substituta"}').ok)
"estilo no plugin: " + (Vendor "MouseState").style
$keys = (Ws '[{"requestType":"GetHotkeyList"}]' -Quiet)[0].data.hotkeys
"atalho registrado: " + (@($keys) -contains "VoltzStudio.ToggleSpotlight")
[void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.ToggleSpotlight"}}]'); Start-Sleep 1
"holofote apos atalho: " + (((Vendor "MouseState").style | ConvertFrom-Json).holofote.ativo)
Close-App; Save-Log
"user.ini MouseStyle: " + (Get-VoltzIni "MouseStyle")
LogLines '\[voltz\] (holofote|mouse ao vivo)'

# Execução 2: SEM VOLTZ_TEST_LIVE - o LiveMouse le a posicao do cursor (so le; nada e movido nem clicado) e
# registra Raw Input (so observa). A Tela e de cor, entao o mapeamento da "fora" e nada e desenhado.
Remove-Item Env:VOLTZ_TEST_LIVE
Start-App
[void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Cena mouse"}}]')
Start-Sleep 3
$v = Ws '[{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste","requestType":"MouseState","requestData":{}}}]' -Quiet
"fornecedor de teste ausente sem VOLTZ_TEST_LIVE: " + ($v.Count -ge 1 -and -not $v[0].ok)
[void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Cena vazia"}}]'); Start-Sleep 3
Close-App; Save-Log
LogLines '\[voltz\] mouse ao vivo|\[voltz-sources\]'
```
Critérios:
1. Execução 1: `AddMouseSource: True`; `estilo no plugin` com `"tamanho":200` e `"opacidade":1.0` (o estilo do `user.ini` foi empurrado);
   `atalho registrado: True`; `holofote apos atalho: True`; `user.ini MouseStyle` com `"holofote":{"ativo":true`; log com
   `[voltz] mouse ao vivo: modo de teste (VOLTZ_TEST_LIVE)` e `[voltz] holofote: ligado`, **sem** `mouse ao vivo: ligado`.
2. Execução 2: log com, nesta ordem, `[voltz] mouse ao vivo: sem captura de monitor compartilhada (fora)`,
   `[voltz] mouse ao vivo: ligado (cliques por raw input)` e, depois da troca para "Cena vazia",
   `[voltz] mouse ao vivo: desligado`; **sem** `[voltz-sources] modo de teste`; `fornecedor de teste ausente sem
   VOLTZ_TEST_LIVE: True`.
3. As duas execuções fecham sozinhas (sem `Stop-Process` no `Close-App`) e o log não tem `QObject::connect` novos.

- [ ] **Step 6: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/shell/MouseStyleStore.hpp frontend/voltz/shell/MouseStyleStore.cpp frontend/voltz/project/LiveMouse.hpp frontend/voltz/project/LiveMouse.cpp frontend/voltz/shell/VoltzShell.hpp frontend/voltz/shell/VoltzShell.cpp
git add frontend/voltz/shell/MouseStyleStore.hpp frontend/voltz/shell/MouseStyleStore.cpp frontend/voltz/project/LiveMouse.hpp frontend/voltz/project/LiveMouse.cpp frontend/voltz/shell/VoltzShell.hpp frontend/voltz/shell/VoltzShell.cpp frontend/cmake/feature-voltz.cmake frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): estilo do mouse no user.ini, atalho do holofote e mouse ao vivo para a fonte Voltz Mouse

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 6: Grupo "Tela Voltz" nos presets, migração das cenas existentes e fontes substitutas de teste

**Files:**
- Modify: `frontend/voltz/shell/SharedSources.hpp`, `frontend/voltz/shell/SharedSources.cpp`
- Modify: `frontend/voltz/shell/PresetBuilder.hpp`, `frontend/voltz/shell/PresetBuilder.cpp`
- Modify: `frontend/voltz/shell/FrontendBridge.hpp`, `frontend/voltz/shell/FrontendBridge.cpp`
- Modify: `frontend/voltz/shell/VoltzShell.cpp`, `frontend/voltz/shell/SourcesPanel.cpp`
- Create: `frontend/voltz/shell/icons/regular/cursor-click.svg`; Modify: `frontend/voltz/shell/voltz-shell.qrc`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`

- [ ] **Step 1: Fonte compartilhada "Voltz Mouse" e substitutas de teste**

Em `frontend/voltz/shell/SharedSources.hpp`, trocar o enum e o comentário do `ensureSharedSource` por:
```cpp
// Mouse = fonte "Voltz Mouse" (plugin voltz-sources) com o ajuste "alvo" = UUID da Tela; só existe com o plugin.
enum class SharedSource { Screen, Camera, Background, Mouse };
```
```cpp
// Devolve a fonte compartilhada, criando-a se preciso (Tela = monitor_capture do primeiro monitor; Câmera =
// dshow_input do primeiro dispositivo; Fundo = color_source_v3 #09090B do tamanho do canvas; Mouse = Voltz Mouse
// apontando para a Tela) e salvando o UUID. Só testes: com VOLTZ_TEST_PRESET_STANDINS, Tela e Câmera viram fontes de
// cor (2560×1440 #2A2A30 e 1280×720 #3A4A5A) — nenhuma captura de tela nem webcam é criada.
OBSSource ensureSharedSource(SharedSource which);
```

Em `frontend/voltz/shell/SharedSources.cpp`:
1. Depois de `#include <OBSApp.hpp>`, acrescentar `#include <voltz/core/live-protocol.hpp>`.
2. Em `configKey`, antes do `}` do `switch`, acrescentar:
```cpp
	case SharedSource::Mouse:
		return "MouseSourceUuid";
```
3. Trocar a função `createShared` inteira por:
```cpp
OBSSource createShared(SharedSource which)
{
	OBSDataAutoRelease settings = obs_data_create();
	const char *id = nullptr;
	QString name;
	/* Só testes: presets sem captura de monitor nem webcam reais (fontes de cor nos tamanhos típicos). */
	const bool standins = qEnvironmentVariableIsSet("VOLTZ_TEST_PRESET_STANDINS");

	switch (which) {
	case SharedSource::Screen:
		name = QTStr("Voltz.Source.Screen");
		if (standins) {
			id = "color_source_v3";
			obs_data_set_int(settings, "color", 0xFF302A2A); // ABGR de #2A2A30
			obs_data_set_int(settings, "width", 2560);
			obs_data_set_int(settings, "height", 1440);
		} else {
			id = "monitor_capture";
			obs_data_set_string(settings, "monitor_id", firstListValue(id, "monitor_id").c_str());
			obs_data_set_bool(settings, "capture_cursor", true);
		}
		break;
	case SharedSource::Camera:
		name = QTStr("Voltz.Source.Camera");
		if (standins) {
			id = "color_source_v3";
			obs_data_set_int(settings, "color", 0xFF5A4A3A); // ABGR de #3A4A5A
			obs_data_set_int(settings, "width", 1280);
			obs_data_set_int(settings, "height", 720);
		} else {
			id = "dshow_input";
			obs_data_set_string(settings, "video_device_id", firstListValue(id, "video_device_id").c_str());
		}
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
	case SharedSource::Mouse: {
		if (!obs_source_get_display_name(live::kMouseSourceId)) {
			blog(LOG_WARNING, "[voltz] plugin voltz-sources ausente: sem a fonte Voltz Mouse");
			return {};
		}
		id = live::kMouseSourceId;
		name = QTStr("Voltz.Source.Mouse");
		OBSSource screen = findSharedSource(SharedSource::Screen);
		obs_data_set_string(settings, live::kTargetSetting, screen ? obs_source_get_uuid(screen) : "");
		break;
	}
	}

	/* Nome já usado por outra fonte: reaproveita só se for do mesmo tipo; senão cria com sufixo. */
	const char *unversionedId = strcmp(id, "color_source_v3") == 0 ? "color_source" : id;
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
```

- [ ] **Step 2: "Tela Voltz" nos presets e migração**

Em `frontend/voltz/shell/PresetBuilder.hpp`, depois da declaração de `applyPreset`:
```cpp
// Tela Voltz (spec §4.4) nas cenas que já existiam: na primeira vez em cada coleção, cada item DIRETO da Tela
// compartilhada vira um grupo "Tela Voltz" com a camada Voltz Mouse (o grupo herda transformação, ordem, visibilidade e
// trava). Marca a Tela (voltz_tela_migrada, nas configurações privadas salvas na coleção) para não repetir — quem
// desagrupar não vê o grupo voltar. Acerta também o "alvo" da Voltz Mouse. Sem o plugin voltz-sources, adia. Thread
// da UI.
void ensureScreenGroups();
```

Em `frontend/voltz/shell/PresetBuilder.cpp`:
1. Includes: depois de `#include <voltz/core/preset-layout.hpp>`, `#include <voltz/core/live-protocol.hpp>`; depois de
   `#include <QString>`, acrescentar:
```cpp
#include <cstring>
#include <utility>
#include <vector>
```
2. No `namespace {`, logo depois da função `addFull(...)`, acrescentar:
```cpp
// Configurações privadas (salvas na coleção): na Tela, se a migração já rodou; no grupo, a versão das camadas
// (1 = Voltz Mouse; o Plano 4B sobe para 2 ao acrescentar a Voltz Anotações).
constexpr const char *kScreenMigratedKey = "voltz_tela_migrada";
constexpr const char *kScreenLayersKey = "voltz_tela_camadas";
constexpr int kScreenLayersVersion = 1;

// Nome livre a partir de `base` ("Tela Voltz", "Tela Voltz 2", …), no formato da deduplicação do libobs.
QString uniqueSourceName(const QString &base)
{
	QString name = base;
	for (int suffix = 2;; suffix++) {
		OBSSourceAutoRelease existing = obs_get_source_by_name(name.toUtf8().constData());
		if (!existing) {
			return name;
		}
		name = QStringLiteral("%1 %2").arg(base).arg(suffix);
	}
}

// Camada dentro da Tela Voltz: no canto (0,0), escala 1, sem caixa (quem escala e posiciona é o grupo).
void placeLayer(obs_sceneitem_t *item)
{
	obs_transform_info info = {};
	vec2_set(&info.pos, 0.0f, 0.0f);
	info.rot = 0.0f;
	vec2_set(&info.scale, 1.0f, 1.0f);
	info.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	info.bounds_type = OBS_BOUNDS_NONE;
	info.bounds_alignment = OBS_ALIGN_CENTER;
	vec2_set(&info.bounds, 0.0f, 0.0f);
	info.crop_to_bounds = false;
	obs_sceneitem_set_info2(item, &info);
}

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

// Põe `screenItem` (item direto da cena) num grupo "Tela Voltz" com a Voltz Mouse por cima. O grupo assume a
// transformação, a ordem, a visibilidade e a trava do item; as camadas ficam em (0,0), escala 1, travadas e com o
// mesmo recorte — o pixel (x,y) da captura cai no mesmo lugar nas duas. Devolve o item do grupo (null se não deu: sem
// plugin ou o libobs recusou).
obs_sceneitem_t *wrapScreenItem(obs_scene_t *scene, obs_sceneitem_t *screenItem)
{
	if (!scene || !screenItem) {
		return nullptr;
	}
	OBSSource mouse = ensureSharedSource(SharedSource::Mouse);
	if (!mouse) {
		return nullptr;
	}
	syncMouseTarget(mouse, obs_sceneitem_get_source(screenItem));

	obs_transform_info info = {};
	obs_sceneitem_get_info2(screenItem, &info);
	obs_sceneitem_crop crop = {};
	obs_sceneitem_get_crop(screenItem, &crop);
	const int order = obs_sceneitem_get_order_position(screenItem);
	const bool visible = obs_sceneitem_visible(screenItem);
	const bool locked = obs_sceneitem_locked(screenItem);

	obs_sceneitem_t *mouseItem = obs_scene_add(scene, mouse);
	if (!mouseItem) {
		return nullptr;
	}
	obs_sceneitem_set_visible(screenItem, true);
	placeLayer(screenItem);
	placeLayer(mouseItem);
	obs_sceneitem_set_crop(mouseItem, &crop);

	obs_sceneitem_t *items[] = {screenItem, mouseItem};
	const QString name = uniqueSourceName(QTStr("Voltz.Source.ScreenGroup"));
	obs_sceneitem_t *group = obs_scene_insert_group(scene, name.toUtf8().constData(), items, 2);
	if (!group) {
		obs_sceneitem_remove(mouseItem);
		obs_sceneitem_set_info2(screenItem, &info);
		obs_sceneitem_set_visible(screenItem, visible);
		blog(LOG_WARNING, "[voltz] tela voltz: o libobs recusou o grupo; Tela mantida como estava");
		return nullptr;
	}
	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(obs_sceneitem_get_source(group));
	obs_data_set_int(privateSettings, kScreenLayersKey, kScreenLayersVersion);
	obs_sceneitem_set_locked(screenItem, true);
	obs_sceneitem_set_locked(mouseItem, true);
	obs_sceneitem_set_info2(group, &info);
	obs_sceneitem_set_order_position(group, order);
	obs_sceneitem_set_visible(group, visible);
	obs_sceneitem_set_locked(group, locked);
	return group;
}

// Cenas de preset já nascem com a Tela Voltz: a migração não tem o que fazer nesta coleção.
void markScreenMigrated()
{
	OBSSource screen = findSharedSource(SharedSource::Screen);
	if (!screen || !obs_source_get_display_name(live::kMouseSourceId)) {
		return;
	}
	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(screen);
	obs_data_set_bool(privateSettings, kScreenMigratedKey, true);
}
```
3. Em `buildTutorial` **e** em `buildScreenOnly` (o trecho é igual nas duas; troque as duas ocorrências):
```cpp
		if (OBSSource screen = ensureSharedSource(SharedSource::Screen)) {
			addFull(scene, screen, canvas, OBS_BOUNDS_SCALE_INNER, false);
		}
```
por:
```cpp
		if (OBSSource screen = ensureSharedSource(SharedSource::Screen)) {
			wrapScreenItem(scene, addFull(scene, screen, canvas, OBS_BOUNDS_SCALE_INNER, false));
		}
```
4. Em `buildSideBySide`, trocar:
```cpp
			if (obs_sceneitem_t *item = obs_scene_add(scene, screen)) {
				placeItem(item, sideBySideScreen(canvas), OBS_BOUNDS_SCALE_INNER, false);
			}
```
por:
```cpp
			if (obs_sceneitem_t *item = obs_scene_add(scene, screen)) {
				placeItem(item, sideBySideScreen(canvas), OBS_BOUNDS_SCALE_INNER, false);
				wrapScreenItem(scene, item);
			}
```
5. Em `applyPreset`, trocar `obs_frontend_save();` por:
```cpp
	markScreenMigrated();
	obs_frontend_save();
```
6. No fim do arquivo, antes do `} // namespace voltz`, acrescentar:
```cpp
void ensureScreenGroups()
{
	OBSSource screen = findSharedSource(SharedSource::Screen);
	if (!screen) {
		return;
	}
	if (OBSSource mouse = findSharedSource(SharedSource::Mouse)) {
		syncMouseTarget(mouse, screen);
	}
	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(screen);
	if (obs_data_get_bool(privateSettings, kScreenMigratedKey)) {
		return;
	}
	if (!obs_source_get_display_name(live::kMouseSourceId)) {
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
				if (!obs_sceneitem_is_group(item) && obs_sceneitem_get_source(item) == search->screen) {
					search->items.emplace_back(OBSScene(search->scene), OBSSceneItem(item));
				}
				return true;
			},
			&found);
	}
	obs_frontend_source_list_free(&scenes);

	int wrapped = 0;
	for (auto &entry : found.items) {
		if (wrapScreenItem(entry.first, entry.second)) {
			wrapped++;
		}
	}
	obs_data_set_bool(privateSettings, kScreenMigratedKey, true);
	blog(LOG_INFO, "[voltz] tela voltz: %d item(ns) migrado(s)", wrapped);
	if (wrapped > 0) {
		obs_frontend_save();
	}
}
```

- [ ] **Step 3: Rodar a migração na abertura e na troca de coleção**

Em `frontend/voltz/shell/FrontendBridge.hpp`, nos `signals:`, depois de `void sceneListChanged();`:
```cpp
	// Coleção de cenas trocada (já carregada).
	void sceneCollectionChanged();
```
Em `frontend/voltz/shell/FrontendBridge.cpp`, no `dispatch`, trocar:
```cpp
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		emit sceneListChanged();
		emit currentSceneChanged(currentSceneUuid());
		break;
```
por:
```cpp
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		emit sceneCollectionChanged();
		emit sceneListChanged();
		emit currentSceneChanged(currentSceneUuid());
		break;
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		emit sceneListChanged();
		emit currentSceneChanged(currentSceneUuid());
		break;
```
Em `frontend/voltz/shell/VoltzShell.cpp`, depois de `#include "MouseStyleStore.hpp"`, acrescentar
`#include "PresetBuilder.hpp"`; logo depois de `liveMouse_ = new LiveMouse(mouseStyles_, this);`:
```cpp
	/* A coleção já está carregada (a casca nasce depois do OBSBasic::OBSInit): Tela Voltz nas cenas de antes. */
	ensureScreenGroups();
	connect(bridge_, &FrontendBridge::sceneCollectionChanged, this, []() { ensureScreenGroups(); });
```

- [ ] **Step 4: Ícone da Voltz Mouse na lista de Fontes**

Baixar o SVG oficial (versão exata) e conferir:
```bash
curl -fsSL "https://unpkg.com/@phosphor-icons/core@2.1.1/assets/regular/cursor-click.svg" -o frontend/voltz/shell/icons/regular/cursor-click.svg
head -c 80 frontend/voltz/shell/icons/regular/cursor-click.svg; echo
```
Expected: começa com `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" fill="currentColor">`.
Em `frontend/voltz/shell/voltz-shell.qrc`, depois de `<file>icons/regular/check.svg</file>`, acrescentar
`<file>icons/regular/cursor-click.svg</file>`. Conferir: `grep -c "<file>" frontend/voltz/shell/voltz-shell.qrc` →
`83`.

Em `frontend/voltz/shell/SourcesPanel.cpp`: includes `#include <voltz/core/live-protocol.hpp>` (depois de
`#include "Icons.hpp"`) e `#include <cstring>` (antes de `#include <map>`); trocar:
```cpp
	const QIcon sourceIcon = row.isGroup ? phosphorIcon(QStringLiteral("stack"), palette::kMuted)
					     : OBSBasic::Get()->GetSourceIcon(obs_source_get_id(source));
```
por:
```cpp
	const char *sourceId = obs_source_get_id(source);
	const QIcon sourceIcon = row.isGroup ? phosphorIcon(QStringLiteral("stack"), palette::kMuted)
				 : strcmp(sourceId, live::kMouseSourceId) == 0
					 ? phosphorIcon(QStringLiteral("cursor-click"), palette::kMuted)
					 : OBSBasic::Get()->GetSourceIcon(sourceId);
```

No fim de `frontend/data/locale/voltz-pt-BR.ini`:
```ini
Voltz.Source.Mouse="Voltz Mouse"
Voltz.Source.ScreenGroup="Tela Voltz"
```
No fim de `frontend/data/locale/voltz-en-US.ini`:
```ini
Voltz.Source.Mouse="Voltz Mouse"
Voltz.Source.ScreenGroup="Voltz Screen"
```

- [ ] **Step 5: Compilar**

CONFIGURAR (ícone novo no `.qrc`); COMPILAR APP (segundo plano); paridade dos locales sem saída.
Expected: `exit=0`.

- [ ] **Step 6: Teste ao vivo — presets com substitutas, grupos, render e migração**

Bloco de teste salvo **com BOM** (`$liveTest = $true`, **`$standins = $true`**). No `try`, depois do `Start-App`:
```powershell
# Fase A: presets (Tela e Câmera de COR - nenhuma captura real)
if (-not (Uia "Só tela / Só câmera — Aplicar")) { throw "preset Só tela nao aplicado" }
Start-Sleep 3
Assert-NoRealCapture
$r = Ws '[{"requestType":"GetSceneItemList","requestData":{"sceneName":"Só tela"}},{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":"Tela Voltz"}},{"requestType":"GetSceneItemList","requestData":{"sceneName":"Só câmera"}}]' -Quiet
"Só tela: " + ((@($r[0].data.sceneItems) | ForEach-Object { "$($_.sceneItemIndex):$($_.sourceName) grupo=$($_.isGroup) caixa=$($_.sceneItemTransform.boundsType) $($_.sceneItemTransform.boundsWidth)x$($_.sceneItemTransform.boundsHeight)" }) -join ' | ')
"Tela Voltz: " + ((@($r[1].data.sceneItems) | ForEach-Object { "$($_.sceneItemIndex):$($_.sourceName) ($($_.inputKind)) travado=$($_.sceneItemLocked) pos=$($_.sceneItemTransform.positionX),$($_.sceneItemTransform.positionY) escala=$($_.sceneItemTransform.scaleX)" }) -join ' | ')
"Só câmera: " + ((@($r[2].data.sceneItems) | ForEach-Object { "$($_.sourceName) grupo=$($_.isGroup)" }) -join ' | ')
[void](Uia "Presets"); Start-Sleep 1
if (-not (Uia "Tutorial — Aplicar")) { throw "preset Tutorial nao aplicado" }; Start-Sleep 3
[void](Uia "Presets"); Start-Sleep 1
if (-not (Uia "Lado a lado — Aplicar")) { throw "preset Lado a lado nao aplicado" }; Start-Sleep 3
$r = Ws '[{"requestType":"GetSceneItemList","requestData":{"sceneName":"Tutorial"}},{"requestType":"GetSceneItemList","requestData":{"sceneName":"Lado a lado"}}]' -Quiet
"Tutorial: " + ((@($r[0].data.sceneItems) | ForEach-Object { "$($_.sceneItemIndex):$($_.sourceName) grupo=$($_.isGroup)" }) -join ' | ')
"Lado a lado: " + ((@($r[1].data.sceneItems) | ForEach-Object { "$($_.sceneItemIndex):$($_.sourceName) grupo=$($_.isGroup) x=$($_.sceneItemTransform.positionX) caixa=$($_.sceneItemTransform.boundsWidth)x$($_.sceneItemTransform.boundsHeight)" }) -join ' | ')
"fontes Voltz Mouse: " + ((@((Ws '[{"requestType":"GetInputList"}]' -Quiet)[0].data.inputs) | Where-Object { $_.inputKind -eq "voltz_mouse_source" } | ForEach-Object { $_.inputName }) -join ', ')
[void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Só tela"}}]'); Start-Sleep 1
"estado: " + ((Vendor "MouseState") | ConvertTo-Json -Compress)
# Tela 2560x1440 escalada para 1920x1080 (0,75): pixel (1280,720) da captura = centro da imagem
[void](Vendor "InjectMouse" '{"x":1280,"y":720}'); Start-Sleep -Milliseconds 300
$img = Scene-Image "Só tela"; $c = $img.GetPixel(320, 180); $k = $img.GetPixel(40, 40); $img.Save("$sp\t6-so-tela.png"); $img.Dispose()
"Só tela: centro=$c lima=$(Near $c 212 245 60) | canto=$k tela=$(Near $k 42 42 48)"
Shot $shell "t6-gravar"
Close-App; Save-Log
"user.ini MouseSourceUuid: " + (Get-VoltzIni "MouseSourceUuid")
LogLines '\[voltz\] (tela voltz|preset aplicado|plugin voltz-sources)|duplicate name'

# Fase B: coleção "de antes do 4A" (Tela direto na cena) - migração na abertura
Remove-Item Env:VOLTZ_TEST_PRESET_STANDINS
New-CleanConfig
Start-App
[void](Ws ('[{"requestType":"CreateScene","requestData":{"sceneName":"Antiga"}},' +
  '{"requestType":"CreateInput","requestData":{"sceneName":"Antiga","inputName":"Tela","inputKind":"color_source_v3","inputSettings":{"color":4281346602,"width":2560,"height":1440}}},' +
  '{"requestType":"CreateInput","requestData":{"sceneName":"Antiga","inputName":"Logo","inputKind":"color_source_v3","inputSettings":{"color":4278255360,"width":200,"height":200}}},' +
  '{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Antiga"}}]'))
$telaId = (Ws '[{"requestType":"GetSceneItemId","requestData":{"sceneName":"Antiga","sourceName":"Tela"}}]')[0].data.sceneItemId
[void](Ws ('[{"requestType":"SetSceneItemTransform","requestData":{"sceneName":"Antiga","sceneItemId":' + $telaId + ',"sceneItemTransform":{"positionX":100,"positionY":50,"boundsType":"OBS_BOUNDS_SCALE_INNER","boundsWidth":1600,"boundsHeight":900}}},' +
  '{"requestType":"SetSceneItemLocked","requestData":{"sceneName":"Antiga","sceneItemId":' + $telaId + ',"sceneItemLocked":true}}]'))
$uuid = (@((Ws '[{"requestType":"GetInputList"}]' -Quiet)[0].data.inputs) | Where-Object { $_.inputName -eq "Tela" }).inputUuid
Close-App
Set-VoltzIni "ScreenSourceUuid" $uuid
Start-App
$r = Ws '[{"requestType":"GetSceneItemList","requestData":{"sceneName":"Antiga"}},{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":"Tela Voltz"}}]' -Quiet
"Antiga: " + ((@($r[0].data.sceneItems) | ForEach-Object { "$($_.sceneItemIndex):$($_.sourceName) grupo=$($_.isGroup) travado=$($_.sceneItemLocked) pos=$($_.sceneItemTransform.positionX),$($_.sceneItemTransform.positionY) caixa=$($_.sceneItemTransform.boundsWidth)x$($_.sceneItemTransform.boundsHeight)" }) -join ' | ')
"Tela Voltz: " + ((@($r[1].data.sceneItems) | ForEach-Object { "$($_.sceneItemIndex):$($_.sourceName)" }) -join ' | ')
# grupo em (100,50) com escala 1600/2560 = 0,625: pixel (1280,720) da captura = (900,500) no canvas = (300,167) na imagem
[void](Vendor "InjectMouse" '{"x":1280,"y":720}'); Start-Sleep -Milliseconds 300
$img = Scene-Image "Antiga"; $p = $img.GetPixel(300, 167); $img.Save("$sp\t6-antiga.png"); $img.Dispose()
"Antiga: halo em (300,167)=$p lima=$(Near $p 212 245 60)"
Close-App; Save-Log; LogLines '\[voltz\] tela voltz'
Start-App
$r = Ws '[{"requestType":"GetSceneItemList","requestData":{"sceneName":"Antiga"}},{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":"Tela Voltz"}}]' -Quiet
"Antiga (2a abertura): " + ((@($r[0].data.sceneItems) | ForEach-Object { "$($_.sceneItemIndex):$($_.sourceName)" }) -join ' | ') + " / grupo: " + ((@($r[1].data.sceneItems) | ForEach-Object { $_.sourceName }) -join ', ')
Close-App; Save-Log
"2a abertura, linhas de migracao: " + @(LogLines '\[voltz\] tela voltz').Count
```
Critérios:
1. Fase A — `Só tela: 0:Tela Voltz grupo=True caixa=OBS_BOUNDS_SCALE_INNER 1920x1080` (um item só);
   `Tela Voltz: 0:Tela (color_source_v3) travado=True pos=0,0 escala=1 | 1:Voltz Mouse (voltz_mouse_source) travado=True pos=0,0 escala=1`;
   `Só câmera: Câmera grupo=False`.
2. `Tutorial: 0:Tela Voltz 2 grupo=True | 1:Câmera redonda grupo=True` (a tela continua **embaixo** da câmera);
   `Lado a lado: 0:Fundo grupo=False | 1:Tela Voltz 3 grupo=True x=32 caixa=1296x1016 | 2:Câmera grupo=False` (mesmo
   lugar e caixa que a Tela tinha: `sideBySideScreen` de 1920×1080).
3. `fontes Voltz Mouse: Voltz Mouse` (uma só, compartilhada); `estado` com `showing` ≥ 1; `Só tela: … lima=True | …
   tela=True`; `user.ini MouseSourceUuid` preenchido; log **sem** `duplicate name` e sem `plugin voltz-sources ausente`.
4. `t6-gravar`: tela Gravar com a lista de Fontes mostrando o grupo "Tela Voltz" (ícone de pilha) com os filhos "Tela"
   e "Voltz Mouse" (ícone de cursor com clique), ambos com cadeado fechado. Descreva.
5. Fase B — `Antiga: 0:Tela Voltz grupo=True travado=True pos=100,50 caixa=1600x900 | 1:Logo grupo=False travado=False`;
   `Tela Voltz: 0:Tela | 1:Voltz Mouse`; `Antiga: halo … lima=True`; log com `[voltz] tela voltz: 1 item(ns) migrado(s)`.
6. Segunda abertura: `Antiga (2a abertura): 0:Tela Voltz | 1:Logo / grupo: Tela, Voltz Mouse` e
   `2a abertura, linhas de migracao: 0` (não migra de novo).
7. Abra `t6-so-tela.png` e `t6-antiga.png` (sintéticas) e descreva: tela cinza com halo lima no centro; na segunda, o
   retângulo da Tela deslocado/encolhido, o quadrado verde "Logo" no canto e o halo no centro do retângulo.

- [ ] **Step 7: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/shell/SharedSources.hpp frontend/voltz/shell/SharedSources.cpp frontend/voltz/shell/PresetBuilder.hpp frontend/voltz/shell/PresetBuilder.cpp frontend/voltz/shell/FrontendBridge.hpp frontend/voltz/shell/FrontendBridge.cpp frontend/voltz/shell/VoltzShell.cpp frontend/voltz/shell/SourcesPanel.cpp
git add frontend/voltz/shell/SharedSources.hpp frontend/voltz/shell/SharedSources.cpp frontend/voltz/shell/PresetBuilder.hpp frontend/voltz/shell/PresetBuilder.cpp frontend/voltz/shell/FrontendBridge.hpp frontend/voltz/shell/FrontendBridge.cpp frontend/voltz/shell/VoltzShell.cpp frontend/voltz/shell/SourcesPanel.cpp frontend/voltz/shell/icons/regular/cursor-click.svg frontend/voltz/shell/voltz-shell.qrc frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): grupo 'Tela Voltz' com a Voltz Mouse nos presets e migracao das cenas existentes

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 7: `ProjectRecorder` — `mouse` no manifesto, eventos do holofote e fonte do ISO no log

**Files:**
- Modify: `frontend/voltz/project/ProjectRecorder.hpp`, `frontend/voltz/project/ProjectRecorder.cpp`,
  `frontend/voltz/shell/VoltzShell.cpp`

- [ ] **Step 1: Ligar o estilo ao projeto**

Em `frontend/voltz/project/ProjectRecorder.hpp`:
1. Depois de `class CursorTracker;`, acrescentar `class MouseStyleStore;`.
2. Na parte `public:`, depois de `int runningIsoCount() const;`:
```cpp
	// Estilo do destaque do mouse: vai para o manifesto no início do projeto (spec §6.1) e o holofote ligado/desligado
	// durante a gravação vira evento. Chamar uma vez, antes de gravar.
	void setMouseStyleStore(MouseStyleStore *store);
```
3. Na parte `private:`, depois de `void checkIsoLoad();`, acrescentar `void onSpotlightToggled(bool active);`; nos
   membros, depois de `IsoOverloadMonitor overload_; …`, acrescentar `MouseStyleStore *mouseStyles_ = nullptr;`.

Em `frontend/voltz/project/ProjectRecorder.cpp`:
1. Depois de `#include <voltz/shell/SharedSources.hpp>`, acrescentar `#include <voltz/shell/MouseStyleStore.hpp>`.
2. Em `onStarting()`, logo depois de `manifest_.program.audioTracks = RecordingConfig::recordingAudioTracks();`:
```cpp
	if (mouseStyles_) {
		/* spec §6.1: o estilo usado vai para o manifesto (o editor pode trocar depois) */
		manifest_.hasMouse = true;
		manifest_.mouse = mouseStyles_->style();
	}
```
3. Em `startIsoTrack`, trocar:
```cpp
	blog(LOG_INFO, "[voltz] iso %s: %dx%d @ %.3f fps, %s, offset %lld ms", trackName, entry.width, entry.height,
	     entry.fps, entry.encoder.c_str(), static_cast<long long>(entry.offsetMs));
```
por:
```cpp
	/* nome e tipo da fonte: a prova de que o ISO grava a captura, não o grupo "Tela Voltz" (spec §11, risco 3) */
	blog(LOG_INFO, "[voltz] iso %s: fonte '%s' (%s), %dx%d @ %.3f fps, %s, offset %lld ms", trackName,
	     obs_source_get_name(source), obs_source_get_unversioned_id(source), entry.width, entry.height, entry.fps,
	     entry.encoder.c_str(), static_cast<long long>(entry.offsetMs));
```
4. No fim do arquivo, antes do `} // namespace voltz`:
```cpp
void ProjectRecorder::setMouseStyleStore(MouseStyleStore *store)
{
	mouseStyles_ = store;
	if (store) {
		connect(store, &MouseStyleStore::spotlightToggled, this,
			[this](bool active) { onSpotlightToggled(active); });
	}
}

void ProjectRecorder::onSpotlightToggled(bool active)
{
	if (!active_ || !timeline_.started()) {
		return;
	}
	const char *type = active ? "holofote_ligado" : "holofote_desligado";
	manifest_.events.push_back(ManifestEvent{timeline_.relativeMs(nowNs()), type, "", 0});
	writeManifest();
	blog(LOG_INFO, "[voltz] evento %s no projeto", type);
}
```

Em `frontend/voltz/shell/VoltzShell.cpp`, logo depois de `mouseStyles_ = new MouseStyleStore(this);`:
```cpp
	recorder_->setMouseStyleStore(mouseStyles_);
```

- [ ] **Step 2: Compilar**

COMPILAR APP (segundo plano). Expected: `exit=0`.

- [ ] **Step 3: Teste ao vivo — gravação com a Tela Voltz (substitutas)**

Bloco de teste salvo **com BOM** (`$liveTest = $true`, **`$standins = $true`**). No `try`, depois do `Start-App`:
```powershell
if (-not (Uia "Só tela / Só câmera — Aplicar")) { throw "preset nao aplicado" }
Start-Sleep 3
Assert-NoRealCapture
# Preparo de gravação (bloco acima)
[void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Só tela"}}]')
[void](Vendor "InjectMouse" '{"x":1280,"y":720}')
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 3
[void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.ToggleSpotlight"}}]'); Start-Sleep 2
[void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.ToggleSpotlight"}}]'); Start-Sleep 2
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 5
Close-App; Save-Log
foreach ($d in Project-Dirs) { [void](Show-Project $d) }
LogLines '\[voltz\] (iso tela|iso camera|evento holofote|holofote|projeto finalizado)'
```
Critérios:
1. Um projeto, `status=ok`; `mouse:` com `"tamanho":200`, `"opacidade":1.0`, `"holofote":{"ativo":false,…}` (o estilo do
   início); `eventos` com `holofote_ligado` e depois `holofote_desligado`, `t_ms` crescentes entre 2000 e 9000, e sem
   `iso_falhou`.
2. `arquivos.tela` 2560×1440 e `arquivos.camera` 1280×720; `tela.mkv : video/… 2560x1440 | audio/aac` (só
   metadados).
3. Log com `[voltz] iso tela: fonte 'Tela' (color_source), 2560x1440` — o ISO grava a captura que está **dentro** do
   grupo "Tela Voltz", não o grupo — e `[voltz] iso camera: fonte 'Câmera' (color_source), 1280x720`; `[voltz] evento
   holofote_ligado no projeto` e `… holofote_desligado …`.

- [ ] **Step 4: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/project/ProjectRecorder.hpp frontend/voltz/project/ProjectRecorder.cpp frontend/voltz/shell/VoltzShell.cpp
git add frontend/voltz/project/ProjectRecorder.hpp frontend/voltz/project/ProjectRecorder.cpp frontend/voltz/shell/VoltzShell.cpp
git commit -m "feat(voltz): estilo do mouse no projeto.voltz, eventos do holofote e fonte do ISO no log

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Casca — Ajustes › Mouse com prévia ao vivo e estilos salvos

**Files:**
- Create: `frontend/voltz/shell/MousePreview.hpp`, `frontend/voltz/shell/MousePreview.cpp`
- Create: `frontend/voltz/shell/MouseSettings.hpp`, `frontend/voltz/shell/MouseSettings.cpp`
- Modify: `frontend/voltz/shell/SettingsPage.hpp`, `frontend/voltz/shell/SettingsPage.cpp`,
  `frontend/voltz/shell/VoltzShell.cpp`, `frontend/voltz/shell/voltz-shell.qss`, `frontend/cmake/feature-voltz.cmake`,
  `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`

- [ ] **Step 1: Prévia ao vivo**

`frontend/voltz/shell/MousePreview.hpp`:
```cpp
#pragma once

#include <voltz/core/mouse-effects.hpp>
#include <voltz/core/mouse-style.hpp>

#include <QElapsedTimer>
#include <QPointF>
#include <QTimer>
#include <QWidget>

#include <vector>

namespace voltz {

// Prévia ao vivo de Ajustes › Mouse: um "desktop" de mentira onde o halo segue o mouse quando ele está sobre a prévia
// (senão fica no centro), clicar faz a onda (cor própria no botão direito) e o holofote escurece o resto. Usa as contas
// do núcleo (mouse-effects) na escala 1 px da prévia = 3 px da captura. Só desenha; não lê o mouse fora dela.
class MousePreview : public QWidget {
public:
	explicit MousePreview(QWidget *parent = nullptr);

	void setMouseStyle(const MouseStyle &style);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void leaveEvent(QEvent *event) override;

private:
	double scale() const;
	QPointF cursorPosition() const;
	void tick();

	MouseStyle style_;
	QPointF cursor_;
	bool hovering_ = false;
	std::vector<Ripple> ripples_; // x/y em pixels da prévia
	QElapsedTimer clock_;
	QTimer animation_;
};

} // namespace voltz
```

`frontend/voltz/shell/MousePreview.cpp`:
```cpp
#include "MousePreview.hpp"

#include <OBSApp.hpp> // QTStr

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QRadialGradient>

#include <algorithm>
#include <cstdint>

namespace voltz {

namespace {

constexpr double kPreviewCaptureHeight = 540.0; // a prévia mostra 540 px de altura da captura (1 px = 3 px)

QColor toQColor(const RgbColor &color, double alpha)
{
	QColor result(color.r, color.g, color.b);
	result.setAlphaF(static_cast<float>(std::clamp(alpha, 0.0, 1.0)));
	return result;
}

// Anel: traço centrado entre o raio interno e o externo.
void drawRing(QPainter &painter, const QPointF &center, double outer, double inner, const QColor &color)
{
	const double thickness = std::max(1.0, outer - inner);
	painter.setPen(QPen(color, thickness));
	painter.setBrush(Qt::NoBrush);
	const double radius = outer - thickness / 2.0;
	painter.drawEllipse(center, radius, radius);
}

} // namespace

MousePreview::MousePreview(QWidget *parent) : QWidget(parent)
{
	setObjectName("mousePreview");
	setAccessibleName(QTStr("Voltz.Mouse.Preview"));
	setFixedSize(320, 180);
	setMouseTracking(true);
	clock_.start();
	animation_.setInterval(16);
	connect(&animation_, &QTimer::timeout, this, [this]() { tick(); });
}

void MousePreview::setMouseStyle(const MouseStyle &style)
{
	style_ = style;
	update();
}

double MousePreview::scale() const
{
	return static_cast<double>(height()) / kPreviewCaptureHeight;
}

QPointF MousePreview::cursorPosition() const
{
	return hovering_ ? cursor_ : QPointF(width() / 2.0, height() / 2.0);
}

void MousePreview::tick()
{
	pruneRipples(ripples_, clock_.nsecsElapsed());
	if (ripples_.empty()) {
		animation_.stop();
	}
	update();
}

void MousePreview::mouseMoveEvent(QMouseEvent *event)
{
	cursor_ = event->position();
	hovering_ = true;
	update();
}

void MousePreview::mousePressEvent(QMouseEvent *event)
{
	const MouseButton button = event->button() == Qt::RightButton    ? MouseButton::Right
				   : event->button() == Qt::MiddleButton ? MouseButton::Middle
									 : MouseButton::Left;
	const QPointF at = event->position();
	pushRipple(ripples_, Ripple{static_cast<int>(at.x()), static_cast<int>(at.y()), button, clock_.nsecsElapsed()});
	animation_.start();
	update();
}

void MousePreview::leaveEvent(QEvent *event)
{
	QWidget::leaveEvent(event);
	hovering_ = false;
	update();
}

void MousePreview::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QRectF area = rect();
	/* "desktop" de mentira: fundo e duas janelas, para o holofote ter o que escurecer */
	painter.fillRect(area, QColor(0x18, 0x18, 0x1C));
	painter.fillRect(QRectF(area.width() * 0.07, area.height() * 0.12, area.width() * 0.52, area.height() * 0.66),
			 QColor(0x26, 0x26, 0x2C));
	painter.fillRect(QRectF(area.width() * 0.64, area.height() * 0.28, area.width() * 0.29, area.height() * 0.5),
			 QColor(0x2E, 0x2E, 0x35));

	const double s = scale();
	const QPointF cursor = cursorPosition();
	if (style_.spotlight.active) {
		const SpotlightGeometry spot = spotlightGeometry(style_.spotlight);
		const double outer = (spot.radius + spot.feather / 2.0) * s;
		const double inner = std::max(0.0, (spot.radius - spot.feather / 2.0) * s);
		QRadialGradient gradient(cursor, outer);
		gradient.setColorAt(0.0, QColor(0, 0, 0, 0));
		gradient.setColorAt(outer > 0.0 ? inner / outer : 0.0, QColor(0, 0, 0, 0));
		gradient.setColorAt(1.0, QColor(0, 0, 0, static_cast<int>(spot.dim * 255.0 + 0.5)));
		painter.fillRect(area, gradient);
	}
	const int64_t now = clock_.nsecsElapsed();
	for (const Ripple &ripple : ripples_) {
		const RippleFrame frame = rippleAt(style_, now - ripple.startNs);
		if (frame.visible) {
			drawRing(painter, QPointF(ripple.x, ripple.y), frame.radius * s, (frame.radius - frame.thickness) * s,
				 toQColor(rippleColor(style_.click, ripple.button), frame.alpha));
		}
	}
	if (style_.halo.active) {
		const HaloGeometry halo = haloGeometry(style_.halo);
		const QColor color = toQColor(style_.halo.color, style_.halo.opacity);
		const double outer = halo.diameter / 2.0 * s;
		if (halo.innerRadius > 0.0) {
			drawRing(painter, cursor, outer, halo.innerRadius * s, color);
		} else {
			painter.setPen(Qt::NoPen);
			painter.setBrush(color);
			painter.drawEllipse(cursor, outer, outer);
		}
	}
	painter.setPen(QPen(QColor(0x1F, 0x1F, 0x24), 1.0));
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(area.adjusted(0.5, 0.5, -0.5, -0.5));
}

} // namespace voltz
```

- [ ] **Step 2: Seção Ajustes › Mouse**

`frontend/voltz/shell/MouseSettings.hpp`:
```cpp
#pragma once

#include <voltz/core/mouse-style.hpp>

#include <QFrame>
#include <QList>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

namespace voltz {

class MousePreview;
class MouseStyleStore;

// Ajustes › Mouse (spec §6.1): halo (cor, tamanho, opacidade, forma), onda do clique (cor, cor do botão direito),
// holofote (raio, escurecimento, atalho), prévia ao vivo e estilos salvos. Cada mudança vale na hora
// (MouseStyleStore). Os rótulos do formulário ficam em formLabels() para o SettingsPage alinhar as colunas.
class MouseSettingsSection : public QFrame {
public:
	MouseSettingsSection(MouseStyleStore *store, QWidget *parent = nullptr);

	const QList<QLabel *> &formLabels() const { return formLabels_; }
	// Relê o estilo, o atalho e a lista de estilos salvos (o atalho pode ter mudado em Configurações).
	void reload();

private:
	QLabel *formLabel(const QString &text);
	QSlider *makeSlider(int minimum, int maximum, const QString &accessibleName);
	QPushButton *makeColorButton(const QString &accessibleName);
	void setColorButton(QPushButton *button, const RgbColor &color);
	void pickColor(RgbColor &value, QPushButton *button);
	void updateValueLabels();
	void updateEnabled();
	void commit();
	void reloadSaved();

	MouseStyleStore *store_;
	MousePreview *preview_ = nullptr;
	QCheckBox *haloActive_ = nullptr;
	QPushButton *haloColor_ = nullptr;
	QSlider *haloSize_ = nullptr;
	QLabel *haloSizeValue_ = nullptr;
	QSlider *haloOpacity_ = nullptr;
	QLabel *haloOpacityValue_ = nullptr;
	QComboBox *haloShape_ = nullptr;
	QCheckBox *clickActive_ = nullptr;
	QPushButton *clickColor_ = nullptr;
	QPushButton *rightColor_ = nullptr;
	QCheckBox *spotlightActive_ = nullptr;
	QSlider *spotlightRadius_ = nullptr;
	QLabel *spotlightRadiusValue_ = nullptr;
	QSlider *spotlightDim_ = nullptr;
	QLabel *spotlightDimValue_ = nullptr;
	QLabel *shortcut_ = nullptr;
	QComboBox *saved_ = nullptr;
	QPushButton *applySaved_ = nullptr;
	QPushButton *removeSaved_ = nullptr;
	RgbColor haloColorValue_;
	RgbColor clickColorValue_;
	RgbColor rightColorValue_;
	QList<QLabel *> formLabels_;
	bool loading_ = false;
};

} // namespace voltz
```

`frontend/voltz/shell/MouseSettings.cpp`:
```cpp
#include "MouseSettings.hpp"

#include "MousePreview.hpp"
#include "MouseStyleStore.hpp"

#include <OBSApp.hpp> // QTStr

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <cmath>
#include <initializer_list>

namespace voltz {

namespace {

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

} // namespace

MouseSettingsSection::MouseSettingsSection(MouseStyleStore *store, QWidget *parent) : QFrame(parent), store_(store)
{
	setObjectName("panel");
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(16, 14, 16, 16);
	layout->setSpacing(10);
	auto *title = new QLabel(QTStr("Voltz.Settings.Mouse").toUpper(), this);
	title->setObjectName("panelTitle");
	layout->addWidget(title);
	auto *hint = new QLabel(QTStr("Voltz.Mouse.Hint"), this);
	hint->setObjectName("muted");
	hint->setWordWrap(true);
	layout->addWidget(hint);

	haloActive_ = new QCheckBox(QTStr("Voltz.Mouse.HaloShow"), this);
	haloColor_ = makeColorButton(QTStr("Voltz.Mouse.HaloColor"));
	haloSize_ = makeSlider(kHaloSizeMin, kHaloSizeMax, QTStr("Voltz.Mouse.HaloSize"));
	haloSizeValue_ = new QLabel(this);
	haloOpacity_ = makeSlider(static_cast<int>(std::lround(kHaloOpacityMin * 100.0)),
				  static_cast<int>(std::lround(kHaloOpacityMax * 100.0)), QTStr("Voltz.Mouse.HaloOpacity"));
	haloOpacityValue_ = new QLabel(this);
	haloShape_ = new QComboBox(this);
	haloShape_->setAccessibleName(QTStr("Voltz.Mouse.Shape"));
	haloShape_->addItem(QTStr("Voltz.Mouse.Shape.Filled"), static_cast<int>(HaloShape::Filled));
	haloShape_->addItem(QTStr("Voltz.Mouse.Shape.Ring"), static_cast<int>(HaloShape::Ring));
	clickActive_ = new QCheckBox(QTStr("Voltz.Mouse.ClickShow"), this);
	clickColor_ = makeColorButton(QTStr("Voltz.Mouse.ClickColor"));
	rightColor_ = makeColorButton(QTStr("Voltz.Mouse.RightColor"));
	spotlightActive_ = new QCheckBox(QTStr("Voltz.Mouse.SpotlightShow"), this);
	spotlightRadius_ = makeSlider(kSpotlightRadiusMin, kSpotlightRadiusMax, QTStr("Voltz.Mouse.SpotlightRadius"));
	spotlightRadiusValue_ = new QLabel(this);
	spotlightDim_ = makeSlider(static_cast<int>(std::lround(kSpotlightDimMin * 100.0)),
				   static_cast<int>(std::lround(kSpotlightDimMax * 100.0)),
				   QTStr("Voltz.Mouse.SpotlightDim"));
	spotlightDimValue_ = new QLabel(this);

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
	addRow(QTStr("Voltz.Mouse.Halo"), haloActive_);
	addRow(QTStr("Voltz.Mouse.Color"), haloColor_);
	addValueRow(QTStr("Voltz.Mouse.Size"), haloSize_, haloSizeValue_);
	addValueRow(QTStr("Voltz.Mouse.Opacity"), haloOpacity_, haloOpacityValue_);
	addRow(QTStr("Voltz.Mouse.Shape"), haloShape_);
	addRow(QTStr("Voltz.Mouse.Click"), clickActive_);
	addRow(QTStr("Voltz.Mouse.Color"), clickColor_);
	addRow(QTStr("Voltz.Mouse.RightButton"), rightColor_);
	addRow(QTStr("Voltz.Mouse.Spotlight"), spotlightActive_);
	addValueRow(QTStr("Voltz.Mouse.Radius"), spotlightRadius_, spotlightRadiusValue_);
	addValueRow(QTStr("Voltz.Mouse.Dim"), spotlightDim_, spotlightDimValue_);

	preview_ = new MousePreview(this);
	auto *columns = new QHBoxLayout();
	columns->setSpacing(24);
	columns->addLayout(grid, 1);
	columns->addWidget(preview_, 0, Qt::AlignTop);
	layout->addLayout(columns);

	shortcut_ = new QLabel(this);
	shortcut_->setObjectName("muted");
	shortcut_->setWordWrap(true);
	layout->addWidget(shortcut_);

	saved_ = new QComboBox(this);
	saved_->setAccessibleName(QTStr("Voltz.Mouse.Styles"));
	applySaved_ = new QPushButton(QTStr("Voltz.Mouse.StyleApply"), this);
	auto *saveButton = new QPushButton(QTStr("Voltz.Mouse.StyleSave"), this);
	removeSaved_ = new QPushButton(QTStr("Voltz.Mouse.StyleDelete"), this);
	auto *savedRow = new QHBoxLayout();
	savedRow->setSpacing(8);
	savedRow->addWidget(formLabel(QTStr("Voltz.Mouse.Styles")));
	savedRow->addSpacing(8); // mesma distância rótulo → campo da grade (16)
	savedRow->addWidget(saved_, 1);
	savedRow->addWidget(applySaved_);
	savedRow->addWidget(saveButton);
	savedRow->addWidget(removeSaved_);
	layout->addSpacing(4);
	layout->addLayout(savedRow);

	for (QCheckBox *box : {haloActive_, clickActive_, spotlightActive_}) {
		connect(box, &QCheckBox::toggled, this, [this]() { commit(); });
	}
	for (QSlider *slider : {haloSize_, haloOpacity_, spotlightRadius_, spotlightDim_}) {
		connect(slider, &QSlider::valueChanged, this, [this]() {
			updateValueLabels();
			commit();
		});
	}
	connect(haloShape_, &QComboBox::currentIndexChanged, this, [this]() { commit(); });
	connect(haloColor_, &QPushButton::clicked, this, [this]() { pickColor(haloColorValue_, haloColor_); });
	connect(clickColor_, &QPushButton::clicked, this, [this]() { pickColor(clickColorValue_, clickColor_); });
	connect(rightColor_, &QPushButton::clicked, this, [this]() { pickColor(rightColorValue_, rightColor_); });
	connect(applySaved_, &QPushButton::clicked, this, [this]() {
		if (saved_->isEnabled()) {
			store_->applySaved(saved_->currentText());
		}
	});
	connect(saveButton, &QPushButton::clicked, this, [this]() {
		bool ok = false;
		const QString name = QInputDialog::getText(this, QTStr("Voltz.Mouse.StyleName.Title"),
							   QTStr("Voltz.Mouse.StyleName.Label"), QLineEdit::Normal,
							   saved_->isEnabled() ? saved_->currentText() : QString(), &ok);
		if (ok && !name.trimmed().isEmpty()) {
			store_->saveCurrentAs(name);
			saved_->setCurrentText(name.trimmed());
		}
	});
	connect(removeSaved_, &QPushButton::clicked, this, [this]() {
		if (saved_->isEnabled()) {
			store_->removeSaved(saved_->currentText());
		}
	});
	connect(store_, &MouseStyleStore::styleChanged, this, [this]() { reload(); });
	connect(store_, &MouseStyleStore::savedStylesChanged, this, [this]() { reloadSaved(); });
	reload();
	reloadSaved();
}

QLabel *MouseSettingsSection::formLabel(const QString &text)
{
	auto *label = new QLabel(text, this);
	formLabels_.append(label);
	return label;
}

QSlider *MouseSettingsSection::makeSlider(int minimum, int maximum, const QString &accessibleName)
{
	auto *slider = new QSlider(Qt::Horizontal, this);
	slider->setObjectName("mouseSlider");
	slider->setRange(minimum, maximum);
	slider->setAccessibleName(accessibleName);
	slider->setMinimumWidth(160);
	return slider;
}

QPushButton *MouseSettingsSection::makeColorButton(const QString &accessibleName)
{
	auto *button = new QPushButton(this);
	button->setObjectName("colorButton");
	button->setAccessibleName(accessibleName);
	button->setIconSize(QSize(14, 14));
	return button;
}

void MouseSettingsSection::setColorButton(QPushButton *button, const RgbColor &color)
{
	button->setIcon(swatch(color));
	button->setText(QString::fromStdString(formatHexColor(color)));
}

void MouseSettingsSection::pickColor(RgbColor &value, QPushButton *button)
{
	const QColor chosen = QColorDialog::getColor(toQColor(value), this, QTStr("Voltz.Mouse.PickColor"));
	if (!chosen.isValid()) {
		return;
	}
	value = fromQColor(chosen);
	setColorButton(button, value);
	commit();
}

void MouseSettingsSection::updateValueLabels()
{
	haloSizeValue_->setText(QTStr("Voltz.Mouse.Pixels").arg(haloSize_->value()));
	haloOpacityValue_->setText(QTStr("Voltz.Mouse.Percent").arg(haloOpacity_->value()));
	spotlightRadiusValue_->setText(QTStr("Voltz.Mouse.Pixels").arg(spotlightRadius_->value()));
	spotlightDimValue_->setText(QTStr("Voltz.Mouse.Percent").arg(spotlightDim_->value()));
}

void MouseSettingsSection::updateEnabled()
{
	const bool halo = haloActive_->isChecked();
	for (QWidget *widget : std::initializer_list<QWidget *>{haloColor_, haloSize_, haloOpacity_, haloShape_}) {
		widget->setEnabled(halo);
	}
	clickColor_->setEnabled(clickActive_->isChecked());
	rightColor_->setEnabled(clickActive_->isChecked());
	spotlightRadius_->setEnabled(spotlightActive_->isChecked());
	spotlightDim_->setEnabled(spotlightActive_->isChecked());
}

void MouseSettingsSection::commit()
{
	if (loading_) {
		return;
	}
	updateEnabled();
	MouseStyle style = store_->style();
	style.halo.active = haloActive_->isChecked();
	style.halo.color = haloColorValue_;
	style.halo.size = haloSize_->value();
	style.halo.opacity = haloOpacity_->value() / 100.0;
	style.halo.shape = static_cast<HaloShape>(haloShape_->currentData().toInt());
	style.click.active = clickActive_->isChecked();
	style.click.color = clickColorValue_;
	style.click.rightColor = rightColorValue_;
	style.spotlight.active = spotlightActive_->isChecked();
	style.spotlight.radius = spotlightRadius_->value();
	style.spotlight.dim = spotlightDim_->value() / 100.0;
	store_->setStyle(style);
}

void MouseSettingsSection::reload()
{
	loading_ = true;
	const MouseStyle &style = store_->style();
	haloActive_->setChecked(style.halo.active);
	haloColorValue_ = style.halo.color;
	setColorButton(haloColor_, haloColorValue_);
	haloSize_->setValue(style.halo.size);
	haloOpacity_->setValue(static_cast<int>(std::lround(style.halo.opacity * 100.0)));
	haloShape_->setCurrentIndex(haloShape_->findData(static_cast<int>(style.halo.shape)));
	clickActive_->setChecked(style.click.active);
	clickColorValue_ = style.click.color;
	setColorButton(clickColor_, clickColorValue_);
	rightColorValue_ = style.click.rightColor;
	setColorButton(rightColor_, rightColorValue_);
	spotlightActive_->setChecked(style.spotlight.active);
	spotlightRadius_->setValue(style.spotlight.radius);
	spotlightDim_->setValue(static_cast<int>(std::lround(style.spotlight.dim * 100.0)));
	updateValueLabels();
	updateEnabled();
	preview_->setMouseStyle(style);
	const QString keys = store_->spotlightShortcutText();
	shortcut_->setText(QTStr("Voltz.Mouse.Shortcut").arg(keys.isEmpty() ? QTStr("Voltz.Mouse.NoShortcut") : keys));
	loading_ = false;
}

void MouseSettingsSection::reloadSaved()
{
	const QString current = saved_->currentText();
	const QSignalBlocker blocker(saved_);
	saved_->clear();
	const std::vector<NamedMouseStyle> styles = store_->savedStyles();
	for (const NamedMouseStyle &item : styles) {
		saved_->addItem(QString::fromStdString(item.name));
	}
	const bool any = !styles.empty();
	if (!any) {
		saved_->addItem(QTStr("Voltz.Mouse.NoStyles"));
	}
	saved_->setEnabled(any);
	applySaved_->setEnabled(any);
	removeSaved_->setEnabled(any);
	if (any) {
		const int index = saved_->findText(current);
		saved_->setCurrentIndex(index >= 0 ? index : 0);
	}
}

} // namespace voltz
```

- [ ] **Step 3: Encaixar em Ajustes, QSS, CMake e locales**

Em `frontend/voltz/shell/SettingsPage.hpp`: nas declarações antecipadas (`class DeviceSelector;` …), acrescentar
`class MouseSettingsSection;` e `class MouseStyleStore;`; trocar o construtor por
`explicit SettingsPage(FrontendBridge *bridge, MouseStyleStore *mouseStyles, QWidget *parent = nullptr);` e, nos
membros, depois de `QCheckBox *hideFromCapture_ = nullptr;`, acrescentar `MouseSettingsSection *mouse_ = nullptr;`;
no comentário da classe, trocar "Gravação (pasta, formato, qualidade, codificador automático), Captura (Task 7) e Sobre"
por "Gravação, Captura, Mouse (destaque do mouse) e Sobre".

Em `frontend/voltz/shell/SettingsPage.cpp`:
1. Depois de `#include "FrontendBridge.hpp"`, acrescentar `#include "MouseSettings.hpp"`.
2. Trocar a assinatura do construtor por
   `SettingsPage::SettingsPage(FrontendBridge *bridge, MouseStyleStore *mouseStyles, QWidget *parent) : QWidget(parent), bridge_(bridge)`.
3. Trocar:
```cpp
	sections_->addWidget(buildCaptureSection());
	sections_->addWidget(buildAboutSection());
```
por:
```cpp
	sections_->addWidget(buildCaptureSection());
	mouse_ = new MouseSettingsSection(mouseStyles, content);
	formLabels_.append(mouse_->formLabels());
	sections_->addWidget(mouse_);
	sections_->addWidget(buildAboutSection());
```
4. Em `showEvent`, depois de `microphone_->reload();`, acrescentar `mouse_->reload();`.

Em `frontend/voltz/shell/VoltzShell.cpp`, trocar `pages_->addWidget(new SettingsPage(bridge_, pages_));` por
`pages_->addWidget(new SettingsPage(bridge_, mouseStyles_, pages_));`.

No fim de `frontend/voltz/shell/voltz-shell.qss`:
```css
/* Ajustes › Mouse: sliders no visual dos do Áudio; botão de cor com amostra */
#voltzShell QSlider#mouseSlider::groove:horizontal {
	background: #1F1F24;
	height: 4px;
	border: none;
	border-radius: 2px;
}

#voltzShell QSlider#mouseSlider::sub-page:horizontal {
	background: #D4F53C;
	border-radius: 2px;
}

#voltzShell QSlider#mouseSlider::sub-page:horizontal:disabled {
	background: #3A3A42;
}

#voltzShell QSlider#mouseSlider::handle:horizontal {
	background: #F2F2F3;
	width: 12px;
	height: 12px;
	margin: -4px 0;
	border-radius: 6px;
}

#voltzShell QSlider#mouseSlider::handle:horizontal:hover {
	background: #FFFFFF;
}

#voltzShell QPushButton#colorButton {
	padding: 5px 12px;
	text-align: left;
}
```

Em `frontend/cmake/feature-voltz.cmake`, na lista do `obs-studio`, **antes** de `voltz/shell/MouseStyleStore.cpp`:
```cmake
    voltz/shell/MousePreview.cpp
    voltz/shell/MousePreview.hpp
    voltz/shell/MouseSettings.cpp
    voltz/shell/MouseSettings.hpp
```

No fim de `frontend/data/locale/voltz-pt-BR.ini`:
```ini
Voltz.Settings.Mouse="Mouse"
Voltz.Mouse.Hint="Destaque do mouse nas cenas com a Tela Voltz (aplique um preset). Mexa e clique na prévia para experimentar."
Voltz.Mouse.Preview="Prévia do destaque do mouse"
Voltz.Mouse.Halo="Halo"
Voltz.Mouse.HaloShow="Mostrar halo"
Voltz.Mouse.Color="Cor"
Voltz.Mouse.HaloColor="Cor do halo"
Voltz.Mouse.Size="Tamanho"
Voltz.Mouse.HaloSize="Tamanho do halo"
Voltz.Mouse.Opacity="Opacidade"
Voltz.Mouse.HaloOpacity="Opacidade do halo"
Voltz.Mouse.Shape="Forma"
Voltz.Mouse.Shape.Filled="Círculo cheio"
Voltz.Mouse.Shape.Ring="Anel"
Voltz.Mouse.Click="Clique"
Voltz.Mouse.ClickShow="Onda ao clicar"
Voltz.Mouse.ClickColor="Cor do clique"
Voltz.Mouse.RightButton="Botão direito"
Voltz.Mouse.RightColor="Cor do botão direito"
Voltz.Mouse.Spotlight="Holofote"
Voltz.Mouse.SpotlightShow="Ligar holofote"
Voltz.Mouse.Radius="Raio"
Voltz.Mouse.SpotlightRadius="Raio do holofote"
Voltz.Mouse.Dim="Escurecer"
Voltz.Mouse.SpotlightDim="Escurecimento do holofote"
Voltz.Mouse.Pixels="%1 px"
Voltz.Mouse.Percent="%1%"
Voltz.Mouse.Shortcut="Atalho do holofote durante a gravação: %1. Troque em Configurações → Atalhos (modo clássico)."
Voltz.Mouse.NoShortcut="nenhum"
Voltz.Mouse.Styles="Estilos salvos"
Voltz.Mouse.NoStyles="Nenhum estilo salvo"
Voltz.Mouse.StyleApply="Aplicar estilo"
Voltz.Mouse.StyleSave="Salvar estilo…"
Voltz.Mouse.StyleDelete="Excluir estilo"
Voltz.Mouse.StyleName.Title="Salvar estilo"
Voltz.Mouse.StyleName.Label="Nome do estilo:"
Voltz.Mouse.PickColor="Escolher cor"
```
No fim de `frontend/data/locale/voltz-en-US.ini`:
```ini
Voltz.Settings.Mouse="Mouse"
Voltz.Mouse.Hint="Mouse highlight in scenes with the Voltz Screen (apply a preset). Move and click in the preview to try it."
Voltz.Mouse.Preview="Mouse highlight preview"
Voltz.Mouse.Halo="Halo"
Voltz.Mouse.HaloShow="Show halo"
Voltz.Mouse.Color="Color"
Voltz.Mouse.HaloColor="Halo color"
Voltz.Mouse.Size="Size"
Voltz.Mouse.HaloSize="Halo size"
Voltz.Mouse.Opacity="Opacity"
Voltz.Mouse.HaloOpacity="Halo opacity"
Voltz.Mouse.Shape="Shape"
Voltz.Mouse.Shape.Filled="Filled circle"
Voltz.Mouse.Shape.Ring="Ring"
Voltz.Mouse.Click="Click"
Voltz.Mouse.ClickShow="Ripple on click"
Voltz.Mouse.ClickColor="Click color"
Voltz.Mouse.RightButton="Right button"
Voltz.Mouse.RightColor="Right button color"
Voltz.Mouse.Spotlight="Spotlight"
Voltz.Mouse.SpotlightShow="Turn on spotlight"
Voltz.Mouse.Radius="Radius"
Voltz.Mouse.SpotlightRadius="Spotlight radius"
Voltz.Mouse.Dim="Dim"
Voltz.Mouse.SpotlightDim="Spotlight dimming"
Voltz.Mouse.Pixels="%1 px"
Voltz.Mouse.Percent="%1%"
Voltz.Mouse.Shortcut="Spotlight shortcut while recording: %1. Change it in Settings → Hotkeys (classic mode)."
Voltz.Mouse.NoShortcut="none"
Voltz.Mouse.Styles="Saved styles"
Voltz.Mouse.NoStyles="No saved styles"
Voltz.Mouse.StyleApply="Apply style"
Voltz.Mouse.StyleSave="Save style…"
Voltz.Mouse.StyleDelete="Delete style"
Voltz.Mouse.StyleName.Title="Save style"
Voltz.Mouse.StyleName.Label="Style name:"
Voltz.Mouse.PickColor="Pick a color"
```

- [ ] **Step 4: Compilar**

CONFIGURAR (arquivos novos); COMPILAR APP (segundo plano); paridade dos locales sem saída. Expected: `exit=0`.

- [ ] **Step 5: Teste ao vivo — controles, estilos salvos, atalho refletido e capturas da janela**

Bloco de teste salvo **com BOM** (`$liveTest = $true`, `$standins = $false`), com um estilo salvo de antemão:
```powershell
$voltzIni = "MouseStyle=$testStyle`r`nMouseStyles=" + '[{"nome":"Grande","estilo":{"halo":{"ativo":true,"cor":"#FF4D4D","tamanho":150,"opacidade":0.5,"forma":"anel"}}}]' + "`r`n"
```
(defina esta linha **antes** de `New-CleanConfig`). No `try`, depois do `Start-App`:
```powershell
[void](Uia "Ajustes"); Start-Sleep 2
& powershell -ExecutionPolicy Bypass -File $uia -ProcessId $proc.Id -List | Select-String -SimpleMatch -Pattern "destaque do mouse","Cor do halo","Tamanho do halo","Ligar holofote","Mostrar halo","Aplicar estilo","Excluir estilo","Estilos salvos" | ForEach-Object { $_.Line }
Resize-Shell 1280 1700; Shot $shell "t8-ajustes"
if (-not (Uia "Ligar holofote")) { throw "Ligar holofote nao encontrado" }; Start-Sleep 1
"holofote ligado pela UI: " + (((Vendor "MouseState").style | ConvertFrom-Json).holofote.ativo)
if (-not (Uia "Mostrar halo")) { throw "Mostrar halo nao encontrado" }; Start-Sleep 1
"halo apos desmarcar: " + (((Vendor "MouseState").style | ConvertFrom-Json).halo.ativo)
Shot $shell "t8-ajustes-sem-halo"
if (-not (Uia "Aplicar estilo")) { throw "Aplicar estilo nao encontrado" }; Start-Sleep 1
$st = (Vendor "MouseState").style | ConvertFrom-Json
"estilo 'Grande': tamanho=$($st.halo.tamanho) forma=$($st.halo.forma) cor=$($st.halo.cor) halo=$($st.halo.ativo) holofote=$($st.holofote.ativo)"
Shot $shell "t8-ajustes-grande"
if (-not (Uia "Excluir estilo")) { throw "Excluir estilo nao encontrado" }; Start-Sleep 1
[void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.ToggleSpotlight"}}]'); Start-Sleep 1
Shot $shell "t8-ajustes-atalho"
Close-App; Save-Log
"user.ini MouseStyle: " + (Get-VoltzIni "MouseStyle")
"user.ini MouseStyles: " + (Get-VoltzIni "MouseStyles")
LogLines '\[voltz\] (holofote|estilo do mouse)|QObject::connect|QWidget'
```
Critérios:
1. A lista UIA mostra "Prévia do destaque do mouse", "Cor do halo", "Tamanho do halo", "Ligar holofote", "Mostrar
   halo", "Aplicar estilo", "Excluir estilo", "Estilos salvos".
2. `holofote ligado pela UI: True`; `halo apos desmarcar: False`; `estilo 'Grande': tamanho=150 forma=anel cor=#FF4D4D
   halo=True holofote=False` (estilo salvo substitui o atual inteiro).
3. `user.ini MouseStyles: []` (excluído) e `user.ini MouseStyle` com `"tamanho":150` e `"holofote":{"ativo":true`
   (o atalho, depois do estilo); log com `[voltz] estilo do mouse aplicado: Grande` e `[voltz] holofote: ligado`/
   `desligado`; sem `QObject::connect`/`QWidget` novos.
4. Capturas (descreva cada uma): `t8-ajustes` — seção MOUSE com o formulário (Halo, Cor `#D4F53C` com amostra lima,
   Tamanho 200 px, Opacidade 100%, Forma, Clique, Cor, Botão direito `#4DA3FF`, Holofote, Raio/Escurecer
   desabilitados), a prévia 320×180 à direita (fundo escuro com duas "janelas" e um halo lima grande no centro), o texto
   do atalho com `Ctrl + Shift + H` e a linha "Estilos salvos" com "Grande"; `t8-ajustes-sem-halo` — prévia escurecida
   pelo holofote, sem halo, controles do halo desabilitados; `t8-ajustes-grande` — anel vermelho na prévia, Tamanho 150
   px, Forma "Anel"; `t8-ajustes-atalho` — "Ligar holofote" marcado de novo pelo atalho, combo com "Nenhum estilo
   salvo" e botões de estilo desabilitados.

- [ ] **Step 6: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/shell/MousePreview.hpp frontend/voltz/shell/MousePreview.cpp frontend/voltz/shell/MouseSettings.hpp frontend/voltz/shell/MouseSettings.cpp frontend/voltz/shell/SettingsPage.hpp frontend/voltz/shell/SettingsPage.cpp frontend/voltz/shell/VoltzShell.cpp
git add frontend/voltz/shell/MousePreview.hpp frontend/voltz/shell/MousePreview.cpp frontend/voltz/shell/MouseSettings.hpp frontend/voltz/shell/MouseSettings.cpp frontend/voltz/shell/SettingsPage.hpp frontend/voltz/shell/SettingsPage.cpp frontend/voltz/shell/VoltzShell.cpp frontend/voltz/shell/voltz-shell.qss frontend/cmake/feature-voltz.cmake frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): Ajustes > Mouse com previa ao vivo, holofote e estilos salvos

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 9: Smoke do mouse (`tools/voltz-smoke-mouse.ps1`) e `mouse` no smoke de projeto

**Files:**
- Create: `tools/voltz-smoke-mouse.ps1`
- Modify: `tools/voltz-smoke-project.ps1`

- [ ] **Step 1: Escrever o smoke (ASCII; acentos por `[char]`)**

`tools/voltz-smoke-mouse.ps1`:
```powershell
<#
  Teste de integracao do Plano 4A (destaque do mouse, plugin voltz-sources, grupo "Tela Voltz").
  Fase A: config limpa com VOLTZ_TEST_LIVE=1 (pedidos "voltz-teste" no obs-websocket; o mouse real nunca e lido) e
  VOLTZ_TEST_PRESET_STANDINS=1 (o preset cria Tela 2560x1440 e Camera 1280x720 de COR: nenhuma captura de tela nem
  webcam). Aplica o preset "So tela / So camera" por UI Automation, confere o grupo "Tela Voltz" (Tela + Voltz Mouse),
  injeta o mouse e confere o desenho por GetSourceScreenshot da cena sintetica (halo lima no centro, holofote
  escurecendo o canto, onda do clique); grava ~6 s alternando o holofote pelo atalho e valida o projeto (mouse no
  manifesto, eventos do holofote, tela.mkv 2560x1440 da captura e nao do grupo).
  Fase B: colecao "de antes do 4A" (Tela direto numa cena, com posicao/caixa/trava) -> reabre e confere a migracao
  para o grupo no mesmo lugar; reabre de novo e confere que nao migra outra vez.
  Seguranca: aborta se existir monitor_capture/window_capture/game_capture/dshow_input; nao move o mouse, nao clica,
  nao digita; so metadados com ffprobe; config portatil guardada e restaurada; -OutDir apagado no fim.
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-mouse.ps1 [-OutDir <pasta>] [-Keep]
#>
param(
    [string]$Config = "RelWithDebInfo",
    [string]$OutDir = (Join-Path $env:TEMP "voltz-smoke-mouse"),
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
$testStyle = '{"halo":{"ativo":true,"cor":"#D4F53C","tamanho":200,"opacidade":1.0,"forma":"cheio"},"clique":{"ativo":true,"cor":"#D4F53C","cor_direito":"#4DA3FF"},"holofote":{"ativo":false,"raio":220,"escurecer":0.55}}'

$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.smokemouse-bak"
$voltzCfg = Join-Path $cfg "Voltz Studio"
$logDir = Join-Path $env:TEMP "voltz-smoke-mouse-logs"
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"
$hadVoltzVideos = Test-Path -LiteralPath $voltzVideos

if ((Test-Path -LiteralPath $OutDir) -and @(Get-ChildItem -Force -LiteralPath $OutDir).Count -gt 0) {
    Write-Host "FALHOU -OutDir ja existe e nao esta vazio: $OutDir"; exit 1
}
if (Test-Path $bak) { Write-Host "FALHOU backup anterior em $bak (restaure ou apague antes de rodar)"; exit 1 }

Add-Type -AssemblyName System.Drawing
$checks = [ordered]@{}
function Check([string]$name, [bool]$ok) { $script:checks[$name] = $ok }

function Ws([string]$json) {
    $file = Join-Path $env:TEMP "voltz-smoke-mouse-req.json"
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

function Vendor([string]$type, [string]$dataJson = '{}') {
    $r = Ws ('[{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste","requestType":"' + $type + '","requestData":' + $dataJson + '}}]')
    if ($r.Count -lt 1 -or -not $r[0].ok) { throw "pedido de teste $type falhou" }
    return $r[0].data.responseData
}

# Seguranca: nenhuma captura real (tela, janela, jogo, webcam) pode existir no app
function Assert-NoRealCapture {
    $inputs = @((Ws '[{"requestType":"GetInputList"}]')[0].data.inputs)
    $real = @($inputs | Where-Object { $_.inputKind -match 'monitor_capture|window_capture|game_capture|dshow_input' })
    if ($real.Count -gt 0) { throw ("captura real presente - abortando: " + (($real | ForEach-Object { $_.inputName }) -join ', ')) }
}

# Render de cena SINTETICA (cor + Voltz Mouse) em 640x360 - nunca a tela real nem quadro de video
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

function Read-JsonFile([string]$path) {
    return (Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json) | ForEach-Object { $_ }
}

function New-CleanConfig([string]$voltzLines) {
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
    New-Item -ItemType Directory -Force $wsDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}')
    [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[Voltz]`r`n" + $voltzLines, (New-Object Text.UTF8Encoding $false))
}

# user.ini [Voltz] com o app FECHADO
function Set-VoltzIni([string]$key, [string]$value) {
    $path = Join-Path $voltzCfg "user.ini"
    $lines = [System.Collections.Generic.List[string]]@(Get-Content -LiteralPath $path -Encoding UTF8)
    $start = $lines.IndexOf("[Voltz]")
    if ($start -lt 0) { $lines.Add("[Voltz]"); $start = $lines.Count - 1 }
    $end = $lines.Count
    for ($i = $start + 1; $i -lt $lines.Count; $i++) { if ($lines[$i].StartsWith("[")) { $end = $i; break } }
    $done = $false
    for ($i = $start + 1; $i -lt $end; $i++) { if ($lines[$i].StartsWith("$key=")) { $lines[$i] = "$key=$value"; $done = $true } }
    if (-not $done) { $lines.Insert($end, "$key=$value") }
    [IO.File]::WriteAllLines($path, $lines, (New-Object Text.UTF8Encoding $false))
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

$ffprobe = (Get-Command ffprobe -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $ffprobe) {
    $ffprobe = Get-ChildItem (Join-Path $root ".deps") -Filter ffprobe.exe -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}

if (Test-Path $cfg) { Move-Item $cfg $bak }
if (Test-Path $logDir) { Remove-Item -Recurse -Force $logDir }
New-Item -ItemType Directory -Force $logDir | Out-Null
$proc = $null
try {
    # ---------------- Fase A: preset com substitutas, desenho e gravacao ----------------
    New-CleanConfig ("MouseStyle=$testStyle`r`nNextProjectName=Smoke mouse`r`n")
    New-Item -ItemType Directory -Force $OutDir | Out-Null
    $env:VOLTZ_TEST_LIVE = "1"
    $env:VOLTZ_TEST_PRESET_STANDINS = "1"
    Check "A: app aberto" (Start-App)
    Check "A: preset 'So tela / So camera' (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    $r = Ws ('[{"requestType":"GetSceneItemList","requestData":{"sceneName":' + (ConvertTo-Json $sceneScreen) + '}},{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":"Tela Voltz"}}]')
    $top = @($r[0].data.sceneItems)
    $inner = @($r[1].data.sceneItems)
    Check "A: cena 'So tela' = um grupo 'Tela Voltz'" ($top.Count -eq 1 -and $top[0].sourceName -eq "Tela Voltz" -and $top[0].isGroup)
    Check "A: grupo = Tela (embaixo) + Voltz Mouse, travadas em (0,0)" ($inner.Count -eq 2 -and $inner[0].sourceName -eq "Tela" -and $inner[1].sourceName -eq "Voltz Mouse" -and $inner[1].inputKind -eq "voltz_mouse_source" -and $inner[0].sceneItemLocked -and $inner[1].sceneItemLocked -and $inner[1].sceneItemTransform.positionX -eq 0)
    [void](Ws ('[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":' + (ConvertTo-Json $sceneScreen) + '}}]'))
    Start-Sleep -Seconds 1
    $state = Vendor "MouseState"
    $style = $state.style | ConvertFrom-Json
    Check "A: Voltz Mouse mostrada ($($state.showing)) com o estilo do user.ini (tamanho $($style.halo.tamanho))" ($state.showing -ge 1 -and $style.halo.tamanho -eq 200)
    # Tela 2560x1440 escalada 0,75 para 1920x1080: pixel (1280,720) da captura = centro da imagem 640x360
    [void](Vendor "InjectMouse" '{"x":1280,"y":720}')
    Start-Sleep -Milliseconds 300
    $img = Scene-Image $sceneScreen; $center = $img.GetPixel(320, 180); $corner = $img.GetPixel(40, 40); $img.Dispose()
    Check "A: halo lima no centro ($center)" (Near $center 212 245 60)
    Check "A: canto = Tela ($corner)" (Near $corner 42 42 48)
    $r = Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.ToggleSpotlight"}}]'
    Start-Sleep -Seconds 1
    $img = Scene-Image $sceneScreen; $dark = $img.GetPixel(40, 40); $img.Dispose()
    Check "A: atalho liga o holofote e escurece o canto ($dark)" ($r.Count -eq 1 -and $r[0].ok -and $dark.R -le $corner.R - 8)
    # clique + estado na MESMA conexao (cada Ws abre um powershell: ~1 s, mais que os 450 ms da onda)
    $r = Ws '[{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste","requestType":"InjectMouse","requestData":{"x":1280,"y":720,"click":"l"}}},{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste","requestType":"MouseState","requestData":{}}}]'
    $r1 = if ($r.Count -eq 2) { $r[1].data.responseData.ripples } else { -1 }
    Start-Sleep -Seconds 1
    $r2 = (Vendor "MouseState").ripples
    Check "A: onda do clique nasce e some ($r1 -> $r2)" ($r1 -eq 1 -and $r2 -eq 0)

    # Gravacao: pasta = -OutDir, audio mutado; o holofote esta ligado e o atalho o desliga e religa
    $r = Ws ('[{"requestType":"SetProfileParameter","requestData":{"parameterCategory":"SimpleOutput","parameterName":"FilePath","parameterValue":' + (ConvertTo-Json $OutDir) + '}},{"requestType":"GetSpecialInputs"}]')
    $special = $r[1].data
    $mutes = @()
    foreach ($n in @($special.desktop1, $special.desktop2, $special.mic1, $special.mic2, $special.mic3, $special.mic4)) {
        if ($n) { $mutes += @{ requestType = "SetInputMute"; requestData = @{ inputName = $n; inputMuted = $true } } }
    }
    if ($mutes.Count -gt 0) { [void](Ws (ConvertTo-Json -InputObject $mutes -Depth 5 -Compress)) }
    Assert-NoRealCapture
    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "A: StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 2
    [void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.ToggleSpotlight"}}]')
    Start-Sleep -Seconds 2
    [void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.ToggleSpotlight"}}]')
    Start-Sleep -Seconds 2
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "A: StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 5
    Check "A: encerrou limpo" (Close-App "A")
    Remove-Item Env:VOLTZ_TEST_PRESET_STANDINS -ErrorAction SilentlyContinue

    $projects = @(Get-ChildItem -LiteralPath $OutDir -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz") })
    Check "A: uma pasta de projeto" ($projects.Count -eq 1)
    if ($projects.Count -eq 1) {
        $dir = $projects[0].FullName
        $m = Read-JsonFile (Join-Path $dir "projeto.voltz")
        $spot = @(@($m.eventos) | Where-Object { $_.tipo -like "holofote_*" } | ForEach-Object { $_.tipo })
        Check "A: manifesto ok com mouse (tamanho $($m.mouse.halo.tamanho), holofote no inicio $($m.mouse.holofote.ativo))" ($m.status -eq "ok" -and $m.mouse.halo.tamanho -eq 200 -and $m.mouse.holofote.ativo -eq $true)
        Check "A: eventos holofote_desligado, holofote_ligado ($($spot -join ', '))" ($spot.Count -eq 2 -and $spot[0] -eq "holofote_desligado" -and $spot[1] -eq "holofote_ligado")
        Check "A: manifesto tela 2560x1440 (a captura, nao o grupo)" ($m.arquivos.tela -and $m.arquivos.tela.largura -eq 2560 -and $m.arquivos.tela.altura -eq 1440)
        Check "A: nenhum iso_falhou" (@(@($m.eventos) | Where-Object { $_.tipo -eq "iso_falhou" }).Count -eq 0)
        $tela = Join-Path $dir "tela.mkv"
        if ($ffprobe -and (Test-Path -LiteralPath $tela)) {
            # so metadados (nunca quadros)
            $ip = (& $ffprobe -v error -show_entries "stream=codec_type,width,height" -of json $tela) | Out-String | ConvertFrom-Json
            $v = @($ip.streams | Where-Object { $_.codec_type -eq "video" })
            Check "A: ffprobe tela.mkv 2560x1440" ($v.Count -eq 1 -and $v[0].width -eq 2560 -and $v[0].height -eq 1440)
        } else {
            Write-Host "AVISO ffprobe ou tela.mkv ausente: resolucao do ISO conferida so pelo manifesto"
        }
    }
    Check "A: log - ISO grava a captura 'Tela' (color_source)" (LogHas "A" "[voltz] iso tela: fonte 'Tela' (color_source)")
    Check "A: log - plugin carregado em modo de teste" ((LogHas "A" "[voltz-sources] carregado") -and (LogHas "A" "[voltz-sources] modo de teste"))
    Check "A: log - mouse real nunca lido" (-not (LogHas "A" "[voltz] mouse ao vivo: ligado"))

    # ---------------- Fase B: migracao de uma colecao de antes do 4A ----------------
    New-CleanConfig ("MouseStyle=$testStyle`r`n")
    Check "B1: app aberto" (Start-App)
    [void](Ws ('[{"requestType":"CreateScene","requestData":{"sceneName":"Antiga"}},' +
        '{"requestType":"CreateInput","requestData":{"sceneName":"Antiga","inputName":"Tela","inputKind":"color_source_v3","inputSettings":{"color":4281346602,"width":2560,"height":1440}}},' +
        '{"requestType":"CreateInput","requestData":{"sceneName":"Antiga","inputName":"Logo","inputKind":"color_source_v3","inputSettings":{"color":4278255360,"width":200,"height":200}}},' +
        '{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Antiga"}}]'))
    $telaId = (Ws '[{"requestType":"GetSceneItemId","requestData":{"sceneName":"Antiga","sourceName":"Tela"}}]')[0].data.sceneItemId
    [void](Ws ('[{"requestType":"SetSceneItemTransform","requestData":{"sceneName":"Antiga","sceneItemId":' + $telaId + ',"sceneItemTransform":{"positionX":100,"positionY":50,"boundsType":"OBS_BOUNDS_SCALE_INNER","boundsWidth":1600,"boundsHeight":900}}},' +
        '{"requestType":"SetSceneItemLocked","requestData":{"sceneName":"Antiga","sceneItemId":' + $telaId + ',"sceneItemLocked":true}}]'))
    $uuid = (@((Ws '[{"requestType":"GetInputList"}]')[0].data.inputs) | Where-Object { $_.inputName -eq "Tela" }).inputUuid
    Check "B1: Tela criada ($uuid)" ([bool]$uuid)
    Check "B1: encerrou limpo" (Close-App "B1")
    Set-VoltzIni "ScreenSourceUuid" $uuid

    Check "B2: app aberto" (Start-App)
    $r = Ws '[{"requestType":"GetSceneItemList","requestData":{"sceneName":"Antiga"}},{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":"Tela Voltz"}}]'
    $items = @($r[0].data.sceneItems)
    $inner = @($r[1].data.sceneItems)
    Check "B2: Tela virou grupo no mesmo lugar (100,50; caixa 1600x900; travado; embaixo do Logo)" ($items.Count -eq 2 -and $items[0].sourceName -eq "Tela Voltz" -and $items[0].isGroup -and $items[0].sceneItemLocked -and $items[0].sceneItemTransform.positionX -eq 100 -and $items[0].sceneItemTransform.positionY -eq 50 -and $items[0].sceneItemTransform.boundsWidth -eq 1600 -and $items[0].sceneItemTransform.boundsHeight -eq 900 -and $items[1].sourceName -eq "Logo")
    Check "B2: grupo com Tela + Voltz Mouse" ($inner.Count -eq 2 -and $inner[0].sourceName -eq "Tela" -and $inner[1].sourceName -eq "Voltz Mouse")
    # grupo em (100,50) com escala 0,625: pixel (1280,720) da captura = (900,500) no canvas = (300,167) na imagem
    [void](Vendor "InjectMouse" '{"x":1280,"y":720}')
    Start-Sleep -Milliseconds 300
    $img = Scene-Image "Antiga"; $p = $img.GetPixel(300, 167); $img.Dispose()
    Check "B2: halo no centro da Tela migrada ($p)" (Near $p 212 245 60)
    Check "B2: encerrou limpo" (Close-App "B2")
    Check "B2: log - 1 item migrado" (LogHas "B2" "[voltz] tela voltz: 1 item(ns) migrado(s)")

    Check "B3: app aberto" (Start-App)
    $r = Ws '[{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":"Tela Voltz"}}]'
    Check "B3: grupo intacto (2 camadas)" ($r.Count -eq 1 -and @($r[0].data.sceneItems).Count -eq 2)
    Check "B3: encerrou limpo" (Close-App "B3")
    Check "B3: nao migra de novo" (-not (LogHas "B3" "[voltz] tela voltz:"))
} catch {
    Check "sem excecao ($($_.Exception.Message))" $false
} finally {
    if ($proc -and -not $proc.HasExited) {
        $proc.Refresh()
        [void]$proc.CloseMainWindow()
        if (-not $proc.WaitForExit(30000)) { Stop-Process -Id $proc.Id -Force }
    }
    Remove-Item Env:VOLTZ_TEST_LIVE, Env:VOLTZ_TEST_PRESET_STANDINS -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 3
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    if (Test-Path $bak) { Move-Item $bak $cfg }
    if (-not $Keep -and (Test-Path -LiteralPath $OutDir)) { Remove-Item -Recurse -Force -LiteralPath $OutDir; Write-Host "apagado: $OutDir" }
    if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) {
        Remove-Item -Force -LiteralPath $voltzVideos
        Write-Host "apagado (vazio): $voltzVideos"
    }
}

$failed = 0
foreach ($k in $checks.Keys) {
    if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
}
Write-Host "logs: $logDir"
if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE MOUSE OK"
```

- [ ] **Step 2: `mouse` no smoke de projeto**

Em `tools/voltz-smoke-project.ps1`, logo depois da linha
`Check "manifesto declara 3 faixas de audio" ($m.arquivos.programa.faixas_audio -eq 3)`, acrescentar:
```powershell
    Check "manifesto: bloco mouse (halo/clique/holofote, estilo padrao)" ($m.mouse -and $m.mouse.halo -and $m.mouse.clique -and $m.mouse.holofote -and $m.mouse.halo.tamanho -eq 46)
```

- [ ] **Step 3: Rodar os dois smokes**

SMOKE MOUSE e SMOKE PROJETO (cada um em primeiro plano, um de cada vez; os dois guardam e restauram a config
portátil).
Expected: `SMOKE MOUSE OK` (todas as linhas `OK`) e `SMOKE PROJETO OK` (com a linha nova do `mouse`). Se algum `FALHOU`,
leia `%TEMP%\voltz-smoke-mouse-logs\<fase>.txt` e corrija antes de seguir.

- [ ] **Step 4: Commitar**

```bash
git add tools/voltz-smoke-mouse.ps1 tools/voltz-smoke-project.ps1
git commit -m "test(voltz): smoke do destaque do mouse (Tela Voltz, render, holofote, migracao) e mouse no smoke de projeto

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: Verificação, registro e push

**Files:** `docs/voltz/backlog-plano-4.md` (novo), `docs/voltz/backlog-plano-3.md`, `docs/voltz/upstream-patches.md`
(conferência), `CLAUDE.md`, `AGENTS.md`, `docs/HISTORICO.md`

- [ ] **Step 1:** COMPILAR TUDO (segundo plano), RODAR TESTES (todos passam; informe o total — esperado **126**
  casos), SMOKE, SMOKE GRAVAÇÃO, SMOKE PROJETO, SMOKE PROJETO `-ProgramOnly` e SMOKE MOUSE (um de cada vez).
- [ ] **Step 2:** Capturas finais **só da janela** (bloco de teste com `$standins = $true`, preset "Só tela / Só
  câmera" por UIA, `InjectMouse` em (1280,720)): Gravar (prévia com a Tela cinza e o halo; Fontes com o grupo "Tela
  Voltz" aberto), Ajustes (`Resize-Shell 1280 1700`; seção Mouse inteira) e Projetos depois de uma gravação de 3 s.
  Descreva cada uma no relatório. Nenhuma captura de quadro de vídeo, nenhuma da tela inteira.
- [ ] **Step 3:** Nenhum arquivo do OBS mudou:
  `git diff f05a6c518..HEAD --stat -- . ':(exclude)frontend/voltz' ':(exclude)plugins/voltz-sources' ':(exclude)docs' ':(exclude)tools' ':(exclude)frontend/data/locale/voltz-pt-BR.ini' ':(exclude)frontend/data/locale/voltz-en-US.ini' ':(exclude)frontend/cmake/feature-voltz.cmake' ':(exclude)CLAUDE.md' ':(exclude)AGENTS.md'`
  → sem saída; `git diff f05a6c518..HEAD -- plugins/CMakeLists.txt docs/voltz/upstream-patches.md` → sem saída.
- [ ] **Step 4: Backlog do Plano 4** — criar `docs/voltz/backlog-plano-4.md`:
```markdown
# Backlog do Plano 4 (anotações + destaque do mouse)

## Feito no Plano 4A
- **Plugin `voltz-sources`** (`plugins/voltz-sources/`, entra pelo `frontend/cmake/feature-voltz.cmake` e liga o
  `voltz-core`; nenhum arquivo do OBS muda): fonte **Voltz Mouse** (`voltz_mouse_source`, fora do "Adicionar fonte"),
  tamanho = tamanho da captura do ajuste `alvo` (UUID), desenho com `data/voltz-mouse.effect` (holofote → ondas →
  halo, cor linear como o `color_source`).
- **Canal frontend → plugin:** procedimentos no proc handler global (`voltz_mouse_position`, `voltz_mouse_click`,
  `voltz_mouse_style`, `voltz_mouse_wanted`; nomes em `voltz/core/live-protocol.hpp`) + `MouseFeed` com mutex; o render
  copia um retrato por quadro.
- **Mouse ao vivo** (`LiveMouse`): sem gancho — `GetPhysicalCursorPos` a 16 ms e cliques por Raw Input
  (`RIDEV_INPUTSINK`, janela `HWND_MESSAGE`); só liga com alguma Voltz Mouse mostrada; mesma conversão do
  `cursor.jsonl` (Tela compartilhada de captura de monitor). O `CursorTracker` do 3A ficou intocado.
- **Estilo** (núcleo `mouse-style`/`mouse-effects`): halo (cor, tamanho, opacidade, cheio/anel), onda do clique (cor,
  cor do botão direito; 450 ms), holofote (raio, escurecimento); `user.ini [Voltz] MouseStyle`, estilos salvos em
  `[Voltz] MouseStyles` (até 20); atalho `VoltzStudio.ToggleSpotlight` (Ctrl+Shift+H).
- **Tela Voltz:** um grupo por cena de preset (Tela + Voltz Mouse, camadas travadas em (0,0), recorte copiado);
  migração uma vez por coleção das cenas com a Tela direta (`voltz_tela_migrada` na Tela, `voltz_tela_camadas = 1` no
  grupo). O ISO continua gravando a captura (log `[voltz] iso tela: fonte 'Tela' (…)`).
- **Ajustes › Mouse** com prévia ao vivo (QPainter com as contas do núcleo), estilos salvos e o texto do atalho.
- **Manifesto:** `mouse` (estilo do início), eventos `holofote_ligado`/`holofote_desligado`, campos desconhecidos dentro
  de `mouse`, `pausas[]` e `eventos[]` preservados (pendência do 3A).
- Testes: 126 casos; `tools/voltz-smoke-mouse.ps1` (presets com substitutas, grupo, render por `GetSourceScreenshot`,
  holofote, onda, gravação, migração); `VOLTZ_TEST_LIVE` (fornecedor "voltz-teste": `InjectMouse`, `MouseState`,
  `SetStyle`, `AddMouseSource`) e `VOLTZ_TEST_PRESET_STANDINS` (presets com fontes de cor).

## Próximo: Plano 4B (desenho na tela)
Overlay topmost por monitor com `WDA_EXCLUDEFROMCAPTURE`, barra de ferramentas, `StrokeStore` → `anotacoes.jsonl`,
fonte Voltz Anotações (camada 2 da Tela Voltz: `voltz_tela_camadas` 1 → 2), atalhos de desenho, Ajustes › Anotações e
Ajustes › Atalhos, `anotacoes_queimadas` no Windows < 10 2004. Ver o fim de
`docs/superpowers/plans/2026-10-16-voltz-plano-4a-mouse.md`.

## Pendências e limites do 4A
- O mouse ao vivo só acompanha a **Tela compartilhada** quando ela é captura de monitor; captura de janela ou outra
  captura → sem halo (mesmo limite do `cursor.jsonl`).
- **Raw Input é por processo e por uso:** se outro componente do processo registrar mouse por `RegisterRawInputDevices`,
  o último registro vence e os cliques param (o OBS 32.2.2 não registra) — conferir ao atualizar o OBS/obs-browser.
- Posição amostrada a 16 ms; cliques fora da captura não geram onda; o halo é desenhado por cima do cursor do Windows
  (a captura já o traz).
- Estilo é global (não por coleção/cena); mudar cor/tamanho no meio da gravação não vira evento (só o holofote).
- Migração só uma vez por coleção: a Tela compartilhada adicionada à mão numa cena nova depois fica sem grupo (usar o
  preset). Fonte Voltz Mouse sem propriedades (diálogo vazio).
- O `cursor.jsonl` continua com o monitor do início da gravação; o halo ao vivo segue a troca de monitor (500 ms).
- `VOLTZ_TEST_LIVE` e `VOLTZ_TEST_PRESET_STANDINS` ficam no código de produção (como `VOLTZ_FAKE_FREE_BYTES`);
  avaliar guarda de compilação no Plano 5.
- `proc_handler` não tem remoção: o `LiveMouse` para no `EXIT`; plugin carregado sem a casca só desenha se alguém
  chamar os procedimentos.

## Testes manuais pendentes do 4A
- Preset Tutorial **real** (monitor real): o halo segue o mouse na prévia e no programa; cliques esquerdo/direito/meio
  e toque de touchpad geram onda (direito na cor própria); holofote pelo Ctrl+Shift+H com outro app em foco; monitor a
  125%/150%; monitor à esquerda do principal; mouse em outro monitor → sem halo.
- Gravar com o preset real e ver num player que o `programa.mp4` tem o destaque e o `tela.mkv` não.
- Ajustes › Mouse: a prévia segue o mouse e faz a onda ao clicar nela; escolher cor no diálogo; "Salvar estilo…" com
  um nome; trocar o atalho em Configurações → Atalhos e ver o texto novo em Ajustes.
- Migração com uma coleção real de antes do 4A (presets do 2B/3): cenas mantêm posição, câmera por cima; desagrupar e
  reabrir não recria o grupo.
- Uso de CPU/GPU com o halo ligado por muito tempo; textos novos em en-US.
```
  Em `docs/voltz/backlog-plano-3.md`: (1) na seção `## Pendências e limites do 3A`, apagar o item
  `- **Manifesto ao reescrever:** campos desconhecidos dentro de `pausas[]` e `eventos[]` ainda não são preservados (os
  de `arquivos.*` passaram a ser no 3B).` (resolvido no 4A); (2) trocar o parágrafo da seção `## Próximo` por
  `Plano 4A feito (ver docs/voltz/backlog-plano-4.md); a seguir o Plano 4B (desenho na tela) e o Plano 5 (marca visual,
  instalador, CI).`
- [ ] **Step 5: `CLAUDE.md`** (depois `cp CLAUDE.md AGENTS.md`). O arquivo está em ~11,9 KB: **antes** de acrescentar,
  mova para `docs/HISTORICO.md` (seção nova `## Estado até o Plano 3B (texto que saiu do CLAUDE.md, 2026-10-16)`) as
  duas linhas atuais de *Estado atual* e deixe no lugar uma só:
  `- Planos 1–3B concluídos (fundação, casca, presets, tema, gravação em projeto com manifesto/marcadores/cursor/ISO tela.mkv+camera.mkv); relato em docs/HISTORICO.md; pendências em docs/voltz/backlog-plano-2.md e backlog-plano-3.md; patches no OBS em docs/voltz/upstream-patches.md.`
  Acrescente em `docs/HISTORICO.md` também `## Plano 4A (2026-10-16)` com 3–4 linhas (plugin + canal, mouse ao vivo sem
  gancho, Tela Voltz + migração, Ajustes › Mouse, 126 casos + smoke do mouse; pendências em backlog-plano-4.md). Depois:
  - *Estado atual*, +1 linha: `- Plano 4A concluído: plugin voltz-sources (Voltz Mouse: halo/onda/holofote), mouse ao vivo sem gancho, grupo 'Tela Voltz' nos presets + migração, Ajustes › Mouse com prévia e estilos salvos, atalho do holofote, mouse no manifesto. Testes: 126 casos + tools/voltz-smoke-mouse.ps1. Pendências: docs/voltz/backlog-plano-4.md.`
  - *Próximos passos* → `1. Escrever e executar o Plano 4B (overlay de desenho com WDA_EXCLUDEFROMCAPTURE, StrokeStore/anotacoes.jsonl, fonte Voltz Anotações como camada 2 da Tela Voltz, atalhos de desenho, Ajustes › Anotações/Atalhos) a partir da spec §6.2–§6.3 e do fim do plano 4A.` e `2. Depois: Plano 5 (marca visual + instalador + CI).`
  - *Decisões*, +2 linhas: `- Fontes Voltz = plugin plugins/voltz-sources (entra pelo feature-voltz.cmake, liga o voltz-core); canal frontend → plugin = procedimentos no proc handler global (voltz/core/live-protocol.hpp) + estado com mutex no plugin; mouse ao vivo sem gancho (GetPhysicalCursorPos + Raw Input) só com Voltz Mouse mostrada.` e `- Tela Voltz = um grupo por cena (OBS não compartilha grupo) com a Tela e a Voltz Mouse compartilhadas e travadas em (0,0); migração uma vez por coleção (voltz_tela_migrada na Tela; voltz_tela_camadas no grupo para o 4B); o ISO continua na captura. Testes de mouse só com VOLTZ_TEST_LIVE + VOLTZ_TEST_PRESET_STANDINS (render por GetSourceScreenshot de cena sintética).`
  - *Armadilhas*, +1 linha cada (só as que se confirmaram na execução): grupo do OBS não é compartilhável entre cenas
    ("Adicionar existente" esconde grupos); `RemoveSceneItem` do obs-websocket não aceita grupo (ler filhos com
    `GetGroupSceneItemList`); `proc_handler` não tem remoção — parar de chamar procedimentos de plugin no `EXIT`;
    fornecedor do obs-websocket só no `obs_module_post_load`; Raw Input é por processo/uso (o último registro vence).
  - Conferir o tamanho: `wc -c CLAUDE.md` perto de 10–12 KB (nunca acima de ~12 KB); `diff CLAUDE.md AGENTS.md` sem saída.
- [ ] **Step 6:** Commit e push:
```bash
git add docs/voltz/backlog-plano-4.md docs/voltz/backlog-plano-3.md docs/HISTORICO.md CLAUDE.md AGENTS.md
git commit -m "docs(voltz): registro do Plano 4A (backlog, memoria)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push origin voltz
```
Expected: push sem `--force`, `voltz -> voltz`.

---

## Auto-revisão (feita ao escrever)

- **Cobertura da spec (parte do 4A):** §3 fontes Voltz no plugin `voltz-sources` — Task 4 (Voltz Mouse; a Voltz
  Anotações é do 4B); §4.3 Ajustes → Mouse — Task 8 (Atalhos e Anotações → 4B, com o porquê); §4.4 grupo "Tela Voltz"
  com as camadas do tamanho da captura — Tasks 4 (tamanho = alvo) e 6 (grupo, presets, migração); §5.2 `mouse` no
  manifesto — Tasks 3 e 7; §6.1 halo/clique/holofote com prévia ao vivo, atalho do holofote, estilos salvos, render pela
  Voltz Mouse a partir da posição/cliques ao vivo e configuração no manifesto — Tasks 1, 2, 4, 5, 7, 8; §8 (erros):
  plugin ausente → presets sem grupo e migração adiada (Task 6), estilo inválido mantém o anterior (Tasks 4/5); §9
  testes unitários de coordenadas/estilos/serialização (Tasks 1–3) e integração por websocket (Task 9); §11 risco 3 — ISO
  continua na captura, conferido por log + manifesto + `ffprobe` com a Tela dentro do grupo (Tasks 7 e 9). Pendência do
  3A "campos desconhecidos em `pausas[]`/`eventos[]`" — Task 3. Fora do 4A, com motivo: §6.2/§6.3 (desenho, overlay,
  `anotacoes.jsonl`, `anotacoes_queimadas`) e Ajustes › Atalhos/Anotações → Plano 4B.
- **Placeholders:** nenhum "TBD". Onde só a execução decide, há critério: valores de pixel com tolerância (`Near`, 24),
  ordem de eventos, linhas de log exatas; se o shader não compilar, o Step 7 da Task 4 diz onde olhar.
- **Consistência de nomes:** núcleo `RgbColor`, `parseHexColor`, `formatHexColor`, `HaloShape`, `HaloStyle`,
  `ClickStyle`, `SpotlightStyle`, `MouseStyle`, `clampMouseStyle`, `mouseStyleToJson`, `mouseStyleFromJson`,
  `NamedMouseStyle`, `savedStylesToJson`, `savedStylesFromJson`, `upsertSavedStyle`, `removeSavedStyle`, limites
  `kHaloSizeMin/Max`, `kHaloOpacityMin/Max`, `kSpotlightRadiusMin/Max`, `kSpotlightDimMin/Max`, `kMaxSavedStyles`;
  `HaloGeometry`/`haloGeometry`, `Ripple`, `RippleFrame`/`rippleAt`, `rippleColor`, `pushRipple`, `pruneRipples`,
  `SpotlightGeometry`/`spotlightGeometry`, `kRippleDurationNs`, `kMaxRipples`, `kRippleMinBase`;
  `voltz::live::{kMouseSourceId, kTargetSetting, kProc*/kDecl* (Position, Click, Style, Wanted), kButton*,
  buttonCode, buttonFromCode, kTestEnv, kTestVendor, testModeEnabled}`; manifesto `hasMouse`, `mouse`, `pauseExtras`,
  `ManifestEvent::extraJson`; plugin `MouseFeed` (`instance`, `setPosition`, `addClick`, `setStyleJson`, `snapshot`,
  `addShowing`, `showing`), `MouseSnapshot`, `registerMouseProcs`, `registerMouseSource`, `registerTestVendor`, pedidos
  `InjectMouse`/`MouseState`/`SetStyle`/`AddMouseSource`; frontend `MouseStyleStore` (`style`, `setStyle`, `toggleSpotlight`,
  `savedStyles`, `saveCurrentAs`, `applySaved`, `removeSaved`, `spotlightShortcutText`, sinais `styleChanged`,
  `spotlightToggled`, `savedStylesChanged`), `LiveMouse` (`shutdown`, `running`), `SharedSource::Mouse`
  (`MouseSourceUuid`), `ensureScreenGroups`, `FrontendBridge::sceneCollectionChanged`,
  `ProjectRecorder::setMouseStyleStore`, `MouseSettingsSection` (`formLabels`, `reload`), `MousePreview`
  (`setMouseStyle`); chaves privadas `voltz_tela_migrada`/`voltz_tela_camadas`; atalho `VoltzStudio.ToggleSpotlight`;
  variáveis `VOLTZ_TEST_LIVE`/`VOLTZ_TEST_PRESET_STANDINS` iguais no código, no bloco de teste e no smoke; locale
  `Voltz.Hotkey.ToggleSpotlight`, `Voltz.Source.Mouse`, `Voltz.Source.ScreenGroup`, `Voltz.Settings.Mouse`,
  `Voltz.Mouse.*` (todas as usadas em `MouseSettings.cpp`/`MousePreview.cpp` existem nos dois `.ini`) e `VoltzMouse` no
  plugin.
- **Contagem de testes:** 107 (base) → 115 (Task 1: 8 casos) → 122 (Task 2: 7 casos) → 126 (Task 3: 4 casos).

---

## Plano 4B (próximo, a escrever depois do 4A) — desenho na tela

- **Overlay** (`frontend/voltz/annotate/OverlayWindow`): uma janela sem borda, topmost, por monitor, com
  `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` **sempre** (independe de `HideOBSWindowsFromCapture`, que é sobre
  as janelas do app); fora do modo desenho, transparente a cliques (`WS_EX_TRANSPARENT | WS_EX_LAYERED`,
  `WS_EX_NOACTIVATE`). Se a afinidade falhar (Windows < 10 2004), o overlay aparece na captura: aviso na casca,
  `"anotacoes_queimadas": true` no manifesto e a camada Voltz Anotações escondida no programa (para não desenhar duas
  vezes).
- **Barra e ferramentas** (`Toolbar`): caneta, marca-texto, linha, seta, retângulo, elipse, texto; 4 cores rápidas +
  seletor, espessura, desfazer, limpar tudo, "somem após N s".
- **`StrokeStore`** (lógica no núcleo: traços em pixels da captura pela mesma conversão do `cursor.jsonl`, serialização
  `anotacoes.jsonl` da spec §6.2, `t_ini`/`t_fim`/`t_limpo`) e **fonte Voltz Anotações** no plugin, alimentada por
  procedimentos novos no mesmo proc handler (traço começado/estendido/terminado/desfeito/limpo) e entrando nos grupos
  existentes pela versão de camadas (`voltz_tela_camadas` 1 → 2, entre a Tela e a Voltz Mouse).
- **Atalhos** (atalhos do frontend do OBS): `VoltzStudio.Draw` (padrão Ctrl+Shift+D, liga/desliga o modo desenho),
  limpar, desfazer e ferramentas; **Esc** só dentro do overlay em modo desenho (um atalho global de Esc dispararia em
  todos os apps). Ajustes › Anotações e Ajustes › Atalhos (estes com o `OBSHotkeyEdit` do OBS, incluindo marcador e
  holofote).
- **Testes seguros:** lógica de traço no núcleo (doctest); traços sintéticos por pedidos novos do fornecedor
  "voltz-teste" (sem passar pelo mouse); o modo desenho que captura a entrada **nunca** é ligado em teste automático
  (overlay só em modo click-through, conferido por `GetWindowLongPtr`/`GetWindowDisplayAffinity`); interação real fica
  para o teste manual do usuário. Pode precisar ser dividido em 4B (overlay + `StrokeStore` + fonte) e 4C (barra,
  texto, atalhos, Ajustes).
