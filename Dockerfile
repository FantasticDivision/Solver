# syntax=docker/dockerfile:1

# ---------- Build stage ----------
FROM ubuntu:24.04 AS builder

# Core build tools + libraries Drogon needs once it's added
# (jsoncpp, openssl, uuid, zlib are Drogon's usual system dependencies —
#  safe to include now so the image doesn't need changes later)
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
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --config Release --parallel

# ---------- Runtime stage ----------
FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
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

EXPOSE 8080
ENTRYPOINT ["./fitsolver_app"]
