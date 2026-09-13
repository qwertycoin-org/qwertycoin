#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <artifact-directory> <maximum-macos-version>" >&2
  exit 64
fi
artifact_dir=$(cd "$1" && pwd -P)
maximum_macos=$2
library_dir="$artifact_dir/lib"
[[ "$(uname -s)" == Darwin ]] || { echo "macOS bundling must run on macOS" >&2; exit 1; }
mkdir -p "$library_dir"

is_macho() { [[ -f "$1" && "$(file -Lb "$1")" == Mach-O* ]]; }
is_system_dependency() { [[ "$1" == /System/Library/* || "$1" == /usr/lib/* ]]; }

resolve_from_configured_paths() {
  local suffix=$1 search_dir candidate
  while IFS= read -r search_dir; do
    [[ -n "$search_dir" ]] || continue
    candidate="$search_dir/$suffix"
    [[ -f "$candidate" ]] && { printf '%s\n' "$candidate"; return 0; }
  done < <(printf '%s' "${QWC_MACOS_LIBRARY_PATH:-}" | tr ':' '\n')
  return 1
}

resolve_dependency() {
  local binary=$1 dependency=$2 candidate suffix rpath
  if [[ "$dependency" == /* ]]; then [[ -f "$dependency" ]] && printf '%s\n' "$dependency"; return; fi
  suffix=${dependency#@rpath/}
  if [[ "$dependency" == @loader_path/* ]]; then
    candidate="$(dirname "$binary")/${dependency#@loader_path/}"
    [[ -f "$candidate" ]] && { printf '%s\n' "$candidate"; return; }
    resolve_from_configured_paths "${dependency#@loader_path/}" && return
  elif [[ "$dependency" == @executable_path/* ]]; then
    candidate="$artifact_dir/${dependency#@executable_path/}"
    [[ -f "$candidate" ]] && { printf '%s\n' "$candidate"; return; }
  elif [[ "$dependency" == @rpath/* ]]; then
    while IFS= read -r rpath; do
      rpath=${rpath//@loader_path/$(dirname "$binary")}
      rpath=${rpath//@executable_path/$artifact_dir}
      candidate="$rpath/$suffix"
      [[ -f "$candidate" ]] && { printf '%s\n' "$candidate"; return; }
    done < <(otool -l "$binary" | awk '$1=="cmd" && $2=="LC_RPATH"{want=1;next} want && $1=="path"{print $2;want=0}')
    resolve_from_configured_paths "$suffix" && return
  fi
}

while true; do
  copied=0
  while IFS= read -r -d '' binary; do
    is_macho "$binary" || continue
    while IFS= read -r dependency; do
      [[ -n "$dependency" ]] || continue
      is_system_dependency "$dependency" && continue
      case "$dependency" in @executable_path/lib/*|@loader_path/*)
        [[ -f "$(dirname "$binary")/${dependency#@loader_path/}" || -f "$artifact_dir/${dependency#@executable_path/}" ]] && continue
      esac
      source=$(resolve_dependency "$binary" "$dependency" || true)
      [[ -n "$source" && -f "$source" ]] || { echo "cannot resolve Mach-O dependency: ${binary#"$artifact_dir/"} -> $dependency" >&2; exit 1; }
      name=$(basename "$source")
      destination="$library_dir/$name"
      if [[ -f "$destination" ]]; then
        cmp -s "$source" "$destination" || { echo "Mach-O dependency basename collision: $name" >&2; exit 1; }
      else
        cp -L "$source" "$destination"
        copied=$((copied + 1))
      fi
    done < <(otool -L "$binary" | tail -n +2 | awk '{print $1}')
  done < <(find "$artifact_dir" -type f -print0)
  (( copied == 0 )) && break
done

while IFS= read -r -d '' binary; do
  is_macho "$binary" || continue
  while IFS= read -r dependency; do
    [[ -n "$dependency" ]] || continue
    is_system_dependency "$dependency" && continue
    name=$(basename "$dependency")
    [[ -f "$library_dir/$name" ]] || continue
    install_name_tool -change "$dependency" "@rpath/$name" "$binary"
  done < <(otool -L "$binary" | tail -n +2 | awk '{print $1}')
  if [[ "$binary" == "$library_dir"/* ]]; then
    install_name_tool -id "@rpath/$(basename "$binary")" "$binary"
  fi
  while IFS= read -r rpath; do
    case "$rpath" in /opt/homebrew/*|/usr/local/*|/Users/runner/*|/opt/hostedtoolcache/*)
      install_name_tool -delete_rpath "$rpath" "$binary" ;;
    esac
  done < <(otool -l "$binary" | awk '$1=="cmd" && $2=="LC_RPATH"{want=1;next} want && $1=="path"{print $2;want=0}')
done < <(find "$artifact_dir" -type f -print0)

for executable in "$artifact_dir/qwertycoind" "$artifact_dir/qwertycoin-wallet-cli" "$artifact_dir/qwertycoin-wallet-rpc"; do
  install_name_tool -add_rpath '@executable_path/lib' "$executable" 2>/dev/null || true
done

while IFS= read -r -d '' binary; do
  is_macho "$binary" || continue
  codesign --force --sign - "$binary"
done < <(find "$library_dir" -type f -print0)
for executable in "$artifact_dir/qwertycoind" "$artifact_dir/qwertycoin-wallet-cli" "$artifact_dir/qwertycoin-wallet-rpc"; do
  codesign --force --sign - "$executable"
done

"$(dirname "$0")/verify_macos_runtime.sh" "$artifact_dir" "$maximum_macos"
