# Reproducible release build: static clang/LLVM, libstdc++, zlib and zstd; glibc 2.35 (Ubuntu 22.04).
FROM ubuntu:22.04 AS build
ARG LLVM_VERSION=20
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates wget cmake make g++ \
    && wget -q --tries=10 --waitretry=5 -O /etc/apt/trusted.gpg.d/apt.llvm.org.asc https://apt.llvm.org/llvm-snapshot.gpg.key \
    && echo "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-${LLVM_VERSION} main" > /etc/apt/sources.list.d/llvm.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends clang-${LLVM_VERSION} libclang-${LLVM_VERSION}-dev llvm-${LLVM_VERSION}-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DClang_DIR=/usr/lib/llvm-${LLVM_VERSION}/lib/cmake/clang \
    && cmake --build build -j"$(nproc)" \
    && ctest --test-dir build --output-on-failure \
    && strip build/cpp-bundle build/cpp-minify

FROM scratch AS dist
COPY --from=build /src/build/cpp-bundle /src/build/cpp-minify /
