#pragma once

// ============================================================
// Shared UI helpers used across multiple source files.
// ============================================================

// ── Braille spinner frames ──────────────────────────────────
// Use with spinnerFrame() and kSpinnerFrameCount.
static constexpr const char* kSpinnerFrames[] =
    {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
static constexpr int kSpinnerFrameCount = 10;

// ── Server-feature metadata ─────────────────────────────────
// Used to render per-item feature icons in city lists / location pickers.
struct FeatureMeta
{
    const char* keyword;
    const char* resource;
    const char* tooltip;
};

static constexpr FeatureMeta kServerFeatures[] = {
    { "p2p",         ":/assets/server-p2p.svg",
      "P2P — Optimized for peer-to-peer file sharing" },
    { "secure core", ":/assets/server-secure-core.svg",
      "Secure Core — Routes traffic through privacy-friendly countries" },
    { "tor",         ":/assets/server-tor.svg",
      "Tor — Routes traffic through the Tor anonymity network" },
};
static constexpr int kServerFeatureCount = std::size(kServerFeatures);

// ── Settings on/off helper ──────────────────────────────────
// Returns true for the common CLI truthy strings.
inline bool isOnString(const QString& v)
{
    return v == QLatin1String("on")
        || v == QLatin1String("true")
        || v == QLatin1String("1")
        || v == QLatin1String("enabled");
}

