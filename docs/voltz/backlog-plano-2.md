# Backlog para o Plano 2 (vindo das revisões do Plano 1)

Itens levantados nas revisões de código do Plano 1 que ficaram de fora de propósito. Entram no Plano 2
(casca completa) a menos que indicado.

## Feito no Plano 2A
- Erros de gravação agora visíveis na casca (sinal `stop` da saída + faixa vermelha).
- `nativeEvent` da casca repassa mudanças de monitor ao display da prévia.
- Cenas por UUID (`ScenesPanel`).
- Pausa habilitada só quando a saída suporta.
- Ícones Phosphor no lugar dos glifos de texto.
- DM Sans empacotada.
- `aboutLine` traduzível (locale `Voltz.*`).
- Mensagens fixas em português do upload de log/crash movidas para o locale.
- Tema do OBS deixou de vazar para a casca (regras QSS com prefixo `#voltzShell`).

## Feito no Plano 2B
- **Presets + primeira execução:** tela Presets com miniaturas de 4 presets (Tutorial, Só tela / Só câmera, Lado a
  lado, Abertura / Pausa); aplicar monta as 5 cenas de fábrica sobre fontes compartilhadas (Tela/Câmera/Fundo por
  UUID); câmera redonda (grupo "Câmera redonda" + máscara circular). Enquanto `[Voltz] PresetsDone` não está
  marcado, o app abre em Presets com faixa de boas-vindas e "Pular".
- **Projetos:** lista as gravações da pasta de saída (abrir, mostrar na pasta, mover para a lixeira).
- **Ajustes › Gravação:** pasta, formato, qualidade e codificador automático (só no modo de saída Simples; trava
  enquanto grava). **Ajustes › Captura:** monitor, câmera e microfone pelas propriedades das fontes. **Sobre** com
  modo clássico e pasta de logs.
- **Padrões de gravação Voltz:** qualidade alta (HQ) + codificador automático + MP4 híbrido, aplicados ao escolher
  um preset e também ao Pular (só no modo Simples e se a qualidade ainda for o padrão do OBS, `Stream`).

## Casca / ciclo de vida
- **Bandeja do Windows:** ícone/menu "Mostrar/Ocultar" hoje alternam a janela clássica, não a casca.
- **"Ocultar janelas do OBS da captura"** (`SetDisplayAffinity`) e overlay da barra de tarefas não cobrem a casca.
- **`obs_frontend_get_main_window()`** ainda devolve a `OBSBasic` oculta (diálogos/docks de plugins se prendem a ela).
- **Confirmação de saída própria da casca** (hoje usa o prompt do OBS, sem mostrar a clássica).
- **Diálogo de encerramento inesperado** (`frontend/OBSApp.cpp` ~110-112) ainda mostra o aviso de privacidade do OBS
  e a caixa "Enviar relatório".
- **Menu clássico** mostra "Verificar atualizações"/"Reparar" desativados — esconder.
- **Erro de gravação avisado duas vezes:** com o `OBSBasic` oculto, o `RecordingStop` do OBS também mostra o balão
  da bandeja (`SysTrayNotify`) junto com a faixa vermelha da casca — decidir junto com a bandeja da casca.
- **Grupos no painel de Fontes:** `obs_scene_enum_items` lista só o nível de topo; um grupo aparece como uma linha,
  sem os filhos (expandir/recolher).
- **Painel de Áudio não reage a Configurações → Áudio:** decaimento e tipo do medidor só valem na criação do
  controle; reconstruir ao aplicar configurações (evento/flag a definir).
- **Acabamento do painel de Áudio (visual):** a escala de dB do `VolumeMeter` fica sobreposta na largura de 280 px
  ("-6054-48-42…"); slider azul do OBS (trocar para lima `#D4F53C` via QSS `#voltzShell`); botões de mudo/monitor e
  seta de menu com o estilo do OBS — alinhar à identidade Voltz (ícones Phosphor `speaker-high`/`microphone`).
- **Rótulos de áudio:** os nomes vêm das fontes globais do OBS (renomeadas para "Som do sistema"/"Microfone" quando
  ainda têm o nome padrão); um rótulo próprio da casca exigiria tornar `VolumeControl::setName` público.
- **macOS:** a ordem de destruição da casca depende do event loop (o flush de DeferredDelete é `#ifndef __APPLE__`) —
  irrelevante enquanto o alvo for só Windows.

- **Ícone de grupo no painel de Fontes:** "Câmera redonda" (grupo) aparece com ícone genérico do OBS; usar ícone Phosphor próprio para grupos (`obs_sceneitem_is_group`).
- **Acabamento de Projetos/Ajustes (visual):** a seta de ordenação do cabeçalho de Projetos é cinza e quase invisível,
  e o cabeçalho "Tamanho" fica 22 px à esquerda dos valores (por causa do `padding-right` da seção); a seta do
  QComboBox ainda é o triângulo do OBS — trocar por `caret-down` Phosphor.
- **Presets em monitor ultrawide:** Tutorial/Só tela usam SCALE_INNER → faixas pretas grandes em 21:9; avaliar opção de recorte.
- **Ajustes › Captura** recarrega dispositivos só ao abrir a tela (sem eventos de hotplug); o combo Microfone depende
  da fonte global do canal 3.

## Marca / i18n
- **Acessibilidade dos botões só com ícone:** dar `setAccessibleName` (= texto da dica) a todos os `QToolButton#iconButton` (Fontes: +, lixeira, setas, propriedades; olho/cadeado por linha; fechar da faixa de erro) — hoje leitores de tela e a UI Automation (`tools/voltz-uia.ps1`) não os encontram.
- "Sobre" do modo clássico ainda tem links "Doar"/"Participe" para obsproject.com.
## Testes / ferramentas
- **Testes manuais pendentes do 2A (precisam de mãos humanas):** tirar pendrive durante gravação → faixa vermelha; adicionar fonte pelo diálogo do `+`; travar/ordem/propriedades/filtros/remover com confirmação; volume/mudo; desativar microfone em Configurações → controle some.
- **Testes manuais pendentes do 2B:** mover para a lixeira pela tela Projetos; trocar câmera/monitor em Ajustes;
  checar a câmera redonda com uma webcam real.
- Smoke sem `--multi` para provar o crash sentinel (`config\Voltz Studio\.sentinel\run_*` existe durante a execução
  e some após saída limpa). O log "No crash sentinel location set" no smoke atual é efeito do `--multi`.
- `ENABLE_VOLTZ_TESTS` liga por padrão; considerar desligar no preset de release/instalador (Plano 5).

## Convivência / instalador (Plano 5)
- `plugins/win-capture/game-capture-file-init.c` (linhas ~154, 202, 291) ainda usa `%ProgramData%\obs-studio-hook\` —
  mesmo diretório do OBS instalado; isolar (ex. `voltz-studio-hook`) e conferir o graphics-hook/camada Vulkan.
- Workflows do GitHub Actions do upstream: um push de TAG no fork dispara o `push.yaml` do OBS — desativar/substituir.
- **Caminho da máscara circular:** `image_path` fica relativo (`../../data/obs-studio/voltz/mask-circle.png`), funciona porque o OBS roda de `bin/64bit`; conferir na build instalada.
