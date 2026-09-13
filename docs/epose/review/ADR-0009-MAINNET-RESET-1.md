# ADR-0009: Coordinated Reset-1 mainnet identity

- **Status:** Accepted for coordinated deployment
- **Date:** 2026-09-13
- **Scope:** Mainnet genesis and P2P identity only
- **Supersedes:** The chain identity section of ADR-0007

## Context

The operator has authorized a coordinated restart of the four public seed
nodes from a genuinely new genesis. Reusing an empty database with the old
genesis would create the same chain and could accept old blocks again, so a
database reset alone is insufficient.

The restart does not authorize changes to emission, address formats, hardfork
schedule, EPoSE timing, qualification, rewards or resource limits. Historical
chain and release evidence must remain attributable to the identity on which it
was produced.

## Decision

Reset-1 uses these values:

- network ID label `QWC2MAIN2026R01` with terminal revision byte `0x02`
  (`515743324d41494e3230323652303102`);
- the unchanged reviewed HF17 genesis transaction template;
- configured genesis start nonce `20000`;
- canonical genesis hash
  `4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39`;
- EPoSE parameter commitment
  `2c26755094535871dd3ede7bd1b50aba82a9fb6831f0a17f32968eb0385145c6`.

The production genesis constructor and focused C++ test independently derive
the canonical hash from the transaction template and nonce. The parameter
commitment changes because its canonical projection intentionally includes the
network ID and genesis hash; all other consensus parameter values are
unchanged.

## Wallet and service identity consequences

Mainnet address prefixes and wallet key derivation are unchanged. Existing
wallet seeds, spend/view keys and primary reward addresses therefore remain
valid and produce the same addresses. Outputs, balances and transaction history
from the superseded chain are not migrated. Wallet key files are preserved,
while caches are rebuilt or rescanned from the Reset-1 genesis.

EPoSE service keystores are deliberately bound to network type, genesis and
parameter commitment. Old keystores must fail closed on Reset-1. Each node gets
a new identity volume and newly generated service keystore while retaining its
existing reward address.

## Operational boundary

The old chain and identity volumes are retained as read-only rollback archives
until the new four-node chain, public RPC, wallet access and Explorer have been
verified. They are never mounted into the new containers. The four seeds must
run one exact image digest and agree on height-zero hash before mining or public
service operation proceeds.

This decision does not convert historical release-gate results into evidence
for Reset-1. Candidate-bound tests and release artifacts must name the new
genesis explicitly.
