# Boost.Beast template

Provisioned from [`Qode-Fleet-Control/fleet-template-v1`](https://github.com/Qode-Fleet-Control/fleet-template-v1) — the fleet
lifecycle contract (`bin/`, `fleet.conf`, `compose.yaml`, deploy workflows) with a Boost.Beast starter laid on top.

A small asynchronous HTTP server on Boost.Beast / Boost.Asio (Boost 1.83 from Debian trixie, header-only), built with CMake. Routes: `GET /` (plain-text greeting) and `GET /health` (`{"status":"ok"}`); anything else is a 404.

## Origin

    hand-written (Boost ships no project generator) — src/main.cpp follows Beast's own example/http/server/async (a listener, one session per connection, an io_context run by a small thread pool); CMakeLists.txt uses find_package(Boost) + Boost::boost


## Run it

### On the fleet

The fleet runs it as containers (the docker runtime): `bin/run` builds the image with
`docker compose build` and then starts it with `docker compose up` in the foreground, publishing `$PORT`.

It listens on `0.0.0.0:$PORT` (default `8080`), read from the environment when the container starts,
and serves at the root of its own hostname (`https://<hash>.<FLEET_APP_DOMAIN>/`). The health check hits `/health`.

### With docker

```sh
PORT=8080 bin/run                 # build + run through compose, Ctrl-C to stop
docker compose up --build             # the same, by hand
curl localhost:8080/health
```

### Without docker

```sh
# Debian/Ubuntu: sudo apt install build-essential cmake libboost-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
PORT=8080 ./build/app
# or: FLEET_RUNTIME=process PORT=8080 bin/run
```

`fleet.conf` drives every script in `bin/`:

| step | docker runtime (fleet) | `FLEET_RUNTIME=process` |
|---|---|---|
| install | — | `(none)` |
| build | `docker compose build` | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j` |
| start | `docker compose up --remove-orphans` | `env PORT="$PORT" ./build/app` |

## Layout

- `CMakeLists.txt` — one executable target `app`, linked to `Boost::boost` (headers) and `Threads::Threads`.
- `src/main.cpp` — `handle_request()` is the router; `session` and `listener` are the Beast example's async plumbing.
- `Dockerfile` — `debian:trixie` build stage, `debian:trixie-slim` runtime (Beast is header-only, so no Boost libraries are needed at runtime), non-root user `app`.
- `compose.yaml` — service `app`, publishes `${PORT:-8080}:${PORT:-8080}`, fleet variables passed through by name.

## Deviations from stock, and why

- The Beast example serves files from a doc root and takes address/port/threads on the command line; here the address is fixed to `0.0.0.0`, the port comes from `$PORT` at runtime (default 8080), and the request handler is a tiny router instead of a file server.
- Added a `/health` route and a SIGINT/SIGTERM handler so `docker stop` ends it cleanly.

## Verified

2026-10-05, Docker 29.8 on linux/amd64:

- `verify.sh <dir> 46502` (the migrate-docker-runtime skill's end-to-end check) → `run=200 restart=200 containers_after_stop=0`.
- `migrate.py audit <dir>` → `READY`.
- `docker compose up` with `PORT=46502`: `GET /` → 200 `Hello from the Boost.Beast template!`, `GET /health` → `{"status":"ok"}`.

The no-docker path (`FLEET_RUNTIME=process`) was not run on a host toolchain; it is the same CMake build the image runs.

See `docs/fleet-lifecycle.md` for the lifecycle contract.
