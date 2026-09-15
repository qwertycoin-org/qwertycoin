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
using System.Security.Cryptography;
using System.Text;

public static class QwcStandardUserProcess
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct USER_INFO_1
    {
        public string usri1_name;
        public string usri1_password;
        public uint usri1_password_age;
        public uint usri1_priv;
        public string usri1_home_dir;
        public string usri1_comment;
        public uint usri1_flags;
        public string usri1_script_path;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct LOCALGROUP_MEMBERS_INFO_3
    {
        public string lgrmi3_domainandname;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct STARTUPINFO
    {
        public int cb;
        public string lpReserved;
        public string lpDesktop;
        public string lpTitle;
        public uint dwX;
        public uint dwY;
        public uint dwXSize;
        public uint dwYSize;
        public uint dwXCountChars;
        public uint dwYCountChars;
        public uint dwFillAttribute;
        public uint dwFlags;
        public short wShowWindow;
        public short cbReserved2;
        public IntPtr lpReserved2;
        public IntPtr hStdInput;
        public IntPtr hStdOutput;
        public IntPtr hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct PROCESS_INFORMATION
    {
        public IntPtr hProcess;
        public IntPtr hThread;
        public uint dwProcessId;
        public uint dwThreadId;
    }

    [DllImport("netapi32.dll", CharSet = CharSet.Unicode)]
    private static extern uint NetUserAdd(
        string serverName,
        uint level,
        ref USER_INFO_1 userInfo,
        out uint parameterError);

    [DllImport("netapi32.dll", CharSet = CharSet.Unicode)]
    private static extern uint NetUserDel(string serverName, string userName);

    [DllImport("netapi32.dll", CharSet = CharSet.Unicode)]
    private static extern uint NetLocalGroupAddMembers(
        string serverName,
        string groupName,
        uint level,
        ref LOCALGROUP_MEMBERS_INFO_3 memberInfo,
        uint totalEntries);

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CreateProcessWithLogonW(
        string userName,
        string domain,
        string password,
        uint logonFlags,
        string applicationName,
        StringBuilder commandLine,
        uint creationFlags,
        IntPtr environment,
        string currentDirectory,
        ref STARTUPINFO startupInfo,
        out PROCESS_INFORMATION processInformation);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetExitCodeProcess(IntPtr process, out uint exitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool TerminateProcess(IntPtr process, uint exitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr handle);

    private static string accountPassword;
    private static string accountName;

    private static string GeneratePassword()
    {
        var bytes = new byte[32];
        using (var generator = RandomNumberGenerator.Create())
            generator.GetBytes(bytes);
        return Convert.ToBase64String(bytes) + "aA1!";
    }

    public static void Create(string userName, string qualifiedName, string usersGroup)
    {
        const uint USER_PRIV_USER = 1;
        const uint UF_SCRIPT = 0x0001;
        const uint UF_NORMAL_ACCOUNT = 0x0200;
        const uint UF_DONT_EXPIRE_PASSWD = 0x10000;
        if (accountPassword != null)
            throw new InvalidOperationException("a temporary test account is already active");

        string generatedPassword = GeneratePassword();
        var userInfo = new USER_INFO_1 {
            usri1_name = userName,
            usri1_password = generatedPassword,
            usri1_password_age = 0,
            usri1_priv = USER_PRIV_USER,
            usri1_home_dir = null,
            usri1_comment = "Temporary QWC EPoSE ACL regression account",
            usri1_flags = UF_SCRIPT | UF_NORMAL_ACCOUNT | UF_DONT_EXPIRE_PASSWD,
            usri1_script_path = null
        };
        uint parameterError;
        uint result = NetUserAdd(null, 1, ref userInfo, out parameterError);
        if (result != 0)
            throw new Win32Exception((int)result,
                "failed to create the temporary standard Windows user");

        var member = new LOCALGROUP_MEMBERS_INFO_3 {
            lgrmi3_domainandname = qualifiedName
        };
        result = NetLocalGroupAddMembers(null, usersGroup, 3, ref member, 1);
        if (result != 0)
        {
            NetUserDel(null, userName);
            throw new Win32Exception((int)result,
                "failed to add the temporary account to the local Users group");
        }
        accountPassword = generatedPassword;
        accountName = userName;
    }

    public static int Run(
        string userName,
        string application,
        string arguments,
        string currentDirectory)
    {
        const uint CREATE_NO_WINDOW = 0x08000000;
        const uint WAIT_FAILED = 0xffffffff;
        if (accountPassword == null || accountName != userName)
            throw new InvalidOperationException("temporary standard-user state is unavailable");

        var startupInfo = new STARTUPINFO();
        startupInfo.cb = Marshal.SizeOf(startupInfo);
        var commandLine = new StringBuilder(
            "\"" + application + "\" " + arguments);
        PROCESS_INFORMATION processInformation;
        if (!CreateProcessWithLogonW(
                userName, ".", accountPassword, 0,
                application, commandLine, CREATE_NO_WINDOW,
                IntPtr.Zero, currentDirectory, ref startupInfo,
                out processInformation))
            throw new Win32Exception(Marshal.GetLastWin32Error(),
                "failed to launch the standard Windows user process");

        try
        {
            uint waitResult = WaitForSingleObject(
                processInformation.hProcess, 15 * 60 * 1000);
            if (waitResult == WAIT_FAILED)
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    "failed while waiting for the standard Windows user process");
            if (waitResult != 0)
            {
                TerminateProcess(processInformation.hProcess, 1);
                throw new TimeoutException(
                    "standard Windows user process exceeded its test deadline");
            }
            uint exitCode;
            if (!GetExitCodeProcess(processInformation.hProcess, out exitCode))
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    "failed to read the standard Windows user process exit code");
            return checked((int)exitCode);
        }
        finally
        {
            if (processInformation.hThread != IntPtr.Zero)
                CloseHandle(processInformation.hThread);
            if (processInformation.hProcess != IntPtr.Zero)
                CloseHandle(processInformation.hProcess);
        }
    }

    public static void Delete(string userName)
    {
        if (accountName != userName)
            return;
        try
        {
            uint result = NetUserDel(null, userName);
            if (result != 0)
                throw new Win32Exception((int)result,
                    "failed to remove the temporary standard Windows user");
        }
        finally
        {
            accountPassword = null;
            accountName = null;
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

function Quote-PowerShellLiteral([string]$Value) {
    return $Value.Replace("'", "''")
}

$suffix = [System.Diagnostics.Process]::GetCurrentProcess().Id
$userName = "qwcacltest$suffix"
$account = "$env:COMPUTERNAME\$userName"
$usersGroup = (Get-LocalGroup -SID "S-1-5-32-545").Name
$createdUser = $false

try {
    [QwcStandardUserProcess]::Create($userName, $account, $usersGroup)
    $createdUser = $true
    $testUser = Get-LocalUser -Name $userName
    $administrators = @(Get-LocalGroupMember -Group (
        Get-LocalGroup -SID "S-1-5-32-544"))
    if ($administrators.SID.Value -contains $testUser.SID.Value) {
        throw "temporary test account unexpectedly belongs to Administrators"
    }

    New-Item -ItemType Directory -Path $TestRoot | Out-Null
    & icacls.exe $TestRoot /inheritance:r /grant:r `
        "${account}:(OI)(CI)F" `
        '*S-1-5-18:(OI)(CI)F' `
        '*S-1-5-32-544:(OI)(CI)F' |
        Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "failed to isolate the standard-user test directory"
    }

    $runtimeRoot = Join-Path $TestRoot "runtime"
    $wrapper = Join-Path $TestRoot "run-standard-user.ps1"
    $result = Join-Path $TestRoot "result.txt"
    $finished = Join-Path $TestRoot "finished.txt"
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
        throw 'standard-user keystore test unexpectedly has administrator membership'
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
    $processExit = [QwcStandardUserProcess]::Run(
        $userName, $pwsh, $arguments, $TestRoot)
    if (-not (Test-Path -LiteralPath $finished -PathType Leaf)) {
        throw "standard-user runtime produced no completion marker"
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
    if ($processExit -ne 0 -or $exitCode -ne "0") {
        throw "standard-user runtime failed: $output"
    }
    if ($output -notmatch 'fixed_runtime=pass starts=4 identities=stable content_sha256=stable broad_acl=rejected acl_only_repair=pass') {
        throw "standard-user runtime did not report the complete proof"
    }
    Write-Output "standard_user_runtime=pass account=non_administrator starts=4 identities=stable"
} finally {
    if ($createdUser) {
        [QwcStandardUserProcess]::Delete($userName)
    }
}
