#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <artifact-directory>" >&2
  exit 64
fi
artifact_dir=$1
[[ -d "$artifact_dir" ]] || { echo "artifact directory is missing" >&2; exit 1; }
mingw_bin=${MINGW_PREFIX:?MINGW_PREFIX is required}/bin

is_system_dll() {
  case "${1,,}" in
    api-ms-win-*|ext-ms-win-*|advapi32.dll|bcrypt.dll|cfgmgr32.dll|comctl32.dll|comdlg32.dll|crypt32.dll|cryptui.dll|dbghelp.dll|dnsapi.dll|dwmapi.dll|dwrite.dll|dxgi.dll|gdi32.dll|imm32.dll|iphlpapi.dll|kernel32.dll|kernelbase.dll|mpr.dll|msvcp_win.dll|msvcrt.dll|mswsock.dll|netapi32.dll|normaliz.dll|ntdll.dll|ole32.dll|oleacc.dll|oleaut32.dll|powrprof.dll|propsys.dll|psapi.dll|rpcrt4.dll|secur32.dll|setupapi.dll|shell32.dll|shlwapi.dll|user32.dll|userenv.dll|usp10.dll|uxtheme.dll|version.dll|winhttp.dll|winmm.dll|winspool.drv|wintrust.dll|ws2_32.dll|wtsapi32.dll|wldap32.dll) return 0 ;;
  esac
  return 1
}

declare -A provided=()
while true; do
  provided=()
  while IFS= read -r -d '' binary; do provided["$(basename "${binary,,}")"]=1; done \
    < <(find "$artifact_dir" -type f \( -iname '*.exe' -o -iname '*.dll' \) -print0)
  copied=0
  while IFS= read -r -d '' binary; do
    while IFS= read -r dependency; do
      [[ -n "$dependency" ]] || continue
      normalized=${dependency,,}
      [[ -n "${provided[$normalized]:-}" ]] && continue
      is_system_dll "$normalized" && continue
      source="$mingw_bin/$dependency"
      [[ -f "$source" ]] || source=$(find "$mingw_bin" -maxdepth 1 -type f -iname "$dependency" | head -n 1 || true)
      [[ -f "$source" ]] || { echo "cannot resolve Windows import: $dependency" >&2; exit 1; }
      cp -L "$source" "$artifact_dir/$dependency"
      provided[$normalized]=1
      copied=$((copied + 1))
    done < <(objdump -p "$binary" | sed -n 's/^[[:space:]]*DLL Name:[[:space:]]*//p')
  done < <(find "$artifact_dir" -type f \( -iname '*.exe' -o -iname '*.dll' \) -print0)
  (( copied == 0 )) && break
done
"$(dirname "$0")/verify_windows_runtime.sh" "$artifact_dir"
