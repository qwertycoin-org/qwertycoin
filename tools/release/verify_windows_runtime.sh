#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then echo "usage: $0 <artifact-directory>" >&2; exit 64; fi
artifact_dir=$1
declare -A provided=()
pe_count=0
while IFS= read -r -d '' binary; do
  provided["$(basename "${binary,,}")"]=1
  objdump -f "$binary" | grep -q 'architecture: i386:x86-64' || {
    echo "unexpected PE architecture: ${binary#"$artifact_dir/"}" >&2; exit 1;
  }
  pe_count=$((pe_count + 1))
done < <(find "$artifact_dir" -type f \( -iname '*.exe' -o -iname '*.dll' \) -print0)
(( pe_count > 0 )) || { echo "no PE files found" >&2; exit 1; }

is_system_dll() {
  case "${1,,}" in
    api-ms-win-*|ext-ms-win-*|advapi32.dll|bcrypt.dll|cfgmgr32.dll|comctl32.dll|comdlg32.dll|crypt32.dll|cryptui.dll|dbghelp.dll|dnsapi.dll|dwmapi.dll|dwrite.dll|dxgi.dll|gdi32.dll|imm32.dll|iphlpapi.dll|kernel32.dll|kernelbase.dll|mpr.dll|msvcp_win.dll|msvcrt.dll|mswsock.dll|netapi32.dll|normaliz.dll|ntdll.dll|ole32.dll|oleacc.dll|oleaut32.dll|powrprof.dll|propsys.dll|psapi.dll|rpcrt4.dll|secur32.dll|setupapi.dll|shell32.dll|shlwapi.dll|user32.dll|userenv.dll|usp10.dll|uxtheme.dll|version.dll|winhttp.dll|winmm.dll|winspool.drv|wintrust.dll|ws2_32.dll|wtsapi32.dll|wldap32.dll) return 0 ;;
  esac
  return 1
}

failed=0
while IFS= read -r -d '' binary; do
  while IFS= read -r dependency; do
    [[ -n "$dependency" ]] || continue
    normalized=${dependency,,}
    if [[ -z "${provided[$normalized]:-}" ]] && ! is_system_dll "$normalized"; then
      echo "unresolved Windows import: ${binary#"$artifact_dir/"} -> $dependency" >&2
      failed=1
    fi
  done < <(objdump -p "$binary" | sed -n 's/^[[:space:]]*DLL Name:[[:space:]]*//p')
done < <(find "$artifact_dir" -type f \( -iname '*.exe' -o -iname '*.dll' \) -print0)
(( failed == 0 )) || exit 1
echo "Windows runtime verified: $pe_count x86-64 PE files, all imports resolved"
