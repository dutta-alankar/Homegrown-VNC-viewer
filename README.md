# Homegrown VNC viewer

![vnc-client logo](src/logo.png)

`vnc-client` is a C++ desktop VNC viewer built with CMake.

It provides:

- Direct VNC connections by host/IP and port.
- Saved connection profiles on the opening screen.
- Searchable profile list.
- Profile thumbnail previews captured from successful sessions.
- Shared clipboard sync between local machine and remote VNC server.
- Optional SSH gateway tunneling with in-app authentication options:
  - Password auth
  - Private key file auth

## Architecture

- GUI: Qt Widgets
- VNC protocol: LibVNCClient
- SSH tunnel transport: system `ssh` process managed by the app (`ssh -N -L ...`) with auth parameters provided in the app UI

## Dependency strategy (submodule-first)

This repository is configured to prefer dependencies from `external/` git submodules.

Configured submodules:

- `external/libvncserver` (used for LibVNCClient build/link)
- `external/qtbase` (optional local Qt source/prefix)
- `external/libssh` (reserved for native SSH backend evolution)

Initialize submodules:

```bash
git submodule update --init --recursive --progress
```

Validated build flow in this workspace:

- `external/libvncserver` was built and linked from submodule source.
- `external/qtbase` and `external/libssh` are checked out as submodules and available for local builds/evolution.
- Qt package resolution still uses your discoverable Qt installation unless you explicitly provide a built Qt prefix from `external/qtbase`.

If submodules are unavailable, CMake falls back to system-installed packages where possible.

## Prerequisites

### Linux (Debian / Ubuntu)

The default build compiles `libvncserver` from the bundled submodule, so its
development dependencies must be present instead of a pre-built `libvncclient`:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config \
  qt6-base-dev \
  libssl-dev libgnutls28-dev liblzo2-dev libjpeg-dev libsasl2-dev \
  openssh-client
```

> **Qt5 fallback:** Replace `qt6-base-dev` with `qtbase5-dev` on older
> distributions — the build system auto-detects which version is available.

> **System libvncclient alternative:** Pass `-DVNC_USE_SUBMODULE_DEPS=OFF` to
> CMake and install `libvncclient-dev` instead of the individual dev libraries
> above.

### macOS (Homebrew)

```bash
brew install cmake qt openssl@3 gnutls lzo jpeg-turbo cyrus-sasl
```

`ssh` is provided by macOS itself and does not need to be installed separately.

> **macOS 26 (Tahoe) note:** Apple removed the AGL OpenGL framework in macOS 26
> but left a non-functional stub on disk, which causes the linker to fail when
> building against Homebrew Qt.  This repository ships a project-local
> `cmake/FindWrapOpenGL.cmake` that shadows Qt's copy and omits the AGL
> reference.  Additionally, `CMakeLists.txt` patches the relevant Qt `.prl` files
> at configure time (idempotently) so the fix survives a `brew upgrade qt`.
> No manual action is required.

## Build

The configure and build commands are identical on both platforms.

### Linux — step by step

```bash
# 1. Install prerequisites (see above).
# 2. Clone and enter the repository.
git clone <repo-url> vnc_client_cpp
cd vnc_client_cpp

# 3. Populate submodules.
git submodule update --init --recursive --progress

# 4. Configure.
cmake -S . -B build -DVNC_USE_SUBMODULE_DEPS=ON

# 5. Build.
cmake --build build -j

# 6. Run.
./build/vnc-client
```

### macOS — step by step

```bash
# 1. Install prerequisites (see above).
# 2. Clone and enter the repository.
git clone <repo-url> vnc_client_cpp
cd vnc_client_cpp

# 3. Populate submodules.
git submodule update --init --recursive --progress

# 4. Configure.
#    If Homebrew Qt is not on the default CMake search path, provide the prefix:
#      cmake -S . -B build -DVNC_USE_SUBMODULE_DEPS=ON \
#            -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
cmake -S . -B build -DVNC_USE_SUBMODULE_DEPS=ON

# 5. Build.
cmake --build build -j

# 6. Run.
./build/vnc-client
```

## Prebuilt binaries

Every push to `main` triggers a GitHub Actions release workflow that builds and
packages self-contained binaries for Linux and macOS.

Download from the rolling release assets:

- `https://github.com/dutta-alankar/vnc_client_cpp/releases/tag/rolling-main`

Available assets:

- `vnc-client-linux-<arch>.tar.gz`
- `vnc-client-macos-<arch>.tar.gz`

Run Linux bundle:

```bash
tar -xzf vnc-client-linux-*.tar.gz
cd vnc-client-linux-*
./run-vnc-client.sh
```

Run macOS bundle:

```bash
tar -xzf vnc-client-macos-*.tar.gz
cd vnc-client-macos-*
./run-vnc-client.sh
```

## Usage

1. Click `Add` on the opening screen.
2. Fill VNC server host/IP and port.
3. Optional: enable `Connect through SSH gateway` and provide:
   - gateway host/IP
   - gateway SSH port
   - SSH user
   - SSH auth mode (`Password` or `Private key file`)
   - remote VNC host/IP and port (behind gateway)
4. Save profile.
5. Use the search box to filter profiles.
6. Select a profile and click `Connect`.

Thumbnail previews are captured from the framebuffer received after 30 second of a successfully connected session and persisted with the profile.

## Data storage

Connection profiles (including thumbnails and tunnel settings) are stored in your OS app-data location as `connections.json`.
