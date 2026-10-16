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
  - Smoke: `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1`; gravação real:
    `tools/voltz-smoke-record.ps1` (grava ~15 s em `Vídeos\Voltz Studio` e apaga a pasta do projeto de teste);
    projeto: `tools/voltz-smoke-project.ps1 -OutDir <pasta vazia>` (websocket + UIA + ffprobe; guarda/restaura a config)
  - Automação da casca: `tools/voltz-uia.ps1` (UI Automation; `-List`; nav = CheckBox) e `tools/voltz-ws.ps1` (obs-websocket).
  - Executável: `build_x64/rundir/RelWithDebInfo/bin/64bit/VoltzStudio.exe`.
    Testar com `--portable --multi` para não tocar config do OBS instalado.
- Ferramentas instaladas: Inno Setup 6 (`%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe`).

## Estado atual
- Planos 1–3A concluídos (fundação, casca completa, presets, tema, gravação em projeto com programa.mp4/manifesto/
  marcadores/cursor/Projetos/avisos de disco); relato em `docs/HISTORICO.md`, pendências do 2 em `docs/voltz/backlog-plano-2.md`. Patches no OBS:
  `docs/voltz/upstream-patches.md`.
- Plano 3B concluído: ISO tela.mkv/camera.mkv por canvas privado (fonte de papel do preset, resolução nativa, HEVC/H.264
  AMF→NVENC→QSV em CQP, áudio-guia), offset_ms, iso_falhou com motivo, desligamento por sobrecarga, "Gravar só a versão
  montada". Testes: 107 casos + smoke de projeto com ISO. Pendências: `docs/voltz/backlog-plano-3.md`.

## Decisões
- **Casca Qt nova** + `OBSBasic` oculta como motor (modo clássico = saída de emergência); não reformar `OBSBasic.ui`.
- Cenas mantidas (simplificadas); layout **H2**; tema **preto #09090B + lima #D4F53C**; DM Sans.
- Ícones: **Phosphor Icons** (MIT; SVGs do `@phosphor-icons/core` embutidos no `.qrc`, recoloridos pelo tema);
  peso **Regular** no estado normal e **Fill** no item ativo. Nada de glifos de texto (●▤◫⚙⏸▶) na UI final.
- Plugins de terceiros: o Voltz procura plugins em `%ProgramData%\Voltz Studio\plugins` e `%APPDATA%\Voltz Studio\plugins` (isolado do OBS instalado, de propósito).
- Lives ocultas mas mantidas (Ajustes → Avançado).
- Gravação em projeto: `programa.mp4` (MP4 híbrido, capítulos, 3 faixas) + ISO `tela.mkv`/`camera.mkv` via
  `obs_canvas` + `cursor.jsonl` + `anotacoes.jsonl` + `projeto.voltz`.
- Anotações/mouse **ao vivo + editáveis**: overlay com `WDA_EXCLUDEFROMCAPTURE`, render no programa por fontes Voltz.
- Instalador **Inno Setup** (repo do OBS não tem script de instalador; só gera ZIP).
- Fontes de áudio globais com o nome padrão do OBS ('Desktop Audio'/'Mic/Aux' ou equivalente traduzido) são renomeadas para 'Som do sistema'/'Microfone' (fica na coleção de cenas; nunca sobrescreve nome personalizado).
- Logo: obturador de 3 lâminas lima inclinado sobre squircle preto; bolinha REC só ao gravar.
- Presets: fontes compartilhadas (Tela/Câmera/Fundo) por UUID em user.ini [Voltz]; câmera redonda = grupo 'Câmera redonda'
  + mask_filter_v2 com `data/voltz/mask-circle.png`; grupo 'Tela Voltz' adiado para o Plano 4. Ajustes de gravação só no
  modo de saída Simples (Avançado → modo clássico).
- Tema `com.voltzpixel.Voltz` (`data/themes/Voltz.ovt`) só quando o usuário não escolheu tema (respeita escolha e alto
  contraste). "Esconder da captura" (`HideOBSWindowsFromCapture`) ligado por padrão no Voltz.
- Ícones em regras QSS (`image:`/`qproperty-icon:`) = SVG pré-colorido em `icons/qss/` (`tools/voltz-qss-icons.sh`) — o
  QSS não passa pelo `PhosphorIconEngine`; no código, sempre `phosphorIcon()`.
- Gravação em projeto sem patch no OBS: no RECORDING_STARTING o ProjectRecorder troca temporariamente a pasta e
  Output/FilenameFormatting=programa (ProfileOverride) e desfaz no próximo giro do laço; 3 faixas no modo Simples
  (RecTracks=7 + mixers das fontes globais, só se ainda padrão). Manifesto em nlohmann `ordered_json` (preserva campos
  desconhecidos). Padrões de projeto uma vez por perfil (`[Voltz] ProjectDefaults=true`).
- ISO sem patch: IsoTrack = obs_canvas_create_private (EPHEMERAL, canal 0 = fonte compartilhada do preset) → encoder
  próprio + AAC da faixa 1 → ffmpeg_muxer; fps = do programa; programa segue em H.264, ISO em HEVC; testes só com
  fontes substitutas via VOLTZ_TEST_ISO_SCREEN/CAMERA.

## Armadilhas
- Preset upstream `windows-x64` exige **VS 2026**; usar o preset versionado `voltz-windows-x64` (VS 2022). Código só exige SDK ≥ 10.0.20348.
- Build Tools precisa do componente **ATL** (`Microsoft.VisualStudio.Component.VC.ATL`) — sem ele falham virtualcam, qsv11 e frontend-tools.
- No Git Bash, flags do MSBuild com `/` (ex. `/v:minimal`) viram caminho → usar `-v:minimal`.
- Instalar componente do VS falha com 8006 se houver `MSBuild.exe` vivo → matar processos ou usar `-nodeReuse:false`.
- `.gitignore` do OBS é whitelist (`/*` + exceções): arquivo novo na raiz precisa de `!nome` para ser versionado.
- Log de módulos AJA/Decklink/NVENC falhando é esperado nesta máquina (sem hardware/GPU NVIDIA).
- Dentro de QWidget, chamar funções livres do Voltz qualificadas (`voltz::windowTitle`) — nome não qualificado resolve para membro do Qt.
- Interceptar só fechamentos `event->spontaneous()` no OBSBasic; `close()` programático (Sair, reiniciar) precisa seguir, senão o app relança depois.
- O fechamento da casca deve chamar `OBSBasic::close()` via `Qt::QueuedConnection` (síncrono destrói a casca dentro do próprio closeEvent).
- Build x86 auxiliar às vezes falha com MSB4018 "Touch"/ZERO_CHECK (race) — rodar de novo.
- Sem coleção de cenas em disco o OBS ignora `--startrecording` (o smoke de gravação abre o app uma vez antes). Fechar
  gravando não loga `==== Recording Stop` (upstream; o arquivo é finalizado).
- Regras da casca no QSS precisam do prefixo `#voltzShell` (o tema do OBS é global e tem `QPushButton { max-height }`).
- Sinais do libobs (cena, saída, `channel_change`, `source_rename`) chegam fora da thread da UI: repassar com `Qt::QueuedConnection`.
- `OBSBasic::AddSourceDialog` só abre se chamado por QAction → disparar `actionAddSource` com `trigger()`.
- A saída de gravação é recriada ao mudar configurações: reconectar o sinal `stop` a cada `RECORDING_STARTING`.
- Especificidade QSS: `#voltzShell QFrame#x QWidget {}` vence `#voltzShell QPushButton#id` — estilo de botão dentro de
  painel precisa de seletor tão específico quanto.
- `#voltzShell QWidget { font-size }` vale para widgets do OBS dentro da casca (ex. `VolumeMeter`) — sobrescrever com regra própria.
- O projeto compila com `QT_NO_CONTEXTLESS_CONNECT`: todo `connect` com lambda precisa de objeto de contexto.
- `objectName` usado por regra QSS deve ser definido antes do `setStyleSheet` do pai.
- Widgets postos com `setItemWidget` não aparecem na UIA; o `SourcesPanel` usa uma fábrica `QAccessible` presa ao
  `objectName` `sourcesList` — não renomear.
- Filhos de grupo emitem sinais pela fonte do grupo: ligar o `signal_handler` de cada grupo para atualizar as Fontes.
- `blog` antes de `obs-main.cpp` abrir o log (AppInit, InitTheme) não chega ao `.txt`.
- Com "esconder da captura" ligado, `PrintWindow` sai preto: testes de captura escrevem `HideOBSWindowsFromCapture=false`
  no `user.ini` portátil. Os smokes apagam a config portátil (`build_x64/rundir/RelWithDebInfo/config`): backup antes.
- Callbacks do frontend rodam do último registrado para o primeiro: criar o `ProjectRecorder` depois da `FrontendBridge`.
- `TriggerHotkeyByName` (websocket) chama o callback do atalho na thread do websocket com o lock dos atalhos preso: só enfileirar.
- Nada de IO, log, lock ou alocação dentro do gancho `WH_MOUSE_LL` (o Windows remove gancho lento).
- `config_set_default_*` deste libobs grava o padrão também como valor do usuário: "ainda no padrão" = comparar com
  `config_get_default_*` (`stillObsDefault()`), não `config_has_user_value`.
- nlohmann em alvo estático: `target_link_libraries PRIVATE` quebra o configurar → `target_include_directories(SYSTEM
  PRIVATE $<TARGET_PROPERTY:nlohmann_json::nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>)`.
- Warnings como erro valem também para o `voltz-core` e os testes (casts explícitos). `signals` é macro do Qt (não usar como nome).
- PowerShell 5.1: `.ps1` com acento precisa de BOM; `Select-String` em log com caminho acentuado precisa de
  `-Encoding UTF8`; função que devolve array de 1 elemento é desenrolada (`return ,@(...)`).
- UIA: "Gravar" é o nome da nav **e** do botão de gravar — `voltz-uia.ps1 -Name "Gravar"` pode iniciar uma gravação.
- O OBS cria sozinho o capítulo "Iniciar" em 0 s: `programa.mp4` tem N marcadores + 1 capítulos.
- `QInputDialog`/`QMessageBox` abrem laço de eventos: um `refresh()` pode recriar a lista — ler tudo do item antes.
- `ffmpeg_muxer`/`mp4_output` são `OBS_OUTPUT_AV`: saída só de vídeo não inicia (o ISO leva um AAC). Mix/canvas
  auxiliar herda o fps do canvas principal.
- ISO: liberar saída → encoders → canvas, nessa ordem (destruir o canvas privado libera o mix na hora); nunca
  `obs_canvas_remove` em canvas privado (o `OBSBasic` enfileira `RemoveCanvas` com uma ref).
- `obs_output_start` que falha (ex. sessão de GPU esgotada) é síncrono e não emite `stop`: tentar o próximo encoder ali.
- Os "skipped frames" do vídeo incluem o atraso de render (`lagged`): excesso do codificador = pulados − atrasados.
- Sinal de saída enfileirado para a UI pode chegar depois de outro projeto começar: conferir a geração do projeto.
- Testes de ISO nunca com `monitor_capture`/webcam real; a "Tela teste" do cursor é captura de monitor com o item de
  cena desativado (conferido antes de gravar).

## Próximos passos
1. Escrever e executar o Plano 4 (anotações + destaque do mouse; o grupo 'Tela Voltz' não pode mudar a fonte do ISO) a
   partir da spec §6 e de `docs/voltz/backlog-plano-3.md`.
2. Depois: Plano 5 (marca visual + instalador + CI).
