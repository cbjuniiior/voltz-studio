# Voltz Studio — Plano 2C: acabamento da casca, bandeja, saída e tema — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fechar o Plano 2: as janelas do OBS que ainda aparecem ganham o tema "Voltz"; a bandeja, o minimizar
e a saída passam a conduzir a **casca** (não a `OBSBasic` oculta); o Voltz Studio fica fora da gravação da tela
por padrão; o diálogo de falha perde o envio de relatório; o painel de Fontes mostra grupos com filhos e os
botões só-ícone ganham nome acessível; Áudio, Projetos e Ajustes recebem o acabamento visual pendente.

**Architecture:** O máximo possível fica em `frontend/voltz/` (casca, QSS, um núcleo testável para a árvore de
fontes). Onde o OBS decide sozinho (escolha do tema, bandeja, balão de erro, pergunta ao sair, diálogo de falha,
menu/"Sobre" do clássico) entram patches pequenos em `#ifdef VOLTZ_STUDIO`, todos registrados em
`docs/voltz/upstream-patches.md`. Ícones usados por regras QSS (`image:`/`qproperty-icon:`) não passam pelo
`PhosphorIconEngine`, então viram SVGs pré-coloridos gerados por script (`icons/qss/`).

**Tech Stack:** C++17, Qt 6.11, libobs/frontend 32.2.2, doctest, temas do OBS (`.obt`/`.ovt`), Windows API
(`SetWindowDisplayAffinity`, `ITaskbarList3`). Teste de UI por UI Automation (`tools/voltz-uia.ps1`),
obs-websocket (`tools/voltz-ws.ps1`) e captura **só da janela** (PrintWindow).

**Spec:** `docs/superpowers/specs/2026-10-15-voltz-studio-base-gravacao-design.md` §4.1 (tokens) e §7 ·
**Backlog:** `docs/voltz/backlog-plano-2.md`.

**Fora do 2C (fica no backlog):** `obs_frontend_get_main_window()` devolvendo a casca (risco com docks/diálogos
de plugins); recorte para monitor ultrawide nos presets; hotplug de dispositivos em Ajustes › Captura; rótulo
próprio de áudio (`VolumeControl::setName` privado); logo/ícones de estado da bandeja e da barra de tarefas
(Plano 5); avisos de disco (Plano 3); macOS.

**Descobertas que mudam o escopo (conferidas no código 32.2.2):**
- *Áudio reagir a Configurações → Áudio* **já funciona no upstream**: `VolumeControl` conecta
  `OBSBasic::profileSettingChanged` (`frontend/components/VolumeControl.cpp:136-146`) e as Configurações emitem
  `"MeterDecayRate"`/`"PeakMeterType"` (`frontend/settings/OBSBasicSettings.cpp:3676,3683`); o número de canais é
  relido pelo próprio `VolumeMeter` a cada 16 ms (`needLayoutChange`, `VolumeMeter.cpp:414`). Task 6 só confirma
  e registra — **sem patch**.
- *Esconder da captura:* `OBSApp::notify` (`frontend/OBSApp.cpp:1489-1525`) já aplica
  `OBSBasic::SetDisplayAffinity` em **toda** janela de topo que aparece, inclusive a casca. O que falta é o
  **padrão ligado** (a chave `BasicWindow/HideOBSWindowsFromCapture` não tem default → `false`) e o interruptor
  em Ajustes.
- *Escala de dB sobreposta:* vem da regra `#voltzShell QWidget { font-size: 13px }` aplicada ao `VolumeMeter`
  (o Yami usa 6pt) — basta uma regra de fonte mais específica.
- *Bandeja:* o OBS alterna no **clique simples** (`QSystemTrayIcon::Trigger`, `OBSBasic_SysTray.cpp:94`), não no
  duplo clique; mantemos o mesmo gatilho, agora para a casca.

---

## Convenções

**Diretório de trabalho:** a raiz do repositório (`voltz-studio`), branch `voltz`. Leia o `CLAUDE.md` antes
(seção **Armadilhas**). Base deste plano: commit `0dd83017e`.

```bash
CMAKE="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
CTEST="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe"
CLANGFMT="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/bin/clang-format.exe"
```

- **CONFIGURAR:** `"$CMAKE" --preset voltz-windows-x64`
- **COMPILAR APP:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target obs-studio --parallel -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
- **COMPILAR TUDO:** `"$CMAKE" --build --preset voltz-windows-x64 --parallel -- -v:minimal -nologo -nodeReuse:false > build-compile.log 2>&1; echo "exit=$?"; grep -E ": error|error MSB" build-compile.log | sort -u | head -20`
- **COMPILAR TESTES:** `"$CMAKE" --build build_x64 --config RelWithDebInfo --target voltz-core-tests -- -v:minimal -nologo -nodeReuse:false`
- **RODAR TESTES:** `"$CTEST" --test-dir build_x64/frontend/voltz/tests -C RelWithDebInfo --output-on-failure`
- **SMOKE:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1`
- **SMOKE GRAVAÇÃO:** `powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-record.ps1`
- **FORMATAR:** `"$CLANGFMT" -i <arquivos .cpp/.hpp novos ou alterados em frontend/voltz/>` (nunca nos arquivos do OBS)

> **Regras fixas:**
> - Builds longos **em segundo plano**, nunca dois ao mesmo tempo. `MSB4018`/`ZERO_CHECK`/"Touch" no
>   `build_x86` → rode de novo. Arquivo novo (código, `data/`, ícone no `.qrc`) exige **CONFIGURAR** antes.
> - **Nunca** encerre processos que você não iniciou: só pelo PID que você lançou (`Stop-Process -Id $proc.Id`);
>   nunca `taskkill /IM`, nunca `Stop-Process -Name`. Ler a lista de processos (`Get-Process`) é permitido.
> - **Nunca** capture a tela inteira — só o retângulo de uma janela do app, via `PrintWindow` (bloco abaixo).
>   **Nunca** capture a tela Gravar com câmera ligada (config limpa não tem câmera; se aplicar o preset Tutorial,
>   neutralize a câmera antes — Task 5).
> - App sempre com `--portable --multi` (exceto o teste de falha da Task 4), com **backup e restauração** da
>   config portátil (`build_x64/rundir/RelWithDebInfo/config`).
> - QSS da casca: toda regra com prefixo `#voltzShell`. `QTStr` vem de `<OBSApp.hpp>`. Sinais do libobs chegam
>   fora da thread da UI → `Qt::QueuedConnection`.
> - Toda alteração em C/C++ do OBS fica em `#ifdef VOLTZ_STUDIO` e é registrada em `docs/voltz/upstream-patches.md`
>   **no mesmo commit**. Arquivos do OBS **não** passam pelo clang-format; mantenha tabs e o estilo vizinho.
> - Textos novos nos **dois** locales (`frontend/data/locale/voltz-pt-BR.ini` e `voltz-en-US.ini`, UTF-8 sem BOM),
>   conferindo paridade: `diff <(cut -d= -f1 frontend/data/locale/voltz-en-US.ini | sort) <(cut -d= -f1 frontend/data/locale/voltz-pt-BR.ini | sort)` → sem saída.
> - `docs/voltz/backlog-plano-2.md` só é commitado na Task 8. Push só na Task 8.

**Commits:** terminar a mensagem com a linha `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

**Bloco de teste (PowerShell 5.1)** — copie para um `.ps1` no scratchpad e coloque os passos da task no `try`.
Ele guarda a config portátil, cria uma limpa com obs-websocket ligado (porta 4455, sem senha) e restaura no fim.
`HideOBSWindowsFromCapture=false` no `user.ini` é **obrigatório para capturas** a partir da Task 3: com o padrão
novo (ligado) a janela fica fora da captura e o `PrintWindow` pode sair preto. Os testes que conferem o padrão
ligado **não** escrevem essa linha (Task 3).
```powershell
$root = "C:\Users\Cassio\Documents\Projetos IA\voltz-studio"
$rundir = Join-Path $root "build_x64\rundir\RelWithDebInfo"; $bin = Join-Path $rundir "bin\64bit"
$sp = "<caminho absoluto do scratchpad>"
$cfg = Join-Path $rundir "config"; $bak = Join-Path $rundir "config.2cbak"
if (Test-Path $bak) { Remove-Item -Recurse -Force $bak }
if (Test-Path $cfg) { Move-Item $cfg $bak }
$voltzCfg = Join-Path $cfg "Voltz Studio"
$wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
New-Item -ItemType Directory -Force $wsDir | Out-Null
[IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}')
$userIni = "[BasicWindow]`r`nHideOBSWindowsFromCapture=false`r`n"   # ajuste por task
[IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), $userIni, (New-Object Text.UTF8Encoding $false))
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class W { [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
 [DllImport("user32.dll")] public static extern bool GetWindowDisplayAffinity(IntPtr h, out uint a);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l); }
'@
[void][W]::SetProcessDPIAware()
function Shot([IntPtr]$h, [string]$n) { $r = New-Object W+RECT; [void][W]::GetWindowRect($h, [ref]$r)
  $b = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [System.Drawing.Graphics]::FromImage($b)
  $d = $g.GetHdc(); [void][W]::PrintWindow($h, $d, 2); $g.ReleaseHdc($d); $g.Dispose(); $b.Save("$sp\$n.png"); $b.Dispose() }
$uia = Join-Path $root "tools\voltz-uia.ps1"; $ws = Join-Path $root "tools\voltz-ws.ps1"
$proc = Start-Process (Join-Path $bin "VoltzStudio.exe") -ArgumentList '--portable','--multi' -WorkingDirectory $bin -PassThru
Start-Sleep 15
try {
  $proc.Refresh(); $shell = $proc.MainWindowHandle   # HWND da casca
  # ... passos da task ...
} finally {
  if (-not $proc.HasExited) { $proc.Refresh(); [void]$proc.CloseMainWindow(); if (-not $proc.WaitForExit(20000)) { Stop-Process -Id $proc.Id -Force } }
  $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
  if ($log) { Copy-Item $log.FullName "$sp\ultimo-log.txt" }
  if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
  if (Test-Path $bak) { Move-Item $bak $cfg }
}
```
Para pedidos ao obs-websocket, grave o JSON num arquivo e chame
`& powershell -ExecutionPolicy Bypass -File $ws -RequestsFile "$sp\req.json"` (ver cabeçalho de `tools/voltz-ws.ps1`).

---

## Mapa de arquivos

**Novos**
| Arquivo | Responsabilidade |
|---|---|
| `frontend/data/themes/Voltz.ovt` | variante "Voltz" do tema Yami (paleta §4.1) para as janelas do OBS |
| `frontend/voltz/shell/ExitDialog.hpp/.cpp` | "Parar a gravação e sair?" com o visual da casca |
| `frontend/voltz/core/source-tree.hpp/.cpp` | achata cena+grupos em linhas do painel de Fontes (ordem, irmãos, recuo) |
| `frontend/voltz/tests/test-source-tree.cpp` | testes do achatamento |
| `frontend/voltz/shell/icons/{regular,fill}/{stack,caret-right,speaker-slash,headphones}*.svg` | ícones Phosphor 2.1.1 novos |
| `frontend/voltz/shell/icons/qss/*.svg` | ícones pré-coloridos para regras QSS (gerados) |
| `tools/voltz-qss-icons.sh` | gera `icons/qss/` a partir dos SVGs Phosphor |

**Modificados (Voltz):** `frontend/voltz/shell/VoltzShell.hpp/.cpp`, `WindowChrome.hpp/.cpp`, `SettingsPage.hpp/.cpp`,
`SourcesPanel.hpp/.cpp`, `AudioPanel.cpp`, `Icons.hpp/.cpp`, `ErrorBanner.cpp`, `voltz-shell.qss`, `voltz-shell.qrc`,
`frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`, `frontend/cmake/feature-voltz.cmake`,
`frontend/data/locale/voltz-*.ini`, `tools/voltz-uia.ps1`.

**Modificados (OBS upstream, `#ifdef VOLTZ_STUDIO`):**
| Arquivo | Task | Motivo |
|---|---|---|
| `frontend/OBSApp_Themes.cpp` | 1 | tema Voltz quando o usuário não escolheu tema |
| `frontend/OBSApp.hpp/.cpp` | 2, 3, 4 | bandeja → casca; iniciar na bandeja; esconder da captura por padrão; diálogo de falha |
| `frontend/widgets/OBSBasic_SysTray.cpp` | 2 | clique/menu da bandeja alternam a casca; nome "Voltz Studio" |
| `frontend/widgets/OBSBasic_Recording.cpp` | 2 | sem balão da bandeja para erro de gravação quando a casca está visível |
| `frontend/widgets/OBSBasic.cpp` | 4 | pergunta de saída da casca; esconder "Verificar atualizações"/"Reparar" |
| `frontend/dialogs/OBSAbout.cpp` | 4 | esconder "Apoie"/"Doar"/"Participe" |

---

### Task 1: Tema "Voltz" (`.ovt`) como padrão + UI Automation em todas as janelas

**Files:**
- Create: `frontend/data/themes/Voltz.ovt`
- Modify: `frontend/OBSApp_Themes.cpp:1085` (logo após `QString themeName = config_get_string(...)`)
- Modify: `tools/voltz-uia.ps1` (procura em todas as janelas de topo do processo e imprime o HWND)
- Modify: `docs/voltz/upstream-patches.md`

Formato real (conferido): temas base são `.obt`, variantes `.ovt` com `@OBSThemeMeta { name; id; extends; author;
dark }`, `@OBSThemeVars { --var: valor; }` e QSS comum depois (`frontend/data/themes/Yami_Classic.ovt`).
`OBSApp::FindThemes` (`OBSApp_Themes.cpp:458`) lê `data/obs-studio/themes/*.ovt`; os arquivos de `frontend/data/`
entram no build/instalação pelo `target_install_resources` (glob na configuração — por isso CONFIGURAR).
A escolha do tema: `InitTheme` lê `Appearance/Theme` do `user.ini`; vazio → `DEFAULT_THEME`
(`com.obsproject.Yami.Original`, `cmake/templates/ui-config.h.in:16`) ou `com.obsproject.System` com alto contraste.

- [ ] **Step 1: Criar `frontend/data/themes/Voltz.ovt`**

As variáveis sobrescrevem as do Yami (`Yami.obt`); as que não aparecem aqui são herdadas. `--primary` é um lima
escurecido (o Yami põe texto branco sobre `--primary`; lima puro ficaria ilegível) e o lima puro vai em
`--primary_lighter` (contorno de foco) e nos realces.
```css
@OBSThemeMeta {
    name: 'Voltz';
    id: 'com.voltzpixel.Voltz';
    extends: 'com.obsproject.Yami';
    author: 'VoltzPixel';
    dark: 'true';
}

@OBSThemeVars {
    /* Paleta Voltz (spec 4.1) */
    --voltz_bg: #09090B;
    --voltz_panel: #111114;
    --voltz_line: #1F1F24;
    --voltz_text: #F2F2F3;
    --voltz_muted: #82828C;
    --voltz_accent: #D4F53C;
    --voltz_rec: #FF4D4D;

    /* Cinzas do Yami: 1 = mais claro, 8 = mais escuro */
    --grey1: #4A4A52;
    --grey2: #3A3A42;
    --grey3: #2C2C34;
    --grey4: #232329;
    --grey5: #1F1F24;
    --grey6: #111114;
    --grey7: #09090B;
    --grey8: #050506;

    --bg_window: var(--grey7);
    --bg_base: var(--grey6);
    --bg_preview: var(--grey8);

    --primary: #444C1E;
    --primary_light: #556122;
    --primary_lighter: var(--voltz_accent);
    --primary_dark: #343A1B;
    --primary_darker: #282C19;

    --danger: var(--voltz_rec);

    --text: var(--voltz_text);
    --text_light: #FFFFFF;
    --text_muted: var(--voltz_muted);
    --text_disabled: #5A5A63;
    --text_inactive: var(--voltz_text);

    --border_color: var(--voltz_line);
    --border_radius: 6px;
    --border_radius_small: 4px;
    --border_radius_large: 8px;

    --input_bg: #18181C;
    --input_bg_hover: #1D1D22;
    --input_bg_focus: #111114;
    --input_border: #2C2C34;
    --input_border_hover: #3A3A42;
    --input_border_focus: var(--voltz_accent);

    --list_item_bg_selected: var(--primary);
    --list_item_bg_hover: #1A1A1F;

    --button_bg: #18181C;
    --button_bg_hover: #222228;
    --button_bg_down: #0E0E11;
    --button_bg_disabled: #111114;
    --button_border: var(--voltz_line);
    --button_border_hover: #3A3A42;
    --button_border_focus: var(--voltz_accent);

    --scrollbar_handle: #2C2C34;
    --scrollbar_bg: #111114;
    --scrollbar_hover: #3A3A42;
    --scrollbar_down: #4A4A52;
    --scrollbar_border: var(--voltz_line);

    --palette_link: var(--voltz_accent);
    --palette_linkVisited: var(--voltz_accent);
}

/* Sliders: o Yami usa --blue2 direto */
QSlider::sub-page:horizontal {
    background-color: var(--voltz_accent);
}

QSlider::add-page:vertical {
    background-color: var(--voltz_accent);
}

QSlider::handle {
    background-color: var(--voltz_text);
}

/* Medidores de áudio do mixer clássico na paleta Voltz */
VolumeMeter {
    qproperty-backgroundNominalColor: #2F3712;
    qproperty-backgroundWarningColor: #4A4013;
    qproperty-backgroundErrorColor: #4A1A1A;
    qproperty-foregroundNominalColor: var(--voltz_accent);
    qproperty-foregroundWarningColor: #F5C83C;
    qproperty-foregroundErrorColor: var(--voltz_rec);
    qproperty-majorTickColor: var(--voltz_muted);
    qproperty-minorTickColor: #3A3A42;
}

.btn-monitor.checked {
    background: var(--primary);
}

.btn-monitor.checked:hover {
    background: var(--primary_light);
    border-color: var(--voltz_accent);
}
```

- [ ] **Step 2: Tema Voltz como padrão (patch em `frontend/OBSApp_Themes.cpp`)**

Logo depois da linha 1085 (`QString themeName = config_get_string(userConfig, "Appearance", "Theme");`) e antes
de `if (themeName.isEmpty() || !GetTheme(themeName)) {`, inserir:
```cpp
#ifdef VOLTZ_STUDIO
	/* Voltz Studio: enquanto o usuário não escolher um tema, usa o "Voltz" (data/themes/Voltz.ovt).
	 * Com alto contraste do Windows, segue o upstream (tema do sistema). */
	if (themeName.isEmpty() && GetTheme("com.voltzpixel.Voltz")) {
		bool highContrast = false;
#ifdef _WIN32
		highContrast = HighContrastEnabled();
#endif
		if (!highContrast) {
			themeName = "com.voltzpixel.Voltz";
			blog(LOG_INFO, "[voltz] tema padrao: com.voltzpixel.Voltz");
		}
	}
#endif
```
(`HighContrastEnabled` já é usado neste arquivo; `utility/platform.hpp` está incluído na linha 22.)

- [ ] **Step 3: `tools/voltz-uia.ps1` em todas as janelas do processo**

Diálogos (propriedades, saída, falha) são janelas de topo separadas; a versão atual só olha a primeira.
Substituir o arquivo inteiro por:
```powershell
# Aciona um botao/elemento do Voltz Studio por UI Automation (Windows PowerShell 5.1).
# Procura em TODAS as janelas de topo visiveis do processo (casca, dialogos, modo classico).
# Uso: voltz-uia.ps1 -ProcessId <pid> -Name "Ajustes" [-TimeoutSeconds 10] [-List]
#   -List imprime, para cada janela, "== janela | <titulo> | hwnd=<handle>" e depois os descendentes.
param(
    [Parameter(Mandatory = $true)][int]$ProcessId,
    [string]$Name = "",
    [int]$TimeoutSeconds = 10,
    [switch]$List
)

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

function Fail([string]$why) {
    Write-Output "FALHOU $why"
    exit 1
}

if (-not $List -and [string]::IsNullOrEmpty($Name)) { Fail "informe -Name ou -List" }

$AE = [System.Windows.Automation.AutomationElement]
$Scope = [System.Windows.Automation.TreeScope]
$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$pidCond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $ProcessId)

function Get-ProcessWindows {
    return @($AE::RootElement.FindAll($Scope::Children, $pidCond))
}

$wins = @()
while ($wins.Count -eq 0 -and (Get-Date) -lt $deadline) {
    $wins = Get-ProcessWindows
    if ($wins.Count -eq 0) { Start-Sleep -Milliseconds 300 }
}
if ($wins.Count -eq 0) { Fail "nenhuma janela do processo $ProcessId" }

if ($List) {
    foreach ($win in $wins) {
        Write-Output ("== janela | {0} | hwnd={1}" -f $win.Current.Name, $win.Current.NativeWindowHandle)
        $all = $win.FindAll($Scope::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
        foreach ($e in $all) {
            $c = $e.Current
            Write-Output ("{0} | {1} | {2}" -f $c.ControlType.ProgrammaticName, $c.Name, $c.ClassName)
        }
    }
    exit 0
}

# Descendente com o nome pedido em qualquer janela do processo (aguarda ate o timeout)
$nameCond = New-Object System.Windows.Automation.PropertyCondition($AE::NameProperty, $Name)
$el = $null
while ($null -eq $el -and (Get-Date) -lt $deadline) {
    foreach ($win in Get-ProcessWindows) {
        $el = $win.FindFirst($Scope::Descendants, $nameCond)
        if ($null -ne $el) { break }
    }
    if ($null -eq $el) { Start-Sleep -Milliseconds 300 }
}
if ($null -eq $el) { Fail "elemento '$Name' nao encontrado em $TimeoutSeconds s" }

try {
    $pattern = $null
    if ($el.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern, [ref]$pattern)) {
        $pattern.Invoke()
    }
    elseif ($el.TryGetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern, [ref]$pattern)) {
        $pattern.Select()
    }
    elseif ($el.TryGetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern, [ref]$pattern)) {
        $pattern.Toggle()
    }
    else {
        Fail "'$Name' nao expoe Invoke/SelectionItem/Toggle"
    }
}
catch {
    Fail "erro ao acionar '$Name': $($_.Exception.Message)"
}

Write-Output "OK $Name"
exit 0
```

- [ ] **Step 4: Registrar o patch**

Em `docs/voltz/upstream-patches.md`, acrescentar ao fim da tabela:
```markdown
| `frontend/OBSApp_Themes.cpp` | `InitTheme`: sem tema escolhido pelo usuário (e sem alto contraste do Windows), usa `com.voltzpixel.Voltz` | janelas do OBS com a paleta Voltz |
| `frontend/data/themes/Voltz.ovt` (novo) | variante "Voltz" do Yami (paleta da spec §4.1) | arquivo novo; não altera os temas do OBS |
```

- [ ] **Step 5: CONFIGURAR + COMPILAR APP (segundo plano) e conferir a instalação do tema**

Run: CONFIGURAR; COMPILAR APP; depois `ls build_x64/rundir/RelWithDebInfo/data/obs-studio/themes/Voltz.ovt`.
Expected: build `exit=0`; o arquivo existe no rundir. Se não existir, o glob de `target_install_resources` não
rodou → CONFIGURAR de novo e recompilar.

- [ ] **Step 6: Verificação visual**

Com o **bloco de teste** (user.ini padrão do bloco):
1. Log (`$sp\ultimo-log.txt` ao fim): contém `[voltz] tema padrao: com.voltzpixel.Voltz`; **não** contém
   `com.voltzpixel.Voltz` em linhas de erro (`missing ancestor`, `does not exist`, `is not base`).
2. Casca: `Shot $shell "t1-gravar"`; aciona "Projetos" e "Ajustes" pelo `$uia` e captura `t1-projetos`,
   `t1-ajustes`. Critério: a casca continua com o próprio visual (fundo `#09090B`, painéis `#111114`, navegação
   lima) — nada do tema vazou nos botões/listas/combos; barras de rolagem agora escuras.
3. Janela do OBS: grave `req.json` com
   `[{"requestType":"GetCurrentProgramScene"},{"requestType":"CreateInput","requestData":{"sceneName":"{{0.sceneName}}","inputName":"Cor teste","inputKind":"color_source_v3"}},{"requestType":"OpenInputPropertiesDialog","requestData":{"inputName":"Cor teste"}}]`,
   rode o `$ws`, espere 2 s, `& powershell -File $uia -ProcessId $proc.Id -List | Select-String "== janela"` e
   capture a janela de propriedades pelo `hwnd` impresso (`Shot ([IntPtr]<hwnd>) "t1-propriedades"`). Critério:
   fundo preto/cinza-escuro, seleção e foco lima, botões `#18181C`. Feche-a com `-Name "Cancelar"`.
4. Modo clássico: `-Name "Abrir modo clássico (OBS completo)"`, liste as janelas, capture a clássica pelo `hwnd`
   (`t1-classico`). Critério: paleta Voltz, sliders do mixer lima, medidores lima/amarelo/vermelho.
5. Escolha do usuário respeitada: repita só o passo 1 com
   `$userIni = "[BasicWindow]`r`nHideOBSWindowsFromCapture=false`r`n[Appearance]`r`nTheme=com.obsproject.Yami.Original`r`n"`
   → o log **não** tem `[voltz] tema padrao`.
Descreva as 5 capturas no relatório.

- [ ] **Step 7: Commit**

```bash
git add frontend/data/themes/Voltz.ovt frontend/OBSApp_Themes.cpp tools/voltz-uia.ps1 docs/voltz/upstream-patches.md
git commit -m "feat(voltz): tema Voltz para as janelas do OBS (padrao sem tema escolhido)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Bandeja e minimizar controlam a casca; erro de gravação avisado uma vez

**Files:**
- Modify: `frontend/OBSApp.hpp` (bloco `#ifdef VOLTZ_STUDIO` junto de `HasVoltzShell()`, ~linha 167)
- Modify: `frontend/OBSApp.cpp:1319-1322` (criação da casca) e novo trecho antes de `void OBSApp::uploadLastAppLog() const` (~linha 1432)
- Modify: `frontend/widgets/OBSBasic_SysTray.cpp:32-33, 78-79, 94-97, 101-107`
- Modify: `frontend/widgets/OBSBasic_Recording.cpp:192-194`
- Modify: `frontend/voltz/shell/VoltzShell.hpp`, `VoltzShell.cpp`
- Modify: `docs/voltz/upstream-patches.md`

Como é hoje: `SystemTrayInit` liga `showHide → OBSBasic::ToggleShowHide` e o clique no ícone
(`IconActivated`, `Trigger`) também chama `ToggleShowHide` — ambos alternam a `OBSBasic` oculta. A casca nasce
**depois** do `SystemTrayInit` (`OBSApp::OBSInit` → `mainWindow->OBSInit()` → casca), então a decisão
"casca ou clássica" tem de ser tomada no clique. O balão de erro: `OBSBasic::RecordingStop`
(`OBSBasic_Recording.cpp:183`) chama `SysTrayNotify` quando a clássica está invisível — com a casca, sempre.

- [ ] **Step 1: `OBSApp.hpp` — API da casca para a bandeja**

Trocar o bloco existente
```cpp
#ifdef VOLTZ_STUDIO
	inline bool HasVoltzShell() const { return !voltzShell.isNull(); }
#endif
```
por
```cpp
#ifdef VOLTZ_STUDIO
	inline bool HasVoltzShell() const { return !voltzShell.isNull(); }
	// Casca existe, está visível e não está minimizada.
	bool IsVoltzShellVisible() const;
	// Bandeja: alterna a casca (visível → esconde; oculta/minimizada → mostra e ativa).
	void ToggleVoltzShell();
#endif
```

- [ ] **Step 2: `OBSApp.cpp` — implementação e "iniciar minimizado na bandeja"**

Trocar (linhas 1319-1322)
```cpp
#ifdef VOLTZ_STUDIO
	voltzShell = new voltz::VoltzShell();
	voltzShell->show();
#endif
```
por
```cpp
#ifdef VOLTZ_STUDIO
	voltzShell = new voltz::VoltzShell();
	/* "Iniciar minimizado na bandeja" do OBS vale para a casca (a OBSBasic já nasce oculta). */
	const bool voltzStartInTray = QSystemTrayIcon::isSystemTrayAvailable() &&
				      config_get_bool(userConfig, "BasicWindow", "SysTrayEnabled") &&
				      config_get_bool(userConfig, "BasicWindow", "SysTrayWhenStarted");
	if (!voltzStartInTray) {
		voltzShell->show();
	}
#endif
```
E inserir logo antes de `void OBSApp::uploadLastAppLog() const`:
```cpp
#ifdef VOLTZ_STUDIO
bool OBSApp::IsVoltzShellVisible() const
{
	return !voltzShell.isNull() && voltzShell->isVisible() && !voltzShell->isMinimized();
}

void OBSApp::ToggleVoltzShell()
{
	if (!voltzShell.isNull()) {
		voltzShell->toggleFromTray();
	}
}

#endif
```
(`QSystemTrayIcon` já chega via `widgets/OBSBasic.hpp`; `VoltzShell.hpp` já é incluído no topo em `#ifdef`.)

- [ ] **Step 3: `OBSBasic_SysTray.cpp` — bandeja alterna a casca**

(a) Linha 33, trocar `trayIcon->setToolTip("OBS Studio");` por:
```cpp
#ifdef VOLTZ_STUDIO
	trayIcon->setToolTip("Voltz Studio");
#else
	trayIcon->setToolTip("OBS Studio");
#endif
```
(b) Linha 79, trocar `connect(showHide, &QAction::triggered, this, &OBSBasic::ToggleShowHide);` por:
```cpp
#ifdef VOLTZ_STUDIO
	/* Voltz: com a casca, "Mostrar/Ocultar" alterna a casca (a clássica abre por Ajustes → modo clássico).
	 * A casca nasce depois do SystemTrayInit: a decisão é feita no clique. */
	connect(showHide, &QAction::triggered, this, [this]() {
		if (App()->HasVoltzShell()) {
			App()->ToggleVoltzShell();
		} else {
			ToggleShowHide();
		}
	});
	connect(trayMenu.data(), &QMenu::aboutToShow, this, [this]() {
		if (App()->HasVoltzShell() && showHide) {
			showHide->setText(App()->IsVoltzShellVisible() ? QTStr("Basic.SystemTray.Hide")
								     : QTStr("Basic.SystemTray.Show"));
		}
	});
#else
	connect(showHide, &QAction::triggered, this, &OBSBasic::ToggleShowHide);
#endif
```
(c) Em `IconActivated` (linhas 94-97), trocar
```cpp
	if (reason == QSystemTrayIcon::Trigger) {
		EnablePreviewDisplay(previewEnabled && !isVisible());
		ToggleShowHide();
	}
```
por
```cpp
	if (reason == QSystemTrayIcon::Trigger) {
#ifdef VOLTZ_STUDIO
		if (App()->HasVoltzShell()) {
			App()->ToggleVoltzShell();
			return;
		}
#endif
		EnablePreviewDisplay(previewEnabled && !isVisible());
		ToggleShowHide();
	}
```
(d) Em `SysTrayNotify` (linha 105), trocar `trayIcon->showMessage("OBS Studio", text, icon, 10000);` por:
```cpp
#ifdef VOLTZ_STUDIO
		trayIcon->showMessage("Voltz Studio", text, icon, 10000);
#else
		trayIcon->showMessage("OBS Studio", text, icon, 10000);
#endif
```

- [ ] **Step 4: `OBSBasic_Recording.cpp` — sem balão quando a casca mostra o erro**

Em `RecordingStop`, entre `blog(LOG_INFO, RECORDING_STOP);` (linha 192) e
`if (code == OBS_OUTPUT_UNSUPPORTED && isVisible()) {` (linha 194), inserir:
```cpp
#ifdef VOLTZ_STUDIO
	/* Voltz: com a casca visível, o erro já aparece na faixa vermelha dela (FrontendBridge::recordingFailed);
	 * não repetir no balão da bandeja. Casca oculta/minimizada: segue o fluxo do OBS (balão). */
	if (code != OBS_OUTPUT_SUCCESS && !isVisible() && App()->IsVoltzShellVisible()) {
		blog(LOG_INFO, "[voltz] erro de gravacao (codigo %d) mostrado pela casca; sem balao da bandeja", code);
	} else
#endif
```
(o `else` emenda com o `if` existente da linha seguinte; o resto da cadeia fica intacto e `OnEvent(STOPPED)`,
`AutoRemux` e `OnDeactivate` continuam rodando.)

- [ ] **Step 5: `VoltzShell.hpp` (arquivo inteiro)**

```cpp
#pragma once

#include <QMainWindow>

class QButtonGroup;
class QStackedWidget;

namespace voltz {

class FrontendBridge;

// Janela principal do Voltz Studio. A janela do OBS (OBSBasic) fica oculta como motor
// e pode ser aberta em Ajustes → "Abrir modo clássico".
class VoltzShell : public QMainWindow {
	Q_OBJECT

public:
	explicit VoltzShell(QWidget *parent = nullptr);

	// Bandeja ("Mostrar/Ocultar" e clique no ícone): visível → esconde; oculta ou minimizada → mostra.
	void toggleFromTray();
	void showFromTray();

protected:
	void changeEvent(QEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void showEvent(QShowEvent *event) override;
	bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
	QWidget *buildSideNav();
	void selectPage(int index);
	// Marca o botão da navegação e troca a página (para trocas que não vêm de clique).
	void showPage(int index);
	// Liga/desliga o desenho das prévias (OBSQTDisplay), como o OBS faz ao esconder/minimizar a janela.
	void setPreviewsEnabled(bool enabled);

	FrontendBridge *bridge_ = nullptr;
	QStackedWidget *pages_ = nullptr;
	QButtonGroup *navGroup_ = nullptr;
	bool exiting_ = false;
};

} // namespace voltz
```

- [ ] **Step 6: `VoltzShell.cpp` — tray, minimizar para a bandeja, prévias**

(a) Includes: acrescentar, em ordem alfabética nos blocos existentes,
`#include <widgets/OBSQTDisplay.hpp>` (depois de `<widgets/OBSBasic.hpp>`) e
`#include <QHideEvent>`, `#include <QShowEvent>`, `#include <QSystemTrayIcon>`, `#include <QTimer>`.

(b) No `namespace {` do topo, depois de `loadStyleSheet()`, acrescentar:
```cpp
// "Minimizar para a bandeja" do OBS (Configurações → Geral → Bandeja), que também vale para a casca.
bool minimizeToTrayEnabled()
{
	config_t *config = App()->GetUserConfig();
	return QSystemTrayIcon::isSystemTrayAvailable() && config_get_bool(config, "BasicWindow", "SysTrayEnabled") &&
	       config_get_bool(config, "BasicWindow", "SysTrayMinimizeToTray");
}
```

(c) Substituir `VoltzShell::changeEvent` inteiro por:
```cpp
void VoltzShell::changeEvent(QEvent *event)
{
	/* Trocar tema/paleta do sistema pode devolver a barra de título ao padrão: reaplica. */
	if (event->type() == QEvent::ThemeChange || event->type() == QEvent::PaletteChange) {
		applyDarkTitleBar(this);
	}
	if (event->type() == QEvent::WindowStateChange) {
		if (isMinimized()) {
			setPreviewsEnabled(false);
			if (minimizeToTrayEnabled()) {
				/* Como o OBSBasic::changeEvent: minimizar com "minimizar para a bandeja" esconde a janela
				 * (volta pelo ícone). Adiado: esconder dentro da própria mudança de estado confunde o
				 * gerenciador de janelas. */
				QTimer::singleShot(0, this, &QWidget::hide);
			}
		} else if (isVisible()) {
			setPreviewsEnabled(true);
		}
	}
	QMainWindow::changeEvent(event);
}
```

(d) Acrescentar, depois de `VoltzShell::changeEvent`:
```cpp
void VoltzShell::toggleFromTray()
{
	if (isVisible() && !isMinimized()) {
		hide();
	} else {
		showFromTray();
	}
}

void VoltzShell::showFromTray()
{
	show();
	setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
	raise();
	activateWindow();
}

void VoltzShell::showEvent(QShowEvent *event)
{
	QMainWindow::showEvent(event);
	setPreviewsEnabled(!isMinimized());
}

void VoltzShell::hideEvent(QHideEvent *event)
{
	QMainWindow::hideEvent(event);
	setPreviewsEnabled(false);
}

void VoltzShell::setPreviewsEnabled(bool enabled)
{
	for (OBSQTDisplay *display : findChildren<OBSQTDisplay *>()) {
		if (obs_display_t *handle = display->GetDisplay()) {
			obs_display_set_enabled(handle, enabled);
		}
	}
}
```

- [ ] **Step 7: Registrar os patches**

Em `docs/voltz/upstream-patches.md`: na linha de `frontend/OBSApp.hpp/.cpp`, acrescentar ao fim da coluna
"O que muda": `; IsVoltzShellVisible()/ToggleVoltzShell() (bandeja); OBSInit não mostra a casca com "iniciar minimizado na bandeja"`.
Acrescentar ao fim da tabela:
```markdown
| `frontend/widgets/OBSBasic_SysTray.cpp` | com casca: clique no ícone e "Mostrar/Ocultar" alternam a casca; texto do item segue a casca (`aboutToShow`); dica e título do balão "Voltz Studio" | casca é a janela principal |
| `frontend/widgets/OBSBasic_Recording.cpp` | `RecordingStop`: com a casca visível, erro de gravação não gera balão da bandeja | a casca já mostra a faixa vermelha |
```

- [ ] **Step 8: FORMATAR, COMPILAR APP, SMOKE**

Run: FORMATAR `frontend/voltz/shell/VoltzShell.hpp frontend/voltz/shell/VoltzShell.cpp`; COMPILAR APP; SMOKE.
Expected: `exit=0`; `SMOKE OK`.

- [ ] **Step 9: Verificação (minimizar para a bandeja e iniciar na bandeja)**

Com o bloco de teste:
1. `$userIni` com `SysTrayMinimizeToTray=true` em `[BasicWindow]`. Depois de abrir:
   `[W]::ShowWindow($shell, 6)` (SW_MINIMIZE), `Start-Sleep 2`, `[W]::IsWindowVisible($shell)` → **False**.
   Encerrar: `[W]::PostMessage($shell, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)` (WM_CLOSE) → `WaitForExit(20000)`
   **True** (saída limpa). Se não sair, `Stop-Process -Id $proc.Id -Force` e reporte.
2. Sem `SysTrayMinimizeToTray`: minimizar → `IsWindowVisible` **True** (só minimizada, na barra de tarefas).
3. `$userIni` com `SysTrayWhenStarted=true`: depois de 15 s, `$proc.MainWindowHandle -eq 0` e o log contém
   `[voltz] shell pronta`. Encerrar pelo PID (`Stop-Process -Id $proc.Id`; com `--multi` não há sentinela).
4. O clique no ícone e o menu da bandeja exigem mexer na área de notificação do usuário — **não automatizar**;
   entram em "Testes manuais pendentes do 2C" (Task 8).

- [ ] **Step 10: Commit**

```bash
git add frontend/OBSApp.hpp frontend/OBSApp.cpp frontend/widgets/OBSBasic_SysTray.cpp frontend/widgets/OBSBasic_Recording.cpp frontend/voltz/shell/VoltzShell.hpp frontend/voltz/shell/VoltzShell.cpp docs/voltz/upstream-patches.md
git commit -m "feat(voltz): bandeja e minimizar controlam a casca; erro de gravacao sem balao duplicado

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Esconder o Voltz Studio da gravação (padrão ligado) + estado na barra de tarefas

**Files:**
- Modify: `frontend/OBSApp.cpp:399` (`InitUserConfigDefaults`)
- Modify: `frontend/voltz/shell/WindowChrome.hpp/.cpp`, `VoltzShell.cpp`, `SettingsPage.hpp/.cpp`, `voltz-shell.qss`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `voltz-en-US.ini`
- Modify: `docs/voltz/upstream-patches.md`

Chave real: `BasicWindow/HideOBSWindowsFromCapture` no `user.ini` (lida em `OBSBasic::SetDisplayAffinity`,
`OBSBasic.cpp:2198-2223`, escrita pelas Configurações em `OBSBasicSettings.cpp:3061-3069`). `OBSApp::notify` já
aplica a afinidade em toda janela que aparece; `OBSBasic::SetDisplayAffinity(QWindow *)` é público. Suporte só
no Windows 10 2004+ (`SetDisplayAffinitySupported()`, `utility/platform.hpp`).

- [ ] **Step 1: Padrão ligado (patch em `frontend/OBSApp.cpp`)**

Em `OBSApp::InitUserConfigDefaults`, depois de
`config_set_default_int(userConfig, "Appearance", "Density", 1);` (linha 399), inserir:
```cpp
#ifdef VOLTZ_STUDIO
	/* Voltz Studio: quem grava tutorial da própria tela não quer o app na gravação. */
	config_set_default_bool(userConfig, "BasicWindow", "HideOBSWindowsFromCapture", true);
#endif
```

- [ ] **Step 2: Textos (acrescentar ao fim dos dois locales)**

`voltz-pt-BR.ini`:
```ini
Voltz.Settings.HideFromCapture="Esconder o Voltz Studio da gravação"
Voltz.Settings.HideFromCapture.Hint="As janelas do Voltz Studio não aparecem quando você grava a tela. Desligue se quiser gravar o próprio app."
Voltz.Settings.HideFromCapture.Unsupported="Requer o Windows 10 versão 2004 ou mais recente."
```
`voltz-en-US.ini`:
```ini
Voltz.Settings.HideFromCapture="Hide Voltz Studio from recordings"
Voltz.Settings.HideFromCapture.Hint="Voltz Studio windows don't show up when you record your screen. Turn this off to record the app itself."
Voltz.Settings.HideFromCapture.Unsupported="Requires Windows 10 version 2004 or later."
```
Conferir a paridade (comando nas Convenções).

- [ ] **Step 3: `WindowChrome` — estado na barra de tarefas e log da afinidade**

Em `WindowChrome.hpp`, depois de `forwardDisplayEvents`, acrescentar:
```cpp
enum class TaskbarState { Idle, Recording, Paused };

// Ícone de estado sobre o botão da janela na barra de tarefas (mesmos ícones do OBS: active/paused).
// No-op fora do Windows.
void setTaskbarState(QWidget *window, TaskbarState state);

// Loga a afinidade de exibição da janela (0x11 = WDA_EXCLUDEFROMCAPTURE: fora da captura de tela).
void logCaptureAffinity(QWidget *window);
```
Em `WindowChrome.cpp`: includes `#include <util/base.h>`, `#include <QIcon>`, `#include <QImage>`,
`#include <QPixmap>` e, no bloco `#ifdef _WIN32`, `#include <ShObjIdl.h>` depois de `<dwmapi.h>`. Acrescentar
antes do `} // namespace voltz`:
```cpp
void setTaskbarState(QWidget *window, TaskbarState state)
{
#ifdef _WIN32
	/* Mesmo caminho do TaskbarOverlaySetStatus do OBS (platform-windows.cpp), mas no HWND da casca:
	 * o do OBS aponta para a OBSBasic oculta, que não tem botão na barra de tarefas. */
	ITaskbarList3 *taskbar = nullptr;
	if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar)))) {
		return;
	}
	if (FAILED(taskbar->HrInit())) {
		taskbar->Release();
		return;
	}
	HWND hwnd = reinterpret_cast<HWND>(window->winId());
	if (state == TaskbarState::Idle) {
		taskbar->SetOverlayIcon(hwnd, nullptr, nullptr);
	} else {
		const QIcon icon(state == TaskbarState::Recording ? QStringLiteral(":/res/images/active.png")
								 : QStringLiteral(":/res/images/paused.png"));
		const int size = GetSystemMetrics(SM_CXSMICON);
		HICON hicon = icon.pixmap(size, size).toImage().toHICON();
		if (hicon) {
			taskbar->SetOverlayIcon(hwnd, hicon, nullptr);
			DestroyIcon(hicon);
		}
	}
	taskbar->Release();
#else
	(void)window;
	(void)state;
#endif
}

void logCaptureAffinity(QWidget *window)
{
#ifdef _WIN32
	DWORD affinity = 0;
	if (GetWindowDisplayAffinity(reinterpret_cast<HWND>(window->winId()), &affinity)) {
		blog(LOG_INFO, "[voltz] afinidade de captura da casca: 0x%lx (%s)", static_cast<unsigned long>(affinity),
		     affinity == WDA_EXCLUDEFROMCAPTURE ? "fora da captura" : "visivel na captura");
	}
#else
	(void)window;
#endif
}
```

- [ ] **Step 4: `VoltzShell.cpp` — ligar estado da barra e log**

No construtor, logo depois do `connect(bridge_, &FrontendBridge::exiting, ...)`:
```cpp
	connect(bridge_, &FrontendBridge::recordingStateChanged, this, [this](bool active, bool paused) {
		setTaskbarState(this, !active ? TaskbarState::Idle
				      : paused ? TaskbarState::Paused
					       : TaskbarState::Recording);
	});
```
Em `VoltzShell::showEvent`, depois de `setPreviewsEnabled(!isMinimized());`:
```cpp
	/* OBSApp::notify já aplicou a afinidade antes de entregar este evento: registra o resultado. */
	logCaptureAffinity(this);
```

- [ ] **Step 5: Ajustes › Captura — interruptor**

`SettingsPage.hpp`: acrescentar `class QCheckBox;` às declarações antecipadas e o membro
`QCheckBox *hideFromCapture_ = nullptr;` depois de `captureHint_`.

`SettingsPage.cpp`: includes `#include <utility/platform.hpp>` (depois de `<widgets/OBSBasic.hpp>`),
`#include <QCheckBox>`, `#include <QGuiApplication>`, `#include <QSignalBlocker>`, `#include <QWindow>`.
Em `buildCaptureSection`, depois de `body->addWidget(captureHint_);`:
```cpp
	hideFromCapture_ = new QCheckBox(QTStr("Voltz.Settings.HideFromCapture"), section);
	hideFromCapture_->setEnabled(SetDisplayAffinitySupported());
	auto *hideHint = new QLabel(SetDisplayAffinitySupported() ? QTStr("Voltz.Settings.HideFromCapture.Hint")
								  : QTStr("Voltz.Settings.HideFromCapture.Unsupported"),
				    section);
	hideHint->setObjectName("muted");
	hideHint->setWordWrap(true);
	body->addSpacing(4);
	body->addWidget(hideFromCapture_);
	body->addWidget(hideHint);

	connect(hideFromCapture_, &QCheckBox::toggled, this, [](bool hide) {
		/* Mesma chave e mesmo efeito de Configurações → Geral do OBS: grava e reaplica em todas as janelas. */
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "HideOBSWindowsFromCapture", hide);
		config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
		for (QWindow *window : QGuiApplication::allWindows()) {
			if (window->isVisible()) {
				OBSBasic::Get()->SetDisplayAffinity(window);
			}
		}
		blog(LOG_INFO, "[voltz] esconder da captura: %s", hide ? "ligado" : "desligado");
	});
```
Em `SettingsPage::showEvent`, depois de `microphone_->reload();`:
```cpp
	{
		/* Pode ter mudado pelas Configurações do OBS: relê sem disparar o toggled. */
		const QSignalBlocker blocker(hideFromCapture_);
		hideFromCapture_->setChecked(
			config_get_bool(App()->GetUserConfig(), "BasicWindow", "HideOBSWindowsFromCapture"));
	}
```
`voltz-shell.qss`, acrescentar ao fim:
```css
#voltzShell QCheckBox {
	background: transparent;
	spacing: 8px;
}
```

- [ ] **Step 6: Registrar o patch**

Na linha de `frontend/OBSApp.hpp/.cpp` de `upstream-patches.md`, acrescentar ao fim de "O que muda":
`; InitUserConfigDefaults: HideOBSWindowsFromCapture padrão true`.

- [ ] **Step 7: FORMATAR, COMPILAR APP, SMOKE**

FORMATAR `WindowChrome.hpp/.cpp`, `VoltzShell.cpp`, `SettingsPage.hpp/.cpp`. COMPILAR APP; SMOKE → `SMOKE OK`.

- [ ] **Step 8: Verificação da afinidade (sem capturar a tela)**

Bloco de teste com `$userIni = ""` (sem a linha `HideOBSWindowsFromCapture` → vale o padrão novo):
1. `$a = 0; [void][W]::GetWindowDisplayAffinity($shell, [ref]$a); "{0:x}" -f $a` → `11`.
   Log: `Hide OBS windows from screen capture: true` e `[voltz] afinidade de captura da casca: 0x11`.
   *Se `GetWindowDisplayAffinity` falhar entre processos (retorna False), o critério passa a ser só o log.*
2. `-Name "Ajustes"`, depois `-Name "Esconder o Voltz Studio da gravação"` (Toggle) → afinidade `0`, log
   `[voltz] esconder da captura: desligado`, e `config\Voltz Studio\user.ini` contém
   `HideOBSWindowsFromCapture=false`. Acionar de novo → `11`.
3. Bloco de teste normal (`HideOBSWindowsFromCapture=false`): afinidade `0`; `Shot $shell "t3-ajustes"` na página
   Ajustes — interruptor desmarcado com a dica abaixo, alinhado à seção Captura.
4. Estado da barra de tarefas: não capturar a barra (é tela do usuário). Conferir por código e registrar como
   teste manual na Task 8 (gravar → bolinha de "ativo" no botão da casca; pausar → ícone de pausa).

- [ ] **Step 9: Commit**

```bash
git add frontend/OBSApp.cpp frontend/voltz/shell/WindowChrome.hpp frontend/voltz/shell/WindowChrome.cpp frontend/voltz/shell/VoltzShell.cpp frontend/voltz/shell/SettingsPage.hpp frontend/voltz/shell/SettingsPage.cpp frontend/voltz/shell/voltz-shell.qss frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini docs/voltz/upstream-patches.md
git commit -m "feat(voltz): esconder o Voltz Studio da gravacao por padrao (Ajustes > Captura) e estado na barra de tarefas

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Saída e falha — confirmação da casca, diálogo de falha, menu e "Sobre" do clássico

**Files:**
- Create: `frontend/voltz/shell/ExitDialog.hpp`, `ExitDialog.cpp`
- Modify: `frontend/voltz/shell/WindowChrome.hpp/.cpp` (`shellStyleSheet()`), `VoltzShell.cpp`, `voltz-shell.qss`
- Modify: `frontend/cmake/feature-voltz.cmake`
- Modify: `frontend/widgets/OBSBasic.cpp` (include ~linha 48; menu ~linha 1370; `promptToClose` ~linha 1974)
- Modify: `frontend/OBSApp.cpp` (`handleUncleanShutdown`, linhas 95-149)
- Modify: `frontend/dialogs/OBSAbout.cpp:37-40`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `voltz-en-US.ini`, `docs/voltz/upstream-patches.md`

Fluxo real de saída: X da casca → `VoltzShell::closeEvent` → `OBSBasic::close()` (fila) → `closeEvent` →
`shouldPromptForClose()` (`ConfirmOnExit` **e** alguma saída ativa — gravação, transmissão, câmera virtual,
replay) → `promptToClose()`. O "Sair" da bandeja e do menu clássico passam pelo mesmo caminho. Por isso a pergunta
própria entra **dentro** de `promptToClose` (um patch), e "respeitar a opção de confirmar ao sair" vem de graça.

- [ ] **Step 1: Textos (acrescentar ao fim dos dois locales)**

`voltz-pt-BR.ini`:
```ini
Voltz.Exit.WindowTitle="Sair do Voltz Studio"
Voltz.Exit.Recording.Title="Parar a gravação e sair?"
Voltz.Exit.Recording.Text="A gravação em andamento será finalizada e salva antes de fechar."
Voltz.Exit.Active.Title="Encerrar e sair?"
Voltz.Exit.Active.Text="Há uma saída ativa (câmera virtual, transmissão ou replay). Ela será encerrada ao fechar."
Voltz.Exit.StopAndQuit="Parar e sair"
Voltz.Exit.Quit="Sair"
Voltz.Exit.Cancel="Cancelar"
Voltz.Crash.Title="O Voltz Studio fechou inesperadamente"
Voltz.Crash.Text="O Voltz Studio não foi fechado corretamente da última vez.\n\nAbrir no modo de segurança (plugins de terceiros, scripts e WebSockets desativados)?"
Voltz.Crash.OpenLogs="Abrir pasta de logs"
```
`voltz-en-US.ini`:
```ini
Voltz.Exit.WindowTitle="Quit Voltz Studio"
Voltz.Exit.Recording.Title="Stop recording and quit?"
Voltz.Exit.Recording.Text="The current recording will be finished and saved before closing."
Voltz.Exit.Active.Title="Stop and quit?"
Voltz.Exit.Active.Text="An output is active (virtual camera, stream or replay buffer). It will be stopped when closing."
Voltz.Exit.StopAndQuit="Stop and quit"
Voltz.Exit.Quit="Quit"
Voltz.Exit.Cancel="Cancel"
Voltz.Crash.Title="Voltz Studio closed unexpectedly"
Voltz.Crash.Text="Voltz Studio did not shut down properly last time.\n\nOpen in Safe Mode (third-party plugins, scripting and WebSockets disabled)?"
Voltz.Crash.OpenLogs="Open logs folder"
```
(o `\n` vira quebra de linha no `text_lookup`, como em `CrashHandling.Labels.Text` do OBS.) Conferir paridade.

- [ ] **Step 2: `shellStyleSheet()` em `WindowChrome`**

`WindowChrome.hpp`: acrescentar `#include <QString>` depois de `#pragma once` e, antes do `} // namespace voltz`:
```cpp
// QSS da casca (":/voltz/voltz-shell.qss"). Diálogos próprios (ex. confirmação de saída) usam o mesmo estilo.
QString shellStyleSheet();
```
`WindowChrome.cpp`: `#include <QFile>` e, antes do `} // namespace voltz`:
```cpp
QString shellStyleSheet()
{
	QFile file(QStringLiteral(":/voltz/voltz-shell.qss"));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return QString();
	}
	return QString::fromUtf8(file.readAll());
}
```
`VoltzShell.cpp`: apagar a função `loadStyleSheet()` do `namespace {` e o `#include <QFile>`; no construtor trocar
`setStyleSheet(loadStyleSheet());` por `setStyleSheet(shellStyleSheet());`.

- [ ] **Step 3: `ExitDialog.hpp`**

```cpp
#pragma once

class QWidget;

namespace voltz {

// Pergunta de saída com o visual da casca. `recording`: gravação ativa ("Parar a gravação e sair?");
// senão fala das outras saídas ativas. Devolve true para sair. Enter = Cancelar.
bool confirmExitWhileActive(QWidget *parent, bool recording);

} // namespace voltz
```

- [ ] **Step 4: `ExitDialog.cpp`**

```cpp
#include "ExitDialog.hpp"

#include "Icons.hpp"
#include "WindowChrome.hpp"

#include <OBSApp.hpp> // QTStr

#include <obs.h>

#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace voltz {

namespace {

// Pai do diálogo: a janela pedida, se visível; senão a casca visível (a OBSBasic fica oculta e um diálogo
// centrado nela apareceria num lugar qualquer).
QWidget *resolveParent(QWidget *requested)
{
	if (requested && requested->isVisible()) {
		return requested->window();
	}
	for (QWidget *widget : QApplication::topLevelWidgets()) {
		if (widget->objectName() == QLatin1String("voltzShell") && widget->isVisible()) {
			return widget;
		}
	}
	return requested;
}

} // namespace

bool confirmExitWhileActive(QWidget *parent, bool recording)
{
	QDialog dialog(resolveParent(parent));
	/* objectName + QSS próprios: as regras "#voltzShell ..." valem mesmo sem a casca como ancestral. */
	dialog.setObjectName("voltzShell");
	dialog.setStyleSheet(shellStyleSheet());
	dialog.setWindowTitle(QTStr("Voltz.Exit.WindowTitle"));
	dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
	dialog.setMinimumWidth(440);

	auto *icon = new QLabel(&dialog);
	icon->setPixmap(phosphorIcon(QStringLiteral("warning-circle"), palette::kRec, IconWeight::Fill).pixmap(28, 28));

	auto *title = new QLabel(QTStr(recording ? "Voltz.Exit.Recording.Title" : "Voltz.Exit.Active.Title"), &dialog);
	title->setObjectName("dialogTitle");
	auto *text = new QLabel(QTStr(recording ? "Voltz.Exit.Recording.Text" : "Voltz.Exit.Active.Text"), &dialog);
	text->setObjectName("muted");
	text->setWordWrap(true);

	auto *texts = new QVBoxLayout();
	texts->setSpacing(6);
	texts->addWidget(title);
	texts->addWidget(text);

	auto *top = new QHBoxLayout();
	top->setSpacing(14);
	top->addWidget(icon, 0, Qt::AlignTop);
	top->addLayout(texts, 1);

	auto *cancel = new QPushButton(QTStr("Voltz.Exit.Cancel"), &dialog);
	auto *quit = new QPushButton(QTStr(recording ? "Voltz.Exit.StopAndQuit" : "Voltz.Exit.Quit"), &dialog);
	quit->setObjectName("dangerButton");
	/* Enter cancela: sair sem querer encerraria a gravação. */
	quit->setAutoDefault(false);
	cancel->setDefault(true);
	QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
	QObject::connect(quit, &QPushButton::clicked, &dialog, &QDialog::accept);

	auto *buttons = new QHBoxLayout();
	buttons->setSpacing(8);
	buttons->addStretch(1);
	buttons->addWidget(cancel);
	buttons->addWidget(quit);

	auto *layout = new QVBoxLayout(&dialog);
	layout->setContentsMargins(22, 20, 22, 18);
	layout->setSpacing(18);
	layout->addLayout(top);
	layout->addLayout(buttons);

	applyDarkTitleBar(&dialog);
	const bool accepted = dialog.exec() == QDialog::Accepted;
	blog(LOG_INFO, "[voltz] confirmacao de saida: %s", accepted ? "sair" : "cancelar");
	return accepted;
}

} // namespace voltz
```
`voltz-shell.qss`, acrescentar ao fim:
```css
#voltzShell QLabel#dialogTitle {
	font-size: 15px;
	font-weight: 600;
}

#voltzShell QPushButton#dangerButton {
	background: #FF4D4D;
	color: #FFFFFF;
	border: none;
	font-weight: 600;
}

#voltzShell QPushButton#dangerButton:hover {
	background: #FF6666;
}
```
`feature-voltz.cmake`: em `target_sources`, depois de `voltz/shell/ErrorBanner.hpp`, acrescentar
`voltz/shell/ExitDialog.cpp` e `voltz/shell/ExitDialog.hpp`.

- [ ] **Step 5: `OBSBasic.cpp` — pergunta da casca e menu sem atualizador**

(a) Depois de `#include <widgets/OBSProjector.hpp>` (linha 48):
```cpp
#ifdef VOLTZ_STUDIO
#include <voltz/shell/ExitDialog.hpp>
#endif
```
(b) Logo depois do bloco
```cpp
#if defined(_WIN32) || defined(__APPLE__)
	if (App()->IsUpdaterDisabled()) {
		...
	}
#endif
```
(linhas 1363-1370, imediatamente antes de `#ifndef WHATSNEW_ENABLED`), inserir:
```cpp
#ifdef VOLTZ_STUDIO
	/* Voltz: sem atualizador do OBS — esconde os itens em vez de mostrá-los desativados. */
	if (ui->actionCheckForUpdates) {
		ui->actionCheckForUpdates->setVisible(false);
	}
	if (ui->actionRepair) {
		ui->actionRepair->setVisible(false);
	}
#endif
```
(`OBSBasic_MainControls.cpp`/`OBSBasic_Updater.cpp` só chamam `setEnabled` nesses itens; a visibilidade fica.)

(c) Em `promptToClose`, substituir o ramo `#ifdef VOLTZ_STUDIO` atual (do `#ifdef` até o `#else`, exclusive):
```cpp
#ifdef VOLTZ_STUDIO
	/* Voltz: com a casca, não reabrir o modo clássico só para perguntar; a pergunta sai sobre a janela ativa */
	QWidget *promptParent = this;
	if (App()->HasVoltzShell()) {
		if (QWidget *active = App()->activeWindow()) {
			promptParent = active;
		}
	} else {
		SetShowing(true);
	}
	QMessageBox::StandardButton button =
		OBSMessageBox::question(promptParent, QTStr("ConfirmExit.Title"), QTStr("ConfirmExit.Text"),
					QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel));
#else
```
por:
```cpp
#ifdef VOLTZ_STUDIO
	/* Voltz: com a casca, a pergunta é o diálogo dela (sem reabrir o modo clássico) */
	QMessageBox::StandardButton button = QMessageBox::Cancel;
	if (App()->HasVoltzShell()) {
		QWidget *promptParent = App()->activeWindow() ? App()->activeWindow() : this;
		button = voltz::confirmExitWhileActive(promptParent, RecordingActive()) ? QMessageBox::Ok
											: QMessageBox::Cancel;
	} else {
		SetShowing(true);
		button = OBSMessageBox::question(this, QTStr("ConfirmExit.Title"), QTStr("ConfirmExit.Text"),
						 QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel));
	}
#else
```

- [ ] **Step 6: `OBSApp.cpp` — diálogo de encerramento inesperado**

Em `handleUncleanShutdown` (linhas 95-149):
(a) depois de `blog(LOG_WARNING, "Crash or unclean shutdown detected");`:
```cpp
#ifdef VOLTZ_STUDIO
	/* Voltz Studio: não envia relatórios de falha — sem aviso de privacidade do OBS nem "Enviar relatório". */
	enableCrashUpload = false;
#endif
```
(b) trocar as duas linhas
```cpp
	crashWarning.setWindowTitle(QTStr("CrashHandling.Dialog.Title"));
	crashWarning.setText(QTStr("CrashHandling.Labels.Text"));
```
por
```cpp
#ifdef VOLTZ_STUDIO
	crashWarning.setWindowTitle(QTStr("Voltz.Crash.Title"));
	crashWarning.setText(QTStr("Voltz.Crash.Text"));
#else
	crashWarning.setWindowTitle(QTStr("CrashHandling.Dialog.Title"));
	crashWarning.setText(QTStr("CrashHandling.Labels.Text"));
#endif
```
(c) logo antes de `crashWarning.setDefaultButton(launchNormalButton);`:
```cpp
#ifdef VOLTZ_STUDIO
	QPushButton *openLogsButton = crashWarning.addButton(QTStr("Voltz.Crash.OpenLogs"), QMessageBox::ActionRole);
	QObject::connect(openLogsButton, &QPushButton::clicked, []() {
		char logsPath[512];
		if (GetAppConfigPath(logsPath, sizeof(logsPath), VOLTZ_CONFIG_DIR "/logs") > 0) {
			QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromUtf8(logsPath)));
		}
	});
#endif
```
(d) trocar a linha `crashWarning.exec();` por:
```cpp
	crashWarning.exec();
#ifdef VOLTZ_STUDIO
	/* "Abrir pasta de logs" não escolhe o modo de abertura: se ele fechou a caixa, pergunta de novo. */
	while (crashWarning.clickedButton() == openLogsButton) {
		crashWarning.exec();
	}
#endif
```
(`QDesktopServices`/`QUrl` já estão incluídos em `OBSApp.cpp`, linhas 44 e 1428.) Com `enableCrashUpload = false`
o código seguinte nunca lê `crashWarning.checkBox()` (linha 136 usa o ternário).

- [ ] **Step 7: `OBSAbout.cpp` — sem links de doação/participação**

Trocar o bloco das linhas 37-40
```cpp
#ifdef VOLTZ_STUDIO
	ui->name->setText(QStringLiteral("Voltz Studio"));
	ui->version->setText(QStringLiteral(VOLTZ_VERSION) + bitness);
#endif
```
por
```cpp
#ifdef VOLTZ_STUDIO
	ui->name->setText(QStringLiteral("Voltz Studio"));
	ui->version->setText(QStringLiteral(VOLTZ_VERSION) + bitness);
	/* Sem "Apoie o OBS Project"/"Fazer uma contribuição"/"Participar" (links do obsproject.com) */
	ui->contribute->hide();
	ui->donate->hide();
	ui->getInvolved->hide();
#endif
```
(fica antes do `delete ui->donate` do ramo `steam`, então o ponteiro ainda é válido.)

- [ ] **Step 8: Registrar os patches**

Em `upstream-patches.md`:
- linha `frontend/widgets/OBSBasic.cpp`, acrescentar ao fim de "O que muda": `; promptToClose com casca usa voltz::confirmExitWhileActive (diálogo da casca); menu Ajuda sem "Verificar atualizações"/"Reparar" (ocultos)`;
- linha `frontend/OBSApp.hpp/.cpp`: `; handleUncleanShutdown: texto Voltz, sem aviso de privacidade/envio de relatório, botão "Abrir pasta de logs"`;
- linha `frontend/dialogs/OBSAbout.cpp`: `; esconde "Apoie"/"Doar"/"Participe"`.

- [ ] **Step 9: CONFIGURAR (arquivo novo), FORMATAR, COMPILAR APP, SMOKE, SMOKE GRAVAÇÃO**

FORMATAR `ExitDialog.hpp/.cpp`, `WindowChrome.hpp/.cpp`, `VoltzShell.cpp`. Expected: `SMOKE OK` nos dois smokes
(o de gravação usa `ConfirmOnExit=false`: fecha gravando **sem** diálogo).

- [ ] **Step 10: Verificação da confirmação de saída**

Bloco de teste. Antes de abrir, `New-Item -ItemType Directory -Force "$sp\rec-2c" | Out-Null`. Passos:
1. `req.json` = `[{"requestType":"SetRecordDirectory","requestData":{"recordDirectory":"<$sp>\\rec-2c"}},{"requestType":"StartRecord"}]`
   (barras invertidas escapadas no JSON). `$ws`; `Start-Sleep 4`.
2. `$proc.Refresh(); [void]$proc.CloseMainWindow(); Start-Sleep 2`; `-List | Select-String "== janela"` →
   exatamente **duas** janelas: a casca e `Sair do Voltz Studio` (a clássica **não** aparece). A lista do diálogo
   tem `Parar a gravação e sair?`, `Parar e sair`, `Cancelar`. `Shot ([IntPtr]<hwnd do diálogo>) "t4-sair"`:
   fundo preto, ícone vermelho, botão vermelho "Parar e sair".
3. `-Name "Cancelar"` → app continua; `GetRecordStatus` → `outputActive: true`; log `[voltz] confirmacao de saida: cancelar`.
4. De novo `CloseMainWindow`, `Start-Sleep 2`, `-Name "Parar e sair"` → `$proc.WaitForExit(30000)` **True**;
   em `$sp\rec-2c` existe 1 arquivo de vídeo com tamanho > 0. Apague a pasta `$sp\rec-2c` (criada pelo teste).
5. Sem saída ativa, `CloseMainWindow` fecha sem diálogo (o SMOKE já prova).

- [ ] **Step 11: Verificação do diálogo de falha (sem `--multi`)**

Só rode se `Get-Process VoltzStudio -ErrorAction SilentlyContinue` não devolver nada (nunca encerre instância
alheia; se houver, pule e registre como teste manual). Com o backup/restauração do bloco de teste, mas abrindo
**sem** `--multi`:
1. `$p1 = Start-Process ... -ArgumentList '--portable'`; `Start-Sleep 12`;
   `Test-Path "$voltzCfg\.sentinel\run_*"` → True; `Stop-Process -Id $p1.Id -Force` (processo seu).
2. `$p2 = Start-Process ... -ArgumentList '--portable'`; `Start-Sleep 8`; `-ProcessId $p2.Id -List` → janela
   `O Voltz Studio fechou inesperadamente` com o texto Voltz, botões `Executar no modo de segurança`,
   `Executar no modo normal`, `Abrir pasta de logs`; **sem** "Política de Privacidade" e **sem** caixa de seleção.
   Capture pelo `hwnd` (`t4-falha`). **Não** acione "Abrir pasta de logs" (abriria o Explorer).
3. `-Name "Executar no modo normal"`; `Start-Sleep 12`; feche a casca (`CloseMainWindow`) → saída limpa e
   `.sentinel` sem `run_*`.

- [ ] **Step 12: Verificação do menu e do "Sobre" (melhor esforço)**

Bloco de teste: `-Name "Abrir modo clássico (OBS completo)"`, depois tente `-Name "Ajuda (&H)"` ou `-Name "Ajuda"`
(nome do item do menu na UIA) e `-List`: o menu aberto **não** lista "Verificar se há atualizações" nem
"Verificar integridade dos arquivos". Se o menu abrir, acione `Sobre (&A)`/`Sobre` e liste o diálogo: sem
"Apoie o OBS Project", "Fazer uma contribuição", "Participar". Se a UIA não abrir menus do Qt, registre os dois
itens como teste manual (Task 8) — a verificação por código basta para o commit.

- [ ] **Step 13: Commit**

```bash
git add frontend/voltz/shell/ExitDialog.hpp frontend/voltz/shell/ExitDialog.cpp frontend/voltz/shell/WindowChrome.hpp frontend/voltz/shell/WindowChrome.cpp frontend/voltz/shell/VoltzShell.cpp frontend/voltz/shell/voltz-shell.qss frontend/cmake/feature-voltz.cmake frontend/widgets/OBSBasic.cpp frontend/OBSApp.cpp frontend/dialogs/OBSAbout.cpp frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini docs/voltz/upstream-patches.md
git commit -m "feat(voltz): confirmacao de saida da casca, dialogo de falha sem envio, menu e Sobre do classico sem links do OBS

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Fontes — grupos com filhos e acessibilidade dos botões só-ícone

**Files:**
- Create: `frontend/voltz/core/source-tree.hpp`, `source-tree.cpp`, `frontend/voltz/tests/test-source-tree.cpp`
- Create: `frontend/voltz/shell/icons/regular/stack.svg`, `caret-right.svg`, `icons/fill/stack-fill.svg`, `caret-right-fill.svg`
- Modify: `frontend/voltz/core/CMakeLists.txt`, `frontend/voltz/tests/CMakeLists.txt`, `frontend/voltz/shell/voltz-shell.qrc`
- Modify: `frontend/voltz/shell/SourcesPanel.hpp`, `SourcesPanel.cpp` (reescritos), `Icons.hpp/.cpp`, `ErrorBanner.cpp`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `voltz-en-US.ini`

Fatos do libobs (conferidos em `libobs/obs-scene.c`): `obs_scene_enum_items` só percorre o nível de topo; filhos
via `obs_sceneitem_group_enum_items` (`:3986`); `obs_scene_find_sceneitem_by_id` não desce em grupos (`:2085`) →
o filho se acha no `obs_sceneitem_group_get_scene(grupo)`; sinais dos filhos (`item_visible`, `reorder`...) saem
pelo `signal_handler` **da fonte do grupo** (`signal_parent`, `:2469`), não da cena. "Recolhido" é a configuração
privada `collapsed` do item do grupo — a mesma que o painel do OBS usa (`SourceTreeItem.cpp:551,566`).

- [ ] **Step 1: Teste (falha) — `frontend/voltz/tests/test-source-tree.cpp`**

```cpp
#include <doctest.h>

#include <voltz/core/source-tree.hpp>

#include <utility>
#include <vector>

using voltz::SourceNode;
using voltz::SourceRow;

namespace {

SourceNode item(int64_t id)
{
	SourceNode node;
	node.id = id;
	return node;
}

SourceNode group(int64_t id, bool collapsed, std::vector<SourceNode> children)
{
	SourceNode node;
	node.id = id;
	node.isGroup = true;
	node.collapsed = collapsed;
	node.children = std::move(children);
	return node;
}

} // namespace

TEST_CASE("flattenSourceTree: cena vazia não tem linhas")
{
	CHECK(voltz::flattenSourceTree({}).empty());
}

TEST_CASE("flattenSourceTree: camada de cima primeiro (o libobs enumera de baixo para cima)")
{
	const std::vector<SourceRow> rows = voltz::flattenSourceTree({item(1), item(2), item(3)});
	REQUIRE(rows.size() == 3);
	CHECK(rows[0].id == 3);
	CHECK(rows[1].id == 2);
	CHECK(rows[2].id == 1);
	for (const SourceRow &row : rows) {
		CHECK(row.groupId == -1);
		CHECK(row.depth == 0);
		CHECK_FALSE(row.isGroup);
		CHECK(row.siblingCount == 3);
	}
	CHECK(rows[0].siblingIndex == 0);
	CHECK(rows[2].siblingIndex == 2);
}

TEST_CASE("flattenSourceTree: filhos do grupo expandido logo abaixo dele, também de cima para baixo")
{
	/* cena (baixo → cima): 1, grupo 2 {10, 11}, 3 */
	const auto rows = voltz::flattenSourceTree({item(1), group(2, false, {item(10), item(11)}), item(3)});
	REQUIRE(rows.size() == 5);
	CHECK(rows[0].id == 3);
	CHECK(rows[1].id == 2);
	CHECK(rows[1].isGroup);
	CHECK_FALSE(rows[1].collapsed);
	CHECK(rows[2].id == 11);
	CHECK(rows[2].groupId == 2);
	CHECK(rows[2].depth == 1);
	CHECK(rows[2].siblingIndex == 0);
	CHECK(rows[2].siblingCount == 2);
	CHECK(rows[3].id == 10);
	CHECK(rows[3].siblingIndex == 1);
	CHECK(rows[4].id == 1);
	CHECK(rows[4].groupId == -1);
	CHECK(rows[4].siblingIndex == 2);
}

TEST_CASE("flattenSourceTree: grupo recolhido esconde os filhos")
{
	const auto rows = voltz::flattenSourceTree({group(5, true, {item(10), item(11)})});
	REQUIRE(rows.size() == 1);
	CHECK(rows[0].id == 5);
	CHECK(rows[0].collapsed);
}

TEST_CASE("canMoveUp/canMoveDown consideram só os irmãos")
{
	/* linhas: grupo 2 (topo da cena), 11 (topo do grupo), 10 (base do grupo), 1 (base da cena) */
	const auto rows = voltz::flattenSourceTree({item(1), group(2, false, {item(10), item(11)})});
	REQUIRE(rows.size() == 4);
	CHECK_FALSE(voltz::canMoveUp(rows[0]));
	CHECK(voltz::canMoveDown(rows[0]));
	CHECK_FALSE(voltz::canMoveUp(rows[1]));
	CHECK(voltz::canMoveDown(rows[1]));
	CHECK(voltz::canMoveUp(rows[2]));
	CHECK_FALSE(voltz::canMoveDown(rows[2]));
	CHECK(voltz::canMoveUp(rows[3]));
	CHECK_FALSE(voltz::canMoveDown(rows[3]));
}
```
`frontend/voltz/tests/CMakeLists.txt`: acrescentar `test-source-tree.cpp` (ordem alfabética, depois de
`test-recording-settings.cpp`). CONFIGURAR; COMPILAR TESTES → **falha** (`voltz/core/source-tree.hpp` não existe).

- [ ] **Step 2: `frontend/voltz/core/source-tree.hpp`**

```cpp
#pragma once

#include <cstdint>
#include <vector>

namespace voltz {

// Item de uma cena na ordem em que o libobs enumera (de baixo para cima). Grupos trazem os filhos na
// mesma ordem; `collapsed` é a configuração privada "collapsed" do item do grupo (a mesma do OBS).
struct SourceNode {
	int64_t id = 0;
	bool isGroup = false;
	bool collapsed = false;
	std::vector<SourceNode> children;
};

// Linha do painel de Fontes, de cima para baixo. `groupId` = id do grupo pai (-1 no nível da cena).
// `siblingIndex` 0 = camada de cima entre os irmãos; `siblingCount` inclui a própria linha.
struct SourceRow {
	int64_t id = 0;
	int64_t groupId = -1;
	int depth = 0;
	bool isGroup = false;
	bool collapsed = false;
	int siblingIndex = 0;
	int siblingCount = 0;
};

// Achata a cena em linhas: camada de cima primeiro; filhos logo abaixo do grupo, se ele não estiver recolhido.
std::vector<SourceRow> flattenSourceTree(const std::vector<SourceNode> &bottomToTop);

// Ordem só muda entre irmãos (obs_sceneitem_set_order age dentro da cena ou do grupo do item).
inline bool canMoveUp(const SourceRow &row)
{
	return row.siblingIndex > 0;
}

inline bool canMoveDown(const SourceRow &row)
{
	return row.siblingIndex + 1 < row.siblingCount;
}

} // namespace voltz
```

- [ ] **Step 3: `frontend/voltz/core/source-tree.cpp`**

```cpp
#include "source-tree.hpp"

namespace voltz {

namespace {

void appendLevel(const std::vector<SourceNode> &bottomToTop, int64_t groupId, int depth, std::vector<SourceRow> &out)
{
	const int count = static_cast<int>(bottomToTop.size());
	for (int i = count - 1; i >= 0; i--) {
		const SourceNode &node = bottomToTop[static_cast<size_t>(i)];
		SourceRow row;
		row.id = node.id;
		row.groupId = groupId;
		row.depth = depth;
		row.isGroup = node.isGroup;
		row.collapsed = node.collapsed;
		row.siblingIndex = count - 1 - i;
		row.siblingCount = count;
		out.push_back(row);
		if (node.isGroup && !node.collapsed) {
			appendLevel(node.children, node.id, depth + 1, out);
		}
	}
}

} // namespace

std::vector<SourceRow> flattenSourceTree(const std::vector<SourceNode> &bottomToTop)
{
	std::vector<SourceRow> rows;
	appendLevel(bottomToTop, -1, 0, rows);
	return rows;
}

} // namespace voltz
```
`frontend/voltz/core/CMakeLists.txt`: `source-tree.cpp` em `PRIVATE` (depois de `recording-settings.cpp`) e
`source-tree.hpp` em `PUBLIC` (depois de `recording-settings.hpp`).

- [ ] **Step 4: Testes passam**

CONFIGURAR; COMPILAR TESTES; RODAR TESTES → todos passam (informe o total de casos).

- [ ] **Step 5: Ícones novos (Phosphor 2.1.1 oficial)**

Mesmo procedimento do Plano 2A (versão fixa, arquivo oficial do pacote; conferido que `stack`, `caret-right`,
`speaker-slash` e `headphones` existem em `package/assets/{regular,fill}` da 2.1.1). Os dois últimos são da Task 6,
mas vêm juntos para baixar uma vez só:
```bash
TMP="$(mktemp -d)" && (cd "$TMP" && npm pack @phosphor-icons/core@2.1.1 --silent && tar -xzf phosphor-icons-core-2.1.1.tgz)
DEST=frontend/voltz/shell/icons
for n in stack caret-right speaker-slash headphones; do
  cp "$TMP/package/assets/regular/$n.svg" "$DEST/regular/$n.svg"
  cp "$TMP/package/assets/fill/$n-fill.svg" "$DEST/fill/$n-fill.svg"
done
head -c 80 "$DEST/regular/stack.svg"; echo
rm -rf "$TMP"
```
Expected: o SVG começa com `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" fill="currentColor">`.
No `voltz-shell.qrc`, acrescentar (mantendo a ordem alfabética de cada bloco) as 8 linhas
`<file>icons/fill/caret-right-fill.svg</file>`, `<file>icons/fill/headphones-fill.svg</file>`,
`<file>icons/fill/speaker-slash-fill.svg</file>`, `<file>icons/fill/stack-fill.svg</file>`,
`<file>icons/regular/caret-right.svg</file>`, `<file>icons/regular/headphones.svg</file>`,
`<file>icons/regular/speaker-slash.svg</file>`, `<file>icons/regular/stack.svg</file>`.
Conferir: `grep -c "<file>" frontend/voltz/shell/voltz-shell.qrc` → `68` (60 + 8).

- [ ] **Step 6: Helper de acessibilidade em `Icons`**

`Icons.hpp`: depois de `#include <QString>`, nada novo; acrescentar `class QAbstractButton;` antes do
`namespace voltz {` e, antes de `loadShellFonts()`:
```cpp
// Botão só com ícone: a dica vira também o nome acessível (leitores de tela e UI Automation usam o nome).
// `context` (ex. nome da fonte) entra só no nome acessível: "Mostrar/ocultar — Tela".
void setButtonHint(QAbstractButton *button, const QString &hint, const QString &context = QString());
```
`Icons.cpp`: `#include <QAbstractButton>` e, antes de `loadShellFonts()`:
```cpp
void setButtonHint(QAbstractButton *button, const QString &hint, const QString &context)
{
	button->setToolTip(hint);
	button->setAccessibleName(context.isEmpty() ? hint : hint + QStringLiteral(" \u2014 ") + context);
}
```
`ErrorBanner.cpp`: trocar `close->setToolTip(QTStr("Voltz.Error.Dismiss"));` por
`setButtonHint(close, QTStr("Voltz.Error.Dismiss"));`.

- [ ] **Step 7: Textos (acrescentar ao fim dos dois locales)**

`voltz-pt-BR.ini`:
```ini
Voltz.Sources.Expand="Expandir grupo"
Voltz.Sources.Collapse="Recolher grupo"
```
`voltz-en-US.ini`:
```ini
Voltz.Sources.Expand="Expand group"
Voltz.Sources.Collapse="Collapse group"
```

- [ ] **Step 8: `SourcesPanel.hpp` (arquivo inteiro)**

```cpp
#pragma once

#include <voltz/core/source-tree.hpp>

#include <obs.hpp>

#include <QFrame>
#include <QString>

#include <atomic>
#include <vector>

class QLabel;
class QListWidget;
class QToolButton;

namespace voltz {

class FrontendBridge;

// Painel "FONTES": fontes da cena atual — grupos com os filhos (expandir/recolher) — com mostrar/ocultar,
// travar, ordem, adicionar, remover, propriedades e filtros.
class SourcesPanel : public QFrame {
	Q_OBJECT

public:
	explicit SourcesPanel(FrontendBridge *bridge, QWidget *parent = nullptr);

private slots:
	void flushRefresh();

private:
	void refresh();
	void bindCurrentScene();
	// Liga os sinais de cada grupo: mudanças nos filhos são emitidas pela fonte do grupo, não pela cena.
	void bindGroupSignals(const std::vector<OBSSceneItem> &groups);
	OBSScene currentScene() const;
	// Item de uma linha na cena dada: groupId < 0 = nível da cena; senão, filho do grupo com esse id.
	static OBSSceneItem findItemIn(obs_scene_t *scene, int64_t groupId, int64_t id);
	OBSSceneItem findItem(int64_t groupId, int64_t id) const;
	const SourceRow *selectedRow() const;
	OBSSceneItem selectedItem() const;
	QWidget *buildRow(obs_sceneitem_t *item, const SourceRow &row, bool reserveCaret);
	void toggleCollapsed(int64_t groupId);
	void scheduleRefresh();
	void updateButtons();
	void addSource();
	void removeSelected();
	void moveSelected(obs_order_movement movement);
	void openProperties();
	void openFilters();
	void showContextMenu(const QPoint &pos);

	// Thread do libobs: agenda um único flushRefresh() na thread da UI por rajada de sinais.
	static void queueRefresh(void *data, calldata_t *params);

	FrontendBridge *bridge_;
	OBSWeakSource sceneSource_;
	// ids de item são por cena (e por grupo): trocar de cena descarta a seleção.
	QString boundSceneUuid_;
	std::atomic<bool> refreshQueued_{false};
	std::vector<OBSSignal> sceneSignals_;
	std::vector<OBSSignal> groupSignals_;
	OBSSignal renameSignal_;
	// Uma entrada por linha da lista, na mesma ordem.
	std::vector<SourceRow> rows_;

	QListWidget *list_ = nullptr;
	QLabel *empty_ = nullptr;
	QToolButton *removeButton_ = nullptr;
	QToolButton *upButton_ = nullptr;
	QToolButton *downButton_ = nullptr;
	QToolButton *propertiesButton_ = nullptr;
};

} // namespace voltz
```

- [ ] **Step 9: `SourcesPanel.cpp` (arquivo inteiro)**

```cpp
#include "SourcesPanel.hpp"

#include "FrontendBridge.hpp"
#include "Icons.hpp"

#include <widgets/OBSBasic.hpp>

#include <obs-frontend-api.h>
#include <OBSApp.hpp> // QTStr

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <map>
#include <utility>

#include "moc_SourcesPanel.cpp"

namespace voltz {

namespace {

constexpr int kRowHeight = 34;
constexpr int kCaretSlot = 20;
constexpr int kIndentPerLevel = 22;
constexpr const char *kSceneSignals[] = {"item_add", "item_remove", "reorder", "refresh", "item_visible", "item_locked"};

QToolButton *makeIconButton(const QString &icon, const QString &hint, QWidget *parent)
{
	auto *button = new QToolButton(parent);
	button->setObjectName("iconButton");
	button->setIcon(phosphorIcon(icon, palette::kMuted));
	button->setIconSize(QSize(16, 16));
	setButtonHint(button, hint);
	return button;
}

bool collectItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	if (!obs_source_removed(obs_sceneitem_get_source(item))) {
		static_cast<std::vector<OBSSceneItem> *>(param)->emplace_back(item);
	}
	return true;
}

} // namespace

SourcesPanel::SourcesPanel(FrontendBridge *bridge, QWidget *parent) : QFrame(parent), bridge_(bridge)
{
	setObjectName("panel");

	auto *title = new QLabel(QTStr("Voltz.Panel.Sources"), this);
	title->setObjectName("panelTitle");
	QToolButton *addButton = makeIconButton(QStringLiteral("plus"), QTStr("Voltz.Sources.Add"), this);

	auto *header = new QHBoxLayout();
	header->setContentsMargins(0, 0, 0, 0);
	header->addWidget(title, 1);
	header->addWidget(addButton);

	list_ = new QListWidget(this);
	list_->setContextMenuPolicy(Qt::CustomContextMenu);
	empty_ = new QLabel(QTStr("Voltz.Sources.Empty"), this);
	empty_->setObjectName("muted");
	empty_->setWordWrap(true);

	removeButton_ = makeIconButton(QStringLiteral("trash"), QTStr("Voltz.Sources.Remove"), this);
	upButton_ = makeIconButton(QStringLiteral("caret-up"), QTStr("Voltz.Sources.MoveUp"), this);
	downButton_ = makeIconButton(QStringLiteral("caret-down"), QTStr("Voltz.Sources.MoveDown"), this);
	propertiesButton_ =
		makeIconButton(QStringLiteral("sliders-horizontal"), QTStr("Voltz.Sources.Properties"), this);

	auto *footer = new QHBoxLayout();
	footer->setContentsMargins(0, 0, 0, 0);
	footer->setSpacing(2);
	footer->addWidget(removeButton_);
	footer->addWidget(upButton_);
	footer->addWidget(downButton_);
	footer->addStretch(1);
	footer->addWidget(propertiesButton_);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(10, 10, 10, 8);
	layout->setSpacing(6);
	layout->addLayout(header);
	layout->addWidget(empty_);
	layout->addWidget(list_, 1);
	layout->addLayout(footer);

	connect(addButton, &QToolButton::clicked, this, &SourcesPanel::addSource);
	connect(removeButton_, &QToolButton::clicked, this, &SourcesPanel::removeSelected);
	connect(upButton_, &QToolButton::clicked, this, [this]() { moveSelected(OBS_ORDER_MOVE_UP); });
	connect(downButton_, &QToolButton::clicked, this, [this]() { moveSelected(OBS_ORDER_MOVE_DOWN); });
	connect(propertiesButton_, &QToolButton::clicked, this, &SourcesPanel::openProperties);
	connect(list_, &QListWidget::itemDoubleClicked, this, [this]() { openProperties(); });
	connect(list_, &QListWidget::currentRowChanged, this, [this]() { updateButtons(); });
	connect(list_, &QListWidget::customContextMenuRequested, this, &SourcesPanel::showContextMenu);
	connect(bridge_, &FrontendBridge::currentSceneChanged, this, [this]() { bindCurrentScene(); });
	connect(bridge_, &FrontendBridge::sceneListChanged, this, [this]() { bindCurrentScene(); });

	renameSignal_.Connect(obs_get_signal_handler(), "source_rename", &SourcesPanel::queueRefresh, this);
	bindCurrentScene();
}

void SourcesPanel::queueRefresh(void *data, calldata_t *)
{
	/* Thread do libobs: só agenda o refresh na thread da UI; uma rajada de sinais vira um refresh só. */
	static_cast<SourcesPanel *>(data)->scheduleRefresh();
}

void SourcesPanel::scheduleRefresh()
{
	if (!refreshQueued_.exchange(true)) {
		QMetaObject::invokeMethod(this, "flushRefresh", Qt::QueuedConnection);
	}
}

void SourcesPanel::flushRefresh()
{
	refreshQueued_ = false;
	refresh();
}

void SourcesPanel::bindCurrentScene()
{
	OBSSourceAutoRelease source = obs_frontend_get_current_scene();

	/* SCENE_LIST_CHANGED também chega aqui: mesma cena viva e já ligada, só redesenha. */
	OBSSource bound = OBSGetStrongRef(sceneSource_);
	if (source && bound.Get() == source.Get() && !sceneSignals_.empty()) {
		refresh();
		return;
	}

	const QString uuid = source ? QString::fromUtf8(obs_source_get_uuid(source)) : QString();
	if (uuid != boundSceneUuid_) {
		/* ids de item são por cena: o mesmo id em outra cena é outro item */
		list_->setCurrentItem(nullptr);
	}
	boundSceneUuid_ = uuid;

	sceneSignals_.clear();
	groupSignals_.clear();
	sceneSource_ = OBSGetWeakRef(source);
	if (source) {
		signal_handler_t *handler = obs_source_get_signal_handler(source);
		for (const char *signal : kSceneSignals) {
			sceneSignals_.emplace_back(handler, signal, &SourcesPanel::queueRefresh, this);
		}
	}
	refresh();
}

void SourcesPanel::bindGroupSignals(const std::vector<OBSSceneItem> &groups)
{
	/* Roda na thread da UI (refresh), fora de qualquer emissão de sinal do libobs. */
	groupSignals_.clear();
	for (const OBSSceneItem &group : groups) {
		signal_handler_t *handler = obs_source_get_signal_handler(obs_sceneitem_get_source(group));
		for (const char *signal : kSceneSignals) {
			groupSignals_.emplace_back(handler, signal, &SourcesPanel::queueRefresh, this);
		}
	}
}

OBSScene SourcesPanel::currentScene() const
{
	OBSSource source = OBSGetStrongRef(sceneSource_);
	return source ? OBSScene(obs_scene_from_source(source)) : OBSScene();
}

OBSSceneItem SourcesPanel::findItemIn(obs_scene_t *scene, int64_t groupId, int64_t id)
{
	if (!scene) {
		return OBSSceneItem();
	}
	if (groupId < 0) {
		return OBSSceneItem(obs_scene_find_sceneitem_by_id(scene, id));
	}
	obs_sceneitem_t *group = obs_scene_find_sceneitem_by_id(scene, groupId);
	if (!group || !obs_sceneitem_is_group(group)) {
		return OBSSceneItem();
	}
	return OBSSceneItem(obs_scene_find_sceneitem_by_id(obs_sceneitem_group_get_scene(group), id));
}

OBSSceneItem SourcesPanel::findItem(int64_t groupId, int64_t id) const
{
	OBSScene scene = currentScene();
	return findItemIn(scene, groupId, id);
}

const SourceRow *SourcesPanel::selectedRow() const
{
	const int row = list_->currentRow();
	return row >= 0 && static_cast<size_t>(row) < rows_.size() ? &rows_[static_cast<size_t>(row)] : nullptr;
}

OBSSceneItem SourcesPanel::selectedItem() const
{
	const SourceRow *row = selectedRow();
	return row ? findItem(row->groupId, row->id) : OBSSceneItem();
}

void SourcesPanel::refresh()
{
	const SourceRow *previous = selectedRow();
	const int64_t selectedGroup = previous ? previous->groupId : -1;
	const int64_t selectedId = previous ? previous->id : -1;
	const int scroll = list_->verticalScrollBar()->value();

	/* Árvore da cena com refs fortes: os itens continuam válidos enquanto as linhas são montadas. */
	std::vector<SourceNode> tree;
	std::vector<OBSSceneItem> groups;
	std::map<std::pair<int64_t, int64_t>, OBSSceneItem> items;
	if (OBSScene scene = currentScene()) {
		std::vector<OBSSceneItem> top;
		obs_scene_enum_items(scene, collectItem, &top);
		for (const OBSSceneItem &item : top) {
			SourceNode node;
			node.id = obs_sceneitem_get_id(item);
			node.isGroup = obs_sceneitem_is_group(item);
			items[{-1, node.id}] = item;
			if (node.isGroup) {
				OBSDataAutoRelease priv = obs_sceneitem_get_private_settings(item);
				node.collapsed = obs_data_get_bool(priv, "collapsed");
				groups.push_back(item);
				/* fora do enum da cena: enumerar o grupo dentro do callback travaria dois mutexes */
				std::vector<OBSSceneItem> children;
				obs_sceneitem_group_enum_items(item, collectItem, &children);
				for (const OBSSceneItem &child : children) {
					SourceNode childNode;
					childNode.id = obs_sceneitem_get_id(child);
					node.children.push_back(childNode);
					items[{node.id, childNode.id}] = child;
				}
			}
			tree.push_back(std::move(node));
		}
	}
	bindGroupSignals(groups);

	list_->clear();
	rows_ = flattenSourceTree(tree);
	const bool hasGroups = !groups.empty();
	for (const SourceRow &row : rows_) {
		auto *listItem = new QListWidgetItem(list_);
		listItem->setSizeHint(QSize(0, kRowHeight));
		auto found = items.find({row.groupId, row.id});
		if (found != items.end()) {
			list_->setItemWidget(listItem, buildRow(found->second, row, hasGroups));
		}
		if (row.groupId == selectedGroup && row.id == selectedId) {
			list_->setCurrentItem(listItem);
		}
	}

	empty_->setVisible(rows_.empty());
	list_->setVisible(!rows_.empty());
	list_->verticalScrollBar()->setValue(scroll);
	updateButtons();
}

QWidget *SourcesPanel::buildRow(obs_sceneitem_t *item, const SourceRow &row, bool reserveCaret)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const QString sourceName = QString::fromUtf8(obs_source_get_name(source));
	const int64_t groupId = row.groupId;
	const int64_t id = row.id;

	auto *widget = new QWidget(list_);
	auto *layout = new QHBoxLayout(widget);
	layout->setContentsMargins(2 + row.depth * kIndentPerLevel, 0, 4, 0);
	layout->setSpacing(6);

	if (row.isGroup) {
		auto *caret = new QToolButton(widget);
		caret->setObjectName("iconButton");
		caret->setIcon(phosphorIcon(row.collapsed ? QStringLiteral("caret-right") : QStringLiteral("caret-down"),
					    palette::kMuted));
		caret->setIconSize(QSize(12, 12));
		caret->setFixedSize(kCaretSlot, kCaretSlot);
		setButtonHint(caret, QTStr(row.collapsed ? "Voltz.Sources.Expand" : "Voltz.Sources.Collapse"),
			      sourceName);
		connect(caret, &QToolButton::clicked, this, [this, id]() { toggleCollapsed(id); });
		layout->addWidget(caret);
	} else if (reserveCaret) {
		/* alinha o ícone com o dos grupos */
		layout->addSpacing(kCaretSlot);
	}

	auto *icon = new QLabel(widget);
	const QIcon sourceIcon = row.isGroup ? phosphorIcon(QStringLiteral("stack"), palette::kMuted)
					     : OBSBasic::Get()->GetSourceIcon(obs_source_get_id(source));
	icon->setPixmap(sourceIcon.pixmap(16, 16));
	auto *name = new QLabel(sourceName, widget);
	name->setTextInteractionFlags(Qt::NoTextInteraction);

	auto *visible = new QToolButton(widget);
	visible->setObjectName("iconButton");
	visible->setCheckable(true);
	visible->setChecked(obs_sceneitem_visible(item));
	visible->setIcon(obs_sceneitem_visible(item) ? phosphorIcon(QStringLiteral("eye"), palette::kText)
						     : phosphorIcon(QStringLiteral("eye-slash"), palette::kMuted));
	visible->setIconSize(QSize(16, 16));
	setButtonHint(visible, QTStr("Voltz.Sources.ToggleVisible"), sourceName);

	auto *lock = new QToolButton(widget);
	lock->setObjectName("iconButton");
	lock->setCheckable(true);
	lock->setChecked(obs_sceneitem_locked(item));
	lock->setIcon(obs_sceneitem_locked(item)
			      ? phosphorIcon(QStringLiteral("lock-simple"), palette::kAccent, IconWeight::Fill)
			      : phosphorIcon(QStringLiteral("lock-simple-open"), palette::kMuted));
	lock->setIconSize(QSize(16, 16));
	setButtonHint(lock, QTStr("Voltz.Sources.ToggleLock"), sourceName);

	layout->addWidget(icon);
	layout->addWidget(name, 1);
	layout->addWidget(lock);
	layout->addWidget(visible);

	/* Os botões só mudam o estado no OBS; o sinal da cena/grupo dispara refresh() e redesenha a linha. */
	connect(visible, &QToolButton::clicked, this, [this, groupId, id](bool checked) {
		if (OBSSceneItem target = findItem(groupId, id)) {
			obs_sceneitem_set_visible(target, checked);
		}
	});
	connect(lock, &QToolButton::clicked, this, [this, groupId, id](bool checked) {
		if (OBSSceneItem target = findItem(groupId, id)) {
			obs_sceneitem_set_locked(target, checked);
		}
	});
	return widget;
}

void SourcesPanel::toggleCollapsed(int64_t groupId)
{
	OBSSceneItem group = findItem(-1, groupId);
	if (!group || !obs_sceneitem_is_group(group)) {
		return;
	}
	OBSDataAutoRelease priv = obs_sceneitem_get_private_settings(group);
	obs_data_set_bool(priv, "collapsed", !obs_data_get_bool(priv, "collapsed"));
	/* Configuração privada não emite sinal: redesenha. Adiado porque o botão clicado pertence à linha
	 * que o refresh vai destruir. */
	scheduleRefresh();
}

void SourcesPanel::updateButtons()
{
	const SourceRow *row = selectedRow();
	removeButton_->setEnabled(row != nullptr);
	/* grupo não tem propriedades (só filtros, pelo menu de contexto) */
	propertiesButton_->setEnabled(row != nullptr && !row->isGroup);
	upButton_->setEnabled(row != nullptr && canMoveUp(*row));
	downButton_->setEnabled(row != nullptr && canMoveDown(*row));
}

void SourcesPanel::addSource()
{
	/* OBSBasic::AddSourceDialog só abre quando chamado por uma QAction (usa sender()). */
	if (QAction *action = OBSBasic::Get()->findChild<QAction *>(QStringLiteral("actionAddSource"))) {
		action->trigger();
	} else {
		blog(LOG_WARNING, "[voltz] acao actionAddSource nao encontrada");
	}
}

void SourcesPanel::removeSelected()
{
	/* Cena e ids capturados antes do diálogo modal: durante ele a cena atual e a seleção podem mudar. */
	OBSScene scene = currentScene();
	const SourceRow *row = selectedRow();
	if (!scene || !row) {
		return;
	}
	const int64_t groupId = row->groupId;
	const int64_t id = row->id;
	QString name;
	{
		OBSSceneItem item = findItemIn(scene, groupId, id);
		if (!item) {
			return;
		}
		name = QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item)));
	}

	const auto answer = QMessageBox::question(this, QTStr("Voltz.Sources.RemoveConfirm.Title"),
						  QTStr("Voltz.Sources.RemoveConfirm.Text").arg(name.toHtmlEscaped()));
	if (answer != QMessageBox::Yes) {
		return;
	}
	if (OBSSceneItem item = findItemIn(scene, groupId, id)) {
		obs_sceneitem_remove(item);
	}
}

void SourcesPanel::moveSelected(obs_order_movement movement)
{
	if (OBSSceneItem item = selectedItem()) {
		obs_sceneitem_set_order(item, movement);
	}
}

void SourcesPanel::openProperties()
{
	const SourceRow *row = selectedRow();
	if (!row || row->isGroup) {
		return;
	}
	if (OBSSceneItem item = selectedItem()) {
		obs_frontend_open_source_properties(obs_sceneitem_get_source(item));
	}
}

void SourcesPanel::openFilters()
{
	if (OBSSceneItem item = selectedItem()) {
		obs_frontend_open_source_filters(obs_sceneitem_get_source(item));
	}
}

void SourcesPanel::showContextMenu(const QPoint &pos)
{
	if (!list_->itemAt(pos)) {
		return;
	}
	const SourceRow *row = selectedRow();
	QMenu menu(this);
	if (!row || !row->isGroup) {
		menu.addAction(QTStr("Voltz.Sources.Properties"), this, &SourcesPanel::openProperties);
	}
	menu.addAction(QTStr("Voltz.Sources.Filters"), this, &SourcesPanel::openFilters);
	menu.addSeparator();
	menu.addAction(QTStr("Voltz.Sources.Remove"), this, &SourcesPanel::removeSelected);
	menu.exec(list_->viewport()->mapToGlobal(pos));
}

} // namespace voltz
```
Observações: `rows_` só é trocado depois de `list_->clear()` — durante o `clear` a linha atual vira −1 e
`updateButtons` não lê `rows_` velho fora do índice. O `kSceneSignals` substitui a lista literal que existia em
`bindCurrentScene`.

- [ ] **Step 10: CONFIGURAR (ícones novos no `.qrc`), FORMATAR, COMPILAR TUDO, RODAR TESTES, SMOKE**

FORMATAR `source-tree.hpp/.cpp`, `test-source-tree.cpp`, `SourcesPanel.hpp/.cpp`, `Icons.hpp/.cpp`, `ErrorBanner.cpp`.
Expected: build `exit=0`, testes passam, `SMOKE OK`.

- [ ] **Step 11: Verificação (grupo do preset Tutorial, câmera neutralizada)**

Bloco de teste. **Sem capturas até o passo 2 terminar.**
1. `-Name "Presets"`, `-Name ("Tutorial " + [char]0x2014 + " Aplicar")`, `Start-Sleep 3`.
2. Neutralizar a câmera: `req.json` =
   `[{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":"Câmera redonda"}}]` → anote o
   `sourceName` do filho (ex. `Câmera`); depois
   `[{"requestType":"SetInputSettings","requestData":{"inputName":"<filho>","inputSettings":{"video_device_id":"voltz-teste-sem-camera"}}},{"requestType":"SetSceneItemEnabled","requestData":{"sceneName":"Câmera redonda","sceneItemId":<id do filho>,"sceneItemEnabled":false}}]`.
   (O nome do grupo vem de `Voltz.Source.RoundCamera`; se o app estiver em inglês, `Round camera`.)
3. `-Name "Gravar"`; `-List`: aparecem `Recolher grupo — Câmera redonda`, `Mostrar/ocultar — Câmera redonda`,
   `Mostrar/ocultar — <filho>`, `Travar/destravar — <filho>`, e os botões do rodapé com nome
   (`Adicionar fonte`, `Remover`, `Mover para cima`, `Mover para baixo`, `Propriedades`).
4. `-Name ("Mostrar/ocultar " + [char]0x2014 + " <filho>")` → `GetGroupSceneItemList` mostra o filho com
   `sceneItemEnabled: true` (estava false); acione de novo → false.
5. `-Name ("Recolher grupo " + [char]0x2014 + " Câmera redonda")` → `-List` não tem mais
   `Mostrar/ocultar — <filho>` e tem `Expandir grupo — Câmera redonda`.
6. Expandir de novo; `Shot $shell "t5-fontes"` (câmera neutralizada e oculta): linha do grupo com seta e ícone
   `stack`, filho recuado com o ícone do OBS, olho/cadeado alinhados.
7. Teste manual (Task 8): remover um filho com confirmação; mover filho para cima/baixo dentro do grupo.

- [ ] **Step 12: Commit**

```bash
git add frontend/voltz/core/source-tree.hpp frontend/voltz/core/source-tree.cpp frontend/voltz/core/CMakeLists.txt frontend/voltz/tests/test-source-tree.cpp frontend/voltz/tests/CMakeLists.txt frontend/voltz/shell/icons frontend/voltz/shell/voltz-shell.qrc frontend/voltz/shell/SourcesPanel.hpp frontend/voltz/shell/SourcesPanel.cpp frontend/voltz/shell/Icons.hpp frontend/voltz/shell/Icons.cpp frontend/voltz/shell/ErrorBanner.cpp frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): grupos com filhos no painel de Fontes e nome acessivel nos botoes so-icone

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Áudio — acabamento visual (e confirmação de que já reage às Configurações)

**Files:**
- Create: `tools/voltz-qss-icons.sh`, `frontend/voltz/shell/icons/qss/*.svg` (8, gerados)
- Modify: `frontend/voltz/shell/voltz-shell.qrc`, `voltz-shell.qss`, `AudioPanel.cpp`
- Modify: `frontend/data/locale/voltz-pt-BR.ini`, `voltz-en-US.ini`

Por que SVG pré-colorido: os botões de mudo/monitor do `VolumeControl` recebem o ícone por QSS
(`qproperty-icon` nas classes `.btn-mute`/`.btn-monitor`, `Yami.obt:1656-1702`) — o `VolumeControl::processMixerState`
chama `setIcon` e logo depois `style()->polish`, que reaplica o ícone do QSS. Ícone via QSS é carregado direto do
arquivo (sem `PhosphorIconEngine`), então o `currentColor` dos SVGs Phosphor precisa vir trocado pela cor.
No Qt, o seletor `.btn-mute` casa com a propriedade dinâmica `class` (é como o tema do OBS funciona).

- [ ] **Step 1: Confirmar "reagir a Configurações → Áudio" (sem código)**

Run:
```bash
grep -n "profileSettingChanged" frontend/components/VolumeControl.cpp frontend/settings/OBSBasicSettings.cpp
grep -n "bool VolumeMeter::needLayoutChange" frontend/components/VolumeMeter.cpp
```
Expected: `VolumeControl.cpp:138` (conecta), `OBSBasicSettings.cpp:3676` e `:3683` (emite) e
`VolumeMeter.cpp:414`. Conclusão a registrar no backlog (Task 8): decaimento e tipo de medidor já são reaplicados
nos controles da casca pelo upstream; canais são relidos pelo medidor. Nenhum patch. O `AudioPanel` continua
aplicando os dois valores na criação (o `VolumeControl` não lê na construção).

- [ ] **Step 2: `tools/voltz-qss-icons.sh`**

```bash
#!/usr/bin/env bash
# Gera SVGs pre-coloridos para regras QSS (image:/qproperty-icon: nao passam pelo PhosphorIconEngine,
# entao o currentColor dos SVGs Phosphor precisa vir trocado pela cor final).
# Uso (da raiz do repo): bash tools/voltz-qss-icons.sh   -> sobrescreve frontend/voltz/shell/icons/qss/
set -euo pipefail
SRC=frontend/voltz/shell/icons
DEST="$SRC/qss"
mkdir -p "$DEST"
# icone peso nome-da-cor cor
while read -r name weight cname hex; do
  [ -z "$name" ] && continue
  if [ "$weight" = fill ]; then in="$SRC/fill/$name-fill.svg"; else in="$SRC/regular/$name.svg"; fi
  sed "s/currentColor/$hex/g" "$in" > "$DEST/$name-$cname.svg"
done <<'EOF'
caret-down regular muted #82828C
caret-down regular accent #D4F53C
caret-up regular accent #D4F53C
speaker-high regular text #F2F2F3
speaker-slash regular rec #FF4D4D
warning-circle regular warn #F5C83C
headphones regular muted #82828C
headphones fill accent #D4F53C
EOF
ls "$DEST"
```
Run: `bash tools/voltz-qss-icons.sh` → 8 arquivos; `grep -L "currentColor" frontend/voltz/shell/icons/qss/*.svg | wc -l` → `8`
(nenhum ficou com `currentColor`). No `.qrc`, acrescentar depois do último `icons/fill/...` as 8 linhas
`<file>icons/qss/caret-down-accent.svg</file>`, `<file>icons/qss/caret-down-muted.svg</file>`,
`<file>icons/qss/caret-up-accent.svg</file>`, `<file>icons/qss/headphones-accent.svg</file>`,
`<file>icons/qss/headphones-muted.svg</file>`, `<file>icons/qss/speaker-high-text.svg</file>`,
`<file>icons/qss/speaker-slash-rec.svg</file>`, `<file>icons/qss/warning-circle-warn.svg</file>`
→ `grep -c "<file>"` = `76`. (Os carets servem a Task 7.)

- [ ] **Step 3: QSS do painel de Áudio (acrescentar ao fim de `voltz-shell.qss`)**

As regras ficam depois de `#voltzShell, #voltzShell QWidget { font-size: 13px }` e têm especificidade igual ou
maior; a fonte pequena do medidor é o que desfaz a escala sobreposta.
```css
/* --- Painel de Áudio: VolumeControl do OBS dentro da casca --- */
#voltzShell VolumeControl {
	background: transparent;
}

#voltzShell VolumeName {
	background: transparent;
	border: none;
	padding: 2px 0;
}

#voltzShell VolumeName QLabel {
	font-size: 12px;
	font-weight: 600;
	color: #F2F2F3;
}

#voltzShell VolumeName::down-arrow {
	image: url(:/voltz/icons/qss/caret-down-muted.svg);
	width: 10px;
	height: 10px;
}

#voltzShell VolumeControl QLabel#volLabel {
	color: #82828C;
	font-size: 11px;
	min-width: 48px;
}

#voltzShell VolumeMeter {
	font-size: 8px;
	color: #82828C;
	qproperty-majorTickColor: #82828C;
	qproperty-minorTickColor: #3A3A42;
	qproperty-backgroundNominalColor: #2F3712;
	qproperty-backgroundWarningColor: #4A4013;
	qproperty-backgroundErrorColor: #4A1A1A;
	qproperty-foregroundNominalColor: #D4F53C;
	qproperty-foregroundWarningColor: #F5C83C;
	qproperty-foregroundErrorColor: #FF4D4D;
	qproperty-backgroundNominalColorDisabled: #2C2C34;
	qproperty-backgroundWarningColorDisabled: #34343C;
	qproperty-backgroundErrorColorDisabled: #26262D;
	qproperty-foregroundNominalColorDisabled: #82828C;
	qproperty-foregroundWarningColorDisabled: #9A9AA3;
	qproperty-foregroundErrorColorDisabled: #6A6A73;
}

#voltzShell VolumeControl QSlider::groove:horizontal {
	background: #1F1F24;
	height: 4px;
	border: none;
	border-radius: 2px;
}

#voltzShell VolumeControl QSlider::sub-page:horizontal {
	background: #D4F53C;
	border-radius: 2px;
}

#voltzShell VolumeControl QSlider::sub-page:horizontal:disabled {
	background: #3A3A42;
}

#voltzShell VolumeControl QSlider::handle:horizontal {
	background: #F2F2F3;
	width: 12px;
	height: 12px;
	margin: -4px 0;
	border-radius: 6px;
}

#voltzShell VolumeControl QSlider::handle:horizontal:hover {
	background: #FFFFFF;
}

#voltzShell VolumeControl QPushButton.btn-mute,
#voltzShell VolumeControl QPushButton.btn-monitor {
	background: transparent;
	border: none;
	border-radius: 6px;
	padding: 4px;
	margin: 0 2px;
	min-height: 0;
	icon-size: 16px;
}

#voltzShell VolumeControl QPushButton.btn-mute:hover,
#voltzShell VolumeControl QPushButton.btn-monitor:hover {
	background: #18181C;
}

#voltzShell VolumeControl QPushButton.btn-mute {
	qproperty-icon: url(:/voltz/icons/qss/speaker-high-text.svg);
}

#voltzShell VolumeControl QPushButton.btn-mute.checked {
	qproperty-icon: url(:/voltz/icons/qss/speaker-slash-rec.svg);
}

#voltzShell VolumeControl QPushButton.btn-monitor {
	qproperty-icon: url(:/voltz/icons/qss/headphones-muted.svg);
}

#voltzShell VolumeControl QPushButton.btn-monitor.checked {
	background: rgba(212, 245, 60, 26);
	qproperty-icon: url(:/voltz/icons/qss/headphones-accent.svg);
}

/* por último: aviso (fonte sem trilha ou só monitor) vence o estado de mudo */
#voltzShell VolumeControl QPushButton.btn-mute.mute-warning {
	qproperty-icon: url(:/voltz/icons/qss/warning-circle-warn.svg);
}
```

- [ ] **Step 4: `AudioPanel.cpp` — esconder o rótulo de categoria e nomear o botão de monitor**

Includes: `#include <QPushButton>`, `#include <QStringList>`. No `namespace {`, depois de `applyFriendlyName`:
```cpp
// Classes de estilo do OBS ficam na propriedade dinâmica "class" (Idian::Utils::addClass).
bool hasStyleClass(const QWidget *widget, const char *styleClass)
{
	return widget->property("class").toString().split(QLatin1Char(' '), Qt::SkipEmptyParts)
		.contains(QLatin1String(styleClass));
}
```
Em `addChannel`, logo antes de `controls_->addWidget(control);`:
```cpp
	/* O rótulo de categoria do mixer ("Global"/"Ativo") não informa nada na casca. */
	for (QLabel *label : control->findChildren<QLabel *>()) {
		if (hasStyleClass(label, "mixer-category")) {
			label->hide();
		}
	}
	/* O OBS nomeia só o botão de mudo (VolControl.Mute); o de monitor fica sem nome para leitores de tela. */
	const QString name = QString::fromUtf8(obs_source_get_name(source));
	for (QPushButton *button : control->findChildren<QPushButton *>()) {
		if (hasStyleClass(button, "btn-monitor")) {
			button->setAccessibleName(QTStr("Voltz.Audio.Monitor") + QStringLiteral(" \u2014 ") + name);
		}
	}
```
Textos (fim dos dois locales): pt-BR `Voltz.Audio.Monitor="Ouvir no fone"`; en-US `Voltz.Audio.Monitor="Monitor in headphones"`.

- [ ] **Step 5: CONFIGURAR (ícones no `.qrc`), FORMATAR `AudioPanel.cpp`, COMPILAR APP, SMOKE**

- [ ] **Step 6: Verificação visual**

Bloco de teste, config limpa (sem preset → sem câmera). `Shot $shell "t6-audio"` na tela Gravar e recorte
mental do painel de Áudio. Critérios:
1. Escala de dB legível (`-60 … 0` sem números encavalados) na coluna de 280 px.
2. Slider: trilho `#1F1F24`, parte preenchida lima, alça branca redonda.
3. Mudo = alto-falante claro (vermelho riscado quando mudo); monitor = fone cinza (lima com fundo lima-claro
   quando ligado); botões sem a pílula `#18181C` da casca.
4. Sem o rótulo "Global"; nome da fonte em DM Sans 12 px semibold com a setinha de menu.
5. `-List` contém `Ouvir no fone — Som do sistema` (e/ou `— Microfone`).
**Incertezas a conferir aqui:** (a) se a seta do nome continuar o triângulo do estilo, a regra
`VolumeName::down-arrow` não pegou (o `VolumeName` desenha `PE_IndicatorArrowDown` direto no `paintEvent`) —
deixe como está e anote no backlog; (b) se os ícones de mudo/monitor não trocarem, confira com `-List` se a
`class` contém `btn-mute`/`btn-monitor` e teste a regra sem o prefixo de tipo (`#voltzShell VolumeControl .btn-mute`);
(c) se a escala ainda encavalar, troque `font-size: 8px` por `7px` e confira de novo.
Opcional (melhor esforço): mutar o canal pelo websocket (`SetInputMute`) e capturar de novo para ver o ícone vermelho.

- [ ] **Step 7: Commit**

```bash
git add tools/voltz-qss-icons.sh frontend/voltz/shell/icons/qss frontend/voltz/shell/voltz-shell.qrc frontend/voltz/shell/voltz-shell.qss frontend/voltz/shell/AudioPanel.cpp frontend/data/locale/voltz-pt-BR.ini frontend/data/locale/voltz-en-US.ini
git commit -m "feat(voltz): acabamento do painel de Audio (escala, slider lima, icones Phosphor de mudo/monitor)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Acabamento visual — seta do combo, cabeçalho de Projetos, rótulos de Ajustes

**Files:**
- Modify: `frontend/voltz/shell/voltz-shell.qss` (regras `QComboBox`, `QTreeWidget#projectsList::item`, `QHeaderView`)
- Modify: `frontend/voltz/shell/SettingsPage.cpp` (formulários em `QGridLayout`)

Decisões: a seta do combo usa o SVG pré-colorido da Task 6 (QSS `image:` não passa pelo `PhosphorIconEngine`);
o desalinhamento de "Tamanho" vem do `padding-right: 22px` só no cabeçalho — as células recebem o **mesmo**
padding horizontal, então texto à direita do cabeçalho e dos valores termina no mesmo x; o `QFormLayout` deixa o
rótulo no topo da linha (≈5 px acima do centro do combo) — troca por `QGridLayout` com `Qt::AlignVCenter`.

- [ ] **Step 1: Captura de referência (antes)**

Bloco de teste: `-Name "Ajustes"` → `Shot $shell "t7-ajustes-antes"`; anote se os combos de Formato/Qualidade
ocupam a largura toda ou só o tamanho do texto (o `QGridLayout` abaixo mantém "tamanho do texto"; se antes era a
largura toda, troque `Qt::AlignLeft | Qt::AlignVCenter` por `Qt::AlignVCenter` no `addRow(QLabel*, QWidget*)`).

- [ ] **Step 2: QSS**

(a) Na regra `#voltzShell QComboBox {`, trocar `padding: 5px 10px;` por `padding: 5px 30px 5px 10px;`.
(b) Acrescentar logo depois da regra `#voltzShell QComboBox:disabled { ... }`:
```css
#voltzShell QComboBox::drop-down {
	subcontrol-origin: padding;
	subcontrol-position: center right;
	width: 26px;
	border: none;
	background: transparent;
}

#voltzShell QComboBox::down-arrow {
	image: url(:/voltz/icons/qss/caret-down-muted.svg);
	width: 12px;
	height: 12px;
}
```
(combo desativado — Ajustes travado durante a gravação — mantém a seta; o texto já fica `#4A4A52`.)
(c) Na regra `#voltzShell QTreeWidget#projectsList::item {`, trocar `padding: 6px 4px;` por
`padding: 6px 22px 6px 4px;` (mesmo padding horizontal do `QHeaderView::section`).
(d) Substituir a regra das setas do cabeçalho por:
```css
#voltzShell QHeaderView::down-arrow {
	image: url(:/voltz/icons/qss/caret-down-accent.svg);
	width: 9px;
	height: 9px;
	subcontrol-origin: padding;
	subcontrol-position: center right;
	right: 6px;
}

#voltzShell QHeaderView::up-arrow {
	image: url(:/voltz/icons/qss/caret-up-accent.svg);
	width: 9px;
	height: 9px;
	subcontrol-origin: padding;
	subcontrol-position: center right;
	right: 6px;
}
```

- [ ] **Step 3: `SettingsPage.cpp` — formulários em grade**

Trocar `#include <QFormLayout>` por `#include <QGridLayout>`. No `namespace {` do topo, depois de `encoderLabel`:
```cpp
// Grade "rótulo | campo" com o rótulo centrado na altura do campo. O QFormLayout deixava o rótulo no topo da
// linha (~5 px acima do centro dos combos).
class FormGrid {
public:
	FormGrid() : grid_(new QGridLayout())
	{
		grid_->setContentsMargins(0, 0, 0, 0);
		grid_->setHorizontalSpacing(16);
		grid_->setVerticalSpacing(10);
		grid_->setColumnStretch(1, 1);
	}

	void addRow(QLabel *label, QWidget *field)
	{
		grid_->addWidget(label, rows_, 0, Qt::AlignLeft | Qt::AlignVCenter);
		grid_->addWidget(field, rows_, 1, Qt::AlignLeft | Qt::AlignVCenter);
		rows_++;
	}

	void addRow(QLabel *label, QLayout *field)
	{
		grid_->addWidget(label, rows_, 0, Qt::AlignLeft | Qt::AlignVCenter);
		grid_->addLayout(field, rows_, 1);
		rows_++;
	}

	QGridLayout *layout() const { return grid_; }

private:
	QGridLayout *grid_;
	int rows_ = 0;
};
```
Em `buildRecordingSection`, trocar o bloco do `QFormLayout` (de `auto *form = new QFormLayout();` até
`body->addLayout(form);`) por:
```cpp
	FormGrid form;
	form.addRow(formLabel(QTStr("Voltz.Settings.Folder"), recording_), folderRow);
	form.addRow(formLabel(QTStr("Voltz.Settings.Format"), recording_), format_);
	form.addRow(formLabel(QTStr("Voltz.Settings.Quality"), recording_), quality_);
	form.addRow(formLabel(QTStr("Voltz.Settings.Encoder"), recording_), encoder_);
	body->addLayout(form.layout());
```
Em `buildCaptureSection`, idem:
```cpp
	FormGrid form;
	form.addRow(formLabel(QTStr("Voltz.Settings.Monitor"), section), monitor_);
	form.addRow(formLabel(QTStr("Voltz.Settings.Camera"), section), camera_);
	form.addRow(formLabel(QTStr("Voltz.Settings.Microphone"), section), microphone_);
	body->addLayout(form.layout());
```
(o `alignFormLabels()` continua igualando a largura dos rótulos entre as seções.)

- [ ] **Step 4: FORMATAR `SettingsPage.cpp`, COMPILAR APP, SMOKE**

- [ ] **Step 5: Verificação visual**

Bloco de teste, com `New-Item -ItemType Directory -Force "$sp\proj-2c"` e três arquivos falsos criados **antes**
de abrir: `a.mp4` (1 KB), `b.mkv` (2 MB), `c.txt`. Depois de abrir:
`[{"requestType":"SetRecordDirectory","requestData":{"recordDirectory":"<$sp>\\proj-2c"}}]` pelo `$ws`;
`-Name "Projetos"`; `-Name "Atualizar"`; `Shot $shell "t7-projetos"`; clique no cabeçalho não é necessário
(a lista já ordena por Data). `-Name "Ajustes"`; `Shot $shell "t7-ajustes"`. Critérios:
1. Projetos: seta de ordenação lima visível no cabeçalho "Data"; o texto "Tamanho" termina no mesmo x dos valores
   (`2,0 MB`, `1,0 KB`).
2. Ajustes: seta `caret-down` cinza nos combos (Formato, Qualidade, Monitor/Câmera/Microfone), sem o triângulo do
   OBS; rótulos centrados na altura dos campos (compare com `t7-ajustes-antes`).
Apague `$sp\proj-2c` ao final.

- [ ] **Step 6: Commit**

```bash
git add frontend/voltz/shell/voltz-shell.qss frontend/voltz/shell/SettingsPage.cpp
git commit -m "fix(voltz): seta Phosphor nos combos, cabecalho de Projetos alinhado e rotulos de Ajustes centrados

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Verificação, registro e push

**Files:** `docs/voltz/backlog-plano-2.md`, `docs/voltz/upstream-patches.md` (conferência), `CLAUDE.md`, `AGENTS.md`

- [ ] **Step 1:** COMPILAR TUDO (segundo plano), RODAR TESTES (todos passam; informe o total de casos), SMOKE,
  SMOKE GRAVAÇÃO.
- [ ] **Step 2:** Capturas finais **só da janela** (bloco de teste, `HideOBSWindowsFromCapture=false`, config
  limpa, sem câmera): Gravar, Projetos, Presets, Ajustes; e o modo clássico. Descreva cada uma no relatório.
- [ ] **Step 3:** `git diff 0dd83017e..HEAD --stat` → os arquivos do OBS alterados são exatamente
  `frontend/OBSApp_Themes.cpp`, `frontend/OBSApp.hpp`, `frontend/OBSApp.cpp`, `frontend/widgets/OBSBasic_SysTray.cpp`,
  `frontend/widgets/OBSBasic_Recording.cpp`, `frontend/widgets/OBSBasic.cpp`, `frontend/dialogs/OBSAbout.cpp`
  (+ `frontend/data/themes/Voltz.ovt` novo) e todos aparecem em `upstream-patches.md`.
  `git diff 0dd83017e..HEAD -- frontend/OBSApp*.cpp frontend/OBSApp.hpp frontend/widgets frontend/dialogs | grep "^[+-]" | grep -v "^+++\|^---"`
  → toda linha nova está entre `#ifdef VOLTZ_STUDIO` … `#endif` (ou no `#else` que preserva o upstream).
- [ ] **Step 4: Backlog** (`docs/voltz/backlog-plano-2.md`):
  - Nova seção `## Feito no Plano 2C` (depois de "Feito no Plano 2B"):
    ```markdown
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
    - **Acabamento:** seta `caret-down` nos combos, seta de ordenação lima e "Tamanho" alinhado em Projetos,
      rótulos de Ajustes centrados.
    ```
  - Remover os itens resolvidos: em "Casca / ciclo de vida" — Bandeja; "Ocultar janelas do OBS da captura";
    Confirmação de saída própria; Diálogo de encerramento inesperado; Menu clássico; Erro de gravação avisado duas
    vezes; Grupos no painel de Fontes; Painel de Áudio não reage; Acabamento do painel de Áudio; Ícone de grupo;
    Acabamento de Projetos/Ajustes. Em "Marca / i18n" — Acessibilidade dos botões só com ícone; "Sobre" do clássico.
    (Se a seta do `VolumeName` não pegou na Task 6, deixe um item curto sobre ela.)
  - Manter: `obs_frontend_get_main_window()`, Rótulos de áudio, macOS, Presets em ultrawide, Ajustes › Captura
    sem hotplug. Acrescentar em "Casca / ciclo de vida":
    `- **Pasta de gravação inválida:** o OBS avisa só por balão da bandeja (`GetOutputFilename`, `OBSApp.cpp`); a casca não mostra nada.`
    e em "Marca / i18n": `- Item "Encerrar OBS" do menu da bandeja (chave `Exit` do OBS) ainda diz "OBS".`
  - Em "Testes / ferramentas", nova linha **Testes manuais pendentes do 2C:** clique no ícone da bandeja e
    "Mostrar/Ocultar" alternam a casca (texto do item correto); restaurar pela bandeja depois de minimizar para a
    bandeja; ícone de estado no botão da barra de tarefas (gravando/pausado); remover e mover filhos de grupo;
    reagir a Configurações → Áudio (decaimento/tipo de medidor) com som real; menu Ajuda e "Sobre" do clássico (se a
    Task 4 não automatizou).
- [ ] **Step 5: `CLAUDE.md`** (e depois `cp CLAUDE.md AGENTS.md`):
  - *Estado atual*, +1 linha: `- Plano 2C concluído: tema Voltz (.ovt) padrão nas janelas do OBS, bandeja/minimizar conduzem a casca, app fora da captura por padrão (Ajustes › Captura), confirmação de saída e diálogo de falha próprios, grupos e acessibilidade em Fontes, acabamento de Áudio/Projetos/Ajustes.`
  - *Próximos passos* → `1. Escrever e executar o Plano 3 (gravação em projeto: programa.mp4 + ISO tela/câmera + cursor.jsonl + projeto.voltz, avisos de disco) a partir da spec §5 e §8.` e `2. Depois: Plano 4 (anotações + mouse), 5 (marca visual + instalador + CI).`
  - *Decisões*, +1 linha: `- Ícones em regras QSS (image:/qproperty-icon:) = SVG pré-colorido em icons/qss/ (tools/voltz-qss-icons.sh); no código, sempre phosphorIcon().`
  - *Armadilhas*, +1 linha cada (só as que se confirmaram na execução): `PrintWindow` sai preto com "esconder da
    captura" ligado → testes escrevem `HideOBSWindowsFromCapture=false` no `user.ini` portátil; filhos de grupo
    emitem sinais pela fonte do grupo (ligar `signal_handler` de cada grupo); `#voltzShell QWidget { font-size }`
    vale para widgets do OBS dentro da casca (ex. `VolumeMeter`) — sobrescrever com regra própria.
  - Conferir o tamanho: `wc -c CLAUDE.md` abaixo de ~10 KB; se passar, mover narrativa para `docs/HISTORICO.md`.
- [ ] **Step 6:** Commit e push:
```bash
git add docs/voltz/backlog-plano-2.md docs/voltz/upstream-patches.md CLAUDE.md AGENTS.md
git commit -m "docs(voltz): registro do Plano 2C (backlog, memoria)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push origin voltz
```

---

## Auto-revisão (feita ao escrever)

- **Cobertura do escopo:** tema `.ovt` padrão + instalação + casca intacta (T1); bandeja/minimizar/balão (T2);
  esconder da captura com padrão ligado + interruptor + overlay (T3); confirmação de saída, falha, menu, "Sobre",
  locales (T4); grupos, ícone de grupo, ações nos filhos, acessibilidade com helper único (T5); Áudio visual +
  reação às Configurações (T6, sem patch: já coberto); combo, Projetos, rótulos (T7); verificação/registro/push (T8).
- **Placeholders:** nenhum "TBD"; onde só a execução decide (seta do `VolumeName`, ícones por classe QSS, tamanho
  da fonte do medidor, `GetWindowDisplayAffinity` entre processos, UIA em menus do Qt, largura dos combos na grade)
  há passo de verificação com critério e alternativa.
- **Consistência de nomes:** `IsVoltzShellVisible`/`ToggleVoltzShell` (OBSApp) ↔ `toggleFromTray`/`showFromTray`
  (VoltzShell); `setTaskbarState`/`TaskbarState`, `logCaptureAffinity`, `shellStyleSheet` (WindowChrome);
  `confirmExitWhileActive` (ExitDialog); `flattenSourceTree`/`SourceNode`/`SourceRow`/`canMoveUp`/`canMoveDown`
  (core); `setButtonHint` (Icons); ícones `icons/qss/<nome>-<cor>.svg` iguais no script, no `.qrc` e no QSS.
