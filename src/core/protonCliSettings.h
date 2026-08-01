#pragma once
// protonCliSettings.h
// Reader for the Proton CLI's own settings file
// (~/.config/Proton/VPN/settings.json). The CLI has no `config get`, so the
// app reads the file directly; under Flatpak this works via the
// `--filesystem=xdg-config/Proton:ro` grant in the manifest.
//
// The JSON schema is mapped to the same stringly key/value shape the CLI's
// `config set` accepts ("kill-switch" -> "standard"/"off", booleans ->
// "on"/"off"), because that is what the settings UI round-trips.

#include <QByteArray>
#include <QMap>
#include <QString>

namespace ProtonCliSettings
{
// Default on-disk location of the CLI settings file.
QString settingsFilePath();

// Pure mapping from the file's JSON to config-set style keys.
// Unknown or missing fields are simply absent from the result.
QMap<QString, QString> parseSettingsJson(const QByteArray& json);

// Convenience: read + parse settingsFilePath(). Empty map when unreadable.
QMap<QString, QString> readSettings();

// Fast path for the single most-consulted flag.
bool portForwardingEnabled();
} // namespace ProtonCliSettings
