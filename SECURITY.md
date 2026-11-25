# Security policy

## Supported versions
The latest release (and the release candidate before 1.0.0). The public API is stable since 0.4.0; fixes go into the next release.

## Reporting a vulnerability
Please **do not open a public issue** for something that can be exploited. Use GitHub's private reporting: the **Security** tab of this repository, "Report a vulnerability". Say what you did, the version or commit, and what happened (a crash with an AddressSanitizer report, or a minimal input, is ideal: the fuzz harnesses in `tests/` take raw bytes).

I read reports as soon as I can and will tell you what I found and when a fix is out. This is a small project maintained by one person, so there is no bounty and no fixed response time, but a memory-safety bug in the library is the thing I care most about.

## What is in scope
- The core in `src/` and the headers in `include/`: memory safety and denial of service from data the library is given (text, bitmaps, fonts, events, themes).
- The backends in `backends/` (the touch filter, the calibration and its stored blob, the display driver).
- The tools in `tools/` when run on untrusted input (`fontgen.py` on a font file).

## What is not
- Code the application gives the library to run (post-effects, painters, backends, custom widgets): it is trusted by design; see the README, "Security and limits".
- The ESP-IDF, its Wi-Fi and TLS stacks, SDL, FreeType, and physical attacks on a device without secure boot or flash encryption (documented residual risks R-20 to R-22 in `docs/06-controls-and-risks.md`).
- Visual spoofing with look-alike letters.

The threat model is `docs/05-threat-model-stride.md`; the controls and the tests behind each are in `docs/06-controls-and-risks.md`.
