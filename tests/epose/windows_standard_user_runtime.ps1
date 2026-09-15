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
using System.Text;

public static class QwcRestrictedProcess
{
    [StructLayout(LayoutKind.Sequential)]
    private struct SID_AND_ATTRIBUTES
    {
        public IntPtr Sid;
        public uint Attributes;
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

    [DllImport("kernel32.dll")]
    private static extern IntPtr GetCurrentProcess();

    [DllImport("advapi32.dll", SetLastError = true)]
    private static extern bool OpenProcessToken(
        IntPtr processHandle,
        uint desiredAccess,
        out IntPtr tokenHandle);

    [DllImport("advapi32.dll", SetLastError = true)]
    private static extern bool CreateRestrictedToken(
        IntPtr existingTokenHandle,
        uint flags,
        uint disableSidCount,
        [In] SID_AND_ATTRIBUTES[] sidsToDisable,
        uint deletePrivilegeCount,
        IntPtr privilegesToDelete,
        uint restrictedSidCount,
        IntPtr sidsToRestrict,
        out IntPtr newTokenHandle);

    [DllImport("advapi32.dll", SetLastError = true)]
    private static extern bool IsTokenRestricted(IntPtr tokenHandle);

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CreateProcessAsUserW(
        IntPtr token,
        string applicationName,
        StringBuilder commandLine,
        IntPtr processAttributes,
        IntPtr threadAttributes,
        bool inheritHandles,
        uint creationFlags,
        IntPtr environment,
        string currentDirectory,
        ref STARTUPINFO startupInfo,
        out PROCESS_INFORMATION processInformation);

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CreateProcessWithTokenW(
        IntPtr token,
        uint logonFlags,
        string applicationName,
        StringBuilder commandLine,
        uint creationFlags,
        IntPtr environment,
        string currentDirectory,
        ref STARTUPINFO startupInfo,
        out PROCESS_INFORMATION processInformation);

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool ConvertStringSidToSidW(
        string stringSid,
        out IntPtr sid);

    [DllImport("kernel32.dll")]
    private static extern IntPtr LocalFree(IntPtr memory);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetExitCodeProcess(IntPtr process, out uint exitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr handle);

    public static int Run(string application, string arguments, string currentDirectory)
    {
        const uint TOKEN_ASSIGN_PRIMARY = 0x0001;
        const uint TOKEN_DUPLICATE = 0x0002;
        const uint TOKEN_QUERY = 0x0008;
        const uint TOKEN_ADJUST_DEFAULT = 0x0080;
        const uint TOKEN_ADJUST_SESSIONID = 0x0100;
        const uint DISABLE_MAX_PRIVILEGE = 0x00000001;
        const uint CREATE_NO_WINDOW = 0x08000000;
        const uint WAIT_FAILED = 0xffffffff;
        const uint ERROR_PRIVILEGE_NOT_HELD = 1314;

        IntPtr sourceToken = IntPtr.Zero;
        IntPtr restrictedToken = IntPtr.Zero;
        IntPtr administratorsSid = IntPtr.Zero;
        PROCESS_INFORMATION processInformation = new PROCESS_INFORMATION();
        try
        {
            uint access = TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY |
                TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID;
            if (!OpenProcessToken(GetCurrentProcess(), access, out sourceToken))
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    "failed to open the current process token");
            if (!ConvertStringSidToSidW("S-1-5-32-544", out administratorsSid))
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    "failed to resolve the built-in Administrators SID");

            var disabledSids = new[] {
                new SID_AND_ATTRIBUTES { Sid = administratorsSid, Attributes = 0 }
            };
            if (!CreateRestrictedToken(sourceToken, DISABLE_MAX_PRIVILEGE,
                    1, disabledSids, 0, IntPtr.Zero, 0, IntPtr.Zero,
                    out restrictedToken))
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    "failed to create a restricted Windows token");
            if (!IsTokenRestricted(restrictedToken))
                throw new InvalidOperationException(
                    "Windows did not mark the child token as restricted");

            var startupInfo = new STARTUPINFO();
            startupInfo.cb = Marshal.SizeOf(startupInfo);
            var commandLine = new StringBuilder(
                "\"" + application + "\" " + arguments);
            bool created = CreateProcessAsUserW(
                restrictedToken, application, commandLine,
                IntPtr.Zero, IntPtr.Zero, false, CREATE_NO_WINDOW,
                IntPtr.Zero, currentDirectory, ref startupInfo,
                out processInformation);
            int createError = created ? 0 : Marshal.GetLastWin32Error();
            if (!created && createError == ERROR_PRIVILEGE_NOT_HELD)
            {
                commandLine = new StringBuilder(
                    "\"" + application + "\" " + arguments);
                created = CreateProcessWithTokenW(
                    restrictedToken, 0, application, commandLine,
                    CREATE_NO_WINDOW, IntPtr.Zero, currentDirectory,
                    ref startupInfo, out processInformation);
                createError = created ? 0 : Marshal.GetLastWin32Error();
            }
            if (!created)
                throw new Win32Exception(createError,
                    "failed to launch the restricted Windows process");

            uint waitResult = WaitForSingleObject(
                processInformation.hProcess, 15 * 60 * 1000);
            if (waitResult == WAIT_FAILED)
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    "failed while waiting for the restricted Windows process");
            if (waitResult != 0)
                throw new TimeoutException(
                    "restricted Windows process exceeded its test deadline");
            uint exitCode;
            if (!GetExitCodeProcess(processInformation.hProcess, out exitCode))
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    "failed to read the restricted Windows process exit code");
            return checked((int)exitCode);
        }
        finally
        {
            if (processInformation.hThread != IntPtr.Zero)
                CloseHandle(processInformation.hThread);
            if (processInformation.hProcess != IntPtr.Zero)
                CloseHandle(processInformation.hProcess);
            if (administratorsSid != IntPtr.Zero)
                LocalFree(administratorsSid);
            if (restrictedToken != IntPtr.Zero)
                CloseHandle(restrictedToken);
            if (sourceToken != IntPtr.Zero)
                CloseHandle(sourceToken);
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
    throw "isolated restricted-token test root already exists"
}

function Quote-PowerShellLiteral([string]$Value) {
    return $Value.Replace("'", "''")
}

New-Item -ItemType Directory -Path $TestRoot | Out-Null
$runtimeRoot = Join-Path $TestRoot "runtime"
$wrapper = Join-Path $TestRoot "run-restricted-user.ps1"
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
        throw 'restricted keystore test unexpectedly has administrator membership'
    }
    `$env:PATH = '$runtime;' + `$env:PATH
    & '$runtimeScript' -Mode VerifyFixed -DaemonPath '$daemon' -TestRoot '$test' |
        Set-Content -LiteralPath '$resultPath' -Encoding UTF8
    '0' | Set-Content -LiteralPath '$finishedPath' -Encoding ASCII
} catch {
    `$safe = `$_.Exception.Message
    if (`$safe -match '(?i)operator_secret_key|service_secret_key') {
        `$safe = 'restricted-token test failed; secret-bearing diagnostic suppressed'
    }
    `$safe = `$safe -replace '(?i)\b[0-9a-f]{64}\b', '<redacted-hex64>'
    `$safe | Set-Content -LiteralPath '$resultPath' -Encoding UTF8
    '1' | Set-Content -LiteralPath '$finishedPath' -Encoding ASCII
}
"@
Set-Content -LiteralPath $wrapper -Value $wrapperBody -Encoding UTF8

$pwsh = (Get-Command pwsh.exe -ErrorAction Stop).Source
$arguments = "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$wrapper`""
$processExit = [QwcRestrictedProcess]::Run($pwsh, $arguments, $TestRoot)
if (-not (Test-Path -LiteralPath $finished -PathType Leaf)) {
    throw "restricted-token runtime produced no completion marker"
}
$exitCode = (Get-Content -LiteralPath $finished -Raw).Trim()
$output = if (Test-Path -LiteralPath $result -PathType Leaf) {
    (Get-Content -LiteralPath $result -Raw).Trim()
} else {
    "no safe restricted-token diagnostic was available"
}
if ($output -match '(?i)operator_secret_key|service_secret_key') {
    throw "secret key material appeared in the restricted-token result"
}
if ($processExit -ne 0 -or $exitCode -ne "0") {
    throw "restricted-token runtime failed: $output"
}
if ($output -notmatch 'fixed_runtime=pass starts=4 identities=stable content_sha256=stable broad_acl=rejected acl_only_repair=pass') {
    throw "restricted-token runtime did not report the complete proof"
}
Write-Output "restricted_windows_token=pass administrator_membership=disabled starts=4 identities=stable"
