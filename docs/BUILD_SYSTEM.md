# Build system

The native extension uses AMBuild. The Makefile provides short local commands,
and the scripts prepare dependencies and run the same L4D2 x86 build.

## Layout

- `extension/`: native C++ entry point, AMBuilder, and local SourceMod SDK config.
  Wrappers live in `core/`, hooks and forwards in `hooks/`, natives in
  `natives/`, and engine ABI declarations in `sdk/engine/`.
- `scripting/include/`: public SourcePawn include.
- `scripting/`: local SourcePawn smoke plugin.
- `gamedata/`: SourceTV Manager gamedata.
- `safetyhook/`: required Git submodule, initialized by `make deps-linux`.
- `scripts/`: Linux dependency setup and build entrypoints.
- `.deps/exts-linux/`: generated HL2SDK, SourceMod, Metamod:Source, and AMBuild.
- `.build/linux-l4d2/`: generated AMBuild files and package tree.

Generated `.deps/` and `.build/` content stays out of Git. Dependency setup
uses fresh paths and stops if any target already exists; it does not update
existing checkouts.

## Container build

The one-shot Compose builder keeps its Linux x86 toolchain and upstream SDK
checkouts in a cached image. `build-deps.lock` records exact upstream commits;
the Debian base image is pinned by digest in `Dockerfile.build`. SafetyHook
comes from the repository's pinned submodule. APT package versions follow the
pinned Debian image's configured Bookworm repositories.

```bash
make build-linux-container
```

This runs `make build-linux package-linux` in a container with no network,
publishes no ports, and has no game-server connection. The source is mounted
read-only; output is written to `.build/linux-l4d2/package/`. The first run
builds the image and downloads dependencies; later runs reuse the image layers.
Refresh dependencies by updating the commit IDs in `build-deps.lock`, then
rebuilding the image. To rebuild without the cache, run
`docker compose -f compose.build.yml build --no-cache builder`.

## Build

```bash
make help
make deps-linux
make build-linux
```

The build targets Linux x86 for the `l4d2` SDK. The expected extension is:

```text
.build/linux-l4d2/package/addons/sourcemod/extensions/sourcetvmanager.ext.2.l4d2.so
```

Dependency and build paths can be overridden, for example:

```bash
make deps-linux DEPS_DIR=/tmp/sourcetvmanager-deps
make build-linux DEPS_DIR=/tmp/sourcetvmanager-deps BUILD_DIR=/tmp/sourcetvmanager-build
```

The build packages the extension, `sourcetvmanager.inc`, and
`sourcetvmanager.games.txt`. It does not install files into a running server.
