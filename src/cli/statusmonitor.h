// statusmonitor.h
// StatusMonitor: long-lived subprocess that runs `protonvpn status` in a
// 15-second loop and emits a parsed snapshot for every iteration.
//
// The subprocess renames itself to "protonvpn-qt-status-mon" via `exec -a`
// so it is easy to identify in ps/top/htop/bpftrace/journalctl traces.
//
// VpnManager owns one instance and connects to statusParsed() to apply state
// changes.  The static helpers parseStatusFields() / parseCityFromServer() are
// shared utilities for parsing `protonvpn status` output.
#pragma once

#include <QMap>
#include <QObject>
#include <QProcess>
#include <QString>

class StatusMonitor : public QObject
{
    Q_OBJECT

public:
    explicit StatusMonitor(QObject* parent = nullptr);
    ~StatusMonitor() override;

    // Start the monitor loop.  Safe to call multiple times — ignored if
    // already running.
    void start();

    // Stop the monitor loop and kill the subprocess.  Any pending auto-restart
    // timer is also cancelled.
    void stop();

    bool isRunning() const;

    // ── Shared parsing helpers ───────────────────────────────────────────────

    // Strip noise lines from `protonvpn status` output and parse all
    // "Key: Value" pairs into a map (keys are lowercased).
    static QMap<QString, QString> parseStatusFields(const QString& combined);

    // Extract the city from a server string like "US-NJ#203 in Secaucus, United States".
    // Returns an empty string if the pattern is not present.
    static QString parseCityFromServer(const QString& server);

signals:
    // Emitted once per complete `protonvpn status` snapshot (every ~15 s).
    void statusParsed(const QMap<QString, QString>& fields);

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void launchProcess();

    QProcess* m_process      = nullptr;
    QString   m_buffer;
    int       m_restartCount = 0;
    bool      m_stopping     = false; // set by stop() to suppress auto-restart
};

