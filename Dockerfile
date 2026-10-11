# syntax=docker/dockerfile:1

# ---------- Build stage ----------
FROM ubuntu:24.04 AS builder

# Core build tools + libraries Drogon needs if an HTTP layer is added
# later (jsoncpp, openssl, uuid, zlib are Drogon's usual system
# dependencies — safe to include now so the image doesn't need changes
# for that then). Not needed for the CLI binary this image ships today.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    libssl-dev \
    zlib1g-dev \
    uuid-dev \
    libjsoncpp-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy only what the build actually needs (not tests/, example/,
# .venv/, third_party/googletest/, ...) so unrelated file changes don't
# bust this layer's cache and the build context sent to the daemon
# stays small.
COPY CMakeLists.txt ./
COPY src ./src
COPY third_party/nlohmann ./third_party/nlohmann

# FITSOLVER_BUILD_PYTHON=OFF: this image only ships the fitsolver_app CLI
# binary below, not the Python extension module, so there's no need for
# Python dev headers in the build stage.
# FITSOLVER_BUILD_TESTS=OFF: skips fetching/building GoogleTest -- no
# network dependency and no wasted build time for a test binary that
# never ships. CI (see .github/workflows/ci.yml) already builds and
# runs the full test suite on every push/PR, so this doesn't reduce
# actual test coverage.
# FITSOLVER_BUILD_BENCHMARKS=OFF: the manual timing/benchmark targets'
# sources live under tests/ and benchmarks/, which aren't copied above,
# and CMake fails to generate *any* target whose source is missing --
# which is what broke this build from 2026-08-19 (fitsolver_timed_test).
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DFITSOLVER_BUILD_PYTHON=OFF -DFITSOLVER_BUILD_TESTS=OFF \
      -DFITSOLVER_BUILD_BENCHMARKS=OFF \
    && cmake --build build --config Release --target fitsolver_app --parallel

# ---------- Runtime stage ----------
FROM ubuntu:24.04 AS runtime

# libssl3t64, not libssl3: Ubuntu 24.04's 64-bit time_t transition
# renamed the OpenSSL 3 runtime package. `libssl3` does not resolve on
# noble ("Unable to locate package"), which is what was failing the
# image build on every commit.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3t64 \
    zlib1g \
    libuuid1 \
    libjsoncpp25 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Run as a non-root user
RUN useradd --create-home --shell /bin/bash appuser
USER appuser
WORKDIR /home/appuser

# Copy only the built app binary out of the builder stage
COPY --from=builder /src/build/fitsolver_app ./fitsolver_app

# fitsolver_app is a one-shot CLI (reads a JSON packing request from
# stdin, writes the JSON response to stdout, then exits) -- not a
# long-running HTTP server, so there's no port to EXPOSE here. Run it
# with `-i` so stdin is actually connected:
#
#   docker run --rm -i ghcr.io/YOUR-ORG/YOUR-REPO:latest < request.json
#
# (See the README's "Python bindings" section for the more common way
# other services are expected to call this: `import fitsolver` directly,
# or the plain C ABI -- this container is for shelling out to it
# instead, e.g. from a language with no Python/C++ interop story.)
ENTRYPOINT ["./fitsolver_app"]
