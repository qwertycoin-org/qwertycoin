# EPoSE receipt diagnostics

EPoSE receipt diagnostics explain local service-check execution without changing
committee selection, quorum, retry timing, receipt validity, qualification, or
rewards. They are observations, not consensus evidence.

## Event model

One receipt attempt begins only after the local node is selected as a verifier,
the receipt is not already canonical, and the existing retry tracker permits a
new try. Routine scheduler decisions such as `not_committee_member`,
`already_canonical`, and `retry_backoff` are counted as skips, not failures.

Every started attempt receives a restart-local numeric `attempt_id` and ends in
exactly one of:

- `success`: the encoded receipt envelope was accepted by local submission;
- `failure`: a typed operational or internal failure occurred;
- `cancelled`: daemon shutdown cancelled the worker;
- `expired`: the round deadline passed before local submission.

Local submission is not canonical inclusion. Canonical inclusion is recorded
only after the receipt slot is observed in canonical EPoSE state. Final
qualification is a third, separate chain-derived state.

The daemon emits individual starts and terminal events at debug level. Round,
periodic, recovery, and final qualification summaries use info level. A
repeated operational failure first warns after three consecutive failures for
the same receipt slot. Further warnings are limited to one per five minutes and
report how many messages were suppressed. Internal failures use error level.

## Stable reason codes

Reason strings are an additive machine-readable interface. Existing strings are
not renamed.

| Reason | Stage | Meaning |
| --- | --- | --- |
| `descriptor_unavailable` | descriptor lookup | No discovery source returned the snapshot-bound descriptor. |
| `descriptor_invalid` | descriptor validation | Descriptor fields, host, port, or signature were invalid. |
| `descriptor_expired` | descriptor validation | Reserved for an existing descriptor-expiry rejection boundary; this observability change adds no new expiry rule. |
| `descriptor_commitment_mismatch` | descriptor validation | Returned or advertised descriptor hash did not match frozen membership. |
| `dns_resolution_failed` | network connect | The endpoint name could not be resolved. |
| `connection_refused` | network connect | The remote host refused the TCP connection. |
| `transport_timeout` | network connect / HTTP | Connect, send, or receive exceeded the existing probe timeout. |
| `transport_failure` | network connect / HTTP | Another bounded transport failure occurred. |
| `http_status_unsuccessful` | HTTP exchange | The endpoint returned a non-200 HTTP status. |
| `remote_challenge_rejected` | HTTP exchange | The probe endpoint explicitly rejected the challenge. |
| `response_malformed` | response decode/validation | JSON, hex, signature, or canonical block encoding was malformed. |
| `response_oversized` | response decode/validation | The bounded HTTP or block response limit was exceeded. |
| `response_unexpected` | response validation | The requested canonical object was unavailable or otherwise unexpected. |
| `response_identity_mismatch` | descriptor/response validation | The service or signing identity did not match the challenge. |
| `response_object_mismatch` | response validation | The returned canonical object or bytes differed from the required object. |
| `response_context_mismatch` | response validation | Network, genesis, parameter, snapshot, anchor, or challenge context differed. |
| `response_signature_mismatch` | response validation | The subject response signature was invalid. |
| `receipt_encoding_failed` | receipt encoding | The authenticated receipt could not be encoded as its typed record. |
| `envelope_encoding_failed` | envelope encoding | The typed record could not be encoded inside the configured envelope budget. |
| `local_submission_rejected` | local submission | Local relay/template admission rejected the envelope. |
| `cancelled` | any worker stage | Shutdown cancellation was observed between bounded steps. |
| `deadline_expired` | local submission | The applicable round deadline passed before submission. |
| `internal_execution_failure` | any stage | An invariant, configuration, async launch, or unexpected exception failed. |

Remote text is never copied into these codes or default logs. The daemon does
not log raw challenges, responses, endpoint payloads, keys, credentials,
keystores, or wallet material.

## Operator-only RPC

`get_epose_diagnostics` is additive and exists only on unrestricted RPC with
RPC authentication configured. Without `--rpc-login`, it returns no diagnostic
data. The restricted public RPC and public probe listener reject it. Keep unrestricted
RPC on loopback or an operator network. A local
client can use a protected netrc file rather than exposing credentials in its
process arguments:

```bash
curl --netrc-file /secure/path/qwertycoind-rpc.netrc \
  -s -X POST http://127.0.0.1:8197/get_epose_diagnostics \
  -H 'Content-Type: application/json' \
  -d '{"recent_limit":50,"epoch":11}'
```

`recent_limit` defaults to 50 and is capped at 100. Optional `epoch` selects
the chain-derived qualification epoch; omitted means current. The response
contains:

- additive schema identifier `diagnostics_version=1`;
- restart-local round counters and grouped failure counters;
- expected scheduler skips;
- bounded recent terminal attempts with public identities and commitments;
- retry scheduling, when known;
- local submission and later canonical-inclusion observations;
- canonical unique receipt counts for the local service identity;
- the epoch-specific service identity resolved from the persistent local
  identity, so historical key rotation is not interpreted through the current
  service key;
- explicit snapshot membership and the final unmet rule;
- committee sizes and dynamically derived quorum per round;
- passed/required rounds and `pending`, `qualified`, `not_qualified`, or
  `unknown` qualification state.

If the snapshot or chain evidence is unavailable, qualification data is
`unknown`; it is never substituted with zero. Before the qualification set is
closed, the state is `pending`, never `not_qualified`.

## Example logs

Routine debug event (public identities abbreviated here only for readability):

```text
event=epose_receipt_attempt_started utc_ms=1790341274986 attempt_id=17 epoch=11 round=1 height=8120 subject=146e... verifier=89ab... endpoint_commitment=03cd... deadline_height=8239 stage=descriptor_lookup outcome=started
event=epose_receipt_attempt_terminal utc_ms=1790341275000 attempt_id=17 epoch=11 round=1 height=8120 outcome=failure stage=network_connect reason=connection_refused duration_ms=14 retry_scheduled=true next_retry_utc_ms=1790341305000
```

Recovery and finalized summary:

```text
event=epose_receipt_recovered attempt_id=20 epoch=11 round=1 previous_failures=3
event=epose_qualification_final utc_ms=1790341305000 epoch=11 outcome=not_qualified canonical_unique_receipts=5/9,0/9,0/9 required_receipts=6,6,6 rounds_passed=0 rounds_required=2 unmet_rounds=2 unmet_rule=receipt_rounds
```

Thresholds are derived from the frozen population and protocol parameters; the
example values are not hard-coded.

## Troubleshooting sequence

1. Confirm the daemon is synchronized and the local service identity is active.
2. Read `qualification_state`. If it is `pending`, do not diagnose a final
   qualification failure yet.
3. Compare each round's canonical receipt count with `required_receipts`.
4. Check grouped failures, then bounded recent attempts for the affected round.
5. Treat `local_submission_accepted=true` only as local admission. Wait for
   `canonical_inclusion_observed=true` before claiming chain inclusion.
6. If descriptor failures follow a renewal or restart, verify that the same
   keystore, active service key, endpoint, and snapshot-bound commitment remain
   available. Never replace a keystore as a diagnostic shortcut.
7. For DNS/refusal/timeout, test the advertised restricted endpoint externally;
   keep unrestricted RPC private.
8. For context, object, or signature mismatch, compare chain tip, genesis,
   parameter set, snapshot, and deployed revision across nodes.

The in-memory operational counters, recent attempts, attempt IDs, and warning
rate-limit state reset on daemon restart. Canonical coverage and finalized
qualification are reconstructed from chain state and therefore do not reset.
Only the four most recent operational epochs, 256 terminal attempts, and 4096
slot states are retained. Response and RPC list sizes are bounded.

Improved diagnostics do not retroactively identify an unrecorded historical
sub-stage and do not by themselves resolve a previous incident.
