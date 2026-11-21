# Deploying Flipkiln on Kubernetes

The Helm chart installs the Vue web server, C++ management API and C++ client runtime as separate Deployments and ClusterIP Services. Both API Deployments can scale independently and share the API build by default. The browser sends `/api` requests to the same origin as the console. The web server proxies `/api/client/` to `<release>-runtime:8080` and other `/api` routes to `<release>-api:8080`. Optional TLS Ingress routes all browser traffic to the web Service. PostgreSQL, credentials, the Ingress controller, TLS certificates, registry access, storage and a NetworkPolicy-capable CNI are provided by the operator.

## Test installation with kind

Prerequisites are a running Docker Engine, Node.js 22, npm, Bash, curl and OpenSSL. The tool script downloads checksummed kind, Helm and kubectl clients for Linux and macOS. The cluster pins the kind node image, Calico manifest and PostgreSQL image and allocates a separate PersistentVolumeClaim for PostgreSQL.

```sh
deploy/kind/tools.sh
export PATH="$PWD/.flipkiln-tools:$PATH"
deploy/kind/run.sh install
kubectl --kubeconfig .flipkiln-tools/flipkiln.kubeconfig -n flipkiln-kind \
    port-forward svc/flipkiln-web 18080:8080
```

Open <http://127.0.0.1:18080> for the console and <http://127.0.0.1:18080/demo> for the client demo. The example signs in as `admin` with `flipkiln-local-demo`. The demo uses the read token `flipkiln-local-demo-read-token`, restricted to `workshop/development`. This profile is intended for local use. The Helm install is named `flipkiln` in namespace `flipkiln-kind`. The runner requires `REUSE_CLUSTER=1` to reuse a named cluster.

To run the full acceptance suite, create a separate clean cluster. The script builds and loads the exact image tags it installs. Set `SKIP_BUILD=1` if those images have already been built. On Linux, install Chromium's system dependencies with `playwright install --with-deps chromium`.

```sh
npm ci --prefix tests
npm exec --prefix tests -- playwright install chromium
FLIPKILN_KIND_CLUSTER=flipkiln-acceptance deploy/kind/run.sh verify
```

The suite opens the installed console and demo in Chromium, publishes and rolls back, checks network allow and deny paths, tests client availability while the management API is offline, and verifies the active release, unpublished draft, history and ETag across image changes. The default successor image adds a test-only additive migration, and the suite verifies that the previous image can use the upgraded schema. To exercise a new application build, prebuild `flipkiln-api:<tag>` and `flipkiln-web:<tag>` and pass `FLIPKILN_UPGRADE_TAG=<tag>`. The suite also installs a failing migration hook to verify that the running Deployment stays unchanged, and restarts PostgreSQL from the same PVC to check readiness, liveness and recovery for both API modes. It writes captures and failure diagnostics to `test-results/kind`. `KEEP_CLUSTER=1` retains a newly created cluster for diagnosis. Tests never delete a reused cluster.

## Testing and production installations

Use an independent release, namespace, database, credentials and TLS Secret for each installation. For example, create a `flipkiln-testing` release in namespace `flipkiln-testing` and a `flipkiln-production` release in namespace `flipkiln-production`. Each installation contains its own product environments `development` and `production`. In the production example, PostgreSQL is reached by private DNS and access is limited by a configured database IP range and the database firewall. Configure the DNS Pod labels for the cluster resolver and the Ingress controller labels for the deployed controller. For a PostgreSQL instance in a different namespace, use `networkPolicy.postgres.namespace` and matching Pod labels and install the database ingress policy in its namespace. `manageIngress: true` applies to the database Pods in the release namespace, where a kind-like bootstrap policy protects PostgreSQL before the first app hook runs. Namespace write privileges, including the ability to create Pods bearing the allowed labels, belong only to the trusted deployment operator.

1. Build API and web images at repository root with `docker build -f apps/api/Dockerfile .` and `docker build -f apps/web/Dockerfile .`. Use a registry the cluster can reach and tag both images with an immutable revision. Set `imagePullSecrets` for a private registry.
2. Prepare PostgreSQL and restrict its network access to the installation. Use a dedicated database, a control user and a separate client runtime user. The pilot uses the same schema-owning control user for the application and migration Job. Use separate Secret objects so only the migration Job receives the bootstrap password. On a shared PostgreSQL server, give each installation distinct database users, revoke database `CONNECT` from `PUBLIC` and grant it to the users for that installation. Provision the client user with `CONNECT` to the database and `USAGE` on `public`, set `default_transaction_read_only = on`, and deny access to the underlying Flipkiln tables. Revoke writable public-schema privileges and database temporary-table privileges inherited from `PUBLIC` in the dedicated database. Set `migrations.runtimeRole` to the client user's PostgreSQL role. The migration Job grants `EXECUTE` on `public.flipkiln_client_snapshot(text,text,text)`. Check the database firewall, TLS and certificate trust for managed PostgreSQL. The PostgreSQL address should resolve and accept connections before starting Helm.
3. Create the namespace and the Secrets referenced in the selected values overlay. Keep their plain-text source values outside Git and prefer your existing Secret manager. The management and client runtime Secrets each need their own `DATABASE_URL`. The migration Secret needs `DATABASE_URL` and `FLIPKILN_ADMIN_PASSWORD`. Provide a strong unique password for the initial `admin` account. The management Deployment and migration Job may share a Secret containing both keys. Supply the client connection separately via `runtime.existingSecret`. Kind optionally seeds `FLIPKILN_DEMO_TOKEN`. Keep `config.demo: false` and `config.secureCookie: true` for testing and production.
4. If your cluster denies all namespace egress or you are moving the database to a new address, allow DNS and the needed PostgreSQL connection for the migration Job before running Helm. Pre-install hooks run before the chart applies its policies. Pre-upgrade hooks initially run under the existing policies. Match `app.kubernetes.io/instance=<release>` and `app.kubernetes.io/component=migrate`, and narrow the permission to the cluster resolver and private database target. Configure the host, the TLS Secret in the release namespace, the database selectors or CIDRs and the registry references in a local copy of the appropriate overlay. The example production values target Traefik on K3s. Use the Ingress class and controller selectors from the target cluster. Create a DNS record for the host and configure the Ingress controller and certificate first.
5. Install or upgrade through Helm. The hook Job runs the new API image with `migrate` before the workloads change. Helm waits for a successful Job and blocks the upgrade if it fails. Wait for Deployment readiness before sending user traffic.

```sh
kubectl create namespace flipkiln-production
kubectl -n flipkiln-production create secret generic flipkiln-production-control \
    --from-env-file=/secure/path/flipkiln-control.env
kubectl -n flipkiln-production create secret generic flipkiln-production-client \
    --from-env-file=/secure/path/flipkiln-client.env
kubectl -n flipkiln-production create secret generic flipkiln-production-migrations \
    --from-env-file=/secure/path/flipkiln-migrations.env
helm upgrade --install flipkiln-production deploy/helm/flipkiln \
    --namespace flipkiln-production --values /secure/path/flipkiln-production.yaml \
    --wait --timeout 5m
kubectl -n flipkiln-production rollout status deploy/flipkiln-production-api
kubectl -n flipkiln-production rollout status deploy/flipkiln-production-runtime
kubectl -n flipkiln-production rollout status deploy/flipkiln-production-web
```

Readiness in both API modes checks its access to the relevant database schema. API liveness checks the process, so a database outage removes the API Pods from traffic while allowing them to recover. Use Kubernetes storage backups and PostgreSQL recovery appropriate to the installation. If migrations fail, inspect `kubectl -n <namespace> get jobs`, then `kubectl -n <namespace> logs job/<release>-migrate-<revision>`. Fix PostgreSQL reachability, credentials or the failed change before retrying. A retry repeats the idempotent migration check. Keep schema updates additive while older Pods may still run.

Rolling back a **configuration** in the Flipkiln release history creates a new active revision from the selected snapshot and replaces the draft. Reverting the **application version** uses Helm and requires compatibility of the old image with the already migrated database:

```sh
helm -n flipkiln-production history flipkiln-production
helm -n flipkiln-production rollback flipkiln-production <helm-revision> --wait --timeout 5m
```

If the old image cannot use the migrated schema, restore from the appropriate database backup under the deployment plan. Helm changes the workload manifests and image references while retaining the database.
