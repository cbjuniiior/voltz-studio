<#
  Smoke test do Voltz Studio.
  Abre o app em modo portatil com configuracao limpa, espera, fecha pela janela principal
  e confere pasta de configuracao, log e encerramento.
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-smoke.ps1 [-SkipShell] [-Seconds 20]
#>
param(
    [string]$Config = "RelWithDebInfo",
    [int]$Seconds = 20,
    [switch]$SkipShell
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rundir = Join-Path $root "build_x64\rundir\$Config"
$bin = Join-Path $rundir "bin\64bit"
$exe = Join-Path $bin "VoltzStudio.exe"
if (-not (Test-Path $exe)) { Write-Host "FALHOU executavel nao encontrado: $exe"; exit 1 }

# Plano 3A: se esta execucao criar Videos\Voltz Studio e ela ficar vazia, e apagada no fim.
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"
$hadVoltzVideos = Test-Path -LiteralPath $voltzVideos

$cfg = Join-Path $rundir "config"
# A config portatil existente e guardada e restaurada no fim (nunca apagada)
$bak = Join-Path $rundir "config.smoke-bak"
if (Test-Path $bak) { Write-Host "FALHOU backup anterior em $bak (restaure ou apague antes de rodar)"; exit 1 }
if (Test-Path $cfg) { Move-Item $cfg $bak }

$failed = 0
$proc = $null
# try/finally: qualquer excecao no meio ainda fecha o app lancado aqui (so pelo PID) e restaura a config portatil
try {
    $proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
    Start-Sleep -Seconds $Seconds
    $aliveAfterStart = -not $proc.HasExited

    $cleanExit = $false
    if ($aliveAfterStart) {
        $proc.Refresh()
        [void]$proc.CloseMainWindow()
        $cleanExit = $proc.WaitForExit(20000)
        if (-not $cleanExit) { Stop-Process -Id $proc.Id -Force }
    }

    $voltzCfg = Join-Path $cfg "Voltz Studio"
    $log = Get-ChildItem (Join-Path $voltzCfg "logs") -Filter *.txt -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime | Select-Object -Last 1

    $checks = [ordered]@{}
    $checks["processo vivo apos $Seconds s"] = $aliveAfterStart
    $checks["pasta de config 'Voltz Studio' criada"] = (Test-Path $voltzCfg)
    $checks["pasta 'obs-studio' ausente"] = -not (Test-Path (Join-Path $cfg "obs-studio"))
    $checks["log gerado"] = [bool]$log
    if (-not $SkipShell) {
        $checks["log contem '[voltz] shell pronta'"] = [bool]($log -and (Select-String -Path $log.FullName -Pattern '\[voltz\] shell pronta' -Quiet))
        $checks["encerrou limpo ao fechar a janela"] = $cleanExit
        $checks["log contem '[voltz] padroes de projeto aplicados'"] = [bool]($log -and (Select-String -Path $log.FullName -Pattern '\[voltz\] padroes de projeto aplicados' -Quiet))
    }

    foreach ($k in $checks.Keys) {
        if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
    }
    if ($log) { Write-Host "log: $($log.FullName)" }
    if (-not $hadVoltzVideos -and (Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) {
        Remove-Item -Force -LiteralPath $voltzVideos
        Write-Host "apagado (vazio): $voltzVideos"
    }
} finally {
    if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
    # Restaura a config portatil original (espera o app soltar os arquivos)
    Start-Sleep -Seconds 5
    if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
    if (Test-Path $bak) { Move-Item $bak $cfg }
}

if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE OK"
