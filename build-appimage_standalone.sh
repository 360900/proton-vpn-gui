#!/usr/bin/env bash
# build-appimage_standalone.sh
# Builds a self-contained AppImage bundling the ProtonVPN Qt App and CLI.
#
# Uses python-build-standalone to embed a portable Python interpreter matching
# the CI system's Python version, ensuring native extensions (e.g. gi._gi) are
# ABI-compatible.  Runs correctly on Arch, Ubuntu, Fedora, or any other distro.
#
# The CLI version bundled is read from src/version.json (cli_version_tested_max).
# Its packages are downloaded from the ProtonVPN public apt repository; PyPI
# supplies the remaining Python dependencies in a venv.
#
# Build-order rationale:
#   1. ProtonVPN .deb packages + gi module are placed in AppDir first so that
#      linuxdeploy can scan their ELF deps and bundle libgirepository, GLib, etc.
#   2. python-build-standalone is copied AFTER linuxdeploy.  Its own libssl.so.3
#      lives under python/lib/ and is found by the Python binary via RPATH.
#      If it were present during linuxdeploy, linuxdeploy might copy a different
#      libssl version to usr/lib/, conflicting with the Qt SSL backend.
#
# Usage:
#   ./build-appimage.sh          # build only
#   ./build-appimage.sh --run    # build, then launch the resulting AppImage
#
# Requirements (install with your package manager before running):
#   cmake, ninja-build, Qt 6 dev headers (qt6-base-dev / qt6-base),
#   python3, python3-gi, python3-gi-cairo,
#   wget, binutils (ar), gcc
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
OUTPUT="${OUTPUT_DIR}/ProtonVPN-Qt-${VERSION}-standalone-x86_64.AppImage"

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

# -- Bundle ProtonVPN CLI packages ---------------------------------------------
# Download from the ProtonVPN public apt repository and extract (no dpkg needed).
# The system still needs proton-vpn-daemon + NetworkManager for VPN connections.

CLI_VERSION=$(python3 -c "
import json
print(json.load(open('${SCRIPT_DIR}/src/version.json'))['cli_version_tested_max'])
")
info "Bundling ProtonVPN CLI v${CLI_VERSION}..."

CLI_DIR="${APPDIR}/usr/share/protonvpn"
DEB_CACHE="${BUILD_ROOT}/protonvpn-debs"
DEB_EXTRACT="${BUILD_ROOT}/protonvpn-extracted"
mkdir -p "${CLI_DIR}" "${DEB_CACHE}" "${DEB_EXTRACT}"

DEB_URLS="${BUILD_ROOT}/protonvpn-deb-urls.txt"
info "Resolving package URLs from ProtonVPN apt repo..."
python3 "${SCRIPT_DIR}/appimage/fetch-cli-debs.py" "${CLI_VERSION}" > "${DEB_URLS}"

while IFS= read -r url; do
    dest="${DEB_CACHE}/$(basename "${url}")"
    if [[ ! -f "${dest}" ]]; then
        info "Downloading $(basename "${url}")..."
        wget -q --show-progress -O "${dest}" "${url}"
    fi
done < "${DEB_URLS}"

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

# Extract in two passes: api-core last so its session helpers take precedence.
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

if [[ -d "${DEB_EXTRACT}/usr/lib/python3/dist-packages" ]]; then
    mkdir -p "${CLI_DIR}/dist-packages"
    cp -r "${DEB_EXTRACT}/usr/lib/python3/dist-packages/." "${CLI_DIR}/dist-packages/"
fi

# -- Bundle PyGObject (gi) from system Python ----------------------------------
# The ProtonVPN CLI imports 'gi' at startup for its NetworkManager backend.
# gi cannot be pip-installed portably; we copy it from the system's Python 3
# package (python3-gi).  It must be in AppDir NOW so that linuxdeploy picks up
# its native dependency: libgirepository-1.0.so.0.
for gi_pkg in gi cairo; do
    src="/usr/lib/python3/dist-packages/${gi_pkg}"
    if [[ -d "${src}" ]]; then
        info "Copying system Python package: ${gi_pkg}"
        cp -r "${src}" "${CLI_DIR}/dist-packages/"
    else
        warn "System Python package not found: ${src} (install python3-gi / python3-gi-cairo)"
    fi
done

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

# -- Bundle Qt + gi shared libraries via linuxdeploy --------------------------
# python-build-standalone is NOT yet in AppDir.  That is deliberate: its own
# libssl.so.3 (newer than Ubuntu's) must not overwrite the libssl that Qt was
# compiled against.  linuxdeploy will bundle Ubuntu's libssl.so.3 here; the
# bundled Python later uses its own copy found via RPATH.
info "Bundling Qt and gi libraries..."

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

# linuxdeploy-plugin-qt skips the Wayland platform plugin on headless CI runners.
# EXTRA_PLATFORM_PLUGINS uses a naming convention that doesn't match Ubuntu's
# libqwayland.so filename, so we copy it manually instead along with any of its
# shared library dependencies (e.g. libQt6WaylandClient) not already bundled.
for _wayland_dir in \
    "/usr/lib/x86_64-linux-gnu/qt6/plugins/platforms" \
    "/usr/lib/qt6/plugins/platforms" \
    "/usr/lib64/qt6/plugins/platforms"; do
    if [[ -f "${_wayland_dir}/libqwayland.so" ]]; then
        cp "${_wayland_dir}/libqwayland.so" "${APPDIR}/usr/plugins/platforms/"
        info "Bundled Wayland platform plugin (${_wayland_dir})"
        # Copy any shared library dependencies of the Wayland plugin that are
        # not already present in the AppDir.
        while IFS= read -r _dep; do
            _dep_name=$(basename "${_dep}")
            if [[ -f "${_dep}" && ! -f "${APPDIR}/usr/lib/${_dep_name}" ]]; then
                cp "${_dep}" "${APPDIR}/usr/lib/"
                info "  Bundled Wayland dep: ${_dep_name}"
            fi
        done < <(ldd "${_wayland_dir}/libqwayland.so" 2>/dev/null \
                 | awk '/=>/ {print $3}' \
                 | grep -v "not found")
        break
    fi
done

# -- Explicitly bundle Qt SSL libraries ----------------------------------------
# Qt loads libssl at runtime through its OpenSSL backend plugin
# (libqopensslbackend.so), not as a link-time ELF dependency, so linuxdeploy
# does not pick it up automatically.  We locate and copy both libraries here.
# python-build-standalone's Python finds its own libssl via DT_RPATH
# ($ORIGIN/../lib), which takes precedence over LD_LIBRARY_PATH, so there is
# no conflict between the two copies.
info "Bundling Qt SSL libraries..."
for lib in libssl.so.3 libcrypto.so.3; do
    src=$(ldconfig -p 2>/dev/null | grep " ${lib} " | awk '{print $NF}' | head -1 || true)
    if [[ -n "${src}" && -f "${src}" ]]; then
        cp -n "${src}" "${APPDIR}/usr/lib/"
        info "  Bundled: ${lib} (${src})"
    else
        warn "  ${lib} not found via ldconfig — Qt TLS will not work"
    fi
done

# -- Portable Python (python-build-standalone) ---------------------------------
# Copied AFTER linuxdeploy so its libssl.so.3 stays in python/lib/ and does not
# replace the Ubuntu libssl.so.3 that linuxdeploy placed in usr/lib/ for Qt.
# The Python binary finds its own libs via RPATH ($ORIGIN/../lib).
PYTHON_STANDALONE_DIR="${CLI_DIR}/python"
PYTHON_CACHE="${BUILD_ROOT}/python-standalone"

# Match the python-build-standalone version to the CI system's Python.
# python3-gi (and other native extensions) are compiled for the system Python;
# the bundled interpreter must be the same major.minor so extensions load correctly.
SYSTEM_PY_VER=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
info "System Python: ${SYSTEM_PY_VER} — fetching matching python-build-standalone"

if [[ ! -d "${PYTHON_CACHE}/python" ]]; then
    info "Resolving latest python-build-standalone release..."
    PBS_URL=$(python3 -c "
import urllib.request, json, sys
py_ver = f'{sys.version_info.major}.{sys.version_info.minor}'
req = urllib.request.Request(
    'https://api.github.com/repos/indygreg/python-build-standalone/releases/latest',
    headers={'User-Agent': 'build-appimage'})
with urllib.request.urlopen(req, timeout=30) as r:
    data = json.loads(r.read())
for asset in data['assets']:
    n = asset['name']
    if (f'cpython-{py_ver}' in n and 'linux-gnu-install_only' in n and
            ('x86_64-unknown' in n or 'x86_64_v1-unknown' in n) and
            n.endswith('.tar.gz')):
        print(asset['browser_download_url'])
        break
")
    [[ -n "${PBS_URL}" ]] || die "Could not find python-build-standalone ${SYSTEM_PY_VER} asset"
    info "Downloading portable Python: $(basename "${PBS_URL}")..."
    mkdir -p "${PYTHON_CACHE}"
    wget -q --show-progress -O "${PYTHON_CACHE}/python.tar.gz" "${PBS_URL}"
    tar -xzf "${PYTHON_CACHE}/python.tar.gz" -C "${PYTHON_CACHE}"
fi

info "Installing portable Python into AppDir..."
mkdir -p "${PYTHON_STANDALONE_DIR}"
cp -r "${PYTHON_CACHE}/python/." "${PYTHON_STANDALONE_DIR}/"
BUNDLED_PYTHON="${PYTHON_STANDALONE_DIR}/bin/python${SYSTEM_PY_VER}"
[[ -x "${BUNDLED_PYTHON}" ]] || die "Bundled Python not found at ${BUNDLED_PYTHON}"

PY_VER=$("${BUNDLED_PYTHON}" -c \
    "import sys; print(f'python{sys.version_info.major}.{sys.version_info.minor}')")
info "Bundled $("${BUNDLED_PYTHON}" --version)"

# -- Create venv with PyPI dependencies ----------------------------------------
VENV="${CLI_DIR}/venv"
"${BUNDLED_PYTHON}" -m venv "${VENV}"
"${VENV}/bin/pip" install --quiet --upgrade pip
"${VENV}/bin/pip" install --quiet \
    click dbus-fast tabulate packaging \
    aiohttp bcrypt python-gnupg pyOpenSSL requests "importlib-metadata" \
    keyring secretstorage cryptography distro fido2 Jinja2 PyNaCl sentry-sdk

VENV_PROTONVPN="${VENV}/bin/protonvpn"
if [[ -f "${VENV_PROTONVPN}" ]]; then
    mv "${VENV_PROTONVPN}" "${VENV_PROTONVPN}.orig"
fi

# -- Create protonvpn launcher -------------------------------------------------
# PYTHONHOME: bundled Python finds its own stdlib.
# PYTHONPATH: ProtonVPN .deb packages + PyPI venv site-packages + system
#             Python dist-packages (for gi, which was bundled from the system).
# The bundled Python uses its own RPATH-resolved OpenSSL; no LD_LIBRARY_PATH
# manipulation is needed.
cat > "${CLI_DIR}/protonvpn" << EOF
#!/bin/bash
PROTON_DIR="\${APPDIR}/usr/share/protonvpn"
export PYTHONHOME="\${PROTON_DIR}/python"
VENV_SITE="\${PROTON_DIR}/venv/lib/${PY_VER}/site-packages"
PROTON_PKG="\${PROTON_DIR}/dist-packages"
export PYTHONPATH="\${PROTON_PKG}:\${VENV_SITE}"
exec "\${PROTON_DIR}/python/bin/python${SYSTEM_PY_VER}" -c "from proton.vpn.cli import main; main()" "\$@"
EOF
chmod +x "${CLI_DIR}/protonvpn"

info "ProtonVPN CLI v${CLI_VERSION} bundled (${CLI_DIR})"

# linuxdeploy may have overwritten AppRun — restore ours.
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
