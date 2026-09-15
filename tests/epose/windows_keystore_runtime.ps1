# Copyright (c) 2026, Qwertycoin
# SPDX-License-Identifier: BSD-3-Clause

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("ReproduceReleaseBug", "VerifyFixed")]
    [string]$Mode,

    [Parameter(Mandatory = $true)]
    [string]$DaemonPath,

    [Parameter(Mandatory = $true)]
    [string]$TestRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$RewardAddress = "QWC1h8SDGxBj74KgQDGKRKAjTfWJywnvM1ZRGv9yX9o89N8qGedHKLheKPmtkLpTmpBWGDT3ZuLUmNVvz3LmU5LrA3gGed58sq"
$ReadyPattern = "EPoSE-v2 service authority ready: identity ([0-9a-f]{64}), service key ([0-9a-f]{64})"
$PermissionPattern = "Failed to load EPoSE-v2 keystore: .*permissions"
$RuntimeFailurePattern = "Failed to initialize EPoSE-v2 service runtime"

if (-not (Test-Path -LiteralPath $DaemonPath -PathType Leaf)) {
    throw "daemon executable not found"
}
if (Test-Path -LiteralPath $TestRoot) {
    throw "isolated test root already exists"
}
New-Item -ItemType Directory -Path $TestRoot | Out-Null

$DataDir = Join-Path $TestRoot "data"
$Keystore = Join-Path $TestRoot "epose-v2.keys"
New-Item -ItemType Directory -Path $DataDir | Out-Null

function Read-Log([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }
    $share = [System.IO.FileShare]::ReadWrite -bor [System.IO.FileShare]::Delete
    try {
        $stream = [System.IO.File]::Open(
            $Path,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            $share)
        try {
            $reader = [System.IO.StreamReader]::new($stream)
            try {
                return $reader.ReadToEnd()
            } finally {
                $reader.Dispose()
            }
        } finally {
            $stream.Dispose()
        }
    } catch [System.IO.IOException] {
        return ""
    }
}

function Get-SafeDiagnosticExcerpt([string[]]$Paths) {
    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($path in $Paths) {
        $text = Read-Log $path
        if ([string]::IsNullOrWhiteSpace($text)) {
            continue
        }
        foreach ($line in ($text -split "`r?`n")) {
            if ($line -match '(?i)operator_secret_key|service_secret_key') {
                continue
            }
            $safe = $line -replace '(?i)\b[0-9a-f]{64}\b', '<redacted-hex64>'
            if (-not [string]::IsNullOrWhiteSpace($safe)) {
                $lines.Add($safe)
            }
        }
    }
    if ($lines.Count -eq 0) {
        return "no safe daemon diagnostics were available"
    }
    return (($lines | Select-Object -Last 40) -join "`n")
}

function Wait-ForRpc([int]$Port, [System.Diagnostics.Process]$Process) {
    $deadline = [DateTime]::UtcNow.AddSeconds(90)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($Process.HasExited) {
            throw "daemon exited before its local RPC became ready"
        }
        try {
            $request = @{
                UseBasicParsing = $true
                Method = "Post"
                Uri = "http://127.0.0.1:$Port/get_height"
                ContentType = "application/json"
                Body = "{}"
                TimeoutSec = 2
            }
            Invoke-WebRequest @request | Out-Null
            return
        } catch {
            Start-Sleep -Milliseconds 250
        }
    }
    throw "local daemon RPC did not become ready"
}

function Stop-DaemonCleanly([int]$Port, [System.Diagnostics.Process]$Process) {
    $request = @{
        UseBasicParsing = $true
        Method = "Post"
        Uri = "http://127.0.0.1:$Port/stop_daemon"
        ContentType = "application/json"
        Body = "{}"
        TimeoutSec = 10
    }
    Invoke-WebRequest @request | Out-Null
    if (-not $Process.WaitForExit(30000)) {
        throw "daemon did not exit after the local stop_daemon request"
    }
    if ($Process.ExitCode -ne 0) {
        throw "daemon returned a non-zero exit code after a clean stop"
    }
}

function Start-EposeDaemon(
    [int]$Run,
    [ValidateSet("Ready", "PermissionFailure")]
    [string]$Expected,
    [switch]$Repair
) {
    $rpcPort = 28190 + $Run
    $p2pPort = 28290 + $Run
    $restrictedPort = 28390 + $Run
    $log = Join-Path $TestRoot ("run-{0}.log" -f $Run)
    $stdout = Join-Path $TestRoot ("run-{0}.stdout" -f $Run)
    $stderr = Join-Path $TestRoot ("run-{0}.stderr" -f $Run)
    $arguments = @(
        "--data-dir=$DataDir",
        "--log-file=$log",
        "--log-level=1",
        "--non-interactive",
        "--disable-dns-checkpoints",
        "--hide-my-port",
        "--no-igd",
        "--out-peers=0",
        "--in-peers=0",
        "--p2p-bind-ip=127.0.0.1",
        "--p2p-bind-port=$p2pPort",
        "--rpc-bind-ip=127.0.0.1",
        "--rpc-bind-port=$rpcPort",
        "--rpc-restricted-bind-ip=127.0.0.1",
        "--rpc-restricted-bind-port=$restrictedPort",
        "--epose-v2-service",
        "--epose-v2-keystore=$Keystore",
        "--epose-v2-reward-address=$RewardAddress",
        "--epose-v2-endpoint-host=node.example.org",
        "--epose-v2-endpoint-port=8198"
    )
    if ($Repair) {
        $arguments += "--epose-v2-repair-keystore-permissions"
    }

    $start = @{
        FilePath = $DaemonPath
        ArgumentList = $arguments
        WorkingDirectory = (Split-Path -Parent $DaemonPath)
        PassThru = $true
        RedirectStandardOutput = $stdout
        RedirectStandardError = $stderr
    }
    $process = Start-Process @start
    $deadline = [DateTime]::UtcNow.AddSeconds(120)
    $logText = ""
    while ([DateTime]::UtcNow -lt $deadline) {
        $logText = Read-Log $log
        if ($Expected -eq "Ready" -and $logText -match $ReadyPattern) {
            $identity = @($Matches[1], $Matches[2])
            Wait-ForRpc $rpcPort $process
            Stop-DaemonCleanly $rpcPort $process
            return $identity
        }
        if (
            $Expected -eq "PermissionFailure" -and
            $logText -match $PermissionPattern -and
            $logText -match $RuntimeFailurePattern
        ) {
            if (-not $process.WaitForExit(30000)) {
                throw "daemon reported unsafe permissions but did not exit"
            }
            if ($process.ExitCode -eq 0) {
                throw "daemon accepted unsafe permissions with a zero exit code"
            }
            return @()
        }
        if ($process.HasExited) {
            $diagnostic = Get-SafeDiagnosticExcerpt @($stderr, $stdout, $log)
            throw "daemon exited with code $($process.ExitCode) before the expected keystore outcome:`n$diagnostic"
        }
        Start-Sleep -Milliseconds 250
    }
    throw "timed out waiting for the expected keystore outcome"
}

function Assert-SecureAcl([string]$Path) {
    $acl = Get-Acl -LiteralPath $Path
    if (-not $acl.AreAccessRulesProtected) {
        throw "new keystore DACL is not protected from inheritance"
    }
    $currentSid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $ownerSid = $acl.Owner
    try {
        $ownerSid = ([System.Security.Principal.NTAccount]$acl.Owner).Translate(
            [System.Security.Principal.SecurityIdentifier]).Value
    } catch {
        # Get-Acl may already return the owner as a SID.
    }
    if ($ownerSid -ne $currentSid) {
        throw "new keystore owner is not the current account"
    }
    $allowed = @($currentSid, "S-1-5-18", "S-1-5-32-544")
    $rules = $acl.GetAccessRules(
        $true, $true, [System.Security.Principal.SecurityIdentifier])
    $currentHasControl = $false
    foreach ($rule in $rules) {
        if ($rule.IsInherited) {
            throw "new keystore DACL contains an inherited rule"
        }
        if ($rule.AccessControlType -ne
                [System.Security.AccessControl.AccessControlType]::Allow) {
            throw "new keystore DACL contains a non-allow rule"
        }
        $sid = $rule.IdentityReference.Value
        if ($allowed -notcontains $sid) {
            throw "new keystore DACL grants an unexpected principal"
        }
        $fullControl = [System.Security.AccessControl.FileSystemRights]::FullControl
        if ($sid -eq $currentSid -and
                ($rule.FileSystemRights -band $fullControl) -eq $fullControl) {
            $currentHasControl = $true
        }
    }
    if (-not $currentHasControl) {
        throw "new keystore DACL does not grant the current account full control"
    }
}

if ($Mode -eq "ReproduceReleaseBug") {
    $null = Start-EposeDaemon -Run 1 -Expected Ready
    $hashBefore = (Get-FileHash -Algorithm SHA256 -LiteralPath $Keystore).Hash
    $null = Start-EposeDaemon -Run 2 -Expected PermissionFailure
    $hashAfter = (Get-FileHash -Algorithm SHA256 -LiteralPath $Keystore).Hash
    if ($hashBefore -ne $hashAfter) {
        throw "published release changed the keystore while reproducing the load failure"
    }
    Write-Output "published_release_reproduction=pass first_start=ready second_start=permissions_rejected content_sha256=stable"
    exit 0
}

$firstIdentity = Start-EposeDaemon -Run 1 -Expected Ready
$hashBefore = (Get-FileHash -Algorithm SHA256 -LiteralPath $Keystore).Hash
Assert-SecureAcl $Keystore
for ($run = 2; $run -le 4; ++$run) {
    $identity = Start-EposeDaemon -Run $run -Expected Ready
    if ($identity[0] -ne $firstIdentity[0] -or $identity[1] -ne $firstIdentity[1]) {
        throw "public EPoSE identity changed across process restarts"
    }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $Keystore).Hash -ne $hashBefore) {
        throw "keystore content changed across process restarts"
    }
}

& icacls.exe $Keystore /grant '*S-1-1-0:(R)' | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "failed to prepare an intentionally broad test ACL"
}
$null = Start-EposeDaemon -Run 5 -Expected PermissionFailure
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $Keystore).Hash -ne $hashBefore) {
    throw "unsafe-ACL rejection changed keystore content"
}
$repairedIdentity = Start-EposeDaemon -Run 6 -Expected Ready -Repair
if ($repairedIdentity[0] -ne $firstIdentity[0] -or
        $repairedIdentity[1] -ne $firstIdentity[1]) {
    throw "public EPoSE identity changed during ACL-only repair"
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $Keystore).Hash -ne $hashBefore) {
    throw "ACL-only repair changed keystore content"
}
Assert-SecureAcl $Keystore
Write-Output "fixed_runtime=pass starts=4 identities=stable content_sha256=stable broad_acl=rejected acl_only_repair=pass"
