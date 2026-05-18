#include "connectionhistory.h"
#include "appconfig.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
// ReSharper disable once CppUnusedIncludeDirective
#include <ranges>

static QString historyFilePath()
{
    // XDG_DATA_HOME / ProtonVPN-Qt / history.json
    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // AppDataLocation already appends the app name on most platforms, but our
    // app name is "ProtonVPN" so we end up with ~/.local/share/ProtonVPN/…
    // Override to be explicit about our sub-dir name.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                        + QStringLiteral("/ProtonVPN-Qt");
    return dir + QStringLiteral("/history.json");
}

ConnectionHistory& ConnectionHistory::instance()
{
    static ConnectionHistory inst;
    return inst;
}

ConnectionHistory::ConnectionHistory(QObject* parent)
    : QObject(parent)
{
    load();
}

QList<ConnectionEntry> ConnectionHistory::entries() const
{
    const int maxCount = AppConfig::instance().recentConnectionsCount();
    if (maxCount == 0)
        return {};
    if (m_entries.size() <= maxCount)
        return m_entries;
    return m_entries.sliced(0, maxCount);
}

void ConnectionHistory::record(const QString& countryCode,
                                const QString& countryName,
                                const QString& city)
{
    const int maxCount = AppConfig::instance().recentConnectionsCount();

    // 0 means the feature is disabled — don't store anything.
    if (maxCount == 0)
        return;

    // If already present, update timestamp and move to front
    const auto it = std::ranges::find_if(m_entries,
        [&countryCode, &city](const ConnectionEntry& e)
        {
            return e.countryCode == countryCode && e.city == city;
        });

    if (it != m_entries.end())
    {
        it->connectedAt = QDateTime::currentDateTime();
        it->countryName = countryName; // keep name fresh
        // Move to front
        const int idx = static_cast<int>(std::ranges::distance(m_entries.begin(), it));
        m_entries.move(idx, 0);
        save();
        emit changed();
        return;
    }

    // New entry — prepend
    ConnectionEntry e;
    e.countryCode  = countryCode;
    e.countryName  = countryName;
    e.city         = city;
    e.connectedAt  = QDateTime::currentDateTime();
    m_entries.prepend(e);

    // Trim to max
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (m_entries.size() > maxCount)
        m_entries.removeLast();

    save();
    emit changed();
}

void ConnectionHistory::clear()
{
    m_entries.clear();
    save();
    emit changed();
}

void ConnectionHistory::load()
{
    QFile f(historyFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return;

    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    f.close();

    m_entries.clear();
    for (const auto& val : arr)
    {
        const QJsonObject obj = val.toObject();
        ConnectionEntry e;
        e.countryCode  = obj.value(QStringLiteral("country_code")).toString();
        e.countryName  = obj.value(QStringLiteral("country_name")).toString();
        e.city         = obj.value(QStringLiteral("city")).toString();
        e.connectedAt  = QDateTime::fromString(
            obj.value(QStringLiteral("connected_at")).toString(), Qt::ISODate);
        if (!e.countryCode.isEmpty())
            m_entries.append(e);
    }
}

void ConnectionHistory::save() const
{
    const QString path = historyFilePath();
    // ReSharper disable once CppExpressionWithoutSideEffects
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray arr;
    for (const auto& e : m_entries)
    {
        QJsonObject obj;
        obj[QStringLiteral("country_code")]  = e.countryCode;
        obj[QStringLiteral("country_name")]  = e.countryName;
        obj[QStringLiteral("city")]          = e.city;
        obj[QStringLiteral("connected_at")]  = e.connectedAt.toString(Qt::ISODate);
        arr.append(obj);
    }

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

