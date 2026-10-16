<#
  Smoke test de gravacao do Voltz Studio.
  Abre o app em modo portatil com configuracao limpa (ConfirmOnExit=false), inicia a gravacao
  com --startrecording, espera, fecha pela janela principal e confere log e arquivo gravado.
  Apaga ao final somente o arquivo exato cujo caminho consta no log da execucao.
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke-record.ps1 [-Seconds 15]
#>
param(
    [string]$Config = "RelWithDebInfo",
    [int]$Seconds = 15
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rundir = Join-Path $root "build_x64\rundir\$Config"
$bin = Join-Path $rundir "bin\64bit"
$exe = Join-Path $bin "VoltzStudio.exe"
if (-not (Test-Path $exe)) { Write-Host "FALHOU executavel nao encontrado: $exe"; exit 1 }

# Plano 3A: a pasta padrao de gravacoes e Videos\Voltz Studio (o app cria). Se esta execucao a criou e ela ficar
# vazia, e apagada no fim.
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"
$hadVoltzVideos = Test-Path -LiteralPath $voltzVideos

$cfg = Join-Path $rundir "config"
# A config portatil existente e guardada e restaurada no fim (nunca apagada)
$bak = Join-Path $rundir "config.smokerecord-bak"
if (Test-Path $bak) { Write-Host "FALHOU backup anterior em $bak (restaure ou apague antes de rodar)"; exit 1 }
if (Test-Path $cfg) { Move-Item $cfg $bak }
$failed = 0
$prime = $null
$proc = $null
# try/finally: qualquer excecao no meio ainda fecha o app lancado aqui (so pelo PID) e restaura a config portatil
try {
    $voltzCfg = Join-Path $cfg "Voltz Studio"
    New-Item -ItemType Directory -Force $voltzCfg | Out-Null
    # Sem BOM; ConfirmOnExit=false evita o dialogo de confirmacao ao fechar gravando
    [System.IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"),
        "[General]`r`nConfirmOnExit=false`r`n[Voltz]`r`nProgramOnly=true`r`n", (New-Object System.Text.UTF8Encoding $false))

    # Passo de preparo: no OBS 32.2.2, Load() retorna cedo quando nao ha colecao de cenas e ignora
    # --startrecording (OBSBasic_SceneCollections.cpp). Uma abertura previa cria a cena padrao em disco.
    $prime = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
    Start-Sleep -Seconds 8
    if (-not $prime.HasExited) {
        $prime.Refresh()
        [void]$prime.CloseMainWindow()
        if (-not $prime.WaitForExit(30000)) { Stop-Process -Id $prime.Id -Force }
    }

    $start = Get-Date
    $proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi', '--startrecording' -WorkingDirectory $bin -PassThru
    Start-Sleep -Seconds $Seconds
    $aliveAfterStart = -not $proc.HasExited

    $cleanExit = $false
    if ($aliveAfterStart) {
        $proc.Refresh()
        [void]$proc.CloseMainWindow()
        $cleanExit = $proc.WaitForExit(30000)
        if (-not $cleanExit) { Stop-Process -Id $proc.Id -Force }
    }

    $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -ge $start } |
        Sort-Object LastWriteTime | Select-Object -Last 1

    # Marcadores no log (OBS 32.2.2): "==== Recording Start" e emitido ao iniciar. "==== Recording Stop" so e
    # emitido quando a gravacao e parada com o app aberto; ao fechar o app gravando, o desligamento destroi
    # as saidas direto e o log traz "Output 'simple_file_output': stopping" (comportamento do upstream).
    # Arquivo gravado: extraido do log, NUNCA por varredura da pasta Videos (nao apagamos o que nao criamos).
    # Padrao no log (OBS 32.2.2):
    #   [mp4 output: 'simple_file_output'] Writing Hybrid MP4/MOV file 'C:/Users/X/Videos/2026-10-15 18-07-26.mp4'...
    #   [ffmpeg muxer: 'simple_file_output'] Writing file 'C:/Users/X/Videos/....mkv'...
    # Regex: "Writing ... file '<caminho>'...". Se nao der para extrair, o check falha e nada e apagado.
    $recPath = $null
    if ($log) {
        $m = Select-String -Path $log.FullName -Pattern "Writing .*?file '(.+)'\.\.\.\s*$" -Encoding UTF8 | Select-Object -Last 1
        if ($m) { $recPath = $m.Matches[0].Groups[1].Value -replace '/', '\' }
    }
    $recFile = $null
    if ($recPath -and (Test-Path -LiteralPath $recPath -PathType Leaf)) {
        $item = Get-Item -LiteralPath $recPath
        if ($item.LastWriteTime -gt $start) { $recFile = $item }
    }
    $withData = @(); if ($recFile -and $recFile.Length -gt 0) { $withData = @($recFile) }
    $files = @($recFile | Where-Object { $_ })

    $checks = [ordered]@{}
    $checks["processo vivo apos $Seconds s"] = $aliveAfterStart
    $checks["log gerado"] = [bool]$log
    $checks["log contem '[voltz] shell pronta'"] = [bool]($log -and (Select-String -Path $log.FullName -Pattern '\[voltz\] shell pronta' -Quiet))
    $checks["log contem '==== Recording Start'"] = [bool]($log -and (Select-String -Path $log.FullName -SimpleMatch '==== Recording Start' -Quiet))
    $checks["log contem parada da saida de gravacao (stopping)"] = [bool]($log -and (Select-String -Path $log.FullName -Pattern "Output '.*file_output': stopping" -Quiet))
    $checks["caminho da gravacao extraido do log"] = [bool]$recPath
    $checks["arquivo de gravacao (exato) existe, novo e com tamanho > 0"] = ($withData.Count -gt 0)
    $checks["encerrou limpo ao fechar a janela"] = $cleanExit

    foreach ($k in $checks.Keys) {
        if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
    }
    Write-Host "caminho parseado: $recPath"
    if ($log) { Write-Host "log: $($log.FullName)" }
    foreach ($f in $withData) { Write-Host "gravacao: $($f.FullName) ($($f.Length) bytes)" }

    # Plano 3A: a gravacao fica numa pasta de projeto (programa.mp4 + projeto.voltz + marcadores.json [+ cursor.jsonl]).
    # Apaga a pasta inteira so se ela tem projeto.voltz e foi criada nesta execucao; senao, so o arquivo exato.
    $projDir = if ($recPath) { Split-Path -Parent $recPath } else { $null }
    $isProject = $projDir -and (Test-Path -LiteralPath (Join-Path $projDir "projeto.voltz")) -and ((Get-Item -LiteralPath $projDir).CreationTime -ge $start)
    if ($isProject) {
        $manifest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $projDir "projeto.voltz") | ConvertFrom-Json
        $closed = $manifest.status -eq "ok" -and @($manifest.eventos | Where-Object { $_.tipo -eq "app_fechado_gravando" }).Count -eq 1
        if ($closed) { Write-Host "OK     projeto.voltz fechado ao sair gravando (status ok, app_fechado_gravando)" }
        else { Write-Host "FALHOU projeto.voltz nao foi fechado ao sair gravando (status '$($manifest.status)')"; $failed++ }
        Remove-Item -Recurse -Force -LiteralPath $projDir
        Write-Host "apagado: $projDir"
    } else {
        foreach ($f in $files) { Remove-Item -Force $f.FullName; Write-Host "apagado: $($f.FullName)" }
    }
    if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) {
        Remove-Item -Force -LiteralPath $voltzVideos
        Write-Host "apagado (vazio): $voltzVideos"
    }

} finally {
    foreach ($p in @($prime, $proc)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
    # Restaura a config portatil original (depois de ler o log e o manifesto; espera o app soltar os arquivos)
    Start-Sleep -Seconds 5
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    if (Test-Path $bak) { Move-Item $bak $cfg }
}

if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE GRAVACAO OK"
