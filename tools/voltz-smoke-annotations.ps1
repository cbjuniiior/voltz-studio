<#
  Teste de integracao do Plano 4B (anotacoes: fonte Voltz Anotacoes, sobreposicao, anotacoes.jsonl).
  Seguranca (o PC e do usuario e esta em uso):
  - VOLTZ_TEST_LIVE=1: tracos SO pelos pedidos "voltz-teste-casca" (InjectStroke etc.), nunca pelo mouse/teclado; no
    Plano 4B a sobreposicao nao tem modo que capture entrada.
  - VOLTZ_TEST_PRESET_STANDINS=1 em todas as fases: Tela 2560x1440 e Camera de COR (nenhuma captura de tela nem
    webcam); Assert-NoRealCapture aborta se houver monitor/janela/jogo/dshow.
  - VOLTZ_TEST_OVERLAY_RECT: a sobreposicao (transparente a cliques) fica num retangulo A DIREITA de todo o desktop
    virtual - o usuario nao a ve. O retangulo e conferido contra o desktop virtual e cada monitor ANTES de abrir o app
    (se cruzar algum, o smoke recusa). O desenho dela e conferido no buffer (ProbeOverlay), nunca por captura de tela.
  - Assert-OverlaySafe depois de cada acao: as janelas VoltzAnnotationOverlay do PID lancado (FindWindowEx com titulo
    nulo E EnumWindows; se as listas divergirem, BLOCKED) precisam ter WS_EX_TOPMOST|TRANSPARENT|TOOLWINDOW|LAYERED|
    NOACTIVATE (0x080800A8), nao estar em primeiro plano, ficar dentro do retangulo de teste e fora do desktop virtual
    e ter a afinidade esperada (0x11; 0 so na fase "queimadas"). Um vigia (job em segundo plano) repete a conferencia a
    cada 200 ms enquanto o app roda. Qualquer falha: o app (PID lancado) e encerrado na hora e o smoke falha.
  - O app fecha por WM_CLOSE na janela da casca guardada na abertura (conferida como do PID lancado): com a
    sobreposicao visivel, Process.MainWindowHandle pode apontar para ela, que ignora WM_CLOSE. Stop-Process so pelo PID.
  - So fala com o app lancado: a porta 4455 precisa estar livre antes e ser do PID lancado depois.
  - Render do programa so por GetSourceScreenshot de cena sintetica; so metadados (ffprobe, nenhum quadro de video);
    config portatil guardada e restaurada; -OutDir apagado no fim; Videos conferida intacta; no fim, nenhuma
    sobreposicao orfa.
  Fases: A (preset, 3 camadas, programa + sobreposicao, desfazer/limpar, gravacao com anotacoes.jsonl, recuperacao de
  um projeto interrompido montado a mao), B (grupo Tela Voltz versao 1 -> 2; sem retangulo de teste e sem captura de
  monitor nao ha sobreposicao), C (queimadas simulado), D (somem apos 1 s, gravando).
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-annotations.ps1 [-OutDir <pasta>] [-Keep]
#>
param(
    [string]$Config = "RelWithDebInfo",
    [string]$OutDir = (Join-Path $env:TEMP "voltz-smoke-annotations"),
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
$o = [string][char]0xF3; $a = [string][char]0xE2; $dash = [string][char]0x2014
$presetScreen = "S${o} tela / S${o} c${a}mera $dash Aplicar"
$sceneScreen = "S${o} tela"
$recoverAll = "Recuperar grava" + [char]0xE7 + [char]0xF5 + "es"

$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.smokeannot-bak"
$voltzCfg = Join-Path $cfg "Voltz Studio"
$logDir = Join-Path $env:TEMP "voltz-smoke-annotations-logs"
$wsPort = 4455

if ((Test-Path -LiteralPath $OutDir) -and @(Get-ChildItem -Force -LiteralPath $OutDir).Count -gt 0) {
    Write-Host "FALHOU -OutDir ja existe e nao esta vazio: $OutDir"; exit 1
}
$oldBaks = @(Get-ChildItem -LiteralPath $rundir -Directory -Force -Filter "config.*bak*" -ErrorAction SilentlyContinue)
if ($oldBaks.Count -gt 0) { Write-Host "FALHOU backup anterior em $rundir ($(($oldBaks | ForEach-Object { $_.Name }) -join ', ')): restaure ou apague antes de rodar"; exit 1 }
if (@(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue).Count -gt 0) {
    Write-Host "FALHOU a porta $wsPort ja esta em uso (outro OBS/Voltz aberto?): o smoke so fala com o app que ele abre"; exit 1
}

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
# P/Invoke de LEITURA (nenhuma funcao que mova o mouse, clique, digite, ative ou mude janelas). A unica escrita e
# PostMessage(WM_CLOSE), usada so na janela da casca do PID lancado.
Add-Type @'
using System; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class VoltzOverlayProbe {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
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
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder sb, int n);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
 public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
 public static string ClassOf(IntPtr h) { var sb = new System.Text.StringBuilder(256); GetClassName(h, sb, 256); return sb.ToString(); }
 public static uint OwnerOf(IntPtr h) { uint owner; GetWindowThreadProcessId(h, out owner); return owner; }
 public static List<long> WindowsOfClass(uint pid, string cls) { var list = new List<long>();
   EnumWindows((h, l) => { if (OwnerOf(h) == pid && ClassOf(h) == cls) list.Add(h.ToInt64()); return true; }, IntPtr.Zero);
   return list; } }
'@
$P = [VoltzOverlayProbe]
[void]$P::SetProcessDPIAware()

# Retangulo de teste: 640x360 A DIREITA de todo o desktop virtual (ninguem ve, nada fica por cima de janela alguma)
$vx = $P::GetSystemMetrics(76); $vy = $P::GetSystemMetrics(77); $vw = $P::GetSystemMetrics(78); $vh = $P::GetSystemMetrics(79)
$ovX = $vx + $vw + 200; $ovY = $vy; $ovW = 640; $ovH = 360
$overlayRect = "{0},{1},{2},{3}" -f $ovX, $ovY, $ovW, $ovH
function Crosses([int]$l, [int]$t, [int]$r, [int]$b, [int]$L2, [int]$T2, [int]$R2, [int]$B2) { return ($l -lt $R2 -and $r -gt $L2 -and $t -lt $B2 -and $b -gt $T2) }
if (Crosses $ovX $ovY ($ovX + $ovW) ($ovY + $ovH) $vx $vy ($vx + $vw) ($vy + $vh)) { Write-Host "FALHOU retangulo de teste $overlayRect cruza o desktop virtual - recusado"; exit 1 }
foreach ($s in [System.Windows.Forms.Screen]::AllScreens) {
    $sb = $s.Bounds
    if (Crosses $ovX $ovY ($ovX + $ovW) ($ovY + $ovH) $sb.Left $sb.Top $sb.Right $sb.Bottom) { Write-Host "FALHOU retangulo de teste $overlayRect sobre o monitor $($s.DeviceName) - recusado"; exit 1 }
}
Write-Host "desktop virtual ($vx,$vy) ${vw}x${vh}; retangulo de teste da sobreposicao: $overlayRect"

# Videos: lista ANTES de abrir o app (o smoke nao pode mexer em nada que ja existe la)
$videosDir = [Environment]::GetFolderPath('MyVideos')
$voltzVideos = Join-Path $videosDir "Voltz Studio"
$hadVoltzVideos = Test-Path -LiteralPath $voltzVideos
function Videos-List { return ,@(Get-ChildItem -Force -LiteralPath $videosDir | ForEach-Object { "$($_.Name)|$($_.LastWriteTimeUtc.Ticks)|$($_.Attributes)" }) }
$videosBefore = Videos-List
Write-Host "Videos antes ($($videosBefore.Count)): $(($videosBefore | ForEach-Object { $_.Split('|')[0] }) -join ', ')"

# Config portatil: impressao digital antes (conferida depois da restauracao)
function Hash-Dir([string]$d) {
    if (-not (Test-Path -LiteralPath $d)) { return "(ausente)" }
    return (Get-ChildItem -Recurse -File -Force -LiteralPath $d | Sort-Object FullName | ForEach-Object { $_.FullName.Substring($d.Length) + "|" + (Get-FileHash -Algorithm MD5 -LiteralPath $_.FullName).Hash }) -join "`n"
}
$cfgHashBefore = Hash-Dir $cfg

$checks = [ordered]@{}
function Check([string]$name, [bool]$ok) { $script:checks[$name] = $ok }

function Ws([string]$json) {
    $file = Join-Path $env:TEMP "voltz-smoke-annotations-req.json"
    [IO.File]::WriteAllText($file, $json, (New-Object Text.UTF8Encoding $false))
    $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $file -Port $wsPort
    Remove-Item -Force $file -ErrorAction SilentlyContinue
    # a virgula preserva o array mesmo com 1 elemento
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

# Os pedidos da casca sao enfileirados para a thread da UI: espera um pouco antes de ler o estado
function Shell-State { Start-Sleep -Milliseconds 400; return ((VendorShell "AnnotationState").estado | ConvertFrom-Json) }

function Feed-State { return ((Vendor "AnnotationFeedState").estado | ConvertFrom-Json) }

function Stroke-Json([string]$tool, [string]$color, [int]$width, [string]$points, [switch]$Open) {
    $req = [ordered]@{ ferr = $tool; cor = $color; esp = $width; pts = $points }
    if ($Open) { $req.aberto = $true }
    return (ConvertTo-Json $req -Compress)
}

function Kill-Launched([string]$why) {
    if ($script:proc -and -not $script:proc.HasExited) { Stop-Process -Id $script:proc.Id -Force }
    throw "BLOCKED $why - app (PID lancado) encerrado"
}

# Janelas de sobreposicao do PID lancado: FindWindowEx (titulo NULO de verdade - $null num parametro string do .NET
# vira "" no PowerShell e so acharia janelas sem titulo) conferido contra EnumWindows; divergencia = BLOCKED.
function Overlay-Windows([int]$procId) {
    $found = @(); $h = [IntPtr]::Zero
    while ($true) {
        $h = $P::FindWindowEx([IntPtr]::Zero, $h, "VoltzAnnotationOverlay", [NullString]::Value)
        if ($h -eq [IntPtr]::Zero) { break }
        if ($P::OwnerOf($h) -eq $procId) { $found += $h.ToInt64() }
    }
    $enum = @($P::WindowsOfClass([uint32]$procId, "VoltzAnnotationOverlay"))
    $f = (@($found | Sort-Object) -join ','); $e = (@($enum | Sort-Object) -join ',')
    if ($f -ne $e) { Kill-Launched "enumeracao divergente: FindWindowEx=[$f] EnumWindows=[$e]" }
    return ,@($found | ForEach-Object { [IntPtr][Int64]$_ })
}

# Seguranca: estilos 0x080800A8, nunca em primeiro plano, dentro do retangulo de teste, sem cruzar o desktop virtual e
# com a afinidade esperada (0x11; 0 so na fase "queimadas"). Qualquer falha: encerra o PID lancado e BLOCKED.
$script:overlays = @()
$script:asserts = 0
$script:expectAff = 0x11
function Assert-OverlaySafe([string]$where) {
    $list = Overlay-Windows $script:proc.Id
    foreach ($h in $list) {
        if ($script:overlays -notcontains $h.ToInt64()) { $script:overlays += $h.ToInt64() }
        $ex = $P::GetWindowLongPtr($h, -20).ToInt64()
        $r = New-Object VoltzOverlayProbe+RECT; [void]$P::GetWindowRect($h, [ref]$r)
        [uint32]$aff = 99; [void]$P::GetWindowDisplayAffinity($h, [ref]$aff)
        $bad = @()
        if (($ex -band 0x080800A8) -ne 0x080800A8) { $bad += ("estilo 0x{0:X}" -f $ex) }
        if ($P::GetForegroundWindow() -eq $h) { $bad += "primeiro plano" }
        if ($r.L -lt $ovX -or $r.T -lt $ovY -or $r.R -gt ($ovX + $ovW) -or $r.B -gt ($ovY + $ovH)) { $bad += "fora do retangulo de teste ($($r.L),$($r.T),$($r.R),$($r.B))" }
        if (Crosses $r.L $r.T $r.R $r.B $vx $vy ($vx + $vw) ($vy + $vh)) { $bad += "cruza o desktop virtual" }
        if ($aff -ne $script:expectAff) { $bad += ("afinidade 0x{0:X}" -f $aff) }
        if ($bad.Count -gt 0) { Kill-Launched ("[{0}] sobreposicao insegura hwnd={1}: {2}" -f $where, $h.ToInt64(), ($bad -join '; ')) }
    }
    $script:asserts++
    Write-Host ("assert[{0}] ok: {1} sobreposicao(oes) do PID" -f $where, $list.Count)
}

# Vigia em segundo plano: a cada 200 ms repete a conferencia (so leitura) em toda janela VoltzAnnotationOverlay do PID;
# se alguma estiver insegura, encerra ESSE PID e registra BLOCKED. Para quando o PID sai ou quando $StopFile existe.
$watchdog = {
    param([int]$TargetPid, [int]$OvX, [int]$OvY, [int]$OvW, [int]$OvH, [int]$ExpectAff, [string]$StopFile, [string]$LogFile)
    Add-Type @'
using System; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class VoltzOverlayWatch {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")] public static extern IntPtr GetWindowLongPtr(IntPtr h, int index);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool GetWindowDisplayAffinity(IntPtr h, out uint affinity);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder sb, int n);
 public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
 public static uint OwnerOf(IntPtr h) { uint owner; GetWindowThreadProcessId(h, out owner); return owner; }
 public static List<long> Overlays(uint pid) { var set = new HashSet<long>(); IntPtr h = IntPtr.Zero;
   while ((h = FindWindowEx(IntPtr.Zero, h, "VoltzAnnotationOverlay", null)) != IntPtr.Zero) { if (OwnerOf(h) == pid) set.Add(h.ToInt64()); }
   EnumWindows((w, l) => { if (OwnerOf(w) == pid) { var sb = new System.Text.StringBuilder(256); GetClassName(w, sb, 256);
     if (sb.ToString() == "VoltzAnnotationOverlay") set.Add(w.ToInt64()); } return true; }, IntPtr.Zero);
   return new List<long>(set); } }
'@
    $W = [VoltzOverlayWatch]
    [void]$W::SetProcessDPIAware()
    $vx = $W::GetSystemMetrics(76); $vy = $W::GetSystemMetrics(77); $vw = $W::GetSystemMetrics(78); $vh = $W::GetSystemMetrics(79)
    $rounds = 0; $seen = @{}
    while ($true) {
        if (Test-Path -LiteralPath $StopFile) { break }
        $p = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
        if (-not $p -or $p.HasExited) { break }
        # uniao das duas enumeracoes (no vigia uma janela pode nascer entre uma e outra; aqui vale a mais ampla)
        foreach ($id in $W::Overlays([uint32]$TargetPid)) {
            $h = [IntPtr]$id
            $seen[$id] = $true
            $ex = $W::GetWindowLongPtr($h, -20).ToInt64()
            $r = New-Object VoltzOverlayWatch+RECT; [void]$W::GetWindowRect($h, [ref]$r)
            [uint32]$aff = 99; [void]$W::GetWindowDisplayAffinity($h, [ref]$aff)
            $bad = @()
            if (($ex -band 0x080800A8) -ne 0x080800A8) { $bad += ("estilo 0x{0:X}" -f $ex) }
            if ($W::GetForegroundWindow() -eq $h) { $bad += "primeiro plano" }
            if ($r.L -lt $OvX -or $r.T -lt $OvY -or $r.R -gt ($OvX + $OvW) -or $r.B -gt ($OvY + $OvH)) { $bad += "fora do retangulo de teste ($($r.L),$($r.T),$($r.R),$($r.B))" }
            if ($r.L -lt ($vx + $vw) -and $r.R -gt $vx -and $r.T -lt ($vy + $vh) -and $r.B -gt $vy) { $bad += "cruza o desktop virtual" }
            if ($aff -ne $ExpectAff) { $bad += ("afinidade 0x{0:X}" -f $aff) }
            if ($bad.Count -gt 0) {
                Stop-Process -Id $TargetPid -Force
                Add-Content -LiteralPath $LogFile ("BLOCKED vigia hwnd={0}: {1}" -f $id, ($bad -join '; '))
                return
            }
        }
        $rounds++
        Start-Sleep -Milliseconds 200
    }
    Add-Content -LiteralPath $LogFile ("vigia: {0} conferencias sem problema; sobreposicoes vistas: {1}" -f $rounds, $seen.Count)
}

# Seguranca: nenhuma captura real (tela, janela, jogo, webcam) pode existir no app
function Assert-NoRealCapture {
    $inputs = @((Ws '[{"requestType":"GetInputList"}]')[0].data.inputs)
    $real = @($inputs | Where-Object { $_.inputKind -match 'monitor_capture|window_capture|game_capture|dshow_input' })
    if ($real.Count -gt 0) { throw ("captura real presente - abortando: " + (($real | ForEach-Object { $_.inputName }) -join ', ')) }
}

# Render de cena SINTETICA em 640x360 - nunca a tela real nem quadro de video
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

# Camadas do (primeiro) grupo de uma cena
function Screen-Layers([string]$scene) {
    $top = @((Ws ('[{"requestType":"GetSceneItemList","requestData":{"sceneName":' + (ConvertTo-Json $scene) + '}}]'))[0].data.sceneItems)
    $group = @($top | Where-Object { $_.isGroup } | Select-Object -First 1)
    if ($group.Count -lt 1) { return ,@() }
    return ,@((Ws ('[{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":' + (ConvertTo-Json ([string]$group[0].sourceName)) + '}}]'))[0].data.sceneItems)
}

function Read-JsonFile([string]$path) {
    return (Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json) | ForEach-Object { $_ }
}

function Read-Jsonl([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return ,@() }
    return ,@(Get-Content -LiteralPath $path -Encoding UTF8 | Where-Object { $_ } | ForEach-Object { $_ | ConvertFrom-Json })
}

function Has([object]$obj, [string]$name) { return ($null -ne $obj) -and ($obj.PSObject.Properties.Name -contains $name) }

function New-CleanConfig([string]$voltzLines) {
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
    New-Item -ItemType Directory -Force $wsDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":' + $wsPort + '}')
    [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[Voltz]`r`n" + $voltzLines, (New-Object Text.UTF8Encoding $false))
}

# Modos de teste: VOLTZ_TEST_LIVE e VOLTZ_TEST_PRESET_STANDINS sempre; o resto conforme a fase
function Set-TestEnv([bool]$overlay, [bool]$burned, [bool]$layersV1) {
    $env:VOLTZ_TEST_LIVE = "1"
    $env:VOLTZ_TEST_PRESET_STANDINS = "1"
    if ($overlay) { $env:VOLTZ_TEST_OVERLAY_RECT = $overlayRect } else { Remove-Item Env:VOLTZ_TEST_OVERLAY_RECT -ErrorAction SilentlyContinue }
    if ($burned) { $env:VOLTZ_TEST_ANNOT_BURNED = "1" } else { Remove-Item Env:VOLTZ_TEST_ANNOT_BURNED -ErrorAction SilentlyContinue }
    if ($layersV1) { $env:VOLTZ_TEST_SCREEN_LAYERS = "1" } else { Remove-Item Env:VOLTZ_TEST_SCREEN_LAYERS -ErrorAction SilentlyContinue }
    $script:expectAff = if ($burned) { 0 } else { 0x11 }
}

function Start-App([string]$tag) {
    if (@(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue).Count -gt 0) { throw "porta $wsPort ocupada antes de abrir o app ($tag)" }
    $script:proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
    $script:tag = $tag
    $script:wdLog = Join-Path $logDir "vigia-$tag.txt"
    $script:wdStop = Join-Path $logDir "vigia-$tag.stop"
    $script:job = Start-Job -ScriptBlock $watchdog -ArgumentList $script:proc.Id, $ovX, $ovY, $ovW, $ovH, $script:expectAff, $script:wdStop, $script:wdLog
    Start-Sleep -Seconds 15
    if ($script:proc.HasExited) { return $false }
    # janela da casca: guardada agora (ainda sem sobreposicao) para o WM_CLOSE do fechamento
    $script:shell = [IntPtr]::Zero
    for ($i = 0; $i -lt 30 -and $script:shell -eq [IntPtr]::Zero; $i++) {
        $script:proc.Refresh()
        $h = $script:proc.MainWindowHandle
        if ($h -ne [IntPtr]::Zero -and $P::OwnerOf($h) -eq $script:proc.Id -and $P::ClassOf($h) -ne "VoltzAnnotationOverlay") { $script:shell = $h } else { Start-Sleep -Milliseconds 500 }
    }
    $owners = @(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue | ForEach-Object { [int]$_.OwningProcess })
    if (-not ($owners -contains $script:proc.Id)) { Kill-Launched "a porta $wsPort nao e do app lancado ($($owners -join ','))" }
    Write-Host "[$tag] PID lancado $($script:proc.Id); casca hwnd=$($script:shell.ToInt64()) classe '$($P::ClassOf($script:shell))'"
    Assert-OverlaySafe "$tag abertura"
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

# Fecha o app lancado aqui (so pelo PID): WM_CLOSE na janela da casca guardada na abertura (Process.CloseMainWindow
# pode mirar a sobreposicao, que ignora WM_CLOSE); Stop-Process -Id so se nao fechar em 30 s. Guarda o log como <tag>.txt.
$script:forced = 0
function Close-App([string]$tag) {
    $clean = $false
    if ($script:proc -and -not $script:proc.HasExited) {
        $procId = $script:proc.Id
        $script:proc.Refresh()
        $shellOk = ($script:shell -ne [IntPtr]::Zero) -and $P::IsWindow($script:shell) -and ($P::OwnerOf($script:shell) -eq $procId) -and ($P::ClassOf($script:shell) -ne "VoltzAnnotationOverlay")
        if ($shellOk) { [void]$P::PostMessage($script:shell, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) } else { [void]$script:proc.CloseMainWindow() }
        $clean = $script:proc.WaitForExit(30000)
        if (-not $clean) { $script:forced++; Write-Host "[$tag] FECHAMENTO FORCADO (30 s)"; Stop-Process -Id $procId -Force; [void]$script:proc.WaitForExit(10000) }
        $left = (Overlay-Windows $procId).Count
        $orphans = @($script:overlays | Where-Object { $P::IsWindow([IntPtr]$_) }).Count
        Check "${tag}: nenhuma sobreposicao depois de fechar (do PID $left, orfas $orphans)" ($left -eq 0 -and $orphans -eq 0)
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

function Project-Dir([string]$like) {
    return ,@(Get-ChildItem -LiteralPath $OutDir -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like $like -and (Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz")) })
}

function Program-Scene { [void](Ws ('[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":' + (ConvertTo-Json $sceneScreen) + '}}]')) }

$ffprobe = (Get-Command ffprobe -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $ffprobe) {
    $ffprobe = Get-ChildItem (Join-Path $root ".deps") -Filter ffprobe.exe -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}

if (Test-Path $cfg) { Move-Item $cfg $bak }
if (Test-Path $logDir) { Remove-Item -Recurse -Force $logDir }
New-Item -ItemType Directory -Force $logDir | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null
$proc = $null
$script:job = $null
$utf8 = New-Object Text.UTF8Encoding $false
try {
    # ---------------- Fase A: camadas, programa, sobreposicao, desfazer/limpar, gravacao, recuperacao ----------------
    New-CleanConfig ("NextProjectName=Smoke anotacoes A`r`n")
    Set-TestEnv $true $false $false
    Check "A: app aberto" (Start-App "A")
    Check "A: preset 'So tela / So camera' (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    Assert-OverlaySafe "A preset"
    $inner = Screen-Layers $sceneScreen
    Check "A: grupo = Tela + Voltz Anotacoes + Voltz Mouse, travadas em (0,0)" ($inner.Count -eq 3 -and $inner[0].sourceName -eq "Tela" -and $inner[1].inputKind -eq "voltz_annotations_source" -and $inner[2].inputKind -eq "voltz_mouse_source" -and $inner[1].sceneItemLocked -and $inner[1].sceneItemTransform.positionX -eq 0 -and $inner[1].sceneItemTransform.positionY -eq 0)
    Program-Scene
    $st = Shell-State
    Check "A: sem tracos nem sobreposicao no inicio (captura $($st.captura -join 'x'))" (@($st.visiveis).Count -eq 0 -and -not $st.sobreposicao.criada -and -not $st.queimadas -and $st.captura[0] -eq 2560 -and $st.captura[1] -eq 1440)
    # linha lima esp 16 de (640,720) a (1920,720): na imagem 640x360 e na sobreposicao 640x360, y=180
    [void](VendorShell "InjectStroke" (Stroke-Json "linha" "#D4F53C" 16 "[[640,720],[1920,720]]"))
    $st = Shell-State; Assert-OverlaySafe "A linha"; $feed = Feed-State
    Check "A: um traco na casca e no plugin, camada mostrada (plugin $($feed.tracos), mostradas $($feed.mostradas))" (@($st.visiveis).Count -eq 1 -and $feed.tracos -eq 1 -and $feed.mostradas -ge 1)
    $img = Scene-Image $sceneScreen; $on = $img.GetPixel(320, 180); $off = $img.GetPixel(320, 150); $img.Dispose()
    Check "A: programa com a linha lima ($on) e a Tela fora dela ($off)" ((Near $on 212 245 60) -and (Near $off 42 42 48))
    $hwnd = [IntPtr][Int64]$st.sobreposicao.hwnd
    $ex = $P::GetWindowLongPtr($hwnd, -20).ToInt64(); [uint32]$aff = 99; [void]$P::GetWindowDisplayAffinity($hwnd, [ref]$aff)
    Check ("A: sobreposicao visivel, transparente a cliques, fora da captura (estilo 0x{0:X}, afinidade 0x{1:X})" -f $ex, $aff) ($P::ClassOf($hwnd) -eq "VoltzAnnotationOverlay" -and $P::IsWindowVisible($hwnd) -and (($ex -band 0x080800A8) -eq 0x080800A8) -and $aff -eq 0x11 -and $st.sobreposicao.afinidade -eq 17 -and ($P::GetForegroundWindow() -ne $hwnd))
    Check "A: sobreposicao de teste fora da area de trabalho ($($st.sobreposicao.retangulo -join ','))" ($st.sobreposicao.retangulo[0] -ge ($vx + $vw) -and $st.sobreposicao.retangulo[0] -eq $ovX -and $st.sobreposicao.retangulo[2] -eq 640 -and $st.sobreposicao.retangulo[3] -eq 360)
    [void](VendorShell "ProbeOverlay" '{"pts":"[[320,180],[320,150]]"}')
    $st = Shell-State; Assert-OverlaySafe "A sonda"
    Check "A: buffer da sobreposicao: lima opaco na linha, transparente fora ($($st.amostras -join ' '))" ($st.amostras[0] -eq "#FFD4F53C" -and $st.amostras[1] -eq "#00000000")
    # marca-texto vermelho esp 8 (24 px a 40%) com juncao em (1280,1080) -> (320,270)
    [void](VendorShell "InjectStroke" (Stroke-Json "marca_texto" "#FF4D4D" 8 "[[640,1080],[1280,1080],[1920,1080]]"))
    Start-Sleep -Milliseconds 300; Assert-OverlaySafe "A marca-texto"
    [void](VendorShell "ProbeOverlay" '{"pts":"[[320,270]]"}')
    $st = Shell-State; Assert-OverlaySafe "A sonda 2"
    $img = Scene-Image $sceneScreen; $hl = $img.GetPixel(320, 270); $img.Dispose()
    Check "A: marca-texto a 40% sem escurecer na juncao: programa ($hl), sobreposicao ($($st.amostras[0]))" ((Near $hl 127 56 60) -and ([string]$st.amostras[0]).StartsWith("#66"))
    [void](VendorShell "UndoStroke")
    $st = Shell-State; Assert-OverlaySafe "A desfazer"; $feed = Feed-State
    $img = Scene-Image $sceneScreen; $hl2 = $img.GetPixel(320, 270); $img.Dispose()
    Check "A: desfazer tira o marca-texto ($hl2)" (@($st.visiveis).Count -eq 1 -and $feed.tracos -eq 1 -and (Near $hl2 42 42 48))
    [void](VendorShell "ClearStrokes")
    $st = Shell-State; Assert-OverlaySafe "A limpar"; $feed = Feed-State
    Check "A: limpar esvazia e esconde a sobreposicao" (@($st.visiveis).Count -eq 0 -and $feed.tracos -eq 0 -and -not $P::IsWindowVisible($hwnd) -and -not $st.sobreposicao.visivel)

    # Gravacao: retangulo antes de gravar; seta; caneta aberta+estendida+terminada; desfazer (caneta); limpar
    # (retangulo + seta); elipse ate o fim
    Prepare-Recording
    [void](VendorShell "InjectStroke" (Stroke-Json "retangulo" "#4DA3FF" 6 "[[100,100],[600,400]]"))
    Start-Sleep -Milliseconds 500; Assert-OverlaySafe "A retangulo"; Assert-NoRealCapture
    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "A: StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 2
    Assert-OverlaySafe "A gravando"
    [void](VendorShell "InjectStroke" (Stroke-Json "seta" "#D4F53C" 8 "[[300,1200],[900,900]]"))
    Start-Sleep -Milliseconds 500; Assert-OverlaySafe "A seta"
    [void](VendorShell "InjectStroke" (Stroke-Json "caneta" "#FF4D4D" 6 "[[1000,1000],[1010,1004]]" -Open))
    Assert-OverlaySafe "A caneta aberta"
    [void](VendorShell "ExtendStroke" '{"pts":"[[1020,1010],[1030,1020]]"}')
    Assert-OverlaySafe "A caneta estendida"
    [void](VendorShell "EndStroke")
    Start-Sleep -Milliseconds 500; Assert-OverlaySafe "A caneta terminada"
    [void](VendorShell "UndoStroke")
    Start-Sleep -Milliseconds 500; Assert-OverlaySafe "A desfazer gravando"
    [void](VendorShell "ClearStrokes")
    Start-Sleep -Milliseconds 500; Assert-OverlaySafe "A limpar gravando"
    [void](VendorShell "InjectStroke" (Stroke-Json "elipse" "#D4F53C" 4 "[[1500,300],[2100,700]]"))
    Start-Sleep -Seconds 1
    Assert-OverlaySafe "A elipse"
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "A: StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 5
    Assert-OverlaySafe "A parado"

    # Recuperacao: pasta "interrompida" montada a mao (manifesto "gravando" + anotacoes.jsonl so com o diario). O
    # programa.mp4 falso (zeros, so para a data de modificacao) evita o aviso modal de recuperacao que falhou.
    $broken = Join-Path $OutDir "2026-01-02 10h00 - Interrompido"
    New-Item -ItemType Directory -Force $broken | Out-Null
    [IO.File]::WriteAllText((Join-Path $broken "projeto.voltz"), '{"formato":"voltz-projeto","versao":1,"app":"Voltz Studio 1.0.0","motor":"libobs 32.2.2","nome":"Interrompido","criado_em":"2026-01-02T10:00:00-03:00","status":"gravando","relogio":"obs_ns","t0_ns":1,"duracao_ms":5000,"pausas":[],"arquivos":{"programa":{"arquivo":"programa.mp4","largura":1920,"altura":1080,"fps":60,"offset_ms":0},"anotacoes":{"arquivo":"anotacoes.jsonl","largura":2560,"altura":1440}},"eventos":[]}', $utf8)
    $fake = Join-Path $broken "programa.mp4"
    [IO.File]::WriteAllBytes($fake, (New-Object byte[] 2048))
    (Get-Item -LiteralPath $fake).LastWriteTime = [DateTime]::Parse("2026-01-02T10:00:42-03:00")
    $journal = @(
        '{"id":1,"ferr":"retangulo","cor":"#4DA3FF","esp":6,"pts":[[100,100],[600,400]],"t_ini":0,"t_fim":0}',
        '{"id":2,"ferr":"seta","cor":"#D4F53C","esp":8,"pts":[[300,1200],[900,900]],"t_ini":1200,"t_fim":1850}',
        '{"id":1,"t_limpo":3000}',
        '{"id":2,"t_limpo":3000}',
        '{"id":3,"ferr":"caneta","cor":"#FF4D4D","esp":6,"pts":[[1000,1000],[1010,1004]],"t_ini":4000,"t_fim":4500}') -join "`n"
    [IO.File]::WriteAllText((Join-Path $broken "anotacoes.jsonl"), $journal + "`n", $utf8)
    Check "A: pagina Projetos (UI Automation)" (Uia "Projetos")
    Start-Sleep -Seconds 2
    Assert-OverlaySafe "A projetos"
    Check "A: recuperar gravacoes (UI Automation)" (Uia $recoverAll)
    Start-Sleep -Seconds 2
    Assert-OverlaySafe "A recuperar"

    Check "A: encerrou limpo" (Close-App "A")
    $dirs = Project-Dir "*Smoke anotacoes A*"
    Check "A: uma pasta de projeto" ($dirs.Count -eq 1)
    if ($dirs.Count -eq 1) {
        $dir = $dirs[0].FullName
        $m = Read-JsonFile (Join-Path $dir "projeto.voltz")
        Check "A: manifesto ok com arquivos.anotacoes 2560x1440 e sem queimadas" ($m.status -eq "ok" -and $m.arquivos.anotacoes.arquivo -eq "anotacoes.jsonl" -and $m.arquivos.anotacoes.largura -eq 2560 -and $m.arquivos.anotacoes.altura -eq 1440 -and -not (Has $m "anotacoes_queimadas"))
        Check "A: tela.mkv continua sendo a captura (2560x1440)" ($m.arquivos.tela -and $m.arquivos.tela.largura -eq 2560 -and $m.arquivos.tela.altura -eq 1440)
        $tela = Join-Path $dir "tela.mkv"
        if ($ffprobe -and (Test-Path -LiteralPath $tela)) {
            # so metadados (nunca quadros)
            $ip = (& $ffprobe -v error -show_entries "stream=codec_type,width,height" -of json $tela) | Out-String | ConvertFrom-Json
            $v = @($ip.streams | Where-Object { $_.codec_type -eq "video" })
            Check "A: ffprobe tela.mkv 2560x1440" ($v.Count -eq 1 -and $v[0].width -eq 2560 -and $v[0].height -eq 1440)
        } else {
            Write-Host "AVISO ffprobe ou tela.mkv ausente: resolucao do ISO conferida so pelo manifesto"
        }
        $lines = Read-Jsonl (Join-Path $dir "anotacoes.jsonl")
        $tools = @($lines | ForEach-Object { $_.ferr }) -join ','
        Check "A: anotacoes.jsonl consolidado na ordem retangulo,seta,caneta,elipse ($tools)" ($lines.Count -eq 4 -and $tools -eq "retangulo,seta,caneta,elipse")
        if ($lines.Count -eq 4) {
            $ret = $lines[0]; $seta = $lines[1]; $caneta = $lines[2]; $elipse = $lines[3]
            Check "A: retangulo de antes da gravacao em 0 e limpo junto com a seta ($($ret.t_limpo))" ($ret.t_ini -eq 0 -and $ret.t_fim -eq 0 -and $ret.t_limpo -gt 0 -and $ret.t_limpo -eq $seta.t_limpo)
            Check "A: seta em pixels da captura (t_ini $($seta.t_ini))" ($seta.pts[0][0] -eq 300 -and $seta.pts[0][1] -eq 1200 -and $seta.pts[1][0] -eq 900 -and $seta.esp -eq 8 -and $seta.cor -eq "#D4F53C" -and $seta.t_ini -gt 0 -and $seta.t_fim -ge $seta.t_ini)
            Check "A: caneta com 4 pontos, desfeita antes do limpar ($($caneta.t_ini)-$($caneta.t_fim), limpo $($caneta.t_limpo))" (@($caneta.pts).Count -eq 4 -and $caneta.t_fim -ge $caneta.t_ini -and $caneta.t_limpo -ge $caneta.t_fim -and $caneta.t_limpo -le $seta.t_limpo)
            Check "A: elipse na tela ate o fim (sem t_limpo)" (-not (Has $elipse "t_limpo") -and $elipse.t_ini -ge $seta.t_limpo -and $elipse.t_fim -ge $elipse.t_ini)
        }
    }
    $rm = Read-JsonFile (Join-Path $broken "projeto.voltz")
    $rl = Read-Jsonl (Join-Path $broken "anotacoes.jsonl")
    $short = @($rl | Where-Object { -not (Has $_ "ferr") }).Count
    Check "A: recuperado (status $($rm.status), $($rl.Count) linha(s), $short curta(s))" ($rm.status -eq "recuperado" -and $rm.arquivos.anotacoes.arquivo -eq "anotacoes.jsonl")
    Check "A: recuperacao consolida o diario (retangulo e seta com t_limpo 3000, caneta sem)" ($rl.Count -eq 3 -and $short -eq 0 -and $rl[0].ferr -eq "retangulo" -and $rl[0].t_limpo -eq 3000 -and $rl[1].ferr -eq "seta" -and $rl[1].t_limpo -eq 3000 -and $rl[2].ferr -eq "caneta" -and -not (Has $rl[2] "t_limpo"))
    Check "A: log - pedidos da casca em modo de teste" (LogHas "A" "pedidos 'voltz-teste-casca' ligados")
    Check "A: log - 4 tracos no anotacoes.jsonl" (LogHas "A" "[voltz] anotacoes: 4 traco(s) em anotacoes.jsonl")
    Check "A: log - recuperar projeto ok" (LogHas "A" "[voltz] recuperar projeto (ok")
    Check "A: log - sobreposicao destruida no EXIT" (LogHas "A" "[voltz] anotacoes: desligadas (sobreposicao destruida)")

    # ---------------- Fase B: grupo Tela Voltz versao 1 (sem anotacoes) -> 2 ----------------
    New-CleanConfig ""
    Set-TestEnv $false $false $true
    Check "B1: app aberto" (Start-App "B1")
    Check "B1: preset (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    $l1 = Screen-Layers $sceneScreen
    Check "B1: grupo versao 1 (Tela + Voltz Mouse)" ($l1.Count -eq 2 -and $l1[1].inputKind -eq "voltz_mouse_source")
    Check "B1: encerrou limpo" (Close-App "B1")
    Set-TestEnv $false $false $false
    Check "B2: app aberto" (Start-App "B2")
    $l2 = Screen-Layers $sceneScreen
    Check "B2: grupo ganhou a Voltz Anotacoes no meio, travada em (0,0)" ($l2.Count -eq 3 -and $l2[0].sourceName -eq "Tela" -and $l2[1].inputKind -eq "voltz_annotations_source" -and $l2[2].inputKind -eq "voltz_mouse_source" -and $l2[1].sceneItemLocked -and $l2[1].sceneItemTransform.positionX -eq 0 -and $l2[1].sceneItemTransform.positionY -eq 0)
    Program-Scene
    # Tela de cor (conferido aqui) e sem retangulo de teste: o app nao tem onde por a sobreposicao
    Assert-NoRealCapture
    [void](VendorShell "InjectStroke" (Stroke-Json "linha" "#D4F53C" 16 "[[640,720],[1920,720]]"))
    $st = Shell-State; Assert-OverlaySafe "B2 linha"
    $img = Scene-Image $sceneScreen; $on = $img.GetPixel(320, 180); $img.Dispose()
    Check "B2: a camada migrada desenha ($on)" (Near $on 212 245 60)
    Check "B2: Tela de cor e sem retangulo de teste: nenhuma sobreposicao" ((-not $st.sobreposicao.criada) -and ((Overlay-Windows $script:proc.Id).Count -eq 0))
    Check "B2: encerrou limpo" (Close-App "B2")
    Check "B2: log - 1 grupo com a camada de anotacoes" (LogHas "B2" "[voltz] tela voltz: 1 grupo(s) com a camada de anotacoes")
    Check "B2: log - sem captura de monitor, sem sobreposicao" (LogHas "B2" "[voltz] anotacoes: sem captura de monitor; sem sobreposicao")
    Check "B3: app aberto" (Start-App "B3")
    $l3 = Screen-Layers $sceneScreen
    Check "B3: grupo intacto (3 camadas)" ($l3.Count -eq 3)
    Check "B3: encerrou limpo" (Close-App "B3")
    Check "B3: nao migra de novo" (-not (LogHas "B3" "[voltz] tela voltz:"))

    # ---------------- Fase C: queimadas (simula Windows < 10 2004) ----------------
    New-CleanConfig ("NextProjectName=Smoke anotacoes C`r`n")
    Set-TestEnv $true $true $false
    Check "C: app aberto" (Start-App "C")
    Check "C: preset (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    Program-Scene
    [void](VendorShell "InjectStroke" (Stroke-Json "linha" "#D4F53C" 16 "[[640,720],[1920,720]]"))
    $st = Shell-State; Assert-OverlaySafe "C linha"; $feed = Feed-State
    $img = Scene-Image $sceneScreen; $on = $img.GetPixel(320, 180); $img.Dispose()
    $hwnd = [IntPtr][Int64]$st.sobreposicao.hwnd
    [uint32]$aff = 99; [void]$P::GetWindowDisplayAffinity($hwnd, [ref]$aff)
    Check "C: queimadas na casca e no plugin; a camada nao desenha ($on)" ($st.queimadas -and $feed.queimadas -and (Near $on 42 42 48))
    Check "C: sobreposicao sem afinidade (aparece na captura) e visivel (afinidade $aff)" ($aff -eq 0 -and $st.sobreposicao.visivel)
    Prepare-Recording
    Assert-NoRealCapture
    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "C: StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 3
    Assert-OverlaySafe "C gravando"
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "C: StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 5
    Assert-OverlaySafe "C parado"
    Check "C: encerrou limpo" (Close-App "C")
    $dirs = Project-Dir "*Smoke anotacoes C*"
    if ($dirs.Count -eq 1) {
        $m = Read-JsonFile (Join-Path $dirs[0].FullName "projeto.voltz")
        $lines = Read-Jsonl (Join-Path $dirs[0].FullName "anotacoes.jsonl")
        Check "C: manifesto com anotacoes_queimadas e a linha de antes da gravacao no anotacoes.jsonl" ($m.anotacoes_queimadas -eq $true -and $lines.Count -eq 1 -and $lines[0].ferr -eq "linha" -and $lines[0].t_ini -eq 0)
    } else {
        Check "C: uma pasta de projeto" $false
    }
    Check "C: log - queimadas (teste)" (LogHas "C" "[voltz] anotacoes: queimadas (teste)")

    # ---------------- Fase D: somem apos 1 s, gravando ----------------
    New-CleanConfig ("NextProjectName=Smoke anotacoes D`r`n" + 'AnnotationStyle={"somem_apos_s":1}' + "`r`n")
    Set-TestEnv $true $false $false
    Check "D: app aberto" (Start-App "D")
    Check "D: preset (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    Prepare-Recording
    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "D: StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 1
    [void](VendorShell "InjectStroke" (Stroke-Json "seta" "#D4F53C" 8 "[[300,300],[900,600]]"))
    $st1 = Shell-State; Assert-OverlaySafe "D seta"
    Start-Sleep -Seconds 2
    $st2 = Shell-State; Assert-OverlaySafe "D depois de sumir"; $feed = Feed-State
    Check "D: o traco some sozinho (antes $(@($st1.visiveis).Count), depois $(@($st2.visiveis).Count), plugin $($feed.tracos))" (@($st1.visiveis).Count -eq 1 -and @($st2.visiveis).Count -eq 0 -and $feed.tracos -eq 0 -and -not $st2.sobreposicao.visivel)
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "D: StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 5
    Check "D: encerrou limpo" (Close-App "D")
    $dirs = Project-Dir "*Smoke anotacoes D*"
    if ($dirs.Count -eq 1) {
        $lines = Read-Jsonl (Join-Path $dirs[0].FullName "anotacoes.jsonl")
        $gap = if ($lines.Count -eq 1) { $lines[0].t_limpo - $lines[0].t_fim } else { -1 }
        Check "D: t_limpo = t_fim + 1000 ($gap)" ($lines.Count -eq 1 -and $gap -eq 1000)
    } else {
        Check "D: uma pasta de projeto" $false
    }
    Check "D: log - somem apos 1 s" (LogHas "D" "[voltz] anotacoes: somem apos 1 s")
} catch {
    Check "sem excecao ($($_.Exception.Message))" $false
} finally {
    if ($script:proc -and -not $script:proc.HasExited) { [void](Close-App "final") }
    if ($script:job) { Stop-Watchdog "final" }
    Remove-Item Env:VOLTZ_TEST_LIVE, Env:VOLTZ_TEST_PRESET_STANDINS, Env:VOLTZ_TEST_OVERLAY_RECT, Env:VOLTZ_TEST_ANNOT_BURNED, Env:VOLTZ_TEST_SCREEN_LAYERS -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 5
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    if (Test-Path $bak) { Move-Item $bak $cfg }
    if (-not $Keep -and (Test-Path -LiteralPath $OutDir)) { Remove-Item -Recurse -Force -LiteralPath $OutDir; Write-Host "apagado: $OutDir" }
    if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) {
        Remove-Item -Force -LiteralPath $voltzVideos
        Write-Host "apagado (vazio): $voltzVideos"
    }
}

$orphans = @($script:overlays | Where-Object { $P::IsWindow([IntPtr]$_) })
Check "nenhuma sobreposicao orfa ($($script:overlays.Count) vista(s))" ($script:overlays.Count -ge 1 -and $orphans.Count -eq 0)
Check "seguranca: $($script:asserts) Assert-OverlaySafe, $($script:forced) fechamento(s) forcado(s)" ($script:asserts -gt 0 -and $script:forced -eq 0)
$videosAfter = Videos-List
Write-Host "Videos depois ($($videosAfter.Count)): $(($videosAfter | ForEach-Object { $_.Split('|')[0] }) -join ', ')"
Check "Videos intacta (antes $($videosBefore.Count), depois $($videosAfter.Count))" (-not (Compare-Object $videosBefore $videosAfter))
$leftBaks = @(Get-ChildItem -LiteralPath $rundir -Directory -Force -Filter "config.*bak*" -ErrorAction SilentlyContinue)
Check "config portatil restaurada igual e sem sobra de backup" (-not (Test-Path $bak) -and $leftBaks.Count -eq 0 -and ($cfgHashBefore -eq (Hash-Dir $cfg)))
$failed = 0
foreach ($k in $checks.Keys) {
    if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
}
Write-Host "logs: $logDir"
if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE ANOTACOES OK"
