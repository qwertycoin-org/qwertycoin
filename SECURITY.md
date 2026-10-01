# Security policy

## Supported line

Security fixes target the latest published Qwertycoin Core release and the
current `main` branch. Historical releases and the legacy pre-v2 network are
not supported.

## Reporting a vulnerability

Do not open a public issue for a suspected vulnerability. Use GitHub's
**Report a vulnerability** form in the Security tab of this repository so the
maintainers can investigate privately.

Include the affected revision, network, reproduction steps, expected impact,
and whether funds, consensus, private keys, RPC exposure, or service-node
identity are involved. Never include wallet seeds, spend keys, service-node
private keys, or production credentials.

The maintainers will acknowledge the report, reproduce it against a pinned
revision, coordinate a fix and disclosure, and publish release guidance when
users or operators must act.

## Scope priorities

Reports involving consensus divergence, inflation, double spending, wallet
funds, key disclosure, unauthenticated administrative RPC access, or remote
code execution receive the highest priority.

