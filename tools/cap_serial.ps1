param(
    [string]$Port = "COM19",
    [int]$Baud = 115200,
    [int]$Seconds = 14
)

$port = [System.IO.Ports.SerialPort]::new($Port, $Baud)
$port.Parity = [System.IO.Ports.Parity]::None
$port.DataBits = 8
$port.StopBits = [System.IO.Ports.StopBits]::One
$port.ReadTimeout = 500
$port.WriteTimeout = 500
$port.Open()
try {
    # 脉冲 DTR 低->高，触发 ESP32 复位（EN 经自动复位电容接到 DTR）
    $port.DtrEnable = $true
    $port.RtsEnable = $true
    Start-Sleep -Milliseconds 50
    $port.DtrEnable = $false
    Start-Sleep -Milliseconds 100
    $port.DtrEnable = $true
    Start-Sleep -Milliseconds 200

    $end = [DateTime]::Now.AddSeconds($Seconds)
    $buf = New-Object System.Text.StringBuilder
    while ([DateTime]::Now -lt $end) {
        try {
            if ($port.BytesToRead -gt 0) {
                $s = $port.ReadExisting()
                if ($s.Length -gt 0) {
                    [void]$buf.Append($s)
                    [Console]::Out.Write($s)
                    [Console]::Out.Flush()
                }
            }
        } catch [System.TimeoutException] {
            # 忽略超时
        }
        Start-Sleep -Milliseconds 30
    }
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot "serial.log"), $buf.ToString(), [System.Text.Encoding]::ASCII)
} finally {
    if ($port.IsOpen) { $port.Close() }
    $port.Dispose()
}
