# Engineering Security Model

BloomOS is offline-first and collects no telemetry by default. Diagnostics remain local unless a user explicitly exports them.

Production SSH is off by default. Developer mode may enable SSH on Wi-Fi devices, with key authentication preferred and no universal private keys or credentials committed to the project.

Updates and packages must be staged and verified with at least SHA-256; release signing is preferred. Downloaded code must never be executed through a `curl | sh` pattern. Logs and support bundles must redact credentials, tokens, and private keys.

Security-sensitive migrations and repair operations must be bounded, logged, reversible where possible, and validated before activation.
