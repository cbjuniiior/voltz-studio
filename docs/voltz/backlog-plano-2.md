# Backlog para o Plano 2 (vindo das revisões do Plano 1)

> Pendências de gravação (Plano 3 em diante) ficam em `docs/voltz/backlog-plano-3.md`.

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

## Feito no Plano 2C
- **Tema "Voltz"** (`data/themes/Voltz.ovt`, variante do Yami com a paleta §4.1) é o padrão das janelas do OBS
  enquanto o usuário não escolhe outro tema (alto contraste do Windows continua no tema do sistema).
- **Bandeja controla a casca:** clique no ícone e "Mostrar/Ocultar" alternam a casca; "minimizar para a bandeja"
  e "iniciar minimizado na bandeja" valem para a casca; prévia pausa com a janela oculta/minimizada.
- **Erro de gravação avisado uma vez:** com a casca visível, sem balão da bandeja (fica a faixa vermelha).
- **Esconder o Voltz Studio da gravação** ligado por padrão (`HideOBSWindowsFromCapture`), com interruptor em
  Ajustes › Captura; estado de gravação/pausa no botão da casca na barra de tarefas.
- **Saída e falha:** confirmação própria da casca ao sair com saída ativa; diálogo de encerramento inesperado
  sem aviso de privacidade/envio e com "Abrir pasta de logs"; menu do clássico sem "Verificar atualizações"/
  "Reparar"; "Sobre" sem "Apoie"/"Doar"/"Participe".
- **Fontes:** grupos com filhos (expandir/recolher, mesma chave `collapsed` do OBS), ícone `stack`, ações nos
  filhos; nome acessível em todos os botões só-ícone (`setButtonHint`).
- **Áudio:** escala de dB legível, slider lima, mudo/monitor com ícones Phosphor (SVG pré-colorido em
  `icons/qss/`, `tools/voltz-qss-icons.sh`). Reagir a Configurações → Áudio já era coberto pelo upstream
  (`VolumeControl` ouve `profileSettingChanged`).
- **Acabamento:** seta `caret-down` nos combos (inclusive no nome dos controles de Áudio), seta de ordenação lima
  e "Tamanho" alinhado em Projetos, rótulos de Ajustes centrados; caminho da pasta de gravações com reticências no
  meio (dica com o caminho completo).

## Casca / ciclo de vida
- **`obs_frontend_get_main_window()`** ainda devolve a `OBSBasic` oculta (diálogos/docks de plugins se prendem a ela).
- **Rótulos de áudio:** os nomes vêm das fontes globais do OBS (renomeadas para "Som do sistema"/"Microfone" quando
  ainda têm o nome padrão); um rótulo próprio da casca exigiria tornar `VolumeControl::setName` público.
- **macOS:** a ordem de destruição da casca depende do event loop (o flush de DeferredDelete é `#ifndef __APPLE__`) —
  irrelevante enquanto o alvo for só Windows.
- **Presets em monitor ultrawide:** Tutorial/Só tela usam SCALE_INNER → faixas pretas grandes em 21:9; avaliar opção de recorte.
- **Bandeja com o modo clássico aberto:** a bandeja alterna só a casca; a clássica minimizada para a bandeja pelo
  OBS só volta por Ajustes → "Abrir modo clássico".
- **PowerShell 5.1 e acentos:** `.ps1` sem BOM é lido como ANSI — nomes com acento passados a `tools/voltz-uia.ps1`
  precisam de arquivo com BOM ou `[char]0xE1` etc.
- **Ajustes › Captura** recarrega dispositivos só ao abrir a tela (sem eventos de hotplug); o combo Microfone depende
  da fonte global do canal 3.

## Marca / i18n
- **Modo clássico ainda com cara de OBS:** menu Ajuda com "Portal de ajuda", "Visitar website", "Entrar no servidor do
  Discord", "Notas de lançamento" (spec §7.3: links de ajuda → GitHub do Voltz); "Sobre" com o texto "OBS Studio é um
  software livre…"; barra de título "OBS 32.2.2-…".
- **Fontes na UI Automation:** a fábrica `QAccessible` do `sourcesList` expõe os botões das linhas, mas as linhas
  deixam de ter `SelectionItem` (selecionar uma linha pela UIA não funciona; teclado/mouse ok). Avaliar expor as
  duas coisas.
- Item "Encerrar OBS" do menu da bandeja (chave `Exit` do OBS) ainda diz "OBS".
- **Tema Voltz no clássico:** com a cena vazia, o texto de ajuda do painel Fontes e "Nenhuma fonte selecionada"
  saem cortados na largura padrão do dock (conferir se o Yami faz o mesmo).

## Testes / ferramentas
- **Testes manuais pendentes do 2A (precisam de mãos humanas):** tirar pendrive durante gravação → faixa vermelha; adicionar fonte pelo diálogo do `+`; travar/ordem/propriedades/filtros/remover com confirmação; volume/mudo; desativar microfone em Configurações → controle some.
- **Testes manuais pendentes do 2B:** mover para a lixeira pela tela Projetos; trocar câmera/monitor em Ajustes;
  checar a câmera redonda com uma webcam real.
- **Testes manuais pendentes do 2C:** clique no ícone da bandeja e "Mostrar/Ocultar" alternam a casca (texto do
  item correto); "Sair" pela bandeja com a casca oculta; restaurar pela bandeja depois de minimizar para a
  bandeja; a prévia volta a desenhar depois de esconder/mostrar; ícone de estado no botão da barra de tarefas
  (gravando/pausado); erro de gravação real com a casca em outra página → volta para Gravar e sem balão;
  "Abrir pasta de logs" no diálogo de falha; "Encerrar e sair?" com câmera virtual/replay ativo; remover e mover
  filhos de grupo nas Fontes; monitor ligado (fone lima) no Áudio; reagir a Configurações → Áudio
  (decaimento/tipo de medidor) com som real; codificador mostrado com qualidade "Stream".
- Smoke sem `--multi` para provar o crash sentinel (`config\Voltz Studio\.sentinel\run_*` existe durante a execução
  e some após saída limpa). O log "No crash sentinel location set" no smoke atual é efeito do `--multi`.
- `ENABLE_VOLTZ_TESTS` liga por padrão; considerar desligar no preset de release/instalador (Plano 5).

## Convivência / instalador (Plano 5)
- `plugins/win-capture/game-capture-file-init.c` (linhas ~154, 202, 291) ainda usa `%ProgramData%\obs-studio-hook\` —
  mesmo diretório do OBS instalado; isolar (ex. `voltz-studio-hook`) e conferir o graphics-hook/camada Vulkan.
- Workflows do GitHub Actions do upstream: um push de TAG no fork dispara o `push.yaml` do OBS — desativar/substituir.
- **Caminho da máscara circular:** `image_path` fica relativo (`../../data/obs-studio/voltz/mask-circle.png`), funciona porque o OBS roda de `bin/64bit`; conferir na build instalada.
