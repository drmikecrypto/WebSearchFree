# syntax=docker/dockerfile:1
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake git ca-certificates libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DWSF_BUILD_CLI=ON -DWSF_BUILD_SERVER=ON -DWSF_BUILD_TESTS=ON -DWSF_BUILD_PYTHON=OFF \
 && cmake --build build -j \
 && ctest --test-dir build --output-on-failure

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates libcurl4 curl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/wsf /usr/local/bin/wsf

EXPOSE 8080
ENV WSF_SEARX_URL=
ENV WSF_BASE_URL=http://127.0.0.1:8080

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD curl -fsS http://127.0.0.1:8080/health >/dev/null || exit 1

ENTRYPOINT ["wsf"]
CMD ["serve", "--host", "0.0.0.0", "--port", "8080"]
