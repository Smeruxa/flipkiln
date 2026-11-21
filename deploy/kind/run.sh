#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
action=${1:-verify}
case "$action" in
    cluster | install | verify) ;;
    *)
        echo 'Usage: deploy/kind/run.sh [cluster|install|verify]' >&2
        exit 2
        ;;
esac

cluster=${FLIPKILN_KIND_CLUSTER:-flipkiln}
namespace=${FLIPKILN_KIND_NAMESPACE:-flipkiln-kind}
port=${FLIPKILN_KIND_PORT:-18080}
image_tag=${FLIPKILN_IMAGE_TAG:-local}
upgrade_tag=${FLIPKILN_UPGRADE_TAG:-"$image_tag-upgrade"}
artifacts=${FLIPKILN_ARTIFACTS:-"$root/test-results/kind"}
export FLIPKILN_BASE_URL="http://127.0.0.1:$port"
export KUBECONFIG=${FLIPKILN_KIND_KUBECONFIG:-"$root/.flipkiln-tools/$cluster.kubeconfig"}
export PATH="$root/.flipkiln-tools:$PATH"
chart=deploy/helm/flipkiln
release=flipkiln
client_endpoint=/api/client/projects/workshop/environments/development/snapshot
postgres_endpoint='flipkiln-postgres:5432/flipkiln?connect_timeout=3'
selected_cluster=false
created_cluster=false
forward_pid=""
restore_control=false
restore_runtime=false
restore_postgres=false
mkdir -p "$artifacts" "$(dirname "$KUBECONFIG")"

kube() { kubectl --context "kind-$cluster" -n "$namespace" "$@"; }
helm_in_cluster() { helm --kube-context "kind-$cluster" --namespace "$namespace" "$@"; }

stop_forward() {
    if [ -n "$forward_pid" ]; then
        kill "$forward_pid" 2>/dev/null || true
        wait "$forward_pid" 2>/dev/null || true
        forward_pid=""
    fi
}

cleanup() {
    local status=$?
    trap - EXIT
    set +e
    stop_forward
    if [ "$selected_cluster" = true ] && [ "$status" -ne 0 ]; then
        kube get pods,services,events,job,pvc,networkpolicy -o wide >"$artifacts/resources.log" 2>&1
        kube describe pods >"$artifacts/pods.log" 2>&1
        kubectl --context "kind-$cluster" get pods -n kube-system >"$artifacts/system-pods.log" 2>&1
        for pod in $(kube get pods -o name); do
            kube logs "$pod" --all-containers >"$artifacts/${pod#pod/}.log" 2>&1
        done
        helm_in_cluster history "$release" >"$artifacts/helm-history.log" 2>&1
    fi
    if [ "$restore_control" = true ]; then kube scale deployment flipkiln-api --replicas=1; fi
    if [ "$restore_runtime" = true ]; then kube scale deployment flipkiln-runtime --replicas=1; fi
    if [ "$restore_postgres" = true ]; then kube scale statefulset flipkiln-postgres --replicas=1; fi
    if [ "$created_cluster" = true ] && [ "$action" = verify ] && [ "${KEEP_CLUSTER:-0}" != 1 ]; then
        kind delete cluster --name "$cluster" --kubeconfig "$KUBECONFIG"
    fi
    exit "$status"
}
trap cleanup EXIT

prepare_cluster() {
    local calico="$artifacts/calico-v3.30.4.yaml" db_password reader_password
    local calico_url=https://raw.githubusercontent.com/projectcalico/calico/v3.30.4/manifests/calico.yaml
    local calico_checksum=1e0f605a1f0a337a198a6ed8512c4ade67a2d8ab239f0bb67062a663fed84e6d
    if kind get clusters | grep -Fxq "$cluster"; then
        if [ "${REUSE_CLUSTER:-0}" != 1 ]; then
            echo "Cluster $cluster exists. Choose a new FLIPKILN_KIND_CLUSTER or set REUSE_CLUSTER=1." >&2
            exit 1
        fi
        kind export kubeconfig --name "$cluster" --kubeconfig "$KUBECONFIG"
    else
        selected_cluster=true
        created_cluster=true
        kind create cluster --name "$cluster" --config deploy/kind/cluster.yaml --kubeconfig "$KUBECONFIG"
    fi
    selected_cluster=true
    curl -fsSL --retry 3 "$calico_url" -o "$calico"
    printf '%s  %s\n' "$calico_checksum" "$calico" | shasum -a 256 -c -
    kubectl --context "kind-$cluster" apply -f "$calico"
    kube -n kube-system rollout status daemonset/calico-node --timeout=6m
    kube wait --for=condition=Ready nodes --all --timeout=3m
    kube -n kube-system rollout status deployment/calico-kube-controllers --timeout=3m
    kube -n kube-system rollout status deployment/coredns --timeout=3m
    if ! kube get namespace "$namespace" >/dev/null 2>&1; then
        kube create namespace "$namespace"
    fi
    if ! kube get secret flipkiln-kind-postgres >/dev/null 2>&1; then
        db_password=$(openssl rand -hex 24)
        kube create secret generic flipkiln-kind-postgres --from-literal="POSTGRES_PASSWORD=$db_password"
    fi
    if ! kube get secret flipkiln-kind-runtime >/dev/null 2>&1; then
        db_password=$(kube get secret flipkiln-kind-postgres -o jsonpath='{.data.POSTGRES_PASSWORD}' |
            openssl base64 -d -A)
        kube create secret generic flipkiln-kind-runtime \
            --from-literal="DATABASE_URL=postgresql://flipkiln:$db_password@$postgres_endpoint" \
            --from-literal=FLIPKILN_ADMIN_PASSWORD=flipkiln-local-demo \
            --from-literal=FLIPKILN_DEMO_TOKEN=flipkiln-local-demo-read-token
    fi
    if ! kube get secret flipkiln-kind-client >/dev/null 2>&1; then
        reader_password=$(openssl rand -hex 24)
        kube create secret generic flipkiln-kind-client \
            --from-literal="DATABASE_URL=postgresql://flipkiln_reader:$reader_password@$postgres_endpoint" \
            --from-literal="FLIPKILN_RUNTIME_PASSWORD=$reader_password"
    fi
    kube apply -f deploy/kind/postgres-policy.yaml -f deploy/kind/postgres.yaml
    kube rollout status statefulset/flipkiln-postgres --timeout=5m
    reader_password=$(client_password)
    kube exec -i flipkiln-postgres-0 -- env "FLIPKILN_RUNTIME_PASSWORD=$reader_password" \
        psql -v ON_ERROR_STOP=1 -U flipkiln -d flipkiln -f - <deploy/postgres/ensure-reader.sql
}

client_password() {
    kube get secret flipkiln-kind-client -o jsonpath='{.data.FLIPKILN_RUNTIME_PASSWORD}' |
        openssl base64 -d -A
}

build_images() {
    if [ "${SKIP_BUILD:-0}" != 1 ]; then
        docker build --tag "flipkiln-api:$image_tag" -f apps/api/Dockerfile .
        docker build --tag "flipkiln-web:$image_tag" -f apps/web/Dockerfile .
    fi
    kind load docker-image --name "$cluster" "flipkiln-api:$image_tag" "flipkiln-web:$image_tag"
}

install_release() {
    local tag=$1
    shift
    helm_in_cluster upgrade --install "$release" "$chart" -f "$chart/values-kind.yaml" \
        --wait --timeout 4m \
        --set-string "api.image.tag=$tag" --set-string "web.image.tag=$tag" "$@" || return
    kube rollout status deployment/flipkiln-api --timeout=3m
    kube rollout status deployment/flipkiln-runtime --timeout=3m
    kube rollout status deployment/flipkiln-web --timeout=3m
}

start_forward() {
    local attempt
    kubectl --context "kind-$cluster" -n "$namespace" port-forward --address 127.0.0.1 \
        svc/flipkiln-web "$port:8080" >"$artifacts/port-forward.log" 2>&1 &
    forward_pid=$!
    for ((attempt = 0; attempt < 30; attempt++)); do
        if grep -Fq "Forwarding from 127.0.0.1:$port" "$artifacts/port-forward.log" &&
            curl -fsS --max-time 2 "$FLIPKILN_BASE_URL/health/web" >/dev/null 2>&1; then return; fi
        if ! kill -0 "$forward_pid" 2>/dev/null; then break; fi
        sleep 1
    done
    cat "$artifacts/port-forward.log" >&2
    return 1
}

read_client_snapshot() {
    curl -fsS --max-time 6 "$FLIPKILN_BASE_URL$client_endpoint" \
        -H 'Authorization: Bearer flipkiln-local-demo-read-token'
}

api_pod() {
    local component=$1
    shift
    kube get pod -l "app.kubernetes.io/instance=flipkiln,app.kubernetes.io/component=$component" "$@"
}

check_network() {
    local db_ip control_ip runtime_ip pg_image control_pod runtime_pod status
    db_ip=$(kube get pod flipkiln-postgres-0 -o jsonpath='{.status.podIP}')
    control_ip=$(api_pod api -o jsonpath='{.items[0].status.podIP}')
    runtime_ip=$(api_pod runtime -o jsonpath='{.items[0].status.podIP}')
    pg_image=$(kube get statefulset flipkiln-postgres -o jsonpath='{.spec.template.spec.containers[0].image}')
    kube delete pod flipkiln-network-check --ignore-not-found --wait=true
    kube run flipkiln-network-check --image="$pg_image" --restart=Never --command -- \
        sh -c "$(cat deploy/kind/check-deny.sh)" probe "$control_ip" "$runtime_ip" "$db_ip"
    kube wait --for=jsonpath='{.status.phase}'=Succeeded pod/flipkiln-network-check --timeout=100s
    kube logs flipkiln-network-check
    kube delete pod flipkiln-network-check --wait=true

    curl -fsS "$FLIPKILN_BASE_URL/api/auth/login" -H 'Content-Type: application/json' \
        --data '{"username":"admin","password":"flipkiln-local-demo"}' >/dev/null
    read_client_snapshot >/dev/null
    control_pod=$(api_pod api -o jsonpath='{.items[0].metadata.name}')
    runtime_pod=$(api_pod runtime -o jsonpath='{.items[0].metadata.name}')
    status=$(kube exec "$runtime_pod" -- curl -sS -o /dev/null -w '%{http_code}' \
        http://127.0.0.1:8080/api/manage/projects)
    test "$status" = 404
    status=$(kube exec "$control_pod" -- curl -sS -o /dev/null -w '%{http_code}' \
        "http://127.0.0.1:8080$client_endpoint")
    test "$status" = 404
}

check_runtime_role() {
    local reader_password permissions
    reader_password=$(client_password)
    permissions=$(kube exec flipkiln-postgres-0 -- env "PGPASSWORD=$reader_password" \
        psql -At -v ON_ERROR_STOP=1 -h 127.0.0.1 -U flipkiln_reader -d flipkiln -c "
      SELECT current_setting('transaction_read_only'),
          has_schema_privilege('public', 'CREATE'),
          has_table_privilege('public.users', 'SELECT'),
          has_table_privilege('public.sessions', 'SELECT'),
          has_table_privilege('public.tokens', 'SELECT'),
          has_table_privilege('public.environments', 'UPDATE'),
          has_function_privilege('public.flipkiln_client_snapshot(text,text,text)', 'EXECUTE')")
    test "$permissions" = 'on|f|f|f|f|f|t'
}

check_independent_scaling() {
    local snapshot control
    snapshot=$(read_client_snapshot)
    restore_runtime=true
    kube scale deployment flipkiln-runtime --replicas=2
    kube rollout status deployment/flipkiln-runtime --timeout=2m
    test "$(kube get deployment flipkiln-api -o jsonpath='{.status.readyReplicas}')" = 1
    control=$(api_pod api -o jsonpath='{.items[0].metadata.name}')
    restore_control=true
    kube scale deployment flipkiln-api --replicas=0
    kube wait --for=delete "pod/$control" --timeout=60s
    test "$(read_client_snapshot)" = "$snapshot"
    kube scale deployment flipkiln-api --replicas=1
    kube rollout status deployment/flipkiln-api --timeout=2m
    restore_control=false
    kube scale deployment flipkiln-runtime --replicas=1
    kube rollout status deployment/flipkiln-runtime --timeout=2m
    restore_runtime=false
}

workload_generations() {
    kube get deployment flipkiln-api flipkiln-runtime flipkiln-web \
        -o jsonpath='{.items[*].metadata.generation}'
}

verify_upgrade() {
    local component old_id new_id
    if [ "$upgrade_tag" = "$image_tag" ]; then
        echo 'FLIPKILN_UPGRADE_TAG must differ from FLIPKILN_IMAGE_TAG.' >&2
        return 1
    fi
    for component in api web; do
        if [ -z "${FLIPKILN_UPGRADE_TAG:-}" ]; then
            {
                printf 'FROM flipkiln-%s:%s\nLABEL org.opencontainers.image.version="%s"\n' \
                    "$component" "$image_tag" "$upgrade_tag"
                if [ "$component" = api ]; then
                    echo 'COPY deploy/kind/fixtures/900_additive.sql /usr/local/share/flipkiln/migrations/'
                fi
            } | docker build --tag "flipkiln-$component:$upgrade_tag" -f - .
        fi
        old_id=$(docker image inspect --format '{{.Id}}' "flipkiln-$component:$image_tag")
        new_id=$(docker image inspect --format '{{.Id}}' "flipkiln-$component:$upgrade_tag")
        test "$old_id" != "$new_id"
    done
    kind load docker-image --name "$cluster" "flipkiln-api:$upgrade_tag" "flipkiln-web:$upgrade_tag"
    node tests/persistence.mjs prepare "$artifacts/before-upgrade.json"
    stop_forward
    install_release "$upgrade_tag"
    if [ -z "${FLIPKILN_UPGRADE_TAG:-}" ]; then
        test "$(kube exec flipkiln-postgres-0 -- psql -At -U flipkiln -d flipkiln \
            -c 'SELECT id FROM public.flipkiln_acceptance_upgrade')" = t
    fi
    start_forward
    node tests/persistence.mjs compare "$artifacts/before-upgrade.json"

    local before after
    before=$(workload_generations)
    kube create secret generic flipkiln-kind-migration-failure \
        --from-literal="DATABASE_URL=postgresql://flipkiln:wrong-password@$postgres_endpoint" \
        --from-literal=FLIPKILN_ADMIN_PASSWORD=flipkiln-local-demo --dry-run=client -o yaml | kube apply -f -
    if install_release "$image_tag" --set-string secrets.migrationSecret=flipkiln-kind-migration-failure; then
        echo 'A failed migration should have blocked the upgrade.' >&2
        return 1
    fi
    local job
    job=$(kube get jobs -l app.kubernetes.io/component=migrate,app.kubernetes.io/instance=flipkiln \
        --sort-by=.metadata.creationTimestamp -o name | tail -n 1)
    kube wait --for=condition=Failed "$job" --timeout=30s
    kube logs "$job" >"$artifacts/expected-migration-failure.log"
    kube delete secret flipkiln-kind-migration-failure
    after=$(workload_generations)
    test "$before" = "$after"
    node tests/persistence.mjs compare "$artifacts/before-upgrade.json"
    stop_forward
    install_release "$image_tag"
    start_forward
    node tests/persistence.mjs compare "$artifacts/before-upgrade.json"
}

assert_outage_status() {
    local path=$1 status attempt
    for ((attempt = 0; attempt < 20; attempt++)); do
        status=$(curl -sS -o /dev/null -w '%{http_code}' --max-time 16 "$FLIPKILN_BASE_URL$path")
        case "$status" in
            502 | 503 | 504) return ;;
        esac
        sleep 1
    done
    echo "Expected an upstream error for $path during the outage, received HTTP $status." >&2
    return 1
}

pod_restart_count() {
    kube get "pod/$1" -o jsonpath='{.status.containerStatuses[0].restartCount}'
}

verify_database_outage() {
    local control runtime control_restarts runtime_restarts
    control=$(api_pod api -o jsonpath='{.items[0].metadata.name}')
    runtime=$(api_pod runtime -o jsonpath='{.items[0].metadata.name}')
    runtime_restarts=$(pod_restart_count "$runtime")
    control_restarts=$(pod_restart_count "$control")
    restore_postgres=true
    kube scale statefulset flipkiln-postgres --replicas=0
    kube wait --for=delete pod/flipkiln-postgres-0 --timeout=60s
    kube wait --for=condition=Ready=false "pod/$control" "pod/$runtime" --timeout=60s
    kube exec "$control" -- curl -fsS --max-time 3 http://127.0.0.1:8080/health/live
    kube exec "$runtime" -- curl -fsS --max-time 3 http://127.0.0.1:8080/health/live
    assert_outage_status /api/auth/session
    assert_outage_status "$client_endpoint"
    kube scale statefulset flipkiln-postgres --replicas=1
    kube rollout status statefulset/flipkiln-postgres --timeout=5m
    kube wait --for=condition=Ready "pod/$control" "pod/$runtime" --timeout=90s
    restore_postgres=false
    test "$control_restarts" = "$(pod_restart_count "$control")"
    test "$runtime_restarts" = "$(pod_restart_count "$runtime")"
    node tests/persistence.mjs compare "$artifacts/before-upgrade.json"
}

prepare_cluster
if [ "$action" = cluster ]; then exit 0; fi
build_images
install_release "$image_tag"
if [ "$action" = install ]; then
    printf 'Open http://127.0.0.1:%s after running:\n' "$port"
    printf 'kubectl --kubeconfig %q -n %q port-forward svc/flipkiln-web %s:8080\n' \
        "$KUBECONFIG" "$namespace" "$port"
    exit 0
fi

start_forward
check_network
check_runtime_role
check_independent_scaling
FLIPKILN_SCREENSHOTS="$artifacts/screenshots" PLAYWRIGHT_HTML_OUTPUT_DIR="$artifacts/playwright-report" \
    npm test --prefix tests -- --output "$artifacts/playwright-results"
verify_upgrade
verify_database_outage
