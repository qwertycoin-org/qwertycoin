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

Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.Principal;

public static class QwcLsaRights
{
    [StructLayout(LayoutKind.Sequential)]
    private struct LSA_OBJECT_ATTRIBUTES
    {
        public int Length;
        public IntPtr RootDirectory;
        public IntPtr ObjectName;
        public uint Attributes;
        public IntPtr SecurityDescriptor;
        public IntPtr SecurityQualityOfService;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct LSA_UNICODE_STRING
    {
        public ushort Length;
        public ushort MaximumLength;
        public IntPtr Buffer;
    }

    [DllImport("advapi32.dll")]
    private static extern uint LsaOpenPolicy(
        IntPtr systemName,
        ref LSA_OBJECT_ATTRIBUTES objectAttributes,
        uint desiredAccess,
        out IntPtr policyHandle);

    [DllImport("advapi32.dll")]
    private static extern uint LsaAddAccountRights(
        IntPtr policyHandle,
        IntPtr accountSid,
        LSA_UNICODE_STRING[] userRights,
        uint countOfRights);

    [DllImport("advapi32.dll")]
    private static extern uint LsaRemoveAccountRights(
        IntPtr policyHandle,
        IntPtr accountSid,
        [MarshalAs(UnmanagedType.U1)]
        bool allRights,
        LSA_UNICODE_STRING[] userRights,
        uint countOfRights);

    [DllImport("advapi32.dll")]
    private static extern uint LsaNtStatusToWinError(uint status);

    [DllImport("advapi32.dll")]
    private static extern uint LsaClose(IntPtr policyHandle);

    public static void SetBatchLogonRight(string sidText, bool add)
    {
        const uint POLICY_CREATE_ACCOUNT = 0x00000010;
        const uint POLICY_LOOKUP_NAMES = 0x00000800;
        var attributes = new LSA_OBJECT_ATTRIBUTES();
        attributes.Length = Marshal.SizeOf(attributes);
        IntPtr policy;
        uint status = LsaOpenPolicy(
            IntPtr.Zero,
            ref attributes,
            POLICY_CREATE_ACCOUNT | POLICY_LOOKUP_NAMES,
            out policy);
        if (status != 0)
            throw new Win32Exception((int)LsaNtStatusToWinError(status));

        IntPtr sid = IntPtr.Zero;
        IntPtr rightBuffer = IntPtr.Zero;
        try
        {
            var securityIdentifier = new SecurityIdentifier(sidText);
            var sidBytes = new byte[securityIdentifier.BinaryLength];
            securityIdentifier.GetBinaryForm(sidBytes, 0);
            sid = Marshal.AllocHGlobal(sidBytes.Length);
            Marshal.Copy(sidBytes, 0, sid, sidBytes.Length);

            const string rightName = "SeBatchLogonRight";
            rightBuffer = Marshal.StringToHGlobalUni(rightName);
            var right = new LSA_UNICODE_STRING
            {
                Length = checked((ushort)(rightName.Length * sizeof(char))),
                MaximumLength = checked((ushort)((rightName.Length + 1) * sizeof(char))),
                Buffer = rightBuffer
            };
            var rights = new[] { right };
            status = add
                ? LsaAddAccountRights(policy, sid, rights, 1)
                : LsaRemoveAccountRights(policy, sid, false, rights, 1);
            if (status != 0)
                throw new Win32Exception((int)LsaNtStatusToWinError(status));
        }
        finally
        {
            if (rightBuffer != IntPtr.Zero)
                Marshal.FreeHGlobal(rightBuffer);
            if (sid != IntPtr.Zero)
                Marshal.FreeHGlobal(sid);
            LsaClose(policy);
        }
    }
}
'@

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
$batchLogonGranted = $false

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
    [QwcLsaRights]::SetBatchLogonRight($testUser.SID.Value, $true)
    $batchLogonGranted = $true

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
    if ($batchLogonGranted) {
        [QwcLsaRights]::SetBatchLogonRight($testUser.SID.Value, $false)
    }
    if ($createdUser) {
        Remove-LocalUser -Name $userName -ErrorAction SilentlyContinue
    }
}
