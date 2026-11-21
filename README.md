# Flipkiln

Flipkiln is a feature flag and remote configuration console for small teams. Edit a draft, preview targeting, review the diff, and publish an immutable revision. Applications read the active release over HTTP. Rollback publishes a new revision from a snapshot in release history.

## Run with Docker Compose

```sh
docker compose up --build -d --wait
docker compose ps
```

Open [the console](http://localhost:8080) and sign in as `admin` with `flipkiln-local-demo`. The local fixture includes the **Workshop** project and a Development read token, `flipkiln-local-demo-read-token`. Admins can create and revoke environment-scoped tokens under **Read tokens**.

1. Open **Checkout demo** in a second tab. Paste the local read token and select **Connect demo**. On a fresh installation the app shows **Standard checkout**, revision 1.
2. In **Workshop / Development**, open `checkout.fast_lane`, turn its **Default value** on, then select **Save draft**. The demo still shows standard checkout.
3. Select **Review changes**, check the `false` to `true` diff, then select **Publish release**. The next checkout poll shows **Express checkout** and the new revision.
4. Open **Releases**, select **Revision 1**, and choose **Review rollback** and **Roll back release**. The demo returns to **Standard checkout** at a new revision.
5. Open `checkout.copy`, set `maxItems` to `0` in **Default value**, and select **Validate**. The console shows the schema error for `/maxItems`.

`docker compose down` stops the local services and retains the PostgreSQL volume.

## Kubernetes

[Installation and update guide](deploy/README.md) · [Helm chart](deploy/helm/flipkiln) · [CI workflow](.github/workflows/ci.yml)

## Implementation

- **Console and demo** use Vue 3, TypeScript, Vite, Vue Router, TanStack Vue Query and Reka UI.
- **API** uses C++20, Drogon, CMake and PostgreSQL. Kubernetes runs management and application reads in separate Deployments with independent scaling.
- **Contracts and checks** use OpenAPI and generated TypeScript types, GoogleTest, Vitest and Playwright.
- **Deployment** uses Docker Compose for local development and Helm on Kubernetes.

[OpenAPI](contracts/openapi.yaml)

## Develop

```sh
npm ci
npm run format:check
npm ci --prefix contracts
npm run check --prefix contracts
npm ci --prefix apps/web
npm run lint --prefix apps/web
npm test --prefix apps/web
```

Run `npm run format` to apply Prettier. EditorConfig sets four-space indentation. The frontend lint checks line length and the API uses clang-format. The [CI workflow](.github/workflows/ci.yml) runs builds and deployment checks.
