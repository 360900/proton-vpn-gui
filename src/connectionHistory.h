#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

/*
// ConnectionHistory – persists recent VPN connections to
//   $XDG_DATA_HOME/ProtonVPN-GUI/history.json
//   (falls back to ~/.local/share/ProtonVPN-GUI/history.json)
*/
struct ConnectionEntry
{
    QString  countryCode; // e.g. "US"
    QString  countryName; // e.g. "United States"
    QString  city;        // empty = fastest server
    QDateTime connectedAt;
};

class ConnectionHistory : public QObject
{
    Q_OBJECT
public:
    static ConnectionHistory& instance();

    // Returns entries newest-first, up to recentConnectionsCount().
    [[nodiscard]] QList<ConnectionEntry> entries() const;

    // Record a new connection (or update the timestamp if already present).
    // Trims to recentConnectionsCount() automatically.
    void record(const QString& countryCode,
                const QString& countryName,
                const QString& city);

    // Returns true if there is any raw history data, regardless of the current
    // recentConnectionsCount() setting (i.e. even when count is 0).
    [[nodiscard]] bool hasAnyEntries() const { return !m_entries.isEmpty(); }

    // Erase all history entries and persist the empty list.
    void clear();

    // Trim stored entries to newMax and emit changed() if any were removed.
    // Call this when the user lowers the "Recent Connections" count setting.
    void trimToCount(int newMax);

signals:
    // Emitted after entries are added via record() or removed via clear().
    void changed();

private:
    explicit ConnectionHistory(QObject* parent = nullptr);
    void load();
    void save() const;

    QList<ConnectionEntry> m_entries;
};

