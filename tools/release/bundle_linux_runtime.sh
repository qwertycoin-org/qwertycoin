#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <artifact-directory>" >&2
  exit 64
fi

artifact_dir=$(realpath "$1")
library_dir="$artifact_dir/lib"
[[ "$(uname -s)" == Linux ]] || { echo "Linux bundling must run on Linux" >&2; exit 1; }
for tool in file ldd patchelf realpath; do
  command -v "$tool" >/dev/null 2>&1 || { echo "missing Linux bundling tool: $tool" >&2; exit 1; }
done
mkdir -p "$library_dir"

is_elf() { [[ -f "$1" && "$(file -Lb "$1")" == ELF* ]]; }
is_system_runtime() {
  case "$1" in
    ld-linux-*.so.*|libanl.so.*|libc.so.*|libdl.so.*|libm.so.*|libnss_*.so.*|\
      libpthread.so.*|libresolv.so.*|librt.so.*|libutil.so.*) return 0 ;;
  esac
  return 1
}
list_dependencies() {
  local binary=$1 search_path=${2:-}
  if [[ -n "$search_path" ]]; then
    LD_LIBRARY_PATH="$search_path" ldd "$binary" 2>/dev/null
  else
    ldd "$binary" 2>/dev/null
  fi | sed -n -E \
    -e 's/^[[:space:]]*[^[:space:]]+[[:space:]]+=>[[:space:]]+(\/[^[:space:]]+).*/\1/p' \
    -e 's/^[[:space:]]*(\/[^[:space:]]+).*/\1/p' || true
}

while true; do
  copied=0
  while IFS= read -r -d '' binary; do
    is_elf "$binary" || continue
    while IFS= read -r dependency; do
      [[ -f "$dependency" ]] || continue
      name=$(basename "$dependency")
      is_system_runtime "$name" && continue
      destination="$library_dir/$name"
      if [[ -e "$destination" ]]; then
        cmp -s "$dependency" "$destination" || {
          echo "dependency basename collision: $dependency and $destination" >&2
          exit 1
        }
      else
        cp -L -- "$dependency" "$destination"
        copied=$((copied + 1))
      fi
    done < <(list_dependencies "$binary" "${QWC_RUNTIME_LIBRARY_PATH:-}")
  done < <(find "$artifact_dir" -type f -print0)
  (( copied == 0 )) && break
done

while IFS= read -r -d '' binary; do
  is_elf "$binary" || continue
  relative=$(realpath --relative-to="$(dirname "$binary")" "$library_dir")
  [[ "$relative" == . ]] && rpath='$ORIGIN' || rpath="\$ORIGIN/$relative"
  patchelf --set-rpath "$rpath" "$binary"
done < <(find "$artifact_dir" -type f -print0)

failed=0
while IFS= read -r -d '' binary; do
  is_elf "$binary" || continue
  if ldd "$binary" 2>&1 | grep -q 'not found'; then
    echo "unresolved dependency in ${binary#"$artifact_dir/"}" >&2
    ldd "$binary" >&2 || true
    failed=1
  fi
  while IFS= read -r dependency; do
    [[ -f "$dependency" ]] || continue
    name=$(basename "$dependency")
    is_system_runtime "$name" && continue
    case "$dependency" in "$artifact_dir"/*) ;; *)
      echo "non-system dependency resolves outside package: ${binary#"$artifact_dir/"} -> $dependency" >&2
      failed=1
    esac
  done < <(list_dependencies "$binary")
done < <(find "$artifact_dir" -type f -print0)
(( failed == 0 )) || exit 1
echo "Linux dependency closure verified inside $artifact_dir"
