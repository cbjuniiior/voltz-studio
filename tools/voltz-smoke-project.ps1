<#
  Teste de integracao da gravacao em projeto (Planos 3A e 3B).
  Abre o Voltz Studio em modo portatil com config limpa (a config portatil existente e guardada e restaurada),
  aplica os padroes Voltz ("Pular"), aponta a pasta de gravacoes para -OutDir e, via obs-websocket + UI Automation,
  grava ~12 s com troca de cena, 2 marcadores (botao "Marcador" e atalho VoltzStudio.AddMarker) e uma pausa.
  ISO (3B): as fontes de papel Tela/Camera sao trocadas por fontes substitutas de cor solida (2560x1440 e 1280x720,
  numa cena que nunca vai ao programa) via VOLTZ_TEST_ISO_SCREEN/VOLTZ_TEST_ISO_CAMERA - nunca a tela real nem a webcam.
  Cursor (3A): "Tela teste" = captura de monitor criada com o item de cena DESATIVADO numa cena que nunca vai ao
  programa (nao renderiza nada; so deixa o CursorTracker achar o monitor); nunca e fonte de papel do ISO.
  Antes de gravar confere por GetInputList/GetSceneItemList que nao ha captura de janela/jogo nem dshow e que toda
  captura de monitor tem todos os itens de cena desativados (GetSceneItemEnabled da "Tela teste"); aborta se nao.
  Fecha o app e valida a pasta do projeto: programa.mp4, projeto.voltz (status ok, pausa, duracoes, tela/camera),
  marcadores.json, cursor.jsonl e, com ffprobe (so metadados), 3 faixas de audio + capitulos no programa e
  resolucao, audio e duracao (+ offset) de tela.mkv/camera.mkv. Com -ProgramOnly ("Gravar so a versao montada")
  confere que nao ha ISO.
  Nao move o mouse nem digita. Audio global mutado. Apaga -OutDir no fim (use -Keep para inspecionar metadados).
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-project.ps1 [-OutDir <pasta>] [-Keep] [-ProgramOnly]
#>
param(
    [string]$Config = "RelWithDebInfo",
    [string]$OutDir = (Join-Path $env:TEMP "voltz-smoke-project"),
    [switch]$Keep,
    [switch]$ProgramOnly
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rundir = Join-Path $root "build_x64\rundir\$Config"
$bin = Join-Path $rundir "bin\64bit"
$exe = Join-Path $bin "VoltzStudio.exe"
$ws = Join-Path $PSScriptRoot "voltz-ws.ps1"
$uia = Join-Path $PSScriptRoot "voltz-uia.ps1"
if (-not (Test-Path $exe)) { Write-Host "FALHOU executavel nao encontrado: $exe"; exit 1 }

# A pasta padrao Videos\Voltz Studio pode ser criada pelo app ao aplicar os padroes; so e apagada se esta execucao a criou e ficou vazia
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"
$hadVoltzVideos = Test-Path -LiteralPath $voltzVideos

$checks = [ordered]@{}
function Check([string]$name, [bool]$ok) { $script:checks[$name] = $ok }

function Ws([string]$json) {
    $file = Join-Path $env:TEMP "voltz-smoke-project-req.json"
    [IO.File]::WriteAllText($file, $json, (New-Object Text.UTF8Encoding $false))
    $lines = & powershell -ExecutionPolicy Bypass -File $ws -RequestsFile $file
    Remove-Item -Force $file -ErrorAction SilentlyContinue
    # a virgula preserva o array mesmo com 1 elemento (senao .Count falha no PowerShell 5.1)
    return ,@($lines | Where-Object { $_ -like '{"index"*' } | ForEach-Object { $_ | ConvertFrom-Json })
}

function Uia([string]$name) {
    $result = & powershell -ExecutionPolicy Bypass -File $uia -ProcessId $script:proc.Id -Name $name
    Write-Host $result
    return ($LASTEXITCODE -eq 0)
}

function Read-JsonFile([string]$path) {
    return (Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json) | ForEach-Object { $_ }
}

# Seguranca: -OutDir so pode ser uma pasta inexistente ou vazia (o script a apaga no fim)
if ((Test-Path -LiteralPath $OutDir) -and @(Get-ChildItem -Force -LiteralPath $OutDir).Count -gt 0) {
    Write-Host "FALHOU -OutDir ja existe e nao esta vazio: $OutDir"
    exit 1
}

# Config portatil: guarda e cria uma limpa (websocket sem senha, pt-BR, sem confirmacao ao sair, nome do projeto)
$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.smokeproject-bak"
if (Test-Path $bak) { Write-Host "FALHOU backup anterior em $bak (restaure ou apague antes de rodar)"; exit 1 }
if (Test-Path $cfg) { Move-Item $cfg $bak }
$voltzCfg = Join-Path $cfg "Voltz Studio"
$logCopy = Join-Path $env:TEMP "voltz-smoke-project-log.txt"
$proc = $null
$completed = $false
# try/finally desde o backup: qualquer excecao ainda fecha o app lancado aqui (so pelo PID) e restaura a config
try {
    $wsDir = Join-Path $voltzCfg "plugin_config\obs-websocket"
    New-Item -ItemType Directory -Force $wsDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $wsDir "config.json"), '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}')
    $voltzIni = "[Voltz]`r`nNextProjectName=Smoke projeto`r`n"
    if ($ProgramOnly) { $voltzIni += "ProgramOnly=true`r`n" }
    [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n" + $voltzIni, (New-Object Text.UTF8Encoding $false))
    New-Item -ItemType Directory -Force $OutDir | Out-Null

    $ffprobe = (Get-Command ffprobe -ErrorAction SilentlyContinue | Select-Object -First 1).Source
    if (-not $ffprobe) {
        $ffprobe = Get-ChildItem (Join-Path $root ".deps") -Filter ffprobe.exe -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
    }

    if (Test-Path $logCopy) { Remove-Item -Force $logCopy }
    # ISO: fontes de papel = substitutas de cor solida (criadas abaixo); nunca a tela real nem a webcam
    $env:VOLTZ_TEST_ISO_SCREEN = "Tela substituta"
    $env:VOLTZ_TEST_ISO_CAMERA = "Camera substituta"
    $proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
    Start-Sleep -Seconds 15
    Check "app aberto" (-not $proc.HasExited)
    Check "Pular (padroes Voltz)" (Uia "Pular")
    Start-Sleep -Seconds 2

    $r = Ws ('[{"requestType":"SetProfileParameter","requestData":{"parameterCategory":"SimpleOutput","parameterName":"FilePath","parameterValue":' + (ConvertTo-Json $OutDir) + '}},' +
        '{"requestType":"GetSpecialInputs"},{"requestType":"GetHotkeyList"},' +
        '{"requestType":"CreateScene","requestData":{"sceneName":"Cena B"}},' +
        '{"requestType":"CreateScene","requestData":{"sceneName":"Fontes de teste"}},' +
        '{"requestType":"CreateInput","requestData":{"sceneName":"Fontes de teste","inputName":"Tela teste","inputKind":"monitor_capture","sceneItemEnabled":false}}]')
    Check "pasta de gravacoes apontada para -OutDir" ($r.Count -ge 1 -and $r[0].ok)
    Check "atalho VoltzStudio.AddMarker registrado" ($r.Count -ge 3 -and (@($r[2].data.hotkeys) -contains "VoltzStudio.AddMarker"))
    $special = $r[1].data
    $mutes = @()
    foreach ($n in @($special.desktop1, $special.desktop2, $special.mic1, $special.mic2, $special.mic3, $special.mic4)) {
        if ($n) { $mutes += @{ requestType = "SetInputMute"; requestData = @{ inputName = $n; inputMuted = $true } } }
    }
    if ($mutes.Count -gt 0) { [void](Ws (ConvertTo-Json -InputObject $mutes -Depth 5 -Compress)) }

    # "Tela teste": captura de monitor com o item DESATIVADO numa cena que nunca vai ao programa - so para o cursor
    $items = (Ws '[{"requestType":"GetInputPropertiesListPropertyItems","requestData":{"inputName":"Tela teste","propertyName":"monitor_id"}}]')[0].data.propertyItems
    $monitorId = (@($items) | Where-Object { $_.itemEnabled } | Select-Object -First 1).itemValue
    Check "monitor para a Tela de teste" ([bool]$monitorId)
    if ($monitorId) { [void](Ws ('[{"requestType":"SetInputSettings","requestData":{"inputName":"Tela teste","inputSettings":{"monitor_id":' + (ConvertTo-Json $monitorId) + '}}}]')) }

    # Substitutas do ISO: cor solida numa cena que nunca vai ao programa (trocar de cena nao afeta o ISO)
    $r = Ws ('[{"requestType":"CreateInput","requestData":{"sceneName":"Fontes de teste","inputName":"Tela substituta","inputKind":"color_source_v3","inputSettings":{"color":4278229248,"width":2560,"height":1440}}},' +
        '{"requestType":"CreateInput","requestData":{"sceneName":"Fontes de teste","inputName":"Camera substituta","inputKind":"color_source_v3","inputSettings":{"color":4294901760,"width":1280,"height":720}}}]')
    Check "fontes substitutas do ISO criadas" ($r.Count -eq 2 -and $r[0].ok -and $r[1].ok)
    Start-Sleep -Seconds 2

    # Seguranca: nenhuma captura de janela/jogo nem dshow; captura de monitor so com TODOS os itens de cena desativados
    # (nao renderiza nada) e nunca como fonte de papel do ISO - senao aborta sem gravar
    $inputs = @((Ws '[{"requestType":"GetInputList"}]')[0].data.inputs)
    $unsafe = @($inputs | Where-Object { $_.inputKind -match 'window_capture|game_capture|dshow' })
    $monitorInputs = @($inputs | Where-Object { $_.inputKind -match 'monitor_capture' } | ForEach-Object { $_.inputName })
    $sceneNames = @((Ws '[{"requestType":"GetSceneList"}]')[0].data.scenes | ForEach-Object { $_.sceneName })
    $groupNames = @((Ws '[{"requestType":"GetGroupList"}]')[0].data.groups)
    $listReqs = @($sceneNames | ForEach-Object { @{ requestType = "GetSceneItemList"; requestData = @{ sceneName = $_ } } }) +
        @($groupNames | ForEach-Object { @{ requestType = "GetGroupSceneItemList"; requestData = @{ sceneName = $_ } } })
    $allItems = @()
    $listOk = $listReqs.Count -gt 0
    if ($listOk) {
        foreach ($resp in (Ws (ConvertTo-Json -InputObject $listReqs -Depth 5 -Compress))) {
            if (-not $resp.ok) { $listOk = $false; continue }
            $allItems += @($resp.data.sceneItems)
        }
    }
    $monitorItems = @($allItems | Where-Object { $monitorInputs -contains $_.sourceName })
    $enabledMonitor = @($monitorItems | Where-Object { $_.sceneItemEnabled })
    $monitorAsRole = @($monitorInputs | Where-Object { $_ -eq $env:VOLTZ_TEST_ISO_SCREEN -or $_ -eq $env:VOLTZ_TEST_ISO_CAMERA })
    $telaTesteEnabled = $null
    $idResp = (Ws '[{"requestType":"GetSceneItemId","requestData":{"sceneName":"Fontes de teste","sourceName":"Tela teste"}}]')[0]
    if ($idResp.ok) {
        $enResp = (Ws ('[{"requestType":"GetSceneItemEnabled","requestData":{"sceneName":"Fontes de teste","sceneItemId":' + [int]$idResp.data.sceneItemId + '}}]'))[0]
        if ($enResp.ok) { $telaTesteEnabled = [bool]$enResp.data.sceneItemEnabled }
    }
    $safe = $unsafe.Count -eq 0 -and $listOk -and $enabledMonitor.Count -eq 0 -and $monitorAsRole.Count -eq 0 -and
        $monitorInputs.Count -eq 1 -and $monitorInputs[0] -eq "Tela teste" -and $monitorItems.Count -eq 1 -and $telaTesteEnabled -eq $false
    Check "seguranca: sem janela/jogo/dshow ($($unsafe.Count)); monitor so 'Tela teste' com item desativado ($($monitorItems.Count) item(ns), ativado=$telaTesteEnabled)" $safe
    if (-not $safe) { throw "captura real presente ou ativa - abortando antes de gravar: $((@($unsafe | ForEach-Object { $_.inputName }) + $monitorInputs) -join ', ')" }

    $r = Ws '[{"requestType":"StartRecord"}]'
    Check "StartRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 3
    [void](Ws '[{"requestType":"SetCurrentProgramScene","requestData":{"sceneName":"Cena B"}}]')
    Start-Sleep -Seconds 1
    Check "botao Marcador (UI Automation)" (Uia "Marcador")
    Start-Sleep -Seconds 1
    $r = Ws '[{"requestType":"TriggerHotkeyByName","requestData":{"hotkeyName":"VoltzStudio.AddMarker"}}]'
    Check "atalho de marcador (websocket)" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 1
    [void](Ws '[{"requestType":"PauseRecord"}]')
    Start-Sleep -Seconds 2
    [void](Ws '[{"requestType":"ResumeRecord"}]')
    Start-Sleep -Seconds 3
    $r = Ws '[{"requestType":"StopRecord"}]'
    Check "StopRecord" ($r.Count -eq 1 -and $r[0].ok)
    Start-Sleep -Seconds 4
    $completed = $true
} finally {
    $cleanExit = $false
    if ($proc -and -not $proc.HasExited) {
        $proc.Refresh()
        [void]$proc.CloseMainWindow()
        $cleanExit = $proc.WaitForExit(30000)
        if (-not $cleanExit) { Stop-Process -Id $proc.Id -Force }
    }
    Check "encerrou limpo ao fechar a janela" $cleanExit
    Remove-Item Env:VOLTZ_TEST_ISO_SCREEN, Env:VOLTZ_TEST_ISO_CAMERA -ErrorAction SilentlyContinue
    $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime | Select-Object -Last 1
    if ($log) { Copy-Item $log.FullName $logCopy -Force }
    Start-Sleep -Seconds 5
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    if (Test-Path $bak) { Move-Item $bak $cfg }
    # Abortou no meio: apaga as gravacoes de teste sem abrir (a validacao abaixo nao roda)
    if (-not $completed -and -not $Keep -and (Test-Path -LiteralPath $OutDir)) { Remove-Item -Recurse -Force -LiteralPath $OutDir; Write-Host "apagado (abortado): $OutDir" }
}

function LogHas([string]$text) { return [bool]((Test-Path $logCopy) -and (Select-String -LiteralPath $logCopy -SimpleMatch $text -Encoding UTF8 -Quiet)) }

$projects = @(Get-ChildItem -LiteralPath $OutDir -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "projeto.voltz") })
Check "uma pasta de projeto em -OutDir" ($projects.Count -eq 1)
if ($projects.Count -eq 1) {
    $dir = $projects[0].FullName
    Check "nome da pasta 'AAAA-MM-DD HHhMM . Smoke projeto'" ($projects[0].Name -match '^\d{4}-\d{2}-\d{2} \d{2}h\d{2} . Smoke projeto$')
    $m = Read-JsonFile (Join-Path $dir "projeto.voltz")
    Check "manifesto formato/versao" ($m.formato -eq "voltz-projeto" -and $m.versao -eq 1)
    Check "manifesto status ok" ($m.status -eq "ok")
    Check "manifesto nome" ($m.nome -eq "Smoke projeto")
    Check "t0_ns preenchido" ([int64]$m.t0_ns -gt 0)
    $pauses = @($m.pausas)
    $pauseMs = 0
    if ($pauses.Count -eq 1 -and $null -ne $pauses[0].fim_ms) { $pauseMs = $pauses[0].fim_ms - $pauses[0].inicio_ms }
    Check "uma pausa fechada entre 1,5 e 5 s ($pauseMs ms)" ($pauses.Count -eq 1 -and $pauseMs -ge 1500 -and $pauseMs -le 5000)
    Check "duracao bruta entre 8 e 40 s ($($m.duracao_ms) ms)" ($m.duracao_ms -ge 8000 -and $m.duracao_ms -le 40000)
    Check "duracao liquida = bruta - pausa" ($m.duracao_liquida_ms -eq ($m.duracao_ms - $pauseMs))
    $prog = Join-Path $dir ([string]$m.arquivos.programa.arquivo)
    Check "programa.mp4 existe e > 0" ($m.arquivos.programa.arquivo -eq "programa.mp4" -and (Test-Path -LiteralPath $prog) -and (Get-Item -LiteralPath $prog).Length -gt 0)
    Check "manifesto declara 3 faixas de audio" ($m.arquivos.programa.faixas_audio -eq 3)
    $tela = $m.arquivos.tela
    $camera = $m.arquivos.camera
    if ($ProgramOnly) {
        Check "so a versao montada: manifesto sem tela/camera" (-not $tela -and -not $camera)
        Check "so a versao montada: sem tela.mkv/camera.mkv" (-not (Test-Path -LiteralPath (Join-Path $dir "tela.mkv")) -and -not (Test-Path -LiteralPath (Join-Path $dir "camera.mkv")))
    } else {
        Check "manifesto: tela.mkv 2560x1440 no fps do programa" ($tela -and $tela.arquivo -eq "tela.mkv" -and $tela.largura -eq 2560 -and $tela.altura -eq 1440 -and $tela.fps -eq $m.arquivos.programa.fps)
        Check "manifesto: camera.mkv 1280x720" ($camera -and $camera.arquivo -eq "camera.mkv" -and $camera.largura -eq 1280 -and $camera.altura -eq 720)
        Check "manifesto: offset_ms dos ISO entre 0 e 1000 ($($tela.offset_ms) / $($camera.offset_ms))" ($tela -and $camera -and $tela.offset_ms -ge 0 -and $tela.offset_ms -le 1000 -and $camera.offset_ms -ge 0 -and $camera.offset_ms -le 1000)
        Check "manifesto: codificador dos ISO ($($tela.codificador))" ($tela -and $camera -and [bool]$tela.codificador -and [bool]$camera.codificador)
        Check "manifesto: nenhum iso_falhou" (@(@($m.eventos) | Where-Object { $_.tipo -eq "iso_falhou" }).Count -eq 0)
    }
    Check "nenhum arquivo .tmp sobrando" (@(Get-ChildItem -LiteralPath $dir -Filter *.tmp).Count -eq 0)

    $markers = @(Read-JsonFile (Join-Path $dir "marcadores.json"))
    Check "2 marcadores (Marcador 1, Marcador 2) em ordem" ($markers.Count -eq 2 -and $markers[0].nome -eq "Marcador 1" -and $markers[1].nome -eq "Marcador 2" -and $markers[0].t_ms -lt $markers[1].t_ms -and $markers[1].t_ms -lt $m.duracao_ms)

    $cursorFile = Join-Path $dir "cursor.jsonl"
    $cursorOk = $false
    $cursorLines = 0
    if ($m.arquivos.cursor -and (Test-Path -LiteralPath $cursorFile)) {
        $w = [int]$m.arquivos.cursor.largura
        $h = [int]$m.arquivos.cursor.altura
        $cursorOk = $w -gt 0 -and $h -gt 0
        foreach ($line in Get-Content -Encoding UTF8 -LiteralPath $cursorFile) {
            if (-not $line) { continue }
            $cursorLines++
            $e = $line | ConvertFrom-Json
            if (@("move", "down", "up", "scroll", "fora") -notcontains $e.e -or $null -eq $e.t -or $e.t -lt 0) { $cursorOk = $false; break }
            if ($e.e -ne "fora" -and ($e.x -lt 0 -or $e.y -lt 0 -or $e.x -ge $w -or $e.y -ge $h)) { $cursorOk = $false; break }
        }
    }
    Check "cursor.jsonl com >= 1 linha valida dentro da captura ($cursorLines linha(s))" ($cursorOk -and $cursorLines -ge 1)

    Check "log: capitulos 'Marcador 1' e 'Marcador 2'" ((LogHas 'Adding chapter "Marcador 1"') -and (LogHas 'Adding chapter "Marcador 2"'))
    Check "log: projeto finalizado (ok" (LogHas '[voltz] projeto finalizado (ok')
    Check "log: sem 'a gravacao nao iniciou'" (-not (LogHas '[voltz] a gravacao nao iniciou'))
    if ($ProgramOnly) {
        Check "log: iso desligado" (LogHas '[voltz] iso: desligado')
    } else {
        Check "log: ISO pausados e retomados com o programa" ((LogHas 'output voltz-iso-tela paused') -and (LogHas 'output voltz-iso-tela unpaused') -and (LogHas 'output voltz-iso-camera paused') -and (LogHas 'output voltz-iso-camera unpaused'))
        Check "log: fontes de teste do ISO usadas" (LogHas "[voltz] iso: fonte de teste 'Tela substituta'")
    }

    if ($ffprobe) {
        $probe = (& $ffprobe -v error -show_entries "stream=codec_type:format=duration" -show_chapters -of json $prog) | Out-String | ConvertFrom-Json
        $audio = @($probe.streams | Where-Object { $_.codec_type -eq "audio" }).Count
        $chapters = @($probe.chapters).Count
        $durMs = [double]$probe.format.duration * 1000
        Check "ffprobe: 3 faixas de audio ($audio)" ($audio -eq 3)
        # o OBS cria sozinho um capitulo "Iniciar" em 0 s; os 2 marcadores vem depois
        Check "ffprobe: 3 capitulos (Iniciar + 2 marcadores) ($chapters)" ($chapters -eq 3)
        $ch1 = @($probe.chapters | Where-Object { $_.tags.title -eq "Marcador 1" }) | Select-Object -First 1
        Check "ffprobe: duracao ~ liquida +-1 s ($([int]$durMs) x $($m.duracao_liquida_ms) ms)" ([math]::Abs($durMs - $m.duracao_liquida_ms) -le 1000)
        if ($ch1 -and $markers.Count -ge 1) {
            Check "ffprobe: capitulo 'Marcador 1' ~ marcador 1 +-700 ms" ([math]::Abs([double]$ch1.start_time * 1000 - $markers[0].t_ms) -le 700)
        } else {
            Check "ffprobe: capitulo 'Marcador 1' encontrado" $false
        }
        if (-not $ProgramOnly) {
            $progDur = [double]$probe.format.duration
            foreach ($iso in @(@{ f = "tela.mkv"; w = 2560; h = 1440; e = $tela }, @{ f = "camera.mkv"; w = 1280; h = 720; e = $camera })) {
                $isoPath = Join-Path $dir $iso.f
                if (-not (Test-Path -LiteralPath $isoPath) -or -not $iso.e) { Check "ffprobe: $($iso.f) existe" $false; continue }
                # so metadados (nunca quadros)
                $ip = (& $ffprobe -v error -show_entries "stream=codec_type,codec_name,width,height:format=duration" -of json $isoPath) | Out-String | ConvertFrom-Json
                $v = @($ip.streams | Where-Object { $_.codec_type -eq "video" })
                $a = @($ip.streams | Where-Object { $_.codec_type -eq "audio" })
                Check "ffprobe: $($iso.f) 1 video $($iso.w)x$($iso.h) hevc/h264 + 1 audio aac" ($v.Count -eq 1 -and $v[0].width -eq $iso.w -and $v[0].height -eq $iso.h -and @("hevc", "h264") -contains $v[0].codec_name -and $a.Count -eq 1 -and $a[0].codec_name -eq "aac")
                $isoDur = [double]$ip.format.duration
                $gap = [math]::Abs($isoDur + [double]$iso.e.offset_ms / 1000.0 - $progDur)
                Write-Host ("ffprobe $($iso.f): $($v[0].codec_name) $($v[0].width)x$($v[0].height) dur=$([math]::Round($isoDur, 3)) offset=$($iso.e.offset_ms) ms prog=$([math]::Round($progDur, 3)) diff=$([math]::Round($gap, 3)) s")
                Check "ffprobe: $($iso.f) duracao + offset ~ programa +-0,5 s ($([math]::Round($isoDur, 2)) s + $($iso.e.offset_ms) ms x $([math]::Round($progDur, 2)) s)" ($gap -le 0.5)
            }
        }
    } else {
        Write-Host "AVISO ffprobe nao encontrado: faixas/capitulos/duracao nao conferidos pelo ffprobe (use o log)"
    }
}

$failed = 0
foreach ($k in $checks.Keys) {
    if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
}
if (Test-Path $logCopy) { Write-Host "log: $logCopy" }
if (-not $Keep) { Remove-Item -Recurse -Force -LiteralPath $OutDir -ErrorAction SilentlyContinue; Write-Host "apagado: $OutDir" }
if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) {
    Remove-Item -Force -LiteralPath $voltzVideos
    Write-Host "apagado (vazio): $voltzVideos"
}
if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE PROJETO OK"
