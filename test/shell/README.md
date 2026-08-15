# Shell integration tests

This harness runs BloomOS shell tests in a pinned Alpine container with BATS. It provides an isolated fake SD-card tree and mock command directory; it never mounts a real handheld card writable.

Run the suite with:

```sh
make test-shell
```

Test files should load `support/test_helper`, call `setup_bloom_fixture` from `setup()`, and invoke production scripts through the shell they declare. Add narrowly scoped mocks instead of placing host utilities or credentials in fixtures.

The repository is mounted read-only. Per-test writable state lives under the container's temporary directory and is removed by BATS teardown.
