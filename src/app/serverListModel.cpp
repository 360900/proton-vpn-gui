// serverListModel.cpp
// See serverListModel.h.

#include "serverListModel.h"

#include "../connectionHistory.h"
#include "../favoritesManager.h"
#include "../geoUtils.h"
#include "vpnFacade.h"

ServerListModel::ServerListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    VpnFacade* facade = VpnFacade::instance();

    connect(facade, &VpnFacade::countriesReady, this,
            [this](const QList<Country>& countries)
            {
                beginResetModel();
                m_countries = countries;
                rebuildFilter();
                endResetModel();
                m_loading = false;
                emit loadingChanged();
            });

    connect(facade, &VpnFacade::citiesReady, this,
            [this](const QString& countryCode, const QList<City>& cities)
            {
                m_cities.insert(countryCode, cities);
                m_citiesLoading.remove(countryCode);
                for (int row = 0; row < m_visible.size(); ++row)
                {
                    if (m_countries.at(m_visible.at(row)).code == countryCode)
                    {
                        const QModelIndex idx = index(row);
                        emit dataChanged(idx, idx,
                                         {CitiesRole, CitiesLoadedRole, CitiesLoadingRole});
                        break;
                    }
                }
            });

    connect(&FavoritesManager::instance(), &FavoritesManager::changed, this, [this]
            {
                emit favoritesChanged();
                if (m_visible.isEmpty() == false)
                {
                    emit dataChanged(index(0), index(m_visible.size() - 1),
                                     {FavoriteRole, CitiesRole});
                }
            });

    connect(&ConnectionHistory::instance(), &ConnectionHistory::changed,
            this, &ServerListModel::recentsChanged);
}

int ServerListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_visible.size();
}

QVariant ServerListModel::data(const QModelIndex& index, const int role) const
{
    if (index.isValid() == false || index.row() >= m_visible.size())
    {
        return {};
    }
    const Country& country = m_countries.at(m_visible.at(index.row()));

    switch (role)
    {
        case NameRole:
            return country.name;
        case CodeRole:
            return country.code;
        case FavoriteRole:
            return FavoritesManager::instance().isFavorite(country.code, QString());
        case CitiesRole:
            return citiesFor(country.code);
        case CitiesLoadedRole:
            return m_cities.contains(country.code);
        case CitiesLoadingRole:
            return m_citiesLoading.value(country.code, false);
        default:
            return {};
    }
}

QHash<int, QByteArray> ServerListModel::roleNames() const
{
    return {
        {NameRole,          "name"},
        {CodeRole,          "code"},
        {FavoriteRole,      "favorite"},
        {CitiesRole,        "cities"},
        {CitiesLoadedRole,  "citiesLoaded"},
        {CitiesLoadingRole, "citiesLoading"},
    };
}

void ServerListModel::setSearchText(const QString& text)
{
    if (m_searchText == text)
    {
        return;
    }
    m_searchText = text;
    beginResetModel();
    rebuildFilter();
    endResetModel();
    emit searchTextChanged();
}

QVariantList ServerListModel::favorites() const
{
    QVariantList list;
    for (const FavoriteEntry& entry : FavoritesManager::instance().entries())
    {
        list.append(QVariantMap{
            {QStringLiteral("countryCode"), entry.countryCode},
            {QStringLiteral("countryName"), entry.countryName},
            {QStringLiteral("city"),        entry.city},
        });
    }
    return list;
}

QVariantList ServerListModel::recents() const
{
    QVariantList list;
    for (const ConnectionEntry& entry : ConnectionHistory::instance().entries())
    {
        list.append(QVariantMap{
            {QStringLiteral("countryCode"), entry.countryCode},
            {QStringLiteral("countryName"), entry.countryName},
            {QStringLiteral("city"),        entry.city},
            {QStringLiteral("when"),        entry.connectedAt},
        });
    }
    return list;
}

void ServerListModel::refresh()
{
    m_loading = true;
    emit loadingChanged();
    VpnFacade::instance()->service()->fetchCountries();
}

void ServerListModel::loadCities(const QString& countryCode)
{
    if (m_cities.contains(countryCode) || m_citiesLoading.value(countryCode, false))
    {
        return;
    }
    m_citiesLoading.insert(countryCode, true);
    for (int row = 0; row < m_visible.size(); ++row)
    {
        if (m_countries.at(m_visible.at(row)).code == countryCode)
        {
            emit dataChanged(index(row), index(row), {CitiesLoadingRole});
            break;
        }
    }
    VpnFacade::instance()->service()->fetchCities(countryCode);
}

void ServerListModel::toggleFavorite(const QString& countryCode, const QString& city)
{
    FavoritesManager::instance().toggle(countryCode, countryName(countryCode), city);
}

bool ServerListModel::isFavorite(const QString& countryCode, const QString& city) const
{
    return FavoritesManager::instance().isFavorite(countryCode, city);
}

QString ServerListModel::countryName(const QString& countryCode) const
{
    for (const Country& country : m_countries)
    {
        if (country.code == countryCode)
        {
            return country.name;
        }
    }
    return GeoUtils::countryCodeToName(countryCode);
}

void ServerListModel::rebuildFilter()
{
    m_visible.clear();
    const QString needle = m_searchText.trimmed();
    for (int i = 0; i < m_countries.size(); ++i)
    {
        const Country& country = m_countries.at(i);
        if (needle.isEmpty() ||
            country.name.contains(needle, Qt::CaseInsensitive) ||
            country.code.compare(needle, Qt::CaseInsensitive) == 0)
        {
            m_visible.append(i);
        }
    }
}

QVariantList ServerListModel::citiesFor(const QString& code) const
{
    QVariantList list;
    for (const City& city : m_cities.value(code))
    {
        list.append(QVariantMap{
            {QStringLiteral("name"),     city.name},
            {QStringLiteral("features"), city.features},
            {QStringLiteral("favorite"),
             FavoritesManager::instance().isFavorite(code, city.name)},
        });
    }
    return list;
}
