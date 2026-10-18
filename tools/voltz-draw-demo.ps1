<#
  Demonstracao MANUAL do modo desenho (Planos 4C e 4D): o usuario abre, liga o modo desenho, desenha e escreve. Nao e
  teste automatico.
  -Launch: guarda a config portatil (config.drawdemo-bak), cria uma limpa (pt-BR, sem confirmacao ao sair) e abre o
    Voltz portatil SEM nenhuma variavel de teste (com VOLTZ_TEST_LIVE o modo desenho e recusado). Com -Freeze, liga
    VOLTZ_DEBUG_FREEZE_UI: o atalho Ctrl+Shift+F12 trava a interface por 8 s (para ver o vigia do modo desenho).
  -Log: mostra as linhas do log do Voltz aberto sobre o modo desenho, a barra, as anotacoes, os atalhos e a depuracao
    (as medidas de redesenho aparecem na linha "modo desenho: desligado (...)", ao sair do modo). O texto digitado nunca
    vai para o log: so o tamanho ("texto confirmado (N caractere(s))").
  -Hotkeys: mostra os atalhos do Voltz gravados no perfil da config portatil ([Hotkeys] VoltzStudio.* do basic.ini; so
    leitura). Atalho que nao aparece = ainda no padrao.
  -Close: fecha o Voltz aberto pelo -Launch (PID salvo, WM_CLOSE na janela da casca guardada) e restaura a config
    portatil (ou apaga a da demonstracao, se nao havia uma antes). Se a pasta Videos\Voltz Studio nao existia antes do
    -Launch e ficou vazia, ela e apagada; com gravacoes dentro, fica.

  O QUE FAZER (depois do -Launch):
    1. Na tela Presets, aplique VOCE MESMO um preset com a Tela real (ex.: Tutorial).
    2. Ligue o modo desenho com Ctrl+Shift+D ou com o botao "Desenhar" da tela Gravar. A barra aparece no alto do
       monitor da Tela; o cursor vira uma cruz so nesse monitor (os outros continuam livres).
    3. Desenhe com o botao esquerdo: caneta, marca-texto, linha, seta, retangulo e elipse; cores (4 + 8 + a cor
       personalizada, com o seletor completo), espessura, desfazer (Ctrl+Z ou o botao), limpar e "tracos somem apos"
       ficam na barra. A linha de baixo da barra mostra o que cada botao faz.
    4. Texto: escolha a ferramenta Texto (T) na barra, clique onde o texto comeca e digite (acentos valem). Enter
       confirma, Shift+Enter quebra a linha, Backspace apaga, Esc (ou Ctrl+Z) cancela so o texto - o Esc seguinte sai do
       modo. Texto em branco (vazio ou so espacos e quebras) nao fica. Tamanho (4 botoes "A") e negrito ficam na barra
       no lugar das espessuras.
    5. Ajustes > Anotacoes (estilo com que o modo comeca, com previa) e Ajustes > Atalhos (clique no campo e aperte a
       combinacao; enquanto o campo esta ativo os atalhos ficam pausados; clicar no fundo da secao tira o foco do
       campo). Em Ajustes a roda do mouse rola a pagina: so muda uma caixa ou um slider depois de clicar nele.
    6. Rode -Log para ver o que o Voltz registrou, -Hotkeys para ver os atalhos gravados e -Close para fechar e
       restaurar a config.

  COMO SAIR DO MODO DESENHO:
    - Sempre: clique direito, o mesmo atalho (Ctrl+Shift+D), o botao Sair (X) da barra e 30 s sem mexer o mouse nem o
      teclado - e, se a interface do Voltz travar, o modo sai sozinho em ~1 s.
    - Esc, Alt+Tab, tecla Windows, clique em outra janela ou outro monitor e Alt+F4 tambem saem, MAS: se o modo foi
      ligado pelo atalho, so depois do primeiro clique na tela (antes dele a janela do desenho ainda nao esta ativa e
      essas teclas vao para o app em uso). A linha "modo desenho: ligado (...)" do -Log diz "ativa: sim" ou "ativa: nao".

  SE ALGO TRAVAR:
    - Primeiro clique direito ou Esc; depois Alt+Tab ou a tecla Windows (estes tres, ligado pelo atalho, so depois do
      primeiro clique na tela); ou espere 30 s parado (o modo sai sozinho).
    - Ctrl+Alt+Del sempre funciona (Gerenciador de Tarefas).
    - Em outro PowerShell: powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Close
      (fecha so o Voltz desta demonstracao; se ele nao fechar em 30 s, encerra o PID que o -Launch abriu).
    - Rode -Log e guarde a saida para relatar o problema.

  Seguranca: este script NAO liga o modo desenho, nao move o mouse, nao clica, nao digita e nao poe janela alguma em
  primeiro plano; so abre e fecha o Voltz que ele mesmo lancou (PID e executavel conferidos). A unica mensagem que ele
  manda a uma janela e WM_CLOSE na janela da casca do PID lancado.
  Uso: powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Launch [-Freeze]
       powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Log
       powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Hotkeys
       powershell -ExecutionPolicy Bypass -File tools/voltz-draw-demo.ps1 -Close
#>
param(
    [switch]$Launch,
    [switch]$Freeze,
    [switch]$Log,
    [switch]$Hotkeys,
    [switch]$Close,
    [string]$Config = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$rundir = Join-Path $root "build_x64\rundir\$Config"
$bin = Join-Path $rundir "bin\64bit"
$exe = Join-Path $bin "VoltzStudio.exe"
$cfg = Join-Path $rundir "config"
$bak = Join-Path $rundir "config.drawdemo-bak"
$marker = Join-Path $cfg "voltz-draw-demo.marker"
$pidFile = Join-Path $env:TEMP "voltz-draw-demo.pid"
# O Voltz cria Videos\Voltz Studio ao abrir: este arquivo diz que ela nao existia antes do -Launch
$videosFlag = Join-Path $env:TEMP "voltz-draw-demo.videos"
$voltzVideos = Join-Path ([Environment]::GetFolderPath('MyVideos')) "Voltz Studio"
$testVars = @("VOLTZ_TEST_LIVE", "VOLTZ_TEST_PRESET_STANDINS", "VOLTZ_TEST_OVERLAY_RECT", "VOLTZ_TEST_ANNOT_BURNED", "VOLTZ_TEST_SCREEN_LAYERS", "VOLTZ_TEST_ISO_SCREEN", "VOLTZ_TEST_ISO_CAMERA", "VOLTZ_TEST_ISO_OVERLOAD", "VOLTZ_DEBUG_FREEZE_UI")

# P/Invoke de leitura; a unica escrita e PostMessage(WM_CLOSE) na janela da casca do PID aberto pelo -Launch
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class VoltzDrawDemoWin {
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder sb, int n);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
 public static string ClassOf(IntPtr h) { var sb = new System.Text.StringBuilder(256); GetClassName(h, sb, 256); return sb.ToString(); }
 public static uint OwnerOf(IntPtr h) { uint owner; GetWindowThreadProcessId(h, out owner); return owner; } }
'@
$Win = [VoltzDrawDemoWin]

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

function Restore-Config {
    Start-Sleep -Seconds 5
    if (Test-Path $bak) {
        if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
        Move-Item $bak $cfg
    } elseif (Test-Path -LiteralPath $marker) {
        # nao havia config portatil antes do -Launch: a da demonstracao sai inteira
        Remove-Item -Recurse -Force $cfg
    }
}

# So a pasta Videos\Voltz Studio que o -Launch viu ausente, e so se estiver vazia (nada do usuario e apagado)
function Restore-Videos {
    if (-not (Test-Path -LiteralPath $videosFlag)) { return }
    if ((Test-Path -LiteralPath $voltzVideos) -and -not (Get-ChildItem -Force -LiteralPath $voltzVideos)) {
        Remove-Item -Force -LiteralPath $voltzVideos
        Write-Host "apagada (vazia, criada pela demonstracao): $voltzVideos"
    } elseif (Test-Path -LiteralPath $voltzVideos) {
        Write-Host "mantida (tem arquivos): $voltzVideos"
    }
    Remove-Item -LiteralPath $videosFlag -ErrorAction SilentlyContinue
}

if ($Launch) {
    if (-not (Test-Path $exe)) { throw "executavel nao encontrado: $exe" }
    if (Test-Path $bak) { throw "ja existe $bak (rode -Close antes)" }
    # backup pendente de outro script (smoke, demonstracao) na pasta da config portatil: nada e mexido
    $oldBaks = @(Get-ChildItem -LiteralPath $rundir -Force -Filter "config.*bak*" -ErrorAction SilentlyContinue)
    if ($oldBaks.Count -gt 0) { throw "backup de config pendente em $rundir ($(($oldBaks | ForEach-Object { $_.Name }) -join ', ')): restaure ou apague antes" }
    if (Launched) { throw "a demonstracao ja esta aberta (rode -Close antes)" }
    if (Test-Path $cfg) { Move-Item $cfg $bak }
    # marca antiga de um -Launch que nao chegou ao -Close: nao vale para esta abertura
    Remove-Item -LiteralPath $videosFlag -ErrorAction SilentlyContinue
    if (-not (Test-Path -LiteralPath $voltzVideos)) { Set-Content -LiteralPath $videosFlag -Value "Videos\Voltz Studio ausente antes do -Launch" }
    # o Voltz herda o ambiente deste PowerShell: nenhuma variavel de teste vai junto (o modo desenho seria recusado)
    $saved = @{}
    foreach ($name in $testVars) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, "Process"); Remove-Item "Env:$name" -ErrorAction SilentlyContinue }
    $proc = $null
    try {
        $voltzCfg = Join-Path $cfg "Voltz Studio"
        New-Item -ItemType Directory -Force $voltzCfg | Out-Null
        [IO.File]::WriteAllText((Join-Path $voltzCfg "user.ini"), "[General]`r`nLanguage=pt-BR`r`nConfirmOnExit=false`r`n", (New-Object Text.UTF8Encoding $false))
        Set-Content -LiteralPath $marker -Value "config criada pelo -Launch da demonstracao do modo desenho"
        if ($Freeze) { $env:VOLTZ_DEBUG_FREEZE_UI = "1" }
        $proc = Start-Process -FilePath $exe -ArgumentList '--portable', '--multi' -WorkingDirectory $bin -PassThru
        # janela da casca para o WM_CLOSE do -Close (nunca a sobreposicao nem a janela de entrada)
        $shell = [IntPtr]::Zero
        for ($i = 0; $i -lt 60 -and $shell -eq [IntPtr]::Zero -and -not $proc.HasExited; $i++) {
            Start-Sleep -Milliseconds 500
            $proc.Refresh()
            $h = $proc.MainWindowHandle
            if ($h -ne [IntPtr]::Zero -and $Win::OwnerOf($h) -eq $proc.Id -and $Win::ClassOf($h) -notmatch '^Voltz(AnnotationOverlay|DrawInput)$') { $shell = $h }
        }
        if ($proc.HasExited) { throw "o Voltz fechou logo depois de abrir" }
        Set-Content -LiteralPath $pidFile -Value ("{0}|{1}" -f $proc.Id, $shell.ToInt64())
    } catch {
        if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
        Remove-Item -LiteralPath $pidFile -ErrorAction SilentlyContinue
        Restore-Config
        Restore-Videos
        throw
    } finally {
        foreach ($name in $testVars) {
            if ($null -ne $saved[$name]) { Set-Item "Env:$name" $saved[$name] } else { Remove-Item "Env:$name" -ErrorAction SilentlyContinue }
        }
    }
    Write-Host "Voltz aberto (PID $($proc.Id)), sem modo de teste."
    Write-Host "  1. Na tela Presets, aplique um preset com a Tela real (ex.: Tutorial)."
    Write-Host "  2. Ligue o modo desenho com Ctrl+Shift+D ou com o botao Desenhar da tela Gravar e desenhe com o botao esquerdo."
    Write-Host "  3. Texto: ferramenta T na barra, clique e digite; Enter confirma, Shift+Enter quebra a linha, Esc cancela o texto."
    Write-Host "  Para sair do modo (sempre): clique direito, Ctrl+Shift+D de novo, o botao Sair (X) da barra ou 30 s parado."
    Write-Host "  Esc, Alt+Tab, tecla Windows, clique em outro monitor e Alt+F4 tambem saem - ligado pelo atalho, so depois do primeiro clique na tela."
    Write-Host "  Se algo travar: clique direito ou Esc; Alt+Tab ou tecla Windows; Ctrl+Alt+Del; ou rode -Close em outro PowerShell."
    if ($Freeze) { Write-Host "  Depuracao: Ctrl+Shift+F12 trava a interface por 8 s (com o modo ligado, ele deve sair sozinho em ~1 s)." }
    Write-Host "  Depois: -Log mostra o que o Voltz registrou; -Hotkeys, os atalhos gravados; -Close fecha o Voltz e restaura a config portatil."
    exit 0
}

if ($Log) {
    $logs = Join-Path $cfg "Voltz Studio\logs"
    $last = Get-ChildItem -LiteralPath $logs -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $last) { throw "nenhum log em $logs (rode -Launch antes)" }
    Write-Host "log: $($last.FullName)"
    Select-String -LiteralPath $last.FullName -Encoding UTF8 -Pattern '\[voltz\] (modo desenho|barra do modo desenho|anotacoes|atalhos|janelas fantasma|depuracao)|\[voltz-sources\] anotacoes' | ForEach-Object { $_.Line }
    exit 0
}

if ($Hotkeys) {
    $profiles = Join-Path $cfg "Voltz Studio\basic\profiles"
    $ini = Get-ChildItem -LiteralPath $profiles -Recurse -Filter basic.ini -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $ini) { throw "nenhum perfil em $profiles (rode -Launch antes)" }
    Write-Host "perfil: $($ini.FullName)"
    $lines = @(Get-Content -LiteralPath $ini.FullName -Encoding UTF8 | Where-Object { $_ -like 'VoltzStudio.*' })
    if ($lines.Count -eq 0) { Write-Host "(nenhum atalho do Voltz gravado: todos no padrao)" } else { $lines | ForEach-Object { Write-Host $_ } }
    exit 0
}

if ($Close) {
    $app = Launched
    if ($app) {
        $p = $app.Process
        $shellOk = ($app.Shell -ne [IntPtr]::Zero) -and $Win::IsWindow($app.Shell) -and ($Win::OwnerOf($app.Shell) -eq $p.Id) -and ($Win::ClassOf($app.Shell) -notmatch '^Voltz(AnnotationOverlay|DrawInput)$')
        # Process.CloseMainWindow pode mirar a sobreposicao (que ignora WM_CLOSE): fecha pela janela da casca
        if ($shellOk) { [void]$Win::PostMessage($app.Shell, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) } else { [void]$p.CloseMainWindow() }
        if (-not $p.WaitForExit(30000)) { Write-Host "o Voltz nao fechou em 30 s: encerrando o PID $($p.Id)"; Stop-Process -Id $p.Id -Force }
    }
    Remove-Item -LiteralPath $pidFile -ErrorAction SilentlyContinue
    Restore-Config
    Restore-Videos
    Write-Host "Voltz fechado e config portatil restaurada."
    exit 0
}

Write-Host "use -Launch [-Freeze], -Log, -Hotkeys ou -Close (veja o comentario no topo do script)"
