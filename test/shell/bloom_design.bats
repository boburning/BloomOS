#!/usr/bin/env bats

@test "Bloom design tokens and original radial mark validate" {
    run python3 /workspace/tools/validate_bloom_design.py \
        --tokens /workspace/build/bloom-design-tokens.json \
        --mark /workspace/src/bloomUi/assets/bloom-mark.svg

    [ "$status" -eq 0 ]
    [[ "$output" == "Bloom design validate: 9 tokens, 4 mark colors" ]]
}

@test "Bloom design rejects an inaccessible text pair" {
    tokens="$BATS_TEST_TMPDIR/tokens.json"
    cp /workspace/build/bloom-design-tokens.json "$tokens"
    python3 - "$tokens" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
data["palette"]["cream"] = data["palette"]["canvas"]
with open(path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(data, stream)
PY

    run python3 /workspace/tools/validate_bloom_design.py \
        --tokens "$tokens" --mark /workspace/src/bloomUi/assets/bloom-mark.svg

    [ "$status" -eq 1 ]
    [[ "$output" == *"contrast cream/canvas is 1.00, below 4.5"* ]]
}
