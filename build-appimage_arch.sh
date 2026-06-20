#!/usr/bin/env bash
# build-appimage_arch.sh
# Builds a self-contained AppImage on Arch Linux (or any modern rolling-release
# distro with up-to-date Python, OpenSSL, and Qt dev headers).
#
# The CLI version bundled is read from src/version.json (cli_version_tested_max).
# Its packages are downloaded from the ProtonVPN public apt repository; PyPI
# supplies the remaining Python dependencies in a venv.
#
# Usage:
#   ./build-appimage_arch.sh          # build only
#   ./build-appimage_arch.sh --run    # build, then launch the resulting AppImage
#
# Requirements (install with your package manager before running):
#   cmake, ninja-build, Qt 6 dev headers (qt6-base-dev / qt6-base),
#   python3, python3-venv, wget, binutils (ar), gcc
#
# linuxdeploy and appimagetool are downloaded automatically on first run
# and cached in .appimage-build/tools/.
#
# Output:
#   dist/ProtonVPN-Qt-<version>-x86_64.AppImage

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="${SCRIPT_DIR}/.appimage-build"
APPDIR="${BUILD_ROOT}/AppDir"
TOOLS_DIR="${BUILD_ROOT}/tools"
OUTPUT_DIR="${SCRIPT_DIR}/dist"
APP_ID="io.github.wheat32.ProtonVPNQt"

# -- Read version --------------------------------------------------------------
VERSION=$(python3 -c "import json; print(json.load(open('${SCRIPT_DIR}/src/version.json'))['app_version'])")
OUTPUT="${OUTPUT_DIR}/ProtonVPN-Qt-${VERSION}-x86_64.AppImage"

# -- Color helpers -------------------------------------------------------------
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}[build-appimage]${NC} $*"; }
warn()  { echo -e "${YELLOW}[build-appimage]${NC} $*"; }
die()   { echo -e "${RED}[build-appimage] ERROR:${NC} $*" >&2; exit 1; }

# -- Sanity checks -------------------------------------------------------------
command -v cmake   >/dev/null 2>&1 || die "cmake is not installed"
command -v python3 >/dev/null 2>&1 || die "python3 is not installed"
command -v wget    >/dev/null 2>&1 || die "wget is not installed"
command -v ar      >/dev/null 2>&1 || die "ar not found — install binutils"
command -v gcc     >/dev/null 2>&1 || die "gcc is not installed (needed for stub library)"
[[ -f "${SCRIPT_DIR}/src/CMakeLists.txt" ]] || die "Run this script from the repository root."

# -- Build the Qt app ----------------------------------------------------------
info "Building ProtonVPN Qt App (Release)..."
cmake -S "${SCRIPT_DIR}/src" -B "${BUILD_ROOT}/native" \
      -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build "${BUILD_ROOT}/native" --parallel

info "Installing into AppDir..."
rm -rf "${APPDIR}"
cmake --install "${BUILD_ROOT}/native" --prefix "${APPDIR}/usr"

# -- Bundle ProtonVPN CLI ------------------------------------------------------
# The CLI is NOT on PyPI, but its .deb packages are publicly available from the
# ProtonVPN apt repository.  We download the exact version from version.json,
# extract the .deb files (no dpkg required — just ar + tar), drop the Python
# packages into AppDir, and install the remaining deps from PyPI in a venv.
#
# The system still needs proton-vpn-daemon + NetworkManager for actual VPN
# connections (that daemon manages kernel-level networking and cannot be bundled).
# Login, status, and server listing work without it.

CLI_VERSION=$(python3 -c "
import json
print(json.load(open('${SCRIPT_DIR}/src/version.json'))['cli_version_tested_max'])
")
info "Bundling ProtonVPN CLI v${CLI_VERSION}..."

CLI_DIR="${APPDIR}/usr/share/protonvpn"
DEB_CACHE="${BUILD_ROOT}/protonvpn-debs"
DEB_EXTRACT="${BUILD_ROOT}/protonvpn-extracted"
mkdir -p "${CLI_DIR}" "${DEB_CACHE}" "${DEB_EXTRACT}"

# Query the ProtonVPN apt repo to get the exact .deb URLs for this CLI version
# and all its ProtonVPN Python dependencies.
DEB_URLS="${BUILD_ROOT}/protonvpn-deb-urls.txt"
info "Resolving package URLs from ProtonVPN apt repo..."
python3 "${SCRIPT_DIR}/appimage/fetch-cli-debs.py" "${CLI_VERSION}" > "${DEB_URLS}"

# Download each .deb (cached — skip if already present)
while IFS= read -r url; do
    dest="${DEB_CACHE}/$(basename "${url}")"
    if [[ ! -f "${dest}" ]]; then
        info "Downloading $(basename "${url}")..."
        wget -q --show-progress -O "${dest}" "${url}"
    fi
done < "${DEB_URLS}"

# Extract .deb files using ar + tar (works on any distro without dpkg).
# A .deb is an ar archive containing debian-binary, control.tar.*, data.tar.*.
extract_deb() {
    local deb="$1" dest="$2"
    local work
    work=$(mktemp -d)
    pushd "${work}" > /dev/null
    ar x "${deb}"
    for data_tar in data.tar.*; do
        tar -xf "${data_tar}" -C "${dest}"
    done
    popd > /dev/null
    rm -rf "${work}"
}

# Extract packages in two passes:
# 1. All packages except proton-vpn-api-core first.
# 2. api-core last — it also ships files into proton/vpn/session/ that must
#    take precedence over the separate proton-vpn-session package's versions
#    (api-core bundles newer session helpers its own code depends on).
while IFS= read -r url; do
    name=$(basename "${url}")
    if [[ "${name}" != python3-proton-vpn-api-core* ]]; then
        extract_deb "${DEB_CACHE}/${name}" "${DEB_EXTRACT}"
    fi
done < "${DEB_URLS}"
while IFS= read -r url; do
    name=$(basename "${url}")
    if [[ "${name}" == python3-proton-vpn-api-core* ]]; then
        extract_deb "${DEB_CACHE}/${name}" "${DEB_EXTRACT}"
    fi
done < "${DEB_URLS}"

# Copy extracted Python packages into the CLI bundle directory
if [[ -d "${DEB_EXTRACT}/usr/lib/python3/dist-packages" ]]; then
    mkdir -p "${CLI_DIR}/dist-packages"
    cp -r "${DEB_EXTRACT}/usr/lib/python3/dist-packages/." "${CLI_DIR}/dist-packages/"
fi

# Create a venv for Python dependencies that ARE on PyPI
VENV="${CLI_DIR}/venv"
python3 -m venv "${VENV}"
"${VENV}/bin/pip" install --quiet --upgrade pip
"${VENV}/bin/pip" install --quiet \
    click dbus-fast tabulate packaging \
    aiohttp bcrypt python-gnupg pyOpenSSL requests "importlib-metadata" \
    keyring secretstorage cryptography distro fido2 Jinja2 PyNaCl sentry-sdk

PY_VER=$("${VENV}/bin/python3" -c \
    "import sys; print(f'python{sys.version_info.major}.{sys.version_info.minor}')")

# Patch the venv entry-point shebang (same issue as before: absolute build-time path)
VENV_PROTONVPN="${VENV}/bin/protonvpn"
if [[ -f "${VENV_PROTONVPN}" ]]; then
    mv "${VENV_PROTONVPN}" "${VENV_PROTONVPN}.orig"
fi

# Create the protonvpn launcher script.
# AppRun puts ${CLI_DIR} on PATH so the Qt app finds this via QProcess.
# PYTHONPATH is set to include both the extracted ProtonVPN .deb packages and
# the PyPI venv site-packages.  System packages (python3-gi, pycairo, etc.)
# are found automatically via Python's standard search path.
#
# LD_LIBRARY_PATH is cleared before invoking python3.  AppRun sets it to
# prefer the bundled Qt libraries, but the bundled libcrypto.so.3 may differ
# from the version the system Python's _ssl.cpython-*.so was compiled against,
# causing an OpenSSL symbol-version mismatch.  Python is a system binary that
# locates its own dependencies via RPATH and system library paths, so it does
# not need LD_LIBRARY_PATH at all.
cat > "${CLI_DIR}/protonvpn" << EOF
#!/bin/bash
PROTON_DIR="\${APPDIR}/usr/share/protonvpn"
VENV_SITE="\${PROTON_DIR}/venv/lib/${PY_VER}/site-packages"
PROTON_PKG="\${PROTON_DIR}/dist-packages"
export PYTHONPATH="\${PROTON_PKG}:\${VENV_SITE}\${PYTHONPATH:+:\${PYTHONPATH}}"
exec env LD_LIBRARY_PATH="" python3 -c "from proton.vpn.cli import main; main()" "\$@"
EOF
chmod +x "${CLI_DIR}/protonvpn"

info "ProtonVPN CLI v${CLI_VERSION} bundled (${CLI_DIR})"

# -- Desktop integration -------------------------------------------------------
DESKTOP_DST="${APPDIR}/usr/share/applications/${APP_ID}.desktop"
ICON_DIR="${APPDIR}/usr/share/icons/hicolor/scalable/apps"

mkdir -p "$(dirname "${DESKTOP_DST}")" "${ICON_DIR}"

# Patch the desktop file: Exec must be the bare binary name; Icon is the app-id.
cp "${SCRIPT_DIR}/proton-vpn-qt-app.desktop" "${DESKTOP_DST}"
sed -i "s|^Exec=.*|Exec=proton_vpn_qt|"  "${DESKTOP_DST}"
sed -i "s|^Icon=.*|Icon=${APP_ID}|"      "${DESKTOP_DST}"

# Icon
cp "${SCRIPT_DIR}/proton-vpn-sign.svg" "${ICON_DIR}/${APP_ID}.svg"

# appimagetool also looks for the desktop file and icon at the AppDir root.
cp "${DESKTOP_DST}"          "${APPDIR}/${APP_ID}.desktop"
cp "${ICON_DIR}/${APP_ID}.svg" "${APPDIR}/${APP_ID}.svg"

# -- Custom AppRun -------------------------------------------------------------
info "Installing AppRun..."
cp "${SCRIPT_DIR}/appimage/AppRun" "${APPDIR}/AppRun"
chmod +x "${APPDIR}/AppRun"

# -- Download linuxdeploy tools (cached after first run) -----------------------
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

# -- Bundle Qt libraries -------------------------------------------------------
info "Bundling Qt libraries..."

QMAKE=$(command -v qmake6 2>/dev/null || command -v qmake 2>/dev/null || true)
[[ -n "${QMAKE}" ]] || die "qmake6/qmake not found — install Qt 6 development tools"
export QMAKE

# linuxdeploy-plugin-qt may try to deploy kimg_jxr.so (KDE JPEG XR plugin),
# which requires libjxrglue — an optional library not installed on all systems.
# We create a minimal stub so the dep check passes, then remove both the stub
# and the plugin from AppDir afterward (we don't need JPEG XR support).
FAKE_LIBS="${BUILD_ROOT}/fake-libs"
mkdir -p "${FAKE_LIBS}"
gcc -shared -Wl,-soname,libjxrglue.so.0 -x c /dev/null \
    -o "${FAKE_LIBS}/libjxrglue.so.0" 2>/dev/null || true

# linuxdeploy finds its Qt plugin by searching PATH for
# linuxdeploy-plugin-qt-x86_64.AppImage.
export PATH="${TOOLS_DIR}:${PATH}"

# Step 1 -- deploy ELF shared-library dependencies via linuxdeploy.
# APPIMAGE_EXTRACT_AND_RUN=1 avoids the need for FUSE (required in CI).
# NO_STRIP=1 prevents linuxdeploy's bundled strip from choking on modern
# ELF libraries that use .relr.dyn (requires binutils 2.38+).
NO_STRIP=1 APPIMAGE_EXTRACT_AND_RUN=1 "${LINUXDEPLOY}" \
    --appdir       "${APPDIR}" \
    --executable   "${APPDIR}/usr/bin/proton_vpn_qt" \
    --desktop-file "${DESKTOP_DST}" \
    --icon-file    "${ICON_DIR}/${APP_ID}.svg"

# Step 2 -- deploy Qt plugins (platform, imageformats, etc.).
# The linuxdeploy-plugin-qt AppImage wrapper resets $QMAKE before calling the
# plugin binary, so we extract it and run the binary directly instead.
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

# Remove the JXR plugin and stub dep — the stub is non-functional and
# kimg_jxr.so would fail to load without the real library.
rm -f "${APPDIR}/usr/plugins/imageformats/kimg_jxr.so"
rm -f "${APPDIR}/usr/lib/libjxrglue.so.0"

# linuxdeploy may generate a default AppRun — restore ours.
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
