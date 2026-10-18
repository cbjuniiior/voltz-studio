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

## Feito no Plano 4C
- **Núcleo:** `draw-session` — `DrawSession` (entrada → `Begin/Extend/End/Undo/Exit`, pixels do desktop → captura com a
  borda presa) com todas as saídas garantidas (Esc, clique direito, perda de ativação/captura, Alt+F4, troca de
  monitores, 30 s parado, vigia: UI 1 s sem pulsar ou pulso nunca dado) e `checkDrawEntry` (saindo > `VOLTZ_TEST_LIVE` >
  recente > sem alvo); retângulo sujo no `annotation-raster` (`strokeBounds`, `rasterizeStrokeClipped`, `redrawRegion`,
  `appendDirtyRect`, `DirtyHistory`) byte a byte igual ao desenho inteiro.
- **Entrada:** `annotate/DrawInputWindow.cpp` — o único arquivo com captura/foco (`SetCapture`/`ReleaseCapture` entre
  apertar e soltar, `SetForegroundWindow` ao nascer e para devolver o primeiro plano) — janela `VoltzDrawInput` numa
  thread própria (Per-Monitor v2), alfa 1/255, sempre no topo, fora da captura, só sobre o monitor da Tela, sem dono,
  buraco na região para a barra; recusa `VOLTZ_TEST_LIVE` de novo antes de criar a thread.
- **Casca:** `DrawModeController` (atalho `VoltzStudio.Draw` Ctrl+Shift+D, `VoltzStudio.UndoAnnotation` e
  `VoltzStudio.ClearAnnotations` sem tecla padrão, botão **Desenhar** na tela Gravar, pulso de 250 ms, alvo conferido a
  cada 1 s, log com as medidas ao sair); `DrawToolbar` (6 ferramentas, 4+8 cores, 4 espessuras, desfazer, limpar,
  somem após nunca/3/5/10 s, sair; sem foco, `isOBSProjectorWindow`, fora da captura, aviso de "queimadas"); estilo
  salvo em `user.ini [Voltz] AnnotationStyle`; cliques sem onda no modo desenho; `DisableProcessWindowsGhosting`.
- **Desempenho:** sobreposição com `UpdateLayeredWindowIndirect` + `prcDirty`; fonte redesenha só a região (o envio da
  textura segue inteiro); medidas no estado de teste, no log do modo desenho e no log do plugin (a cada 600).
- **Riscos herdados resolvidos:** fantasma (desligado no processo + vigia local na thread de entrada), `ensureClickThrough`
  devolve o `WS_EX_TOPMOST` com `SetWindowPos`, Raw Input intocado (o modo desenho não registra nada), `-Close` da demo
  apaga a config que ela criou.
- Testes: 170 casos; `tools/voltz-smoke-draw.ps1` (fases A–C); `tools/voltz-draw-demo.ps1` (demo manual, `-Freeze`);
  pedidos novos `SetAnnotationStyle` e `ToolbarImage` (aceita `{"queimadas":true}`); `VOLTZ_DEBUG_FREEZE_UI` (atalho
  `VoltzStudio.DebugFreezeUi`).
- **Revisões (desvios do plano, registrados no fim dele):** a barra acha o monitor pelo `HMONITOR`
  (`screenForMonitor`; no Qt 6.11 o `QScreen::name()` é o nome amigável); barra e janela de entrada fora da captura
  sempre que suportado (sem depender de "queimadas"); o `DrawModeController` trata o EXIT no próprio callback (roda
  antes do `ProjectRecorder` e fecha o traço); a thread de entrada nunca bloqueia no mutex com a janela viva (backlog +
  `try_lock`); saída do app com a UI parada no máximo ~2 s; botão Desenhar não religa ao sair (`drawWasActive_` no
  `pressed`, guarda de 500 ms de perda de foco) e atalho/botão recusados 500 ms depois de uma saída durante uma trava
  da UI (`recusado (recente)`); `finish` fecha o traço aberto; destrutor sem uso de memória liberada (`QPointer`,
  `QSignalBlocker`); barra com fundo alfa 1, `WS_EX_NOACTIVATE` e afinidade antes do `show()`, recentraliza ao crescer e
  volta para cima da sobreposição a cada traço.

## Feito no Plano 4D
- **Núcleo:** texto no modelo (`"ferr":"texto"`, `"texto"`, `"tam"`, `"fonte"` no `anotacoes.jsonl`; âncora em
  `"pts"`), UTF-8 e limites (`annotation-text`: 280 caracteres, 12 linhas), estilo com tamanho/fonte do texto, texto em
  andamento no `StrokeStore`; layout com métricas por interface (`text-layout`); FreeType das deps + DM Sans Regular/Bold
  embutidas no `voltz-core` (`embed-fonts.cmake`, `annotation-font`) com o texto no rasterizador (mesmo desenho na
  sobreposição e na fonte do plugin, parcial = inteiro byte a byte); digitação na `DrawSession`
  (`TextBegin/TextEdit/TextCommit/TextCancel`, Esc e Ctrl+Z cancelam só o texto, toda saída confirma); atalhos
  (`hotkey-bindings`: tabela com os padrões, formato do perfil, regra de combinação, conflitos).
- **Casca:** texto ao vivo (`AnnotationController`: texto inteiro ao plugin a cada mudança, cursor só na sobreposição,
  conteúdo nunca no log); a janela de entrada traduz teclas (`TranslateMessage`/`WM_CHAR`, substitutos, `WM_SYSCHAR`
  engolido, cursor em I) sem nenhuma API de foco nova; barra com o T, tamanhos e negrito, seletor de cor completo
  (`ColorPicker`, sem diálogo) e linha de dica; Ajustes › Anotações (estilo com prévia pelo núcleo, estado, atalho) e
  Ajustes › Atalhos (`OBSHotkeyEdit` sem patch, atalhos pausados com o campo em foco, validação, conflitos, limpar e
  restaurar); marcador e holofote carregam o atalho pelo `HotkeyBinding`; Limpar e desfazer (barra/atalho) encerram
  também a digitação na `DrawSession` (`endTyping`).
- Testes: 204 casos; `tools/voltz-smoke-text.ps1` (fases A–B); `tools/voltz-draw-demo.ps1 -Hotkeys`; pedidos novos
  `InjectText`, `TypeText`, `CommitText`, `CancelText`, `HotkeyState` e `ToolbarImage {ferramenta, seletor}`; smokes com
  guarda de backup anterior e limpeza passo a passo (`Cleanup`/`Restore-Config`).

## Próximo: Plano 4E (todos os monitores, barra arrastável)
Desenho e sobreposição em todos os monitores (traços fora da captura só na sobreposição, em pixels do desktop; nada no
`anotacoes.jsonl` nem no programa), janela de entrada sobre mais de um monitor, barra arrastável com posição salva por
monitor e o buraco acompanhando; geometria no núcleo e monitores falsos fora do desktop virtual nos testes. Ver o fim de
`docs/superpowers/plans/2026-10-17-voltz-plano-4d-texto-ajustes.md`.

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
- A sobreposição só existe sobre o monitor da Tela compartilhada de captura de monitor; Tela de janela/outra captura →
  sem sobreposição (os traços continuam no programa e no `anotacoes.jsonl`).
- `AnnotationFeed` e `StrokeStore` não limitam o número de traços (só os pontos por traço: 20 000).
- "Somem após" sem animação (o traço sai inteiro no instante `t_fim + N s`).
- O `anotacoes.jsonl` usa a Tela compartilhada do início da gravação; trocar o monitor no meio não vira evento.
- Pedidos de teste da casca são enfileirados (sem resposta síncrona): quem testa espera ~400 ms antes de ler o estado.
- O fornecedor "voltz-teste-casca" não tem `unregister` do fornecedor em si (só dos pedidos, no EXIT). Ele é
  registrado pelo frontend depois do `OBSInit`, fora do `obs_module_post_load` (funciona no obs-websocket 5.7.4, que só
  insere num mapa; rever ao atualizar o obs-websocket).
- Com `VOLTZ_TEST_LIVE` e **sem** `VOLTZ_TEST_OVERLAY_RECT`, um traço injetado põe a sobreposição sobre o monitor real
  (é o que a demo manual usa); só os pedidos de teste criam traços. Os smokes sempre definem o retângulo de teste.
- Nome "Voltz Anotações" cortado sem reticências na lista de Fontes em larguras menores do painel.
- A migração do 4A segue marcando `voltz_tela_migrada` mesmo quando o libobs recusa o grupo (ver as pendências do 4A);
  a versão de camadas 1 → 2 roda sempre, independente dela.
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

## Pendências e limites do 4C
- O modo desenho só cobre o monitor da Tela compartilhada de captura de monitor; Tela de janela/outra captura → recusado
  ("sem_alvo"). Outros monitores continuam livres (clicar neles sai do modo por perda de ativação).
- Pelo atalho, o Voltz está em segundo plano e o Windows não deixa a janela de entrada ativar sozinha: até o primeiro
  clique (que a ativa e já desenha) só saem o clique direito, o mesmo atalho, o X da barra e os 30 s parado; Esc, Ctrl+Z,
  Alt+Tab, tecla Windows, outro monitor e Alt+F4 vão para o app em uso.
- Com "Desligar atalhos com o app em foco" (Configurações → Geral → Atalhos), o mesmo atalho não sai do modo desenho.
- Atalhos do OBS não consomem a tecla: Ctrl+Shift+D também chega ao app em uso. Desfazer/limpar globais sem tecla padrão.
- O vigia sai do modo quando a UI fica 1 s sem pulsar — inclusive em travadas legítimas (carregar coleção, iniciar
  gravação): o usuário liga de novo. Com a UI travada, a barra (Qt) fica parada e bloqueia a sua área do monitor até a
  UI voltar.
- Atalho/botão recusados por 500 ms depois de uma saída durante uma trava da UI (`recusado (recente)`), contados a partir
  do tratamento na UI: com uma fila muito longa depois da trava, um gatilho atrasado ainda pode religar o modo.
- O modo não sai quando a gravação começa ou para.
- `DisableProcessWindowsGhosting` vale para o processo todo: com o Voltz travado, a janela dele não vira fantasma (não
  dá para arrastá-la/minimizá-la até voltar; o Gerenciador de Tarefas funciona).
- A barra fica no alto e ao centro do monitor da Tela (não arrasta); a região da
  janela de entrada tem um buraco retangular do tamanho da barra (fundo da barra com alfa 1 para os cantos não deixarem
  o clique passar). `SetWindowRgn` numa janela em camadas (o buraco) só é confirmado pelo roteiro manual.
- Monitor da Tela não achado entre as telas do Qt → a barra cai no monitor principal (com aviso no log).
- No Windows < 10 2004 (sem `WDA_EXCLUDEFROMCAPTURE`) a barra e a janela de entrada aparecem na captura (além dos
  traços "queimados").
- O `cursor.jsonl` continua registrando os cliques feitos ao desenhar (separá-los fica para o editor).
- Medidas (smoke, sobreposição 2560×1440): parcial média 121 µs, máx. 273 µs (61 parciais); completa máx. 6 515 µs;
  fonte máx. 4 984 µs (12 parciais, 1 completa). O `raster_us` da fonte inclui o envio da textura inteira
  (`gs_texture_set_image`, ~15 MB por mudança, no máximo uma vez por quadro): o ganho do retângulo sujo na fonte é só
  na CPU. Rasterizar fora da thread gráfica só se o roteiro manual mostrar quadros perdidos.
- `redrawRegion` reconstrói a `strokeShape` de todos os traços a cada redesenho: O(N²) ao longo de um traço longo
  (limitado por `kMaxStrokePoints` = 20 000).
- `DirtyHistory` guarda 64 versões; se a fonte ficar mais atrás que isso, o redesenho é completo (contador `completas`
  no estado).
- `VOLTZ_DEBUG_FREEZE_UI` (como `VOLTZ_TEST_LIVE`) fica no código de produção; avaliar guarda de compilação no Plano 5.
- Pendências do 4B que continuam: limite de número de traços, "somem após" sem animação, monitor trocado no meio da
  gravação não vira evento no `anotacoes.jsonl`, pedidos de teste da casca enfileirados (~400 ms).
- Decisão (4D): o modo desenho não sai ao começar/parar a gravação (pelo botão Gravar já sai por foco; pelo atalho,
  quem desenha quer continuar).

## Testes manuais pendentes do 4C
- Todo o "Roteiro de teste manual do Plano 4C" do fim do plano (ligar/sair por cada caminho — inclusive pelo atalho sem
  clique antes e no meio de um traço —, cada ferramenta, barra sem roubar o Esc e por cima dos traços, botão Desenhar com
  a casca em outro monitor, vigia com `-Freeze` e atalho durante a trava, ligar logo depois de abrir/trocar de coleção,
  halo sem onda ao desenhar e com onda depois, gravação com traços e `-Close` gravando com traço aberto, escala 125/150%,
  monitor à esquerda e dois monitores do mesmo modelo, queimadas numa VM < 2004, textos em en-US).

## Pendências e limites do 4D
- Texto sem kerning, ligaduras e escritas complexas (layout próprio, sem HarfBuzz); caractere que a DM Sans não tem
  (emoji, CJK) vira o retângulo `.notdef`. Só as duas fontes embutidas (DM Sans e DM Sans Negrito).
- Sem quebra automática, cursor só no fim, sem seleção nem colar (Ctrl+V não cola); até 280 caracteres e 12 linhas;
  texto confirmado não se edita (desfazer e escrever de novo). Ctrl+Z durante a digitação cancela o texto inteiro.
- `textIsBlank` só considera U+0020, U+00A0 e `\n` (U+3000/U+2003 contam como tinta: o texto "invisível" é registrado).
- IME (chinês, japonês): a composição usa a janela padrão do Windows, no canto; não testado.
- Pelo atalho sem clique antes (janela de entrada ainda não ativa), as teclas vão para o app em uso: o primeiro clique
  ativa e já começa o texto (limite do 4C).
- Sobreposição com escala ≠ 1 (monitor ≠ captura): o tamanho do texto usa a escala média; pode diferir ~1 px do programa.
- O plugin não avisa no log quando a fonte embutida falha (só a casca avisa).
- `static EmbeddedFonts` e a ordem de destruição no descarregamento: risco teórico (o controller e a sobreposição morrem
  no EXIT).
- `discardOpen` consome um id (buraco inofensivo na numeração).
- Ajustes › Atalhos edita só a primeira tecla de cada atalho (as outras, do modo clássico, ficam e aparecem como "+N");
  combinação sem Ctrl/Alt/Windows é recusada (sozinhas, só F1–F24 e mouse lateral); conflito só avisa; o campo só ganha
  foco por clique (sem acesso só pelo teclado) e, com o foco, **todos** os atalhos do OBS ficam pausados até sair do
  campo.
- `checkVoltzHotkeyCombo` aceita só modificador como tecla (`OBS_KEY_CONTROL`/`SHIFT`/`ALT`/`META`) e nome de tecla
  inexistente com Ctrl. Só acontece num perfil editado à mão.
- O seletor de cor não tem campo hexadecimal nem lista de cores recentes; em Ajustes a cor vem do diálogo do Qt. A marca
  do quadrado do seletor tem desvio de 1 px (irrelevante).
- A seção Anotações e a de Atalhos ficam abaixo de Mouse: a rolagem e a edição por teclado só foram vistas no roteiro.
  As linhas de checkbox do Mouse (Halo, Clique, Holofote) ficam um pouco mais baixas que as demais.
- Medidas do smoke do texto (sobreposição 2560×1440, texto 96 px), por execução: parciais média 430 µs / máx 1 397 µs,
  completa 6 414 µs, fonte máx 6 303 µs; parciais 450 / 1 285 µs, completa 10 017 µs, fonte máx 17 882 µs; sobreposição
  máx 8 933 µs (parciais média 936 µs, completa 48 114 µs), fonte máx **121 277 µs** (acima de 100 ms; logo após um
  build completo: única falha do smoke); Task 12: 15 parciais média 416 µs / máx 1 359 µs, completa 6 883 µs, fonte máx
  5 771 µs. A fonte do plugin varia muito com a carga da máquina (o `raster_us` inclui o envio da textura inteira):
  pendente medir o raster da fonte do plugin com a máquina sob carga.
- Smokes: `Restore-Config`/`Cleanup` copiados em 4 smokes (extrair para um módulo comum); espera fixa de 15 s na
  abertura; os testes ao vivo trazem a casca para o primeiro plano e a entrada real do usuário pode cair nela durante o
  smoke; o smoke nunca clica no campo de atalho (o caminho de foco do `VoltzHotkeyEdit` fica no roteiro manual).
- Testes do núcleo a reforçar: UTF-8 acima de U+10FFFF (`F4 90 80 80`), sobrelonga de 3 bytes e 4 bytes truncada no
  fim; `setOpenText`/`setOpenTextStyle` com texto já encerrado; Shift+Enter no limite de 12 linhas; Esc renovando a
  atividade e Esc + 30 s sem texto; teclas recusadas (`\b`/0x1B) contando como atividade (Tick em t + ocioso − 1 ms);
  `unitsPerEm<=0` no layout; literais `U'á'` em vez de `\u` no `test-text-layout.cpp`. `decodeUtf8` tem um
  `if/continue/else if` redundante (cosmético).
- Pendências do 4B/4C que continuam: limite de número de traços, "somem após" sem animação, monitor trocado no meio da
  gravação não vira evento, pedidos de teste da casca enfileirados (~400 ms), vigia sai em travadas legítimas, atalhos
  do OBS não consomem a tecla.

## Testes manuais pendentes do 4D
- Todo o "Roteiro de teste manual do Plano 4D" do fim do plano (digitar de verdade, teclas mortas e AltGr, Shift+Enter,
  Esc/Ctrl+Z só no texto, saídas com o texto aberto, seletor de cor arrastando, linha de dica, gravação com texto,
  Ajustes › Anotações e › Atalhos com o teclado, roda do mouse em Ajustes, atalho novo com outro app em foco, modo
  clássico, 125/150%, en-US); inclui o caminho de foco do campo de atalho (clicar, gravar a tecla, sair).
