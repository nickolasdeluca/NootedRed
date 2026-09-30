# Exact-build payload generation

Run `python3 Tools/RBPlus/build-rbplus-payload.py` on the matching x86_64 macOS 25E253 installation. It assembles `rbplus-payload.s`, rejects relocations, verifies the original cache page hashes against `static-20260929/manifest.json`, and regenerates `NootedRed/rbplus-payload.h`. Object, disassembly, and digest outputs stay in this directory and are ignored. The payload disables RB+ for dual-source blending and updates color control when blend state changes. This is not a generic patch for other OS builds or GPUs.

Regeneration on the current patched boot refused a cache-page hash mismatch, so the generator has not rewritten the header. Use an unmodified cache whose hashes match the manifest; do not bypass the hash check. The byte arrays in the committed header are identical to the tested candidate; its `#pragma once` and offset `#define`s were added by hand in the layout the current generator emits.
