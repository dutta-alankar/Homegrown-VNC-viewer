#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
OUT_DIR="${2:-dist}"
ARCH="$(uname -m)"
BUNDLE_NAME="vnc-client-linux-${ARCH}"
BUNDLE_DIR="${OUT_DIR}/${BUNDLE_NAME}"

EXE="${BUILD_DIR}/vnc-client"
if [[ ! -x "${EXE}" ]]; then
  echo "error: executable not found at ${EXE}" >&2
  exit 1
fi

mkdir -p "${BUNDLE_DIR}/bin" "${BUNDLE_DIR}/lib" "${BUNDLE_DIR}/plugins"
cp "${EXE}" "${BUNDLE_DIR}/bin/vnc-client"

copy_dep_file() {
  local src="$1"
  [[ -z "${src}" ]] && return 0
  [[ ! -e "${src}" ]] && return 0
  local base
  base="$(basename "${src}")"
  [[ "${base}" == "linux-vdso.so.1" ]] && return 0
  [[ "${base}" == ld-linux* ]] && return 0
  cp -L "${src}" "${BUNDLE_DIR}/lib/${base}" 2>/dev/null || true
}

collect_from_ldd() {
  local target="$1"
  ldd "${target}" | while IFS= read -r line; do
    local dep
    dep="$(sed -nE 's/.*=> ([^ ]+) \(.*/\1/p' <<<"${line}")"
    if [[ -z "${dep}" ]]; then
      dep="$(sed -nE 's#^\s*(/[^ ]+) \(.*#\1#p' <<<"${line}")"
    fi
    if [[ -n "${dep}" ]]; then
      copy_dep_file "${dep}"
    fi
  done
}

collect_from_ldd "${BUNDLE_DIR}/bin/vnc-client"

if compgen -G "${BUILD_DIR}/external/libvncserver/libvncclient*.so*" > /dev/null; then
  cp -L ${BUILD_DIR}/external/libvncserver/libvncclient*.so* "${BUNDLE_DIR}/lib/" || true
fi

# Ensure transitive dependencies of copied libs are bundled as well.
for lib in "${BUNDLE_DIR}"/lib/*.so*; do
  [[ -e "${lib}" ]] || continue
  collect_from_ldd "${lib}"
done

QT_PLUGIN_DIR=""
if command -v qtpaths6 >/dev/null 2>&1; then
  QT_PLUGIN_DIR="$(qtpaths6 --plugin-dir)"
elif command -v qtpaths >/dev/null 2>&1; then
  QT_PLUGIN_DIR="$(qtpaths --plugin-dir)"
fi

if [[ -n "${QT_PLUGIN_DIR}" && -d "${QT_PLUGIN_DIR}" ]]; then
  for d in platforms xcbglintegrations platformthemes imageformats; do
    if [[ -d "${QT_PLUGIN_DIR}/${d}" ]]; then
      mkdir -p "${BUNDLE_DIR}/plugins/${d}"
      cp -L "${QT_PLUGIN_DIR}/${d}"/*.so* "${BUNDLE_DIR}/plugins/${d}/" 2>/dev/null || true
    fi
  done
fi

if command -v patchelf >/dev/null 2>&1; then
  patchelf --set-rpath '$ORIGIN/../lib' "${BUNDLE_DIR}/bin/vnc-client" || true
  for lib in "${BUNDLE_DIR}"/lib/*.so*; do
    [[ -e "${lib}" ]] || continue
    patchelf --set-rpath '$ORIGIN' "${lib}" || true
  done
  for plib in "${BUNDLE_DIR}"/plugins/*/*.so*; do
    [[ -e "${plib}" ]] || continue
    patchelf --set-rpath '$ORIGIN/../../lib:$ORIGIN' "${plib}" || true
  done
fi

cat > "${BUNDLE_DIR}/run-vnc-client.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${ROOT}/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${ROOT}/plugins"
exec "${ROOT}/bin/vnc-client" "$@"
EOF
chmod +x "${BUNDLE_DIR}/run-vnc-client.sh"

tar -C "${OUT_DIR}" -czf "${OUT_DIR}/${BUNDLE_NAME}.tar.gz" "${BUNDLE_NAME}"
echo "Created ${OUT_DIR}/${BUNDLE_NAME}.tar.gz"