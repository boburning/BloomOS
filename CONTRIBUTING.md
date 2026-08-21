# Contributing to BloomOS

BloomOS evolves through small, reviewable changes while keeping the main branch buildable.

## Before opening a pull request

1. Open or reference an issue describing the problem and intended scope.
2. Preserve Onion compatibility unless the change documents why that is impossible.
3. Add or update automated tests for behavior that can be tested off-device.
4. Record physical-device testing for hardware-sensitive changes.
5. Document migration, compatibility, and rollback implications.
6. Preserve copyright, license, and third-party attribution notices.
7. For roadmap or epic work, reference both the specific child issue and its
   parent epic. After merge, comment on each with the merged evidence and the
   concrete work that remains; close an issue only when its acceptance criteria
   are satisfied.

Do not commit ROMs, BIOS files, credentials, private keys, device addresses, or other user data.

## Pull request description

Substantial changes should describe the problem, root cause, approach, alternatives considered, compatibility impact, tests, devices tested, migration impact, and rollback plan.

Issue tracking is part of completion, not a substitute for repository evidence.
Keep broad milestones open while physical, migration, recovery, or release gates
remain pending, even when an individual implementation increment is complete.

By participating, you agree to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
