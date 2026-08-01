#pragma once
// updateChecker.h
// Checks the repository's version.json on GitHub for a newer release,
// a few seconds after startup (opt-out via the check-for-updates setting).

#include <QObject>

class QNetworkAccessManager;

class UpdateChecker final : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);

    // Schedules the check (no-op when disabled in settings).
    void checkSoon();

signals:
    void updateAvailable(const QString& currentVersion, const QString& newVersion);

private:
    void performCheck();

    QNetworkAccessManager* m_network;
};
