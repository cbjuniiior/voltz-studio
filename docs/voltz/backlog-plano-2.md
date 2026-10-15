# Backlog para o Plano 2 (vindo das revisões do Plano 1)

Itens levantados nas revisões de código do Plano 1 que ficaram de fora de propósito. Entram no Plano 2
(casca completa) a menos que indicado.

## Casca / ciclo de vida
- **OBRIGATÓRIO antes de qualquer teste com usuário real. Erros de gravação invisíveis:** `OBSBasic::RecordingStop` só mostra diálogos de erro `if (isVisible())`
  (OBSBasic_Recording.cpp ~190-226); com a clássica oculta, erros de encoder/disco cheio não chegam ao usuário.
  Expor erro no `RECORDING_STOPPED` para a casca ou tratar `HasVoltzShell()` como visível nesses checks.
- **`nativeEvent` da casca:** repassar `WM_MOVE`/`WM_DISPLAYCHANGE` ao `OBSQTDisplay` (como `OBSBasic::nativeEvent`,
  OBSBasic.cpp ~1780-1798) para a prévia acompanhar troca de monitor SDR/HDR.
- **Cenas por UUID:** guardar o UUID da cena em `Qt::UserRole` e usar `obs_get_source_by_uuid` (nome é ambíguo com
  canvases do OBS 32); `clearSelection()` quando a cena atual não estiver na lista.
- **Pausa só quando suportada:** habilitar ⏸ apenas se `obs_output_can_pause(obs_frontend_get_recording_output())`.
- **Bandeja do Windows:** ícone/menu "Mostrar/Ocultar" hoje alternam a janela clássica, não a casca.
- **"Ocultar janelas do OBS da captura"** (`SetDisplayAffinity`) e overlay da barra de tarefas não cobrem a casca.
- **`obs_frontend_get_main_window()`** ainda devolve a `OBSBasic` oculta (diálogos/docks de plugins se prendem a ela).
- **Confirmação de saída própria da casca** (hoje usa o prompt do OBS, sem mostrar a clássica).
- **Diálogo de encerramento inesperado** (`frontend/OBSApp.cpp` ~110-112) ainda mostra o aviso de privacidade do OBS
  e a caixa "Enviar relatório".
- **Menu clássico** mostra "Verificar atualizações"/"Reparar" desativados — esconder.
- **macOS:** a ordem de destruição da casca depende do event loop (o flush de DeferredDelete é `#ifndef __APPLE__`) —
  irrelevante enquanto o alvo for só Windows.

## Marca / i18n
- **Ícones Phosphor (pedido do usuário):** trocar todos os glifos de texto do protótipo (barra lateral ●▤◫⚙,
  botões ⏸/▶/●/■) por ícones do Phosphor Icons (`@phosphor-icons/core`, MIT, versão fixa). Só os SVGs usados vão
  para `frontend/voltz/shell/icons/` + `.qrc`; recolorir em runtime com a cor do tema (normal `#82828C`, ativo
  `#D4F53C`). Peso Regular no normal, Fill no ativo. Incluir a licença MIT do Phosphor no instalador/Sobre.
- Mensagens fixas em português no `UploadLog`/`uploadCrashLogToServer` -> mover para o locale.
- `voltz::aboutLine` tem texto em português fixo; receber o template traduzido (pt-BR/en-US) do locale da casca.
- "Sobre" do modo clássico ainda tem links "Doar"/"Participe" para obsproject.com.
- Fonte DM Sans ainda não empacotada (cai em Segoe UI); glifos ⏸/▶ podem renderizar mal — trocar por ícones.
- Tema do OBS (stylesheet global) ainda vaza para a casca em alguns widgets.

## Testes / ferramentas
- Smoke sem `--multi` para provar o crash sentinel (`config\Voltz Studio\.sentinel\run_*` existe durante a execução
  e some após saída limpa). O log "No crash sentinel location set" no smoke atual é efeito do `--multi`.
- `ENABLE_VOLTZ_TESTS` liga por padrão; considerar desligar no preset de release/instalador (Plano 5).

## Convivência / instalador (Plano 5)
- `plugins/win-capture/game-capture-file-init.c` (linhas ~154, 202, 291) ainda usa `%ProgramData%\obs-studio-hook\` —
  mesmo diretório do OBS instalado; isolar (ex. `voltz-studio-hook`) e conferir o graphics-hook/camada Vulkan.
- Workflows do GitHub Actions do upstream: um push de TAG no fork dispara o `push.yaml` do OBS — desativar/substituir.
