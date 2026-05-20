#pragma once

#include <QObject>
#include <QString>
#include <QList>

// ---------------------------------------------------------------------------
// FavoritesManager – persists favorite VPN locations to
//   $XDG_DATA_HOME/ProtonVPN-Qt/favorites.json
// ---------------------------------------------------------------------------
struct FavoriteEntry
{
    QString countryCode; // e.g. "US"
    QString countryName; // e.g. "United States"
    QString city;        // empty = fastest server in country
};

class FavoritesManager : public QObject
{
    Q_OBJECT
public:
    static FavoritesManager& instance();

    // Returns all saved favorites in insertion order.
    [[nodiscard]] QList<FavoriteEntry> entries() const;

    // Returns true if the given (countryCode, city) is already a favorite.
    // city can be empty to check for the "fastest server in country" entry.
    [[nodiscard]] bool isFavorite(const QString& countryCode, const QString& city) const;

    // Add a Location to favorites (no-op if already present).
    void add(const QString& countryCode, const QString& countryName, const QString& city);

    // Remove a location from favorites (no-op if not present).
    void remove(const QString& countryCode, const QString& city);

    // Toggle favorite state.
    void toggle(const QString& countryCode, const QString& countryName, const QString& city);

    // Returns true if there are any saved favorites.
    [[nodiscard]] bool hasAnyEntries() const { return !m_entries.isEmpty(); }

    // Remove all favorites and persist.
    void clear();

signals:
    void changed();

private:
    explicit FavoritesManager(QObject* parent = nullptr);
    void load();
    void save() const;

    QList<FavoriteEntry> m_entries;
};

