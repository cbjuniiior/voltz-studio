# Aciona um botao/elemento da casca do Voltz Studio por UI Automation (Windows PowerShell 5.1).
# Uso: voltz-uia.ps1 -ProcessId <pid> -Name "Ajustes" [-TimeoutSeconds 10] [-List]
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
$deadline = (Get-Date).AddSeconds($TimeoutSeconds)

# Janela principal do processo
$pidCond = New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $ProcessId)
$win = $null
while ($null -eq $win -and (Get-Date) -lt $deadline) {
    $win = $AE::RootElement.FindFirst([System.Windows.Automation.TreeScope]::Children, $pidCond)
    if ($null -eq $win) { Start-Sleep -Milliseconds 300 }
}
if ($null -eq $win) { Fail "janela do processo $ProcessId nao encontrada" }

if ($List) {
    $all = $win.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($e in $all) {
        $c = $e.Current
        Write-Output ("{0} | {1} | {2}" -f $c.ControlType.ProgrammaticName, $c.Name, $c.ClassName)
    }
    exit 0
}

# Descendente com o nome pedido (aguarda ate o timeout)
$nameCond = New-Object System.Windows.Automation.PropertyCondition($AE::NameProperty, $Name)
$el = $null
while ($null -eq $el -and (Get-Date) -lt $deadline) {
    $el = $win.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $nameCond)
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
