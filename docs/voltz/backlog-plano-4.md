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

## Próximo: Plano 4B (desenho na tela)
Overlay topmost por monitor com `WDA_EXCLUDEFROMCAPTURE`, barra de ferramentas, `StrokeStore` → `anotacoes.jsonl`,
fonte Voltz Anotações (camada 2 da Tela Voltz: `voltz_tela_camadas` 1 → 2), atalhos de desenho, Ajustes › Anotações e
Ajustes › Atalhos, `anotacoes_queimadas` no Windows < 10 2004. Ver o fim de
`docs/superpowers/plans/2026-10-16-voltz-plano-4a-mouse.md`.

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
