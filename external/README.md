# Third-party dependencies

This project is configured to prefer dependencies from git submodules under `external/`.

Expected submodules:

- `external/libvncserver` (provides LibVNCClient)
- `external/qtbase` (optional local Qt source/prefix)
- `external/libssh` (reserved for native SSH tunnel backend)

Initialize all submodules:

```bash
git submodule update --init --recursive
```

Notes:

- `libvncserver` is consumed directly in CMake when present.
- Qt can still be resolved from a system install if `external/qtbase` is not built/provisioned as a prefix.
- Current implementation uses the system `ssh` binary for tunnel transport with in-app auth options.
