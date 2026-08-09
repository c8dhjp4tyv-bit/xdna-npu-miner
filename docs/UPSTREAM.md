# Upstream Sources and Licensing

This file is the authoritative registry of external sources used to understand or implement the mining protocol/workload.

## Rule

Do not rely on remembered protocol behavior. For every material algorithm/protocol claim, record the current authoritative source, exact version/commit/tag when available, relevant file/path, and licensing implications.

## M0 source audit checklist

The M0 agent must identify and verify, at minimum:

- official Qubic documentation relevant to mining/useful proof of work;
- official/current Qubic core/node implementation relevant to work generation and validation;
- official/current reference miner(s), if any;
- current pool/proxy/protocol specifications or reference implementations used by miners;
- exact algorithm implementation defining candidate generation/mutation/scoring;
- license text for every repository used as an implementation reference;
- any protocol/version negotiation or epoch/algorithm-switch behavior that can invalidate a miner.

## Source record template

Copy this section for each material source:

```text
### Source S-NNN

Project/source:
Authority: Official / Maintainer / Secondary
URL/repository:
Commit/tag/version:
Accessed date:
Relevant files/paths:
Relevant behavior established:
License:
Reuse decision: Allowed / Reference only / Clean-room required / Unresolved
Notes:
```

## Licensing decision requirements

For every source whose code may influence implementation, answer:

1. What is the exact license?
2. Is modification/distribution permitted?
3. Does it impose source-disclosure, field-of-use, notice, attribution or other conditions?
4. Are those terms compatible with the intended project license?
5. May code be copied/adapted, or should it be used only as behavioral reference?
6. If uncertain, treat code as reference-only until resolved.

## Clean-room guidance

When a source is reference-only or licensing is incompatible:

- document externally observable behavior and protocol semantics;
- derive independent test vectors where legally/technically appropriate;
- implement from the specification/behavior rather than copying structure or code;
- record the decision in `docs/DECISIONS.md`.

## Initial state

No upstream source has yet been accepted here as authoritative for implementation. M0 must populate this registry before M1 begins.
