# Alterações em arquivos do OBS (upstream)

As alterações em C/C++ ficam dentro de `#ifdef VOLTZ_STUDIO`, EXCETO: (a) literais de caminho de config, que agora usam
`VOLTZ_CONFIG_DIR` (macro definida só para o alvo `obs-studio` em `feature-voltz.cmake`); (b) arquivos de plugin/shared
(`plugins/win-dshow/**`, `shared/obs-shared-memory-queue/**`): `VOLTZ_STUDIO` não é visível aos alvos de plugin, então
essas mudanças são incondicionais. Alterações em CMake são marcadas com comentários "Voltz Studio". Ao atualizar o OBS
(merge de uma tag nova do upstream), revise cada item abaixo.
Para listar tudo: `git diff ba2f32bdf..HEAD --stat` (base = tag 32.2.2).

| Arquivo | O que muda | Por quê |
|---|---|---|
| `CMakePresets.json` | preset `voltz-windows-x64` | VS 2022, opções desligadas, GUID da câmera virtual |
| `cmake/common/bootstrap.cmake` | `OBS_COMPANY_NAME`, `OBS_PRODUCT_NAME`, site, comentário, copyright | identidade nos `.rc` de todos os módulos |
| `frontend/CMakeLists.txt` | `include(cmake/feature-voltz.cmake)`; `OUTPUT_NAME VoltzStudio` | código Voltz e nome do executável |
| `frontend/cmake/os-windows.cmake` | não compila `updater` | sem atualizador do OBS |
| `frontend/cmake/windows/obs.rc.in` | versão/empresa/produto do Voltz | propriedades do `.exe` |
| `frontend/obs-main.cpp` | `opt_disable_updater = true`; caminhos de logs/crashes/profiler via `VOLTZ_CONFIG_DIR` | sem atualizador; config própria |
| 14 arquivos em `frontend/` (`git grep VOLTZ_CONFIG_DIR frontend`) | `"obs-studio/..."` (12 arquivos) e `"/obs-studio/..."` (`OBSBasic_Profiles.cpp`, `OBSBasic_SceneCollections.cpp`; 8 ocorrências com barra inicial) → `VOLTZ_CONFIG_DIR` | config em `%APPDATA%\Voltz Studio` |
| `frontend/utility/platform-windows.cpp` | mutex `VoltzStudioCore` / `VoltzStudioPortable` dentro de `#ifdef VOLTZ_STUDIO` (upstream em `#else`) | convive com OBS aberto |
| `frontend/widgets/OBSBasic_MainControls.cpp` | `UploadLog` abre pasta; links de ajuda (textos Voltz vêm do locale, chaves `Voltz.*`) | nada sai para obsproject.com |
| `frontend/utility/CrashHandler.cpp` | `uploadCrashLogToServer` não envia: loga, abre a pasta `crashes` e emite `crashLogUploadFailed` (não deixa o diálogo travado); texto Voltz vem do locale (`Voltz.*`) | idem |
| `frontend/dialogs/OBSAbout.cpp` | nome/versão Voltz (linha `aboutLine` do locale `Voltz.*`); sem busca do Patreon | idem + identidade |
| `frontend/widgets/OBSBasic.cpp` | sem assistente na 1ª execução; oculta na abertura em todas as plataformas (`hideWindowOnStart = true`); `closeEvent`: com casca e `event->spontaneous()` → `ignore()` + `SetShowing(false)` (só o X do sistema esconde a clássica; `close()` programático — Sair, reiniciar — segue); `promptToClose`: com casca não chama `SetShowing(true)` e usa `App()->activeWindow()` como pai | casca é a janela principal |
| `frontend/OBSApp.hpp/.cpp` | cria `VoltzShell`; apenas `voltzShell` + `HasVoltzShell()`; em `OBSApp::AppInit`, `voltz::loadShellLocale()` logo após `InitLocale()` + include de `voltz/shell/ShellLocale.hpp` (ambos em `#ifdef VOLTZ_STUDIO`); IsVoltzShellVisible()/ToggleVoltzShell() (bandeja); OBSInit não mostra a casca com "iniciar minimizado na bandeja" (ou `--minimize-to-tray`, lido antes do `OBSBasic::OBSInit`, que zera a flag; `extern bool opt_minimize_tray`) | ciclo de vida da casca; textos da casca no idioma do app |
| `plugins/win-dshow/virtualcam-module/virtualcam-filter.cpp` | `VoltzStudio.exe` | detecção do próprio app |
| `plugins/win-dshow/virtualcam-module/virtualcam-module.cpp` | "Voltz Virtual Camera" | nome da câmera |
| `plugins/win-dshow/shared-memory-queue.c`, `shared/obs-shared-memory-queue/shared-memory-queue.c` | `VoltzVirtualCamVideo` | não colidir com a câmera do OBS |
| `frontend/data/locale/voltz-*.ini` (novos) | textos `Voltz.*` em pt-BR e en-US, copiados para a pasta de dados do OBS | arquivos novos; não alteram os `.ini` do OBS |
| `.gitignore` | `!AGENTS.md`, `!CLAUDE.md`, `!installer`, `!tools` | versionar arquivos do Voltz na raiz |
| `frontend/OBSApp_Themes.cpp` | `InitTheme`: sem tema escolhido pelo usuário (e sem alto contraste do Windows), usa `com.voltzpixel.Voltz` | janelas do OBS com a paleta Voltz |
| `frontend/data/themes/Voltz.ovt` (novo) | variante "Voltz" do Yami (paleta da spec §4.1) | arquivo novo; não altera os temas do OBS |
| `frontend/widgets/OBSBasic_SysTray.cpp` | com casca: clique no ícone e "Mostrar/Ocultar" alternam a casca; texto do item segue a casca (`aboutToShow`); dica e título do balão "Voltz Studio" | casca é a janela principal |
| `frontend/widgets/OBSBasic_Recording.cpp` | `RecordingStop`: com a casca visível, erro de gravação não gera balão da bandeja | a casca já mostra a faixa vermelha |
