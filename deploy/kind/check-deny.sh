#!/bin/sh
set -eu
control_ip=$1
runtime_ip=$2
storage_ip=$3
db_ip=$4
for service in flipkiln-postgres flipkiln-api flipkiln-runtime flipkiln-storage-api; do
    getent hosts "$service"
done
for host in "$control_ip" flipkiln-api "$runtime_ip" flipkiln-runtime "$storage_ip" flipkiln-storage-api; do
    if timeout 6 wget -T 4 -qO- "http://$host:8080/health/live"; then
        printf 'Unexpected direct HTTP access at %s\n' "$host" >&2
        exit 1
    fi
    printf 'Direct HTTP connection denied at %s\n' "$host"
done
for host in "$db_ip" flipkiln-postgres; do
    rc=0
    pg_isready -h "$host" -t 4 || rc=$?
    test "$rc" -eq 2
done
