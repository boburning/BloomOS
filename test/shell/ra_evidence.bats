#!/usr/bin/env bats

@test "pending gpSP evidence template is valid but cannot imply certification" {
    run python3 /workspace/tools/validate_ra_evidence.py \
        /workspace/docs/validation/retroachievements/gpsp-template.json \
        --policy /workspace/build/ra-core-policy.json
    [ "$status" -eq 0 ]
}

@test "pending PCSX-ReARMed evidence template matches the exact policy SHA" {
    run python3 /workspace/tools/validate_ra_evidence.py \
        /workspace/docs/validation/retroachievements/pcsx-rearmed-template.json \
        --policy /workspace/build/ra-core-policy.json
    [ "$status" -eq 0 ]
}

@test "complete evidence rejects pending results and missing operator fields" {
    candidate="$BATS_TEST_TMPDIR/evidence.json"
    python3 - /workspace/docs/validation/retroachievements/gpsp-template.json "$candidate" <<'PY'
import json
import sys
record = json.load(open(sys.argv[1], encoding='utf-8'))
record['state'] = 'complete'
json.dump(record, open(sys.argv[2], 'w', encoding='utf-8'))
PY
    run python3 /workspace/tools/validate_ra_evidence.py "$candidate" \
        --policy /workspace/build/ra-core-policy.json
    [ "$status" -eq 1 ]
    [[ "$output" == *"complete evidence contains pending results"* ]]
    [[ "$output" == *"complete evidence lacks device"* ]]
}

@test "evidence rejects secrets and raw ROM identity fields" {
    candidate="$BATS_TEST_TMPDIR/evidence.json"
    python3 - /workspace/docs/validation/retroachievements/gpsp-template.json "$candidate" <<'PY'
import json
import sys
record = json.load(open(sys.argv[1], encoding='utf-8'))
record['token'] = 'secret'
record['rom_path'] = '/private/game.gba'
json.dump(record, open(sys.argv[2], 'w', encoding='utf-8'))
PY
    run python3 /workspace/tools/validate_ra_evidence.py "$candidate" \
        --policy /workspace/build/ra-core-policy.json
    [ "$status" -eq 1 ]
    [[ "$output" == *"prohibited secret or ROM identity field"* ]]
}
