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
- Smoke novo `tools/voltz-smoke-project.ps1` (ffprobe: 3 faixas, 3 capítulos — "Iniciar" + 2 marcadores —, duração).
- Revisão final: taxa medida abaixo de 0,5 MB/min aparece em KB/min (antes "~0 MB/min"); projeto de formato mais
  novo mostra "—" também na duração; "Renomear" em Projetos não usa mais a linha da lista depois do diálogo (a lista
  pode ser recriada se a gravação começar/parar com o diálogo aberto).

## Plano 3B (próximo)
- ISO `tela.mkv`/`camera.mkv` via `obs_canvas` (spec §5.3), só a fonte de captura, resolução nativa.
- Cadeia de encoders AMF → NVENC → QSV → x264 para 3 sessões (§5.4); sem GPU para 3 → só programa + aviso +
  evento.
- `offset_ms` do primeiro quadro de cada ISO (§5.5); `arquivos.tela.origem` (mesmo formato do `arquivos.cursor`);
  eventos `iso_falhou` com `faixa`.

Depois: Plano 4 (anotações `anotacoes.jsonl` + destaque do mouse), Plano 5 (marca visual, instalador, CI).

## Pendências e limites do 3A
- Cursor em **captura de janela** (evento `janela` com o retângulo; hoje: sem `cursor.jsonl` +
  `cursor_indisponivel`); **trocar de monitor no meio da gravação** (o cursor continua mapeado para o monitor do
  início); rolagem horizontal; teclas/atalhos (opcional).
- Marcador pelo atalho do próprio OBS ("Adicionar marcador de capítulo") ou `CreateRecordChapter` do websocket vira
  capítulo mas não entra no `marcadores.json`. O OBS cria sozinho o capítulo "Iniciar" em 0 s: o `programa.mp4` tem
  N marcadores + 1 capítulos (o editor deve ignorar o "Iniciar").
- Fontes de áudio de **cena** (não globais) continuam em todas as faixas (entram em "mic" e "sistema").
- Modo de saída Avançado: `AdvOut/RecTracks` não é ajustado; divisão automática de arquivo (`RecSplitFile`) geraria
  vários `programa*`.
- Formato MKV/MOV escolhido pelo usuário: programa vira `programa.mkv`/`.mov`, sem capítulos no MKV.
- Renomear projeto troca só o `nome` do manifesto (a pasta mantém o nome).
- Falha síncrona ao iniciar (`inicio_falhou`) não foi exercitada por teste automático.
- **Pasta de gravação inválida:** o OBS avisa só por balão da bandeja (`GetOutputFilename`, `OBSApp.cpp`); a casca
  não mostra nada. (veio do backlog do Plano 2)
- **Pouco disco no início:** com < 1 GB livre o app começa a gravar e para em ~3 ms (pasta de projeto quase vazia,
  evento `disco_cheio`). Melhor recusar o início e avisar (na casca antes do `startRecording`; para as outras origens
  só com patch ou parando no STARTED).
- **Corrida no `ProfileOverride`:** um `SetProfileParameter` do websocket na mesma chave (pasta ou
  `FilenameFormatting`) entre o `RECORDING_STARTING` e o próximo giro do laço é desfeito pela restauração.
- **Perfis existentes** mudam uma vez (`[Voltz] ProjectDefaults=true` no perfil): pasta para `Vídeos\Voltz Studio`
  se ainda no padrão do OBS e `RecTracks` 1→7. Usuário que voltou ao padrão do OBS antes do 3A vê a troca.
- **Código de erro do `stop`:** o OBS liga o sinal `stop` da saída antes do `ProjectRecorder`; em tese a thread da
  UI pode processar o `RecordingStop` (e o `STOPPED`) antes de o nosso callback guardar o código → projeto com erro
  fechado como `ok`. Janela de microssegundos; resolver lendo o código pelo próprio evento quando houver API.
- **`CursorTracker::stop`:** o `join` da thread do gancho travaria se o `WM_QUIT` não chegasse (o envio é repetido
  200× por 1 s). Risco teórico; um `join` com prazo + `TerminateThread` não vale o custo hoje.
- **Manifesto ao reescrever:** campos desconhecidos dentro de `pausas[]`, `eventos[]`, `arquivos.programa` e
  `arquivos.cursor` não são preservados (só os de nível de cima e outros filhos de `arquivos`). O 3B/Plano 4 devem
  ler/escrever os campos novos pelo modelo.
- **Outra instância (`--multi`)** gravando na mesma pasta: o projeto dela aparece como "Recuperável" nesta.
- **Toast do marcador** desloca os botões centrais da barra ~6 px (stretch dos dois lados); fixar a largura do
  rótulo ou tirá-lo do fluxo.
- **Projetos › "Pasta:"** com caminho longo é cortado à direita sem reticências (Ajustes já usa reticências no meio
  com dica do caminho completo); reaproveitar o mesmo rótulo.

## Testes manuais pendentes do 3A
- Gravar ~30 s mexendo o mouse rápido: o mouse não fica lento e o log traz `0 evento(s) descartado(s)`.
- Cliques esquerdo/direito/meio e rolagem aparecem no `cursor.jsonl` com coordenadas certas, em monitor 125%/150% e
  em monitor à esquerda do principal; mouse em outro monitor → `fora`.
- Renomear projeto; mover projeto inteiro para a lixeira pela tela Projetos (se `QFile::moveToTrash` falhar com
  pasta, aparece "Não foi possível mover…"); "Mostrar na pasta" e "Abrir" em Projetos.
- Combo do nome do projeto: lista dos 8 nomes recentes; confirmar o nome com Enter e com perda de foco.
- Atalho Ctrl+Shift+M real durante a gravação (marcador + capítulo) e trocar o atalho em Configurações → Atalhos.
- Recuperação depois de um encerramento real do processo gravando (finalizar pelo Gerenciador de Tarefas): o
  `programa.mp4` híbrido toca, o projeto aparece "Recuperável" e o botão "Recuperar" funciona.
- Encher o disco de verdade (pendrive pequeno) → aviso âmbar e parada limpa.
