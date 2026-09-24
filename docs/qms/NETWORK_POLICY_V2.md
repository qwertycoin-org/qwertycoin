# QMS2 strict network policy

## Native CLI and Qt clients

Strict messenger mode is the default for every new contact. While enabled, all wallet
node traffic used by that wallet instance (RPC reads, block scanning, transaction
submission, redirects, retries, and background refresh) must use a user-provided SOCKS5
proxy with proxy-side DNS resolution. There is no direct fallback.

The wallet does not start, install, configure, or monitor Tor. It accepts an explicit
proxy endpoint and an explicit wallet-node endpoint. Clearnet endpoints require strict
TLS validation through the proxy. Onion endpoints remain proxy-only. Redirects to a
different origin, certificate failures, proxy failures, local resolver use, or any
attempt to bypass the configured route fail closed and leave offline drafts/contact
management available.

Native code must centralize this policy below the GUI so daemon selection, automatic
node discovery, wallet refresh, and submit paths cannot bypass it. Logs contain only a
redacted policy error, never contact identifiers, messages, proxy credentials, or
destination query data.

## Browser Web Wallet limitation

A normal web page cannot open SOCKS5 connections, select remote DNS behavior, inspect
the browser's actual egress route, or prove that extensions/service workers/browser
fallbacks did not connect directly. Pretending otherwise would violate fail-closed
behavior.

Therefore the browser QMS2 implementation provides offline contact, draft, encryption,
decryption, and transaction-preparation functions, but messenger synchronization and
broadcast remain disabled unless a separately reviewed transport supplies an
attestable proxy-only channel. Acceptable future approaches include a local companion
with a narrow authenticated API that tunnels through a user-run Tor SOCKS5 endpoint,
or a browser distribution whose entire network stack is externally forced through Tor.
Neither is part of this implementation and no live Tor test is claimed.

## Offline verification

Tests must inject a transport adapter and prove that proxy failure, DNS failure,
redirect, TLS failure, or missing proxy configuration produces no direct connection
attempt. Live Tor routing is a manual follow-up only after the operator supplies a
proxy and endpoint.
