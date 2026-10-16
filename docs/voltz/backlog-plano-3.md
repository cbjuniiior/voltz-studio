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

## Feito no Plano 3B
- **ISO `tela.mkv`/`camera.mkv`** (spec §5.3) por `IsoTrack`: canvas privado do libobs (`obs_canvas_create_private`,
  `EPHEMERAL`, canal 0 = fonte compartilhada do preset) na resolução da fonte (par) e no fps do programa → encoder
  próprio + AAC 160 kbps da faixa 1 → `ffmpeg_muxer` (MKV). Sem patch no OBS. Trocar de cena não afeta o ISO.
- **Encoders** (núcleo `iso-encoders`): AMF → NVENC → QSV, HEVC antes de H.264 em cada família, CQP 14/20 (acima do
  programa), GOP 2 s; cada faixa tenta a cadeia inteira; sem GPU → sem ISO + aviso. Programa continua em H.264.
- **Manifesto:** `arquivos.tela`/`arquivos.camera` (`arquivo`, `largura`, `altura`, `fps`, `offset_ms`, `codificador`,
  `origem` na tela quando é captura de monitor); eventos `iso_falhou` com `faixa` e `motivo` (`sem_imagem`, `canvas`,
  `sem_codificador`, `saida`, `erro_saida`, `sobrecarga`, `arquivo_ausente`); campos desconhecidos dentro de
  `arquivos.*` preservados ao reescrever.
- **Ciclo:** início no `RECORDING_STARTED` (`offset_ms` ≥ 0, sinal `start` do ISO − `t0`), pausa junto, parada pedida
  no `STOPPING` e esperada (5 s, depois força) no `STOPPED`/`EXIT`; ISO que para sozinho não para o programa; regra de
  sobrecarga (`IsoOverloadMonitor`: duas janelas de 5 s seguidas com excesso do codificador ≥ 5% ou atraso de render ≥ 20%) desliga os ISO.
- **Casca:** faixa âmbar "Gravando só a versão montada"/"Uma faixa separada não está sendo gravada"; Ajustes ›
  Gravação › "Gravar só a versão montada" (`user.ini [Voltz] ProgramOnly`). Taxa de disco medida soma os ISO; a
  estimativa inclui os ISO das fontes compartilhadas.
- **Recuperação:** ISO sem arquivo sai do manifesto (`arquivo_ausente`); duração pelo maior mtime (programa ou ISO).
- Testes: 107 casos; `tools/voltz-smoke-project.ps1` valida `tela.mkv`/`camera.mkv` por `ffprobe` (resolução, áudio,
  duração + offset ≈ programa) e o modo `-ProgramOnly`; fontes de papel de teste por `VOLTZ_TEST_ISO_SCREEN`/
  `VOLTZ_TEST_ISO_CAMERA`.
- Spike 3B: ISO via `obs_canvas_create_private` OK (sem view); AMF HEVC 2560×1440 + 1280×720 + programa H.264 720p30
  na RX 6650 XT: 0% de quadros pulados em condição limpa; o ruído de renderização do ambiente (0 a 10%, também sem
  ISO) domina a medida; preset `quality` mantido; offsets de 50 a 170 ms (inits AMF em sequência); pausa alinhada
  (~2 ms); 0 vazamentos; tamanhos ~220 MB/min tela 1440p CQP14, ~60 MB/min câmera 720p.
- **Regra de sobrecarga ajustada pelo spike:** a regra original (≥ 5% atrasados ou pulados) desligaria os ISO por
  engano — o atraso de render é contado também nos "pulados" do codificador. Vale: excesso do codificador
  (`Δpulados − Δatrasados`) ≥ 5% ou atraso de render ≥ 20% em duas janelas de 5 s seguidas.
- Observação: o programa limpo após "Pular" é 1280×720@30 (base 1920×1080, padrão do OBS).
- **Revisão final:** falha de ISO enfileirada (sinal `stop` → thread da UI) conferida pela geração do projeto — não
  cai mais no projeto seguinte; `voltz-smoke.ps1`/`voltz-smoke-record.ps1` restauram a config portátil em
  `try/finally`; o smoke de projeto voltou a ter a "Tela teste" (`monitor_capture` com o item de cena **desativado**,
  conferido por `GetSceneItemEnabled`, nunca fonte de papel) e volta a exigir `cursor.jsonl` válido.
- **Taxa medida na Task 9** (clipe sintético `testsrc2`, 30 s, AMF HEVC CQP 14): tela 2560×1440 **214,8 MB/min**,
  câmera 1280×720 **60,4 MB/min**, programa H.264 720p30 67,2 MB/min. O núcleo estima (0,12 bit/pixel + áudio) ~100,7,
  ~26,1 e ~29,2 MB/min: ~2,2× abaixo nesse conteúdo de alta entropia, igual para programa e ISO (ver pendências).

## Próximo
Plano 4 (anotações `anotacoes.jsonl` + destaque do mouse; o grupo `Tela Voltz` não pode mudar a fonte do ISO),
Plano 5 (marca visual, instalador, CI).

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
- **Manifesto ao reescrever:** campos desconhecidos dentro de `pausas[]` e `eventos[]` ainda não são preservados (os
  de `arquivos.*` passaram a ser no 3B).
- **Outra instância (`--multi`)** gravando na mesma pasta: o projeto dela aparece como "Recuperável" nesta.
- **Toast do marcador** desloca os botões centrais da barra ~6 px (stretch dos dois lados); fixar a largura do
  rótulo ou tirá-lo do fluxo.
- **Projetos › "Pasta:"** com caminho longo é cortado à direita sem reticências (Ajustes já usa reticências no meio
  com dica do caminho completo); reaproveitar o mesmo rótulo.

## Pendências e limites do 3B
- **`camera_desconectada`** (spec §8): sem detecção (o `dshow_input` não avisa); exige heurística por quadros parados e
  teste com webcam real.
- **Câmera no fps nativo:** o ISO grava no fps do programa (o libobs força o fps do principal nos mixes); dividir com
  `obs_encoder_set_frame_rate_divisor` quando o fps real da câmera for conhecido.
- **Programa em HEVC** (spec §5.4): mantido H.264 por compatibilidade de reprodução; virar opção em Ajustes.
- **ISO sem GPU (x264):** hoje desligado com aviso; `isoEncoderChain(…, allowSoftware=true)` já existe.
- **Fonte que muda de tamanho** durante a gravação: o canvas fica no tamanho do início. **Fonte acima do limite do
  codificador** (ex. 5120×1440 em H.264): sem ISO (`sem_codificador`); reduzir escala.
- **`offset_ms`** tem erro de até ~1 quadro (sinal `start` de cada saída); o editor pode refinar pelo áudio-guia (faixa
  1) que vai em cada ISO.
- Tela ISO só da fonte **compartilhada** do preset: sem preset não há `tela.mkv`, mesmo com captura de monitor na cena.
- Ao destruir o canvas privado, o obs-websocket emite `CanvasRemoved` de um canvas que nunca listou (cosmético).
- MKV de ISO após queda do app não tem duração no cabeçalho (o editor deve medir).
- **`finishIsoTracks` bloqueia a thread da UI** até 5 s por faixa no pior caso (+ 2 s depois de forçar). Medido:
  ~0,56 s no desligamento por sobrecarga (a parada é pedida ali mesmo) e ~40 ms nas paradas normais (a parada já foi
  pedida no `STOPPING`). Mover a espera para fora da thread da UI se aparecer travamento.
- **Estimativa de disco:** o núcleo usa 0,12 bit/pixel para programa e ISO; a medida com clipe sintético deu ~2,2× mais
  (ver "Taxa medida na Task 9"). Calibrar com gravações reais (tela parada vs movimento) antes de mexer no fator — o
  aviso de disco usa a taxa **medida** depois de 10 s, então a estimativa só afeta a previsão antes de gravar.
- `VOLTZ_TEST_ISO_OVERLOAD` (soma 20% de "pulados" fictícios) é variável só de teste e fica no código de produção,
  como `VOLTZ_FAKE_FREE_BYTES` e `VOLTZ_TEST_ISO_SCREEN`/`CAMERA`; avaliar um guarda de compilação para testes.
- Aviso cosmético do FFmpeg `2 frames left in the queue on closing` ao fechar o `ffmpeg_muxer` (aparece também no
  programa; os arquivos fecham íntegros).
- A versão "baseado no OBS Studio" em Ajustes › Sobre vem do `git describe` da configuração (`32.2.2-65-g…-modified`);
  fixar a string de versão para builds de distribuição (Plano 5).

## Testes manuais pendentes do 3B
- Gravar com o preset Tutorial **real** (monitor + webcam) e conferir `tela.mkv`/`camera.mkv`: resolução nativa,
  `arquivos.tela.origem` igual ao `arquivos.cursor.origem`, e a sincronia vista pelo próprio usuário num player/editor.
- Carga real com 3 encodes AMF (tela 1440p/4K em movimento + webcam): log sem excesso de `skipped frames due to encoding lag` (descontado o ruído de render do ambiente) e
  sem `iso_falhou`/`sobrecarga` (a sobrecarga só dispara com duas janelas ruins seguidas, ver Decisões).
- Faixa âmbar com a GPU **realmente** sobrecarregada (ex. jogo pesado aberto): `iso_falhou`/`sobrecarga` e o
  programa segue.
- Desconectar a webcam (tirar do USB) durante a gravação: o programa segue; anotar o que acontece com `camera.mkv`.
- "Gravar só a versão montada" com o preset real: nenhum `.mkv`, estimativa de espaço menor na barra inferior; o
  checkbox também no modo de saída Avançado.
- Textos novos (faixa âmbar, motivos, Ajustes) em en-US.

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
