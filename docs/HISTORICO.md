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
