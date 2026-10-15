<#
  Smoke test de gravacao do Voltz Studio.
  Abre o app em modo portatil com configuracao limpa (ConfirmOnExit=false), inicia a gravacao
  com --startrecording, espera, fecha pela janela principal e confere log e arquivo gravado.
  Os arquivos de teste gravados em Videos sao apagados ao final.
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

$cfg = Join-Path $rundir "config"
if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
$voltzCfg = Join-Path $cfg "Voltz Studio"
New-Item -ItemType Directory -Force $voltzCfg | Out-Null
# Sem BOM; ConfirmOnExit=false evita o dialogo de confirmacao ao fechar gravando
[System.IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"),
    "[General]`r`nConfirmOnExit=false`r`n", (New-Object System.Text.UTF8Encoding $false))

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
$videos = [Environment]::GetFolderPath('MyVideos')
$exts = '.mkv', '.mp4', '.mov', '.flv', '.ts'
$files = @(Get-ChildItem $videos -File -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -gt $start -and ($exts -contains $_.Extension.ToLower()) })
$withData = @($files | Where-Object { $_.Length -gt 0 })

$checks = [ordered]@{}
$checks["processo vivo apos $Seconds s"] = $aliveAfterStart
$checks["log gerado"] = [bool]$log
$checks["log contem '[voltz] shell pronta'"] = [bool]($log -and (Select-String -Path $log.FullName -Pattern '\[voltz\] shell pronta' -Quiet))
$checks["log contem '==== Recording Start'"] = [bool]($log -and (Select-String -Path $log.FullName -SimpleMatch '==== Recording Start' -Quiet))
$checks["log contem parada da saida de gravacao (stopping)"] = [bool]($log -and (Select-String -Path $log.FullName -Pattern "Output '.*file_output': stopping" -Quiet))
$checks["arquivo de gravacao com tamanho > 0"] = ($withData.Count -gt 0)
$checks["encerrou limpo ao fechar a janela"] = $cleanExit

$failed = 0
foreach ($k in $checks.Keys) {
    if ($checks[$k]) { Write-Host "OK     $k" } else { Write-Host "FALHOU $k"; $failed++ }
}
if ($log) { Write-Host "log: $($log.FullName)" }
foreach ($f in $withData) { Write-Host "gravacao: $($f.FullName) ($($f.Length) bytes)" }

# Apaga a saida de teste (arquivos nossos, criados apos o inicio do smoke)
foreach ($f in $files) { Remove-Item -Force $f.FullName; Write-Host "apagado: $($f.FullName)" }

if ($failed -gt 0) { exit 1 }
Write-Host "SMOKE GRAVACAO OK"
