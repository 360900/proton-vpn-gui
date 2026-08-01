#pragma once
// cliParsers.h
// Pure functions that turn raw `protonvpn` CLI text output into the typed
// models in cliTypes.h. This is the single home for every parser in the app -
// no other file may interpret CLI output.
//
// All functions are side-effect free and fixture-tested (tst_cliParsers with
// tests/fixtures/cli/*). If the CLI's wording or table layout changes, a test
// breaks here instead of the app silently misbehaving.

#include "cliTypes.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace CliParsers
{
// Removes informational noise lines the CLI mixes into its output
// (update notices, port-forwarding guides, natpmpc hints) and returns the
// remaining lines. Shared by status parsing and connect-output cleanup.
QStringList stripNoiseLines(const QString& text);

// stripNoiseLines() joined back together with leading blank lines dropped -
// the human-readable remainder of a `protonvpn connect` success message.
QString stripNoise(const QString& text);

// Parse all "Key: Value" lines into a map. Keys are trimmed and lowercased,
// values trimmed with case preserved. Noise lines are stripped first.
QMap<QString, QString> parseKeyValueFields(const QString& text);

// Parse a full `protonvpn status` output into a typed snapshot.
// state is Connected iff the status field says so (any other value,
// including a missing field, is Disconnected - status polls never
// report transitional states).
StatusSnapshot parseStatus(const QString& text);

// Extract country code and city from a connected-server string like
// "US-NJ#203 in Secaucus, United States". Missing parts come back empty.
ServerInfo parseServerInfo(const QString& server);

// Parse the `protonvpn countries list` table ("Country  Code" rows after a
// "--" separator line).
QList<Country> parseCountriesTable(const QString& text);

// Parse the `protonvpn cities list <CC>` table ("City  Features" rows after
// a "--" separator line). The features column may be empty.
QList<City> parseCitiesTable(const QString& text);

// Parse `protonvpn info` style output: "Key: 'value'" pairs (quoted values).
// Keys keep their original case ("Account").
QMap<QString, QString> parseInfoMap(const QString& text);

// Interpret the combined output of a finished `protonvpn signin` run.
// Mirrors the CLI's observable behavior: success is exit 0 with no error
// text; "401" marks bad credentials; a Python traceback marks a CLI crash.
LoginResult parseLoginOutput(int exitCode, const QString& combined);

// Free accounts are detected by the upgrade hint the CLI appends to
// `protonvpn config list` output.
AccountType parseAccountTier(const QString& combined);

// Extract the CLI version (semver) from the no-args banner output.
// The banner goes to stderr; pass stderr + stdout combined. Scans lines
// bottom-up and returns the first semver found, or an empty string.
QString parseCliVersion(const QString& combined);

// Canonical VpnState <-> string mapping (used by the D-Bus adaptor and logs).
QString vpnStateToString(VpnState state);
} // namespace CliParsers
