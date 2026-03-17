#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

// ---------------------------------------------------------------------------
// ConnectionHistory – persists recent VPN connections to
//   $XDG_DATA_HOME/ProtonVPN-Qt/history.json
//   (falls back to ~/.local/share/ProtonVPN-Qt/history.json)
// ---------------------------------------------------------------------------
struct ConnectionEntry
{
    QString  countryCode; // e.g. "US"
    QString  countryName; // e.g. "United States"
    QString  city;        // empty = fastest server
    QDateTime connectedAt;
};

class ConnectionHistory
{
public:
    static ConnectionHistory& instance();

    // Returns entries newest-first, up to recentConnectionsCount().
    [[nodiscard]] QList<ConnectionEntry> entries() const;

    // Record a new connection (or update the timestamp if already present).
    // Trims to recentConnectionsCount() automatically.
    void record(const QString& countryCode,
                const QString& countryName,
                const QString& city);

private:
    ConnectionHistory();
    void load();
    void save() const;

    QList<ConnectionEntry> m_entries;
};

