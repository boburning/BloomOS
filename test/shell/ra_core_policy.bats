#!/usr/bin/env bats

@test "RA core policy matches every exact shipped default core identity" {
    run python3 /workspace/tools/validate_ra_core_policy.py \
        --policy /workspace/build/ra-core-policy.json \
        --core-manifest /workspace/build/core-manifest.json
    [ "$status" -eq 0 ]
    [[ "$output" == *"matches exact shipped core identities"* ]]
}

@test "RA core policy rejects stale SHA and missing default system" {
    candidate="$BATS_TEST_TMPDIR/policy.json"
    python3 - /workspace/build/ra-core-policy.json "$candidate" <<'PY'
import json
import sys
policy = json.load(open(sys.argv[1], encoding='utf-8'))
policy['entries'][0]['binary_sha256'] = '0' * 64
policy['entries'] = [entry for entry in policy['entries'] if entry['system'] != 'gba']
json.dump(policy, open(sys.argv[2], 'w', encoding='utf-8'))
PY
    run python3 /workspace/tools/validate_ra_core_policy.py \
        --policy "$candidate" --core-manifest /workspace/build/core-manifest.json
    [ "$status" -eq 1 ]
    [[ "$output" == *"stale core SHA"* ]]
    [[ "$output" == *"missing required default systems: gba"* ]]
}

@test "RA core policy rejects Verified claim without physical evidence" {
    candidate="$BATS_TEST_TMPDIR/policy.json"
    python3 - /workspace/build/ra-core-policy.json "$candidate" <<'PY'
import json
import sys
policy = json.load(open(sys.argv[1], encoding='utf-8'))
policy['entries'][0]['bloom_ra_status'] = 'verified'
json.dump(policy, open(sys.argv[2], 'w', encoding='utf-8'))
PY
    run python3 /workspace/tools/validate_ra_core_policy.py \
        --policy "$candidate" --core-manifest /workspace/build/core-manifest.json
    [ "$status" -eq 1 ]
    [[ "$output" == *"verified policy lacks physical evidence"* ]]
}

@test "RA core policy permits fallbacks but rejects a duplicate system core pair" {
    candidate="$BATS_TEST_TMPDIR/policy.json"
    python3 - /workspace/build/ra-core-policy.json "$candidate" <<'PY'
import json
import sys
policy = json.load(open(sys.argv[1], encoding='utf-8'))
policy['entries'].append(dict(policy['entries'][0]))
json.dump(policy, open(sys.argv[2], 'w', encoding='utf-8'))
PY
    run python3 /workspace/tools/validate_ra_core_policy.py \
        --policy "$candidate" --core-manifest /workspace/build/core-manifest.json
    [ "$status" -eq 1 ]
    [[ "$output" == *"duplicate policy system/core"* ]]
}
