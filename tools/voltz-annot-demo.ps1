<#
  Demonstracao MANUAL das anotacoes do Plano 4B: o usuario roda e OLHA (nao e teste automatico).
  -Launch: guarda a config portatil (config.annotdemo-bak), cria uma limpa com o obs-websocket ligado (porta 4455, sem
    senha) e abre o Voltz portatil com VOLTZ_TEST_LIVE=1 (os tracos entram pelos pedidos de teste; o halo do mouse nao
    acompanha o mouse real nesse modo). Na tela Presets, aplique VOCE MESMO um preset com a Tela real (ex.: Tutorial).
  -Action desenhar: seta, retangulo, elipse, caneta (onda) e marca-texto, em pixels da captura da Tela.
  -Action desfazer | limpar | estado.
  -Close: fecha o Voltz aberto pelo -Launch (pelo PID salvo, com WM_CLOSE na janela da casca) e restaura a config
    portatil - ou apaga a da demonstracao (obs-websocket sem senha), se nao havia config portatil antes do -Launch.
  O desenho aparece por cima do monitor capturado (janela transparente a cliques: clique e digite normalmente por baixo
  dela) e no programa (fonte Voltz Anotacoes); a captura da Tela NAO o contem.
  Seguranca: este script so INJETA tracos pelos pedidos "voltz-teste-casca" (InjectStroke, UndoStroke, ClearStrokes,
  AnnotationState - nenhum outro pedido e aceito) e recusa desenhar se o app disser que esta em modo desenho. Nao move
  o mouse, nao clica, nao digita, nao captura entrada e so fala com o Voltz que o -Launch abriu (PID e porta conferidos).
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-annot-demo.ps1 -Launch
       powershell -ExecutionPolicy Bypass -File tools/voltz-annot-demo.ps1 -Action desenhar
       powershell -ExecutionPolicy Bypass -File tools/voltz-annot-demo.ps1 -Close
#>
param(
    [switch]$Launch,
    [ValidateSet("", "desenhar", "desfazer", "limpar", "estado")][string]$Action = "",
    [switch]$Close,
    [string]$Config = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rundir = Join-Path $root "build_x64\rundir\$Config"
$bin = Join-Path $rundir "bin\64bit"
$exe = Join-Path $bin "VoltzStudio.exe"
$ws = Join-Path $PSScriptRoot "voltz-ws.ps1"
$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.annotdemo-bak"
$pidFile = Join-Path $env:TEMP "voltz-annot-demo.pid"
$marker = Join-Path $cfg "voltz-annot-demo.marker"
$wsPort = 4455

# P/Invoke de leitura; a unica escrita e PostMessage(WM_CLOSE) na janela da casca do PID aberto pelo -Launch
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class VoltzDemoWin {
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder sb, int n);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
 public static string ClassOf(IntPtr h) { var sb = new System.Text.StringBuilder(256); GetClassName(h, sb, 256); return sb.ToString(); }
 public static uint OwnerOf(IntPtr h) { uint owner; GetWindowThreadProcessId(h, out owner); return owner; } }
'@
$Win = [VoltzDemoWin]

function Port-Owners { return ,@(Get-NetTCPConnection -LocalPort $wsPort -State Listen -ErrorAction SilentlyContinue | ForEach-Object { [int]$_.OwningProcess }) }

# PID e janela da casca salvos pelo -Launch; o processo precisa ser o VoltzStudio.exe deste build
function Launched {
    if (-not (Test-Path -LiteralPath $pidFile)) { return $null }
    $parts = ([string](Get-Content -LiteralPath $pidFile -TotalCount 1)).Split('|')
    $id = [int]$parts[0]
    $hwnd = if ($parts.Count -gt 1) { [IntPtr][Int64]$parts[1] } else { [IntPtr]::Zero }
    $p = Get-Process -Id $id -ErrorAction SilentlyContinue
    if (-not $p -or $p.HasExited -or $p.ProcessName -ne "VoltzStudio" -or $p.Path -ne $exe) { return $null }
    return @{ Process = $p; Shell = $hwnd }
}

function Ws([string]$json) {
    $file = Join-Path $env:TEMP "voltz-annot-demo-req.json"
    [IO.File]::WriteAllText($file, $json, (New-Object Text.UTF8Encoding $false))
    $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $file -Port $wsPort
    Remove-Item -Force $file -ErrorAction SilentlyContinue
    return ,@($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json })
}

# So estes pedidos da casca: nenhum deles le ou captura a entrada do usuario
$allowed = @("InjectStroke", "UndoStroke", "ClearStrokes", "AnnotationState")
function Shell([string]$type, [string]$dataJson = '{}') {
    if ($allowed -notcontains $type) { throw "pedido '$type' nao permitido na demonstracao" }
    $app = Launched
    if (-not $app) { throw "o Voltz desta demonstracao nao esta aberto (rode -Launch antes)" }
    if (-not ((Port-Owners) -contains $app.Process.Id)) { throw "a porta $wsPort nao e do Voltz aberto pelo -Launch (PID $($app.Process.Id))" }
    $r = Ws ('[{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste-casca","requestType":"' + $type + '","requestData":' + $dataJson + '}}]')
    if ($r.Count -lt 1 -or -not $r[0].ok) { throw "pedido $type falhou (o Voltz foi aberto com -Launch?)" }
    return $r[0].data.responseData
}

function Stroke([string]$tool, [string]$color, [int]$width, $points) {
    $pts = "[" + (($points | ForEach-Object { "[{0},{1}]" -f [int]$_[0], [int]$_[1] }) -join ",") + "]"
    $req = [ordered]@{ ferr = $tool; cor = $color; esp = $width; pts = $pts }
    [void](Shell "InjectStroke" (ConvertTo-Json $req -Compress))
}

function Restore-Config {
    Start-Sleep -Seconds 5
    if (Test-Path $bak) {
        if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
        Move-Item $bak $cfg
    } elseif (Test-Path -LiteralPath $marker) {
        # nao havia config portatil antes do -Launch: a da demonstracao (websocket sem senha na 4455) sai inteira
        Remove-Item -Recurse -Force $cfg
    }
}

if ($Launch) {
    if (-not (Test-Path $exe)) { throw "executavel nao encontrado: $exe" }
    if (Test-Path $bak) { throw "ja existe $bak (rode -Close antes)" }
    # backup pendente de outro script (smoke, demonstracao) na pasta da config portatil: nada e mexido
    $oldBaks = @(Get-ChildItem -LiteralPath $rundir -Force -Filter "config.*bak*" -ErrorAction SilentlyContinue)
    if ($oldBaks.Count -gt 0) { throw "backup de config pendente em $rundir ($(($oldBaks | ForEach-Object { $_.Name }) -join ', ')): restaure ou apague antes" }
    if (Launched) { throw "a demonstracao ja esta aberta (rode -Close antes)" }
    if ((Port-Owners).Count -gt 0) { throw "a porta $wsPort ja esta em uso (feche o OBS/Voltz que a usa antes)" }
    if (Test-Path $cfg) { Move-Item $cfg $bak }
    $hadLive = Test-Path Env:VOLTZ_TEST_LIVE
    try {
        $voltzCfg = Join-Path $cfg "Voltz Studio"
        $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
        New-Item -ItemType Directory -Force $wsDir | Out-Null
        [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":' + $wsPort + '}')
        [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n", (New-Object Text.UTF8Encoding $false))
        # marca: esta config e da demonstracao (o -Close a apaga se nao havia config portatil antes)
        Set-Content -LiteralPath $marker -Value "config criada pelo -Launch da demonstracao das anotacoes"
        $env:VOLTZ_TEST_LIVE = "1"
        $proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
        if (-not $hadLive) { Remove-Item Env:VOLTZ_TEST_LIVE -ErrorAction SilentlyContinue }
        # janela da casca (ainda sem sobreposicao) para o WM_CLOSE do -Close
        $shell = [IntPtr]::Zero
        for ($i = 0; $i -lt 60 -and $shell -eq [IntPtr]::Zero -and -not $proc.HasExited; $i++) {
            Start-Sleep -Milliseconds 500
            $proc.Refresh()
            $h = $proc.MainWindowHandle
            if ($h -ne [IntPtr]::Zero -and $Win::OwnerOf($h) -eq $proc.Id -and $Win::ClassOf($h) -ne "VoltzAnnotationOverlay") { $shell = $h }
        }
        if ($proc.HasExited) { throw "o Voltz fechou logo depois de abrir" }
        Set-Content -LiteralPath $pidFile -Value ("{0}|{1}" -f $proc.Id, $shell.ToInt64())
    } catch {
        if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
        if (-not $hadLive) { Remove-Item Env:VOLTZ_TEST_LIVE -ErrorAction SilentlyContinue }
        Remove-Item -LiteralPath $pidFile -ErrorAction SilentlyContinue
        Restore-Config
        throw
    }
    Write-Host "Voltz aberto (PID $($proc.Id)). Aplique um preset com a Tela real e rode -Action desenhar."
    exit 0
}

if ($Close) {
    $app = Launched
    if ($app) {
        $p = $app.Process
        $shellOk = ($app.Shell -ne [IntPtr]::Zero) -and $Win::IsWindow($app.Shell) -and ($Win::OwnerOf($app.Shell) -eq $p.Id) -and ($Win::ClassOf($app.Shell) -ne "VoltzAnnotationOverlay")
        # Process.CloseMainWindow pode mirar a sobreposicao (que ignora WM_CLOSE): fecha pela janela da casca
        if ($shellOk) { [void]$Win::PostMessage($app.Shell, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) } else { [void]$p.CloseMainWindow() }
        if (-not $p.WaitForExit(30000)) { Write-Host "o Voltz nao fechou em 30 s: encerrando o PID $($p.Id)"; Stop-Process -Id $p.Id -Force }
    }
    Remove-Item -LiteralPath $pidFile -ErrorAction SilentlyContinue
    Restore-Config
    Write-Host "Voltz fechado e config portatil restaurada."
    exit 0
}

switch ($Action) {
    "desenhar" {
        $st = (Shell "AnnotationState").estado | ConvertFrom-Json
        if ($st.modo_desenho) { throw "o app esta em modo desenho: esta demonstracao so injeta tracos" }
        $w = [int]$st.captura[0]; $h = [int]$st.captura[1]
        if ($w -le 0) { throw "sem Tela compartilhada: aplique um preset primeiro" }
        $e = [Math]::Max(4, [int]($w / 320))
        Stroke "seta" "#D4F53C" $e @(@((0.12 * $w), (0.25 * $h)), @((0.32 * $w), (0.42 * $h)))
        Stroke "retangulo" "#4DA3FF" $e @(@((0.45 * $w), (0.18 * $h)), @((0.8 * $w), (0.42 * $h)))
        Stroke "elipse" "#FF4D4D" $e @(@((0.1 * $w), (0.55 * $h)), @((0.35 * $w), (0.85 * $h)))
        $wave = @()
        for ($i = 0; $i -le 60; $i++) { $wave += , @(((0.45 + 0.4 * $i / 60) * $w), ((0.75 + 0.05 * [Math]::Sin($i / 4.0)) * $h)) }
        Stroke "caneta" "#D4F53C" $e $wave
        Stroke "marca_texto" "#FFE14D" $e @(@((0.45 * $w), (0.6 * $h)), @((0.85 * $w), (0.6 * $h)))
        Write-Host "5 tracos desenhados (captura ${w}x${h}); a sobreposicao fica sobre o monitor da Tela."
    }
    "desfazer" { [void](Shell "UndoStroke"); Write-Host "ultimo traco desfeito" }
    "limpar" { [void](Shell "ClearStrokes"); Write-Host "tracos limpos" }
    "estado" { (Shell "AnnotationState").estado }
    default { Write-Host "use -Launch, -Action desenhar|desfazer|limpar|estado ou -Close" }
}
