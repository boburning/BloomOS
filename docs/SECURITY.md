# Engineering Security Model

BloomOS is offline-first and collects no telemetry by default. Diagnostics remain local unless a user explicitly exports them.

Production SSH is off by default. Automatic developer SSH is limited to
Wi-Fi-capable hardware and requires both `/mnt/SDCARD/.bloom-dev` and a valid
public-key file under `.bloom/authorized_keys`. Password and
blank-password authentication are disabled. Private keys, device addresses,
and accepted host fingerprints remain on the developer workstation.
BloomOS bundles a dedicated Dropbear server with public-key authentication and
modern SSH algorithms. `bloom-device` pins the configured identity, permits
public-key authentication only, and refuses unknown or changed host keys.

Updates and packages must be staged and verified with at least SHA-256; release signing is preferred. Downloaded code must never be executed through a `curl | sh` pattern. Logs and support bundles must redact credentials, tokens, and private keys.

Security-sensitive migrations and repair operations must be bounded, logged, reversible where possible, and validated before activation.
