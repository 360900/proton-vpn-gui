#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

// ---------------------------------------------------------------------------
// GeoUtils — shared utilities for country detection and flag icon loading.
// These are free functions in a namespace so any page can use them without
// coupling to a specific widget class.
// ---------------------------------------------------------------------------
namespace GeoUtils
{
    // Detect the user's country via system timezone then locale territory.
    // Returns a 2-letter uppercase ISO 3166-1 alpha-2 code (e.g. "US", "DE"),
    // or an empty string if detection fails.
    QString detectUserCountry();

    // Convert a 2-letter country code to its English display name
    // (e.g. "US" → "United States").  Returns the code itself if unknown.
    QString countryCodeToName(const QString& code);

    // Render an SVG resource path into a QPixmap at the given pixel size.
    QPixmap svgPixmap(const QString& resourcePath, int size = 16);

    // Render an SVG resource path into a QPixmap with explicit dimensions.
    // Useful for non-square assets such as 4:3 flags.
    QPixmap svgPixmap(const QString& resourcePath, int width, int height);

    // Return a QIcon for the given country code using the embedded /flags/
    // resources (e.g. "US" → :/flags/us).  Icons are cached so each SVG is
    // only decoded once per session.  Returns a null QIcon if no flag exists.
    QIcon flagIcon(const QString& countryCode);
} // namespace GeoUtils

