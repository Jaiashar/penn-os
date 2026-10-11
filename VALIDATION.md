# Validation

Verified October 11, 2026 using the Linux x86-64 Docker image (Ubuntu 20.04, Clang 10).

- `make` builds both `bin/penn-os` and `bin/pennfat` with the original `-Wall -Werror` flags.
- PennFAT smoke checks pass: create a filesystem, mount it, create a file, list it and unmount it.
- The OS boots against that filesystem in an interactive terminal. `echo`, process listing (`ps`), a background `sleep` process and `jobs` pass smoke checks.
- The published source is secret-scanned before publication.

These checks confirm buildability and basic operation. They are not exhaustive validation of scheduling fairness, filesystem corruption handling, stress behavior, signal races or memory safety. No kernel, scheduler or filesystem implementation was rewritten for publication.
