#pragma once
// cliTypes.h
// Typed domain models shared between the vpncore library and the UI.
// These replace the stringly-typed QMap payloads that used to flow out of
// VpnManager, and give every CLI parser a concrete result type.

#include <QMap>
#include <QString>

enum class VpnState
{
    Unknown,
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

enum class AccountType
{
    Unknown,
    Free,
    Plus
};

// One row of `protonvpn countries list`.
struct Country
{
    QString name; // "United States"
    QString code; // "US"

    bool operator==(const Country&) const = default;
};

// One row of `protonvpn cities list <CC>`.
struct City
{
    QString name;     // "Secaucus"
    QString features; // "P2P, Tor" - CLI's feature column, verbatim

    bool operator==(const City&) const = default;
};

// Parsed from a connected-server string like "US-NJ#203 in Secaucus, United States".
struct ServerInfo
{
    QString countryCode; // "US" (uppercased) - empty if not parseable
    QString city;        // "Secaucus"        - empty if not parseable

    bool operator==(const ServerInfo&) const = default;
};

// One parsed `protonvpn status` snapshot.
struct StatusSnapshot
{
    VpnState state = VpnState::Unknown; // Connected or Disconnected (status polls never report transitions)
    QString server;                     // e.g. "US-NJ#203 in Secaucus, United States" - empty when disconnected
    QMap<QString, QString> raw;         // every "Key: Value" pair, lowercased keys

    bool operator==(const StatusSnapshot&) const = default;
};

// Outcome of a `protonvpn signin` run.
struct LoginResult
{
    bool ok         = false; // exit 0 and no error text
    bool authFailed = false; // wrong credentials / expired session ("401")
    bool crash      = false; // CLI raised a Python traceback
    QString errorText;       // human-readable failure summary (empty when ok)

    bool operator==(const LoginResult&) const = default;
};
