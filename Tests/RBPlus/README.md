# RB+ regression checks

On an x86_64 Mac with the exact macOS 25E253 Metal cache, from the repository root:

```sh
mkdir -p out
clang -O2 -I NootedRed Tests/RBPlus/payload-cpu-test.c -o out/rbplus-cpu-test
out/rbplus-cpu-test
clang++ -std=c++14 -I NootedRed Tests/RBPlus/test-build-gate.cpp -o out/rbplus-build-gate-test
out/rbplus-build-gate-test
```

The CPU test checks 16,384 single-byte page corruptions and 20,000 command-emitter states, including blend transitions. It loads the installed Metal driver but does not create a Metal device or submit GPU work. Exact-page matching is mandatory. If the installed kext already patched the pages, the test restores only its process-private copies for the baseline comparison. The build-gate test covers unavailable, incorrect, truncated, and supported OS build values.

## Scope

`-NRedRBPlus` opts into the correction. It is restricted to build 25E253, the exact Darwin kernel identity, and device 1638 revision c9. Both complete cache pages must match their expected bytes individually before replacement. The OS build check runs at page validation because the build string is empty during early plugin startup.

Eight GPU trials on the earlier 0.8.10-based candidate passed (baseline, alpha, dual-source, transitions at 1x and 4x), followed by user testing in Slack, Claude, Edge and Chrome without the original corruption. The current-tree integration has CPU and Release-build validation; it has not been installed or reboot-tested. Allocation errors and locked-mapping diagnostics also occurred before this fix. Three Metal compiler crashes remain unresolved. Do not describe the system as log-clean.
