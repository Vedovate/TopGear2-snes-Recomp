# Security Policy & Legal / DMCA Scope

## Copyright & DMCA Compliance

This repository contains **zero copyrighted game assets**:

- No SNES ROM images (`*.sfc`, `*.smc`, `*.fig`, `*.swc`, `*.rom`).
- No pre-lifted static recompilation C shards (`generated/*.c` must be generated locally by the user from their own legally obtained copy of `Top Gear 2 (USA).sfc`).
- No extracted artwork, tile data, music, or sound samples.
- No bundled proprietary runtime libraries (`SDL2.dll` is loaded dynamically from the user's system and is not committed).

If you believe any file in this repository inadvertently infringes copyright, please open a GitHub issue marked `[Legal/DMCA]` or contact the maintainer directly so it can be audited and removed promptly.

## Reporting Security Vulnerabilities

If you discover a memory-safety vulnerability, buffer overflow, path-traversal bug, or unsafe file-parsing issue in the C runtime, lifter, save-state loader, or config parser:

1. **Do not** open a public issue with an exploit payload if it affects end users running untrusted save states or ROMs.
2. Report it privately via GitHub Security Advisories (**Security -> Advisories -> Report a vulnerability**) on this repository.
3. Include steps to reproduce, affected source files, and a minimal test case where possible.
