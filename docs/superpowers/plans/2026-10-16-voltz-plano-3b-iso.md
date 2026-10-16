# Voltz Studio — Plano 3B: faixas ISO (tela.mkv e camera.mkv) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Toda gravação em projeto passa a gravar, além do `programa.mp4`, as faixas cruas `tela.mkv` e `camera.mkv`
(spec §5.3) — uma por fonte de papel Tela/Câmera, na resolução nativa da fonte, só a fonte de captura, imune à troca de
cena —, com cadeia de encoders AMF → NVENC → QSV (HEVC antes de H.264, CQP acima da qualidade do programa), `offset_ms`
do primeiro quadro relativo a `t0_ns`, `arquivos.tela`/`arquivos.camera` no manifesto, eventos `iso_falhou` com `faixa`
e `motivo`, pausa junto com o programa, desligamento com aviso quando a GPU não dá conta, recuperação ciente dos ISO e o
ajuste "Gravar só a versão montada".

**Architecture:** Lógica pura e testada no núcleo (`frontend/voltz/core/`): cadeia/configuração dos encoders do ISO,
qualidade, regra de sobrecarga (`iso-encoders`) e o modelo do manifesto com `tela`/`camera`, `motivo` nos eventos e
fusão com campos desconhecidos dentro de `arquivos.*` (`project-manifest`). Em `frontend/voltz/project/`, a classe nova
`IsoTrack` encapsula **um** ISO: canvas privado do libobs (`obs_canvas_create_private`) com a fonte no canal 0, na
resolução da fonte → encoder de vídeo próprio + AAC da faixa 1 → `ffmpeg_muxer` (MKV). O `ProjectRecorder` (que já
conduz o projeto pelos eventos do frontend) cria um `IsoTrack` por fonte de papel no `RECORDING_STARTED`, pausa/retoma
com o programa, pede a parada no `RECORDING_STOPPING` e fecha no `STOPPED`/`EXIT`. A casca ganha só a faixa âmbar de
aviso na tela Gravar e o checkbox em Ajustes. **Nenhum patch em arquivo do OBS.**

**Tech Stack:** C++17, Qt 6.11.1, libobs/frontend 32.2.2 (`obs_canvas_*`, `obs_video_encoder_create`,
`obs_audio_encoder_create`, `obs_output_*`, plugin `ffmpeg_muxer`), nlohmann_json 3.11, doctest. Testes ao vivo por
obs-websocket (`tools/voltz-ws.ps1`), UI Automation (`tools/voltz-uia.ps1`), `ffprobe` (só metadados) e captura **só da
janela** do app.

**Spec:** `docs/superpowers/specs/2026-10-15-voltz-studio-base-gravacao-design.md` §4.3, §5.1–§5.5, §8, §9, §11 (risco
2 e 3) · **Backlog:** `docs/voltz/backlog-plano-3.md` (seção "Plano 3B (próximo)" e "Pendências e limites do 3A") ·
**Modelo de formato:** `docs/superpowers/plans/2026-10-16-voltz-plano-3a-projeto.md`.

**Base deste plano:** commit `a24c10c2d` (branch `voltz`, publicado). Contagem de testes na base: **91** casos.

**Fora do 3B (vai para `docs/voltz/backlog-plano-3.md` na Task 9), com o porquê:**
- **`camera_desconectada`** (spec §8): o `dshow_input` não emite sinal de desconexão; detectar exige heurística por
  quadros parados e validar exige ligar/desligar uma webcam real — proibido nos testes automáticos deste plano.
- **Programa em HEVC** (spec §5.4 pede HEVC "se disponível" também no programa): o `programa.mp4` continua no H.264 do
  modo Simples. É o arquivo que o usuário abre direto, e o Windows sem a extensão HEVC (paga na Store) não o reproduz.
  O ISO (insumo do editor, lido pelo FFmpeg) vai em HEVC.
- **ISO em x264** (máquina sem GPU): ISO desligado com aviso. Três encodes em CPU competiriam com o programa (quadros
  perdidos na versão montada). A cadeia do núcleo já aceita `allowSoftware` para um ajuste futuro.
- **Câmera no fps nativo** (o exemplo da spec mostra `camera.mkv` a 30 fps): o libobs força o fps do canvas principal em
  todo mix auxiliar (`libobs/obs.c:615-622`); dá para dividir com `obs_encoder_set_frame_rate_divisor`, mas o fps real
  do `dshow_input` não é confiável (resolução/fps "preferidos" do dispositivo). O ISO grava no fps do programa e o
  manifesto registra o fps real.
- **Fonte que muda de tamanho no meio** (câmera troca de resolução, monitor muda de resolução) e **fonte maior que o
  limite do codificador** (ex. 5120×1440 em H.264): o canvas fica no tamanho do início; acima do limite o encoder não
  inicia e a faixa fica fora (`sem_codificador`).
- **Grupo `Tela Voltz` com as camadas Voltz** (spec §4.4, risco 3): é do Plano 4. Hoje a fonte de papel Tela é a
  própria captura compartilhada, então o ISO já sai "só a fonte de captura". O Plano 4 deve manter o ISO apontando para
  a captura, não para o grupo.
- Preservar campos desconhecidos **dentro de** `pausas[]`/`eventos[]` (os de `arquivos.*` passam a ser preservados aqui).

**Descobertas que decidem o desenho (conferidas no código 32.2.2):**
- **Canvas privado:** `obs_canvas_create_private(name, &ovi, flags)` (`libobs/obs-canvas.c:200-204`) cria o mix
  (`obs_create_video_mix`) e o coloca em `obs->video.mixes`; por ser privado **não** entra em `named_canvases`
  (`obs-canvas.c:176-179`), então não aparece em `obs_enum_canvases` (`libobs/obs.c:1956-1970`), no obs-websocket
  (`GetCanvasList`) nem no salvamento da coleção (`frontend/utility/OBSCanvas.cpp:54`). `obs_canvas_set_channel(canvas,
  0, fonte)` (`obs-canvas.c:431-476`) só referencia a fonte no canal: a fonte continua pertencendo ao canvas principal e
  às cenas — trocar de cena não afeta o ISO. Sem a flag `ACTIVATE` a view é `AUX_VIEW` (a fonte é "mostrada", como numa
  projeção; quem a ativa é o programa). `EPHEMERAL` impede `obs_save_canvas`.
- **Encoder num canvas:** precedente no próprio frontend — `obs_encoder_set_video(video_encoder,
  obs_canvas_get_video(canvas))` (`frontend/utility/MultitrackVideoOutput.cpp:305`). Encoders de textura (AMF/NVENC/QSV)
  funcionam por mix: `gpu_encode_available` olha o mix do `video_t` do encoder (`libobs/obs-encoder.c:219-226`).
- **fps do mix auxiliar = fps do principal:** `obs_init_video_mix` sobrescreve `fps_num/fps_den` com os do canvas
  principal (`libobs/obs.c:615-622`, "main view graphics thread drives all frame output").
- **Mix só renderiza com encoder ativo** (`update_active_state`, `libobs/obs-video.c:1054-1074`); mixes de views
  diferentes não reaproveitam textura (`can_reuse_mix_texture`, `obs-video.c:136-154`) ⇒ custo de GPU só durante a
  gravação.
- **Destruir o canvas libera o mix na hora** (`obs_canvas_destroy` → `obs_canvas_clear_mix`, `obs-canvas.c:206-232` e
  `266-283`) ⇒ ordem obrigatória: parar a saída → soltar saída → soltar encoders → soltar o canvas.
  `obs_output_destroy` espera o fim da captura (`libobs/obs-output.c:291-303`).
- **Não chamar `obs_canvas_remove` no canvas privado:** dispara o sinal global `canvas_remove`, que o `OBSBasic`
  enfileira em `RemoveCanvas` segurando uma referência (`frontend/widgets/OBSBasic.cpp:956`,
  `OBSBasic_Canvases.cpp:20-24`). Basta soltar a referência. (Ao destruir, o obs-websocket ainda emite um evento
  `CanvasRemoved` pelo `canvas_destroy`, `plugins/obs-websocket/src/eventhandler/EventHandler.cpp:620-629` — cosmético,
  vai para o backlog.)
- **`ffmpeg_muxer` exige áudio:** flags `OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_MULTI_TRACK |
  OBS_OUTPUT_CAN_PAUSE` (`plugins/obs-ffmpeg/obs-ffmpeg-mux.c:864-875`) e `can_begin_data_capture` recusa saída de
  vídeo sem encoder de áudio (`obs-output.c:1353-1365`); `mp4_output`/`flv_output` também são `OBS_OUTPUT_AV`. ⇒ cada ISO
  leva um AAC (`ffmpeg_aac`, `plugins/obs-ffmpeg/obs-ffmpeg-audio-encoders.c:466`) da faixa 1 (mix): serve de referência
  de sincronia para o editor. O muxer escolhe o contêiner pela extensão do `path` (`.mkv`).
- **Falha de encoder é síncrona:** `ffmpeg_mux_start` chama `obs_output_can_begin_data_capture` e
  `obs_output_initialize_encoders` antes de abrir o processo (`obs-ffmpeg-mux.c:380-443`); `obs_output_start` devolve
  falso e **não** emite `stop` (`obs-output.c:396-417`) ⇒ dá para tentar o próximo encoder da cadeia na hora (é assim que
  "a GPU não sustenta mais uma sessão" aparece). O sinal `start` sai dentro do `obs_output_start`, na mesma thread.
- **Pausa alinhada ao quadro:** `obs_output_pause` usa o timestamp de vídeo mais próximo de cada encoder
  (`obs-output.c:705-847`); todos os mixes recebem os quadros com o mesmo `video_time` ⇒ pausar o ISO logo depois do
  programa cai no mesmo limite de quadro (±1). O OBS loga `output <nome> paused`/`unpaused`.
- **Eventos do frontend:** `RECORDING_STOPPING` vem do sinal `stopping` da saída do programa
  (`frontend/widgets/OBSBasic_Recording.cpp:138-149`); `RECORDING_PAUSED`/`UNPAUSED` depois do `obs_output_pause` do
  programa (`OBSBasic_Recording.cpp:296-358`). `obs_output_stop` de uma saída emite `stopping` e para no `os_gettime_ns`
  da chamada (`obs-output.c:530-549`).
- **IDs dos encoders (registrados pelos plugins, confirmados):** AMD `h265_texture_amf` e `h264_texture_amf`
  (`plugins/obs-ffmpeg/texture-amf.cpp:2213` e `:1802`); NVIDIA `obs_nvenc_hevc_tex`/`obs_nvenc_h264_tex`
  (`plugins/obs-nvenc/nvenc.c:1426`/`:1404`) e os de compatibilidade `ffmpeg_hevc_nvenc`/`ffmpeg_nvenc`
  (`nvenc-compat.c:403`/`:400`); Intel `obs_qsv11_hevc`, `obs_qsv11_v2` e `obs_qsv11` (`plugins/obs-qsv11/obs-qsv11.c:1393`,
  `:1327`, `:1293`); `obs_x264` (`plugins/obs-x264/obs-x264.c:851`). AMF/NVENC/QSV só se registram se o hardware existe;
  esta máquina tem **AMD RX 6650 XT** (AMF sim, NVENC não).
- **Chaves de configuração:** AMF `rate_control`=`CQP`, `cqp`, `preset` (`quality|balanced|speed`), `keyint_sec`
  (GOP = keyint×fps, `texture-amf.cpp:1988-1991`), `profile` (no HEVC o próprio AMF escolhe `main`/`main10`,
  `texture-amf.cpp:2086`); NVENC/QSV `rate_control`=`CQP`, `cqp`, `profile`, `keyint_sec`
  (`plugins/obs-nvenc/nvenc-properties.c:41-61`, `obs-qsv11.c:165-181`); x264 `rate_control`=`CRF`, `crf`, `preset`. O
  modo Simples grava "Alta" em CQP `CalcCRF(16)` = 16 a partir de 1080p e "Arquivo menor" em 23
  (`frontend/utility/SimpleOutput.cpp:416-432`, `524-537`).
- **Log do libobs para validar:** `Output 'voltz-iso-tela': Total frames output: N` (`obs-output.c:442-468`),
  `[ffmpeg muxer: 'voltz-iso-tela'] Writing file '...'` (`obs-ffmpeg-mux.c:441`) e, quando há atraso,
  `Video stopped, number of skipped frames due to encoding lag: a/b (x%)` (`libobs/media-io/video-io.c:451-462`). No
  fim do app, `Number of memory leaks: N` (`frontend/obs-main.cpp:1104`).
- **Alternativa se o canvas falhar (já especificada na Task 1, Step 8):** `obs_view_create` + `obs_view_set_source(view,
  0, fonte)` + `obs_view_add2(view, &ovi)` → `video_t` próprio; desmontagem `obs_view_remove` → `obs_view_set_source(view,
  0, nullptr)` → `obs_view_destroy` (`libobs/obs-view.c:150-181`). É exatamente o padrão da câmera virtual do OBS para
  fonte única (`frontend/utility/BasicOutputHandler.cpp:260-267` e `:380-385`) e é o que o plugin "Source Record" usa.
  Usa a mesma infraestrutura de mix (`obs_create_video_mix`), mas sem o objeto canvas (sem contexto/UUID/sinais globais).
  Se até ela falhar, o problema está no mix/encoder ⇒ parar e reportar (BLOCKED), sem patch.
- **Câmera de teste sem webcam:** `dshow_input` com `video_device_id` sem `:` falha em `DecodeDeviceDStr`
  (`plugins/win-dshow/encode-dstr.hpp:41-44`) antes de enumerar/abrir dispositivo (`win-dshow.cpp:879-891`) ⇒ fonte
  0×0 sem tocar na webcam. Para "câmera com imagem" os testes usam `color_source_v3` (ou, no spike, `ffmpeg_source` com
  um clipe sintético `testsrc2` gerado pelo `ffmpeg`).

---

## Decisões técnicas (e o porquê)

| Tema | Decisão | Por quê |
|---|---|---|
| Caminho do ISO | `IsoTrack`: `obs_canvas_create_private("voltz-iso-<faixa>", ovi, EPHEMERAL)` + canal 0 = fonte → `obs_video_encoder_create` → `ffmpeg_muxer` (`path` = `tela.mkv`/`camera.mkv`) | Spec §5.3; API pública; invisível ao websocket e à coleção; sem patch. Spike na Task 1; alternativa `obs_view` especificada. |
| Áudio no ISO | AAC 160 kbps da faixa 1 (mix) em cada ISO | O `ffmpeg_muxer` exige áudio; de quebra o editor ganha uma referência de sincronia por correlação de áudio. |
| Resolução / fps | Tamanho da fonte no início, arredondado para baixo a par; fps = do programa | NV12/P010 pedem lados pares; o libobs força o fps do principal nos mixes. O manifesto registra o real. |
| Fontes de papel | Só as **compartilhadas** do preset (`user.ini [Voltz] ScreenSourceUuid`/`CameraSourceUuid`); sem fonte → sem arquivo, sem evento | "Fonte com papel" da spec. Nunca gravar uma captura que o usuário não marcou (o cursor do 3A cai na primeira captura de monitor; o ISO **não**). |
| Testes do ISO | `VOLTZ_TEST_ISO_SCREEN` / `VOLTZ_TEST_ISO_CAMERA` (nome de fonte) trocam as fontes de papel; o código recusa `monitor_capture`/`window_capture`/`game_capture` | Mesmo padrão do `VOLTZ_FAKE_FREE_BYTES`; permite testar sem gravar a tela nem ligar a webcam do usuário. |
| Cadeia de encoders | AMF → NVENC → QSV, em cada família HEVC antes de H.264; x264 **fora** do ISO; cada faixa tenta a cadeia inteira | Spec §5.4. Encoder que não inicia (sessão esgotada) cai no próximo; nenhum serve → a faixa fica fora com `sem_codificador`. |
| Qualidade | CQP 14 (Alta) / 20 (Arquivo menor); AMF `preset=quality`; GOP 2 s; perfil `high` no H.264, automático/`main` no HEVC | Acima do programa (16/23 no OBS); GOP curto ajuda o editor a buscar quadros. |
| Programa | Continua H.264 do modo Simples (sem mudança) | Compatibilidade de reprodução (ver "Fora do 3B"). |
| "A GPU não sustenta" | (1) encoder que não inicia → próxima opção da cadeia, senão faixa fora; (2) **sobrecarga**: na pulsação de 5 s, se ≥ 5% dos quadros do programa atrasaram na renderização ou foram pulados pelo codificador (janela ≥ 60 quadros, a primeira janela só calibra) → todos os ISO param com `iso_falhou`/`sobrecarga` | Spec §5.4 e §8: a versão montada tem prioridade. |
| Início / fim | Começa no `RECORDING_STARTED` (programa confirmado); parada graciosa pedida no `RECORDING_STOPPING`; `STOPPED`/`EXIT` esperam o `stop` de cada ISO até 5 s e então forçam | O programa nunca espera o ISO para começar; offset sempre ≥ 0; ao fechar o app gravando, os MKV são finalizados antes do libobs desligar. |
| `offset_ms` | `timeline.relativeMs(ns do sinal "start" do ISO)` — mesmo método do `t0_ns` (sinal `start` do programa) | Spec §5.5. Erro ≤ ~1 quadro (cada saída começa no próximo quadro após o gancho); o áudio-guia permite refinar no editor. |
| Pausa | `obs_output_pause` em cada ISO no `RECORDING_PAUSED`/`UNPAUSED` | Spec §5.3 "pausa aplica-se a todas as saídas"; ISO e programa ficam com a mesma duração líquida. |
| Falha durante a gravação | Sinal `stop` sem pedido nosso (erro do muxer/encoder, `StopOutput` do websocket) → `iso_falhou` com `motivo: erro_saida` (+ `codigo` se ≠ 0), faixa âmbar; o programa segue | Spec §8. |
| Manifesto | `IsoFile` (`arquivo`, `largura`, `altura`, `fps`, `offset_ms`, `codificador`, `origem` só na tela quando é captura de monitor — mesmo formato do `arquivos.cursor.origem`); `ManifestEvent.reason` → `motivo`; `arquivos.*` fundem com o JSON lido (campos desconhecidos dentro de cada arquivo sobrevivem; arquivo ausente no modelo sai do JSON) | Spec §5.2; pendência do 3A. |
| Recuperação | `ProjectLibrary::recover` tira do manifesto os ISO sem arquivo (ou vazio) com `iso_falhou`/`arquivo_ausente` e estima a duração pelo **maior** mtime entre programa e ISO | MKV escrito incrementalmente sobrevive à queda; o editor não deve abrir arquivo que não existe. |
| Ajuste | Ajustes › Gravação › "Gravar só a versão montada" (`user.ini [Voltz] ProgramOnly`, padrão desligado; travado durante a gravação) | Spec §4.3; também é o "desligar" para quem não quer a câmera em arquivo separado. |
| Disco | Taxa medida = (programa + ISO) / tempo líquido; estimativa antes de gravar soma o ISO das fontes compartilhadas com imagem | O aviso de disco fica honesto com 3 arquivos crescendo. |
| Aviso na casca | Faixa âmbar na tela Gravar: título "Gravando só a versão montada" quando nenhum ISO está gravando, senão "Uma faixa separada não está sendo gravada"; texto com faixa(s) e motivo(s) | Spec §5.4 ("gravando só a versão montada"). |

---

## Convenções

**Diretório de trabalho:** a raiz do repositório (`voltz-studio`), branch `voltz`. Leia o `CLAUDE.md` inteiro antes
(seções **Decisões** e **Armadilhas**). Base deste plano: commit `a24c10c2d`.

```bash
CMAKE="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
CTEST="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe"
CLANGFMT="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin/clang-format.exe"
```

- **CONFIGURAR:** `"$CMAKE" --preset voltz-windows-x64`
- **COMPILAR APP:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target obs-studio --parallel -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
- **COMPILAR TUDO:** `"$CMAKE" --build --preset voltz-windows-x64 --parallel -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
- **COMPILAR TESTES:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target voltz-core-tests -- -v:minimal -nologo -nodeReuse:false`
- **RODAR TESTES:** `"$CTEST" --test-dir build_x64/frontend/voltz/tests -C RelWithDebInfo --output-on-failure`
  (contagem de casos: `build_x64/frontend/voltz/tests/RelWithDebInfo/voltz-core-tests.exe` → linha
  `test cases: N | N passed`)
- **SMOKE:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1`
- **SMOKE GRAVAÇÃO:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-record.ps1`
- **SMOKE PROJETO:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-project.ps1 -OutDir "<scratchpad>\smoke-projeto"`
  (a Task 8 acrescenta `-ProgramOnly`)
- **FORMATAR:** `"$CLANGFMT" -i <arquivos .cpp/.hpp novos ou alterados em frontend/voltz/>` (nunca nos arquivos do OBS)

`<scratchpad>` = `C:\Users\Cassio\AppData\Local\Temp\claude\C--Users-Cassio-Documents-Projetos-IA-voltz-studio\eee63625-8704-4453-920b-073461dee2a9\scratchpad`.

> **Regras fixas:**
> - **Sem patch no OBS.** Se algo se mostrar impossível sem mexer em arquivo do OBS, pare e reporte; se o controlador
>   autorizar, a mudança vai dentro de `#ifdef VOLTZ_STUDIO` e é registrada em `docs/voltz/upstream-patches.md` **no
>   mesmo commit**.
> - Builds longos **em segundo plano**, nunca dois ao mesmo tempo. `MSB4018`/`ZERO_CHECK`/"Touch" no `build_x86` →
>   rode de novo. Arquivo novo (código, `data/`, ícone no `.qrc`) exige **CONFIGURAR** antes de compilar.
> - **Warnings como erro** (`/W3`) valem para tudo, inclusive `voltz-core` e testes: casts explícitos em conversões
>   numéricas; `%zu` para `size_t` no `blog`.
> - O alvo compila com `QT_NO_CONTEXTLESS_CONNECT`: todo `connect` com lambda leva objeto de contexto.
> - Sinais do libobs (saída, fonte) chegam **fora** da thread da UI → só atômicos/condição ali, e o resto via
>   `QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)`.
> - QSS da casca: toda regra com prefixo `#voltzShell`; `setObjectName` **antes** do `setStyleSheet` do pai fazer
>   efeito. Ícones no código sempre por `phosphorIcon()`.
> - Textos novos nos **dois** locales (`frontend/data/locale/voltz-pt-BR.ini` e `voltz-en-US.ini`, UTF-8 sem BOM), com
>   paridade: `diff <(cut -d= -f1 frontend/data/locale/voltz-en-US.ini | sort) <(cut -d= -f1 frontend/data/locale/voltz-pt-BR.ini | sort)` → sem saída.
> - `clang-format -i` só nos arquivos de `frontend/voltz/`.
> - **Não** commitar `build-compile.log` nem `docs/voltz/backlog-plano-*.md` fora da Task 9. **Push só na Task 9**,
>   nunca `--force`.
>
> **Regras de segurança dos testes ao vivo (obrigatórias — releia antes de cada teste):**
> - **Nunca** encerre processos que você não iniciou: só pelo PID que você lançou (`Stop-Process -Id $proc.Id`); nunca
>   `taskkill /IM`, nunca `Stop-Process -Name`. Ler a lista de processos é permitido.
> - **Nunca** capture a tela inteira — só o retângulo de uma janela do app, via `PrintWindow` (bloco abaixo).
> - **Nunca capture nem inspecione quadros** de `programa.mp4`, `tela.mkv` ou `camera.mkv` (podem conter a tela ou a
>   webcam do usuário): nada de `ffmpeg -i … -frames`, `ffplay`, miniaturas, abrir no player ou no Explorer. Valide
>   **só metadados** com `ffprobe -show_entries stream=…:format=…` (e `-show_chapters`) e **apague** os arquivos no fim
>   (o bloco apaga `$out`).
> - **ISO só com fontes substitutas:** `color_source_v3`, `ffmpeg_source` com clipe **sintético** (`testsrc2` gerado
>   pelo `ffmpeg` no scratchpad) ou `dshow_input` com `video_device_id` **inválido sem `:`** (fonte 0×0, nenhuma câmera é
>   aberta), apontadas por `VOLTZ_TEST_ISO_SCREEN`/`VOLTZ_TEST_ISO_CAMERA`. **Nunca** aponte essas variáveis para
>   `monitor_capture`/`window_capture`/`game_capture` (o código recusa, mas não conte com isso), **nunca** crie
>   `dshow_input` com um dispositivo real e **nunca aplique preset** nos testes (criaria a Tela e a Câmera reais
>   compartilhadas — o "Pular" da primeira execução não cria fontes).
> - A "Tela teste" (`monitor_capture` do 3A) só existe para o `CursorTracker`, numa cena que nunca vai ao programa, com o
>   item desativado; ela **não** é fonte de papel e não gera ISO.
> - **Nunca** mova o mouse, clique nem digite na sessão do usuário (nada de `SendInput`/`SetCursorPos`/`SendKeys`). UI
>   Automation `Invoke`/`Toggle` não move o mouse e é permitido.
> - Não abra o Explorer. **Nunca** toque no que já existe em `%USERPROFILE%\Videos` (o bloco só apaga
>   `Vídeos\Voltz Studio` se ele foi criado pelo teste e ficou vazio).
> - App sempre com `--portable --multi`, com **backup e restauração** da config portátil
>   (`build_x64/rundir/RelWithDebInfo/config`) — os smokes antigos **apagam** essa pasta.
> - Gravações de teste vão para `<scratchpad>\gravacoes` (`SetProfileParameter`) e são apagadas no fim; o áudio global
>   é **mutado** antes de gravar. `HideOBSWindowsFromCapture=false` no `user.ini` portátil para o `PrintWindow`.
> - PowerShell 5.1 lê `.ps1` sem BOM como ANSI: script com acento (ex. `-Name "Gravar só a versão montada"`) precisa ser
>   salvo **com BOM**; `Select-String` em log com caminho acentuado precisa de `-Encoding UTF8`.

**Commits:** mensagem em PT-BR terminando com a linha `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

**Bloco de teste (PowerShell 5.1)** — copie para um `.ps1` **com BOM** no scratchpad e coloque os passos da task no
`try`. Ele guarda a config portátil, cria uma limpa com obs-websocket (porta 4455, sem senha), idioma pt-BR, sem
confirmação ao sair, define as fontes de papel de teste por variável de ambiente, cria `gravacoes` no scratchpad e
restaura/apaga tudo no fim. **Valide dentro do `try`, depois de `Close-App` + `Save-Log`: o `finally` apaga `$out`.**
```powershell
$root = "C:\Users\Cassio\Documents\Projetos IA\voltz-studio"
$rundir = Join-Path $root "build_x64\rundir\RelWithDebInfo"; $bin = Join-Path $rundir "bin\64bit"
$sp = "C:\Users\Cassio\AppData\Local\Temp\claude\C--Users-Cassio-Documents-Projetos-IA-voltz-studio\eee63625-8704-4453-920b-073461dee2a9\scratchpad"
$out = Join-Path $sp "gravacoes"
$cfg = Join-Path $rundir "config"; $bak = Join-Path $rundir "config.3bbak"
if (Test-Path $bak) { throw "backup anterior em $bak - restaure (Move-Item) antes de continuar" }
if (Test-Path $cfg) { Move-Item $cfg $bak }
$voltzCfg = Join-Path $cfg "Voltz Studio"
$wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
New-Item -ItemType Directory -Force $wsDir | Out-Null
[IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}')
$programOnly = $false   # ajuste por task: $true = "Gravar só a versão montada" já ligado
$userIni = "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[BasicWindow]`r`nHideOBSWindowsFromCapture=false`r`n"
if ($programOnly) { $userIni += "[Voltz]`r`nProgramOnly=true`r`n" }
[IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), $userIni, (New-Object Text.UTF8Encoding $false))
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null
# Fontes de papel do ISO = fontes substitutas pelo nome (nunca a tela real nem a webcam)
$env:VOLTZ_TEST_ISO_SCREEN = "Tela substituta"; $env:VOLTZ_TEST_ISO_CAMERA = "Camera substituta"
$spike = $false   # só a Task 1: $true liga o gancho temporário VOLTZ_ISO_SPIKE
if ($spike) { $env:VOLTZ_ISO_SPIKE = "1" }
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"; $hadVoltzVideos = Test-Path -LiteralPath $voltzVideos
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class W { [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f); }
'@
[void][W]::SetProcessDPIAware()
function Shot([IntPtr]$h, [string]$n) { $r = New-Object W+RECT; [void][W]::GetWindowRect($h, [ref]$r)
  $b = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [System.Drawing.Graphics]::FromImage($b)
  $d = $g.GetHdc(); [void][W]::PrintWindow($h, $d, 2); $g.ReleaseHdc($d); $g.Dispose(); $b.Save("$sp\$n.png"); $b.Dispose() }
$uia = Join-Path $root "tools\voltz-uia.ps1"; $ws = Join-Path $root "tools\voltz-ws.ps1"
# Pedidos ao obs-websocket: devolve as respostas (objetos com index/requestType/ok/data); a vírgula preserva o array
function Ws([string]$json) { $f = Join-Path $sp "req.json"
  [IO.File]::WriteAllText($f, $json, (New-Object Text.UTF8Encoding $false))
  $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $f
  $lines | ForEach-Object { Write-Host $_ }
  return ,@($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json }) }
function Uia([string]$name) { & powershell -ExecutionPolicy Bypass -File $uia -ProcessId $proc.Id -Name $name | Out-Host; return ($LASTEXITCODE -eq 0) }
function Close-App { if (-not $proc.HasExited) { $proc.Refresh(); [void]$proc.CloseMainWindow(); if (-not $proc.WaitForExit(30000)) { Stop-Process -Id $proc.Id -Force } } }
function Save-Log { $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
  if ($log) { Copy-Item $log.FullName "$sp\ultimo-log.txt" -Force } }
function LogLines([string]$pattern) { if (Test-Path "$sp\ultimo-log.txt") { Select-String -LiteralPath "$sp\ultimo-log.txt" -Encoding UTF8 -Pattern $pattern | ForEach-Object { $_.Line } } }
function Project-Dirs { @(Get-ChildItem -LiteralPath $out -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz") } | Sort-Object CreationTime) }
function Read-Json([string]$path) { (Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json) | ForEach-Object { $_ } }
$ffprobe = (Get-Command ffprobe -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $ffprobe) { $ffprobe = Get-ChildItem (Join-Path $root ".deps") -Filter ffprobe.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName }
# Só metadados (nunca quadros)
function Probe([string]$file) { (& $ffprobe -v error -show_entries "stream=codec_type,codec_name,width,height,avg_frame_rate:format=duration" -of json $file) | Out-String | ConvertFrom-Json }
function Show-Project([System.IO.DirectoryInfo]$d) {
  $m = Read-Json (Join-Path $d.FullName "projeto.voltz")
  Write-Host ("== {0}: status={1} bruta={2} liquida={3} pausas={4}" -f $d.Name, $m.status, $m.duracao_ms, $m.duracao_liquida_ms, @($m.pausas).Count)
  Write-Host ("arquivos: " + ($m.arquivos | ConvertTo-Json -Depth 6 -Compress))
  Write-Host ("eventos: " + (ConvertTo-Json -InputObject @($m.eventos) -Depth 4 -Compress))
  foreach ($f in @("programa.mp4", "tela.mkv", "camera.mkv")) {
    $path = Join-Path $d.FullName $f
    if (-not (Test-Path -LiteralPath $path)) { Write-Host "$f : ausente"; continue }
    $p = Probe $path
    $streams = ($p.streams | ForEach-Object { "$($_.codec_type)/$($_.codec_name) $($_.width)x$($_.height) $($_.avg_frame_rate)" }) -join " | "
    Write-Host ("{0} : {1} | dur={2}" -f $f, $streams, $p.format.duration) }
  return $m }
$proc = Start-Process (Join-Path $bin "VoltzStudio.exe") -ArgumentList '--portable','--multi' -WorkingDirectory $bin -PassThru
Start-Sleep 15
try {
  $proc.Refresh(); $shell = $proc.MainWindowHandle   # HWND da casca
  # ... passos da task (validação no fim: Close-App; Save-Log; Show-Project ...) ...
} finally {
  Close-App
  Save-Log
  if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
  if (Test-Path $bak) { Move-Item $bak $cfg }
  Remove-Item Env:VOLTZ_TEST_ISO_SCREEN, Env:VOLTZ_TEST_ISO_CAMERA, Env:VOLTZ_ISO_SPIKE -ErrorAction SilentlyContinue
  if (Test-Path $out) { Remove-Item -Recurse -Force $out }   # apaga programa/tela/camera de teste sem abrir
  if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) { Remove-Item -Force -LiteralPath $voltzVideos }
}
```

**Preparo de gravação** (primeiros passos dentro do `try`): aplica os padrões Voltz pelo "Pular" da primeira execução
(qualidade HQ + codificador automático + MP4 híbrido; **não** cria fontes), aponta a pasta de gravações para `$out` e
muta o áudio global.
```powershell
if (-not (Uia "Pular")) { throw "botao Pular nao encontrado" }
Start-Sleep 2
$r = Ws ('[{"requestType":"SetProfileParameter","requestData":{"parameterCategory":"SimpleOutput","parameterName":"FilePath","parameterValue":' + (ConvertTo-Json $out) + '}},{"requestType":"GetSpecialInputs"}]')
$special = $r[1].data
$mutes = @(); foreach ($n in @($special.desktop1, $special.desktop2, $special.mic1, $special.mic2, $special.mic3, $special.mic4)) { if ($n) { $mutes += @{ requestType = "SetInputMute"; requestData = @{ inputName = $n; inputMuted = $true } } } }
if ($mutes.Count -gt 0) { [void](Ws (ConvertTo-Json -InputObject $mutes -Depth 5 -Compress)) }
```

**Fontes substitutas** (Tasks 5–7): cena "Cena ISO" no programa com dois blocos de cor — "Tela substituta" 2560×1440
(resolução diferente do programa, para provar a nativa) e "Camera substituta" 1280×720. `Set-CameraStandIn "dshow"` troca
a câmera por um `dshow_input` com dispositivo inválido (0×0, nenhuma webcam é aberta).
```powershell
$colorCamera = '{"requestType":"CreateInput","requestData":{"sceneName":"Cena ISO","inputName":"Camera substituta","inputKind":"color_source_v3","inputSettings":{"color":4294901760,"width":1280,"height":720}}}'
$dshowCamera = '{"requestType":"CreateInput","requestData":{"sceneName":"Cena ISO","inputName":"Camera substituta","inputKind":"dshow_input","inputSettings":{"video_device_id":"voltz-teste-sem-dispositivo"}}}'
function New-IsoStandIns {
  $r = Ws ('[{"requestType":"CreateScene","requestData":{"sceneName":"Cena ISO"}},' +
    '{"requestType":"CreateInput","requestData":{"sceneName":"Cena ISO","inputName":"Tela substituta","inputKind":"color_source_v3","inputSettings":{"color":4278229248,"width":2560,"height":1440}}},' +
    $colorCamera + ',{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Cena ISO"}}]')
  if (@($r | Where-Object { -not $_.ok }).Count -gt 0) { throw "fontes substitutas nao criadas" } }
function Set-CameraStandIn([string]$kind) {
  [void](Ws '[{"requestType":"RemoveInput","requestData":{"inputName":"Camera substituta"}}]'); Start-Sleep 1
  $r = Ws ('[' + $(if ($kind -eq "dshow") { $dshowCamera } else { $colorCamera }) + ']')
  if (-not $r[0].ok) { throw "camera substituta ($kind) nao criada" }; Start-Sleep 2 }
```

---

## Mapa de arquivos

**Novos — núcleo (`frontend/voltz/core/`, sem Qt/libobs) e testes**
| Arquivo | Responsabilidade |
|---|---|
| `core/iso-encoders.hpp/.cpp` | cadeia de encoders do ISO (AMF → NVENC → QSV, HEVC antes de H.264), nível de qualidade, chaves de configuração por família, regra de sobrecarga (`FrameCounters`/`isoOverloaded`) |
| `tests/test-iso-encoders.cpp` | doctest do acima |

**Novos — projeto (`frontend/voltz/project/`, alvo `obs-studio`)**
| Arquivo | Responsabilidade |
|---|---|
| `project/IsoTrack.hpp/.cpp` | um ISO: canvas privado + fonte no canal 0 → encoder de vídeo (tenta a cadeia) + AAC faixa 1 → `ffmpeg_muxer` MKV; pausa, parada graciosa com espera, falha durante a gravação, liberação na ordem certa |

**Modificados (Voltz):** `frontend/voltz/core/project-manifest.hpp/.cpp`, `frontend/voltz/core/CMakeLists.txt`,
`frontend/voltz/tests/test-project-manifest.cpp`, `frontend/voltz/tests/CMakeLists.txt`,
`frontend/cmake/feature-voltz.cmake`, `frontend/voltz/project/ProjectRecorder.hpp/.cpp`,
`frontend/voltz/project/ProjectLibrary.cpp`, `frontend/voltz/shell/RecordingConfig.hpp/.cpp`,
`frontend/voltz/shell/RecordPage.hpp/.cpp`, `frontend/voltz/shell/SettingsPage.hpp/.cpp`,
`frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`, `tools/voltz-smoke-project.ps1`,
`tools/voltz-smoke-record.ps1`; na Task 9: `docs/voltz/backlog-plano-3.md`, `CLAUDE.md`, `AGENTS.md`,
`docs/HISTORICO.md`.

**Modificados (OBS upstream):** nenhum. (`docs/voltz/upstream-patches.md` não muda; a Task 9 confere.)

**Contagem de testes:** 91 (base) → 99 (Task 2) → 104 (Task 3).

---

### Task 1: Spike de validação — `IsoTrack` (canvas privado → MKV) com gancho temporário

Valida o **risco 2 da spec** (§11: "ISO via `obs_canvas` … sem afetar a saída principal; medir carga com 3 encodes
AMF") antes de construir o resto. O `IsoTrack` escrito aqui **fica** (as tasks seguintes o usam); só o gancho
`VOLTZ_ISO_SPIKE` no `ProjectRecorder` é temporário (a Task 5 o substitui).

**Files:**
- Create: `frontend/voltz/project/IsoTrack.hpp`, `frontend/voltz/project/IsoTrack.cpp`
- Modify: `frontend/cmake/feature-voltz.cmake` (lista de fontes do alvo `obs-studio`)
- Modify: `frontend/voltz/project/ProjectRecorder.hpp`, `frontend/voltz/project/ProjectRecorder.cpp` (gancho temporário)

- [ ] **Step 1: Criar `frontend/voltz/project/IsoTrack.hpp`**

```cpp
#pragma once

#include <obs.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace voltz {

// Uma tentativa de encoder de vídeo para o ISO: id registrado no libobs + configurações.
struct IsoEncoderAttempt {
	std::string id;
	OBSData settings;
};

// Por que um ISO não iniciou (vira o "motivo" do evento iso_falhou).
enum class IsoFailure { None, NoImage, Canvas, NoEncoder, Output };

// "sem_imagem" | "canvas" | "sem_codificador" | "saida" ("" para None).
const char *isoFailureCode(IsoFailure failure);

// Faixa ISO (spec §5.3): canvas privado do libobs com uma única fonte no canal 0, na resolução da fonte → encoder de
// vídeo próprio + AAC da faixa 1 (mix) → ffmpeg_muxer (MKV). Vive na thread da UI; os sinais da saída chegam na
// thread do libobs e só mexem em atômicos/condição (e em onFailed).
class IsoTrack {
public:
	explicit IsoTrack(std::string track);
	~IsoTrack();
	IsoTrack(const IsoTrack &) = delete;
	IsoTrack &operator=(const IsoTrack &) = delete;

	// Cria o canvas e tenta cada encoder em ordem até a saída iniciar. Falso: `failure` diz por quê (detalhe no log).
	bool start(obs_source_t *source, const std::string &pathUtf8, const std::vector<IsoEncoderAttempt> &attempts,
		   IsoFailure &failure);
	void setPaused(bool paused);
	// Pede a parada graciosa (não espera). Idempotente.
	void requestStop();
	// Para (se ainda não pediu), espera o sinal "stop" até timeoutMs (depois força), loga e libera tudo.
	void finish(int timeoutMs);

	bool running() const;
	const std::string &track() const { return track_; }
	uint64_t startNs() const { return startNs_.load(); }
	uint32_t width() const { return width_; }
	uint32_t height() const { return height_; }
	double fps() const { return fps_; }
	const std::string &encoderId() const { return encoderId_; }

	// Thread do libobs: a saída parou sem a gente pedir (erro do muxer/encoder, StopOutput do websocket…).
	std::function<void(int code)> onFailed;

private:
	static void handleStart(void *data, calldata_t *params);
	static void handleStop(void *data, calldata_t *params);
	bool createVideo(obs_source_t *source);
	void releaseVideo();
	video_t *videoOutput() const;
	// 1: iniciou; 0: este encoder não serviu (tenta o próximo); -1: a saída nem foi criada.
	int tryStart(const IsoEncoderAttempt &attempt, const std::string &base, const std::string &pathUtf8);
	void release();

	std::string track_;
	OBSCanvasAutoRelease canvas_;
	OBSEncoderAutoRelease video_;
	OBSEncoderAutoRelease audio_;
	OBSOutputAutoRelease output_;
	OBSSignal startSignal_;
	OBSSignal stopSignal_;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	double fps_ = 0.0;
	std::string encoderId_;
	std::atomic<uint64_t> startNs_{0};
	std::atomic<bool> started_{false};
	std::atomic<bool> stopped_{false};
	std::atomic<bool> stopRequested_{false};
	std::mutex mutex_;
	std::condition_variable stopCv_;
};

} // namespace voltz
```

- [ ] **Step 2: Criar `frontend/voltz/project/IsoTrack.cpp`**

```cpp
#include "IsoTrack.hpp"

#include <util/platform.h>

#include <chrono>
#include <utility>

namespace voltz {

namespace {

constexpr int kForceStopWaitMs = 2000;
constexpr int64_t kAudioKbps = 160;

} // namespace

const char *isoFailureCode(IsoFailure failure)
{
	switch (failure) {
	case IsoFailure::NoImage:
		return "sem_imagem";
	case IsoFailure::Canvas:
		return "canvas";
	case IsoFailure::NoEncoder:
		return "sem_codificador";
	case IsoFailure::Output:
		return "saida";
	case IsoFailure::None:
		break;
	}
	return "";
}

IsoTrack::IsoTrack(std::string track) : track_(std::move(track)) {}

IsoTrack::~IsoTrack()
{
	finish(3000);
}

bool IsoTrack::running() const
{
	return started_.load() && !stopped_.load();
}

video_t *IsoTrack::videoOutput() const
{
	return canvas_ ? obs_canvas_get_video(canvas_) : nullptr;
}

bool IsoTrack::createVideo(obs_source_t *source)
{
	obs_video_info ovi = {};
	if (!obs_get_video_info(&ovi)) {
		return false;
	}
	ovi.base_width = width_;
	ovi.base_height = height_;
	ovi.output_width = width_;
	ovi.output_height = height_;
	/* O libobs força o fps do canvas principal em todo mix auxiliar (obs.c, obs_init_video_mix). */
	fps_ = ovi.fps_den ? static_cast<double>(ovi.fps_num) / static_cast<double>(ovi.fps_den) : 0.0;
	const std::string name = "voltz-iso-" + track_;
	/* Privado: fora da lista de canvases (não é salvo nem aparece no obs-websocket). Sem ACTIVATE: a fonte só é
	 * "mostrada" aqui (AUX_VIEW); quem a ativa é a cena do programa. Sem MIX_AUDIO. */
	canvas_ = obs_canvas_create_private(name.c_str(), &ovi, EPHEMERAL);
	if (!canvas_ || !obs_canvas_get_video(canvas_)) {
		canvas_ = nullptr;
		return false;
	}
	obs_canvas_set_channel(canvas_, 0, source);
	return true;
}

void IsoTrack::releaseVideo()
{
	if (!canvas_) {
		return;
	}
	obs_canvas_set_channel(canvas_, 0, nullptr);
	/* Única referência (canvas privado): obs_canvas_destroy tira o mix da lista e o libera na hora — por isso só
	 * depois de a saída e os encoders (que apontam para o video_t do mix) terem sido soltos. Não usar
	 * obs_canvas_remove: o sinal global "canvas_remove" faria o OBSBasic enfileirar RemoveCanvas com uma ref. */
	canvas_ = nullptr;
}

bool IsoTrack::start(obs_source_t *source, const std::string &pathUtf8, const std::vector<IsoEncoderAttempt> &attempts,
		     IsoFailure &failure)
{
	failure = IsoFailure::None;
	const char *sourceName = obs_source_get_name(source);
	/* NV12/P010 pedem lados pares: fonte ímpar perde 1 px na borda. */
	width_ = obs_source_get_width(source) & ~1u;
	height_ = obs_source_get_height(source) & ~1u;
	if (width_ < 2 || height_ < 2) {
		blog(LOG_WARNING, "[voltz] iso %s: fonte '%s' sem imagem (%ux%u)", track_.c_str(), sourceName, width_,
		     height_);
		failure = IsoFailure::NoImage;
		return false;
	}
	if (!createVideo(source)) {
		blog(LOG_ERROR, "[voltz] iso %s: canvas privado %ux%u nao foi criado", track_.c_str(), width_, height_);
		failure = IsoFailure::Canvas;
		release();
		return false;
	}

	const std::string base = "voltz-iso-" + track_;
	OBSDataAutoRelease audioSettings = obs_data_create();
	obs_data_set_int(audioSettings, "bitrate", kAudioKbps);
	/* Faixa 1 do Voltz (mix): o ffmpeg_muxer exige áudio (OBS_OUTPUT_AV) e o editor ganha uma referência de
	 * sincronia. */
	audio_ = obs_audio_encoder_create("ffmpeg_aac", (base + "-aac").c_str(), audioSettings, 0, nullptr);
	if (!audio_) {
		blog(LOG_ERROR, "[voltz] iso %s: encoder AAC nao foi criado", track_.c_str());
		failure = IsoFailure::Output;
		release();
		return false;
	}
	obs_encoder_set_audio(audio_, obs_get_audio());

	bool outputFailed = false;
	for (const IsoEncoderAttempt &attempt : attempts) {
		const int result = tryStart(attempt, base, pathUtf8);
		if (result > 0) {
			blog(LOG_INFO, "[voltz] iso %s: gravando '%s' de '%s' (%ux%u, %s)", track_.c_str(),
			     pathUtf8.c_str(), sourceName, width_, height_, encoderId_.c_str());
			return true;
		}
		if (result < 0) {
			outputFailed = true;
			break;
		}
	}
	failure = outputFailed ? IsoFailure::Output : IsoFailure::NoEncoder;
	blog(LOG_WARNING, "[voltz] iso %s: nenhum codificador iniciou (%zu opcao(oes))", track_.c_str(),
	     attempts.size());
	release();
	return false;
}

int IsoTrack::tryStart(const IsoEncoderAttempt &attempt, const std::string &base, const std::string &pathUtf8)
{
	OBSDataAutoRelease outputSettings = obs_data_create();
	obs_data_set_string(outputSettings, "path", pathUtf8.c_str());
	output_ = obs_output_create("ffmpeg_muxer", base.c_str(), outputSettings, nullptr);
	if (!output_) {
		blog(LOG_ERROR, "[voltz] iso %s: saida ffmpeg_muxer nao foi criada", track_.c_str());
		return -1;
	}
	video_ = obs_video_encoder_create(attempt.id.c_str(), (base + "-video").c_str(), attempt.settings, nullptr);
	if (!video_) {
		blog(LOG_INFO, "[voltz] iso %s: encoder '%s' nao registrado", track_.c_str(), attempt.id.c_str());
		output_ = nullptr;
		return 0;
	}
	obs_encoder_set_video(video_, videoOutput());
	obs_output_set_video_encoder(output_, video_);
	obs_output_set_audio_encoder(output_, audio_, 0);
	signal_handler_t *handler = obs_output_get_signal_handler(output_);
	startSignal_.Connect(handler, "start", &IsoTrack::handleStart, this);
	stopSignal_.Connect(handler, "stop", &IsoTrack::handleStop, this);
	startNs_ = 0;
	stopped_ = false;
	stopRequested_ = false;
	/* Falha de encoder (ex. sessão de GPU esgotada) é síncrona e não emite "stop" (obs-output.c). */
	if (obs_output_start(output_)) {
		started_ = true;
		encoderId_ = attempt.id;
		return 1;
	}
	const char *error = obs_output_get_last_error(output_);
	blog(LOG_WARNING, "[voltz] iso %s: '%s' nao iniciou (%s); tentando a proxima opcao", track_.c_str(),
	     attempt.id.c_str(), error && *error ? error : "sem detalhe");
	startSignal_.Disconnect();
	stopSignal_.Disconnect();
	output_ = nullptr;
	video_ = nullptr;
	return 0;
}

void IsoTrack::setPaused(bool paused)
{
	if (running() && !obs_output_pause(output_, paused)) {
		blog(LOG_WARNING, "[voltz] iso %s: %s falhou", track_.c_str(), paused ? "pausar" : "retomar");
	}
}

void IsoTrack::requestStop()
{
	if (!running() || stopRequested_.exchange(true)) {
		return;
	}
	obs_output_stop(output_);
}

void IsoTrack::finish(int timeoutMs)
{
	if (started_.load()) {
		requestStop();
		std::unique_lock<std::mutex> lock(mutex_);
		if (!stopCv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() { return stopped_.load(); })) {
			lock.unlock();
			blog(LOG_WARNING, "[voltz] iso %s: a parada passou de %d ms; forcando", track_.c_str(), timeoutMs);
			obs_output_force_stop(output_);
			lock.lock();
			stopCv_.wait_for(lock, std::chrono::milliseconds(kForceStopWaitMs),
					 [this]() { return stopped_.load(); });
		}
		lock.unlock();
		video_t *video = videoOutput();
		blog(LOG_INFO, "[voltz] iso %s: %d quadro(s) na saida; codificador pulou %u de %u (%s)", track_.c_str(),
		     obs_output_get_total_frames(output_), video ? video_output_get_skipped_frames(video) : 0u,
		     video ? video_output_get_total_frames(video) : 0u, encoderId_.c_str());
	}
	release();
}

void IsoTrack::release()
{
	startSignal_.Disconnect();
	stopSignal_.Disconnect();
	/* Ordem obrigatória: saída (obs_output_destroy espera o fim da captura e solta os encoders) → encoders → canvas. */
	output_ = nullptr;
	video_ = nullptr;
	audio_ = nullptr;
	releaseVideo();
	started_ = false;
}

void IsoTrack::handleStart(void *data, calldata_t *)
{
	static_cast<IsoTrack *>(data)->startNs_.store(os_gettime_ns());
}

void IsoTrack::handleStop(void *data, calldata_t *params)
{
	auto *self = static_cast<IsoTrack *>(data);
	const int code = static_cast<int>(calldata_int(params, "code"));
	/* Primeiro marca parada (running() já falso quando a casca contar os ISO), depois avisa. O IsoTrack não some no
	 * meio: release() desconecta o sinal, e signal_handler_disconnect espera este callback terminar. */
	{
		std::lock_guard<std::mutex> lock(self->mutex_);
		self->stopped_.store(true);
	}
	self->stopCv_.notify_all();
	if (!self->stopRequested_.load() && self->onFailed) {
		self->onFailed(code);
	}
}

} // namespace voltz
```

- [ ] **Step 3: Registrar no CMake** — em `frontend/cmake/feature-voltz.cmake`, na lista `target_sources(obs-studio
  PRIVATE …)`, depois de `voltz/project/CursorTracker.hpp`:

```cmake
    voltz/project/IsoTrack.cpp
    voltz/project/IsoTrack.hpp
```

- [ ] **Step 4: Gancho temporário do spike no `ProjectRecorder`**

`frontend/voltz/project/ProjectRecorder.hpp` — antes de `#include <voltz/core/project-manifest.hpp>`:
```cpp
#include "IsoTrack.hpp"

```
e, na seção `private:`, logo depois de `void stopCursor();`:
```cpp
	// Plano 3B, Task 1 (spike, temporário — a Task 5 troca por startIsoTracks): VOLTZ_ISO_SPIKE=1.
	void startIsoSpike();
	void requestIsoStop();
	void finishIsoTracks();
```
e, depois de `std::unique_ptr<CursorTracker> cursor_;`:
```cpp
	std::vector<std::unique_ptr<IsoTrack>> isoTracks_;
```

`frontend/voltz/project/ProjectRecorder.cpp`, cinco trocas exatas:

(a) destrutor — `stopCursor();` passa a ser seguido de `finishIsoTracks();`:
```cpp
ProjectRecorder::~ProjectRecorder()
{
	stopCursor();
	finishIsoTracks();
```
(b) `dispatch` — depois do bloco `case OBS_FRONTEND_EVENT_RECORDING_STARTED: onStarted(); break;`:
```cpp
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		requestIsoStop();
		break;
```
(c) `onStarted` — o trecho `startCursor();` / `writeManifest();` / `heartbeat_.start();` vira:
```cpp
	startCursor();
	startIsoSpike();
	writeManifest();
	heartbeat_.start();
```
(d) `onPaused` — antes de `const int64_t now = nowNs();`:
```cpp
	for (const auto &track : isoTracks_) {
		track->setPaused(paused);
	}
```
(e) `finish` — o trecho `heartbeat_.stop();` / `stopCursor();` / `writeManifest();` vira:
```cpp
	heartbeat_.stop();
	stopCursor();
	finishIsoTracks();
	writeManifest();
```

E no fim do arquivo, entre o `}` de `ProjectRecorder::stopCursor()` e `} // namespace voltz`:
```cpp
void ProjectRecorder::startIsoSpike()
{
	if (!qEnvironmentVariableIsSet("VOLTZ_ISO_SPIKE")) {
		return;
	}
	struct Role {
		const char *variable;
		const char *track;
		const char *file;
	};
	const Role roles[] = {{"VOLTZ_TEST_ISO_SCREEN", "tela", "tela.mkv"},
			      {"VOLTZ_TEST_ISO_CAMERA", "camera", "camera.mkv"}};
	for (const Role &role : roles) {
		const QString name = qEnvironmentVariable(role.variable);
		OBSSourceAutoRelease source = name.isEmpty() ? nullptr
							     : obs_get_source_by_name(name.toUtf8().constData());
		if (!source) {
			blog(LOG_WARNING, "[voltz] iso-spike %s: fonte '%s' nao encontrada", role.track,
			     qUtf8Printable(name));
			continue;
		}
		std::vector<IsoEncoderAttempt> attempts;
		for (const char *id : {"h265_texture_amf", "h264_texture_amf"}) {
			OBSDataAutoRelease settings = obs_data_create();
			obs_data_set_string(settings, "rate_control", "CQP");
			obs_data_set_int(settings, "cqp", 14);
			obs_data_set_string(settings, "preset", "quality");
			obs_data_set_int(settings, "keyint_sec", 2);
			attempts.push_back(IsoEncoderAttempt{id, OBSData(settings.Get())});
		}
		auto track = std::make_unique<IsoTrack>(role.track);
		const std::string trackName = role.track;
		track->onFailed = [trackName](int code) {
			blog(LOG_WARNING, "[voltz] iso-spike %s: parou sozinha (codigo %d)", trackName.c_str(), code);
		};
		IsoFailure failure = IsoFailure::None;
		const std::string path = nativeUtf8Path(QDir(projectDir_).filePath(QString::fromUtf8(role.file)));
		if (!track->start(source, path, attempts, failure)) {
			blog(LOG_WARNING, "[voltz] iso-spike %s: nao iniciou (%s)", role.track, isoFailureCode(failure));
			continue;
		}
		blog(LOG_INFO, "[voltz] iso-spike %s: offset %lld ms, %ux%u @ %.3f fps, %s", role.track,
		     static_cast<long long>(timeline_.relativeMs(static_cast<int64_t>(track->startNs()))),
		     track->width(), track->height(), track->fps(), track->encoderId().c_str());
		isoTracks_.push_back(std::move(track));
	}
}

void ProjectRecorder::requestIsoStop()
{
	for (const auto &track : isoTracks_) {
		track->requestStop();
	}
}

void ProjectRecorder::finishIsoTracks()
{
	requestIsoStop();
	for (const auto &track : isoTracks_) {
		track->finish(5000);
	}
	isoTracks_.clear();
}
```

- [ ] **Step 5: CONFIGURAR + COMPILAR APP (segundo plano)**

Run: **CONFIGURAR** e depois **COMPILAR APP**.
Expected: `exit=0` e nenhuma linha de erro. (Erro `EPHEMERAL` não declarado ⇒ conferir `#include <obs.hpp>`; aviso de
conversão ⇒ cast explícito, não desligar o aviso.)

- [ ] **Step 6: Rodar o spike (ao vivo)**

Bloco de teste com `$spike = $true` (o resto igual). Antes de iniciar o app, gere os clipes **sintéticos** (só padrão de
teste do `ffmpeg`, nenhum conteúdo do usuário) — coloque estas linhas logo antes de `$proc = Start-Process …`:
```powershell
$ffmpeg = (Get-Command ffmpeg -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $ffmpeg) { $ffmpeg = Get-ChildItem (Join-Path $root ".deps") -Filter ffmpeg.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName }
$fx = Join-Path $sp "fixtures"; New-Item -ItemType Directory -Force $fx | Out-Null
& $ffmpeg -v error -y -f lavfi -i "testsrc2=size=2560x1440:rate=60" -t 10 -c:v libx264 -preset ultrafast -pix_fmt yuv420p (Join-Path $fx "tela-teste.mp4")
& $ffmpeg -v error -y -f lavfi -i "testsrc2=size=1280x720:rate=30" -t 10 -c:v libx264 -preset ultrafast -pix_fmt yuv420p (Join-Path $fx "camera-teste.mp4")
```
No `try`:
```powershell
# Preparo de gravação (bloco acima): Pular, pasta $out, áudio mudo
# Substitutas com movimento (carga realista de codificação): clipes sintéticos em loop, na cena do programa
$r = Ws ('[{"requestType":"CreateScene","requestData":{"sceneName":"Cena ISO"}},' +
  '{"requestType":"CreateInput","requestData":{"sceneName":"Cena ISO","inputName":"Tela substituta","inputKind":"ffmpeg_source","inputSettings":{"is_local_file":true,"looping":true,"local_file":' + (ConvertTo-Json (Join-Path $fx "tela-teste.mp4")) + '}}},' +
  '{"requestType":"CreateInput","requestData":{"sceneName":"Cena ISO","inputName":"Camera substituta","inputKind":"ffmpeg_source","inputSettings":{"is_local_file":true,"looping":true,"local_file":' + (ConvertTo-Json (Join-Path $fx "camera-teste.mp4")) + '}}},' +
  '{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Cena ISO"}}]')
Start-Sleep 3
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 6
[void](Ws '[{"requestType":"GetOutputList"}]')   # deve listar voltz-iso-tela e voltz-iso-camera com outputActive=true
[void](Ws '[{"requestType":"PauseRecord"}]'); Start-Sleep 2
[void](Ws '[{"requestType":"ResumeRecord"}]'); Start-Sleep 6
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 5
Close-App; Save-Log
$m = Show-Project (Project-Dirs)[-1]
LogLines 'iso-spike|\[voltz\] iso |voltz-iso-|simple_file_output|encoding lag|lagged frames|memory leaks|output .*paused'
```
No `finally`, acrescente `Remove-Item -Recurse -Force $fx -ErrorAction SilentlyContinue`.

- [ ] **Step 7: Critérios objetivos do spike (todos precisam passar)**

| # | Critério | Como medir (saída do Step 6) |
|---|---|---|
| P1 | `tela.mkv` existe com **1 vídeo `hevc` (ou `h264`) 2560×1440** e **1 áudio `aac`** | linha `tela.mkv : video/hevc 2560x1440 … \| audio/aac …` |
| P2 | `camera.mkv` com 1 vídeo 1280×720 + 1 áudio `aac`; `avg_frame_rate` dos dois ISO ≈ fps do programa (60 ± 1) | linha `camera.mkv : …` |
| P3 | Duração casa com o programa: `\|dur_iso + offset_ms/1000 − dur_programa\| ≤ 0,5 s` para cada ISO (a pausa de 2 s sai de todos) | `dur=` das linhas + `offset` da linha `iso-spike` |
| P4 | Programa intacto: `programa.mp4` 1920×1080, 3 faixas de áudio, `status: ok`, duração ≈ `duracao_liquida_ms` (±1 s) | `Show-Project` |
| P5 | `[voltz] iso-spike tela: offset X ms` e `… camera: offset Y ms` com 0 ≤ X, Y ≤ 1000 | `LogLines` |
| P6 | Carga: em cada `[voltz] iso <faixa>: N quadro(s) na saida; codificador pulou a de b`, a/b ≤ 2%; nenhuma linha `skipped frames due to encoding lag` acima de 2,0%; a linha `Output 'simple_file_output': Number of lagged frames …`, se existir, ≤ 2,0% | `LogLines` |
| P7 | Pausa aplicada aos ISO: `output voltz-iso-tela paused`, `… unpaused` (e o mesmo para `camera`) | `LogLines` |
| P8 | App fecha limpo e `Number of memory leaks: 0` | `LogLines` (se ≠ 0, rode o mesmo bloco com `$spike = $false` e sem `StartRecord`: o spike passa se o número for igual ao da base) |

Anote no relatório os números medidos: codificador usado em cada ISO, offsets, % de quadros pulados/atrasados.

- [ ] **Step 8: Se algum critério falhar — árvore de decisão (não pule etapas)**

1. **P1/P2 falham com `canvas privado … nao foi criado`, arquivo vazio ou `0 quadro(s) na saida`** ⇒ trocar o canvas
   pela **view** (padrão da câmera virtual, `frontend/utility/BasicOutputHandler.cpp:260-267`/`:380-385`). Só o
   `IsoTrack` muda; o resto do plano continua igual. Em `IsoTrack.hpp`, troque o membro
   `OBSCanvasAutoRelease canvas_;` por:
   ```cpp
   	obs_view_t *view_ = nullptr;
   	video_t *viewVideo_ = nullptr;
   ```
   e em `IsoTrack.cpp` substitua `videoOutput`, `createVideo` e `releaseVideo` por:
   ```cpp
   video_t *IsoTrack::videoOutput() const
   {
   	return viewVideo_;
   }

   bool IsoTrack::createVideo(obs_source_t *source)
   {
   	obs_video_info ovi = {};
   	if (!obs_get_video_info(&ovi)) {
   		return false;
   	}
   	ovi.base_width = width_;
   	ovi.base_height = height_;
   	ovi.output_width = width_;
   	ovi.output_height = height_;
   	fps_ = ovi.fps_den ? static_cast<double>(ovi.fps_num) / static_cast<double>(ovi.fps_den) : 0.0;
   	view_ = obs_view_create();
   	obs_view_set_source(view_, 0, source);
   	viewVideo_ = obs_view_add2(view_, &ovi);
   	if (!viewVideo_) {
   		obs_view_set_source(view_, 0, nullptr);
   		obs_view_destroy(view_);
   		view_ = nullptr;
   		return false;
   	}
   	return true;
   }

   void IsoTrack::releaseVideo()
   {
   	if (!view_) {
   		return;
   	}
   	/* Mesmo padrão da câmera virtual: desliga o mix (o thread gráfico o libera no próximo quadro), solta a fonte e
   	 * destrói a view — depois da saída e dos encoders. */
   	obs_view_remove(view_);
   	obs_view_set_source(view_, 0, nullptr);
   	obs_view_destroy(view_);
   	view_ = nullptr;
   	viewVideo_ = nullptr;
   }
   ```
   COMPILAR APP e repita os Steps 6–7. Passou ⇒ siga o plano e registre na Task 9 (Decisões do `CLAUDE.md` e backlog)
   "ISO via `obs_view` (canvas privado falhou: <sintoma>)". Falhou de novo ⇒ **BLOCKED**: pare, reverta o gancho
   (`git checkout -- frontend/voltz/project/ProjectRecorder.*`) e reporte ao controlador o log e as saídas do `ffprobe`.
   **Não** faça patch no OBS.
2. **`'h265_texture_amf' nao iniciou` mas o `h264_texture_amf` iniciou** ⇒ não é falha (a cadeia existe para isso):
   P1 aceita `h264`. Registre o motivo do log no relatório.
3. **P6 falha (quadros pulados/atrasados > 2%)** ⇒ repita com `preset` `balanced` e, se preciso, `speed` no gancho
   (`obs_data_set_string(settings, "preset", "balanced")`). O primeiro preset que passar vira o valor de
   `kIsoAmdPreset` na Task 2 (anote no relatório). Nem `speed` passa ⇒ reporte (**DONE_WITH_CONCERNS**) com os números:
   o plano segue, e a regra de sobrecarga da Task 5 é quem protege o programa.
4. **P3 falha só por causa da pausa** (ISO ~2 s mais longo) ⇒ confira P7. Sem `output voltz-iso-… paused` no log, o
   `obs_output_pause` do ISO está falhando (`[voltz] iso …: pausar falhou`) ⇒ **BLOCKED** com o log.
5. **P3 falha sem pausa envolvida, P4 ou P8 falham** ⇒ **BLOCKED** com o log completo (`$sp\ultimo-log.txt`).

- [ ] **Step 9: Formatar e commitar** (só com o spike aprovado, com canvas ou com a view)

```bash
"$CLANGFMT" -i frontend/voltz/project/IsoTrack.hpp frontend/voltz/project/IsoTrack.cpp frontend/voltz/project/ProjectRecorder.hpp frontend/voltz/project/ProjectRecorder.cpp
```
COMPILAR APP (segundo plano, `exit=0`), então:
```bash
git add frontend/voltz/project/IsoTrack.hpp frontend/voltz/project/IsoTrack.cpp frontend/voltz/project/ProjectRecorder.hpp frontend/voltz/project/ProjectRecorder.cpp frontend/cmake/feature-voltz.cmake
git commit -m "feat(voltz): IsoTrack (canvas privado com uma fonte para MKV) e spike do ISO

Gancho temporario VOLTZ_ISO_SPIKE no ProjectRecorder (substituido na Task 5 do Plano 3B).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Núcleo — cadeia de encoders do ISO, qualidade e regra de sobrecarga

**Files:**
- Create: `frontend/voltz/core/iso-encoders.hpp`, `frontend/voltz/core/iso-encoders.cpp`
- Create: `frontend/voltz/tests/test-iso-encoders.cpp`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`

- [ ] **Step 1: Escrever o teste (falha: cabeçalho não existe)**

`frontend/voltz/tests/test-iso-encoders.cpp`:
```cpp
#include <doctest.h>

#include <voltz/core/iso-encoders.hpp>

#include <cstdint>
#include <string>
#include <vector>

using voltz::EncoderFamily;
using voltz::EncoderSettings;
using voltz::FrameCounters;
using voltz::IsoEncoderCandidate;
using voltz::RecordingQuality;

namespace {

std::vector<std::string> ids(const std::vector<IsoEncoderCandidate> &chain)
{
	std::vector<std::string> out;
	for (const IsoEncoderCandidate &candidate : chain) {
		out.push_back(candidate.id);
	}
	return out;
}

std::string text(const EncoderSettings &settings, const std::string &key)
{
	for (const auto &item : settings.strings) {
		if (item.first == key) {
			return item.second;
		}
	}
	return "<ausente>";
}

int64_t number(const EncoderSettings &settings, const std::string &key)
{
	for (const auto &item : settings.ints) {
		if (item.first == key) {
			return item.second;
		}
	}
	return -1;
}

IsoEncoderCandidate candidate(const char *id, const char *codec, EncoderFamily family)
{
	return IsoEncoderCandidate{id, codec, family};
}

} // namespace

TEST_CASE("cadeia do ISO só com AMD: HEVC antes de H.264 e sem x264 por padrão")
{
	const std::vector<std::string> available = {"ffmpeg_aac", "obs_x264", "h264_texture_amf", "h265_texture_amf",
						    "av1_texture_amf"};
	const std::vector<IsoEncoderCandidate> chain = voltz::isoEncoderChain(available, false);
	REQUIRE(chain.size() == 2);
	CHECK(ids(chain) == std::vector<std::string>{"h265_texture_amf", "h264_texture_amf"});
	CHECK(chain[0].codec == "hevc");
	CHECK(chain[1].codec == "h264");
	CHECK(chain[0].family == EncoderFamily::Amd);
	CHECK(ids(voltz::isoEncoderChain(available, true)) ==
	      std::vector<std::string>{"h265_texture_amf", "h264_texture_amf", "obs_x264"});
}

TEST_CASE("cadeia do ISO segue AMD, depois NVIDIA, depois Intel")
{
	const std::vector<std::string> available = {"obs_qsv11_v2",       "obs_qsv11_hevc",   "obs_nvenc_h264_tex",
						    "obs_nvenc_hevc_tex", "ffmpeg_nvenc",     "ffmpeg_hevc_nvenc",
						    "h264_texture_amf",   "h265_texture_amf", "obs_x264"};
	CHECK(ids(voltz::isoEncoderChain(available, false)) ==
	      std::vector<std::string>{"h265_texture_amf", "h264_texture_amf", "obs_nvenc_hevc_tex", "obs_nvenc_h264_tex",
				       "obs_qsv11_hevc", "obs_qsv11_v2"});
}

TEST_CASE("NVENC de compatibilidade e QSV v1 entram quando os novos não existem")
{
	CHECK(ids(voltz::isoEncoderChain({"ffmpeg_nvenc", "ffmpeg_hevc_nvenc", "obs_qsv11"}, false)) ==
	      std::vector<std::string>{"ffmpeg_hevc_nvenc", "ffmpeg_nvenc", "obs_qsv11"});
}

TEST_CASE("sem GPU a cadeia fica vazia; x264 só quando permitido")
{
	CHECK(voltz::isoEncoderChain({"obs_x264", "ffmpeg_aac"}, false).empty());
	CHECK(ids(voltz::isoEncoderChain({"obs_x264", "ffmpeg_aac"}, true)) == std::vector<std::string>{"obs_x264"});
	CHECK(voltz::isoEncoderChain({}, true).empty());
}

TEST_CASE("qualidade do ISO é maior (QP menor) que a do programa")
{
	CHECK(voltz::isoQualityLevel(RecordingQuality::High) == 14);
	CHECK(voltz::isoQualityLevel(RecordingQuality::High) < 16);
	CHECK(voltz::isoQualityLevel(RecordingQuality::Compact) == 20);
	CHECK(voltz::isoQualityLevel(RecordingQuality::Compact) < 23);
}

TEST_CASE("configuração AMF: CQP, preset de qualidade, GOP de 2 s; perfil só no H.264")
{
	const EncoderSettings hevc =
		voltz::isoEncoderSettings(candidate("h265_texture_amf", "hevc", EncoderFamily::Amd), 14);
	CHECK(text(hevc, "rate_control") == "CQP");
	CHECK(text(hevc, "preset") == voltz::kIsoAmdPreset);
	CHECK(number(hevc, "cqp") == 14);
	CHECK(number(hevc, "keyint_sec") == 2);
	CHECK(text(hevc, "profile") == "<ausente>");
	const EncoderSettings h264 =
		voltz::isoEncoderSettings(candidate("h264_texture_amf", "h264", EncoderFamily::Amd), 20);
	CHECK(text(h264, "profile") == "high");
	CHECK(number(h264, "cqp") == 20);
}

TEST_CASE("configuração NVENC/QSV em CQP com perfil por codec; x264 em CRF")
{
	const EncoderSettings nvenc =
		voltz::isoEncoderSettings(candidate("obs_nvenc_hevc_tex", "hevc", EncoderFamily::Nvidia), 20);
	CHECK(text(nvenc, "rate_control") == "CQP");
	CHECK(text(nvenc, "profile") == "main");
	CHECK(number(nvenc, "cqp") == 20);
	CHECK(text(nvenc, "preset") == "<ausente>");
	const EncoderSettings qsv = voltz::isoEncoderSettings(candidate("obs_qsv11_v2", "h264", EncoderFamily::Intel), 14);
	CHECK(text(qsv, "profile") == "high");
	CHECK(number(qsv, "keyint_sec") == 2);
	const EncoderSettings x264 = voltz::isoEncoderSettings(candidate("obs_x264", "h264", EncoderFamily::Software), 14);
	CHECK(text(x264, "rate_control") == "CRF");
	CHECK(number(x264, "crf") == 14);
	CHECK(number(x264, "cqp") == -1);
	CHECK(text(x264, "preset") == "veryfast");
}

TEST_CASE("sobrecarga: 5% ou mais de quadros atrasados ou pulados numa janela de pelo menos 60")
{
	const FrameCounters before{1000, 10, 1000, 5};
	CHECK_FALSE(voltz::isoOverloaded(before, FrameCounters{1300, 24, 1300, 5})); // 14/300 = 4,7%
	CHECK(voltz::isoOverloaded(before, FrameCounters{1300, 25, 1300, 5}));       // 15/300 = 5% (render)
	CHECK(voltz::isoOverloaded(before, FrameCounters{1300, 10, 1300, 20}));      // 15/300 = 5% (codificador)
	CHECK_FALSE(voltz::isoOverloaded(FrameCounters{}, FrameCounters{50, 50, 50, 50})); // janela < 60
	CHECK_FALSE(voltz::isoOverloaded(before, FrameCounters{100, 0, 100, 0}));          // contadores voltaram
}
```

Em `frontend/voltz/tests/CMakeLists.txt`, depois de `test-disk-space.cpp`:
```cmake
    test-iso-encoders.cpp
```

- [ ] **Step 2: Rodar e ver falhar**

Run: **CONFIGURAR** e **COMPILAR TESTES**.
Expected: FALHA de compilação `cannot open include file: 'voltz/core/iso-encoders.hpp'`.

- [ ] **Step 3: Implementar**

`frontend/voltz/core/iso-encoders.hpp`:
```cpp
#pragma once

#include "recording-settings.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace voltz {

// GOP dos ISO: curto para o editor achar quadros-chave perto de qualquer ponto.
constexpr int kIsoKeyframeSeconds = 2;
// Preset do AMF nos ISO (a Task 1 do Plano 3B confirma que "quality" sustenta 3 sessões nesta GPU).
constexpr const char *kIsoAmdPreset = "quality";

enum class EncoderFamily { Amd, Nvidia, Intel, Software };

// Encoder de vídeo candidato para um ISO: id registrado no libobs, codec ("hevc" | "h264") e família.
struct IsoEncoderCandidate {
	std::string id;
	std::string codec;
	EncoderFamily family = EncoderFamily::Software;
};

// Cadeia da spec §5.4 entre os encoders registrados: AMD AMF → NVIDIA NVENC → Intel QSV, em cada família HEVC antes
// de H.264. x264 por último só com allowSoftware (no ISO ele competiria com o programa pela CPU).
std::vector<IsoEncoderCandidate> isoEncoderChain(const std::vector<std::string> &availableIds, bool allowSoftware);

// Nível CQP/CRF do ISO, sempre de qualidade maior que a do programa (o OBS grava "Alta" em 16 e "Arquivo menor" em
// 23): Alta → 14, Arquivo menor → 20.
int isoQualityLevel(RecordingQuality quality);

// Configurações do encoder (chaves dos plugins do libobs).
struct EncoderSettings {
	std::vector<std::pair<std::string, std::string>> strings;
	std::vector<std::pair<std::string, int64_t>> ints;
};

// CQP (CRF no x264) com `qualityLevel`, GOP de kIsoKeyframeSeconds; AMF com kIsoAmdPreset; perfil "high" no H.264 e,
// no HEVC, "main" (NVENC/QSV) ou automático (AMF escolhe main/main10).
EncoderSettings isoEncoderSettings(const IsoEncoderCandidate &candidate, int qualityLevel);

// Contadores de quadros do programa: renderizados/atrasados (obs_get_total_frames/obs_get_lagged_frames) e
// entregues/pulados pelo codificador (video_output_get_total_frames/get_skipped_frames do vídeo principal).
struct FrameCounters {
	uint64_t rendered = 0;
	uint64_t lagged = 0;
	uint64_t encoded = 0;
	uint64_t skipped = 0;
};

// "A GPU não sustenta" (spec §5.4): desde `before`, ≥ 5% dos quadros do programa atrasaram na renderização ou foram
// pulados pelo codificador, numa janela de pelo menos 60 quadros. Contador que voltou (reinício do vídeo) → falso.
bool isoOverloaded(const FrameCounters &before, const FrameCounters &now);

} // namespace voltz
```

`frontend/voltz/core/iso-encoders.cpp`:
```cpp
#include "iso-encoders.hpp"

#include <algorithm>
#include <initializer_list>

namespace voltz {

namespace {

constexpr uint64_t kMinWindowFrames = 60;
constexpr uint64_t kOverloadPercent = 5;

// Primeiro id registrado, na ordem de preferência; vazio se nenhum.
std::string firstAvailable(const std::vector<std::string> &ids, std::initializer_list<const char *> preference)
{
	for (const char *id : preference) {
		if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
			return id;
		}
	}
	return {};
}

void add(std::vector<IsoEncoderCandidate> &chain, const std::string &id, const char *codec, EncoderFamily family)
{
	if (!id.empty()) {
		chain.push_back(IsoEncoderCandidate{id, codec, family});
	}
}

bool overPercent(uint64_t part, uint64_t whole)
{
	return whole >= kMinWindowFrames && part * 100 >= whole * kOverloadPercent;
}

} // namespace

std::vector<IsoEncoderCandidate> isoEncoderChain(const std::vector<std::string> &availableIds, bool allowSoftware)
{
	std::vector<IsoEncoderCandidate> chain;
	add(chain, firstAvailable(availableIds, {"h265_texture_amf"}), "hevc", EncoderFamily::Amd);
	add(chain, firstAvailable(availableIds, {"h264_texture_amf"}), "h264", EncoderFamily::Amd);
	add(chain, firstAvailable(availableIds, {"obs_nvenc_hevc_tex", "ffmpeg_hevc_nvenc"}), "hevc",
	    EncoderFamily::Nvidia);
	add(chain, firstAvailable(availableIds, {"obs_nvenc_h264_tex", "ffmpeg_nvenc"}), "h264", EncoderFamily::Nvidia);
	add(chain, firstAvailable(availableIds, {"obs_qsv11_hevc"}), "hevc", EncoderFamily::Intel);
	add(chain, firstAvailable(availableIds, {"obs_qsv11_v2", "obs_qsv11"}), "h264", EncoderFamily::Intel);
	if (allowSoftware) {
		add(chain, firstAvailable(availableIds, {"obs_x264"}), "h264", EncoderFamily::Software);
	}
	return chain;
}

int isoQualityLevel(RecordingQuality quality)
{
	return quality == RecordingQuality::Compact ? 20 : 14;
}

EncoderSettings isoEncoderSettings(const IsoEncoderCandidate &candidate, int qualityLevel)
{
	EncoderSettings settings;
	const bool hevc = candidate.codec == "hevc";
	settings.ints.emplace_back("keyint_sec", kIsoKeyframeSeconds);
	switch (candidate.family) {
	case EncoderFamily::Amd:
		settings.strings.emplace_back("rate_control", "CQP");
		settings.strings.emplace_back("preset", kIsoAmdPreset);
		settings.ints.emplace_back("cqp", qualityLevel);
		if (!hevc) {
			settings.strings.emplace_back("profile", "high");
		}
		break;
	case EncoderFamily::Nvidia:
	case EncoderFamily::Intel:
		settings.strings.emplace_back("rate_control", "CQP");
		settings.strings.emplace_back("profile", hevc ? "main" : "high");
		settings.ints.emplace_back("cqp", qualityLevel);
		break;
	case EncoderFamily::Software:
		settings.strings.emplace_back("rate_control", "CRF");
		settings.strings.emplace_back("preset", "veryfast");
		settings.strings.emplace_back("profile", "high");
		settings.ints.emplace_back("crf", qualityLevel);
		break;
	}
	return settings;
}

bool isoOverloaded(const FrameCounters &before, const FrameCounters &now)
{
	if (now.rendered < before.rendered || now.lagged < before.lagged || now.encoded < before.encoded ||
	    now.skipped < before.skipped) {
		return false;
	}
	return overPercent(now.lagged - before.lagged, now.rendered - before.rendered) ||
	       overPercent(now.skipped - before.skipped, now.encoded - before.encoded);
}

} // namespace voltz
```

Em `frontend/voltz/core/CMakeLists.txt`: em `PRIVATE`, depois de `disk-space.cpp`, a linha `iso-encoders.cpp`; em
`PUBLIC`, depois de `disk-space.hpp`, a linha `iso-encoders.hpp`.

- [ ] **Step 4: Rodar e ver passar**

Run: **CONFIGURAR**, **COMPILAR TESTES**, **RODAR TESTES** e o executável para a contagem.
Expected: `100% tests passed`; `test cases: 99 | 99 passed`.
(Se a Task 1 trocou o preset do AMF, ajuste `kIsoAmdPreset` aqui e no relatório; o teste usa a própria constante.)

- [ ] **Step 5: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/iso-encoders.hpp frontend/voltz/core/iso-encoders.cpp frontend/voltz/tests/test-iso-encoders.cpp
git add frontend/voltz/core/iso-encoders.hpp frontend/voltz/core/iso-encoders.cpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/test-iso-encoders.cpp frontend/voltz/tests/CMakeLists.txt
git commit -m "feat(voltz): nucleo da cadeia de encoders do ISO, qualidade e regra de sobrecarga

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Núcleo — manifesto com `tela`/`camera`, `motivo` nos eventos e campos desconhecidos em `arquivos.*`

**Files:**
- Modify: `frontend/voltz/core/project-manifest.hpp`, `frontend/voltz/core/project-manifest.cpp`
- Test: `frontend/voltz/tests/test-project-manifest.cpp`

- [ ] **Step 1: Escrever os testes (falham: `IsoFile`, `reason` e `dropUnusableIsoFiles` não existem)**

Em `frontend/voltz/tests/test-project-manifest.cpp`: na lista de `using`, acrescente `using voltz::IsoFile;`. Depois
da função `contains` (ainda dentro do `namespace {}`), acrescente:
```cpp
size_t count(const std::string &text, const std::string &part)
{
	size_t found = 0;
	for (size_t pos = text.find(part); pos != std::string::npos; pos = text.find(part, pos + part.size())) {
		found++;
	}
	return found;
}

ProjectManifest withIso()
{
	ProjectManifest m = sample();
	m.screen.present = true;
	m.screen.file = "tela.mkv";
	m.screen.width = 2560;
	m.screen.height = 1440;
	m.screen.fps = 60;
	m.screen.offsetMs = 3;
	m.screen.encoder = "h265_texture_amf";
	m.screen.hasOrigin = true;
	m.screen.origin.type = "monitor";
	m.screen.origin.id = "\\\\.\\DISPLAY1";
	m.screen.origin.rect[0] = 0;
	m.screen.origin.rect[1] = 0;
	m.screen.origin.rect[2] = 2560;
	m.screen.origin.rect[3] = 1440;
	m.screen.origin.scale = 1.25;
	m.camera.present = true;
	m.camera.file = "camera.mkv";
	m.camera.width = 1920;
	m.camera.height = 1080;
	m.camera.fps = 60;
	m.camera.offsetMs = 5;
	m.camera.encoder = "h265_texture_amf";
	return m;
}
```
E no fim do arquivo, os 5 casos novos:
```cpp
TEST_CASE("tela e câmera saem com arquivo, tamanho, fps, offset, codificador e origem só na tela")
{
	const std::string json = voltz::serializeManifest(withIso());
	CHECK(contains(json, "\"arquivo\": \"tela.mkv\""));
	CHECK(contains(json, "\"arquivo\": \"camera.mkv\""));
	CHECK(contains(json, "\"offset_ms\": 3"));
	CHECK(contains(json, "\"offset_ms\": 5"));
	CHECK(contains(json, "\"codificador\": \"h265_texture_amf\""));
	CHECK(count(json, "\"origem\"") == 2); // tela + cursor; a câmera não tem
	CHECK(json.find("\"programa\"") < json.find("\"tela\""));
	CHECK(json.find("\"tela\"") < json.find("\"camera\""));
	CHECK(json.find("\"camera\"") < json.find("\"cursor\""));
}

TEST_CASE("parseManifest lê tela e câmera de volta")
{
	const voltz::ManifestParse parsed = voltz::parseManifest(voltz::serializeManifest(withIso()));
	REQUIRE(parsed.ok);
	const IsoFile &screen = parsed.manifest.screen;
	CHECK(screen.present);
	CHECK(screen.file == "tela.mkv");
	CHECK(screen.width == 2560);
	CHECK(screen.height == 1440);
	CHECK(screen.fps == doctest::Approx(60.0));
	CHECK(screen.offsetMs == 3);
	CHECK(screen.encoder == "h265_texture_amf");
	CHECK(screen.hasOrigin);
	CHECK(screen.origin.type == "monitor");
	CHECK(screen.origin.id == "\\\\.\\DISPLAY1");
	CHECK(screen.origin.rect[2] == 2560);
	CHECK(screen.origin.scale == doctest::Approx(1.25));
	const IsoFile &camera = parsed.manifest.camera;
	CHECK(camera.present);
	CHECK(camera.offsetMs == 5);
	CHECK_FALSE(camera.hasOrigin);
}

TEST_CASE("evento com motivo vai e volta; sem motivo não escreve a chave")
{
	ProjectManifest m = sample();
	m.events.push_back(ManifestEvent{0, "iso_falhou", "camera", 0, "sem_imagem"});
	const std::string json = voltz::serializeManifest(m);
	CHECK(contains(json, "\"faixa\": \"camera\""));
	CHECK(contains(json, "\"motivo\": \"sem_imagem\""));
	CHECK(count(json, "\"motivo\"") == 1);
	const voltz::ManifestParse parsed = voltz::parseManifest(json);
	REQUIRE(parsed.ok);
	REQUIRE(parsed.manifest.events.size() == 3);
	CHECK(parsed.manifest.events[2].track == "camera");
	CHECK(parsed.manifest.events[2].reason == "sem_imagem");
	CHECK(parsed.manifest.events[0].reason.empty());
}

TEST_CASE("campos desconhecidos dentro de arquivos.* sobrevivem; arquivo que saiu do modelo some do JSON")
{
	voltz::ManifestParse parsed = voltz::parseManifest(R"({"formato":"voltz-projeto","versao":1,"nome":"X",
		"status":"ok","arquivos":{"programa":{"arquivo":"programa.mp4","hdr":true},
		"tela":{"arquivo":"tela.mkv","largura":2560,"nota":"x"},"camera":{"arquivo":"camera.mkv"}}})");
	REQUIRE(parsed.ok);
	CHECK(parsed.manifest.screen.present);
	CHECK(parsed.manifest.camera.present);
	parsed.manifest.name = "Y";
	parsed.manifest.camera.present = false;
	const std::string rewritten = voltz::serializeManifest(parsed.manifest);
	CHECK(contains(rewritten, "\"nome\": \"Y\""));
	CHECK(contains(rewritten, "\"hdr\": true"));
	CHECK(contains(rewritten, "\"nota\": \"x\""));
	CHECK(contains(rewritten, "\"tela.mkv\""));
	CHECK_FALSE(contains(rewritten, "camera.mkv"));
}

TEST_CASE("dropUnusableIsoFiles tira o ISO sem arquivo e registra iso_falhou com motivo")
{
	ProjectManifest m = withIso();
	m.events.clear();
	const int dropped =
		voltz::dropUnusableIsoFiles(m, [](const std::string &file) { return file == "camera.mkv"; });
	CHECK(dropped == 1);
	CHECK_FALSE(m.screen.present);
	CHECK(m.camera.present);
	REQUIRE(m.events.size() == 1);
	CHECK(m.events[0].type == "iso_falhou");
	CHECK(m.events[0].track == "tela");
	CHECK(m.events[0].reason == "arquivo_ausente");
	CHECK(m.events[0].tMs == 0);
	CHECK(voltz::dropUnusableIsoFiles(m, [](const std::string &) { return true; }) == 0);
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: **COMPILAR TESTES**.
Expected: FALHA de compilação (`'IsoFile': is not a member of 'voltz'`, `'screen': is not a member of
'voltz::ProjectManifest'`).

- [ ] **Step 3: Implementar — cabeçalho**

Em `frontend/voltz/core/project-manifest.hpp`:

1. Acrescente `#include <functional>` junto dos outros includes da biblioteca padrão.
2. Depois de `constexpr const char *kCursorFileName = "cursor.jsonl";`:
```cpp
constexpr const char *kScreenIsoFileName = "tela.mkv";
constexpr const char *kCameraIsoFileName = "camera.mkv";
// "faixa" dos eventos e chave em "arquivos"
constexpr const char *kScreenTrack = "tela";
constexpr const char *kCameraTrack = "camera";
```
3. Depois da struct `CursorFile`:
```cpp
// arquivos.tela / arquivos.camera (3B): ISO de uma fonte de papel Tela/Câmera (spec §5.3).
struct IsoFile {
	bool present = false;
	std::string file;
	int width = 0;
	int height = 0;
	double fps = 0.0;
	int64_t offsetMs = 0;   // primeiro quadro em relação ao t0_ns (spec §5.5)
	std::string encoder;    // "codificador": id do encoder no libobs (ex. "h265_texture_amf")
	bool hasOrigin = false; // só na tela, quando é captura de monitor
	CaptureOrigin origin;   // mesmo formato de arquivos.cursor.origem
};
```
4. Troque a struct `ManifestEvent` por:
```cpp
// eventos[]: { "t_ms", "tipo", "faixa"?, "codigo"?, "motivo"? }
struct ManifestEvent {
	int64_t tMs = 0;
	std::string type;
	std::string track;
	int code = 0;
	std::string reason;
};
```
5. Em `ProjectManifest`, entre `ProgramFile program;` e `CursorFile cursor;`:
```cpp
	IsoFile screen;
	IsoFile camera;
```
e troque o comentário de `preservedJson` por:
```cpp
	// JSON lido do disco. Ao reescrever, os campos que esta versão não conhece (no nível de cima e dentro de cada
	// arquivos.*) voltam intactos; arquivo ausente no modelo sai de "arquivos".
	std::string preservedJson;
```
6. Depois da declaração de `recoverManifest`:
```cpp
// Recuperação: tira do manifesto os ISO (tela/camera) cujo arquivo não serve (`usable` falso: não existe ou está
// vazio) e registra, para cada um, o evento iso_falhou (t_ms 0, faixa, motivo "arquivo_ausente"). Devolve quantos tirou.
int dropUnusableIsoFiles(ProjectManifest &manifest, const std::function<bool(const std::string &file)> &usable);
```

- [ ] **Step 4: Implementar — `project-manifest.cpp`**

1. No `namespace {}` (depois de `fpsValue`), acrescente:
```cpp
Json originJson(const CaptureOrigin &origin)
{
	Json json = Json::object();
	json["tipo"] = origin.type;
	json["id"] = origin.id;
	json["retangulo"] = Json::array({origin.rect[0], origin.rect[1], origin.rect[2], origin.rect[3]});
	json["escala"] = origin.scale;
	return json;
}

void readOrigin(const Json &json, CaptureOrigin &origin)
{
	origin.type = getString(json, "tipo");
	origin.id = getString(json, "id");
	origin.scale = getDouble(json, "escala", 1.0);
	if (const Json *rect = getChild(json, "retangulo", true)) {
		for (size_t i = 0; i < 4 && i < rect->size(); i++) {
			if ((*rect)[i].is_number()) {
				origin.rect[i] = static_cast<int>((*rect)[i].get<int64_t>());
			}
		}
	}
}

// Objeto `key` dentro de "arquivos", mantendo os campos que esta versão não conhece; cria se ausente/de outro tipo.
Json &fileEntry(Json &files, const char *key)
{
	Json &entry = files[key];
	if (!entry.is_object()) {
		entry = Json::object();
	}
	return entry;
}

void writeIso(Json &files, const char *key, const IsoFile &iso)
{
	if (!iso.present) {
		files.erase(std::string(key));
		return;
	}
	Json &entry = fileEntry(files, key);
	entry["arquivo"] = iso.file;
	entry["largura"] = iso.width;
	entry["altura"] = iso.height;
	entry["fps"] = fpsValue(iso.fps);
	entry["offset_ms"] = iso.offsetMs;
	if (iso.encoder.empty()) {
		entry.erase(std::string("codificador"));
	} else {
		entry["codificador"] = iso.encoder;
	}
	if (iso.hasOrigin) {
		entry["origem"] = originJson(iso.origin);
	} else {
		entry.erase(std::string("origem"));
	}
}

void readIso(const Json &files, const char *key, IsoFile &iso)
{
	const Json *entry = getChild(files, key, false);
	if (!entry) {
		return;
	}
	iso.present = true;
	iso.file = getString(*entry, "arquivo");
	iso.width = static_cast<int>(getInt(*entry, "largura", 0));
	iso.height = static_cast<int>(getInt(*entry, "altura", 0));
	iso.fps = getDouble(*entry, "fps", 0.0);
	iso.offsetMs = getInt(*entry, "offset_ms", 0);
	iso.encoder = getString(*entry, "codificador");
	if (const Json *origin = getChild(*entry, "origem", false)) {
		iso.hasOrigin = true;
		readOrigin(*origin, iso.origin);
	}
}
```
2. Em `serializeManifest`, troque o trecho inteiro que vai de `Json &files = root["arquivos"];` até o fim do bloco
   `if (m.cursor.present) { … }` por:
```cpp
	Json &files = root["arquivos"];
	if (m.program.present) {
		Json &program = fileEntry(files, "programa");
		program["arquivo"] = m.program.file;
		program["largura"] = m.program.width;
		program["altura"] = m.program.height;
		program["fps"] = fpsValue(m.program.fps);
		program["offset_ms"] = m.program.offsetMs;
		program["faixas_audio"] = m.program.audioTracks;
	} else {
		files.erase(std::string("programa"));
	}
	writeIso(files, kScreenTrack, m.screen);
	writeIso(files, kCameraTrack, m.camera);
	if (m.cursor.present) {
		Json &cursor = fileEntry(files, "cursor");
		cursor["arquivo"] = m.cursor.file;
		cursor["largura"] = m.cursor.width;
		cursor["altura"] = m.cursor.height;
		cursor["origem"] = originJson(m.cursor.origin);
	} else {
		files.erase(std::string("cursor"));
	}
```
3. Ainda em `serializeManifest`, no laço dos eventos, depois do `if (event.code != 0) { … }`:
```cpp
		if (!event.reason.empty()) {
			item["motivo"] = event.reason;
		}
```
4. Em `parseManifest`, dentro de `if (const Json *files = getChild(root, "arquivos", false)) {`: troque o bloco de
   leitura do `origem` do cursor (o `if (const Json *origin = getChild(*cursor, "origem", false)) { … }` inteiro) por
```cpp
			if (const Json *origin = getChild(*cursor, "origem", false)) {
				readOrigin(*origin, m.cursor.origin);
			}
```
   e, depois do bloco do `programa`, acrescente:
```cpp
		readIso(*files, kScreenTrack, m.screen);
		readIso(*files, kCameraTrack, m.camera);
```
5. Ainda em `parseManifest`, troque o `push_back` dos eventos por:
```cpp
			m.events.push_back(ManifestEvent{getInt(item, "t_ms", 0), getString(item, "tipo"),
							 getString(item, "faixa"),
							 static_cast<int>(getInt(item, "codigo", 0)),
							 getString(item, "motivo")});
```
6. No fim do arquivo, antes de `} // namespace voltz`:
```cpp
int dropUnusableIsoFiles(ProjectManifest &manifest, const std::function<bool(const std::string &file)> &usable)
{
	int dropped = 0;
	const std::pair<IsoFile *, const char *> entries[] = {{&manifest.screen, kScreenTrack},
							      {&manifest.camera, kCameraTrack}};
	for (const auto &entry : entries) {
		IsoFile &iso = *entry.first;
		if (!iso.present || usable(iso.file)) {
			continue;
		}
		iso = IsoFile{};
		manifest.events.push_back(ManifestEvent{0, "iso_falhou", entry.second, 0, "arquivo_ausente"});
		dropped++;
	}
	return dropped;
}
```

- [ ] **Step 5: Rodar e ver passar**

Run: **COMPILAR TESTES**, **RODAR TESTES** e o executável.
Expected: `100% tests passed`; `test cases: 104 | 104 passed` (os 8 casos antigos do manifesto continuam passando —
em especial "versão mais nova é lida e campos desconhecidos sobrevivem à reescrita", que agora passa pelo modelo do
`tela`). Se `files.erase(std::string(...))` não compilar com o `ordered_json`, use
`files.erase(files.find(std::string(key)))` precedido de `if (files.contains(std::string(key)))`.

- [ ] **Step 6: Compilar o app (as structs mudaram de tamanho)**

Run: **COMPILAR APP** (segundo plano).
Expected: `exit=0`.

- [ ] **Step 7: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/core/project-manifest.hpp frontend/voltz/core/project-manifest.cpp frontend/voltz/tests/test-project-manifest.cpp
git add frontend/voltz/core/project-manifest.hpp frontend/voltz/core/project-manifest.cpp frontend/voltz/tests/test-project-manifest.cpp
git commit -m "feat(voltz): manifesto com tela/camera, motivo nos eventos e campos desconhecidos em arquivos

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: `RecordingConfig` — encoders registrados, "Gravar só a versão montada" e estimativa com ISO

**Files:**
- Modify: `frontend/voltz/shell/RecordingConfig.hpp`, `frontend/voltz/shell/RecordingConfig.cpp`

- [ ] **Step 1: Declarar** — em `frontend/voltz/shell/RecordingConfig.hpp`, dentro de `class RecordingConfig`, logo
  depois de `static std::string automaticEncoder();`:
```cpp
	// Ids de todos os encoders registrados no libobs (obs_enum_encoder_types).
	static std::vector<std::string> registeredEncoderIds();
	// Ajustes › Gravação › "Gravar só a versão montada" (user.ini [Voltz] ProgramOnly; padrão desligado): sem os ISO
	// tela.mkv/camera.mkv (spec §4.3).
	static bool programOnly();
	static void setProgramOnly(bool programOnly);
```
e troque o comentário de `estimatedBytesPerMinute` por:
```cpp
	// Estimativa de bytes por minuto com a configuração atual (antes de haver medição real): programa + ISO das fontes
	// compartilhadas Tela/Câmera com imagem, se "Gravar só a versão montada" estiver desligado.
	static uint64_t estimatedBytesPerMinute();
```

- [ ] **Step 2: Implementar** — em `frontend/voltz/shell/RecordingConfig.cpp`:

1. Includes: depois de `#include "RecordingConfig.hpp"` acrescente `#include "SharedSources.hpp"`; depois de
   `#include <utility/platform.hpp> // GetDefaultVideoSavePath` acrescente `#include <OBSApp.hpp> // App()`.
2. Troque a função `RecordingConfig::automaticEncoder()` inteira por:
```cpp
std::vector<std::string> RecordingConfig::registeredEncoderIds()
{
	std::vector<std::string> ids;
	const char *id = nullptr;
	for (size_t i = 0; obs_enum_encoder_types(i, &id); i++) {
		if (id) {
			ids.emplace_back(id);
		}
	}
	return ids;
}

std::string RecordingConfig::automaticEncoder()
{
	return pickRecEncoder(registeredEncoderIds());
}

bool RecordingConfig::programOnly()
{
	config_t *user = App()->GetUserConfig();
	return config_has_user_value(user, "Voltz", "ProgramOnly") && config_get_bool(user, "Voltz", "ProgramOnly");
}

void RecordingConfig::setProgramOnly(bool programOnly)
{
	config_set_bool(App()->GetUserConfig(), "Voltz", "ProgramOnly", programOnly);
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
	blog(LOG_INFO, "[voltz] gravar so a versao montada: %s", programOnly ? "sim" : "nao");
}
```
3. Em `RecordingConfig::estimatedBytesPerMinute()`, troque a última linha `return estimateBytesPerMinute(input);` por:
```cpp
	uint64_t total = estimateBytesPerMinute(input);
	if (programOnly()) {
		return total;
	}
	/* ISO (Plano 3B): um arquivo por fonte de papel com imagem, na resolução da fonte, no fps do programa, com 1
	 * faixa AAC de 160 kbps. Qualidade "Alta" sempre (o ISO grava acima do programa). */
	for (SharedSource role : {SharedSource::Screen, SharedSource::Camera}) {
		OBSSource source = findSharedSource(role);
		const uint32_t width = source ? obs_source_get_width(source) : 0;
		const uint32_t height = source ? obs_source_get_height(source) : 0;
		if (width < 2 || height < 2) {
			continue;
		}
		RateInput iso;
		iso.width = static_cast<int>(width);
		iso.height = static_cast<int>(height);
		iso.fps = input.fps;
		iso.audioTracks = 1;
		iso.audioKbps = 160;
		total += estimateBytesPerMinute(iso);
	}
	return total;
```

- [ ] **Step 3: COMPILAR APP (segundo plano)**

Expected: `exit=0`. (Nenhum comportamento muda ainda: `programOnly()` só é lido a partir da Task 5; a estimativa só
cresce quando há fonte compartilhada — numa config limpa sem preset, igual à de antes.)

- [ ] **Step 4: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/shell/RecordingConfig.hpp frontend/voltz/shell/RecordingConfig.cpp
git add frontend/voltz/shell/RecordingConfig.hpp frontend/voltz/shell/RecordingConfig.cpp
git commit -m "feat(voltz): encoders registrados, ajuste 'gravar so a versao montada' e estimativa de disco com ISO

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: `ProjectRecorder` grava os ISO no ciclo do projeto

Troca o gancho do spike pelo fluxo definitivo: fontes de papel, cadeia de encoders do núcleo, `arquivos.tela`/
`arquivos.camera` com `offset_ms`, eventos `iso_falhou` com `motivo`, falha durante a gravação, regra de sobrecarga,
taxa de disco somando os ISO e o sinal `isoFailed` para a casca.

**Files:**
- Modify: `frontend/voltz/project/ProjectRecorder.hpp` (arquivo inteiro abaixo)
- Modify: `frontend/voltz/project/ProjectRecorder.cpp`

- [ ] **Step 1: Substituir `frontend/voltz/project/ProjectRecorder.hpp` inteiro por:**

```cpp
#pragma once

#include "IsoTrack.hpp"

#include <voltz/core/iso-encoders.hpp>
#include <voltz/core/project-manifest.hpp>
#include <voltz/core/project-markers.hpp>
#include <voltz/core/project-timeline.hpp>
#include <voltz/shell/RecordingConfig.hpp>
#include <voltz/shell/SharedSources.hpp>

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QDir>
#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <memory>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace voltz {

class CursorTracker;

// Caminho de pasta/arquivo do Qt → UTF-8 com separadores do Windows, para as funções do núcleo.
inline std::string nativeUtf8Path(const QString &path)
{
	return QDir::toNativeSeparators(path).toUtf8().toStdString();
}

// Gravação em projeto (spec §5). Toda gravação — pela casca, modo clássico, atalho, obs-websocket ou
// --startrecording — vira a pasta "<pasta de gravações>/AAAA-MM-DD HHhMM · Nome" com programa.mp4, projeto.voltz,
// marcadores.json, cursor.jsonl e, por fonte de papel Tela/Câmera, tela.mkv/camera.mkv (ISO, Plano 3B). Não conhece a
// casca: reage aos eventos do frontend do OBS e emite sinais (thread da UI).
class ProjectRecorder : public QObject {
	Q_OBJECT

public:
	explicit ProjectRecorder(QObject *parent = nullptr);
	~ProjectRecorder() override;

	// Nome do próximo projeto (user.ini [Voltz] NextProjectName; vazio = "Gravação").
	QString nextProjectName() const;
	void setNextProjectName(const QString &name);

	bool isActive() const { return active_; }
	QString currentProjectDir() const { return projectDir_; }
	QString currentProjectName() const { return QString::fromStdString(manifest_.name); }

	// "Marcador N" agora: capítulo no MP4 + marcadores.json + manifesto. Falso sem projeto gravando ou com a
	// gravação pausada (o OBS não aceita capítulo pausado).
	bool addMarker();

	// Bytes/min medidos nos arquivos do projeto atual (programa + ISO; 0 sem projeto ou antes de 10 s).
	uint64_t measuredBytesPerMinute() const;

	// Faixas ISO (tela.mkv/camera.mkv) gravando agora.
	int runningIsoCount() const;

	// Espaço livre da pasta. VOLTZ_FAKE_FREE_BYTES (só para testes) substitui o valor real. Falso se não souber.
	static bool freeBytes(const QString &folder, quint64 &out);

signals:
	void projectStarted(const QString &dir, const QString &name);
	void projectFinished(const QString &dir, const QString &status);
	void markerAdded(int number, qint64 tMs);
	void stoppedForLowDisk(quint64 freeBytes);
	void manifestWriteFailed(const QString &backupPath);
	// Uma faixa ISO ("tela" | "camera") não iniciou ou parou durante a gravação; `reason` é o "motivo" do evento
	// iso_falhou ("sem_imagem", "canvas", "sem_codificador", "saida", "erro_saida", "sobrecarga"). O programa segue.
	void isoFailed(const QString &track, const QString &reason);

private:
	static void handleEvent(enum obs_frontend_event event, void *data);
	static void handleHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void handleOutputStart(void *data, calldata_t *params);
	static void handleOutputStopping(void *data, calldata_t *params);
	static void handleOutputStop(void *data, calldata_t *params);

	void dispatch(enum obs_frontend_event event);
	void onStarting();
	void onStarted();
	void onPaused(bool paused);
	void onStopped();
	void onExit();
	void finish(ProjectStatus status, const char *eventType, int code = 0);
	void heartbeat();
	void checkDisk();
	bool writeManifest();
	bool writeMarkers();
	void registerHotkey();
	void loadHotkeyBinding();
	void unregisterHotkey();
	void startCursor();
	void stopCursor();
	void startIsoTracks();
	void startIsoTrack(SharedSource role, const char *trackName, const char *fileName, IsoFile &entry,
			   const std::vector<IsoEncoderAttempt> &attempts);
	void requestIsoStop();
	void finishIsoTracks();
	void onIsoFailed(const std::string &track, int code);
	void checkIsoLoad();

	bool registered_ = true;
	bool active_ = false;
	bool lowDiskStop_ = false;
	bool manifestWarned_ = false;
	QString projectDir_;
	ProjectManifest manifest_;
	ProjectTimeline timeline_;
	std::vector<Marker> markers_;
	std::unique_ptr<CursorTracker> cursor_;
	std::vector<std::unique_ptr<IsoTrack>> isoTracks_;
	// Falhas de início dos ISO, emitidas (isoFailed) depois do projectStarted.
	std::vector<std::pair<QString, QString>> pendingIsoFailures_;
	FrameCounters loadBaseline_;
	bool loadBaselineSet_ = false;
	ProfileOverride override_;
	QTimer heartbeat_;
	obs_hotkey_id markerHotkey_ = OBS_INVALID_HOTKEY_ID;
	OBSSignal startSignal_;
	OBSSignal stoppingSignal_;
	OBSSignal stopSignal_;
	std::atomic<uint64_t> outputStartNs_{0};
	std::atomic<uint64_t> outputStopNs_{0};
	std::atomic<long long> lastStopCode_{0};
};

} // namespace voltz
```

- [ ] **Step 2: `ProjectRecorder.cpp` — helpers no `namespace {}`**

No `namespace {}` do topo, depois de `constexpr int kHeartbeatMs = 5000;`, acrescente:
```cpp
constexpr int kIsoStopTimeoutMs = 5000;
```
e, antes do `} // namespace` que fecha esse bloco (depois de `findScreenCaptureSource`), acrescente:
```cpp
// Fonte de papel de um ISO: a compartilhada do preset (user.ini [Voltz]). Nos testes, VOLTZ_TEST_ISO_SCREEN /
// VOLTZ_TEST_ISO_CAMERA trocam pela fonte com esse nome — nunca uma captura de tela/janela/jogo.
OBSSource findIsoSource(SharedSource role)
{
	const char *variable = role == SharedSource::Screen ? "VOLTZ_TEST_ISO_SCREEN" : "VOLTZ_TEST_ISO_CAMERA";
	const QString testName = qEnvironmentVariable(variable);
	if (testName.isEmpty()) {
		return findSharedSource(role);
	}
	OBSSourceAutoRelease source = obs_get_source_by_name(testName.toUtf8().constData());
	const char *id = source ? obs_source_get_unversioned_id(source) : "";
	if (!source || strcmp(id, "monitor_capture") == 0 || strcmp(id, "window_capture") == 0 ||
	    strcmp(id, "game_capture") == 0) {
		blog(LOG_WARNING, "[voltz] %s='%s' ignorada (inexistente ou captura de tela)", variable,
		     qUtf8Printable(testName));
		return {};
	}
	blog(LOG_INFO, "[voltz] iso: fonte de teste '%s' (%s)", qUtf8Printable(testName), variable);
	return OBSSource(source.Get());
}

// Cadeia de encoders do núcleo (spec §5.4) convertida para o libobs.
std::vector<IsoEncoderAttempt> isoAttempts(RecordingQuality quality)
{
	std::vector<IsoEncoderAttempt> attempts;
	const int level = isoQualityLevel(quality);
	for (const IsoEncoderCandidate &candidate :
	     isoEncoderChain(RecordingConfig::registeredEncoderIds(), /*allowSoftware=*/false)) {
		const EncoderSettings values = isoEncoderSettings(candidate, level);
		OBSDataAutoRelease data = obs_data_create();
		for (const auto &item : values.strings) {
			obs_data_set_string(data, item.first.c_str(), item.second.c_str());
		}
		for (const auto &item : values.ints) {
			obs_data_set_int(data, item.first.c_str(), item.second);
		}
		attempts.push_back(IsoEncoderAttempt{candidate.id, OBSData(data.Get())});
	}
	return attempts;
}

FrameCounters programFrameCounters()
{
	video_t *video = obs_get_video();
	FrameCounters counters;
	counters.rendered = obs_get_total_frames();
	counters.lagged = obs_get_lagged_frames();
	counters.encoded = video ? video_output_get_total_frames(video) : 0u;
	counters.skipped = video ? video_output_get_skipped_frames(video) : 0u;
	return counters;
}
```

- [ ] **Step 3: `ProjectRecorder.cpp` — trocar o spike pelo fluxo definitivo**

1. Em `onStarted`, troque `startIsoSpike();` por `startIsoTracks();` e troque as duas linhas
```cpp
	emit projectStarted(projectDir_, QString::fromStdString(manifest_.name));
	checkDisk();
```
por
```cpp
	emit projectStarted(projectDir_, QString::fromStdString(manifest_.name));
	for (const auto &failure : pendingIsoFailures_) {
		emit isoFailed(failure.first, failure.second);
	}
	pendingIsoFailures_.clear();
	checkDisk();
```
2. Em `heartbeat`, troque
```cpp
	manifest_.pauses = timeline_.pauses();
	writeManifest();
	checkDisk();
```
por
```cpp
	manifest_.pauses = timeline_.pauses();
	checkIsoLoad();
	writeManifest();
	checkDisk();
```
3. Troque o corpo de `measuredBytesPerMinute` inteiro por:
```cpp
uint64_t ProjectRecorder::measuredBytesPerMinute() const
{
	if (!active_ || !timeline_.started()) {
		return 0;
	}
	const QDir dir(projectDir_);
	qint64 size = 0;
	auto add = [&dir, &size](bool present, const std::string &file) {
		if (!present || file.empty()) {
			return;
		}
		const QFileInfo info(dir.filePath(QString::fromStdString(file)));
		if (info.exists()) {
			size += info.size();
		}
	};
	add(manifest_.program.present, manifest_.program.file);
	add(manifest_.screen.present, manifest_.screen.file);
	add(manifest_.camera.present, manifest_.camera.file);
	/* qualificado: a função livre do núcleo tem o mesmo nome deste método */
	return voltz::measuredBytesPerMinute(static_cast<uint64_t>(size), timeline_.netMs(nowNs()));
}
```
4. **Apague** a função `ProjectRecorder::startIsoSpike()` inteira (a da Task 1) e, no lugar dela, cole:
```cpp
int ProjectRecorder::runningIsoCount() const
{
	int running = 0;
	for (const auto &track : isoTracks_) {
		if (track->running()) {
			running++;
		}
	}
	return running;
}

void ProjectRecorder::startIsoTracks()
{
	finishIsoTracks();
	pendingIsoFailures_.clear();
	loadBaselineSet_ = false;
	manifest_.screen = IsoFile{};
	manifest_.camera = IsoFile{};
	if (RecordingConfig::programOnly()) {
		blog(LOG_INFO, "[voltz] iso: desligado (gravar so a versao montada)");
		return;
	}
	const std::vector<IsoEncoderAttempt> attempts = isoAttempts(RecordingConfig::quality());
	startIsoTrack(SharedSource::Screen, kScreenTrack, kScreenIsoFileName, manifest_.screen, attempts);
	startIsoTrack(SharedSource::Camera, kCameraTrack, kCameraIsoFileName, manifest_.camera, attempts);
}

void ProjectRecorder::startIsoTrack(SharedSource role, const char *trackName, const char *fileName, IsoFile &entry,
				    const std::vector<IsoEncoderAttempt> &attempts)
{
	OBSSource source = findIsoSource(role);
	if (!source) {
		/* Sem preset (ou sem a fonte): não há papel a gravar. Sem evento — não é falha. */
		blog(LOG_INFO, "[voltz] iso %s: sem fonte de papel; sem %s", trackName, fileName);
		return;
	}
	auto track = std::make_unique<IsoTrack>(trackName);
	const std::string name = trackName;
	track->onFailed = [this, name](int code) {
		/* thread do libobs: só enfileira */
		QMetaObject::invokeMethod(this, [this, name, code]() { onIsoFailed(name, code); }, Qt::QueuedConnection);
	};
	IsoFailure failure = IsoFailure::None;
	const std::string path = nativeUtf8Path(QDir(projectDir_).filePath(QString::fromUtf8(fileName)));
	if (!track->start(source, path, attempts, failure)) {
		const char *reason = isoFailureCode(failure);
		manifest_.events.push_back(ManifestEvent{timeline_.relativeMs(nowNs()), "iso_falhou", trackName, 0, reason});
		pendingIsoFailures_.emplace_back(QString::fromUtf8(trackName), QString::fromUtf8(reason));
		return;
	}
	entry.present = true;
	entry.file = fileName;
	entry.width = static_cast<int>(track->width());
	entry.height = static_cast<int>(track->height());
	entry.fps = track->fps();
	entry.offsetMs = track->startNs() ? timeline_.relativeMs(static_cast<int64_t>(track->startNs())) : 0;
	entry.encoder = track->encoderId();
	if (role == SharedSource::Screen && strcmp(obs_source_get_unversioned_id(source), "monitor_capture") == 0) {
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		const std::string monitorId = obs_data_get_string(settings, "monitor_id");
		const MonitorDescription monitor = findMonitor(monitorId);
		if (monitor.found) {
			entry.hasOrigin = true;
			entry.origin.type = "monitor";
			entry.origin.id = monitorId;
			entry.origin.rect[0] = monitor.rect.left;
			entry.origin.rect[1] = monitor.rect.top;
			entry.origin.rect[2] = monitor.rect.width;
			entry.origin.rect[3] = monitor.rect.height;
			entry.origin.scale = monitor.scale;
		}
	}
	blog(LOG_INFO, "[voltz] iso %s: %dx%d @ %.3f fps, %s, offset %lld ms", trackName, entry.width, entry.height,
	     entry.fps, entry.encoder.c_str(), static_cast<long long>(entry.offsetMs));
	isoTracks_.push_back(std::move(track));
}

void ProjectRecorder::onIsoFailed(const std::string &track, int code)
{
	if (!active_) {
		return;
	}
	blog(LOG_WARNING, "[voltz] iso %s: parou sozinha (codigo %d); o programa continua", track.c_str(), code);
	manifest_.events.push_back(ManifestEvent{timeline_.relativeMs(nowNs()), "iso_falhou", track, code, "erro_saida"});
	writeManifest();
	emit isoFailed(QString::fromStdString(track), QStringLiteral("erro_saida"));
}

void ProjectRecorder::checkIsoLoad()
{
	if (runningIsoCount() == 0) {
		return;
	}
	const FrameCounters now = programFrameCounters();
	if (!loadBaselineSet_) {
		/* Primeira janela só calibra: o início dos encoders costuma atrasar alguns quadros. */
		loadBaseline_ = now;
		loadBaselineSet_ = true;
		return;
	}
	const bool overloaded = isoOverloaded(loadBaseline_, now);
	loadBaseline_ = now;
	if (!overloaded) {
		return;
	}
	blog(LOG_WARNING, "[voltz] iso: o programa esta perdendo quadros; desligando as faixas separadas");
	const int64_t tMs = timeline_.relativeMs(nowNs());
	QStringList stopped;
	for (const auto &track : isoTracks_) {
		if (track->running()) {
			manifest_.events.push_back(ManifestEvent{tMs, "iso_falhou", track->track(), 0, "sobrecarga"});
			stopped.append(QString::fromStdString(track->track()));
		}
	}
	finishIsoTracks();
	for (const QString &track : stopped) {
		emit isoFailed(track, QStringLiteral("sobrecarga"));
	}
}
```
   (`requestIsoStop` e `finishIsoTracks` da Task 1 ficam como estão, mas troque o `5000` de `finishIsoTracks` por
   `kIsoStopTimeoutMs`.)
5. Acrescente `#include <QStringList>` aos includes do Qt do arquivo.

- [ ] **Step 4: COMPILAR APP (segundo plano)**

Expected: `exit=0`. (`IsoFile` e `kScreenTrack` vêm do `project-manifest.hpp`; `findMonitor`/`MonitorDescription` do
`MonitorLocator.hpp`, já incluído.)

- [ ] **Step 5: Teste ao vivo — quatro gravações numa sessão**

Bloco de teste (padrão: `$programOnly = $false`, `$spike = $false`); cole também o bloco **Fontes substitutas**. No
`try`:
```powershell
# Preparo de gravação (bloco acima)
New-IsoStandIns
# A: tela + câmera, com pausa
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 4
[void](Ws '[{"requestType":"GetOutputList"}]')
[void](Ws '[{"requestType":"PauseRecord"}]'); Start-Sleep 2
[void](Ws '[{"requestType":"ResumeRecord"}]'); Start-Sleep 4
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 4
# B: a saída da câmera para no meio (simula falha do ISO); o programa e a tela seguem
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 3
[void](Ws '[{"requestType":"StopOutput","requestData":{"outputName":"voltz-iso-camera"}}]'); Start-Sleep 3
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 4
# C: câmera sem imagem (dshow com dispositivo inválido: nenhuma webcam é aberta)
Set-CameraStandIn "dshow"
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 4
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 4
# D: fechar o app gravando
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 4
Close-App; Save-Log
$dirs = Project-Dirs
"projetos: $($dirs.Count)"
$results = @(); foreach ($d in $dirs) { $results += Show-Project $d }
LogLines '\[voltz\] iso|voltz-iso-.*(paused|Total frames)|encoding lag|memory leaks|projeto finalizado'
```
Critérios:
1. `projetos: 4`, todos `status=ok`; `programa.mp4` 1920×1080 com 3 faixas de áudio em todos.
2. **A:** `arquivos.tela` = `tela.mkv`, `largura` 2560, `altura` 1440, `fps` 60, `offset_ms` entre 0 e 1000,
   `codificador` `h265_texture_amf` (ou `h264_texture_amf`), **sem** `origem` (a substituta não é captura de monitor);
   `arquivos.camera` 1280×720. `ffprobe`: `tela.mkv` vídeo hevc/h264 2560x1440 + áudio aac; `camera.mkv` 1280x720 + aac;
   `|dur_iso + offset_ms/1000 − dur_programa| ≤ 0,5 s` para os dois. Uma pausa no manifesto; log com
   `output voltz-iso-tela paused`/`unpaused` e o mesmo para a câmera. Sem `iso_falhou`.
3. **B:** evento `{"tipo":"iso_falhou","faixa":"camera","motivo":"erro_saida"}` com `t_ms` entre 2000 e 5000; log
   `[voltz] iso camera: parou sozinha`; `camera.mkv` existe e é **mais curto** que o programa (~3 s a menos); `tela.mkv`
   com duração ≈ programa (critério de A); programa `status=ok`.
4. **C:** sem `arquivos.camera`, sem `camera.mkv`, evento `iso_falhou` com `faixa` `camera` e `motivo` `sem_imagem`;
   log `[voltz] iso camera: fonte 'Camera substituta' sem imagem`; `tela.mkv` normal.
5. **D:** evento `app_fechado_gravando`; `tela.mkv` com `dur` > 2 s no `ffprobe` (MKV finalizado antes de o libobs
   desligar); câmera como em C; log sem `a parada passou de`.
6. Log com `Number of memory leaks: 0` e nenhuma linha `[voltz] … ignorada` (as variáveis de teste acharam as
   substitutas).

Se o critério 5 falhar porque o `EXIT` chega com o vídeo já parado (o `tela.mkv` sem duração), reporte com o log — não
há como finalizar depois do `EXIT` sem patch.

- [ ] **Step 6: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/project/ProjectRecorder.hpp frontend/voltz/project/ProjectRecorder.cpp
git add frontend/voltz/project/ProjectRecorder.hpp frontend/voltz/project/ProjectRecorder.cpp
git commit -m "feat(voltz): ISO tela.mkv e camera.mkv no ciclo do projeto (offset, pausa, falha, sobrecarga)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Recuperação e Projetos cientes dos ISO

**Files:**
- Modify: `frontend/voltz/project/ProjectLibrary.cpp` (função `recover`)
- Sem mudança (conferir no teste): `ProjectLibrary::scan` já soma **todos** os arquivos da pasta no tamanho
  (`entry.sizeBytes`), então a coluna Tamanho de Projetos inclui `tela.mkv`/`camera.mkv`.

- [ ] **Step 1: Trocar `ProjectLibrary::recover` inteira por:**

```cpp
bool ProjectLibrary::recover(const QString &projectDir)
{
	ProjectManifest manifest;
	bool newer = false;
	if (!readManifest(projectDir, manifest, &newer) || newer || manifest.status != ProjectStatus::Recording) {
		return false;
	}
	const QDir dir(projectDir);
	const QDateTime created = QDateTime::fromString(QString::fromStdString(manifest.createdAt), Qt::ISODate);
	/* Duração estimada pelo arquivo modificado por último (programa ou ISO): todos crescem até a queda. */
	qint64 fileElapsedMs = 0;
	auto elapsedOf = [&dir, &created, &fileElapsedMs](bool present, const std::string &file) {
		if (!present || file.empty() || !created.isValid()) {
			return;
		}
		const QFileInfo info(dir.filePath(QString::fromStdString(file)));
		if (info.exists()) {
			fileElapsedMs = std::max<qint64>(fileElapsedMs, created.msecsTo(info.lastModified()));
		}
	};
	elapsedOf(manifest.program.present, manifest.program.file);
	elapsedOf(manifest.screen.present, manifest.screen.file);
	elapsedOf(manifest.camera.present, manifest.camera.file);
	/* MKV escrito aos poucos sobrevive à queda; ISO sem arquivo (ou vazio) sai do manifesto com iso_falhou. */
	const int dropped = dropUnusableIsoFiles(manifest, [&dir](const std::string &file) {
		const QFileInfo info(dir.filePath(QString::fromStdString(file)));
		return !file.empty() && info.exists() && info.size() > 0;
	});
	recoverManifest(manifest, fileElapsedMs);
	const bool ok = writeFileAtomically(nativeUtf8Path(dir.filePath(QString::fromUtf8(kManifestFileName))),
					    serializeManifest(manifest));
	blog(LOG_INFO, "[voltz] recuperar projeto (%s, %lld ms, %d ISO sem arquivo): %s", ok ? "ok" : "falhou",
	     static_cast<long long>(manifest.durationMs), dropped, qUtf8Printable(projectDir));
	return ok;
}
```

- [ ] **Step 2: COMPILAR APP (segundo plano)**

Expected: `exit=0`.

- [ ] **Step 3: Teste ao vivo — recuperação e tamanho**

Bloco de teste salvo **com BOM** (usa "Recuperar gravações"); cole também o bloco **Fontes substitutas**. No `try`:
```powershell
# Preparo de gravação (bloco acima)
New-IsoStandIns
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 3
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 4
$real = (Project-Dirs)[-1].FullName
$sum = (Get-ChildItem -LiteralPath $real -File | Measure-Object Length -Sum).Sum
"soma da pasta real: $sum bytes (" + ((Get-ChildItem -LiteralPath $real -File | ForEach-Object { $_.Name }) -join ", ") + ")"
# Projeto interrompido com ISO: tela.mkv nunca chegou a ser criado; camera.mkv existe e foi o último a ser escrito
$utf8 = New-Object Text.UTF8Encoding $false
$broken = Join-Path $out "2026-01-02 10h00 - Interrompido ISO"; New-Item -ItemType Directory -Force $broken | Out-Null
$brokenJson = '{"formato":"voltz-projeto","versao":1,"app":"Voltz Studio 1.0.0","motor":"libobs 32.2.2","nome":"Interrompido ISO","criado_em":"2026-01-02T10:00:00-03:00","status":"gravando","relogio":"obs_ns","t0_ns":1,"duracao_ms":30000,"pausas":[],"arquivos":{"programa":{"arquivo":"programa.mp4","largura":1920,"altura":1080,"fps":60,"offset_ms":0,"faixas_audio":3,"extra":"x"},"tela":{"arquivo":"tela.mkv","largura":2560,"altura":1440,"fps":60,"offset_ms":40,"codificador":"h265_texture_amf"},"camera":{"arquivo":"camera.mkv","largura":1280,"altura":720,"fps":60,"offset_ms":70,"codificador":"h265_texture_amf"}},"eventos":[]}'
[IO.File]::WriteAllText((Join-Path $broken "projeto.voltz"), $brokenJson, $utf8)
$fakeProgram = Join-Path $broken "programa.mp4"; [IO.File]::WriteAllBytes($fakeProgram, (New-Object byte[] 2048))
(Get-Item -LiteralPath $fakeProgram).LastWriteTime = [DateTime]::Parse("2026-01-02T10:00:42-03:00")
$fakeCamera = Join-Path $broken "camera.mkv"; [IO.File]::WriteAllBytes($fakeCamera, (New-Object byte[] 4096))
(Get-Item -LiteralPath $fakeCamera).LastWriteTime = [DateTime]::Parse("2026-01-02T10:00:50-03:00")
[void](Uia "Projetos"); Start-Sleep 2; Shot $shell "t6-projetos"
[void](Uia "Recuperar gravações"); Start-Sleep 2; Shot $shell "t6-recuperado"
Read-Json (Join-Path $broken "projeto.voltz") | ConvertTo-Json -Depth 6 | Out-Host
Close-App; Save-Log
LogLines 'recuperar projeto'
```
(Os arquivos `programa.mp4`/`camera.mkv` do projeto falso são só zeros criados pelo teste — nada a inspecionar.)
Critérios:
1. `t6-projetos`: o projeto real ("Gravação", ~00:00:03, `OK`) com **Tamanho** igual à `soma da pasta real` impressa
   (programa + tela + câmera + json/jsonl; arredondado como a tela mostra) e "Interrompido ISO" `Recuperável` com a faixa
   "Recuperar gravações".
2. `t6-recuperado`: "Interrompido ISO" `Recuperado`, duração `00:00:50`.
3. Manifesto de "Interrompido ISO": `status: recuperado`; `duracao_ms` entre 49000 e 51000 (mtime da **câmera**, não do
   programa); **sem** `arquivos.tela`; `arquivos.camera` intacto (com `codificador`); `arquivos.programa.extra = "x"`
   preservado; eventos `iso_falhou` (`faixa: tela`, `motivo: arquivo_ausente`, `t_ms: 0`) e depois `recuperado`.
4. Log: `[voltz] recuperar projeto (ok, 50000 ms, 1 ISO sem arquivo)` (± 1000 ms).

- [ ] **Step 4: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/project/ProjectLibrary.cpp
git add frontend/voltz/project/ProjectLibrary.cpp
git commit -m "feat(voltz): recuperacao de projeto considera os ISO (duracao e arquivos ausentes)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Casca — aviso na tela Gravar e "Gravar só a versão montada" em Ajustes

**Files:**
- Modify: `frontend/voltz/shell/RecordPage.hpp`, `frontend/voltz/shell/RecordPage.cpp`
- Modify: `frontend/voltz/shell/SettingsPage.hpp`, `frontend/voltz/shell/SettingsPage.cpp`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `frontend/data/locale/voltz-en-US.ini`

- [ ] **Step 1: Textos (os dois locales, mesmas chaves)** — acrescente no fim de
  `frontend/data/locale/voltz-pt-BR.ini`:
```ini
Voltz.Settings.ProgramOnly="Gravar só a versão montada"
Voltz.Settings.ProgramOnly.Hint="Sem os arquivos separados da tela e da câmera (tela.mkv e camera.mkv). Ocupa menos disco e placa de vídeo, mas o editor não poderá remontar o layout."
Voltz.Iso.Off.Title="Gravando só a versão montada"
Voltz.Iso.Partial.Title="Uma faixa separada não está sendo gravada"
Voltz.Iso.Failed.Text="Não foi possível gravar em arquivo separado: %1. A versão montada (programa.mp4) continua sendo gravada normalmente."
Voltz.Iso.Failed.Item="%1 (%2)"
Voltz.Iso.Track.tela="a tela"
Voltz.Iso.Track.camera="a câmera"
Voltz.Iso.Reason.sem_imagem="a fonte não está enviando imagem"
Voltz.Iso.Reason.canvas="não foi possível preparar o vídeo"
Voltz.Iso.Reason.sem_codificador="nenhum codificador de vídeo da placa disponível"
Voltz.Iso.Reason.saida="não foi possível criar o arquivo"
Voltz.Iso.Reason.erro_saida="a gravação do arquivo parou"
Voltz.Iso.Reason.sobrecarga="a placa de vídeo não deu conta de todas as gravações"
```
e no fim de `frontend/data/locale/voltz-en-US.ini`:
```ini
Voltz.Settings.ProgramOnly="Record the composed video only"
Voltz.Settings.ProgramOnly.Hint="Skips the separate screen and camera files (tela.mkv and camera.mkv). Uses less disk space and GPU, but the editor won't be able to rearrange the layout."
Voltz.Iso.Off.Title="Recording the composed video only"
Voltz.Iso.Partial.Title="A separate track isn't being recorded"
Voltz.Iso.Failed.Text="Couldn't record to a separate file: %1. The composed video (programa.mp4) keeps recording normally."
Voltz.Iso.Failed.Item="%1 (%2)"
Voltz.Iso.Track.tela="the screen"
Voltz.Iso.Track.camera="the camera"
Voltz.Iso.Reason.sem_imagem="the source isn't sending video"
Voltz.Iso.Reason.canvas="the video couldn't be set up"
Voltz.Iso.Reason.sem_codificador="no GPU video encoder available"
Voltz.Iso.Reason.saida="the file couldn't be created"
Voltz.Iso.Reason.erro_saida="writing the file stopped"
Voltz.Iso.Reason.sobrecarga="the GPU couldn't keep up with all recordings"
```
Paridade: `diff <(cut -d= -f1 frontend/data/locale/voltz-en-US.ini | sort) <(cut -d= -f1 frontend/data/locale/voltz-pt-BR.ini | sort)` → sem saída.

- [ ] **Step 2: Faixa âmbar na tela Gravar**

`frontend/voltz/shell/RecordPage.hpp`: acrescente `#include <QStringList>` aos includes do Qt; em `private:`, depois de
`void refreshDiskInfo();`:
```cpp
	void showIsoFailure(const QString &track, const QString &reason);
```
e depois de `ErrorBanner *diskBanner_ = nullptr;`:
```cpp
	ErrorBanner *isoBanner_ = nullptr;
```
e depois de `bool diskBannerDismissed_ = false;`:
```cpp
	// Faixas ISO que falharam no projeto atual ("a câmera (motivo)"), na ordem em que falharam.
	QStringList isoFailures_;
```

`frontend/voltz/shell/RecordPage.cpp`:
1. No construtor, troque
```cpp
	diskBanner_->setWarning(true);
	centerColumn->addWidget(errorBanner_);
	centerColumn->addWidget(diskBanner_);
```
por
```cpp
	diskBanner_->setWarning(true);
	isoBanner_ = new ErrorBanner(this);
	isoBanner_->setWarning(true);
	centerColumn->addWidget(errorBanner_);
	centerColumn->addWidget(diskBanner_);
	centerColumn->addWidget(isoBanner_);
```
2. Troque a conexão do `projectStarted` por:
```cpp
	connect(recorder_, &ProjectRecorder::projectStarted, this, [this](const QString &, const QString &name) {
		const QSignalBlocker blocker(projectName_);
		projectName_->setEditText(name);
		isoFailures_.clear();
		isoBanner_->hide();
	});
	connect(recorder_, &ProjectRecorder::isoFailed, this, &RecordPage::showIsoFailure);
```
3. Antes de `} // namespace voltz` no fim do arquivo:
```cpp
void RecordPage::showIsoFailure(const QString &track, const QString &reason)
{
	const QString item =
		QTStr("Voltz.Iso.Failed.Item")
			.arg(QTStr(qUtf8Printable(QStringLiteral("Voltz.Iso.Track.") + track)),
			     QTStr(qUtf8Printable(QStringLiteral("Voltz.Iso.Reason.") + reason)));
	if (!isoFailures_.contains(item)) {
		isoFailures_.append(item);
	}
	/* spec §5.4: sem nenhum ISO gravando, "gravando só a versão montada". */
	const QString title = recorder_->runningIsoCount() == 0 ? QTStr("Voltz.Iso.Off.Title")
							       : QTStr("Voltz.Iso.Partial.Title");
	isoBanner_->showError(title, QTStr("Voltz.Iso.Failed.Text").arg(isoFailures_.join(QStringLiteral(", ")).toHtmlEscaped()));
	blog(LOG_INFO, "[voltz] aviso de ISO: %s (%s)", qUtf8Printable(track), qUtf8Printable(reason));
}
```

- [ ] **Step 3: Checkbox em Ajustes › Gravação**

`frontend/voltz/shell/SettingsPage.hpp`, depois de `QLabel *encoder_ = nullptr;`:
```cpp
	QCheckBox *programOnly_ = nullptr;
```

`frontend/voltz/shell/SettingsPage.cpp`:
1. Em `buildRecordingSection`, troque
```cpp
	form.addRow(formLabel(QTStr("Voltz.Settings.Encoder"), recording_), encoder_);
	body->addLayout(form.layout());
```
por
```cpp
	form.addRow(formLabel(QTStr("Voltz.Settings.Encoder"), recording_), encoder_);
	body->addLayout(form.layout());

	programOnly_ = new QCheckBox(QTStr("Voltz.Settings.ProgramOnly"), recording_);
	auto *programOnlyHint = new QLabel(QTStr("Voltz.Settings.ProgramOnly.Hint"), recording_);
	programOnlyHint->setObjectName("muted");
	programOnlyHint->setWordWrap(true);
	body->addSpacing(4);
	body->addWidget(programOnly_);
	body->addWidget(programOnlyHint);
	connect(programOnly_, &QCheckBox::toggled, this, [this](bool checked) {
		if (!loading_) {
			RecordingConfig::setProgramOnly(checked);
		}
	});
```
2. Em `loadRecording`, depois de `encoder_->setText(encoderLabel());`:
```cpp
	programOnly_->setChecked(RecordingConfig::programOnly());
```
3. Em `setRecordingLocked`, depois de `quality_->setEnabled(editable);`:
```cpp
	/* Vale também no modo Avançado (o ISO não depende do modo de saída); só trava durante a gravação. */
	programOnly_->setEnabled(!locked);
```

- [ ] **Step 4: COMPILAR APP (segundo plano)**

Expected: `exit=0`.

- [ ] **Step 5: Teste ao vivo — faixa, Ajustes travado e "só a versão montada"**

Bloco de teste salvo **com BOM** (usa "Gravar só a versão montada"); cole também o bloco **Fontes substitutas**. No
`try`:
```powershell
# Preparo de gravação (bloco acima)
New-IsoStandIns
Set-CameraStandIn "dshow"        # câmera sem imagem → faixa âmbar (tela continua gravando)
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 2
[void](Uia "Gravar")             # gravando, o botão se chama "Parar": "Gravar" é só o item da navegação
Start-Sleep 2; Shot $shell "t7-faixa-parcial"
[void](Uia "Ajustes"); Start-Sleep 2; Shot $shell "t7-ajustes-gravando"
[void](Ws '[{"requestType":"StopOutput","requestData":{"outputName":"voltz-iso-tela"}}]'); Start-Sleep 2
[void](Uia "Gravar"); Start-Sleep 2; Shot $shell "t7-faixa-so-montada"
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 4
[void](Uia "Ajustes"); Start-Sleep 1
[void](Uia "Gravar só a versão montada"); Start-Sleep 1; Shot $shell "t7-ajustes-marcado"
"user.ini: " + (Select-String -LiteralPath (Join-Path $voltzCfg "user.ini") -SimpleMatch "ProgramOnly" | ForEach-Object { $_.Line })
[void](Ws '[{"requestType":"StartRecord"}]'); Start-Sleep 3
[void](Ws '[{"requestType":"StopRecord"}]'); Start-Sleep 4
[void](Uia "Gravar só a versão montada"); Start-Sleep 1
"user.ini depois: " + (Select-String -LiteralPath (Join-Path $voltzCfg "user.ini") -SimpleMatch "ProgramOnly" | ForEach-Object { $_.Line })
Close-App; Save-Log
foreach ($d in Project-Dirs) { [void](Show-Project $d) }
LogLines 'aviso de ISO|iso: desligado|gravar so a versao montada'
```
Critérios:
1. `t7-faixa-parcial`: faixa **âmbar** acima da prévia com "Uma faixa separada não está sendo gravada" e "Não foi
   possível gravar em arquivo separado: a câmera (a fonte não está enviando imagem). A versão montada (programa.mp4)
   continua sendo gravada normalmente." A prévia mostra só os blocos de cor da "Cena ISO".
2. `t7-ajustes-gravando`: seção Gravação travada (aviso "As opções de gravação ficam bloqueadas…") com o checkbox
   "Gravar só a versão montada" **desabilitado** e a dica abaixo dele.
3. `t7-faixa-so-montada`: mesma faixa com o título **"Gravando só a versão montada"** e os dois itens ("a câmera (…),
   a tela (a gravação do arquivo parou)").
4. `t7-ajustes-marcado`: checkbox marcado; `user.ini: ProgramOnly=true`; depois do segundo toggle,
   `user.ini depois: ProgramOnly=false`.
5. Projetos: o 1º com `tela.mkv` (curto) e `camera` ausente (`iso_falhou` `sem_imagem` e `iso_falhou` tela
   `erro_saida`); o 2º **sem** `tela`/`camera` no manifesto e sem `.mkv`; log com `[voltz] iso: desligado (gravar so a
   versao montada)`.
6. Log sem `QObject::connect` / `QWidget` warnings novos.

Descreva as capturas no relatório.

- [ ] **Step 6: Formatar e commitar**

```bash
"$CLANGFMT" -i frontend/voltz/shell/RecordPage.hpp frontend/voltz/shell/RecordPage.cpp frontend/voltz/shell/SettingsPage.hpp frontend/voltz/shell/SettingsPage.cpp
git add frontend/voltz/shell/RecordPage.hpp frontend/voltz/shell/RecordPage.cpp frontend/voltz/shell/SettingsPage.hpp frontend/voltz/shell/SettingsPage.cpp frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): aviso de faixa separada na tela Gravar e ajuste 'gravar so a versao montada'

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Smoke de projeto com ISO e smokes antigos protegidos

**Files:**
- Modify: `tools/voltz-smoke-project.ps1`, `tools/voltz-smoke-record.ps1`

- [ ] **Step 1: `tools/voltz-smoke-project.ps1` — cabeçalho e parâmetro**

Troque o comentário do topo (`<# … #>`) por:
```powershell
<#
  Teste de integracao da gravacao em projeto (Planos 3A e 3B).
  Abre o Voltz Studio em modo portatil com config limpa (a config portatil existente e guardada e restaurada),
  aplica os padroes Voltz ("Pular"), aponta a pasta de gravacoes para -OutDir e, via obs-websocket + UI Automation,
  grava ~12 s com troca de cena, 2 marcadores (botao "Marcador" e atalho VoltzStudio.AddMarker) e uma pausa.
  ISO (3B): as fontes de papel Tela/Camera sao trocadas por fontes substitutas de cor solida (2560x1440 e 1280x720,
  numa cena que nunca vai ao programa) via VOLTZ_TEST_ISO_SCREEN/VOLTZ_TEST_ISO_CAMERA - nunca a tela real nem a webcam.
  Fecha o app e valida a pasta do projeto: programa.mp4, projeto.voltz (status ok, pausa, duracoes, tela/camera),
  marcadores.json, cursor.jsonl e, com ffprobe (so metadados), 3 faixas de audio + capitulos no programa e resolucao,
  audio e duracao (+ offset) de tela.mkv/camera.mkv. Com -ProgramOnly ("Gravar so a versao montada") confere que nao ha ISO.
  Nao move o mouse nem digita. Audio global mutado. Apaga -OutDir no fim (use -Keep para inspecionar metadados).
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-project.ps1 [-OutDir <pasta>] [-Keep] [-ProgramOnly]
#>
```
e troque o bloco `param(` por:
```powershell
param(
    [string]$Config = "RelWithDebInfo",
    [string]$OutDir = (Join-Path $env:TEMP "voltz-smoke-project"),
    [switch]$Keep,
    [switch]$ProgramOnly
)
```

- [ ] **Step 2: `user.ini`, variáveis de ambiente e limpeza**

Troque a linha
```powershell
[IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[Voltz]`r`nNextProjectName=Smoke projeto`r`n", (New-Object Text.UTF8Encoding $false))
```
por
```powershell
$voltzIni = "[Voltz]`r`nNextProjectName=Smoke projeto`r`n"
if ($ProgramOnly) { $voltzIni += "ProgramOnly=true`r`n" }
[IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n" + $voltzIni, (New-Object Text.UTF8Encoding $false))
```
Troque a linha
```powershell
$proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
```
por
```powershell
# ISO: fontes de papel = substitutas de cor solida (criadas abaixo); nunca a tela real nem a webcam
$env:VOLTZ_TEST_ISO_SCREEN = "Tela substituta"
$env:VOLTZ_TEST_ISO_CAMERA = "Camera substituta"
$proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
```
e, no `finally`, logo depois de `Check "encerrou limpo ao fechar a janela" $cleanExit`:
```powershell
    Remove-Item Env:VOLTZ_TEST_ISO_SCREEN, Env:VOLTZ_TEST_ISO_CAMERA -ErrorAction SilentlyContinue
```

- [ ] **Step 3: Criar as substitutas** — logo depois da linha
  `if ($monitorId) { [void](Ws ('[{"requestType":"SetInputSettings"…` (fim do bloco da "Tela teste"):
```powershell

    # Substitutas do ISO: cor solida numa cena que nunca vai ao programa (trocar de cena nao afeta o ISO)
    $r = Ws ('[{"requestType":"CreateInput","requestData":{"sceneName":"Fontes de teste","inputName":"Tela substituta","inputKind":"color_source_v3","inputSettings":{"color":4278229248,"width":2560,"height":1440}}},' +
        '{"requestType":"CreateInput","requestData":{"sceneName":"Fontes de teste","inputName":"Camera substituta","inputKind":"color_source_v3","inputSettings":{"color":4294901760,"width":1280,"height":720}}}]')
    Check "fontes substitutas do ISO criadas" ($r.Count -eq 2 -and $r[0].ok -and $r[1].ok)
```

- [ ] **Step 4: Validação do manifesto** — troque a linha
```powershell
    Check "manifesto sem tela/camera (ficam para o 3B)" (-not $m.arquivos.tela -and -not $m.arquivos.camera)
```
por
```powershell
    $tela = $m.arquivos.tela
    $camera = $m.arquivos.camera
    if ($ProgramOnly) {
        Check "so a versao montada: manifesto sem tela/camera" (-not $tela -and -not $camera)
        Check "so a versao montada: sem tela.mkv/camera.mkv" (-not (Test-Path -LiteralPath (Join-Path $dir "tela.mkv")) -and -not (Test-Path -LiteralPath (Join-Path $dir "camera.mkv")))
    } else {
        Check "manifesto: tela.mkv 2560x1440 no fps do programa" ($tela -and $tela.arquivo -eq "tela.mkv" -and $tela.largura -eq 2560 -and $tela.altura -eq 1440 -and $tela.fps -eq $m.arquivos.programa.fps)
        Check "manifesto: camera.mkv 1280x720" ($camera -and $camera.arquivo -eq "camera.mkv" -and $camera.largura -eq 1280 -and $camera.altura -eq 720)
        Check "manifesto: offset_ms dos ISO entre 0 e 1000 ($($tela.offset_ms) / $($camera.offset_ms))" ($tela -and $camera -and $tela.offset_ms -ge 0 -and $tela.offset_ms -le 1000 -and $camera.offset_ms -ge 0 -and $camera.offset_ms -le 1000)
        Check "manifesto: codificador dos ISO ($($tela.codificador))" ($tela -and $camera -and [bool]$tela.codificador -and [bool]$camera.codificador)
        Check "manifesto: nenhum iso_falhou" (@(@($m.eventos) | Where-Object { $_.tipo -eq "iso_falhou" }).Count -eq 0)
    }
```
e, depois de `Check "log: sem 'a gravacao nao iniciou'" …`:
```powershell
    if ($ProgramOnly) {
        Check "log: iso desligado" (LogHas '[voltz] iso: desligado')
    } else {
        Check "log: ISO pausados e retomados com o programa" ((LogHas 'output voltz-iso-tela paused') -and (LogHas 'output voltz-iso-tela unpaused') -and (LogHas 'output voltz-iso-camera paused') -and (LogHas 'output voltz-iso-camera unpaused'))
        Check "log: fontes de teste do ISO usadas" (LogHas "[voltz] iso: fonte de teste 'Tela substituta'")
    }
```

- [ ] **Step 5: Validação com ffprobe dos ISO** — dentro do `if ($ffprobe) {`, logo depois do bloco
  `if ($ch1 -and $markers.Count -ge 1) { … } else { … }`:
```powershell
        if (-not $ProgramOnly) {
            $progDur = [double]$probe.format.duration
            foreach ($iso in @(@{ f = "tela.mkv"; w = 2560; h = 1440; e = $tela }, @{ f = "camera.mkv"; w = 1280; h = 720; e = $camera })) {
                $isoPath = Join-Path $dir $iso.f
                if (-not (Test-Path -LiteralPath $isoPath) -or -not $iso.e) { Check "ffprobe: $($iso.f) existe" $false; continue }
                # so metadados (nunca quadros)
                $ip = (& $ffprobe -v error -show_entries "stream=codec_type,codec_name,width,height:format=duration" -of json $isoPath) | Out-String | ConvertFrom-Json
                $v = @($ip.streams | Where-Object { $_.codec_type -eq "video" })
                $a = @($ip.streams | Where-Object { $_.codec_type -eq "audio" })
                Check "ffprobe: $($iso.f) 1 video $($iso.w)x$($iso.h) hevc/h264 + 1 audio aac" ($v.Count -eq 1 -and $v[0].width -eq $iso.w -and $v[0].height -eq $iso.h -and @("hevc", "h264") -contains $v[0].codec_name -and $a.Count -eq 1 -and $a[0].codec_name -eq "aac")
                $isoDur = [double]$ip.format.duration
                $gap = [math]::Abs($isoDur + [double]$iso.e.offset_ms / 1000.0 - $progDur)
                Check "ffprobe: $($iso.f) duracao + offset ~ programa +-0,5 s ($([math]::Round($isoDur, 2)) s + $($iso.e.offset_ms) ms x $([math]::Round($progDur, 2)) s)" ($gap -le 0.5)
            }
        }
```

- [ ] **Step 6: `tools/voltz-smoke-record.ps1` — defesa em profundidade** (config limpa não tem fontes de papel, mas o
  smoke de gravação não deve nunca gerar ISO). Troque
```powershell
    "[General]`r`nConfirmOnExit=false`r`n", (New-Object System.Text.UTF8Encoding $false))
```
por
```powershell
    "[General]`r`nConfirmOnExit=false`r`n[Voltz]`r`nProgramOnly=true`r`n", (New-Object System.Text.UTF8Encoding $false))
```

- [ ] **Step 7: Rodar os smokes** (backup/restauração da config portátil: o smoke de projeto guarda e restaura sozinho;
  antes do SMOKE GRAVAÇÃO, que apaga a config, copie `build_x64/rundir/RelWithDebInfo/config` para
  `config.3bbak-smoke` e devolva depois)

Run:
```bash
powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-project.ps1 -OutDir "C:\Users\Cassio\AppData\Local\Temp\claude\C--Users-Cassio-Documents-Projetos-IA-voltz-studio\eee63625-8704-4453-920b-073461dee2a9\scratchpad\smoke-projeto"
powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-project.ps1 -OutDir "C:\Users\Cassio\AppData\Local\Temp\claude\C--Users-Cassio-Documents-Projetos-IA-voltz-studio\eee63625-8704-4453-920b-073461dee2a9\scratchpad\smoke-projeto" -ProgramOnly
powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-record.ps1
```
Expected: as duas execuções do smoke de projeto terminam em `SMOKE PROJETO OK` (todas as linhas `OK`, inclusive as
novas de ISO na primeira e as de "so a versao montada" na segunda); o smoke de gravação termina sem `FALHOU`.

- [ ] **Step 8: Commitar**

```bash
git add tools/voltz-smoke-project.ps1 tools/voltz-smoke-record.ps1
git commit -m "test(voltz): smoke de projeto valida tela.mkv/camera.mkv e o modo so a versao montada

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Verificação, registro e push

**Files:** `docs/voltz/backlog-plano-3.md`, `docs/voltz/upstream-patches.md` (conferência), `CLAUDE.md`, `AGENTS.md`,
`docs/HISTORICO.md`

- [ ] **Step 1:** COMPILAR TUDO (segundo plano), RODAR TESTES (todos passam; informe o total — esperado **104** casos),
  SMOKE, SMOKE GRAVAÇÃO, SMOKE PROJETO e SMOKE PROJETO `-ProgramOnly` (com backup/restauração da config portátil, como
  na Task 8).
- [ ] **Step 2:** Capturas finais **só da janela** (bloco de teste, config limpa, **Preparo de gravação**, **Fontes
  substitutas** com a câmera em `dshow` e uma gravação de 3 s para Projetos não ficar vazia): Gravar durante a gravação
  (faixa âmbar), Projetos, Ajustes. Descreva cada uma no relatório. Nenhuma captura de quadro de vídeo.
- [ ] **Step 3:** Nenhum arquivo do OBS mudou:
  `git diff a24c10c2d..HEAD --stat -- . ':(exclude)frontend/voltz' ':(exclude)docs' ':(exclude)tools' ':(exclude)frontend/data/locale/voltz-pt-BR.ini' ':(exclude)frontend/data/locale/voltz-en-US.ini' ':(exclude)frontend/cmake/feature-voltz.cmake' ':(exclude)CLAUDE.md' ':(exclude)AGENTS.md'`
  → sem saída. `docs/voltz/upstream-patches.md` fica igual (nada a registrar no 3B). E nada de `VOLTZ_ISO_SPIKE` sobrou:
  `git grep -n "VOLTZ_ISO_SPIKE\|startIsoSpike" -- frontend` → sem saída.
- [ ] **Step 4: Backlog do Plano 3** — em `docs/voltz/backlog-plano-3.md`:
  1. Troque a seção `## Plano 3B (próximo)` inteira (o título e os itens, até antes da linha `Depois: Plano 4 …`) por:
  ```markdown
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
    sobrecarga (≥ 5% de quadros do programa atrasados/pulados em 5 s) desliga os ISO.
  - **Casca:** faixa âmbar "Gravando só a versão montada"/"Uma faixa separada não está sendo gravada"; Ajustes ›
    Gravação › "Gravar só a versão montada" (`user.ini [Voltz] ProgramOnly`). Taxa de disco medida soma os ISO; a
    estimativa inclui os ISO das fontes compartilhadas.
  - **Recuperação:** ISO sem arquivo sai do manifesto (`arquivo_ausente`); duração pelo maior mtime (programa ou ISO).
  - Testes: 104 casos; `tools/voltz-smoke-project.ps1` valida `tela.mkv`/`camera.mkv` por `ffprobe` (resolução, áudio,
    duração + offset ≈ programa) e o modo `-ProgramOnly`; fontes de papel de teste por `VOLTZ_TEST_ISO_SCREEN`/
    `VOLTZ_TEST_ISO_CAMERA`.
  - Spike (Task 1): <preencher com o resultado medido: caminho usado (canvas ou view), encoder, offsets, % de quadros
    pulados/atrasados com 3 encodes AMF em 1440p60 + 720p + 1080p>.

  ## Próximo
  Plano 4 (anotações `anotacoes.jsonl` + destaque do mouse; o grupo `Tela Voltz` não pode mudar a fonte do ISO),
  Plano 5 (marca visual, instalador, CI).
  ```
  2. Apague a linha solta `Depois: Plano 4 (anotações …), Plano 5 (marca visual, instalador, CI).` que vinha logo depois
     da antiga seção do 3B.
  3. Na seção `## Pendências e limites do 3A`, troque o item **Manifesto ao reescrever** por:
  ```markdown
  - **Manifesto ao reescrever:** campos desconhecidos dentro de `pausas[]` e `eventos[]` ainda não são preservados (os
    de `arquivos.*` passaram a ser no 3B).
  ```
  4. Acrescente, antes de `## Testes manuais pendentes do 3A`:
  ```markdown
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
  - Parar os ISO bloqueia a thread da UI até 5 s por faixa no pior caso (normalmente < 1 quadro: a parada é pedida no
    `STOPPING`).

  ## Testes manuais pendentes do 3B
  - Gravar com o preset Tutorial **real** (monitor + webcam) e conferir `tela.mkv`/`camera.mkv`: resolução nativa,
    `arquivos.tela.origem` igual ao `arquivos.cursor.origem`, e a sincronia vista pelo próprio usuário num player.
  - Carga real com 3 encodes AMF (tela 1440p/4K em movimento + webcam): log sem `skipped frames due to encoding lag` e
    sem `iso_falhou`/`sobrecarga`.
  - Desconectar a webcam durante a gravação (o programa segue; anotar o que acontece com `camera.mkv`).
  - "Gravar só a versão montada" com o preset real: nenhum `.mkv`, estimativa de espaço menor na barra inferior.
  ```
- [ ] **Step 5: `CLAUDE.md`** (depois `cp CLAUDE.md AGENTS.md`). O arquivo já passa de 10 KB (10,9 KB): **antes** de
  acrescentar, mova para `docs/HISTORICO.md` (seção nova `## Planos 2 e 3A (resumo que saiu do CLAUDE.md)`) o texto das
  linhas de *Estado atual* dos Planos 1, 2A, 2B, 2C e 3A e deixe no lugar delas uma linha só:
  `- Planos 1–3A concluídos (fundação, casca completa, presets, tema, gravação em projeto com programa.mp4/manifesto/marcadores/cursor/Projetos/avisos de disco); detalhes em docs/HISTORICO.md.`
  Depois:
  - *Estado atual*, +1 linha: `- Plano 3B concluído: ISO tela.mkv/camera.mkv por canvas privado (fonte de papel do preset, resolução nativa, HEVC/H.264 AMF→NVENC→QSV em CQP, áudio-guia), offset_ms, iso_falhou com motivo, desligamento por sobrecarga, "Gravar só a versão montada". Testes: 104 casos + smoke de projeto com ISO.`
  - *Próximos passos* → `1. Escrever e executar o Plano 4 (anotações + destaque do mouse; o grupo 'Tela Voltz' não pode mudar a fonte do ISO) a partir da spec §6 e de docs/voltz/backlog-plano-3.md.` e `2. Depois: Plano 5 (marca visual + instalador + CI).`
  - *Decisões*, +1 linha: `- ISO sem patch: IsoTrack = obs_canvas_create_private (EPHEMERAL, canal 0 = fonte compartilhada do preset) → encoder próprio + AAC da faixa 1 → ffmpeg_muxer; fps = do programa; programa segue em H.264, ISO em HEVC; testes só com fontes substitutas via VOLTZ_TEST_ISO_SCREEN/CAMERA.` (se o spike usou a view, troque "obs_canvas_create_private (EPHEMERAL…)" por "obs_view + obs_view_add2 (canvas privado falhou: <sintoma>)").
  - *Armadilhas*, +1 linha cada (só as que se confirmaram na execução): `ffmpeg_muxer`/`mp4_output` exigem encoder de
    áudio (`OBS_OUTPUT_AV`): saída só de vídeo não inicia; mix auxiliar herda o fps do principal; liberar saída →
    encoders → canvas, nessa ordem (destruir o canvas libera o mix na hora); não chamar `obs_canvas_remove` em canvas
    privado (o `OBSBasic` enfileira `RemoveCanvas` com uma ref); `obs_output_start` que falha não emite `stop`; testes
    de ISO nunca com `monitor_capture`/webcam real.
  - Conferir o tamanho: `wc -c CLAUDE.md` perto de 10 KB (nunca acima de ~12 KB); `diff CLAUDE.md AGENTS.md` sem saída.
- [ ] **Step 6:** Commit e push:
```bash
git add docs/voltz/backlog-plano-3.md docs/HISTORICO.md CLAUDE.md AGENTS.md
git commit -m "docs(voltz): registro do Plano 3B (backlog, memoria)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push origin voltz
```
Expected: push sem `--force`, `voltz -> voltz`.

---

## Auto-revisão (feita ao escrever)

- **Cobertura da spec e do backlog do 3B:** ISO `tela.mkv`/`camera.mkv` via `obs_canvas` na resolução nativa, só a
  fonte de captura, imune à troca de cena (§5.3) — Tasks 1 e 5 (o smoke da Task 8 troca de cena com as substitutas fora
  das cenas do programa); cadeia AMF → NVENC → QSV com HEVC/H.264, CQP acima do programa (§5.4) — Task 2; "sem GPU para 3
  sessões → só versão montada + aviso + evento" (§5.4, §8) — encoder que não inicia (Tasks 1/5), regra de sobrecarga
  (Tasks 2/5), faixa âmbar com o texto da spec (Task 7); `offset_ms` do primeiro quadro relativo a `t0_ns` (§5.5) —
  Task 5, validado por `ffprobe` (duração + offset ≈ programa) nas Tasks 1, 5 e 8; `arquivos.tela.origem` no formato do
  `arquivos.cursor` e `iso_falhou` com `faixa` (§5.2, backlog) — Tasks 3 e 5; ISO falhando não para o programa (§8) —
  Task 5 cenário B (`StopOutput`) e Task 7; pausa junto (§5.3) — Tasks 1/5/8 pelo log `output … paused`; fechar gravando
  e recuperação com ISO (§8) — Task 5 cenário D e Task 6; Projetos somando tudo — já somava, conferido na Task 6;
  "gravar só a versão montada" (§4.3) — Tasks 4 e 7, smoke `-ProgramOnly`; campos desconhecidos em `arquivos.*`
  (pendência do 3A) — Task 3; risco 2 da §11 (canvas + carga com 3 encodes AMF) — spike da Task 1 com critérios P1–P8 e
  alternativa `obs_view` com código; risco 3 (camadas Voltz fora do ISO) — o ISO usa a captura compartilhada, a regra
  vai para o Plano 4. Fora, com motivo: `camera_desconectada`, programa em HEVC, x264 no ISO, fps nativo da câmera,
  mudança de tamanho/limite do codificador.
- **Placeholders:** nenhum "TBD". O único texto a preencher é o resultado medido do spike no backlog (Task 9, Step 4),
  que só a execução produz. Onde só a execução decide, há critério e saída: spike (P1–P8 e árvore de decisão do Step 8),
  preset do AMF (`kIsoAmdPreset`), `erase` do `ordered_json` (Task 3, Step 5), finalização no `EXIT` (Task 5, critério 5).
- **Consistência de nomes:** `IsoTrack` (`start`, `setPaused`, `requestStop`, `finish`, `running`, `track`, `startNs`,
  `width`, `height`, `fps`, `encoderId`, `onFailed`), `IsoEncoderAttempt`, `IsoFailure`/`isoFailureCode`
  (`sem_imagem`/`canvas`/`sem_codificador`/`saida`); núcleo `EncoderFamily`, `IsoEncoderCandidate`, `isoEncoderChain`,
  `isoQualityLevel`, `EncoderSettings`, `isoEncoderSettings`, `FrameCounters`, `isoOverloaded`, `kIsoKeyframeSeconds`,
  `kIsoAmdPreset`; manifesto `IsoFile`, `ProjectManifest::screen`/`camera`, `ManifestEvent::reason` (`motivo`),
  `dropUnusableIsoFiles`, `kScreenIsoFileName`/`kCameraIsoFileName`/`kScreenTrack`/`kCameraTrack`; `ProjectRecorder`
  (`runningIsoCount`, sinal `isoFailed`, `startIsoTracks`, `startIsoTrack`, `requestIsoStop`, `finishIsoTracks`,
  `onIsoFailed`, `checkIsoLoad`, `kIsoStopTimeoutMs`); `RecordingConfig::registeredEncoderIds`/`programOnly`/
  `setProgramOnly`; casca `RecordPage::showIsoFailure`/`isoBanner_`/`isoFailures_`, `SettingsPage::programOnly_`; locale
  `Voltz.Settings.ProgramOnly*`, `Voltz.Iso.*` (todas as chaves de motivo usadas por `isoFailed` existem nos dois
  `.ini`; `arquivo_ausente` só aparece no manifesto); variáveis `VOLTZ_TEST_ISO_SCREEN`/`VOLTZ_TEST_ISO_CAMERA` iguais
  no código, no bloco de teste e no smoke; `VOLTZ_ISO_SPIKE`/`startIsoSpike` só na Task 1 e removidos na Task 5
  (conferido na Task 9); saídas `voltz-iso-tela`/`voltz-iso-camera` iguais no `IsoTrack` e nos testes.
- **Contagem de testes:** 91 (base) → 99 (Task 2: 8 casos) → 104 (Task 3: 5 casos).
