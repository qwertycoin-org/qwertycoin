# QMS2 strict network policy

## Native CLI and Qt clients

Strict messenger mode is the default for every new contact. While enabled, all wallet
node traffic used by that wallet instance (RPC reads, block scanning, transaction
submission, redirects, retries, and background refresh) must use a user-provided SOCKS5
proxy with proxy-side DNS resolution. There is no direct fallback.

The wallet does not start, install, configure, or monitor Tor. This candidate accepts
an explicit proxy endpoint and only an explicit Tor v3 onion wallet-node endpoint.
Clearnet endpoints, user-info authorities, malformed ports, non-HTTP schemes, and
redirect targets outside that pinned onion origin are unsupported and fail before a
connection attempt. Proxy failure or any attempt to bypass the configured route leaves
offline drafts/contact management available. Supporting a clearnet node through a Tor
exit would additionally require strict TLS/certificate and redirect-origin enforcement;
that path is deliberately not implemented or claimed here.

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
decryption, and carrier-plan functions, but wallet synchronization, transaction
construction, and broadcast remain disabled unless a separately reviewed transport
supplies an attestable proxy-only channel. Acceptable future approaches include a local companion
with a narrow authenticated API that tunnels through a user-run Tor SOCKS5 endpoint,
or a browser distribution whose entire network stack is externally forced through Tor.
Neither is part of this implementation and no live Tor test is claimed.

## Offline verification

Offline tests prove that a missing/malformed proxy, clearnet/DNS hostname, invalid
scheme, user-info authority, malformed v3 onion, and invalid port are rejected before
the wallet can initialize or replace its active transport. Runtime proxy failure cannot
fall back because only the configured SOCKS client owns the accepted onion route. No
clearnet TLS or redirect path exists in this candidate. Live Tor routing and failure
injection remain manual follow-up only after the operator supplies a proxy and endpoint.
