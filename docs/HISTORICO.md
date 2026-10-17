# Histórico do Voltz Studio

Arquivo do que saiu do `CLAUDE.md`/`AGENTS.md` na poda (relatos de planos entregues). **Não** é carregado
automaticamente; consultar sob demanda. Detalhes de cada plano: `docs/superpowers/plans/` e `docs/voltz/backlog-*.md`.

## Estado atual (texto até o Plano 2C, arquivado em 2026-10-16)
- Plano 1 (Fundação) concluído: `VoltzStudio.exe` 1.0.0, config em `%APPDATA%\Voltz Studio`, sem phone-home,
  câmera virtual própria, `voltz-core` + testes doctest, casca `VoltzShell` (Gravar com cenas/prévia/gravar/pausar)
  sobre `OBSBasic` oculta. Patches no OBS listados em `docs/voltz/upstream-patches.md`.
- Plano 2A concluído: tela Gravar completa (cenas por UUID, fontes, áudio, erros de gravação visíveis), textos
  pt-BR/en-US (`frontend/data/locale/voltz-*.ini`), ícones Phosphor, DM Sans, barra de título escura.
- Plano 2B concluído: Presets (4, com primeira execução/Pular), Projetos (abrir/mostrar/lixeira), Ajustes de
  gravação (pasta/formato/qualidade) e captura (monitor/câmera/microfone); padrões HQ + codificador automático + MP4 híbrido.
- Plano 2C concluído: tema Voltz (.ovt) padrão nas janelas do OBS, bandeja/minimizar conduzem a casca, app fora da
  captura por padrão (Ajustes › Captura), confirmação de saída e diálogo de falha próprios, grupos e acessibilidade em
  Fontes, acabamento de Áudio/Projetos/Ajustes. Plano 2 fechado; pendências em `docs/voltz/backlog-plano-2.md`.

## Plano 3A (2026-10-16)
- Toda gravação vira pasta de projeto `AAAA-MM-DD HHhMM · Nome` (programa.mp4 com 3 faixas e capítulos,
  projeto.voltz v1, marcadores.json, cursor.jsonl); Projetos lista/renomeia/recupera; barra inferior com nome do
  projeto, Marcador, espaço e avisos de disco. Sem patch no OBS. 91 casos de teste + `tools/voltz-smoke-project.ps1`.
- Revisão final: taxa abaixo de 0,5 MB/min em KB/min; duração "—" para manifesto de formato mais novo; "Renomear" em
  Projetos não usa a linha da lista depois do diálogo modal. Feito, limites e testes manuais pendentes em
  `docs/voltz/backlog-plano-3.md`.

## Planos 1–2 e 3A (resumo que saiu do CLAUDE.md, 2026-10-16)
- Planos 1–2 concluídos: `VoltzStudio.exe` 1.0.0 (config em `%APPDATA%\Voltz Studio`, sem phone-home), casca
  `VoltzShell` sobre `OBSBasic` oculta com Gravar/Projetos/Presets/Ajustes, tema Voltz, bandeja. Relato em
  `docs/HISTORICO.md`; pendências em `docs/voltz/backlog-plano-2.md`; patches no OBS em `docs/voltz/upstream-patches.md`.
- Plano 3A concluído: toda gravação vira pasta de projeto (programa.mp4 com 3 faixas e capítulos, projeto.voltz v1,
  marcadores.json, cursor.jsonl), Projetos lista/recupera, barra inferior com nome do projeto, Marcador, espaço e
  avisos de disco. Testes: 91 casos + `tools/voltz-smoke-project.ps1`. Pendências em `docs/voltz/backlog-plano-3.md`.

## Plano 3B (2026-10-16)
- ISO `tela.mkv`/`camera.mkv` por `IsoTrack` (canvas privado do libobs, fonte compartilhada do preset no canal 0,
  resolução nativa, fps do programa) → AMF/NVENC/QSV (HEVC antes de H.264, CQP 14/20) + AAC da faixa 1 →
  `ffmpeg_muxer`. Manifesto com `arquivos.tela`/`camera`, `offset_ms`, `iso_falhou` com `motivo`; regra de sobrecarga
  por janelas de 5 s; faixa âmbar na tela Gravar; "Gravar só a versão montada". 107 casos de teste; smoke de projeto
  com ISO e `-ProgramOnly`. Sem patch no OBS.
- Revisão final: falha de ISO enfileirada conferida pela geração do projeto; smokes antigos restauram a config em
  `try/finally`; smoke de projeto com a "Tela teste" (item desativado) e `cursor.jsonl` obrigatório de novo. Medida de
  taxa com clipe sintético: tela 1440p 214,8 MB/min, câmera 720p 60,4 MB/min (estimativa do núcleo ~2,2× abaixo).
  Feito, limites e testes manuais pendentes em `docs/voltz/backlog-plano-3.md`.

## Estado até o Plano 3B (texto que saiu do CLAUDE.md, 2026-10-16)
- Planos 1–3A concluídos (fundação, casca completa, presets, tema, gravação em projeto com programa.mp4/manifesto/
  marcadores/cursor/Projetos/avisos de disco); relato em `docs/HISTORICO.md`, pendências do 2 em `docs/voltz/backlog-plano-2.md`. Patches no OBS:
  `docs/voltz/upstream-patches.md`.
- Plano 3B concluído: ISO tela.mkv/camera.mkv por canvas privado (fonte de papel do preset, resolução nativa, HEVC/H.264
  AMF→NVENC→QSV em CQP, áudio-guia), offset_ms, iso_falhou com motivo, desligamento por sobrecarga, "Gravar só a versão
  montada". Testes: 107 casos + smoke de projeto com ISO. Pendências: `docs/voltz/backlog-plano-3.md`.

## Decisões e armadilha condensadas na poda do Plano 4A (texto original do CLAUDE.md, 2026-10-16)
- Gravação em projeto sem patch no OBS: no RECORDING_STARTING o ProjectRecorder troca temporariamente a pasta e
  Output/FilenameFormatting=programa (ProfileOverride) e desfaz no próximo giro do laço; 3 faixas no modo Simples
  (RecTracks=7 + mixers das fontes globais, só se ainda padrão). Manifesto em nlohmann `ordered_json` (preserva campos
  desconhecidos). Padrões de projeto uma vez por perfil (`[Voltz] ProjectDefaults=true`).
- ISO sem patch: IsoTrack = obs_canvas_create_private (EPHEMERAL, canal 0 = fonte compartilhada do preset) → encoder
  próprio + AAC da faixa 1 → ffmpeg_muxer; fps = do programa; programa segue em H.264, ISO em HEVC; testes só com
  fontes substitutas via VOLTZ_TEST_ISO_SCREEN/CAMERA.
- Testes de ISO nunca com `monitor_capture`/webcam real; a "Tela teste" do cursor é captura de monitor com o item de
  cena desativado (conferido antes de gravar).
- Fontes de áudio globais com o nome padrão do OBS ('Desktop Audio'/'Mic/Aux' ou equivalente traduzido) são renomeadas para 'Som do sistema'/'Microfone' (fica na coleção de cenas; nunca sobrescreve nome personalizado).
- O OBS cria sozinho o capítulo "Iniciar" em 0 s: `programa.mp4` tem N marcadores + 1 capítulos.

## Plano 4A (2026-10-16)
- Plugin `plugins/voltz-sources` (entra pelo `feature-voltz.cmake`, sem patch no OBS) com a fonte Voltz Mouse (halo,
  onda do clique, holofote; tamanho da captura alvo) e o canal frontend → plugin por procedimentos no proc handler
  global + `MouseFeed` com mutex.
- Mouse ao vivo sem gancho (`LiveMouse`: `GetPhysicalCursorPos` a 16 ms + Raw Input), só com Voltz Mouse mostrada;
  grupo "Tela Voltz" por cena nos presets + migração uma vez por coleção; Ajustes › Mouse com prévia ao vivo e estilos
  salvos; atalho do holofote (Ctrl+Shift+H) com eventos no manifesto; `mouse` no `projeto.voltz`.
- 126 casos de teste + `tools/voltz-smoke-mouse.ps1`. Revisão final: a contagem de Voltz Mouse mostradas vazava ao
  apagar a fonte/trocar de coleção (corrigido no `destroy`). Pendências em `docs/voltz/backlog-plano-4.md`.
