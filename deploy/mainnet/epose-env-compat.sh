#!/bin/sh
# Normalize public EPoSe environment variables at the deployment boundary.
# Values are never printed. The legacy variables can be removed after the
# documented compatibility window.

qwc_epose_legacy_env_used=0

qwc_resolve_epose_env()
{
  qwc_canonical_name=$1
  qwc_legacy_name=$2
  qwc_default_value=$3

  eval "qwc_canonical_is_set=\${${qwc_canonical_name}+x}"
  eval "qwc_legacy_is_set=\${${qwc_legacy_name}+x}"
  eval "qwc_canonical_value=\${${qwc_canonical_name}-}"
  eval "qwc_legacy_value=\${${qwc_legacy_name}-}"

  if [ "$qwc_canonical_is_set" = x ] && [ "$qwc_legacy_is_set" = x ] \
      && [ "$qwc_canonical_value" != "$qwc_legacy_value" ]; then
    echo "conflicting EPoSe environment variables: $qwc_canonical_name and deprecated $qwc_legacy_name" >&2
    return 64
  fi

  if [ "$qwc_legacy_is_set" = x ]; then
    qwc_epose_legacy_env_used=1
  fi

  if [ "$qwc_canonical_is_set" = x ]; then
    qwc_resolved_value=$qwc_canonical_value
  elif [ "$qwc_legacy_is_set" = x ]; then
    qwc_resolved_value=$qwc_legacy_value
  elif [ "$qwc_default_value" != __QWC_REQUIRED__ ]; then
    qwc_resolved_value=$qwc_default_value
  else
    echo "$qwc_canonical_name is required" >&2
    return 64
  fi

  if [ -z "$qwc_resolved_value" ]; then
    echo "$qwc_canonical_name must not be empty" >&2
    return 64
  fi
  export "$qwc_canonical_name=$qwc_resolved_value"
}

qwc_resolve_epose_env QWC_EPOSE_KEYSTORE_PATH QWC_EPOSE_V2_KEYSTORE_PATH \
  /service-node/epose-v2-keystore
qwc_resolve_epose_env QWC_EPOSE_HOST QWC_EPOSE_V2_ENDPOINT_HOST __QWC_REQUIRED__
qwc_resolve_epose_env QWC_EPOSE_PORT QWC_EPOSE_V2_ENDPOINT_PORT 8198
qwc_resolve_epose_env QWC_EPOSE_DISCOVERY_ENDPOINTS \
  QWC_EPOSE_V2_DISCOVERY_ENDPOINTS __QWC_REQUIRED__

case "$QWC_EPOSE_PORT" in
  *[!0-9]*|'')
    echo "QWC_EPOSE_PORT must be an integer between 1 and 65535" >&2
    return 64 2>/dev/null || exit 64
    ;;
esac
if [ "$QWC_EPOSE_PORT" -lt 1 ] || [ "$QWC_EPOSE_PORT" -gt 65535 ]; then
  echo "QWC_EPOSE_PORT must be an integer between 1 and 65535" >&2
  return 64 2>/dev/null || exit 64
fi

if [ "$qwc_epose_legacy_env_used" -eq 1 ]; then
  echo "Deprecated EPoSe environment variable names were used; migrate to QWC_EPOSE_KEYSTORE_PATH, QWC_EPOSE_HOST, QWC_EPOSE_PORT and QWC_EPOSE_DISCOVERY_ENDPOINTS." >&2
fi

unset qwc_canonical_name qwc_legacy_name qwc_default_value
unset qwc_canonical_is_set qwc_legacy_is_set
unset qwc_canonical_value qwc_legacy_value qwc_resolved_value
unset qwc_epose_legacy_env_used
unset -f qwc_resolve_epose_env 2>/dev/null || true
