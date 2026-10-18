<#
  Teste de integracao do Plano 4D (texto das anotacoes, Ajustes > Anotacoes e Ajustes > Atalhos). O modo desenho NUNCA
  liga aqui e nada e digitado: o texto entra so pelos pedidos de teste da casca (InjectText, TypeText, CommitText,
  CancelText), que chamam o AnnotationController direto.
  Seguranca (o PC e do usuario e esta em uso):
  - VOLTZ_TEST_LIVE=1 recusa o modo desenho duas vezes (DrawModeController e DrawInputWindow). O unico gatilho usado e
    TriggerHotkeyByName "VoltzStudio.Draw", so para provar a recusa (inclusive depois de limpar e restaurar o atalho).
  - Nenhuma entrada e injetada; nada e posto em primeiro plano; nenhum campo de texto da casca recebe foco (o campo de
    atalho nunca e acionado: so os botoes "Limpar atalho" e "Restaurar padrao", por UI Automation/Invoke). O unico
    P/Invoke que escreve e PostMessage(WM_CLOSE) na janela da casca do PID lancado, guardada na abertura.
  - Assert-DrawSafe depois de cada acao e um vigia (job, 200 ms), so leitura, iguais aos do voltz-smoke-draw.ps1:
    sobreposicoes do PID dentro do retangulo de teste (fora do desktop virtual), transparentes a cliques, fora do primeiro
    plano e da captura; NENHUMA janela VoltzDrawInput; NENHUMA barra "VoltzDrawToolbar" visivel; nenhuma thread do PID
    com janela Voltz Win32 ativa, com foco ou com o mouse capturado. Falha: o PID lancado e encerrado (BLOCKED).
  - Tela e Camera de COR (VOLTZ_TEST_PRESET_STANDINS); Assert-NoRealCapture; render so por GetSourceScreenshot de cena
    sintetica; a barra so e desenhada fora da tela (ToolbarImage, sem janela); so metadados da gravacao (nenhum quadro).
  - Config portatil guardada e restaurada (conferida por hash); -OutDir apagado no fim; Videos conferida intacta.
  Fases: A (texto ao vivo na sobreposicao e no programa, gravacao com o texto no anotacoes.jsonl, cancelar, privacidade
  do log), B (Ajustes: atalhos limpar/restaurar no perfil e ao vivo, secoes presentes, barra com o texto e o seletor).
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-text.ps1 [-OutDir <pasta vazia>] [-Keep]
#>
param(
    [string]$Config = "RelWithDebInfo",
    [string]$OutDir = (Join-Path $env:TEMP "voltz-smoke-text"),
    [switch]$Keep
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rundir = Join-Path $root "build_x64\rundir\$Config"
$bin = Join-Path $rundir "bin\64bit"
$exe = Join-Path $bin "VoltzStudio.exe"
$ws = Join-Path $PSScriptRoot "voltz-ws.ps1"
$uia = Join-Path $PSScriptRoot "voltz-uia.ps1"
if (-not (Test-Path $exe)) { Write-Host "FALHOU executavel nao encontrado: $exe"; exit 1 }

# Acentos montados por codigo: este arquivo e ASCII (o PowerShell 5.1 le .ps1 sem BOM como ANSI)
$o = [string][char]0xF3; $a = [string][char]0xE2; $dash = [string][char]0x2014; $ccedil = [string][char]0xE7
$atilde = [string][char]0xE3; $eacute = [string][char]0xE9; $otilde = [string][char]0xF5
$presetScreen = "S${o} tela / S${o} c${a}mera $dash Aplicar"
$sceneScreen = "S${o} tela"
$drawTitle = "Ligar/desligar modo desenho"
$clearDraw = "Limpar atalho $dash $drawTitle"
$resetDraw = "Restaurar padr${atilde}o $dash $drawTitle"
$editDraw = "Atalho $dash $drawTitle"
$textSizeName = "Tamanho do texto"
$previewName = "Pr${eacute}via das anota${ccedil}${otilde}es"
$accentText = "A${ccedil}${atilde}o 4D`nlinha 2"

$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.smoketext-bak"
$voltzCfg = Join-Path $cfg "Voltz Studio"
$logDir = Join-Path $env:TEMP "voltz-smoke-text-logs"
$wsPort = 4455

if ((Test-Path -LiteralPath $OutDir) -and @(Get-ChildItem -Force -LiteralPath $OutDir).Count -gt 0) {
    Write-Host "FALHOU -OutDir ja existe e nao esta vazio: $OutDir"; exit 1
}
$oldBaks = @(Get-ChildItem -LiteralPath $rundir -Directory -Force -Filter "config.*bak*" -ErrorAction SilentlyContinue)
if ($oldBaks.Count -gt 0) { Write-Host "FALHOU backup anterior em $rundir ($(($oldBaks | ForEach-Object { $_.Name }) -join ', ')): restaure ou apague antes"; exit 1 }
if (@(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue).Count -gt 0) {
    Write-Host "FALHOU a porta $wsPort ja esta em uso (outro OBS/Voltz aberto?)"; exit 1
}

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
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
    $file = Join-Path $env:TEMP "voltz-smoke-text-req.json"
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
# So LEITURA: existe um elemento com esse nome numa janela do PID lancado? (nada e acionado nem focado)
function UiaExists([string]$name) {
    $AE = [System.Windows.Automation.AutomationElement]
    $pidCond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $script:proc.Id)
    $nameCond = New-Object System.Windows.Automation.PropertyCondition($AE::NameProperty, $name)
    foreach ($win in @($AE::RootElement.FindAll([System.Windows.Automation.TreeScope]::Children, $pidCond))) {
        if ($null -ne $win.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $nameCond)) { return $true }
    }
    return $false
}
function Vendor([string]$type, [string]$dataJson = '{}', [string]$vendorName = 'voltz-teste') {
    $r = Ws ('[{"requestType":"CallVendorRequest","requestData":{"vendorName":"' + $vendorName + '","requestType":"' + $type + '","requestData":' + $dataJson + '}}]')
    if ($r.Count -lt 1 -or -not $r[0].ok) { throw "pedido de teste $vendorName/$type falhou" }
    return $r[0].data.responseData
}
function VendorShell([string]$type, [string]$dataJson = '{}') { return (Vendor $type $dataJson 'voltz-teste-casca') }
function Shell-State { Start-Sleep -Milliseconds 400; return ((VendorShell "AnnotationState").estado | ConvertFrom-Json) }
function Feed-State { return ((Vendor "AnnotationFeedState").estado | ConvertFrom-Json) }
function Hotkeys { Start-Sleep -Milliseconds 300; return ((VendorShell "HotkeyState").atalhos | ConvertFrom-Json) }
function BindingsText($hotkeys, [string]$name) { return (ConvertTo-Json -InputObject $hotkeys.$name -Compress -Depth 5) }
function Text-Json([string]$points, [string]$text, [switch]$Open) {
    $req = [ordered]@{ pts = $points; texto = $text }
    if ($Open) { $req.aberto = $true }
    return (ConvertTo-Json $req -Compress)
}
# Pontos "[[x,y],...]" de uma grade (passo dx, dy) no retangulo [x0,x1] x [y0,y1]
function Grid([int]$x0, [int]$x1, [int]$dx, [int]$y0, [int]$y1, [int]$dy) {
    $pts = @()
    for ($y = $y0; $y -le $y1; $y += $dy) { for ($x = $x0; $x -le $x1; $x += $dx) { $pts += "[$x,$y]" } }
    return "[" + ($pts -join ",") + "]"
}

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
# Pixels quase lima na regiao [x0,x1) x [y0,y1) da imagem
function Lime-Count($img, [int]$x0, [int]$x1, [int]$y0, [int]$y1) {
    $n = 0
    for ($y = $y0; $y -lt $y1; $y++) { for ($x = $x0; $x -lt $x1; $x++) { if (Near $img.GetPixel($x, $y) 212 245 60 40) { $n++ } } }
    return $n
}

function New-CleanConfig([string]$voltzLines) {
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
    New-Item -ItemType Directory -Force $wsDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":' + $wsPort + '}')
    [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[BasicWindow]`r`nHideOBSWindowsFromCapture=false`r`n[Voltz]`r`n" + $voltzLines, (New-Object Text.UTF8Encoding $false))
}
function Set-TestEnv {
    $env:VOLTZ_TEST_LIVE = "1"
    $env:VOLTZ_TEST_PRESET_STANDINS = "1"
    $env:VOLTZ_TEST_OVERLAY_RECT = $script:overlayRect
    Remove-Item Env:VOLTZ_DEBUG_FREEZE_UI, Env:VOLTZ_TEST_ANNOT_BURNED, Env:VOLTZ_TEST_SCREEN_LAYERS -ErrorAction SilentlyContinue
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
function Apply-Preset([string]$tag) {
    Check "${tag}: preset 'So tela / So camera' (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    [void](Ws ('[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":' + (ConvertTo-Json $sceneScreen) + '}}]'))
    Assert-DrawSafe "$tag preset"
}
function Assert-Refused([string]$where) {
    $r = Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.Draw"}}]'
    Start-Sleep -Milliseconds 500
    Assert-DrawSafe "$where atalho"
    $st = Shell-State
    Check "${where}: TriggerHotkeyByName VoltzStudio.Draw recusado (modo $($st.modo_desenho), recusa '$($st.desenho_recusa)')" ($r.Count -eq 1 -and $r[0].ok -and (-not $st.modo_desenho) -and $st.desenho_recusa -eq "teste")
}
# Gravacoes em -OutDir e audio global mutado
function Prepare-Recording {
    $r = Ws ('[{"requestType":"SetProfileParameter","requestData":{"parameterCategory":"SimpleOutput","parameterName":"FilePath","parameterValue":' + (ConvertTo-Json $OutDir) + '}},{"requestType":"GetSpecialInputs"}]')
    $special = $r[1].data
    $mutes = @()
    foreach ($n in @($special.desktop1, $special.desktop2, $special.mic1, $special.mic2, $special.mic3, $special.mic4)) {
        if ($n) { $mutes += @{ requestType = "SetInputMute"; requestData = @{ inputName = $n; inputMuted = $true } } }
    }
    if ($mutes.Count -gt 0) { [void](Ws (ConvertTo-Json -InputObject $mutes -Depth 5 -Compress)) }
}
function Read-Jsonl([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return ,@() }
    return ,@(Get-Content -LiteralPath $path -Encoding UTF8 | Where-Object { $_ } | ForEach-Object { $_ | ConvertFrom-Json })
}
function Has([object]$obj, [string]$name) { return ($null -ne $obj) -and ($obj.PSObject.Properties.Name -contains $name) }
# basic.ini do perfil (o unico perfil da config limpa)
function Profile-Ini {
    $ini = Get-ChildItem -LiteralPath (Join-Path $voltzCfg "basic\profiles") -Recurse -Filter basic.ini -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $ini) { return "" }
    return [IO.File]::ReadAllText($ini.FullName, [Text.Encoding]::UTF8)
}

# O log contem o texto? Bytes lidos como UTF-8 e como Windows-1252 (log gravado em ANSI); $false sem o arquivo
function LogHasAnyEncoding([string]$tag, [string]$text) {
    $file = Join-Path $logDir "$tag.txt"
    if (-not (Test-Path -LiteralPath $file)) { return $false }
    $bytes = [IO.File]::ReadAllBytes($file)
    return ([Text.Encoding]::UTF8.GetString($bytes).Contains($text) -or [Text.Encoding]::GetEncoding(1252).GetString($bytes).Contains($text))
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
$textStyle = '{"ferr":"texto","cor":"#D4F53C","esp":4,"somem_apos_s":0,"tam":96,"fonte":"dm_sans_negrito"}'
try {
    # Backup dentro do try: qualquer falha daqui em diante ainda restaura a config no finally
    # Backup anterior (execucao abortada): o Move-Item poria a config DENTRO dele; cfgOwned segue falso (nada restaurado)
    if (Test-Path -LiteralPath $bak) { throw "backup anterior em ${bak}: resolva antes" }
    if (Test-Path $cfg) { Move-Item $cfg $bak }
    $script:cfgOwned = $true
    if (Test-Path $logDir) { Remove-Item -Recurse -Force $logDir }
    New-Item -ItemType Directory -Force $logDir | Out-Null
    New-Item -ItemType Directory -Force $OutDir | Out-Null

    # ---------------- Fase A: texto ao vivo (sobreposicao 2560x1440, escala 1) e gravacao ----------------
    Set-TestRect 2560 1440
    New-CleanConfig "NextProjectName=Smoke texto`r`n"
    Set-TestEnv
    Check "A: app aberto" (Start-App "A")
    Apply-Preset "A"
    Prepare-Recording
    Assert-Refused "A"
    [void](VendorShell "SetAnnotationStyle" (ConvertTo-Json @{ estilo = $textStyle } -Compress))
    $st = Shell-State
    Check "A: estilo do texto aplicado ($($st.estilo))" ($st.estilo -eq $textStyle)

    [void](VendorShell "InjectText" (Text-Json "[[400,300]]" "Voltz" -Open))
    $st = Shell-State
    Assert-DrawSafe "A texto aberto"
    $last = if ($st.ultimo) { $st.ultimo | ConvertFrom-Json } else { $null }
    Check "A: texto em andamento (digitando $($st.digitando), cursor $(@($st.cursor_texto) -join ','))" ($st.digitando -and @($st.cursor_texto).Count -eq 4)
    Check "A: traco de texto com o estilo (ferr texto, tam 96, negrito, ancora 400,300)" ($null -ne $last -and $last.ferr -eq "texto" -and $last.texto -eq "Voltz" -and $last.tam -eq 96 -and $last.fonte -eq "dm_sans_negrito" -and $last.pts[0][0] -eq 400 -and $last.pts[0][1] -eq 300)
    $caret = @($st.cursor_texto)
    $caretMid = [int](($caret[1] + $caret[3]) / 2)
    [void](VendorShell "ProbeOverlay" (ConvertTo-Json @{ pts = (Grid 400 760 6 310 400 6) } -Compress))
    $st = Shell-State
    $inked = @($st.amostras | Where-Object { $_ -eq "#FFD4F53C" }).Count
    Check "A: sobreposicao com as letras ($inked amostras lima de $(@($st.amostras).Count))" ($inked -ge 20)
    [void](VendorShell "ProbeOverlay" (ConvertTo-Json @{ pts = ("[[{0},{1}],[1500,700],[2000,1200]]" -f $caret[0], $caretMid) } -Compress))
    $st = Shell-State
    Check "A: cursor de digitacao so na sobreposicao ($($st.amostras -join ','))" ($st.amostras[0] -eq "#FFD4F53C" -and $st.amostras[1] -eq "#00000000" -and $st.amostras[2] -eq "#00000000")

    $partialsBefore = [int]$st.sobreposicao.parciais
    [void](VendorShell "TypeText" '{"texto":"Voltz Studio"}')
    $st = Shell-State
    Assert-DrawSafe "A digitando"
    $last = $st.ultimo | ConvertFrom-Json
    Check "A: TypeText troca o texto ($($last.texto)); redesenho parcial ($partialsBefore -> $($st.sobreposicao.parciais))" ($last.texto -eq "Voltz Studio" -and [int]$st.sobreposicao.parciais -gt $partialsBefore)
    $feed = Feed-State
    Check "A: o plugin tem o texto ($($feed.tracos) traco(s), ids $($feed.ids -join ','))" ($feed.tracos -eq 1)
    $img = Scene-Image $sceneScreen
    $lime = Lime-Count $img 95 300 72 110
    $img.Save((Join-Path $logDir "programa-texto.png")); $img.Dispose()
    Check "A: programa com o texto ($lime pixels lima na regiao do texto)" ($lime -ge 40)

    [void](VendorShell "CommitText")
    $st = Shell-State
    Assert-DrawSafe "A confirmado"
    Check "A: confirmar termina a digitacao (digitando $($st.digitando), aberto $($st.aberto), cursor $(@($st.cursor_texto).Count))" ((-not $st.digitando) -and $st.aberto -eq 0 -and @($st.cursor_texto).Count -eq 0)

    # acentos e duas linhas (conferidos no anotacoes.jsonl, lido do arquivo em UTF-8)
    [void](VendorShell "InjectText" (Text-Json "[[400,700]]" $accentText))
    $st = Shell-State
    Assert-DrawSafe "A acentos"
    Check "A: texto com acentos confirmado ($(@($st.visiveis).Count) na tela)" (@($st.visiveis).Count -eq 2 -and -not $st.digitando)

    # gravacao: os dois textos de antes (t_ini 0), um digitado gravando, um cancelado com algo (t_limpo) e um vazio
    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "A: StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 2
    [void](VendorShell "InjectText" (Text-Json "[[1400,300]]" "Aula" -Open))
    [void](VendorShell "TypeText" '{"texto":"Aula 4D"}')
    [void](VendorShell "CommitText")
    Start-Sleep -Milliseconds 500; Assert-DrawSafe "A gravando 1"
    [void](VendorShell "InjectText" (Text-Json "[[1400,600]]" "x" -Open))
    [void](VendorShell "CancelText")
    Start-Sleep -Milliseconds 500; Assert-DrawSafe "A gravando 2"
    [void](VendorShell "InjectText" (Text-Json "[[1400,900]]" "" -Open))
    [void](VendorShell "CancelText")
    $st = Shell-State
    Assert-DrawSafe "A gravando 3"
    Check "A: cancelados saem ($(@($st.visiveis).Count) na tela, digitando $($st.digitando))" (@($st.visiveis).Count -eq 3 -and -not $st.digitando)
    Start-Sleep -Seconds 1
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "A: StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 5
    $ov = $st.sobreposicao
    $feed = Feed-State
    Write-Host ("A: sobreposicao: {0} parciais (media {1} us, max {2} us), {3} completa(s) (max {4} us); fonte: max {5} us" -f $ov.parciais, $ov.parcial_us_media, $ov.parcial_us_max, $ov.completas, $ov.completa_us_max, $feed.raster_us_max)
    Check "A: redesenho do texto abaixo de 100 ms (sobreposicao max $($ov.parcial_us_max) us, fonte max $($feed.raster_us_max) us)" ([int64]$ov.parcial_us_max -lt 100000 -and [int64]$feed.raster_us_max -lt 100000)
    Check "A: encerrou limpo" (Close-App "A")
    $dirs = @(Get-ChildItem -LiteralPath $OutDir -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "*Smoke texto*" -and (Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz")) })
    Check "A: uma pasta de projeto" ($dirs.Count -eq 1)
    if ($dirs.Count -eq 1) {
        $lines = Read-Jsonl (Join-Path $dirs[0].FullName "anotacoes.jsonl")
        $texts = @($lines | Where-Object { $_.ferr -eq "texto" })
        $byText = @{}
        foreach ($l in $texts) { $byText[$l.texto] = $l }
        Check "A: anotacoes.jsonl com 4 textos ($(@($texts | ForEach-Object { $_.texto }) -join ' | '))" ($texts.Count -eq 4)
        Check "A: textos de antes da gravacao com t_ini 0" ($byText.ContainsKey("Voltz Studio") -and $byText["Voltz Studio"].t_ini -eq 0 -and $byText.ContainsKey($accentText) -and $byText[$accentText].t_ini -eq 0)
        Check "A: acentos e quebra de linha preservados" ($byText.ContainsKey($accentText) -and $byText[$accentText].tam -eq 96 -and $byText[$accentText].fonte -eq "dm_sans_negrito")
        Check "A: digitado gravando com t_ini > 0 e t_fim" ($byText.ContainsKey("Aula 4D") -and $byText["Aula 4D"].t_ini -gt 0 -and (Has $byText["Aula 4D"] "t_fim"))
        Check "A: cancelado com algo tem t_limpo; o vazio nao entra" ($byText.ContainsKey("x") -and (Has $byText["x"] "t_limpo") -and -not $byText.ContainsKey(""))
    }
    Check "A: log - texto comecado/confirmado/cancelado" ((LogHas "A" "[voltz] anotacoes: texto comecado (tam 96, fonte dm_sans_negrito)") -and (LogHas "A" "[voltz] anotacoes: texto confirmado (12 caractere(s))") -and (LogHas "A" "[voltz] anotacoes: texto cancelado"))
    $logA = Join-Path $logDir "A.txt"
    $typedInLog = @(@("Aula 4D", "linha 2", "A${ccedil}${atilde}o 4D") | Where-Object { LogHasAnyEncoding "A" $_ })
    Check "A: log existe e sem o conteudo digitado, nem acentuado em UTF-8/Windows-1252 (privacidade; achados: $($typedInLog -join ' | '))" ((Test-Path -LiteralPath $logA) -and $typedInLog.Count -eq 0)
    Check "A: log - o modo desenho nunca ligou" (-not (LogHas "A" "[voltz] modo desenho: ligado"))

    # ---------------- Fase B: Ajustes (atalhos e anotacoes) e a barra com o texto ----------------
    Set-TestRect 640 360
    New-CleanConfig "NextProjectName=Smoke texto B`r`n"
    Set-TestEnv
    Check "B: app aberto" (Start-App "B")
    $defaultDraw = '{"bindings":[{"shift":true,"control":true,"key":"OBS_KEY_D"}]}'
    $hk = Hotkeys
    Check "B: atalhos padrao ao vivo (desenhar $(BindingsText $hk 'VoltzStudio.Draw'))" ((BindingsText $hk "VoltzStudio.Draw") -eq $defaultDraw -and $hk.'VoltzStudio.AddMarker'.bindings[0].key -eq "OBS_KEY_M" -and $hk.'VoltzStudio.ToggleSpotlight'.bindings[0].key -eq "OBS_KEY_H" -and @($hk.'VoltzStudio.UndoAnnotation'.bindings).Count -eq 0 -and -not (Has $hk "VoltzStudio.DebugFreezeUi"))
    Check "B: Ajustes (UI Automation)" (Uia "Ajustes")
    Start-Sleep -Seconds 1
    Check "B: secoes Anotacoes e Atalhos presentes (so leitura)" ((UiaExists $textSizeName) -and (UiaExists $previewName) -and (UiaExists $editDraw))
    Assert-DrawSafe "B ajustes"
    Check "B: Limpar atalho de desenhar (UI Automation)" (Uia $clearDraw)
    $hk = Hotkeys
    $ini = Profile-Ini
    Check "B: atalho de desenhar limpo ao vivo e no perfil ($(BindingsText $hk 'VoltzStudio.Draw'))" ((BindingsText $hk "VoltzStudio.Draw") -eq '{"bindings":[]}' -and $ini.Contains('VoltzStudio.Draw={"bindings":[]}'))
    Assert-Refused "B sem atalho"
    Check "B: Restaurar padrao (UI Automation)" (Uia $resetDraw)
    $hk = Hotkeys
    $ini = Profile-Ini
    Check "B: atalho de desenhar de volta ao padrao ao vivo e no perfil" ((BindingsText $hk "VoltzStudio.Draw") -eq $defaultDraw -and $ini.Contains("VoltzStudio.Draw=$defaultDraw"))
    Assert-Refused "B restaurado"
    $bar = VendorShell "ToolbarImage" '{"ferramenta":"texto","seletor":true}'
    if ($bar.png) { [IO.File]::WriteAllBytes((Join-Path $logDir "barra-texto.png"), [Convert]::FromBase64String($bar.png)) }
    Check "B: barra com o texto e o seletor aberto, fora da tela e sem janela nativa ($($bar.largura)x$($bar.altura), nativa $($bar.janela_nativa))" ($bar.largura -ge 400 -and $bar.altura -ge 150 -and -not $bar.janela_nativa)
    Assert-DrawSafe "B barra"
    Check "B: encerrou limpo" (Close-App "B")
    Check "B: log - atalho limpo e restaurado" ((LogHas "B" '[voltz] atalhos: VoltzStudio.Draw = {"bindings":[]}') -and (LogHas "B" "[voltz] atalhos: VoltzStudio.Draw = $defaultDraw"))
    Check "B: log - o modo desenho nunca ligou" (-not (LogHas "B" "[voltz] modo desenho: ligado"))
    Check "B: log - os atalhos nunca foram pausados (campo de atalho nunca focado)" (-not (LogHas "B" "[voltz] atalhos: editando"))
} catch {
    Check "sem excecao ($($_.Exception.Message))" $false
} finally {
    Cleanup "fechar o app" { if ($script:proc -and -not $script:proc.HasExited) { [void](Close-App "final") } }
    Cleanup "encerrar o PID lancado" { if ($script:proc -and -not $script:proc.HasExited) { $script:forced++; Stop-Process -Id $script:proc.Id -Force; [void]$script:proc.WaitForExit(10000) } }
    Cleanup "parar o vigia" { if ($script:job) { Stop-Watchdog "final" } }
    Cleanup "variaveis de teste" { Remove-Item Env:VOLTZ_TEST_LIVE, Env:VOLTZ_TEST_PRESET_STANDINS, Env:VOLTZ_TEST_OVERLAY_RECT -ErrorAction SilentlyContinue }
    Cleanup "restaurar a config portatil" { Restore-Config }
    Cleanup "apagar -OutDir" { if (-not $Keep -and (Test-Path -LiteralPath $OutDir)) { Remove-Item -Recurse -Force -LiteralPath $OutDir; Write-Host "apagado: $OutDir" } }
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
Write-Host "SMOKE TEXTO OK"
