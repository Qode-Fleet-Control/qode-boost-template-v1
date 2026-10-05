# Built by .github/workflows/deploy.yml and pushed to Artifact Registry.
#
# Boost.Beast on Debian trixie (Boost 1.83). Multi-stage: Beast/Asio are
# header-only, so the runtime stage needs only libstdc++ — it is a slim
# Debian image with a non-root user. The port is read from $PORT when the
# container starts, not at build time.
FROM debian:trixie AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential cmake ninja-build libboost-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY src ./src
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build \
 && install -D build/app /out/app

FROM debian:trixie-slim AS runtime
RUN useradd -r -u 10001 app
WORKDIR /app
ARG BUILD_ID=""
ENV PORT=8080 BUILD_ID=$BUILD_ID
COPY --from=build /out/app /app/app
EXPOSE 8080
USER app
ENTRYPOINT ["/app/app"]
