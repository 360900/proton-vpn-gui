#pragma once
// serverListModel.h
// ServerListModel - the sidebar's data: filterable country list (with
// lazily-fetched cities), plus favorites and recents as list properties.
// One instance is created from QML; it self-wires to VpnFacade.

#include "../core/cliTypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>
#include <QVariantList>

class ServerListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QVariantList favorites READ favorites NOTIFY favoritesChanged)
    Q_PROPERTY(QVariantList recents READ recents NOTIFY recentsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY loadingChanged)

public:
    enum Roles
    {
        NameRole = Qt::UserRole + 1,
        CodeRole,
        FavoriteRole,      // country-level favorite (city == "")
        CitiesRole,        // QVariantList of {name, features, favorite}
        CitiesLoadedRole,
        CitiesLoadingRole,
    };

    explicit ServerListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString searchText() const { return m_searchText; }
    void setSearchText(const QString& text);

    QVariantList favorites() const;
    QVariantList recents() const;
    bool loading() const { return m_loading; }
    int  totalCount() const { return m_countries.size(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadCities(const QString& countryCode);
    Q_INVOKABLE void toggleFavorite(const QString& countryCode, const QString& city);
    Q_INVOKABLE bool isFavorite(const QString& countryCode, const QString& city) const;
    Q_INVOKABLE QString countryName(const QString& countryCode) const;

signals:
    void searchTextChanged();
    void favoritesChanged();
    void recentsChanged();
    void loadingChanged();

private:
    void rebuildFilter();
    QVariantList citiesFor(const QString& code) const;

    QList<Country> m_countries;       // full list from the CLI
    QList<int>     m_visible;         // indices into m_countries after filtering
    QHash<QString, QList<City>> m_cities;   // code -> cities (cache)
    QHash<QString, bool>        m_citiesLoading;
    QString m_searchText;
    bool    m_loading = false;
};
