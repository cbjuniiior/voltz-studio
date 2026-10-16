# Aciona um botao/elemento do Voltz Studio por UI Automation (Windows PowerShell 5.1).
# Procura em TODAS as janelas de topo visiveis do processo (casca, dialogos, modo classico).
# Uso: voltz-uia.ps1 -ProcessId <pid> -Name "Ajustes" [-TimeoutSeconds 10] [-List]
#   -List imprime, para cada janela, "== janela | <titulo> | hwnd=<handle>" e depois os descendentes.
param(
    [Parameter(Mandatory = $true)][int]$ProcessId,
    [string]$Name = "",
    [int]$TimeoutSeconds = 10,
    [switch]$List
)

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

function Fail([string]$why) {
    Write-Output "FALHOU $why"
    exit 1
}

if (-not $List -and [string]::IsNullOrEmpty($Name)) { Fail "informe -Name ou -List" }

$AE = [System.Windows.Automation.AutomationElement]
$Scope = [System.Windows.Automation.TreeScope]
$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$pidCond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $ProcessId)

function Get-ProcessWindows {
    return @($AE::RootElement.FindAll($Scope::Children, $pidCond))
}

$wins = @()
while ($wins.Count -eq 0 -and (Get-Date) -lt $deadline) {
    $wins = Get-ProcessWindows
    if ($wins.Count -eq 0) { Start-Sleep -Milliseconds 300 }
}
if ($wins.Count -eq 0) { Fail "nenhuma janela do processo $ProcessId" }

if ($List) {
    foreach ($win in $wins) {
        Write-Output ("== janela | {0} | hwnd={1}" -f $win.Current.Name, $win.Current.NativeWindowHandle)
        $all = $win.FindAll($Scope::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
        foreach ($e in $all) {
            $c = $e.Current
            Write-Output ("{0} | {1} | {2}" -f $c.ControlType.ProgrammaticName, $c.Name, $c.ClassName)
        }
    }
    exit 0
}

# Descendente com o nome pedido em qualquer janela do processo (aguarda ate o timeout)
$nameCond = New-Object System.Windows.Automation.PropertyCondition($AE::NameProperty, $Name)
$el = $null
while ($null -eq $el -and (Get-Date) -lt $deadline) {
    foreach ($win in Get-ProcessWindows) {
        $el = $win.FindFirst($Scope::Descendants, $nameCond)
        if ($null -ne $el) { break }
    }
    if ($null -eq $el) { Start-Sleep -Milliseconds 300 }
}
if ($null -eq $el) { Fail "elemento '$Name' nao encontrado em $TimeoutSeconds s" }

try {
    $pattern = $null
    if ($el.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern, [ref]$pattern)) {
        $pattern.Invoke()
    }
    elseif ($el.TryGetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern, [ref]$pattern)) {
        $pattern.Select()
    }
    elseif ($el.TryGetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern, [ref]$pattern)) {
        $pattern.Toggle()
    }
    else {
        Fail "'$Name' nao expoe Invoke/SelectionItem/Toggle"
    }
}
catch {
    Fail "erro ao acionar '$Name': $($_.Exception.Message)"
}

Write-Output "OK $Name"
exit 0
