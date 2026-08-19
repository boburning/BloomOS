[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9_.-]{1,63}$')]
    [string]$Username,
    [string]$DeviceHost = "192.168.1.180",
    [string]$DeviceUser = "root",
    [string]$IdentityFile = "$env:USERPROFILE\.ssh\bloomos_dev_automation_ed25519",
    [string]$KnownHostsFile = "$PSScriptRoot\..\artifacts\mini-tests\plus-ssh-known-hosts"
)

$ErrorActionPreference = "Stop"
foreach ($path in @($IdentityFile, $KnownHostsFile)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required SSH file not found: $path"
    }
}

$sshArguments = @(
    "-o", "BatchMode=yes",
    "-o", "IdentitiesOnly=yes",
    "-o", "StrictHostKeyChecking=yes",
    "-o", "LogLevel=ERROR",
    "-i", $IdentityFile,
    "-o", "UserKnownHostsFile=$KnownHostsFile",
    "$DeviceUser@$DeviceHost"
)

function Invoke-BloomSsh {
    param([string]$Command, [string]$InputText)
    $start = [Diagnostics.ProcessStartInfo]::new("ssh.exe")
    $start.UseShellExecute = $false
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $sshArguments + @($Command)) {
        [void]$start.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::Start($start)
    if ($PSBoundParameters.ContainsKey("InputText")) {
        $process.StandardInput.Write($InputText)
    }
    $process.StandardInput.Close()
    $output = $process.StandardOutput.ReadToEnd()
    $errorOutput = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        throw "BloomOS command failed for '$Command': $errorOutput"
    }
    return $output
}

$token = $null
try {
    $token = (Invoke-BloomSsh -Command "cat /appconfigs/bloom/achievements/credentials").Trim()
    if ([string]::IsNullOrWhiteSpace($token) -or $token.Length -ge 128 -or $token -match '[\x00-\x1f\x7f]') {
        throw "BloomOS has no valid device-local RetroAchievements credential."
    }
    $candidateResponse = Invoke-BloomSsh -Command "/mnt/SDCARD/.tmp_update/bin/bloom-ra catalog candidates" |
        ConvertFrom-Json
    $matched = 0
    foreach ($group in @($candidateResponse.candidates | Group-Object console_id)) {
        $games = @()
        foreach ($candidate in $group.Group) {
            $lookup = Invoke-RestMethod -Method Get `
                -Uri "https://retroachievements.org/dorequest.php?r=gameid&m=$($candidate.content_hash)" `
                -UserAgent "BloomOS/0.1.0 (Windows) bloom-ra-refresh/1.0.0"
            if ($lookup.Success -ne $true -or [int]$lookup.GameID -le 0) {
                continue
            }
            $response = Invoke-RestMethod -Method Post -Uri "https://retroachievements.org/dorequest.php" `
                -UserAgent "BloomOS/0.1.0 (Windows) bloom-ra-refresh/1.0.0" `
                -ContentType "application/x-www-form-urlencoded" `
                -Body @{ r = "patch"; u = $Username; t = $token; g = [string]$lookup.GameID }
            if ($response.Success -ne $true -or $null -eq $response.PatchData) {
                throw "RetroAchievements metadata lookup failed for an identified game."
            }
            $patch = $response.PatchData
            $achievementCount = @($patch.Achievements).Count
            if ($achievementCount -le 0 -or [int]$patch.ConsoleID -ne [int]$candidate.console_id) {
                continue
            }
            $games += [ordered]@{
                Title = [string]$patch.Title
                ID = [int]$patch.ID
                ConsoleID = [int]$patch.ConsoleID
                NumAchievements = $achievementCount
                Hashes = @([string]$candidate.content_hash)
            }
        }
        if ($games.Count -eq 0) {
            continue
        }
        $revision = "connect-$((Get-Date).ToUniversalTime().ToString('yyyyMMdd'))-installed"
        $json = ConvertTo-Json -InputObject @($games) -Compress -Depth 5
        [void](Invoke-BloomSsh `
            -Command "/mnt/SDCARD/.tmp_update/bin/bloom-ra catalog import-installed $($group.Name) $revision" `
            -InputText $json)
        $matched += $games.Count
    }
    [void](Invoke-BloomSsh -Command "/mnt/SDCARD/.tmp_update/bin/bloom-ra scan --all")
    $status = Invoke-BloomSsh -Command "/mnt/SDCARD/.tmp_update/bin/bloom-ra status" | ConvertFrom-Json
    Write-Host "RetroAchievements metadata refreshed for $matched installed games; $($status.identified_games) indexed games are identified."
}
finally {
    $token = $null
}
