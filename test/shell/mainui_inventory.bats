#!/usr/bin/env bats

@test "MainUI inventory covers implementation paths and stable removal gates" {
    run python3 /workspace/tools/validate_mainui_inventory.py \
        --repository /workspace \
        --manifest /workspace/build/mainui-responsibilities.json

    [ "$status" -eq 0 ]
    [[ "$output" == "mainui inventory validate: 12 responsibilities" ]]
}

@test "MainUI inventory rejects missing implementation evidence" {
    manifest="$BATS_TEST_TMPDIR/mainui-responsibilities.json"
    cp /workspace/build/mainui-responsibilities.json "$manifest"
    python3 - "$manifest" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
data["responsibilities"][0]["current_paths"] = ["src/not-present"]
with open(path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(data, stream)
PY

    run python3 /workspace/tools/validate_mainui_inventory.py \
        --repository /workspace --manifest "$manifest"

    [ "$status" -eq 1 ]
    [[ "$output" == *"references a missing path: src/not-present"* ]]
}

@test "MainUI inventory rejects duplicate responsibilities and unsafe paths" {
    manifest="$BATS_TEST_TMPDIR/mainui-responsibilities.json"
    cp /workspace/build/mainui-responsibilities.json "$manifest"
    python3 - "$manifest" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
data["responsibilities"][1]["id"] = data["responsibilities"][0]["id"]
data["responsibilities"][1]["current_paths"] = ["../outside"]
with open(path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(data, stream)
PY

    run python3 /workspace/tools/validate_mainui_inventory.py \
        --repository /workspace --manifest "$manifest"

    [ "$status" -eq 1 ]
    [[ "$output" == *"id must be a unique non-empty string"* ]]
    [[ "$output" == *"unsafe repository path: ../outside"* ]]
}
