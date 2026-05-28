#pragma once

#include <QCoreApplication>

// ============================================================
// Shared UI helpers used across multiple source files.
// ============================================================

//  Braille spinner frames
// Use with spinnerFrame() and kSpinnerFrameCount.
static constexpr const char* kSpinnerFrames[] =
{
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};
static constexpr int kSpinnerFrameCount = 10;

//  Server-feature metadata
// Used to render per-item feature icons in city lists / location pickers.
struct FeatureMeta
{
    const char* keyword;
    const char* resource;
    const char* tooltip;
};

static constexpr FeatureMeta kServerFeatures[] =
{
    // Designated initializers: C++23 feature!
    {
        .keyword = "p2p",
        .resource = ":/assets/server-p2p.svg",
        .tooltip = "P2P - Optimized for peer-to-peer file sharing"
    },
    {
        .keyword = "secure core",
        .resource = ":/assets/server-secure-core.svg",
        .tooltip = "Secure Core - Routes traffic through privacy-friendly countries"
    },
    {
        .keyword = "tor",
        .resource = ":/assets/server-tor.svg",
        .tooltip = "Tor - Routes traffic through the Tor anonymity network"
    },
};

static constexpr int kServerFeatureCount = std::size(kServerFeatures);

// Returns the translated tooltip for a server feature.
// Use this instead of accessing meta.tooltip directly so the string is
// extracted by lupdate and translated at runtime.
inline QString translatedFeatureTooltip(const FeatureMeta& meta)
{
    return QCoreApplication::translate("FeatureMeta", meta.tooltip);
}

//  Settings on/off helper
// Returns true for the common CLI truthy strings.
inline bool isOnString(const QString& v)
{
    return v == QLatin1String("on")
        || v == QLatin1String("true")
        || v == QLatin1String("1")
        || v == QLatin1String("enabled");
}

