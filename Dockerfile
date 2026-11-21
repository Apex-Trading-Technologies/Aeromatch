# ==============================================================================
# Multi-stage Dockerfile for AeroMatch Open-Source Matching Engine
# ==============================================================================

# Stage 1: Build environment
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DAEROMATCH_BUILD_TESTS=ON \
    -DAEROMATCH_BUILD_BENCHMARKS=OFF \
    && cmake --build build --target aeromatch aeromatch_tests

# Run unit tests during container build
RUN ctest --test-dir build --output-on-failure

# Stage 2: Minimal runtime image
FROM ubuntu:24.04-slim AS runtime

RUN useradd -m -u 1000 -s /bin/bash aeromatch

WORKDIR /home/aeromatch
COPY --from=builder /app/build/aeromatch /usr/local/bin/aeromatch

USER aeromatch
EXPOSE 8080

ENTRYPOINT ["aeromatch"]
CMD ["--test-run"]
