#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <build-directory> <artifact-name> <output-directory> <platform>" >&2
  exit 64
fi
build_dir=$1
artifact_name=$2
output_dir=$3
platform=$4
script_dir=$(cd "$(dirname "$0")" && pwd -P)
[[ "$artifact_name" =~ ^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$ ]] || { echo "unsafe artifact name" >&2; exit 64; }
[[ "$platform" =~ ^(linux|macos|windows)$ ]] || { echo "unsupported platform" >&2; exit 64; }
[[ -d "$build_dir" ]] || { echo "build directory is missing" >&2; exit 1; }
: "${QWC_BUILD_INFO:?QWC_BUILD_INFO is required}"
: "${QWC_RELEASE_EVIDENCE:?QWC_RELEASE_EVIDENCE is required}"
: "${QWC_EPOSE_GATE:?QWC_EPOSE_GATE is required}"
: "${QWC_RELEASE_VERSION:?QWC_RELEASE_VERSION is required}"

for target in "$output_dir/$artifact_name" "$output_dir/$artifact_name.sha256" "$output_dir/$artifact_name.tar.gz" "$output_dir/$artifact_name.zip"; do
  [[ ! -e "$target" ]] || { echo "refusing to overwrite existing output: $target" >&2; exit 1; }
done
mkdir -p "$output_dir/$artifact_name/evidence"
artifact_dir="$output_dir/$artifact_name"

copy_binary() {
  local expected_name=$1 found="" candidate count=0
  while IFS= read -r candidate; do
    found=$candidate
    count=$((count + 1))
  done < <(find "$build_dir" -type f -name "$expected_name" -print | LC_ALL=C sort)
  (( count == 1 )) || {
    echo "expected exactly one built program named $expected_name, found $count" >&2
    exit 1
  }
  cp "$found" "$artifact_dir/$expected_name"
}
if [[ "$platform" == windows ]]; then
  copy_binary qwertycoind.exe
  copy_binary qwertycoin-wallet-cli.exe
  copy_binary qwertycoin-wallet-rpc.exe
else
  copy_binary qwertycoind
  copy_binary qwertycoin-wallet-cli
  copy_binary qwertycoin-wallet-rpc
fi

cp LICENSE "$artifact_dir/LICENSE"
cp "$script_dir/README-RELEASE.md" "$artifact_dir/README-RELEASE.md"
cp "$script_dir/THIRD-PARTY-NOTICES.md" "$artifact_dir/THIRD-PARTY-NOTICES.md"
cp "$QWC_BUILD_INFO" "$artifact_dir/BUILD-INFO.json"
cp "$QWC_RELEASE_EVIDENCE" "$artifact_dir/evidence/RELEASE-EVIDENCE.json"
cp "$QWC_EPOSE_GATE" "$artifact_dir/evidence/EPOSE-RELEASE-GATE.json"
cp docs/epose/PARAMETER_MANIFEST_V2.json "$artifact_dir/evidence/PARAMETER-MANIFEST-V2.json"
if [[ -n "${QWC_RELEASE_EVIDENCE_DIR:-}" ]]; then
  [[ -d "$QWC_RELEASE_EVIDENCE_DIR" ]] || { echo "release evidence directory is missing" >&2; exit 1; }
  cp -R "$QWC_RELEASE_EVIDENCE_DIR/." "$artifact_dir/evidence/"
fi

case "$platform" in
  linux)
    "$script_dir/bundle_linux_runtime.sh" "$artifact_dir"
    "$script_dir/verify_linux_abi.sh" "$artifact_dir" "${QWC_LINUX_GLIBC_CEILING:-2.35}" "${QWC_LINUX_GLIBCXX_CEILING:-3.4.30}"
    ;;
  macos)
    "$script_dir/bundle_macos_runtime.sh" "$artifact_dir" "${QWC_MACOS_MIN_VERSION:-15.0}"
    ;;
  windows)
    "$script_dir/bundle_windows_runtime.sh" "$artifact_dir"
    ;;
esac

python3 "$script_dir/smoke_core.py" \
  --package-root "$artifact_dir" \
  --expected-version "$QWC_RELEASE_VERSION" \
  --output "$artifact_dir/evidence/RUNTIME-SMOKE.json"
python3 - "$artifact_dir" <<'PY'
import json
from pathlib import Path
import sys

root = Path(sys.argv[1])
evidence_path = root / "evidence/RELEASE-EVIDENCE.json"
smoke_path = root / "evidence/RUNTIME-SMOKE.json"
info_path = root / "BUILD-INFO.json"
evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
smoke = json.loads(smoke_path.read_text(encoding="utf-8"))
for key, value in smoke.items():
    if key != "schema_version":
        evidence[key] = value
evidence_path.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
info = json.loads(info_path.read_text(encoding="utf-8"))
info["verification"] = evidence
info_path.write_text(json.dumps(info, indent=2, sort_keys=True) + "\n", encoding="utf-8")
PY

for required in BUILD-INFO.json README-RELEASE.md THIRD-PARTY-NOTICES.md LICENSE evidence/RELEASE-EVIDENCE.json evidence/EPOSE-RELEASE-GATE.json evidence/PARAMETER-MANIFEST-V2.json evidence/RUNTIME-SMOKE.json; do
  [[ -f "$artifact_dir/$required" && ! -L "$artifact_dir/$required" ]] || { echo "required payload missing: $required" >&2; exit 1; }
done

(
  cd "$output_dir"
  if command -v sha256sum >/dev/null 2>&1; then
    find "$artifact_name" -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum >"$artifact_name.sha256"
  else
    find "$artifact_name" -type f -print0 | LC_ALL=C sort -z | xargs -0 shasum -a 256 >"$artifact_name.sha256"
  fi
  if [[ "$platform" == windows ]]; then
    zip -Xqr "$artifact_name.zip" "$artifact_name" "$artifact_name.sha256"
    sha256sum "$artifact_name.zip" >"$artifact_name.zip.sha256"
  else
    tar -czf "$artifact_name.tar.gz" "$artifact_name" "$artifact_name.sha256"
    if command -v sha256sum >/dev/null 2>&1; then
      sha256sum "$artifact_name.tar.gz" >"$artifact_name.tar.gz.sha256"
    else
      shasum -a 256 "$artifact_name.tar.gz" >"$artifact_name.tar.gz.sha256"
    fi
  fi
)
