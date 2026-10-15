<#
  Cliente minimo do obs-websocket (protocolo v5, sem senha) para testes automatizados da casca.
  Conecta, identifica e envia os pedidos em sequencia; imprime cada resposta como uma linha JSON.

  Os pedidos sao um array JSON de objetos { "requestType": "...", "requestData": { ... } }.
  Strings no formato "{{N.campo}}" sao trocadas pelo campo `campo` do responseData da resposta N
  (indice a partir de 0), o que permite encadear pedidos (ex. usar o nome da cena atual).

  Requer o servidor ligado sem autenticacao, ex. config portatil
  config/Voltz Studio/plugin_config/obs-websocket/config.json com
  {"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4455}

  Tipos de entrada sao versionados (ex. texto = "text_gdiplus_v3", tela = "monitor_capture").
  Defina $env:VOLTZ_WS_DEBUG=1 para ver as mensagens cruas recebidas.

  Uso (passar JSON por -Requests via `powershell -File` perde as aspas; prefira arquivo ou chamada direta):
    powershell -ExecutionPolicy Bypass -File tools/voltz-ws.ps1 -RequestsFile pedidos.json
    & tools/voltz-ws.ps1 -Requests '[{"requestType":"GetVersion"}]'   # de dentro do PowerShell
#>
param(
    [string]$Requests,
    [string]$RequestsFile,
    [int]$Port = 4455,
    [int]$TimeoutSeconds = 10
)

$ErrorActionPreference = "Stop"

if ($RequestsFile) { $Requests = Get-Content -Raw -Encoding UTF8 $RequestsFile }
if (-not $Requests) { Write-Host "FALHOU informe -Requests ou -RequestsFile"; exit 1 }
# No PowerShell 5.1 o ConvertFrom-Json devolve o array inteiro como um objeto so; desenrola.
$pending = @($Requests | ConvertFrom-Json | ForEach-Object { $_ })

$socket = New-Object System.Net.WebSockets.ClientWebSocket
$socket.Options.AddSubProtocol("obswebsocket.json")
$cts = New-Object System.Threading.CancellationTokenSource
$cts.CancelAfter([TimeSpan]::FromSeconds($TimeoutSeconds * ($pending.Count + 1)))

function Receive-Message {
    $buffer = New-Object byte[] 65536
    $stream = New-Object System.IO.MemoryStream
    do {
        $segment = New-Object System.ArraySegment[byte] -ArgumentList (, $buffer)
        $result = $socket.ReceiveAsync($segment, $cts.Token).GetAwaiter().GetResult()
        if ($result.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Close) {
            throw "conexao fechada pelo servidor: $($result.CloseStatus) $($result.CloseStatusDescription)"
        }
        $stream.Write($buffer, 0, $result.Count)
    } while (-not $result.EndOfMessage)
    $text = [System.Text.Encoding]::UTF8.GetString($stream.ToArray())
    if ($env:VOLTZ_WS_DEBUG) { Write-Host "<< $text" }
    return $text | ConvertFrom-Json
}

function Send-Message($message) {
    $json = $message | ConvertTo-Json -Depth 20 -Compress
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    $segment = New-Object System.ArraySegment[byte] -ArgumentList (, $bytes)
    [void]$socket.SendAsync($segment, [System.Net.WebSockets.WebSocketMessageType]::Text, $true, $cts.Token).GetAwaiter().GetResult()
}

# Recebe mensagens ate chegar a de `op` esperado (eventos op 5 sao ignorados).
function Receive-Op([int]$op) {
    while ($true) {
        $message = Receive-Message
        if ($message.op -eq $op) { return $message }
    }
}

$responses = @()
$failed = 0
try {
    [void]$socket.ConnectAsync([Uri]"ws://127.0.0.1:$Port", $cts.Token).GetAwaiter().GetResult()
    $hello = Receive-Op 0
    if ($hello.d.authentication) { throw "o servidor exige senha; este cliente so funciona sem autenticacao" }
    Send-Message @{ op = 1; d = @{ rpcVersion = 1; eventSubscriptions = 0 } }
    [void](Receive-Op 2)

    for ($i = 0; $i -lt $pending.Count; $i++) {
        $request = $pending[$i]
        $dataJson = if ($request.requestData) { $request.requestData | ConvertTo-Json -Depth 20 -Compress } else { "{}" }
        # "{{N.campo}}" sozinho na string vira o valor com o tipo original (numero continua numero);
        # dentro de um texto maior, vira texto.
        $dataJson = [regex]::Replace($dataJson, '"\{\{(\d+)\.([A-Za-z0-9_]+)\}\}"', {
                param($m)
                $value = $responses[[int]$m.Groups[1].Value].d.responseData.($m.Groups[2].Value)
                return (ConvertTo-Json -InputObject $value -Compress)
            })
        $dataJson = [regex]::Replace($dataJson, '\{\{(\d+)\.([A-Za-z0-9_]+)\}\}', {
                param($m)
                $value = $responses[[int]$m.Groups[1].Value].d.responseData.($m.Groups[2].Value)
                return ([string]$value).Replace('\', '\\').Replace('"', '\"')
            })
        $requestId = "voltz-$i"
        Send-Message @{ op = 6; d = @{ requestType = $request.requestType; requestId = $requestId; requestData = ($dataJson | ConvertFrom-Json) } }
        do { $response = Receive-Op 7 } while ($response.d.requestId -ne $requestId)
        $responses += $response
        $status = $response.d.requestStatus
        $line = [ordered]@{ index = $i; requestType = $request.requestType; ok = $status.result; code = $status.code; comment = $status.comment; data = $response.d.responseData }
        Write-Host ($line | ConvertTo-Json -Depth 20 -Compress)
        if (-not $status.result) { $failed++ }
    }
} catch {
    Write-Host "FALHOU $($_.Exception.Message) (linha $($_.InvocationInfo.ScriptLineNumber))"
    exit 1
} finally {
    if ($socket.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
        try { [void]$socket.CloseAsync([System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure, "", [System.Threading.CancellationToken]::None).GetAwaiter().GetResult() } catch {}
    }
    $socket.Dispose()
}

if ($failed -gt 0) { Write-Host "FALHOU $failed pedido(s)"; exit 1 }
Write-Host "WS OK"
