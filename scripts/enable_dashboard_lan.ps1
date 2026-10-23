# Run in an elevated Windows PowerShell. Does not restart WSL or PopX.
# TargetAddress must be reachable from Windows: WSL IPv4, or a published container endpoint.
param(
    [Parameter(Mandatory=$true)][string]$TargetAddress,
    [int]$TargetPort = 8080,
    [int]$ListenPort = 8080
)
$ErrorActionPreference = 'Stop'
if ($TargetPort -lt 1 -or $TargetPort -gt 65535 -or $ListenPort -lt 1 -or $ListenPort -gt 65535) {
    throw 'Porta non valida'
}
$null = [System.Net.IPAddress]::Parse($TargetAddress)
if (-not (Test-NetConnection -ComputerName $TargetAddress -Port $TargetPort -InformationLevel Quiet)) {
    throw "Windows non raggiunge ${TargetAddress}:${TargetPort}. Verificare IP WSL e pubblicazione della porta del container prima di proseguire."
}
$existing = netsh interface portproxy show v4tov4
if ($existing -match "(?m)^\s*0\.0\.0\.0\s+$ListenPort\s+") {
    throw "Esiste gia un inoltro sulla porta $ListenPort. Verificarlo prima di modificarlo."
}
netsh interface portproxy add v4tov4 listenaddress=0.0.0.0 listenport=$ListenPort connectaddress=$TargetAddress connectport=$TargetPort
if ($LASTEXITCODE -ne 0) { throw 'Creazione inoltro fallita' }
New-NetFirewallRule -DisplayName "PopX dashboard LAN $ListenPort" -Direction Inbound -Action Allow -Protocol TCP -LocalPort $ListenPort -Profile Private -RemoteAddress LocalSubnet
Write-Host "Dal telefono sulla stessa LAN: http://<IPv4-LAN-del-PC>:$ListenPort"
Write-Host "Rimozione: netsh interface portproxy delete v4tov4 listenaddress=0.0.0.0 listenport=$ListenPort"
Write-Host "Rimozione firewall: Remove-NetFirewallRule -DisplayName 'PopX dashboard LAN $ListenPort'"
