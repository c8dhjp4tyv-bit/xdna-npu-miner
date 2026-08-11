# Pearl Testing

Run CPU, gateway/work contracts, full CTest, physical P2/P3/P5 differentials,
P8/P9 benchmarks, CLI modes, and the P10 endurance harness in that order.
Every XDNA record includes exact CPU parity, dispatch completion, transfer
counters, and `cpu_fallbacks: 0`. Gateway tests use bounded local Unix sockets
and cover malformed JSON, invalid base64, oversized targets, unsafe endpoints,
and rejection categories. P7 remains blocked unless an official gateway,
prover/useful-work provider, and local/simnet node are actually running.
