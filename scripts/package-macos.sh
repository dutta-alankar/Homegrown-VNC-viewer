#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
OUT_DIR="${2:-dist}"
ARCH="$(uname -m)"
EXE="${BUILD_DIR}/vnc-client"
BUNDLE_NAME="vnc-client-macos-${ARCH}"
APP_NAME="vnc-client.app"
APP_DIR="${OUT_DIR}/${BUNDLE_NAME}/${APP_NAME}"
MACOS_DIR="${APP_DIR}/Contents/MacOS"
RES_DIR="${APP_DIR}/Contents/Resources"

if [[ ! -x "${EXE}" ]]; then
  echo "error: executable not found at ${EXE}" >&2
  exit 1
fi

QT_PREFIX="${QT_PREFIX:-}"
if [[ -z "${QT_PREFIX}" ]]; then
  if command -v brew >/dev/null 2>&1; then
    QT_PREFIX="$(brew --prefix qt)"
  fi
fi
if [[ -z "${QT_PREFIX}" || ! -x "${QT_PREFIX}/bin/macdeployqt" ]]; then
  echo "error: macdeployqt not found. Set QT_PREFIX to your Qt installation prefix." >&2
  exit 1
fi

mkdir -p "${MACOS_DIR}" "${RES_DIR}"
cp "${EXE}" "${MACOS_DIR}/vnc-client"

cat > "${APP_DIR}/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>
  <string>vnc-client</string>
  <key>CFBundleDisplayName</key>
  <string>vnc-client</string>
  <key>CFBundleIdentifier</key>
  <string>com.dutta-alankar.vnc-client</string>
  <key>CFBundleVersion</key>
  <string>1.0</string>
  <key>CFBundleShortVersionString</key>
  <string>1.0</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleExecutable</key>
  <string>vnc-client</string>
  <key>LSMinimumSystemVersion</key>
  <string>13.0</string>
</dict>
</plist>
EOF

"${QT_PREFIX}/bin/macdeployqt" "${APP_DIR}" -verbose=1

cat > "${OUT_DIR}/${BUNDLE_NAME}/run-vnc-client.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${ROOT}/vnc-client.app/Contents/MacOS/vnc-client" "$@"
EOF
chmod +x "${OUT_DIR}/${BUNDLE_NAME}/run-vnc-client.sh"

tar -C "${OUT_DIR}" -czf "${OUT_DIR}/${BUNDLE_NAME}.tar.gz" "${BUNDLE_NAME}"
echo "Created ${OUT_DIR}/${BUNDLE_NAME}.tar.gz"