# Copyright (c) 2026, Qwertycoin
# SPDX-License-Identifier: BSD-3-Clause

param(
    [Parameter(Mandatory = $true)]
    [string]$DaemonPath,

    [Parameter(Mandatory = $true)]
    [string]$RuntimeBin,

    [Parameter(Mandatory = $true)]
    [string]$TestRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not (Test-Path -LiteralPath $DaemonPath -PathType Leaf)) {
    throw "daemon executable not found"
}
if (-not (Test-Path -LiteralPath $RuntimeBin -PathType Container)) {
    throw "native runtime directory not found"
}
if (Test-Path -LiteralPath $TestRoot) {
    throw "isolated standard-user test root already exists"
}

$suffix = [System.Diagnostics.Process]::GetCurrentProcess().Id
$userName = "qwcacltest$suffix"
$account = "$env:COMPUTERNAME\$userName"
$taskName = "qwc-epose-acl-$suffix"
$runtimeRoot = Join-Path $TestRoot "runtime"
$wrapper = Join-Path $TestRoot "run-standard-user.ps1"
$result = Join-Path $TestRoot "result.txt"
$finished = Join-Path $TestRoot "finished.txt"
$registered = $false
$createdUser = $false

function Quote-PowerShellLiteral([string]$Value) {
    return $Value.Replace("'", "''")
}

try {
    New-Item -ItemType Directory -Path $TestRoot | Out-Null
    $testUser = New-LocalUser -Name $userName -NoPassword -AccountNeverExpires
    $createdUser = $true
    $usersGroup = Get-LocalGroup -SID 'S-1-5-32-545'
    Add-LocalGroupMember -Group $usersGroup -Member $account
    $administrators = @(Get-LocalGroupMember -Group (
        Get-LocalGroup -SID 'S-1-5-32-544'))
    if ($administrators.SID.Value -contains $testUser.SID.Value) {
        throw "temporary test account unexpectedly belongs to Administrators"
    }

    & icacls.exe $TestRoot /inheritance:r |
        Out-Null
    & icacls.exe $TestRoot /grant:r `
        "${account}:(OI)(CI)F" `
        '*S-1-5-18:(OI)(CI)F' `
        '*S-1-5-32-544:(OI)(CI)F' |
        Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "failed to isolate the standard-user test directory"
    }

    $runtimeScript = Quote-PowerShellLiteral (
        (Resolve-Path ./tests/epose/windows_keystore_runtime.ps1).Path)
    $daemon = Quote-PowerShellLiteral ((Resolve-Path $DaemonPath).Path)
    $runtime = Quote-PowerShellLiteral ((Resolve-Path $RuntimeBin).Path)
    $test = Quote-PowerShellLiteral $runtimeRoot
    $resultPath = Quote-PowerShellLiteral $result
    $finishedPath = Quote-PowerShellLiteral $finished
    $wrapperBody = @"
`$ErrorActionPreference = 'Stop'
try {
    `$principal = [Security.Principal.WindowsPrincipal]::new(
        [Security.Principal.WindowsIdentity]::GetCurrent())
    if (`$principal.IsInRole(
            [Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'scheduled keystore test unexpectedly has administrator membership'
    }
    `$env:PATH = '$runtime;' + `$env:PATH
    & '$runtimeScript' -Mode VerifyFixed -DaemonPath '$daemon' -TestRoot '$test' |
        Set-Content -LiteralPath '$resultPath' -Encoding UTF8
    '0' | Set-Content -LiteralPath '$finishedPath' -Encoding ASCII
} catch {
    `$safe = `$_.Exception.Message
    if (`$safe -match '(?i)operator_secret_key|service_secret_key') {
        `$safe = 'standard-user test failed; secret-bearing diagnostic suppressed'
    }
    `$safe = `$safe -replace '(?i)\b[0-9a-f]{64}\b', '<redacted-hex64>'
    `$safe | Set-Content -LiteralPath '$resultPath' -Encoding UTF8
    '1' | Set-Content -LiteralPath '$finishedPath' -Encoding ASCII
}
"@
    Set-Content -LiteralPath $wrapper -Value $wrapperBody -Encoding UTF8

    $pwsh = (Get-Command pwsh.exe -ErrorAction Stop).Source
    $arguments = "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$wrapper`""
    $action = New-ScheduledTaskAction -Execute $pwsh -Argument $arguments
    $principal = New-ScheduledTaskPrincipal `
        -UserId $account -LogonType S4U -RunLevel Limited
    $settings = New-ScheduledTaskSettingsSet `
        -ExecutionTimeLimit (New-TimeSpan -Minutes 15) `
        -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
    Register-ScheduledTask -TaskName $taskName -Action $action `
        -Principal $principal -Settings $settings | Out-Null
    $registered = $true
    Start-ScheduledTask -TaskName $taskName

    $deadline = [DateTime]::UtcNow.AddMinutes(15)
    while ([DateTime]::UtcNow -lt $deadline -and
            -not (Test-Path -LiteralPath $finished -PathType Leaf)) {
        Start-Sleep -Milliseconds 500
    }
    if (-not (Test-Path -LiteralPath $finished -PathType Leaf)) {
        $info = Get-ScheduledTaskInfo -TaskName $taskName
        throw "standard-user task timed out (last result $($info.LastTaskResult))"
    }
    $exitCode = (Get-Content -LiteralPath $finished -Raw).Trim()
    $output = if (Test-Path -LiteralPath $result -PathType Leaf) {
        (Get-Content -LiteralPath $result -Raw).Trim()
    } else {
        "no safe standard-user diagnostic was available"
    }
    if ($output -match '(?i)operator_secret_key|service_secret_key') {
        throw "secret key material appeared in the standard-user result"
    }
    if ($exitCode -ne "0") {
        throw "standard-user runtime failed: $output"
    }
    if ($output -notmatch 'fixed_runtime=pass starts=4 identities=stable content_sha256=stable broad_acl=rejected acl_only_repair=pass') {
        throw "standard-user runtime did not report the complete proof"
    }
    Write-Output "standard_user_runtime=pass account=non_administrator starts=4 identities=stable"
} finally {
    if ($registered) {
        Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false `
            -ErrorAction SilentlyContinue
    }
    if ($createdUser) {
        Remove-LocalUser -Name $userName -ErrorAction SilentlyContinue
    }
}
