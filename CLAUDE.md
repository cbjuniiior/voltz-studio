# voltz-studio

> Memória deste projeto para agentes de IA (Claude Code · Codex). É carregado
> automaticamente no início de cada sessão — mantenha **conciso** (visão de
> relance, não um diário).
>
> **REGRA — auto-contexto:** ao concluir uma mudança significativa, ANTES de
> encerrar, atualize as seções **Estado atual** e **Próximos passos** abaixo
> (1–2 linhas cada) com o que mudou e o que falta. Não reescreva o histórico
> inteiro. Mantenha CLAUDE.md e AGENTS.md com o mesmo conteúdo.

## Visão geral
**Voltz Studio**: app Windows x64 instalável para **gravar conteúdo 16:9** (tutoriais/aulas), fork do
OBS Studio 32.2.2 com casca própria (marca VoltzPixel). Visão em 5 etapas: 1 Base · 2 Gravação em projeto ·
3 Editor (web/React + FFmpeg) · 4 Câmera IA · 5 Legendas. Spec atual (etapas 1+2):
`docs/superpowers/specs/2026-10-15-voltz-studio-base-gravacao-design.md`.

## Stack & estrutura
- C/C++ + Qt 6 (motor libobs + frontend). Código Voltz novo em `frontend/voltz/` (shell, core, project,
  annotate, tests) e `plugins/voltz-sources/`. Instalador Inno Setup em `installer/`.
- Fork: `github.com/cbjuniiior/voltz-studio`, branch de trabalho **`voltz`** (base tag `32.2.2`; branch padrão do fork, publicado).
  Remotes: `origin` (fork), `upstream` (obsproject). Clone parcial (`blob:none`).
- **Build:** CMake do VS 2022 Build Tools
  (`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`)
  - `cmake --preset voltz-windows-x64` (versionado em `CMakePresets.json`, VS 17 2022, saída `build_x64/`)
  - `cmake --build --preset voltz-windows-x64 --parallel -- -v:minimal -nologo -nodeReuse:false`
  - Testes: `ctest --test-dir build_x64/frontend/voltz/tests -C RelWithDebInfo --output-on-failure`
  - Smokes (`powershell -ExecutionPolicy Bypass -File tools/<nome>.ps1`; todos guardam/restauram a config portátil):
    `voltz-smoke` · `voltz-smoke-record` (grava ~15 s em `Vídeos\Voltz Studio`) · `voltz-smoke-project -OutDir <vazia>` ·
    `voltz-smoke-mouse -OutDir <vazia>` · `voltz-smoke-annotations -OutDir <vazia>` · `voltz-smoke-draw`; demos manuais:
    `voltz-annot-demo`, `voltz-draw-demo` (modo desenho, sem variável de teste).
  - Automação da casca: `tools/voltz-uia.ps1` (UI Automation; `-List`; nav = CheckBox) e `tools/voltz-ws.ps1` (obs-websocket).
  - Executável: `build_x64/rundir/RelWithDebInfo/bin/64bit/VoltzStudio.exe`.
    Testar com `--portable --multi` para não tocar config do OBS instalado.
- Ferramentas instaladas: Inno Setup 6 (`%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe`).

## Estado atual
- Planos 1–4A concluídos (fundação, casca, presets, tema, gravação em projeto com manifesto/marcadores/cursor/ISO,
  plugin voltz-sources com Voltz Mouse e grupo Tela Voltz); relato em `docs/HISTORICO.md`; pendências em
  `docs/voltz/backlog-plano-*.md`; patches no OBS em `docs/voltz/upstream-patches.md`.
- Planos 4B–4C concluídos: anotações (núcleo, rasterizador com retângulo sujo, `anotacoes.jsonl`, fonte Voltz
  Anotações, sobreposição fora da captura) e modo desenho (Ctrl+Shift+D/botão Desenhar, janela de entrada em thread
  própria, saídas garantidas, barra). Testes: 170 casos + smokes de anotações e desenho.

## Decisões
- **Casca Qt nova** + `OBSBasic` oculta como motor (modo clássico = saída de emergência); não reformar `OBSBasic.ui`.
- Cenas mantidas (simplificadas); layout **H2**; tema **preto #09090B + lima #D4F53C**; DM Sans.
- Ícones: **Phosphor Icons** (MIT; SVGs do `@phosphor-icons/core` embutidos no `.qrc`, recoloridos pelo tema);
  peso **Regular** no estado normal e **Fill** no item ativo. Nada de glifos de texto (●▤◫⚙⏸▶) na UI final.
- Lives ocultas mas mantidas (Ajustes → Avançado).
- Gravação em projeto: `programa.mp4` (MP4 híbrido, capítulos, 3 faixas) + ISO `tela.mkv`/`camera.mkv` via
  `obs_canvas` + `cursor.jsonl` + `anotacoes.jsonl` + `projeto.voltz`.
- Instalador **Inno Setup** (repo do OBS não tem script de instalador; só gera ZIP).
- Logo: obturador de 3 lâminas lima inclinado sobre squircle preto; bolinha REC só ao gravar.
- Presets: fontes compartilhadas (Tela/Câmera/Fundo) por UUID em user.ini [Voltz]; câmera redonda = grupo 'Câmera redonda'
  + mask_filter_v2 com `data/voltz/mask-circle.png`. Ajustes de gravação só no modo de saída Simples (Avançado → modo
  clássico).
- Tema `com.voltzpixel.Voltz` (`data/themes/Voltz.ovt`) só quando o usuário não escolheu tema (respeita escolha e alto
  contraste). "Esconder da captura" (`HideOBSWindowsFromCapture`) ligado por padrão no Voltz.
- Ícones em regras QSS (`image:`/`qproperty-icon:`) = SVG pré-colorido em `icons/qss/` (`tools/voltz-qss-icons.sh`) — o
  QSS não passa pelo `PhosphorIconEngine`; no código, sempre `phosphorIcon()`.
- Gravação em projeto e ISO sem patch no OBS (`ProfileOverride` troca pasta/nome no RECORDING_STARTING; `IsoTrack` =
  canvas privado → encoder próprio → ffmpeg_muxer); detalhes em `docs/HISTORICO.md`.
- Decisões arquivadas (pastas de plugins de terceiros, renomear áudio global, anotações ao vivo + editáveis):
  `docs/HISTORICO.md` → "Poda do Plano 4B".
- Fontes Voltz = plugin `plugins/voltz-sources` (entra pelo `feature-voltz.cmake`, liga o `voltz-core`); canal
  frontend → plugin = procedimentos no proc handler global (`voltz/core/live-protocol.hpp`) + estado com mutex no
  plugin; mouse ao vivo sem gancho (`GetPhysicalCursorPos` + Raw Input) só com Voltz Mouse mostrada.
- Tela Voltz = um grupo por cena (Tela + Voltz Anotações + Voltz Mouse compartilhadas, travadas em (0,0)); migração
  uma vez por coleção (`voltz_tela_migrada`) e versão de camadas no grupo (`voltz_tela_camadas` 1 → 2, sempre); ISO na captura.
- Anotações: traços em pixels da captura da Tela; rasterizador por CPU no núcleo (o mesmo na sobreposição e na fonte);
  sobreposição = janela Win32 pura, sempre transparente a cliques no 4B, `WDA_EXCLUDEFROMCAPTURE` sempre (sem suporte →
  "queimadas": fonte não desenha + manifesto); `anotacoes.jsonl` = diário só de acréscimo, consolidado no fim/recuperação.
- Modo desenho: só por ação do usuário (nunca com `VOLTZ_TEST_LIVE`), só no monitor da Tela; janela de entrada Win32 em
  thread própria (saídas e vigia de 1 s pela `DrawSession` do núcleo); captura/foco só em `annotate/DrawInputWindow.cpp`.

## Armadilhas
- Preset upstream `windows-x64` exige **VS 2026**; usar o preset versionado `voltz-windows-x64` (VS 2022). Código só exige SDK ≥ 10.0.20348.
- Armadilhas arquivadas (ATL/8006 do VS, gancho `WH_MOUSE_LL`, nlohmann em alvo estático, saídas/ordem/falha do ISO,
  quadros pulados, geração do projeto, fechamento OBSBasic/casca, `--startrecording`, `AddSourceDialog`, `blog` cedo,
  laço do `QInputDialog`, log AJA/NVENC): `docs/HISTORICO.md` → "Poda do Plano 4B".
- No Git Bash, flags do MSBuild com `/` (ex. `/v:minimal`) viram caminho → usar `-v:minimal`.
- `.gitignore` do OBS é whitelist (`/*` + exceções): arquivo novo na raiz precisa de `!nome` para ser versionado.
- Dentro de QWidget, função livre com nome de membro do Qt (`windowTitle`, `styleSheet()`) resolve para o membro:
  qualificar (`voltz::windowTitle`) ou renomear (`toolbarStyleSheet`).
- Build x86 auxiliar às vezes falha com MSB4018 "Touch"/ZERO_CHECK (race) — rodar de novo.
- Regras da casca no QSS precisam do prefixo `#voltzShell` (o tema do OBS é global e tem `QPushButton { max-height }`).
- Sinais do libobs (cena, saída, `channel_change`, `source_rename`) chegam fora da thread da UI: repassar com `Qt::QueuedConnection`.
- A saída de gravação é recriada ao mudar configurações: reconectar o sinal `stop` a cada `RECORDING_STARTING`.
- Armadilhas de QSS da casca (especificidade, font-size, objectName antes do setStyleSheet): `docs/HISTORICO.md` → "Poda do Plano 4B".
- O projeto compila com `QT_NO_CONTEXTLESS_CONNECT`: todo `connect` com lambda precisa de objeto de contexto.
- Widgets postos com `setItemWidget` não aparecem na UIA; o `SourcesPanel` usa uma fábrica `QAccessible` presa ao
  `objectName` `sourcesList` — não renomear.
- Filhos de grupo emitem sinais pela fonte do grupo: ligar o `signal_handler` de cada grupo para atualizar as Fontes.
- Com "esconder da captura" ligado, `PrintWindow` sai preto: testes de captura escrevem `HideOBSWindowsFromCapture=false`
  no `user.ini` portátil. Os smokes apagam a config portátil (`build_x64/rundir/RelWithDebInfo/config`): backup antes.
- Callbacks do frontend rodam do último registrado para o primeiro: criar o `ProjectRecorder` depois da `FrontendBridge`
  e o `MouseStyleStore` depois do recorder (o EXIT do store grava o estilo e solta o atalho antes); o `DrawModeController`
  (último) trata o EXIT e fecha o traço antes do recorder.
- `TriggerHotkeyByName` (websocket) chama o callback do atalho na thread do websocket com o lock dos atalhos preso: só enfileirar.
- `config_set_default_*` deste libobs grava o padrão também como valor do usuário: "ainda no padrão" = comparar com
  `config_get_default_*` (`stillObsDefault()`), não `config_has_user_value`.
- Warnings como erro valem também para o `voltz-core` e os testes (casts explícitos). `signals` é macro do Qt (não usar como nome).
- UIA: "Gravar" é o nome da nav **e** do botão de gravar — `voltz-uia.ps1 -Name "Gravar"` pode iniciar uma gravação.
- Testes ao vivo nunca com captura/webcam reais: ISO por `VOLTZ_TEST_ISO_SCREEN/CAMERA` (a "Tela teste" do cursor é
  `monitor_capture` com o item desativado); mouse por `VOLTZ_TEST_LIVE` + `VOLTZ_TEST_PRESET_STANDINS`, render por
  `GetSourceScreenshot` de cena sintética.
- Armadilhas arquivadas do 4A–4B (grupos: item novo/entre cenas; show/hide no tick; Raw Input por processo; sRGB da
  fonte; PowerShell 5.1: BOM, array de 1, `$null`): `docs/HISTORICO.md` → "Poda do Plano 4C".
- `proc_handler` não tem remoção: quem chama procedimentos de plugin para no `EXIT` (antes do descarregamento).
- Fornecedor do obs-websocket: plugin só no `obs_module_post_load`; frontend depois do `OBSInit`, com mutex próprio (o
  callback roda sem lock e o unregister não espera); `obs_data` não guarda lista de números (pontos/estado como texto
  JSON); `CreateInput` recusa fontes `OBS_SOURCE_CAP_DISABLED`.
- `OBSApp::notify` reaplica `HideOBSWindowsFromCapture` (inclusive `WDA_NONE`) em toda janela Qt mostrada: janela que
  precisa ficar fora da captura é Win32 pura ou tem a propriedade `isOBSProjectorWindow`.
- `WDA_EXCLUDEFROMCAPTURE` antes do Windows 10 2004 vira caixa preta: só com `SetDisplayAffinitySupported()`.
- Testes com sobreposição: `VOLTZ_TEST_OVERLAY_RECT` fora do desktop virtual + `Assert-OverlaySafe` a cada traço + vigia
  (`FindWindowEx` ∪ `EnumWindows`); fechar por `WM_CLOSE` na casca guardada (o `MainWindowHandle` vira a sobreposição).
- `HTTRANSPARENT` só passa o clique a janelas da mesma thread; dono de outra thread liga as filas de entrada;
  `SetWindowLongPtr(GWL_EXSTYLE)` espera a thread dona. Teste nunca liga o modo desenho (só prova a recusa).
- Qt 6.11: `QScreen::name()` é o nome amigável (repete em monitores iguais): achar por `HMONITOR` (`screenForMonitor`).
- Thread de entrada nunca bloqueia no `DrawShared::mutex` com a janela viva (backlog + `try_lock`); sob ele só
  atribuição/swap. Gatilho enfileirado numa trava da UI chega depois do drain: recusa de 500 ms
  (`recusado (recente)`); o botão Desenhar lê o estado no `pressed`.

## Próximos passos
1. Usuário roda o roteiro manual do 4C; depois escrever e executar o Plano 4D (texto, Ajustes › Anotações/Atalhos,
   todos os monitores) a partir do fim do plano 4C e do `docs/voltz/backlog-plano-4.md`.
2. Depois: Plano 5 (marca visual + instalador + CI).
