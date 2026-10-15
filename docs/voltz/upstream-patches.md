# Alterações em arquivos do OBS (upstream)

Toda alteração em arquivo do OBS fica dentro de `#ifdef VOLTZ_STUDIO` (C/C++) ou é comentada com
"Voltz Studio" (CMake). Ao atualizar o OBS (merge de uma tag nova do upstream), revise cada item abaixo.
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
| `frontend/utility/platform-windows.cpp` | mutex `VoltzStudioCore` / `VoltzStudioPortable` | convive com OBS aberto |
| `frontend/widgets/OBSBasic_MainControls.cpp` | `UploadLog` abre pasta; links de ajuda | nada sai para obsproject.com |
| `frontend/utility/CrashHandler.cpp` | `uploadCrashLogToServer` não envia (só loga) | idem |
| `frontend/dialogs/OBSAbout.cpp` | nome/versão Voltz; sem busca do Patreon | idem + identidade |
| `frontend/widgets/OBSBasic.cpp` | sem assistente na 1ª execução; oculta na abertura em todas as plataformas (`hideWindowOnStart = true`); `closeEvent`: com casca e `event->spontaneous()` → `ignore()` + `SetShowing(false)` (só o X do sistema esconde a clássica; `close()` programático — Sair, reiniciar — segue); `promptToClose`: com casca não chama `SetShowing(true)` e usa `App()->activeWindow()` como pai | casca é a janela principal |
| `frontend/OBSApp.hpp/.cpp` | cria `VoltzShell`; apenas `voltzShell` + `HasVoltzShell()` | ciclo de vida da casca |
| `plugins/win-dshow/virtualcam-module/virtualcam-filter.cpp` | `VoltzStudio.exe` | detecção do próprio app |
| `plugins/win-dshow/virtualcam-module/virtualcam-module.cpp` | "Voltz Virtual Camera" | nome da câmera |
| `plugins/win-dshow/shared-memory-queue.c`, `shared/obs-shared-memory-queue/shared-memory-queue.c` | `VoltzVirtualCamVideo` | não colidir com a câmera do OBS |
| `.gitignore` | `!AGENTS.md`, `!CLAUDE.md`, `!installer`, `!tools` | versionar arquivos do Voltz na raiz |
