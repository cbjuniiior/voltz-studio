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
- **Revisão final:** a contagem de Voltz Mouse mostradas vazava quando a fonte era apagada ou a coleção trocada (o
  libobs só chama `hide` no tick, e fonte removida sai do tick) → o `LiveMouse` seguia lendo o mouse sem nada para
  desenhar. Cada fonte guarda se está contada e o `destroy` desconta (conferido: apagar a Voltz Mouse mostrada leva
  `showing` de 1 a 0). Conferido sem bug: escritores do `MouseFeed` (thread do `LiveMouse`, UI, websocket) e leitor
  (render) só sob o mutex; alvo/tamanho da fonte sob mutex/atômicos; o `LiveMouse` é o único que chama os
  procedimentos e para no `EXIT` (síncrono, antes do `obs_shutdown`); migração sem vazamento de referência
  (`obs_scene_insert_group` não soma ref) e segura com Tela ausente, Tela em várias cenas (um grupo por item), nome
  "Tela Voltz" já usado, Tela dentro de grupo (não migra), cena sem Tela e troca de coleção; o estilo é gravado uma
  vez, 400 ms depois da **última** mudança (temporizador único reiniciado); os smokes só apagam a config portátil
  (com backup), um `-OutDir` vazio/inexistente e `Vídeos\Voltz Studio` se ficou vazia e foi criada pelo teste.

## Feito no Plano 4B
- **Núcleo:** modelo do traço (`annotation-model`: caneta, marca-texto, linha, seta, retângulo, elipse; formas guardam
  primeiro e último ponto; linha do `anotacoes.jsonl` da spec; `AnnotationStyle` em `user.ini [Voltz] AnnotationStyle`
  com `somem_apos_s`), rasterizador por CPU em BGRA pré-multiplicado (`annotation-raster`: cápsulas + ponta da seta,
  maior cobertura por traço — marca-texto 3× a espessura a 40% sem "contas" nas junções), `StrokeStore` (um traço em
  andamento, desfazer, limpar, "somem após" no instante exato) e diário do `anotacoes.jsonl` (`annotation-journal`:
  linha completa ao terminar, linha curta `{"id","t_limpo"}` ao sair; consolidado no fim e na recuperação).
- **Plugin:** fonte **Voltz Anotações** (`voltz_annotations_source`, fora do "Adicionar fonte"), `TargetSize` comum às
  duas fontes, `AnnotationFeed` com mutex e versão alimentado por `voltz_annot_begin/extend/remove/clear/burned`;
  rasteriza só quando muda; framebuffer sRGB desligado + `ONE/INVSRCALPHA` (mesma cor da sobreposição).
- **Tela Voltz versão 2:** Tela · Voltz Anotações · Voltz Mouse; grupos versão 1 (4A) ganham a camada na abertura e na
  troca de coleção (log `[voltz] tela voltz: N grupo(s) com a camada de anotacoes`).
- **Sobreposição** (`OverlayWindow`): janela Win32 pura sobre o monitor da Tela, `WS_EX_LAYERED | TRANSPARENT |
  NOACTIVATE | TOOLWINDOW | TOPMOST`, `HTTRANSPARENT`, `MA_NOACTIVATE`, sem método que a torne clicável;
  `WDA_EXCLUDEFROMCAPTURE` sempre, conferido por `GetWindowDisplayAffinity`; criada só com traço, escondida sem traço,
  destruída no EXIT.
- **Queimadas:** sem `SetDisplayAffinitySupported()` ou com afinidade recusada → sobreposição na captura, fonte não
  desenha, `"anotacoes_queimadas": true`.
- **Projeto:** `anotacoes.jsonl` (traços já na tela no início com `t_ini = t_fim = 0`), `arquivos.anotacoes` com
  `origem` quando a Tela é captura de monitor.
- Testes: 153 casos; `tools/voltz-smoke-annotations.ps1` (fases A–D); `tools/voltz-annot-demo.ps1` (demo manual);
  fornecedor "voltz-teste-casca" (`InjectStroke`, `ExtendStroke`, `EndStroke`, `UndoStroke`, `ClearStrokes`,
  `ProbeOverlay`, `AnnotationState`) e pedidos novos no "voltz-teste" (`AnnotationFeedState`, `AddAnnotationSource`,
  `FeedStroke`, `FeedClear`); `VOLTZ_TEST_OVERLAY_RECT`, `VOLTZ_TEST_ANNOT_BURNED`, `VOLTZ_TEST_SCREEN_LAYERS`.
- **Revisão final:** sem alvo por um instante (Tela trocada, captura reiniciando com tamanho 0) a sobreposição era
  escondida, a conferência de 1 s parava e o alvo antigo ficava guardado — com o mesmo monitor de volta ela só
  reaparecia no próximo traço → agora esquece o alvo e segue conferindo enquanto houver traço; o `ProjectRecorder`
  guarda o controlador num `QPointer`. Conferido sem bug: `AnnotationFeed` só sob o mutex (procedimentos na UI,
  pedidos de teste no websocket, render copia só quando a versão muda); fornecedor da casca com mutex próprio,
  `gTarget` anulado no EXIT antes de o controlador morrer e eventos enfileirados descartados pelo Qt; `shutdown()` no
  EXIT (antes do descarregamento do plugin) destrói a sobreposição, e toda entrada do controlador que chama
  `voltz_annot_*` recusa depois dele; a janela morre com o processo numa queda; nenhum
  `SetCapture`/`SetForegroundWindow`/`SetFocus`/`SendInput`/gancho no código do 4B (grep); `ensureClickThrough` só
  acrescenta bits; diário com `flush` por linha, última linha cortada descartada na consolidação, consolidação por
  escrita atômica; migração 1 → 2 pela cena-mãe + `obs_sceneitem_group_add_item`, protegida pela versão no grupo;
  scripts só encerram o PID lançado e só apagam a config portátil (com backup), o `-OutDir` vazio e
  `Vídeos\Voltz Studio` vazia criada pelo teste.

## Próximo: Plano 4C (modo desenho)
Modo desenho com captura de entrada e saídas garantidas (Esc, clique direito, atalho, perda de foco, inatividade,
vigia da thread da UI), barra de ferramentas (Qt com `isOBSProjectorWindow` + `WDA_EXCLUDEFROMCAPTURE`), ferramenta de
texto, cores rápidas/seletor/espessura, atalhos `VoltzStudio.Draw` (Ctrl+Shift+D)/limpar/desfazer, Ajustes › Anotações
e Ajustes › Atalhos, sobreposição em todos os monitores, aviso visível de "queimadas". Ver o fim de
`docs/superpowers/plans/2026-10-16-voltz-plano-4b-anotacoes.md`.

## Pendências e limites do 4A
- O mouse ao vivo só acompanha a **Tela compartilhada** quando ela é captura de monitor; captura de janela ou outra
  captura → sem halo (mesmo limite do `cursor.jsonl`).
- **Raw Input é por processo e por uso:** se outro componente do processo registrar mouse por `RegisterRawInputDevices`,
  o último registro vence e os cliques param (o OBS 32.2.2 não registra) — conferir ao atualizar o OBS/obs-browser.
  O inverso também vale: o `RIDEV_REMOVE` do `LiveMouse` ao parar desfaz o registro de mouse do processo inteiro. Se
  algum plugin passar a usar Raw Input de mouse, registrar uma vez e não remover (ou conferir
  `GetRegisteredRawInputDevices` antes).
- Posição amostrada a 16 ms; cliques fora da captura não geram onda; o halo é desenhado por cima do cursor do Windows
  (a captura já o traz).
- Estilo é global (não por coleção/cena); mudar cor/tamanho no meio da gravação não vira evento (só o holofote).
- Migração só uma vez por coleção: a Tela compartilhada adicionada à mão numa cena nova depois fica sem grupo (usar o
  preset). Fonte Voltz Mouse sem propriedades (diálogo vazio).
- Migração marca `voltz_tela_migrada` mesmo quando o libobs recusa um grupo (aviso no log, Tela mantida como estava):
  não tenta de novo. Transição de mostrar/esconder, modo de mistura e filtro de escala do item da Tela ficam na camada
  (dentro do grupo), não no grupo.
- Fonte compartilhada achada pelo UUID do `user.ini` (global, do 2B): numa coleção cuja Tela não é a desse UUID não há
  migração nem mouse ao vivo até aplicar um preset nela.
- `LiveMouse::stop` espera a thread sem prazo (o `WM_QUIT` é reenviado por 1 s): mesmo risco teórico do
  `CursorTracker::stop`.
- `MouseFeed::snapshot` copia o vetor de ondas (até 16) a cada quadro e Voltz Mouse — alocação pequena na thread de
  render; trocar por array fixo se o perfil mostrar custo.
- O `cursor.jsonl` continua com o monitor do início da gravação; o halo ao vivo segue a troca de monitor (500 ms).
- `VOLTZ_TEST_LIVE` e `VOLTZ_TEST_PRESET_STANDINS` ficam no código de produção (como `VOLTZ_FAKE_FREE_BYTES`);
  avaliar guarda de compilação no Plano 5.
- `proc_handler` não tem remoção: o `LiveMouse` para no `EXIT`; plugin carregado sem a casca só desenha se alguém
  chamar os procedimentos.

## Testes manuais pendentes do 4A
- Preset Tutorial **real** (monitor e webcam reais): o halo segue o mouse na prévia e no programa; cliques
  esquerdo/direito/meio e toque de **touchpad** geram onda (direito na cor própria); holofote pelo **Ctrl+Shift+H no
  teclado real** com outro app em foco; monitor a **125%/150%**; **multimonitor** (monitor à esquerda do principal;
  mouse em outro monitor → sem halo).
- Gravar com o preset real e ver num player que o `programa.mp4` tem o destaque e o `tela.mkv` não.
- **Desempenho** com o halo ligado gravando por muito tempo: o mouse do sistema não fica lento; uso de CPU/GPU.
- Ajustes › Mouse: a **prévia segue o mouse real** e faz a **onda ao clicar nela**; escolher cor no **seletor de cor**;
  **arrastar os sliders** (o `user.ini` só é regravado 400 ms depois de parar); **"Salvar estilo…"** com um nome;
  trocar o atalho em Configurações → Atalhos e ver o texto novo em Ajustes.
- **Migração de uma coleção real** de antes do 4A (presets do 2B/3): cenas mantêm posição, câmera por cima; desagrupar
  e reabrir não recria o grupo.
- Textos novos em en-US.

## Pendências e limites do 4B
- Traços só entram por pedidos de teste (`VOLTZ_TEST_LIVE`): o usuário ainda não desenha (Plano 4C).
- A sobreposição só existe sobre o monitor da Tela compartilhada de captura de monitor; Tela de janela/outra captura →
  sem sobreposição (os traços continuam no programa e no `anotacoes.jsonl`).
- A fonte e a sobreposição re-rasterizam a imagem inteira a cada mudança (CPU, ~15 MB em 2560×1440): ok para traços
  injetados; no 4C, com o mouse desenhando a 60 Hz, medir e, se preciso, redesenhar só o retângulo sujo
  (`UpdateLayeredWindowIndirect` com `prcDirty`) e rasterizar fora da thread gráfica.
- `AnnotationFeed` e `StrokeStore` não limitam o número de traços (só os pontos por traço: 20 000).
- "Somem após" sem animação (o traço sai inteiro no instante `t_fim + N s`).
- O `anotacoes.jsonl` usa a Tela compartilhada do início da gravação; trocar o monitor no meio não vira evento.
- Pedidos de teste da casca são enfileirados (sem resposta síncrona): quem testa espera ~400 ms antes de ler o estado.
- O fornecedor "voltz-teste-casca" não tem `unregister` do fornecedor em si (só dos pedidos, no EXIT). Ele é
  registrado pelo frontend depois do `OBSInit`, fora do `obs_module_post_load` (funciona no obs-websocket 5.7.4, que só
  insere num mapa; rever ao atualizar o obs-websocket).
- `ensureClickThrough` refaz os bits do estilo, mas `SetWindowLongPtr` com `WS_EX_TOPMOST` não muda a ordem Z: se algo
  tirar o "sempre no topo", só um `SetWindowPos(HWND_TOPMOST, …, SWP_NOACTIVATE)` o devolve (a sobreposição continua
  transparente a cliques de qualquer jeito).
- Com `VOLTZ_TEST_LIVE` e **sem** `VOLTZ_TEST_OVERLAY_RECT`, um traço injetado põe a sobreposição sobre o monitor real
  (é o que a demo manual usa); só os pedidos de teste criam traços. Os smokes sempre definem o retângulo de teste.
- Janela "fantasma" do Windows: se a thread da UI travar > 5 s com a sobreposição visível, conferir no 4C se o DWM cria
  um fantasma dela (que receberia cliques); se criar, `DisableProcessWindowsGhosting` ou o vigia da UI do 4C.
- Nome "Voltz Anotações" cortado sem reticências na lista de Fontes em larguras menores do painel.
- A migração do 4A segue marcando `voltz_tela_migrada` mesmo quando o libobs recusa o grupo (ver as pendências do 4A);
  a versão de camadas 1 → 2 roda sempre, independente dela.
- `tools/voltz-annot-demo.ps1 -Close` só restaura a config portátil quando havia uma antes do `-Launch`; sem ela, a
  config da demo (obs-websocket sem senha na 4455) fica em `build_x64/rundir/RelWithDebInfo/config`.
- Recuperação com `anotacoes.jsonl` de ponta a ponta (derrubar o app gravando) só no roteiro manual: o OBS mostra um
  diálogo modal na abertura seguinte. O smoke cobre a recuperação de um projeto interrompido montado à mão.
- `tools/voltz-smoke-annotations.ps1` exige a porta 4455 livre (recusa se outro OBS/Voltz a usar) e leva ~6 min.

## Testes manuais pendentes do 4B
- `tools/voltz-annot-demo.ps1 -Launch` com o preset Tutorial **real**, depois `-Action desenhar`: os 5 traços aparecem
  sobre o monitor, alinhados com a prévia; clicar e digitar por baixo dos traços funciona; a sobreposição não aparece
  na barra de tarefas nem no Alt+Tab; esconder "Voltz Anotações" (olho, dentro do grupo) some os traços da prévia mas
  não do monitor (a captura não contém a sobreposição).
- Gravar ~10 s com traços: `programa.mp4` com os traços; `tela.mkv` sem; `anotacoes.jsonl` com os 5 traços em pixels
  da captura; manifesto com `arquivos.anotacoes.origem` do monitor.
- Escala 125%/150% e monitor à esquerda do principal (origem negativa): traços alinhados na sobreposição e na prévia.
- Trocar o monitor da Tela em Ajustes › Captura com traços na tela: em ~1 s a sobreposição vai para o novo monitor.
- Recuperação: fechar o app pelo Gerenciador de Tarefas gravando com traços; reabrir; Projetos › Recuperar: o
  `anotacoes.jsonl` fica consolidado (sem linhas curtas).
- `-Close`: a sobreposição some, o Voltz fecha e a config portátil volta a ser a de antes.
- Windows < 10 2004 (máquina virtual, se houver): log `queimadas`, manifesto `anotacoes_queimadas: true`, traços uma
  vez só no programa.
- Desempenho com dezenas de traços na tela: sistema responsivo; CPU parada sem mudanças.
- Textos novos em en-US ("Voltz Annotations").
