[CmdletBinding()]
param(
    [string]$DeviceHost = "192.168.1.180",
    [string]$DeviceUser = "root",
    [string]$IdentityFile = "$env:USERPROFILE\.ssh\bloomos_dev_automation_ed25519",
    [string]$KnownHostsFile = "$PSScriptRoot\..\artifacts\mini-tests\plus-ssh-known-hosts",
    [ValidateSet("softcore", "hardcore")]
    [string]$Mode = "softcore",
    [ValidateSet("disabled", "automatic")]
    [string]$OfflineCasual = "disabled"
)

$ErrorActionPreference = "Stop"
if ($Mode -eq "hardcore" -and $OfflineCasual -eq "automatic") {
    throw "Offline Casual cannot be enabled with Hardcore."
}
if (-not (Test-Path -LiteralPath $IdentityFile -PathType Leaf)) {
    throw "SSH identity file not found: $IdentityFile"
}
if (-not (Test-Path -LiteralPath $KnownHostsFile -PathType Leaf)) {
    throw "Pinned SSH known-hosts file not found: $KnownHostsFile"
}

$username = (Read-Host "RetroAchievements username").Trim()
if ($username -notmatch '^[A-Za-z0-9_.-]{1,63}$') {
    throw "The username contains unsupported characters."
}
$securePassword = Read-Host "RetroAchievements password" -AsSecureString
$passwordPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($securePassword)
$password = $null
$token = $null
try {
    $password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($passwordPointer)
    $response = Invoke-RestMethod -Method Post -Uri "https://retroachievements.org/dorequest.php" `
        -ContentType "application/x-www-form-urlencoded" `
        -Body @{ r = "login2"; u = $username; p = $password }
    if ($response.Success -ne $true -or [string]::IsNullOrWhiteSpace([string]$response.Token)) {
        throw "RetroAchievements rejected the login."
    }
    $token = [string]$response.Token
    if ($token.Length -ge 128 -or $token -match '[\x00-\x1f\x7f]') {
        throw "RetroAchievements returned an invalid token."
    }

    $ssh = [Diagnostics.ProcessStartInfo]::new("ssh.exe")
    $ssh.UseShellExecute = $false
    $ssh.RedirectStandardInput = $true
    $ssh.RedirectStandardOutput = $true
    $ssh.RedirectStandardError = $true
    foreach ($argument in @(
        "-o", "BatchMode=yes",
        "-o", "IdentitiesOnly=yes",
        "-o", "StrictHostKeyChecking=yes",
        "-o", "LogLevel=ERROR",
        "-i", $IdentityFile,
        "-o", "UserKnownHostsFile=$KnownHostsFile",
        "$DeviceUser@$DeviceHost",
        "/mnt/SDCARD/.tmp_update/bin/bloom-ra account configure $username $Mode $OfflineCasual"
    )) {
        [void]$ssh.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::Start($ssh)
    $process.StandardInput.WriteLine($token)
    $process.StandardInput.Close()
    $output = $process.StandardOutput.ReadToEnd()
    $errorOutput = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        throw "BloomOS rejected the account configuration: $errorOutput"
    }
    $status = $output | ConvertFrom-Json
    if ($status.authenticated -ne $true) {
        throw "BloomOS did not confirm authenticated account storage."
    }
    Write-Host "RetroAchievements account configured. Password was not stored; token output was redacted."
}
finally {
    if ($null -ne $passwordPointer) {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordPointer)
    }
    $password = $null
    $token = $null
    $securePassword.Dispose()
}
