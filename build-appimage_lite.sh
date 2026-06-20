#!/usr/bin/env bash
# build-appimage_lite.sh
# Builds a lightweight AppImage containing only the ProtonVPN Qt App.
# The ProtonVPN CLI must be installed separately on the host system.
# Analogous to the Flatpak build, but without the sandbox.
#
# Usage:
#   ./build-appimage_lite.sh          # build only
#   ./build-appimage_lite.sh --run    # build, then launch the resulting AppImage
#
# Requirements:
#   cmake, ninja-build, Qt 6 dev headers (qt6-base-dev / qt6-base),
#   python3, wget, binutils (ar), gcc
#
# Output:
#   dist/ProtonVPN-Qt-<version>-lite-x86_64.AppImage

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="${SCRIPT_DIR}/.appimage-build"
APPDIR="${BUILD_ROOT}/AppDir-lite"
TOOLS_DIR="${BUILD_ROOT}/tools"
OUTPUT_DIR="${SCRIPT_DIR}/dist"
APP_ID="io.github.wheat32.ProtonVPNQt"

# -- Read version --------------------------------------------------------------
VERSION=$(python3 -c "import json; print(json.load(open('${SCRIPT_DIR}/src/version.json'))['app_version'])")
OUTPUT="${OUTPUT_DIR}/ProtonVPN-Qt-${VERSION}-lite-x86_64.AppImage"

# -- Color helpers -------------------------------------------------------------
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}[build-appimage-lite]${NC} $*"; }
warn()  { echo -e "${YELLOW}[build-appimage-lite]${NC} $*"; }
die()   { echo -e "${RED}[build-appimage-lite] ERROR:${NC} $*" >&2; exit 1; }

# -- Sanity checks -------------------------------------------------------------
command -v cmake >/dev/null 2>&1 || die "cmake is not installed"
command -v python3 >/dev/null 2>&1 || die "python3 is not installed"
command -v wget   >/dev/null 2>&1 || die "wget is not installed"
command -v ar     >/dev/null 2>&1 || die "ar not found — install binutils"
command -v gcc    >/dev/null 2>&1 || die "gcc is not installed (needed for stub library)"
[[ -f "${SCRIPT_DIR}/src/CMakeLists.txt" ]] || die "Run this script from the repository root."

# -- Build the Qt app ----------------------------------------------------------
info "Building ProtonVPN Qt App (Release)..."
cmake -S "${SCRIPT_DIR}/src" -B "${BUILD_ROOT}/native-lite" \
      -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build "${BUILD_ROOT}/native-lite" --parallel

info "Installing into AppDir..."
rm -rf "${APPDIR}"
cmake --install "${BUILD_ROOT}/native-lite" --prefix "${APPDIR}/usr"

# -- Desktop integration -------------------------------------------------------
DESKTOP_DST="${APPDIR}/usr/share/applications/${APP_ID}.desktop"
ICON_DIR="${APPDIR}/usr/share/icons/hicolor/scalable/apps"

mkdir -p "$(dirname "${DESKTOP_DST}")" "${ICON_DIR}"

cp "${SCRIPT_DIR}/proton-vpn-qt-app.desktop" "${DESKTOP_DST}"
sed -i "s|^Exec=.*|Exec=proton_vpn_qt|"  "${DESKTOP_DST}"
sed -i "s|^Icon=.*|Icon=${APP_ID}|"      "${DESKTOP_DST}"

cp "${SCRIPT_DIR}/proton-vpn-sign.svg" "${ICON_DIR}/${APP_ID}.svg"
cp "${DESKTOP_DST}"            "${APPDIR}/${APP_ID}.desktop"
cp "${ICON_DIR}/${APP_ID}.svg" "${APPDIR}/${APP_ID}.svg"

# -- Custom AppRun -------------------------------------------------------------
info "Installing AppRun..."
cp "${SCRIPT_DIR}/appimage/AppRun" "${APPDIR}/AppRun"
chmod +x "${APPDIR}/AppRun"

# -- Download linuxdeploy tools (shared cache with standalone build) -----------
LINUXDEPLOY="${TOOLS_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${TOOLS_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"
APPIMAGETOOL="${TOOLS_DIR}/appimagetool-x86_64.AppImage"

mkdir -p "${TOOLS_DIR}"

declare -A TOOL_URLS=(
    ["${LINUXDEPLOY}"]="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    ["${LINUXDEPLOY_QT}"]="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    ["${APPIMAGETOOL}"]="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
)

for dest in "${!TOOL_URLS[@]}"; do
    if [[ ! -f "${dest}" ]]; then
        info "Downloading $(basename "${dest}")..."
        wget -q --show-progress -O "${dest}" "${TOOL_URLS[${dest}]}"
        chmod +x "${dest}"
    fi
done

# -- Bundle Qt shared libraries ------------------------------------------------
info "Bundling Qt libraries..."

QMAKE=$(command -v qmake6 2>/dev/null || command -v qmake 2>/dev/null || true)
[[ -n "${QMAKE}" ]] || die "qmake6/qmake not found — install Qt 6 development tools"
export QMAKE

FAKE_LIBS="${BUILD_ROOT}/fake-libs"
mkdir -p "${FAKE_LIBS}"
gcc -shared -Wl,-soname,libjxrglue.so.0 -x c /dev/null \
    -o "${FAKE_LIBS}/libjxrglue.so.0" 2>/dev/null || true

export PATH="${TOOLS_DIR}:${PATH}"

NO_STRIP=1 APPIMAGE_EXTRACT_AND_RUN=1 "${LINUXDEPLOY}" \
    --appdir       "${APPDIR}" \
    --executable   "${APPDIR}/usr/bin/proton_vpn_qt" \
    --desktop-file "${DESKTOP_DST}" \
    --icon-file    "${ICON_DIR}/${APP_ID}.svg"

LINUXDEPLOY_QT_EXTRACTED="${BUILD_ROOT}/linuxdeploy-plugin-qt-extracted"
if [[ ! -d "${LINUXDEPLOY_QT_EXTRACTED}" ]]; then
    info "Extracting linuxdeploy-plugin-qt..."
    (cd "${BUILD_ROOT}" && APPIMAGE_EXTRACT_AND_RUN=1 "${LINUXDEPLOY_QT}" \
        --appimage-extract 2>/dev/null)
    mv "${BUILD_ROOT}/squashfs-root" "${LINUXDEPLOY_QT_EXTRACTED}"
fi

info "Deploying Qt plugins..."
LD_LIBRARY_PATH="${FAKE_LIBS}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
NO_STRIP=1 \
    "${LINUXDEPLOY_QT_EXTRACTED}/usr/bin/linuxdeploy-plugin-qt" \
    --appdir "${APPDIR}"

rm -f "${APPDIR}/usr/plugins/imageformats/kimg_jxr.so"
rm -f "${APPDIR}/usr/lib/libjxrglue.so.0"

for _wayland_dir in \
    "/usr/lib/x86_64-linux-gnu/qt6/plugins/platforms" \
    "/usr/lib/qt6/plugins/platforms" \
    "/usr/lib64/qt6/plugins/platforms"; do
    if [[ -f "${_wayland_dir}/libqwayland.so" ]]; then
        cp "${_wayland_dir}/libqwayland.so" "${APPDIR}/usr/plugins/platforms/"
        info "Bundled Wayland platform plugin (${_wayland_dir})"
        break
    fi
done

cp "${SCRIPT_DIR}/appimage/AppRun" "${APPDIR}/AppRun"
chmod +x "${APPDIR}/AppRun"

# -- Create AppImage -----------------------------------------------------------
mkdir -p "${OUTPUT_DIR}"
info "Creating AppImage → ${OUTPUT}..."
APPIMAGE_EXTRACT_AND_RUN=1 "${APPIMAGETOOL}" "${APPDIR}" "${OUTPUT}"

info "Done: ${OUTPUT}"

# -- Optional launch -----------------------------------------------------------
if [[ "${1:-}" == "--run" ]]; then
    info "Launching ${OUTPUT}..."
    chmod +x "${OUTPUT}"
    "${OUTPUT}"
fi
