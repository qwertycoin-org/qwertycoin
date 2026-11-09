#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
mkdir "$scratch/src" "$scratch/translations"
cp "$repo/src/cryptonote_config.h" "$scratch/src/cryptonote_config.h"
printf '%s\n' '<?xml version="1.0" encoding="utf-8"?><TS version="2.1"></TS>' > "$scratch/translations/check.ts"
cd "$scratch"
sh "$repo/utils/translations/update-translations.sh" > /dev/null
python3 - translations/check.ts <<'PY'
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
sources = {
    message.findtext("source")
    for context in root.findall("context")
    if context.findtext("name") == "cryptonote::simple_wallet"
    for message in context.findall("message")
}
required = {"Mainnet", "Testnet", "Stagenet", "Fakechain", "Devnet", "Undefined"}
if not required <= sources:
    raise SystemExit("network names missing from cryptonote::simple_wallet")
PY
mkdir "$scratch/legacy-bin"
for tool in lupdate-qt4 lupdate
do
  printf '%s\n' '#!/bin/sh' 'if [ "$1" = "-help" ]; then echo "Usage: lupdate"; exit 0; fi' 'exit 43' > "$scratch/legacy-bin/$tool"
  chmod +x "$scratch/legacy-bin/$tool"
  if legacy_output=$(PATH="$scratch/legacy-bin:/usr/bin:/bin" sh "$repo/utils/translations/update-translations.sh" 2>&1); then
    exit 1
  fi
  case "$legacy_output" in
    *"lupdate with -tr-function-alias support required"*) ;;
    *) exit 1 ;;
  esac
done
