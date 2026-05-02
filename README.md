# vnc-client

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
git submodule update --init --recursive
```

Validated build flow in this workspace:

- `external/libvncserver` was built and linked from submodule source.
- `external/qtbase` and `external/libssh` are checked out as submodules and available for local builds/evolution.
- Qt package resolution still uses your discoverable Qt installation unless you explicitly provide a built Qt prefix from `external/qtbase`.

If submodules are unavailable, CMake falls back to system-installed packages where possible.

## Build prerequisites

### Option A: build with system packages (quickest)

Debian/Ubuntu example:

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config qt6-base-dev libvncclient-dev openssh-client
```

### Option B: use checked-out submodules

1. Initialize submodules.
2. Build/provide Qt in a way discoverable by CMake (for example via `CMAKE_PREFIX_PATH`).
3. CMake will automatically use `external/libvncserver` when present.

## Configure and compile

```bash
cmake -S . -B build -DVNC_USE_SUBMODULE_DEPS=ON
cmake --build build -j
```

## Run

```bash
./build/vnc-client
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

Thumbnail previews are captured from the first received framebuffer of each connected session and persisted with the profile.

## Data storage

Connection profiles (including thumbnails and tunnel settings) are stored in your OS app-data location as `connections.json`.
