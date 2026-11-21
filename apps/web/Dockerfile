FROM node:22.22.3-alpine@sha256:e58326d0d441090181ac150dc2078d3e2cf6a0d42e809aebba3ef5880935ffdd AS build
WORKDIR /src/apps/web
COPY apps/web/package.json apps/web/package-lock.json ./
RUN npm ci
COPY apps/web ./
COPY contracts/generated /src/contracts/generated
RUN npm run build

FROM nginxinc/nginx-unprivileged:1.28.0-alpine@sha256:c97ff0bf7cbae369953c6da1232ec14ad9f971d66360c5698db0856a4cd657a0
ENV FLIPKILN_API_UPSTREAM=http://api:8080
ENV FLIPKILN_RUNTIME_UPSTREAM=http://runtime:8080
ENV NGINX_ENVSUBST_OUTPUT_DIR=/tmp
ENV NGINX_ENVSUBST_FILTER="^FLIPKILN_(API|RUNTIME)_UPSTREAM$"
COPY apps/web/docker/nginx.conf /etc/nginx/nginx.conf
COPY apps/web/docker/app.conf.template /etc/nginx/templates/app.conf.template
COPY --from=build /src/apps/web/dist/ /usr/share/nginx/html/
EXPOSE 8080
