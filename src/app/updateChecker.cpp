// updateChecker.cpp
// See updateChecker.h.

#include "updateChecker.h"

#include "../appConfig.h"
#include "../core/debug.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVersionNumber>

namespace
{
constexpr int UPDATE_CHECK_DELAY_MS   = 3'000;
constexpr int UPDATE_CHECK_TIMEOUT_MS = 10'000;
constexpr const char* UPDATE_VERSION_URL =
    "https://raw.githubusercontent.com/360900/vela/main/src/version.json";
} // namespace

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void UpdateChecker::checkSoon()
{
    if (AppConfig::instance().checkForUpdates() == false)
    {
        return;
    }
    QTimer::singleShot(UPDATE_CHECK_DELAY_MS, this, &UpdateChecker::performCheck);
}

void UpdateChecker::performCheck()
{
    DBG_APP(QStringLiteral("Checking for updates..."));
    QNetworkRequest request{QUrl(QString::fromLatin1(UPDATE_VERSION_URL))};
    request.setTransferTimeout(UPDATE_CHECK_TIMEOUT_MS);

    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]
    {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
        {
            DBG_APP(QStringLiteral("Update check failed: ") + reply->errorString());
            return;
        }

        const QString remoteVersion = QJsonDocument::fromJson(reply->readAll())
                                          .object()
                                          .value(QStringLiteral("app_version"))
                                          .toString();
        const QString localVersion = QCoreApplication::applicationVersion();
        if (remoteVersion.isEmpty() || localVersion.isEmpty())
        {
            return;
        }

        if (QVersionNumber::fromString(remoteVersion) > QVersionNumber::fromString(localVersion))
        {
            DBG_APP(QStringLiteral("Update available: v%1 -> v%2")
                        .arg(localVersion, remoteVersion));
            emit updateAvailable(localVersion, remoteVersion);
        }
        else
        {
            DBG_APP(QStringLiteral("Up to date (v%1)").arg(localVersion));
        }
    });
}
