FROM debian:bookworm-slim AS dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates cmake g++ git libjsoncpp-dev libpq-dev libssl-dev ninja-build \
      nlohmann-json3-dev uuid-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
RUN git clone --branch v1.9.14 --depth 1 --recurse-submodules --shallow-submodules \
      https://github.com/drogonframework/drogon.git /tmp/drogon \
    && test "$(git -C /tmp/drogon rev-parse HEAD)" = 3aae769cc405cf5abb9d477675c6430924f02c7c \
    && cmake -S /tmp/drogon -B /tmp/drogon/build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 \
      -DBUILD_TESTING=OFF -DBUILD_CTL=OFF -DBUILD_EXAMPLES=OFF \
      -DBUILD_YAML_CONFIG=OFF -DBUILD_BROTLI=OFF \
      -DUSE_MYSQL=OFF -DUSE_SQLITE3=OFF \
    && cmake --build /tmp/drogon/build -j 3 \
    && cmake --install /tmp/drogon/build \
    && rm -rf /tmp/drogon

FROM dependencies AS build
WORKDIR /build
COPY apps/api/CMakeLists.txt ./
COPY apps/api/src ./src
RUN cmake -S . -B out -G Ninja -DCMAKE_BUILD_TYPE=Release -DFLIPKILN_BUILD_TESTS=OFF \
    && cmake --build out -j 3

FROM build AS test
COPY apps/api/tests/domain_test.cpp ./tests/
RUN cmake -S . -B out -G Ninja -DFLIPKILN_BUILD_TESTS=ON \
    && cmake --build out -j 3 \
    && ctest --test-dir out --output-on-failure

FROM debian:bookworm-slim AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl libjsoncpp25 libpq5 libssl3 libuuid1 zlib1g \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /build/out/flipkiln-api /usr/local/bin/flipkiln-api
COPY apps/api/migrations /usr/local/share/flipkiln/migrations
COPY contracts/examples/workshop.json /usr/local/share/flipkiln/workshop.json
USER 10001:10001
EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/flipkiln-api"]
