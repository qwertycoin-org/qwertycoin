#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 <artifact-directory> <maximum-glibc> <maximum-glibcxx>" >&2
  exit 64
fi
artifact_dir=$(realpath "$1")
maximum_glibc=$2
maximum_glibcxx=$3
[[ "$maximum_glibc" =~ ^[0-9]+([.][0-9]+)+$ ]] || exit 64
[[ "$maximum_glibcxx" =~ ^[0-9]+([.][0-9]+)+$ ]] || exit 64

version_greater() {
  local candidate=$1 maximum=$2 highest
  highest=$(printf '%s\n%s\n' "$candidate" "$maximum" | LC_ALL=C sort -Vu | tail -n 1)
  [[ "$candidate" != "$maximum" && "$highest" == "$candidate" ]]
}

elf_count=0
highest_glibc=0
highest_glibcxx=0
violations=""
while IFS= read -r -d '' binary; do
  description=$(file -Lb "$binary")
  [[ "$description" == ELF* ]] || continue
  [[ "$description" == *x86-64* ]] || {
    echo "unexpected ELF architecture: ${binary#"$artifact_dir/"}: $description" >&2
    exit 1
  }
  elf_count=$((elf_count + 1))
  while IFS= read -r symbol; do
    [[ -n "$symbol" ]] || continue
    family=${symbol%%_*}
    version=${symbol#*_}
    if [[ "$family" == GLIBC ]]; then
      version_greater "$version" "$highest_glibc" && highest_glibc=$version
      version_greater "$version" "$maximum_glibc" && violations+="${binary#"$artifact_dir/"}: $symbol"$'\n'
    else
      version_greater "$version" "$highest_glibcxx" && highest_glibcxx=$version
      version_greater "$version" "$maximum_glibcxx" && violations+="${binary#"$artifact_dir/"}: $symbol"$'\n'
    fi
  done < <(readelf --version-info "$binary" 2>/dev/null | grep -Eo 'GLIBC(XX)?_[0-9]+([.][0-9]+)+' | LC_ALL=C sort -u || true)
done < <(find "$artifact_dir" -type f -print0)

(( elf_count > 0 )) || { echo "no ELF files found" >&2; exit 1; }
if [[ -n "$violations" ]]; then
  echo "Linux ABI ceiling exceeded:" >&2
  printf '%s' "$violations" | LC_ALL=C sort -u >&2
  exit 1
fi
printf 'Linux ABI verified: %d ELF files, maximum GLIBC_%s and GLIBCXX_%s\n' "$elf_count" "$highest_glibc" "$highest_glibcxx"
