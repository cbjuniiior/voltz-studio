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
    `tools/voltz-smoke-record.ps1` (grava ~15 s em Vídeos e apaga o arquivo de teste)
  - Executável: `build_x64/rundir/RelWithDebInfo/bin/64bit/VoltzStudio.exe`.
    Testar com `--portable --multi` para não tocar config do OBS instalado.
- Ferramentas instaladas: Inno Setup 6 (`%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe`).

## Estado atual
- Plano 1 (Fundação) concluído: `VoltzStudio.exe` 1.0.0, config em `%APPDATA%\Voltz Studio`, sem phone-home,
  câmera virtual própria, `voltz-core` + testes doctest, casca `VoltzShell` (Gravar com cenas/prévia/gravar/pausar)
  sobre `OBSBasic` oculta. Patches no OBS listados em `docs/voltz/upstream-patches.md`.
- Plano 2A concluído: tela Gravar completa (cenas por UUID, fontes, áudio, erros de gravação visíveis), textos
  pt-BR/en-US (`frontend/data/locale/voltz-*.ini`), ícones Phosphor, DM Sans, barra de título escura.

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
- Smoke de gravação usa `--startrecording` + `ConfirmOnExit=false` no user.ini portátil; sem coleção de cenas em disco o OBS ignora `--startrecording` (por isso o script abre o app uma vez antes). Fechar gravando não loga `==== Recording Stop` (comportamento do upstream; o arquivo é finalizado).
- Regras da casca no QSS precisam do prefixo `#voltzShell` (o tema do OBS é global e tem `QPushButton { max-height }`).
- Sinais do libobs (cena, saída, `channel_change`, `source_rename`) chegam fora da thread da UI: repassar com `Qt::QueuedConnection`.
- `OBSBasic::AddSourceDialog` só abre se chamado por QAction → disparar `actionAddSource` com `trigger()`.
- A saída de gravação é recriada ao mudar configurações: reconectar o sinal `stop` a cada `RECORDING_STARTING`.

## Próximos passos
1. Escrever e executar o Plano 2B (Presets, Projetos, Ajustes, tema `.ovt` do clássico, bandeja, diálogos de saída/falha) a partir de `docs/voltz/backlog-plano-2.md`.
2. Depois: Plano 3 (gravação em projeto), 4 (anotações + mouse), 5 (marca visual + instalador + CI).
