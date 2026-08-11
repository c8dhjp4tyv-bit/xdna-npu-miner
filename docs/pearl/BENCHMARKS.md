# Pearl Benchmarks

P8 uses a fixed 16-item P2 corpus, two warm-ups, batches 1/2/4/8, and
one/two/four-column artifacts. P9 uses the selected c4 artifact, tile 4x64x8,
rank 128, K 2048, eight warm-ups, 100 raw iterations, and two full-candidate
iterations. Exact results and runtime identity are in
`docs/evidence/pearl-p8-batching-four-column.json` and
`docs/evidence/pearl-p9-benchmark.json`. Power and NPU telemetry are null when
no trustworthy measurement source is available; no profitability is inferred.
