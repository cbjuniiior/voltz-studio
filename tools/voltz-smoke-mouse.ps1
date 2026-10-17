<#
  Teste de integracao do Plano 4A (destaque do mouse, plugin voltz-sources, grupo "Tela Voltz").
  Fase A: config limpa com VOLTZ_TEST_LIVE=1 (pedidos "voltz-teste" no obs-websocket; o mouse real nunca e lido) e
  VOLTZ_TEST_PRESET_STANDINS=1 (o preset cria Tela 2560x1440 e Camera 1280x720 de COR: nenhuma captura de tela nem
  webcam). Aplica o preset "So tela / So camera" por UI Automation, confere o grupo "Tela Voltz" (Tela + Voltz Anotacoes + Voltz Mouse),
  injeta o mouse e confere o desenho por GetSourceScreenshot da cena sintetica (halo lima no centro, holofote
  escurecendo o canto, onda do clique); grava ~6 s alternando o holofote pelo atalho e valida o projeto (mouse no
  manifesto, eventos do holofote, tela.mkv 2560x1440 da captura e nao do grupo).
  Fase B: colecao "de antes do 4A" (Tela direto numa cena, com posicao/caixa/trava) -> reabre e confere a migracao
  para o grupo no mesmo lugar; reabre de novo e confere que nao migra outra vez.
  Seguranca: aborta se existir monitor_capture/window_capture/game_capture/dshow_input; nao move o mouse, nao clica,
  nao digita; so metadados com ffprobe; config portatil guardada e restaurada; -OutDir apagado no fim.
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-mouse.ps1 [-OutDir <pasta>] [-Keep]
#>
param(
    [string]$Config = "RelWithDebInfo",
    [string]$OutDir = (Join-Path $env:TEMP "voltz-smoke-mouse"),
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
$testStyle = '{"halo":{"ativo":true,"cor":"#D4F53C","tamanho":200,"opacidade":1.0,"forma":"cheio"},"clique":{"ativo":true,"cor":"#D4F53C","cor_direito":"#4DA3FF"},"holofote":{"ativo":false,"raio":220,"escurecer":0.55}}'

$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.smokemouse-bak"
$voltzCfg = Join-Path $cfg "Voltz Studio"
$logDir = Join-Path $env:TEMP "voltz-smoke-mouse-logs"
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"
$hadVoltzVideos = Test-Path -LiteralPath $voltzVideos
$videosDir = [Environment]::GetFolderPath('MyVideos')
$videosBefore = @(Get-ChildItem -Force -LiteralPath $videosDir | ForEach-Object { "$($_.Name)|$($_.LastWriteTimeUtc.Ticks)|$($_.Attributes)" })

if ((Test-Path -LiteralPath $OutDir) -and @(Get-ChildItem -Force -LiteralPath $OutDir).Count -gt 0) {
    Write-Host "FALHOU -OutDir ja existe e nao esta vazio: $OutDir"; exit 1
}
if (Test-Path $bak) { Write-Host "FALHOU backup anterior em $bak (restaure ou apague antes de rodar)"; exit 1 }

Add-Type -AssemblyName System.Drawing
$checks = [ordered]@{}
function Check([string]$name, [bool]$ok) { $script:checks[$name] = $ok }

function Ws([string]$json) {
    $file = Join-Path $env:TEMP "voltz-smoke-mouse-req.json"
    [IO.File]::WriteAllText($file, $json, (New-Object Text.UTF8Encoding $false))
    $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $file
    Remove-Item -Force $file -ErrorAction SilentlyContinue
    # a virgula preserva o array mesmo com 1 elemento
    return ,@($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json })
}

function Uia([string]$name) {
    $result = & powershell -ExecutionPolicy Bypass -File $uia -ProcessId $script:proc.Id -Name $name
    Write-Host $result
    return ($LASTEXITCODE -eq 0)
}

function Vendor([string]$type, [string]$dataJson = '{}') {
    $r = Ws ('[{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste","requestType":"' + $type + '","requestData":' + $dataJson + '}}]')
    if ($r.Count -lt 1 -or -not $r[0].ok) { throw "pedido de teste $type falhou" }
    return $r[0].data.responseData
}

# Seguranca: nenhuma captura real (tela, janela, jogo, webcam) pode existir no app
function Assert-NoRealCapture {
    $inputs = @((Ws '[{"requestType":"GetInputList"}]')[0].data.inputs)
    $real = @($inputs | Where-Object { $_.inputKind -match 'monitor_capture|window_capture|game_capture|dshow_input' })
    if ($real.Count -gt 0) { throw ("captura real presente - abortando: " + (($real | ForEach-Object { $_.inputName }) -join ', ')) }
}

# Render de cena SINTETICA (cor + Voltz Mouse) em 640x360 - nunca a tela real nem quadro de video
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

function Read-JsonFile([string]$path) {
    return (Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json) | ForEach-Object { $_ }
}

function New-CleanConfig([string]$voltzLines) {
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
    New-Item -ItemType Directory -Force $wsDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}')
    [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n[Voltz]`r`n" + $voltzLines, (New-Object Text.UTF8Encoding $false))
}

# user.ini [Voltz] com o app FECHADO
function Set-VoltzIni([string]$key, [string]$value) {
    $path = Join-Path $voltzCfg "user.ini"
    $lines = [System.Collections.Generic.List[string]]@(Get-Content -LiteralPath $path -Encoding UTF8)
    $start = $lines.IndexOf("[Voltz]")
    if ($start -lt 0) { $lines.Add("[Voltz]"); $start = $lines.Count - 1 }
    $end = $lines.Count
    for ($i = $start + 1; $i -lt $lines.Count; $i++) { if ($lines[$i].StartsWith("[")) { $end = $i; break } }
    $done = $false
    for ($i = $start + 1; $i -lt $end; $i++) { if ($lines[$i].StartsWith("$key=")) { $lines[$i] = "$key=$value"; $done = $true } }
    if (-not $done) { $lines.Insert($end, "$key=$value") }
    [IO.File]::WriteAllLines($path, $lines, (New-Object Text.UTF8Encoding $false))
}

function Start-App {
    $script:proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
    Start-Sleep -Seconds 15
    return (-not $script:proc.HasExited)
}

# Fecha o app lancado aqui (so pelo PID) e guarda o log desta execucao como <tag>.txt
function Close-App([string]$tag) {
    $clean = $false
    if ($script:proc -and -not $script:proc.HasExited) {
        $script:proc.Refresh()
        [void]$script:proc.CloseMainWindow()
        $clean = $script:proc.WaitForExit(30000)
        if (-not $clean) { Stop-Process -Id $script:proc.Id -Force }
    }
    $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
    if ($log) { Copy-Item $log.FullName (Join-Path $logDir "$tag.txt") -Force }
    return $clean
}

function LogHas([string]$tag, [string]$text) {
    $file = Join-Path $logDir "$tag.txt"
    return [bool]((Test-Path $file) -and (Select-String -LiteralPath $file -SimpleMatch $text -Encoding UTF8 -Quiet))
}

$ffprobe = (Get-Command ffprobe -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $ffprobe) {
    $ffprobe = Get-ChildItem (Join-Path $root ".deps") -Filter ffprobe.exe -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}

if (Test-Path $cfg) { Move-Item $cfg $bak }
if (Test-Path $logDir) { Remove-Item -Recurse -Force $logDir }
New-Item -ItemType Directory -Force $logDir | Out-Null
$proc = $null
try {
    # ---------------- Fase A: preset com substitutas, desenho e gravacao ----------------
    New-CleanConfig ("MouseStyle=$testStyle`r`nNextProjectName=Smoke mouse`r`n")
    New-Item -ItemType Directory -Force $OutDir | Out-Null
    $env:VOLTZ_TEST_LIVE = "1"
    $env:VOLTZ_TEST_PRESET_STANDINS = "1"
    Check "A: app aberto" (Start-App)
    Check "A: preset 'So tela / So camera' (UI Automation)" (Uia $presetScreen)
    Start-Sleep -Seconds 3
    Assert-NoRealCapture
    # o nome do grupo varia ("Tela Voltz", "Tela Voltz 2"...) conforme a ordem das cenas: acha-se pelo item da cena
    $top = @((Ws ('[{"requestType":"GetSceneItemList","requestData":{"sceneName":' + (ConvertTo-Json $sceneScreen) + '}}]'))[0].data.sceneItems)
    $groupName = if ($top.Count -ge 1) { [string]$top[0].sourceName } else { "" }
    $inner = @()
    if ($groupName) { $inner = @((Ws ('[{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":' + (ConvertTo-Json $groupName) + '}}]'))[0].data.sceneItems) }
    Check "A: cena 'So tela' = um grupo 'Tela Voltz*' ($groupName)" ($top.Count -eq 1 -and $top[0].isGroup -and $groupName -like "Tela Voltz*")
    Check "A: grupo = Tela (embaixo) + Voltz Anotacoes + Voltz Mouse, travadas em (0,0)" ($inner.Count -eq 3 -and $inner[0].sourceName -eq "Tela" -and $inner[1].inputKind -eq "voltz_annotations_source" -and $inner[2].sourceName -eq "Voltz Mouse" -and $inner[2].inputKind -eq "voltz_mouse_source" -and $inner[0].sceneItemLocked -and $inner[1].sceneItemLocked -and $inner[2].sceneItemLocked -and $inner[2].sceneItemTransform.positionX -eq 0)
    [void](Ws ('[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":' + (ConvertTo-Json $sceneScreen) + '}}]'))
    Start-Sleep -Seconds 1
    $state = Vendor "MouseState"
    $style = $state.style | ConvertFrom-Json
    Check "A: Voltz Mouse mostrada ($($state.showing)) com o estilo do user.ini (tamanho $($style.halo.tamanho))" ($state.showing -ge 1 -and $style.halo.tamanho -eq 200)
    # Tela 2560x1440 escalada 0,75 para 1920x1080: pixel (1280,720) da captura = centro da imagem 640x360
    [void](Vendor "InjectMouse" '{"x":1280,"y":720}')
    Start-Sleep -Milliseconds 300
    $img = Scene-Image $sceneScreen; $center = $img.GetPixel(320, 180); $corner = $img.GetPixel(40, 40); $img.Dispose()
    Check "A: halo lima no centro ($center)" (Near $center 212 245 60)
    Check "A: canto = Tela ($corner)" (Near $corner 42 42 48)
    $r = Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.ToggleSpotlight"}}]'
    Start-Sleep -Seconds 1
    $img = Scene-Image $sceneScreen; $dark = $img.GetPixel(40, 40); $img.Dispose()
    Check "A: atalho liga o holofote e escurece o canto ($dark)" ($r.Count -eq 1 -and $r[0].ok -and $dark.R -le $corner.R - 8)
    # clique + estado na MESMA conexao (cada Ws abre um powershell: ~1 s, mais que os 450 ms da onda)
    $r = Ws '[{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste","requestType":"InjectMouse","requestData":{"x":1280,"y":720,"click":"l"}}},{"requestType":"CallVendorRequest","requestData":{"vendorName":"voltz-teste","requestType":"MouseState","requestData":{}}}]'
    $r1 = if ($r.Count -eq 2) { $r[1].data.responseData.ripples } else { -1 }
    Start-Sleep -Seconds 1
    $r2 = (Vendor "MouseState").ripples
    Check "A: onda do clique nasce e some ($r1 -> $r2)" ($r1 -eq 1 -and $r2 -eq 0)

    # Gravacao: pasta = -OutDir, audio mutado; o holofote esta ligado e o atalho o desliga e religa
    $r = Ws ('[{"requestType":"SetProfileParameter","requestData":{"parameterCategory":"SimpleOutput","parameterName":"FilePath","parameterValue":' + (ConvertTo-Json $OutDir) + '}},{"requestType":"GetSpecialInputs"}]')
    $special = $r[1].data
    $mutes = @()
    foreach ($n in @($special.desktop1, $special.desktop2, $special.mic1, $special.mic2, $special.mic3, $special.mic4)) {
        if ($n) { $mutes += @{ requestType = "SetInputMute"; requestData = @{ inputName = $n; inputMuted = $true } } }
    }
    if ($mutes.Count -gt 0) { [void](Ws (ConvertTo-Json -InputObject $mutes -Depth 5 -Compress)) }
    Assert-NoRealCapture
    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "A: StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 2
    [void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.ToggleSpotlight"}}]')
    Start-Sleep -Seconds 2
    [void](Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.ToggleSpotlight"}}]')
    Start-Sleep -Seconds 2
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "A: StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 5
    Check "A: encerrou limpo" (Close-App "A")
    Remove-Item Env:VOLTZ_TEST_PRESET_STANDINS -ErrorAction SilentlyContinue

    $projects = @(Get-ChildItem -LiteralPath $OutDir -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz") })
    Check "A: uma pasta de projeto" ($projects.Count -eq 1)
    if ($projects.Count -eq 1) {
        $dir = $projects[0].FullName
        $m = Read-JsonFile (Join-Path $dir "projeto.voltz")
        $spot = @(@($m.eventos) | Where-Object { $_.tipo -like "holofote_*" } | ForEach-Object { $_.tipo })
        Check "A: manifesto ok com mouse (tamanho $($m.mouse.halo.tamanho), holofote no inicio $($m.mouse.holofote.ativo))" ($m.status -eq "ok" -and $m.mouse.halo.tamanho -eq 200 -and $m.mouse.holofote.ativo -eq $true)
        Check "A: eventos holofote_desligado, holofote_ligado ($($spot -join ', '))" ($spot.Count -eq 2 -and $spot[0] -eq "holofote_desligado" -and $spot[1] -eq "holofote_ligado")
        Check "A: manifesto tela 2560x1440 (a captura, nao o grupo)" ($m.arquivos.tela -and $m.arquivos.tela.largura -eq 2560 -and $m.arquivos.tela.altura -eq 1440)
        Check "A: nenhum iso_falhou" (@(@($m.eventos) | Where-Object { $_.tipo -eq "iso_falhou" }).Count -eq 0)
        $tela = Join-Path $dir "tela.mkv"
        if ($ffprobe -and (Test-Path -LiteralPath $tela)) {
            # so metadados (nunca quadros)
            $ip = (& $ffprobe -v error -show_entries "stream=codec_type,width,height" -of json $tela) | Out-String | ConvertFrom-Json
            $v = @($ip.streams | Where-Object { $_.codec_type -eq "video" })
            Check "A: ffprobe tela.mkv 2560x1440" ($v.Count -eq 1 -and $v[0].width -eq 2560 -and $v[0].height -eq 1440)
        } else {
            Write-Host "AVISO ffprobe ou tela.mkv ausente: resolucao do ISO conferida so pelo manifesto"
        }
    }
    Check "A: log - ISO grava a captura 'Tela' (color_source)" (LogHas "A" "[voltz] iso tela: fonte 'Tela' (color_source)")
    Check "A: log - plugin carregado em modo de teste" ((LogHas "A" "[voltz-sources] carregado") -and (LogHas "A" "[voltz-sources] modo de teste"))
    Check "A: log - mouse real nunca lido" (-not (LogHas "A" "[voltz] mouse ao vivo: ligado"))

    # ---------------- Fase B: migracao de uma colecao de antes do 4A ----------------
    New-CleanConfig ("MouseStyle=$testStyle`r`n")
    Check "B1: app aberto" (Start-App)
    [void](Ws ('[{"requestType":"CreateScene","requestData":{"sceneName":"Antiga"}},' +
        '{"requestType":"CreateInput","requestData":{"sceneName":"Antiga","inputName":"Tela","inputKind":"color_source_v3","inputSettings":{"color":4281346602,"width":2560,"height":1440}}},' +
        '{"requestType":"CreateInput","requestData":{"sceneName":"Antiga","inputName":"Logo","inputKind":"color_source_v3","inputSettings":{"color":4278255360,"width":200,"height":200}}},' +
        '{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Antiga"}}]'))
    $telaId = (Ws '[{"requestType":"GetSceneItemId","requestData":{"sceneName":"Antiga","sourceName":"Tela"}}]')[0].data.sceneItemId
    [void](Ws ('[{"requestType":"SetSceneItemTransform","requestData":{"sceneName":"Antiga","sceneItemId":' + $telaId + ',"sceneItemTransform":{"positionX":100,"positionY":50,"boundsType":"OBS_BOUNDS_SCALE_INNER","boundsWidth":1600,"boundsHeight":900}}},' +
        '{"requestType":"SetSceneItemLocked","requestData":{"sceneName":"Antiga","sceneItemId":' + $telaId + ',"sceneItemLocked":true}}]'))
    $uuid = (@((Ws '[{"requestType":"GetInputList"}]')[0].data.inputs) | Where-Object { $_.inputName -eq "Tela" }).inputUuid
    Check "B1: Tela criada ($uuid)" ([bool]$uuid)
    Check "B1: encerrou limpo" (Close-App "B1")
    Set-VoltzIni "ScreenSourceUuid" $uuid

    Check "B2: app aberto" (Start-App)
    $items = @((Ws '[{"requestType":"GetSceneItemList","requestData":{"sceneName":"Antiga"}}]')[0].data.sceneItems)
    $gname = if ($items.Count -ge 1) { [string]$items[0].sourceName } else { "" }
    $inner = @()
    if ($gname) { $inner = @((Ws ('[{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":' + (ConvertTo-Json $gname) + '}}]'))[0].data.sceneItems) }
    Check "B2: Tela virou grupo no mesmo lugar (100,50; caixa 1600x900; travado; embaixo do Logo)" ($items.Count -eq 2 -and $items[0].sourceName -eq "Tela Voltz" -and $items[0].isGroup -and $items[0].sceneItemLocked -and $items[0].sceneItemTransform.positionX -eq 100 -and $items[0].sceneItemTransform.positionY -eq 50 -and $items[0].sceneItemTransform.boundsWidth -eq 1600 -and $items[0].sceneItemTransform.boundsHeight -eq 900 -and $items[1].sourceName -eq "Logo")
    Check "B2: grupo com Tela + Voltz Anotacoes + Voltz Mouse" ($inner.Count -eq 3 -and $inner[0].sourceName -eq "Tela" -and $inner[1].inputKind -eq "voltz_annotations_source" -and $inner[2].sourceName -eq "Voltz Mouse")
    # grupo em (100,50) com escala 0,625: pixel (1280,720) da captura = (900,500) no canvas = (300,167) na imagem
    [void](Vendor "InjectMouse" '{"x":1280,"y":720}')
    Start-Sleep -Milliseconds 300
    $img = Scene-Image "Antiga"; $p = $img.GetPixel(300, 167); $img.Dispose()
    Check "B2: halo no centro da Tela migrada ($p)" (Near $p 212 245 60)
    Check "B2: encerrou limpo" (Close-App "B2")
    Check "B2: log - 1 item migrado" (LogHas "B2" "[voltz] tela voltz: 1 item(ns) migrado(s)")

    Check "B3: app aberto" (Start-App)
    $i3 = @((Ws '[{"requestType":"GetSceneItemList","requestData":{"sceneName":"Antiga"}}]')[0].data.sceneItems)
    $g3 = if ($i3.Count -ge 1) { [string]$i3[0].sourceName } else { "" }
    $r = Ws ('[{"requestType":"GetGroupSceneItemList","requestData":{"sceneName":' + (ConvertTo-Json $g3) + '}}]')
    Check "B3: cena com 2 itens e grupo intacto (3 camadas)" ($i3.Count -eq 2 -and $r.Count -eq 1 -and @($r[0].data.sceneItems).Count -eq 3)
    Check "B3: encerrou limpo" (Close-App "B3")
    Check "B3: nao migra de novo" (-not (LogHas "B3" "[voltz] tela voltz:"))
} catch {
    Check "sem excecao ($($_.Exception.Message))" $false
} finally {
    if ($proc -and -not $proc.HasExited) {
        $proc.Refresh()
        [void]$proc.CloseMainWindow()
        if (-not $proc.WaitForExit(30000)) { Stop-Process -Id $proc.Id -Force }
    }
    Remove-Item Env:VOLTZ_TEST_LIVE, Env:VOLTZ_TEST_PRESET_STANDINS -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 5
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    if (Test-Path $bak) { Move-Item $bak $cfg }
    if (-not $Keep -and (Test-Path -LiteralPath $OutDir)) { Remove-Item -Recurse -Force -LiteralPath $OutDir; Write-Host "apagado: $OutDir" }
    if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) {
        Remove-Item -Force -LiteralPath $voltzVideos
        Write-Host "apagado (vazio): $voltzVideos"
    }
}

$videosAfter = @(Get-ChildItem -Force -LiteralPath $videosDir | ForEach-Object { "$($_.Name)|$($_.LastWriteTimeUtc.Ticks)|$($_.Attributes)" })
Check "Videos intacta (antes $($videosBefore.Count), depois $($videosAfter.Count))" (-not (Compare-Object $videosBefore $videosAfter))
Check "config portatil restaurada sem sobra de backup" (-not (Test-Path $bak))
$failed = 0
foreach ($k in $checks.Keys) {
    if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
}
Write-Host "logs: $logDir"
if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE MOUSE OK"
