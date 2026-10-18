<#
  Teste de integracao do Plano 4C (modo desenho). O modo desenho NUNCA liga aqui.
  Seguranca (o PC e do usuario e esta em uso):
  - VOLTZ_TEST_LIVE=1 recusa o modo desenho duas vezes (DrawModeController e DrawInputWindow). O unico gatilho usado e
    TriggerHotkeyByName "VoltzStudio.Draw", so para provar a recusa. O botao de desenhar da casca nunca e acionado.
  - Nenhuma entrada e injetada; nada e posto em primeiro plano. O unico P/Invoke que escreve e PostMessage(WM_CLOSE) na
    janela da casca do PID lancado, guardada na abertura.
  - Assert-DrawSafe depois de cada acao e um vigia (job, 200 ms), so leitura: sobreposicoes VoltzAnnotationOverlay do PID
    (FindWindowEx com titulo nulo UNIAO EnumWindows; listas divergentes = BLOCKED) com WS_EX 0x080800A8, fora do
    primeiro plano, dentro do retangulo de teste, fora do desktop virtual, afinidade 0x11; NENHUMA janela VoltzDrawInput;
    NENHUMA janela visivel "VoltzDrawToolbar"; nenhuma thread do PID com janela Voltz Win32 ativa, com foco ou com o
    mouse capturado (GetGUIThreadInfo). Falha: o PID lancado e encerrado e o smoke falha (BLOCKED).
  - Sobreposicao de teste sempre fora do desktop virtual (VOLTZ_TEST_OVERLAY_RECT, conferido contra cada monitor antes de
    abrir o app); Tela e Camera de COR (VOLTZ_TEST_PRESET_STANDINS); Assert-NoRealCapture; render so por
    GetSourceScreenshot de cena sintetica; a barra so e desenhada fora da tela (ToolbarImage, sem janela).
  - Fase C trava a thread da UI por 8 s (VOLTZ_DEBUG_FREEZE_UI): a janela do Voltz fica parada; so leitura
    (IsHungAppWindow, janelas "Ghost").
  - Config portatil guardada e restaurada (conferida por hash); Videos conferida intacta; no fim, nenhuma janela Voltz.
  Fases: A (recusa, estilo, barra), B (redesenho parcial 2560x1440 com medidas), C (UI travada, sem fantasma).
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-draw.ps1
#>
param([string]$Config = "RelWithDebInfo")

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rundir = Join-Path $root "build_x64\rundir\$Config"
$bin = Join-Path $rundir "bin\64bit"
$exe = Join-Path $bin "VoltzStudio.exe"
$ws = Join-Path $PSScriptRoot "voltz-ws.ps1"
$uia = Join-Path $PSScriptRoot "voltz-uia.ps1"
if (-not (Test-Path $exe)) { Write-Host "FALHOU executavel nao encontrado: $exe"; exit 1 }

# Acentos montados por codigo: este arquivo e ASCII (o PowerShell 5.1 le .ps1 sem BOM como ANSI)
$o = [string][char]0xF3; $a = [string][char]0xE2; $dash = [string][char]0x2014
$presetScreen = "S${o} tela / S${o} c${a}mera $dash Aplicar"
$sceneScreen = "S${o} tela"

$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.smokedraw-bak"
$voltzCfg = Join-Path $cfg "Voltz Studio"
$logDir = Join-Path $env:TEMP "voltz-smoke-draw-logs"
$wsPort = 4455

$oldBaks = @(Get-ChildItem -LiteralPath $rundir -Directory -Force -Filter "config.*bak*" -ErrorAction SilentlyContinue)
if ($oldBaks.Count -gt 0) { Write-Host "FALHOU backup anterior em $rundir ($(($oldBaks | ForEach-Object { $_.Name }) -join ', ')): restaure ou apague antes"; exit 1 }
if (@(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue).Count -gt 0) {
    Write-Host "FALHOU a porta $wsPort ja esta em uso (outro OBS/Voltz aberto?)"; exit 1
}

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
# P/Invoke de LEITURA (nada que mova o mouse, clique, digite, ative ou mude janelas); PostMessage so para WM_CLOSE
$probeSource = @'
using System; using System.Runtime.InteropServices; using System.Collections.Generic; using System.Text;
public static class VoltzProbe {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
 [StructLayout(LayoutKind.Sequential)] public struct GUITHREADINFO { public int cbSize; public int flags; public IntPtr hwndActive; public IntPtr hwndFocus; public IntPtr hwndCapture; public IntPtr hwndMenuOwner; public IntPtr hwndMoveSize; public IntPtr hwndCaret; public RECT rcCaret; }
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")] public static extern IntPtr GetWindowLongPtr(IntPtr h, int index);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool GetWindowDisplayAffinity(IntPtr h, out uint affinity);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
 [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint threadId, ref GUITHREADINFO info);
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder sb, int n);
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder sb, int n);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
 public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
 public static string ClassOf(IntPtr h) { var sb = new StringBuilder(256); GetClassName(h, sb, 256); return sb.ToString(); }
 public static string TitleOf(IntPtr h) { var sb = new StringBuilder(256); GetWindowText(h, sb, 256); return sb.ToString(); }
 public static uint OwnerOf(IntPtr h) { uint owner; GetWindowThreadProcessId(h, out owner); return owner; }
 public static List<long> OfClass(uint pid, string cls, out bool divergent) {
   var byFind = new HashSet<long>(); IntPtr h = IntPtr.Zero;
   while ((h = FindWindowEx(IntPtr.Zero, h, cls, null)) != IntPtr.Zero) { if (OwnerOf(h) == pid) byFind.Add(h.ToInt64()); }
   var byEnum = new HashSet<long>();
   EnumWindows((w, l) => { if (OwnerOf(w) == pid && ClassOf(w) == cls) byEnum.Add(w.ToInt64()); return true; }, IntPtr.Zero);
   divergent = !byFind.SetEquals(byEnum); byFind.UnionWith(byEnum); return new List<long>(byFind); }
 public static List<long> VisibleTitled(uint pid, string title) { var list = new List<long>();
   EnumWindows((w, l) => { if (OwnerOf(w) == pid && IsWindowVisible(w) && TitleOf(w) == title) list.Add(w.ToInt64()); return true; }, IntPtr.Zero);
   return list; }
 public static int GhostsOver(long[] windows) { var rects = new List<RECT>();
   foreach (long w in windows) { RECT r; if (GetWindowRect(new IntPtr(w), out r)) rects.Add(r); }
   int count = 0;
   EnumWindows((h, l) => { if (ClassOf(h) == "Ghost") { RECT g; if (GetWindowRect(h, out g)) { foreach (RECT r in rects) { if (r.L == g.L && r.T == g.T && r.R == g.R && r.B == g.B) { count++; break; } } } } return true; }, IntPtr.Zero);
   return count; }
 public static string VoltzHolder(int[] threads) {
   foreach (int t in threads) { GUITHREADINFO gi = new GUITHREADINFO(); gi.cbSize = Marshal.SizeOf(typeof(GUITHREADINFO));
     if (!GetGUIThreadInfo((uint)t, ref gi)) continue;
     IntPtr[] held = { gi.hwndActive, gi.hwndFocus, gi.hwndCapture }; string[] names = { "ativa", "foco", "captura" };
     for (int i = 0; i < 3; i++) { if (held[i] == IntPtr.Zero) continue; string c = ClassOf(held[i]);
       if (c == "VoltzAnnotationOverlay" || c == "VoltzDrawInput") return names[i] + " (" + c + ", thread " + t + ")"; } }
   return ""; }
}
'@
Add-Type $probeSource
$P = [VoltzProbe]
[void]$P::SetProcessDPIAware()

$vx = $P::GetSystemMetrics(76); $vy = $P::GetSystemMetrics(77); $vw = $P::GetSystemMetrics(78); $vh = $P::GetSystemMetrics(79)
function Crosses([int]$l, [int]$t, [int]$r, [int]$b, [int]$L2, [int]$T2, [int]$R2, [int]$B2) { return ($l -lt $R2 -and $r -gt $L2 -and $t -lt $B2 -and $b -gt $T2) }
# Retangulo de teste A DIREITA de todo o desktop virtual; recusa se cruzar o desktop ou algum monitor
function Set-TestRect([int]$w, [int]$h) {
    $script:ovX = $vx + $vw + 200; $script:ovY = $vy; $script:ovW = $w; $script:ovH = $h
    $script:overlayRect = "{0},{1},{2},{3}" -f $script:ovX, $script:ovY, $w, $h
    if (Crosses $script:ovX $script:ovY ($script:ovX + $w) ($script:ovY + $h) $vx $vy ($vx + $vw) ($vy + $vh)) { throw "retangulo de teste $($script:overlayRect) cruza o desktop virtual" }
    foreach ($s in [System.Windows.Forms.Screen]::AllScreens) {
        $sb = $s.Bounds
        if (Crosses $script:ovX $script:ovY ($script:ovX + $w) ($script:ovY + $h) $sb.Left $sb.Top $sb.Right $sb.Bottom) { throw "retangulo de teste $($script:overlayRect) sobre $($s.DeviceName)" }
    }
    Write-Host "retangulo de teste da sobreposicao: $($script:overlayRect) (desktop virtual ($vx,$vy) ${vw}x${vh})"
}

$videosDir = [Environment]::GetFolderPath('MyVideos')
# O app cria Videos\Voltz Studio ao abrir: se ela nao existia antes e ficar vazia, e apagada no fim
$voltzVideos = Join-Path $videosDir "Voltz Studio"
$hadVoltzVideos = Test-Path -LiteralPath $voltzVideos
function Videos-List { return ,@(Get-ChildItem -Force -LiteralPath $videosDir | ForEach-Object { "$($_.Name)|$($_.LastWriteTimeUtc.Ticks)|$($_.Attributes)" }) }
$videosBefore = Videos-List
function Hash-Dir([string]$d) {
    if (-not (Test-Path -LiteralPath $d)) { return "(ausente)" }
    return (Get-ChildItem -Recurse -File -Force -LiteralPath $d | Sort-Object FullName | ForEach-Object { $_.FullName.Substring($d.Length) + "|" + (Get-FileHash -Algorithm MD5 -LiteralPath $_.FullName).Hash }) -join "`n"
}
$cfgHashBefore = Hash-Dir $cfg

$checks = [ordered]@{}
function Check([string]$name, [bool]$ok) { $script:checks[$name] = $ok }

function Ws([string]$json) {
    $file = Join-Path $env:TEMP "voltz-smoke-draw-req.json"
    [IO.File]::WriteAllText($file, $json, (New-Object Text.UTF8Encoding $false))
    $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $file -Port $wsPort
    Remove-Item -Force $file -ErrorAction SilentlyContinue
    return ,@($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json })
}
function Uia([string]$name) {
    $result = & powershell -ExecutionPolicy Bypass -File $uia -ProcessId $script:proc.Id -Name $name
    Write-Host $result
    return ($LASTEXITCODE -eq 0)
}
function Vendor([string]$type, [string]$dataJson = '{}', [string]$vendorName = 'voltz-teste') {
    $r = Ws ('[{"requestType":"CallVendorRequest","requestData":{"vendorName":"' + $vendorName + '","requestType":"' + $type + '","requestData":' + $dataJson + '}}]')
    if ($r.Count -lt 1 -or -not $r[0].ok) { throw "pedido de teste $vendorName/$type falhou" }
    return $r[0].data.responseData
}
function VendorShell([string]$type, [string]$dataJson = '{}') { return (Vendor $type $dataJson 'voltz-teste-casca') }
function Shell-State { Start-Sleep -Milliseconds 400; return ((VendorShell "AnnotationState").estado | ConvertFrom-Json) }
function Feed-State { return ((Vendor "AnnotationFeedState").estado | ConvertFrom-Json) }
function Stroke-Json([string]$tool, [string]$color, [int]$width, [string]$points, [switch]$Open) {
    $req = [ordered]@{ ferr = $tool; cor = $color; esp = $width; pts = $points }
    if ($Open) { $req.aberto = $true }
    return (ConvertTo-Json $req -Compress)
}
function Pts($list) { return "[" + (($list | ForEach-Object { "[{0},{1}]" -f $_[0], $_[1] }) -join ",") + "]" }

function Kill-Launched([string]$why) {
    if ($script:proc -and -not $script:proc.HasExited) { Stop-Process -Id $script:proc.Id -Force }
    throw "BLOCKED $why - app (PID lancado) encerrado"
}

$script:overlays = @()
$script:asserts = 0
function Assert-DrawSafe([string]$where, [switch]$Quiet) {
    $procId = [uint32]$script:proc.Id
    $div = $false
    $list = $P::OfClass($procId, "VoltzAnnotationOverlay", [ref]$div)
    if ($div) { Kill-Launched "[$where] FindWindowEx e EnumWindows divergem nas sobreposicoes" }
    foreach ($id in $list) {
        $h = [IntPtr][Int64]$id
        if ($script:overlays -notcontains $id) { $script:overlays += $id }
        $ex = $P::GetWindowLongPtr($h, -20).ToInt64()
        $r = New-Object VoltzProbe+RECT; [void]$P::GetWindowRect($h, [ref]$r)
        [uint32]$aff = 99; [void]$P::GetWindowDisplayAffinity($h, [ref]$aff)
        $bad = @()
        if (($ex -band 0x080800A8) -ne 0x080800A8) { $bad += ("estilo 0x{0:X}" -f $ex) }
        if ($P::GetForegroundWindow() -eq $h) { $bad += "primeiro plano" }
        if ($r.L -lt $script:ovX -or $r.T -lt $script:ovY -or $r.R -gt ($script:ovX + $script:ovW) -or $r.B -gt ($script:ovY + $script:ovH)) { $bad += "fora do retangulo de teste ($($r.L),$($r.T),$($r.R),$($r.B))" }
        if (Crosses $r.L $r.T $r.R $r.B $vx $vy ($vx + $vw) ($vy + $vh)) { $bad += "cruza o desktop virtual" }
        if ($aff -ne 0x11) { $bad += ("afinidade 0x{0:X}" -f $aff) }
        if ($bad.Count -gt 0) { Kill-Launched ("[{0}] sobreposicao insegura hwnd={1}: {2}" -f $where, $id, ($bad -join '; ')) }
    }
    $divIn = $false
    $inputs = $P::OfClass($procId, "VoltzDrawInput", [ref]$divIn)
    if ($inputs.Count -gt 0 -or $divIn) { Kill-Launched "[$where] janela de entrada do modo desenho em teste ($($inputs -join ','))" }
    if ($P::VisibleTitled($procId, "VoltzDrawToolbar").Count -gt 0) { Kill-Launched "[$where] barra do modo desenho visivel em teste" }
    $holder = $P::VoltzHolder([int[]]@((Get-Process -Id $script:proc.Id).Threads | ForEach-Object { $_.Id }))
    if ($holder) { Kill-Launched "[$where] janela Voltz com $holder" }
    $script:asserts++
    if (-not $Quiet) { Write-Host ("assert[{0}] ok: {1} sobreposicao(oes), nenhuma janela de entrada nem barra" -f $where, $list.Count) }
}

# Vigia (job = outro processo): a mesma conferencia a cada 200 ms; falha = encerra ESSE PID e escreve BLOCKED
$watchdog = {
    param([int]$TargetPid, [int]$OvX, [int]$OvY, [int]$OvW, [int]$OvH, [string]$ProbeSource, [string]$StopFile, [string]$LogFile)
    Add-Type $ProbeSource
    $W = [VoltzProbe]
    [void]$W::SetProcessDPIAware()
    $vx = $W::GetSystemMetrics(76); $vy = $W::GetSystemMetrics(77); $vw = $W::GetSystemMetrics(78); $vh = $W::GetSystemMetrics(79)
    $rounds = 0
    while ($true) {
        if (Test-Path -LiteralPath $StopFile) { break }
        $p = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
        if (-not $p -or $p.HasExited) { break }
        $bad = @()
        $div = $false
        foreach ($id in $W::OfClass([uint32]$TargetPid, "VoltzAnnotationOverlay", [ref]$div)) {
            $h = [IntPtr][Int64]$id
            $ex = $W::GetWindowLongPtr($h, -20).ToInt64()
            $r = New-Object VoltzProbe+RECT; [void]$W::GetWindowRect($h, [ref]$r)
            [uint32]$aff = 99; [void]$W::GetWindowDisplayAffinity($h, [ref]$aff)
            if (($ex -band 0x080800A8) -ne 0x080800A8) { $bad += ("estilo 0x{0:X}" -f $ex) }
            if ($W::GetForegroundWindow() -eq $h) { $bad += "primeiro plano" }
            if ($r.L -lt $OvX -or $r.T -lt $OvY -or $r.R -gt ($OvX + $OvW) -or $r.B -gt ($OvY + $OvH)) { $bad += "fora do retangulo de teste" }
            if ($r.L -lt ($vx + $vw) -and $r.R -gt $vx -and $r.T -lt ($vy + $vh) -and $r.B -gt $vy) { $bad += "cruza o desktop virtual" }
            if ($aff -ne 0x11) { $bad += ("afinidade 0x{0:X}" -f $aff) }
        }
        $divIn = $false
        if ($W::OfClass([uint32]$TargetPid, "VoltzDrawInput", [ref]$divIn).Count -gt 0) { $bad += "janela de entrada do modo desenho" }
        if ($W::VisibleTitled([uint32]$TargetPid, "VoltzDrawToolbar").Count -gt 0) { $bad += "barra visivel" }
        $holder = $W::VoltzHolder([int[]]@($p.Threads | ForEach-Object { $_.Id }))
        if ($holder) { $bad += "janela Voltz com $holder" }
        if ($bad.Count -gt 0) {
            Stop-Process -Id $TargetPid -Force
            Add-Content -LiteralPath $LogFile ("BLOCKED vigia: " + ($bad -join '; '))
            return
        }
        $rounds++
        Start-Sleep -Milliseconds 200
    }
    Add-Content -LiteralPath $LogFile ("vigia: {0} conferencias sem problema" -f $rounds)
}

function Assert-NoRealCapture {
    $inputs = @((Ws '[{"requestType":"GetInputList"}]')[0].data.inputs)
    $real = @($inputs | Where-Object { $_.inputKind -match 'monitor_capture|window_capture|game_capture|dshow_input' })
    if ($real.Count -gt 0) { throw ("captura real presente - abortando: " + (($real | ForEach-Object { $_.inputName }) -join ', ')) }
}
function Scene-Image([string]$scene) {
    Assert-NoRealCapture
    $r = Ws ('[{"requestType":"GetSourceScreenshot","requestData":{"sourceName":' + (ConvertTo-Json $scene) + ',"imageFormat":"png","imageWidth":640,"imageHeight":360}}]')
    if ($r.Count -lt 1 -or -not $r[0].ok) { throw "GetSourceScreenshot falhou para '$scene'" }
    $bytes = [Convert]::FromBase64String(($r[0].data.imageData -replace '^data:image/png;base64,', ''))
    return [System.Drawing.Bitmap]::FromStream((New-Object IO.MemoryStream (, $bytes)))
}
function Near($c, [int]$r, [int]$g, [int]$b, [int]$tol = 24) {
    return ([Math]::Abs($c.R - $r) -le $tol) -and ([Math]::Abs($c.G - $g) -le $tol) -and ([Math]::Abs($c.B - $b) -le $tol)
}

function New-CleanConfig {
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
    New-Item -ItemType Directory -Force $wsDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":' + $wsPort + '}')
    [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[BasicWindow]`r`nHideOBSWindowsFromCapture=false`r`n[Voltz]`r`nNextProjectName=Smoke desenho`r`n", (New-Object Text.UTF8Encoding $false))
}
function Set-TestEnv([bool]$freeze) {
    $env:VOLTZ_TEST_LIVE = "1"
    $env:VOLTZ_TEST_PRESET_STANDINS = "1"
    $env:VOLTZ_TEST_OVERLAY_RECT = $script:overlayRect
    if ($freeze) { $env:VOLTZ_DEBUG_FREEZE_UI = "1" } else { Remove-Item Env:VOLTZ_DEBUG_FREEZE_UI -ErrorAction SilentlyContinue }
    Remove-Item Env:VOLTZ_TEST_ANNOT_BURNED, Env:VOLTZ_TEST_SCREEN_LAYERS -ErrorAction SilentlyContinue
}

function Start-App([string]$tag) {
    if (@(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue).Count -gt 0) { throw "porta $wsPort ocupada antes de abrir o app ($tag)" }
    $script:proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
    $script:wdLog = Join-Path $logDir "vigia-$tag.txt"
    $script:wdStop = Join-Path $logDir "vigia-$tag.stop"
    $script:job = Start-Job -ScriptBlock $watchdog -ArgumentList $script:proc.Id, $script:ovX, $script:ovY, $script:ovW, $script:ovH, $probeSource, $script:wdStop, $script:wdLog
    Start-Sleep -Seconds 15
    if ($script:proc.HasExited) { return $false }
    $script:shell = [IntPtr]::Zero
    for ($i = 0; $i -lt 30 -and $script:shell -eq [IntPtr]::Zero; $i++) {
        $script:proc.Refresh()
        $h = $script:proc.MainWindowHandle
        if ($h -ne [IntPtr]::Zero -and $P::OwnerOf($h) -eq $script:proc.Id -and $P::ClassOf($h) -notmatch '^Voltz(AnnotationOverlay|DrawInput)$') { $script:shell = $h } else { Start-Sleep -Milliseconds 500 }
    }
    $owners = @(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue | ForEach-Object { [int]$_.OwningProcess })
    if (-not ($owners -contains $script:proc.Id)) { Kill-Launched "a porta $wsPort nao e do app lancado ($($owners -join ','))" }
    Write-Host "[$tag] PID lancado $($script:proc.Id); casca hwnd=$($script:shell.ToInt64()) classe '$($P::ClassOf($script:shell))'"
    Assert-DrawSafe "$tag abertura"
    return ($script:shell -ne [IntPtr]::Zero)
}

function Stop-Watchdog([string]$tag) {
    if (-not $script:job) { return }
    New-Item -ItemType File -Force $script:wdStop | Out-Null
    [void](Wait-Job $script:job -Timeout 20)
    Remove-Job $script:job -Force
    $script:job = $null
    Remove-Item -LiteralPath $script:wdStop -ErrorAction SilentlyContinue
    $text = if (Test-Path -LiteralPath $script:wdLog) { (Get-Content -LiteralPath $script:wdLog) -join ' ' } else { "(sem registro)" }
    Write-Host "[$tag] $text"
    Check "${tag}: vigia sem problema ($text)" ($text -like "vigia: * conferencias sem problema*" -and $text -notlike "*BLOCKED*")
}

$script:forced = 0
function Close-App([string]$tag) {
    $clean = $false
    if ($script:proc -and -not $script:proc.HasExited) {
        $procId = $script:proc.Id
        $shellOk = ($script:shell -ne [IntPtr]::Zero) -and $P::IsWindow($script:shell) -and ($P::OwnerOf($script:shell) -eq $procId) -and ($P::ClassOf($script:shell) -notmatch '^Voltz(AnnotationOverlay|DrawInput)$')
        if ($shellOk) { [void]$P::PostMessage($script:shell, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) } else { [void]$script:proc.CloseMainWindow() }
        $clean = $script:proc.WaitForExit(30000)
        if (-not $clean) { $script:forced++; Write-Host "[$tag] FECHAMENTO FORCADO (30 s)"; Stop-Process -Id $procId -Force; [void]$script:proc.WaitForExit(10000) }
        $div = $false
        $left = $P::OfClass([uint32]$procId, "VoltzAnnotationOverlay", [ref]$div).Count + $P::OfClass([uint32]$procId, "VoltzDrawInput", [ref]$div).Count
        $orphans = @($script:overlays | Where-Object { $P::IsWindow([IntPtr][Int64]$_) }).Count
        Check "${tag}: nenhuma janela Voltz depois de fechar (do PID $left, orfas $orphans)" ($left -eq 0 -and $orphans -eq 0)
    }
    Stop-Watchdog $tag
    $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
    if ($log) { Copy-Item $log.FullName (Join-Path $logDir "$tag.txt") -Force }
    if ($clean) { Check "${tag}: log - Number of memory leaks: 0" (LogHas $tag "Number of memory leaks: 0") }
    return $clean
}
function LogHas([string]$tag, [string]$text) {
    $file = Join-Path $logDir "$tag.txt"
    return [bool]((Test-Path $file) -and (Select-String -LiteralPath $file -SimpleMatch $text -Encoding UTF8 -Quiet))
}
function LogCount([string]$tag, [string]$text) {
    $file = Join-Path $logDir "$tag.txt"
    if (-not (Test-Path $file)) { return 0 }
    return @(Select-String -LiteralPath $file -SimpleMatch $text -Encoding UTF8).Count
}
function Apply-Preset([string]$tag) {
    Check "${tag}: preset 'So tela / So camera' (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    [void](Ws ('[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":' + (ConvertTo-Json $sceneScreen) + '}}]'))
    Assert-DrawSafe "$tag preset"
}

# Passo de limpeza isolado: a falha de um nao impede os outros (a config portatil e restaurada de qualquer jeito)
function Cleanup([string]$what, [scriptblock]$step) {
    try { & $step } catch { Write-Host "FALHOU limpeza ($what): $($_.Exception.Message)"; Check "limpeza: $what ($($_.Exception.Message))" $false }
}
# Apaga a config de teste e devolve o backup SO se este script guardou a do usuario (ou ela nao existia)
function Restore-Config {
    if (-not $script:cfgOwned) { Write-Host "config portatil nao foi guardada por este script: nada apagado nem restaurado"; return }
    Start-Sleep -Seconds 5
    if (Test-Path $cfg) {
        try { Remove-Item -Recurse -Force $cfg } catch { Start-Sleep -Seconds 5; Remove-Item -Recurse -Force $cfg }
    }
    if (Test-Path $bak) {
        if (Test-Path $cfg) { throw "a config de teste nao saiu; backup mantido em $bak" }
        Move-Item $bak $cfg
    }
}

$script:cfgOwned = $false
$proc = $null
$script:job = $null
try {
    # Backup dentro do try: qualquer falha daqui em diante ainda restaura a config no finally
    # Backup anterior (execucao abortada): o Move-Item poria a config DENTRO dele; cfgOwned segue falso (nada restaurado)
    if (Test-Path -LiteralPath $bak) { throw "backup anterior em ${bak}: resolva antes" }
    if (Test-Path $cfg) { Move-Item $cfg $bak }
    $script:cfgOwned = $true
    if (Test-Path $logDir) { Remove-Item -Recurse -Force $logDir }
    New-Item -ItemType Directory -Force $logDir | Out-Null

    # ---------------- Fase A: recusa, estilo, barra ----------------
    Set-TestRect 640 360
    New-CleanConfig
    Set-TestEnv $false
    Check "A: app aberto" (Start-App "A")
    Apply-Preset "A"
    $st = Shell-State
    Check "A: modo desenho desligado ao abrir" (-not $st.modo_desenho)
    for ($i = 1; $i -le 3; $i++) {
        $r = Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.Draw"}}]'
        Check "A: TriggerHotkeyByName VoltzStudio.Draw #$i aceito" ($r.Count -eq 1 -and $r[0].ok)
        Start-Sleep -Milliseconds 500
        Assert-DrawSafe "A atalho $i"
        $st = Shell-State
        Check "A: recusado #$i (modo_desenho $($st.modo_desenho), recusa '$($st.desenho_recusa)')" ((-not $st.modo_desenho) -and $st.desenho_recusa -eq "teste")
    }
    $style = '{"ferr":"seta","cor":"#4DA3FF","esp":8,"somem_apos_s":2,"tam":48,"fonte":"dm_sans_negrito"}'
    [void](VendorShell "SetAnnotationStyle" (ConvertTo-Json @{ estilo = $style } -Compress))
    $st = Shell-State
    Check "A: estilo aplicado ($($st.estilo))" ($st.estilo -eq $style)
    [void](VendorShell "InjectStroke" '{"pts":"[[400,400],[1200,800]]"}')
    $st = Shell-State
    Assert-DrawSafe "A traco"
    $last = if ($st.ultimo) { $st.ultimo | ConvertFrom-Json } else { $null }
    Check "A: traco sem ferr/cor/esp usa o estilo (seta #4DA3FF 8)" ($null -ne $last -and $last.ferr -eq "seta" -and $last.cor -eq "#4DA3FF" -and $last.esp -eq 8)
    Start-Sleep -Seconds 3
    $st = Shell-State
    Check "A: somem apos 2 s (na tela: $(@($st.visiveis).Count))" (@($st.visiveis).Count -eq 0)
    $bar = VendorShell "ToolbarImage"
    if ($bar.png) { [IO.File]::WriteAllBytes((Join-Path $logDir "barra-A.png"), [Convert]::FromBase64String($bar.png)) }
    Check "A: barra desenhada fora da tela, sem janela nativa ($($bar.largura)x$($bar.altura), nativa $($bar.janela_nativa))" ($bar.largura -ge 400 -and $bar.altura -ge 36 -and -not $bar.janela_nativa)
    Assert-DrawSafe "A barra"
    Check "A: encerrou limpo" (Close-App "A")
    Check "A: log - desligado ao abrir" (LogHas "A" "[voltz] modo desenho: desligado ao abrir")
    Check "A: log - 3 recusas (teste)" ((LogCount "A" "[voltz] modo desenho: recusado (teste)") -ge 3)
    Check "A: log - o modo desenho nunca ligou" (-not (LogHas "A" "[voltz] modo desenho: ligado"))
    Check "A: log - janelas fantasma desligadas" (LogHas "A" "[voltz] janelas fantasma do Windows desligadas no processo")
    $ini = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $voltzCfg "user.ini")
    Check "A: user.ini com o AnnotationStyle da barra" ($ini -match [regex]::Escape("AnnotationStyle=$style"))

    # ---------------- Fase B: redesenho parcial (2560x1440, escala 1) e medidas ----------------
    Set-TestRect 2560 1440
    New-CleanConfig
    Set-TestEnv $false
    Check "B: app aberto" (Start-App "B")
    Apply-Preset "B"
    $pts = @()
    for ($i = 0; $i -le 601; $i++) { $pts += , @([int](200 + $i * 3.6), [int](700 + 200 * [Math]::Sin($i / 40.0))) }
    [void](VendorShell "InjectStroke" (Stroke-Json "caneta" "#D4F53C" 8 (Pts $pts[0..1]) -Open))
    Start-Sleep -Milliseconds 300
    for ($batch = 0; $batch -lt 3; $batch++) {
        $reqs = @()
        for ($k = 0; $k -lt 20; $k++) {
            $j = $batch * 20 + $k
            $reqs += @{ requestType = "CallVendorRequest"; requestData = @{ vendorName = "voltz-teste-casca"; requestType = "ExtendStroke"; requestData = @{ pts = (Pts $pts[(2 + 10 * $j)..(11 + 10 * $j)]) } } }
        }
        [void](Ws (ConvertTo-Json -InputObject $reqs -Depth 6 -Compress))
        Start-Sleep -Milliseconds 300
        Assert-DrawSafe "B lote $batch"
    }
    [void](VendorShell "EndStroke")
    $probe = "[[{0},{1}],[{2},{3}]]" -f $pts[0][0], $pts[0][1], $pts[300][0], $pts[300][1]
    [void](VendorShell "ProbeOverlay" (ConvertTo-Json @{ pts = $probe } -Compress))
    $st = Shell-State
    $feed = Feed-State
    $ov = $st.sobreposicao
    Write-Host ("B: sobreposicao 2560x1440: {0} parciais (media {1} us, max {2} us), {3} completa(s) (max {4} us)" -f $ov.parciais, $ov.parcial_us_media, $ov.parcial_us_max, $ov.completas, $ov.completa_us_max)
    Write-Host ("B: fonte: {0} parciais, {1} completas, ultimo {2} us, max {3} us" -f $feed.parciais, $feed.completas, $feed.raster_us_ultimo, $feed.raster_us_max)
    Check "B: sobreposicao redesenhou so o retangulo sujo ($($ov.parciais) parciais, $($ov.completas) completas)" ($ov.parciais -ge 60 -and $ov.completas -le 2)
    Check "B: parcial mais rapido que completo (media $($ov.parcial_us_media) us, completo max $($ov.completa_us_max) us)" ($ov.parcial_us_media -lt $ov.completa_us_max)
    Check "B: parcial abaixo de 100 ms (max $($ov.parcial_us_max) us)" ($ov.parcial_us_max -lt 100000)
    Check "B: fonte redesenhou parcialmente ($($feed.parciais) parciais, $($feed.completas) completas)" ($feed.parciais -ge 1 -and $feed.completas -ge 1)
    Check "B: buffer da sobreposicao com o traco ($($st.amostras -join ','))" (@($st.amostras).Count -eq 2 -and $st.amostras[0] -eq "#FFD4F53C" -and $st.amostras[1] -eq "#FFD4F53C")
    $img = Scene-Image $sceneScreen
    $c = $img.GetPixel([int]($pts[300][0] / 4), [int]($pts[300][1] / 4)); $img.Dispose()
    Check "B: programa com o traco ($c)" (Near $c 212 245 60 40)
    [void](VendorShell "UndoStroke")
    [void](VendorShell "ProbeOverlay" (ConvertTo-Json @{ pts = $probe } -Compress))
    $st = Shell-State
    Assert-DrawSafe "B desfazer"
    Check "B: desfazer limpa a sobreposicao ($($st.amostras -join ','), visivel $($st.sobreposicao.visivel))" ((-not $st.sobreposicao.visivel) -and $st.amostras[0] -eq "#00000000" -and $st.amostras[1] -eq "#00000000")
    Check "B: encerrou limpo" (Close-App "B")

    # ---------------- Fase C: UI travada 8 s, sem janela fantasma ----------------
    Set-TestRect 640 360
    New-CleanConfig
    Set-TestEnv $true
    Check "C: app aberto" (Start-App "C")
    Apply-Preset "C"
    [void](VendorShell "InjectStroke" (Stroke-Json "linha" "#D4F53C" 16 "[[640,720],[1920,720]]"))
    $st = Shell-State
    Assert-DrawSafe "C traco"
    Check "C: sobreposicao visivel (fora da area de trabalho)" ($st.sobreposicao.visivel)
    $watch = [long[]]@($script:shell.ToInt64(), [long]$st.sobreposicao.hwnd)
    $r = Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.DebugFreezeUi"}}]'
    Check "C: atalho de depuracao aceito" ($r.Count -eq 1 -and $r[0].ok)
    $hung = $false; $ghosts = 0
    for ($i = 0; $i -lt 44; $i++) {
        Start-Sleep -Milliseconds 250
        if ($P::IsHungAppWindow($script:shell)) { $hung = $true }
        $ghosts = [Math]::Max($ghosts, $P::GhostsOver($watch))
        Assert-DrawSafe "C travada $i" -Quiet
    }
    Check "C: a UI travou de verdade (IsHungAppWindow)" $hung
    Check "C: nenhuma janela fantasma sobre a casca ou a sobreposicao ($ghosts)" ($ghosts -eq 0)
    Start-Sleep -Seconds 2
    $st = Shell-State
    Check "C: a UI voltou ($(@($st.visiveis).Count) traco na tela)" (@($st.visiveis).Count -eq 1)
    Check "C: encerrou limpo" (Close-App "C")
    Check "C: log - atalho de depuracao ligado" (LogHas "C" "[voltz] depuracao (VOLTZ_DEBUG_FREEZE_UI): atalho VoltzStudio.DebugFreezeUi ligado")
    Check "C: log - UI travada 8000 ms e de volta" ((LogHas "C" "[voltz] depuracao: thread da UI travada por 8000 ms") -and (LogHas "C" "[voltz] depuracao: thread da UI de volta"))
} catch {
    Check "sem excecao ($($_.Exception.Message))" $false
} finally {
    Cleanup "fechar o app" { if ($script:proc -and -not $script:proc.HasExited) { [void](Close-App "final") } }
    Cleanup "encerrar o PID lancado" { if ($script:proc -and -not $script:proc.HasExited) { $script:forced++; Stop-Process -Id $script:proc.Id -Force; [void]$script:proc.WaitForExit(10000) } }
    Cleanup "parar o vigia" { if ($script:job) { Stop-Watchdog "final" } }
    Cleanup "variaveis de teste" { Remove-Item Env:VOLTZ_TEST_LIVE, Env:VOLTZ_TEST_PRESET_STANDINS, Env:VOLTZ_TEST_OVERLAY_RECT, Env:VOLTZ_DEBUG_FREEZE_UI -ErrorAction SilentlyContinue }
    Cleanup "restaurar a config portatil" { Restore-Config }
    Cleanup "Videos\Voltz Studio" {
        if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) {
            Remove-Item -Force -LiteralPath $voltzVideos
            Write-Host "apagado (vazio): $voltzVideos"
        }
    }
}

$orphans = @($script:overlays | Where-Object { $P::IsWindow([IntPtr][Int64]$_) })
Check "nenhuma sobreposicao orfa ($($script:overlays.Count) vista(s))" ($script:overlays.Count -ge 1 -and $orphans.Count -eq 0)
Check "seguranca: $($script:asserts) Assert-DrawSafe, $($script:forced) fechamento(s) forcado(s)" ($script:asserts -gt 0 -and $script:forced -eq 0)
$videosAfter = Videos-List
Check "Videos intacta (antes $($videosBefore.Count), depois $($videosAfter.Count))" (-not (Compare-Object $videosBefore $videosAfter))
$leftBaks = @(Get-ChildItem -LiteralPath $rundir -Directory -Force -Filter "config.*bak*" -ErrorAction SilentlyContinue)
Check "config portatil restaurada igual e sem sobra de backup" (-not (Test-Path $bak) -and $leftBaks.Count -eq 0 -and ($cfgHashBefore -eq (Hash-Dir $cfg)))
$failed = 0
foreach ($k in $checks.Keys) {
    if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
}
Write-Host "logs: $logDir"
if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE DESENHO OK"
