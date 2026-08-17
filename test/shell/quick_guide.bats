#!/usr/bin/env bats

@test "Bloom Quick Guide artwork regenerates byte for byte" {
    target="$BATS_TEST_TMPDIR/quick-guide"

    run python3 /workspace/tools/generate_quick_guide.py --output "$target"

    [ "$status" -eq 0 ]
    for page in page1.png page2.png page3.png page4.png; do
        cmp "$target/$page" "/workspace/static/packages/App/Quick Guide/App/Onion_Manual/$page"
    done
}

@test "Bloom Quick Guide pages use the InfoPanel display dimensions" {
    target="$BATS_TEST_TMPDIR/quick-guide"
    python3 /workspace/tools/generate_quick_guide.py --output "$target"

    run python3 - "$target" <<'PY'
import pathlib
import struct
import sys

for page in pathlib.Path(sys.argv[1]).glob("page*.png"):
    data = page.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    assert struct.unpack(">II", data[16:24]) == (640, 480)
PY
    [ "$status" -eq 0 ]
}
