#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
awk 'length > 110 {
    printf "%s:%d: line is %d characters wide (maximum 110)\n", FILENAME, FNR, length;
    failed = 1;
}
END {exit failed}' compose.yaml .github/workflows/*.yml \
    deploy/kind/*.sh deploy/kind/*.yaml deploy/kind/fixtures/*.sql deploy/postgres/*.sql \
    deploy/helm/flipkiln/*.yaml deploy/helm/flipkiln/templates/* tests/*.ts tests/*.mjs tests/browser/*.ts
