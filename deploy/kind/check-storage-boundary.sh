#!/bin/sh
set -eu
allowed=$1
other=$2
storage=http://flipkiln-storage-api:8080

status() {
    curl -sS -o /dev/null --max-time 5 -w '%{http_code}' "$@"
}

with_key() {
    printf 'X-Flipkiln-Storage-Key: %s\n' "$FLIPKILN_STORAGE_KEY" | status -H @- "$1"
}

test -z "${DATABASE_URL+x}"
test -z "${FLIPKILN_STORAGE_READER_DATABASE_URL+x}"
getent hosts flipkiln-postgres >/dev/null
curl --version | grep -qw telnet
db_socket=telnet://flipkiln-postgres:5432
if db_result=$(curl -v --connect-timeout 3 --max-time 4 "$db_socket" </dev/null 2>&1); then
    exit 1
fi
case "$db_result" in
    *'Connected to'*) exit 1 ;;
esac
for mode in control runtime; do
    endpoint="$storage/internal/v1/$mode/ready"
    test "$(status "$endpoint")" = 403
    test "$(status -H 'X-Flipkiln-Storage-Key: invalid-fixture-storage-secret-value' "$endpoint")" = 403
done
test "$(with_key "$storage/internal/v1/$allowed/ready")" = 200
test "$(with_key "$storage/internal/v1/$other/ready")" = 403
test "$(with_key "$storage/api/auth/session")" = 404
printf '%s storage access passed with independent credentials\n' "$allowed"
