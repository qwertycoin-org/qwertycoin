#!/bin/sh

lupdate=$(command -v lupdate 2> /dev/null)
if test -z "$lupdate" || ! "$lupdate" -help 2>&1 | grep -q -- '-tr-function-alias'
then
  echo "lupdate with -tr-function-alias support required" >&2
  exit 1
fi

echo "using $lupdate"
"$lupdate" -tr-function-alias QT_TRANSLATE_NOOP+=QWC_TRANSLATABLE_NETWORK_NAME `find src -name \*.cpp` src/cryptonote_config.h -ts translations/*.ts -no-obsolete

