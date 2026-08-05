#!/usr/bin/env bash
# build-flatpak.sh
# Sets up the Flatpak builder environment and builds the Flatpak bundle.
#
# Usage:
#   ./build-flatpak.sh              # build the pinned release tag (Flathub mode)
#   ./build-flatpak.sh --local      # build the current checkout (for local testing)
#   ./build-flatpak.sh --install    # build and install for the current user
#   Flags can be combined: ./build-flatpak.sh --local --install
#
# Requirements:
#   flatpak, plus either flatpak-builder or the org.flatpak.Builder Flatpak.
#
# The output bundle is written to:
#   dist/io.github._360900.Vela.flatpak

set -euo pipefail

MANIFEST="io.github._360900.Vela.yml"
APP_ID="io.github._360900.Vela"
BUILD_DIR=".flatpak-build"
REPO_DIR=".flatpak-repo"
BUNDLE="dist/${APP_ID}.flatpak"
RUNTIME="org.kde.Platform"
RUNTIME_VERSION=""   # resolved dynamically from Flathub at runtime
SOURCE_URL="https://github.com/360900/vela.git"

# ── Resolve latest KDE runtime version from Flathub ──────────────────────────
resolve_runtime_version() {
    info "Querying Flathub for the latest ${RUNTIME} version..."
    local latest
    latest=$(flatpak remote-ls --user --runtime --columns=ref flathub 2>/dev/null \
        | grep "^runtime/${RUNTIME}/x86_64/" \
        | sed "s|runtime/${RUNTIME}/x86_64/||" \
        | grep -E '^[0-9]+\.[0-9]+$' \
        | sort -V \
        | tail -1)

    if [[ -z "$latest" ]]; then
        die "Could not resolve the latest ${RUNTIME} version from Flathub. Are you online?"
    fi

    RUNTIME_VERSION="$latest"
    info "Using KDE runtime version: ${RUNTIME_VERSION}"
}

# ── Color helpers ────────────────────────────────────────────────────────────
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()    { echo -e "${GREEN}[build-flatpak]${NC} $*"; }
warn()    { echo -e "${YELLOW}[build-flatpak]${NC} $*"; }
die()     { echo -e "${RED}[build-flatpak] ERROR:${NC} $*" >&2; exit 1; }

# ── Arguments ─────────────────────────────────────────────────────────────────
LOCAL_BUILD=false
INSTALL_AFTER=false
for arg in "$@"; do
    case "$arg" in
        --local)   LOCAL_BUILD=true ;;
        --install) INSTALL_AFTER=true ;;
        -h|--help)
            grep '^#' "$0" | head -12
            exit 0
            ;;
        *) die "Unknown argument: $arg (expected --local and/or --install)" ;;
    esac
done

# ── Sanity checks ─────────────────────────────────────────────────────────────
command -v flatpak >/dev/null 2>&1 || die "flatpak is not installed. Install it with: sudo apt install flatpak"

# Prefer a host flatpak-builder; fall back to the org.flatpak.Builder Flatpak.
if command -v flatpak-builder >/dev/null 2>&1; then
    BUILDER=(flatpak-builder)
elif flatpak info org.flatpak.Builder >/dev/null 2>&1; then
    BUILDER=(flatpak run --filesystem="$PWD" org.flatpak.Builder)
else
    die "flatpak-builder is not installed. Install it with: sudo apt install flatpak-builder
       or: flatpak install --user flathub org.flatpak.Builder"
fi

[[ -f "$MANIFEST" ]] || die "Manifest '$MANIFEST' not found. Run this script from the repository root."

# ── Select what to build ──────────────────────────────────────────────────────
BUILD_MANIFEST="$MANIFEST"
LOCAL_MANIFEST=""

if [[ "$LOCAL_BUILD" == true ]]; then
    # Build the working tree, not the pinned release tag: copy the manifest and
    # swap the LAST module's (the GUI's) git source for a dir source. The
    # tracked manifest is left untouched.
    # Kept inside the repo so the org.flatpak.Builder sandbox can read it;
    # removed automatically on exit.
    LOCAL_MANIFEST="$(mktemp ./.${APP_ID}.local.XXXXXX.yml)"
    trap 'rm -f "$LOCAL_MANIFEST"' EXIT
    # Truncate at the last "    sources:" block (the GUI module is last in the
    # file); everything before it is kept as-is.
    awk 'BEGIN { last = 0 }
         /^    sources:/ { last = NR }
         { lines[NR] = $0 }
         END { for (i = 1; i <= length(lines); i++) if (i < last) print lines[i] }' \
        "$MANIFEST" > "$LOCAL_MANIFEST"
    printf '    sources:\n      - type: dir\n        path: %s\n        skip:\n          - .git\n          - .flatpak-build\n          - .flatpak-repo\n          - dist\n' "$PWD" >> "$LOCAL_MANIFEST"
    BUILD_MANIFEST="$LOCAL_MANIFEST"
    info "Local test build: compiling the current checkout instead of the pinned tag."
else
    # ── Preflight: verify the pinned source tag exists upstream ──────────────
    TAG=$(sed -n "s/^[[:space:]]*tag:[[:space:]]*//p" "$MANIFEST" | head -1 | tr -d "'\"")
    [[ -n "$TAG" ]] || die "No git 'tag:' entry found in the sources of $MANIFEST."

    if ! git ls-remote --exit-code --tags "$SOURCE_URL" "refs/tags/$TAG" >/dev/null 2>&1; then
        die "Source tag '$TAG' not found on $SOURCE_URL.
       Tag and push the release first:  git tag $TAG && git push origin $TAG
       Or build the current checkout instead:  $0 --local"
    fi
    info "Source tag verified upstream: $TAG"
fi

# ── Ensure the KDE runtime + SDK are available ───────────────────────────
info "Checking for Flatpak remote 'flathub'..."
if ! flatpak remote-list --user | grep -q flathub; then
    info "Adding flathub remote (user)..."
    flatpak remote-add --user --if-not-exists flathub \
        https://dl.flathub.org/repo/flathub.flatpakrepo
fi

resolve_runtime_version

# ── Patch manifest with resolved runtime version ──────────────────────────────
info "Patching manifest: runtime-version -> ${RUNTIME_VERSION}..."
sed -i "s/^runtime-version:.*/runtime-version: '${RUNTIME_VERSION}'/" "$BUILD_MANIFEST"

for component in "${RUNTIME}/${RUNTIME_VERSION}" "${RUNTIME%Platform}Sdk/${RUNTIME_VERSION}"; do
    ref="${component%%/*}/x86_64/${component#*/}"
    if ! flatpak info --user "$ref" >/dev/null 2>&1; then
        info "Installing $ref from flathub..."
        flatpak install --user --noninteractive flathub "$ref" || \
            warn "Could not auto-install $ref. You may need to run: flatpak install flathub $ref"
    fi
done

# ── Build ─────────────────────────────────────────────────────────────────────
info "Building Flatpak (this may take a while on first run)..."
mkdir -p dist

"${BUILDER[@]}" \
    --force-clean \
    --repo="$REPO_DIR" \
    "$BUILD_DIR" \
    "$BUILD_MANIFEST"

# ── Export bundle ─────────────────────────────────────────────────────────────
info "Exporting bundle to $BUNDLE..."
flatpak build-bundle \
    "$REPO_DIR" \
    "$BUNDLE" \
    "$APP_ID"

info "Build complete: $BUNDLE"

# ── Flathub submission reminder ───────────────────────────────────────────────
# Flathub requires git sources to pin both the tag and its full commit sha.
if [[ "$LOCAL_BUILD" == false ]] && ! grep -qE '^[[:space:]]+commit:[[:space:]]*[0-9a-f]{40}' "$MANIFEST"; then
    TAG_SHA=$(git ls-remote --tags "$SOURCE_URL" "refs/tags/$TAG" 2>/dev/null | cut -f1 || true)
    warn "Manifest pins tag '$TAG' but no 'commit'. Flathub requires both."
    if [[ -n "$TAG_SHA" ]]; then
        warn "Add this line under 'tag: $TAG' before submitting:"
        warn "    commit: $TAG_SHA"
    fi
fi

# ── Optional install ──────────────────────────────────────────────────────────
if [[ "$INSTALL_AFTER" == true ]]; then
    info "Installing $APP_ID for current user..."
    flatpak install --user --noninteractive "$BUNDLE"
    info "Run with: flatpak run $APP_ID"
fi
