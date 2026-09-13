#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then echo "usage: $0 <artifact-directory> <maximum-macos-version>" >&2; exit 64; fi
artifact_dir=$1
maximum=$2
version_greater() {
  [[ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -n 1)" == "$1" && "$1" != "$2" ]]
}
count=0
while IFS= read -r -d '' binary; do
  [[ "$(file -Lb "$binary")" == Mach-O* ]] || continue
  count=$((count + 1))
  lipo -archs "$binary" | tr ' ' '\n' | grep -qx arm64 || { echo "non-arm64 Mach-O: ${binary#"$artifact_dir/"}" >&2; exit 1; }
  codesign --verify --strict "$binary"
  if otool -L "$binary" | tail -n +2 | awk '{print $1}' | grep -E '^(/opt/homebrew|/usr/local|/Users/runner|/opt/hostedtoolcache)'; then
    echo "runner-local Mach-O dependency: ${binary#"$artifact_dir/"}" >&2; exit 1
  fi
  if otool -l "$binary" | awk '$1=="cmd"&&$2=="LC_RPATH"{want=1;next} want&&$1=="path"{print $2;want=0}' | grep -E '^/'; then
    echo "absolute Mach-O rpath: ${binary#"$artifact_dir/"}" >&2; exit 1
  fi
  found=0
  while IFS= read -r minos; do
    [[ -n "$minos" ]] || continue
    found=1
    version_greater "$minos" "$maximum" && { echo "Mach-O requires macOS $minos above $maximum" >&2; exit 1; }
  done < <(otool -l "$binary" | awk '
    $1=="cmd"&&$2=="LC_BUILD_VERSION"{build=1;next} build&&$1=="minos"{print $2;build=0}
    $1=="cmd"&&$2=="LC_VERSION_MIN_MACOSX"{legacy=1;next} legacy&&$1=="version"{print $2;legacy=0}')
  (( found == 1 )) || { echo "Mach-O has no deployment target" >&2; exit 1; }
done < <(find "$artifact_dir" -type f -print0)
(( count > 0 )) || { echo "no Mach-O files found" >&2; exit 1; }
echo "macOS runtime verified: $count signed arm64 Mach-O files, minimum target at most $maximum"
