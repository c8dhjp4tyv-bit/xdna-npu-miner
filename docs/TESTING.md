# Testing Strategy

Testing is organized around correctness first, then hardware verification, then performance.

## Test classes

### 1. Unit tests
Pure deterministic tests for parsing, serialization, arithmetic helpers, domain types and edge cases.

### 2. CPU golden-vector tests
Trusted reference outputs for the exact mining/scoring workload selected in M0/M1.

Golden vectors must capture all semantics relevant to accelerator agreement, including fixed-width arithmetic, overflow/saturation, rounding, layout and endianness where applicable.

### 3. Differential CPU/NPU tests
For identical inputs:

```text
CPU reference output == XDNA1 output
```

Where exact equality is not mathematically appropriate, the accepted comparison rule must be justified from authoritative arithmetic semantics before the test is written. Tolerances must never be widened merely to hide accelerator mismatch.

### 4. Hardware smoke tests
Verify:

- expected XDNA1 device identity;
- runtime/driver accessibility;
- deterministic NPU execution;
- actual NPU dispatch evidence;
- explicit failure when hardware/runtime is unavailable.

### 5. Protocol/integration tests
Use authoritative captured/mock vectors where possible and live authorized endpoints only when appropriate.

Verify:

- job/work acquisition;
- parsing;
- stale-work handling;
- reconnection;
- result/share serialization;
- submission responses;
- malformed input;
- timeout behavior.

### 6. End-to-end mining tests
Combine job -> candidate/control -> XDNA compute -> CPU verification -> submission.

No end-to-end test may skip CPU/NPU mismatch detection.

### 7. Endurance/recovery tests
Exercise:

- long-running workload;
- repeated job switches;
- XRT/NPU runtime errors;
- worker/device recreation if architecture uses it;
- network disconnect/reconnect;
- bounded resource usage;
- graceful shutdown.

## Required evidence format

For material test runs, record in `docs/AI_HANDOFF.md` or a versioned test artifact:

- command;
- git commit;
- hardware identity;
- software stack versions;
- test count;
- pass/fail count;
- exact failures;
- whether NPU hardware was actually exercised.

## M0 testing work

M0 has no miner implementation to test. It must instead define authoritative interoperability vectors/sources to be used by M1 and M6.

## M1 minimum gate

- deterministic CPU golden implementation;
- checked/reproducible vectors;
- edge arithmetic tests;
- authoritative comparison where available.

## M2 minimum gate

- XDNA1 device detected positively;
- tiny deterministic hardware kernel executes;
- output matches CPU expected result;
- hardware dispatch evidence recorded.

## M3/M4 minimum gate

- representative fixed vectors;
- boundary vectors;
- randomized/differential corpus;
- reproducible failing inputs;
- zero unexplained mismatches.

## M5+ regression rule

Every optimization must rerun the correctness suite before benchmark results are accepted.

## Security-oriented tests

Ensure the application does not:

- expose secrets in logs;
- accept unsafe implicit remote-control behavior;
- continue mining after explicit shutdown;
- silently alter configured endpoints;
- hide CPU fallback as NPU execution.
