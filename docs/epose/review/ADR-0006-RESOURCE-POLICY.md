# ADR-0006: EPoSE v2 Endpoint and Resource Policy

- **Status:** Candidate primitives implemented; handler integration required
- **Date:** 2026-09-06
- **Scope:** CO-09, non-activating

## Decision

Endpoint descriptors are signed by the service identity and bind network,
genesis, parameter set, typed transport, canonical host, port, service
kind/version, sequence, and expiry. Consensus stores/verifies the descriptor
commitment; DNS and live connection results remain local policy.

Public-internet probes accept canonical IPv4, IPv6, or lowercase fully
qualified DNS names only. Literal loopback, private, link-local, multicast,
unspecified, carrier-grade NAT, documentation, benchmark, IPv4-mapped private,
and cloud-metadata addresses are rejected. Every DNS answer must be bounded and
all resolved addresses are checked again at connection time. Redirects and
alternate URL schemes are outside the descriptor and are not allowed.

Probe admission is bounded by request/response bytes, timeout, global
concurrency, tracked peers, and per-peer concurrency. Admission-work contexts
are checked against an already allowed canonical context before any cache/VM
allocation. RPC pages have explicit page and scan ceilings with checked
arithmetic.

## Consensus boundary

Probe timeout, DNS answers, connection success, and local overload never affect
validation of an otherwise complete canonical block. They affect only whether
a node originates/relays evidence. On-chain receipt validation remains
deterministic and performs no network I/O.

## Remaining integration

The primitives are not yet connected to P2P, RPC, DNS resolution, or RandomX
VM construction. Integration must prohibit redirects, enforce byte limits while
streaming, revalidate the chosen socket address, isolate signing credentials,
and shed local relay/probe work without changing block validity. Fuzzing and
sustained malicious-load measurements remain activation gates.
