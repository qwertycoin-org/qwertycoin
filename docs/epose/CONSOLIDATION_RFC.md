# Consolidation RFC

> Historical snapshot - not current protocol documentation. This RFC records an
> earlier consolidation proposal and is retained for audit history.

## Historical Concept

Historical QWC material describes consolidation as a wallet-side scalability feature that combines many old outputs into fewer outputs. It also compares the idea to Genesis Reference Blocks for node-side scalability.

## Monero-Based Assessment

Monero already has production pruning for prunable transaction data while preserving independent validation properties. A modern Monero-based QWC node still needs enough historical data for:

- key image and double-spend checks,
- RingCT validation,
- output and decoy selection,
- wallet restore from seed,
- reorg validation,
- independent sync from genesis or trusted checkpoints.

## Decision

Do not implement historical Reference/Mega Blocks in the first EPoSE v2 consensus upgrade.

Reason: a reference-state shortcut risks weak subjectivity, privacy regressions, restore problems for long-offline wallets, and new trust assumptions. Monero pruning should be inherited and tested first.

This decision can be revisited only if a concrete mechanism preserves independent validation, wallet restore, decoy correctness, and reorg safety.
