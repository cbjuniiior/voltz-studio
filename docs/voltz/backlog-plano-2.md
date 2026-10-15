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

## Marca / i18n
- "Sobre" do modo clássico ainda tem links "Doar"/"Participe" para obsproject.com.
## Testes / ferramentas
- **Testes manuais pendentes do 2A (precisam de mãos humanas):** tirar pendrive durante gravação → faixa vermelha; adicionar fonte pelo diálogo do `+`; travar/ordem/propriedades/filtros/remover com confirmação; volume/mudo; desativar microfone em Configurações → controle some.
- Smoke sem `--multi` para provar o crash sentinel (`config\Voltz Studio\.sentinel\run_*` existe durante a execução
  e some após saída limpa). O log "No crash sentinel location set" no smoke atual é efeito do `--multi`.
- `ENABLE_VOLTZ_TESTS` liga por padrão; considerar desligar no preset de release/instalador (Plano 5).

## Convivência / instalador (Plano 5)
- `plugins/win-capture/game-capture-file-init.c` (linhas ~154, 202, 291) ainda usa `%ProgramData%\obs-studio-hook\` —
  mesmo diretório do OBS instalado; isolar (ex. `voltz-studio-hook`) e conferir o graphics-hook/camada Vulkan.
- Workflows do GitHub Actions do upstream: um push de TAG no fork dispara o `push.yaml` do OBS — desativar/substituir.
