// protonvpncli.cpp
// All VpnManager methods that interact with the protonvpn CLI by spawning
// a QProcess.  State management, settings, and polling infrastructure live
// in vpnmanager.cpp.

#include "../vpnmanager.h"
#include "statusmonitor.h"

#include <QProcess>
#include <QRegularExpression>
#include <functional>
#include <ranges>

// ---------------------------------------------------------------------------
// Internal helper — run any protonvpn sub-command asynchronously.
// ---------------------------------------------------------------------------

void VpnManager::runCommand(const QStringList& args,
                            std::function<void(int, const QString&, const QString&)> callback)
{
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished,
            this, [process, callback](int exitCode, QProcess::ExitStatus)
            {
                const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                const QString err = QString::fromUtf8(process->readAllStandardError()).trimmed();
                callback(exitCode, out, err);
                process->deleteLater();
            });
    process->start(QStringLiteral("protonvpn"), args);
}

// ---------------------------------------------------------------------------
// Installation / login
// ---------------------------------------------------------------------------

void VpnManager::checkInstalled()
{
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished,
            this, [this, process](const int exitCode, QProcess::ExitStatus)
            {
                Q_UNUSED(exitCode)
                const QString out = QString::fromUtf8(process->readAllStandardOutput());
                const QString err = QString::fromUtf8(process->readAllStandardError());
                const bool installed = !out.isEmpty() || !err.isEmpty();
                process->deleteLater();
                emit installedResult(installed);
            });
    process->start(QStringLiteral("protonvpn"), QStringList{QStringLiteral("--help")});
    if (!process->waitForStarted(2000))
    {
        process->deleteLater();
        emit installedResult(false);
    }
}

void VpnManager::checkLoginStatus()
{
    runCommand({QStringLiteral("info")}, [this](int, const QString& out, const QString&)
    {
        bool loggedIn = false;
        QString username;

        const QRegularExpression re(QStringLiteral(R"(Account:\s*'([^']+)')"));
        const auto match = re.match(out);
        if (match.hasMatch())
        {
            const QString accountVal = match.captured(1).trimmed();
            if (accountVal != QStringLiteral("None") && !accountVal.isEmpty())
            {
                loggedIn = true;
                username = accountVal;
            }
        }

        if (loggedIn)
        {
            startStatusMonitor();
            fetchAccountType();
        }

        emit loginStatusResult(loggedIn, username);
    });
}

void VpnManager::login(const QString& username, const QString& password)
{
    // CLI flow: protonvpn signin <username>
    //   stderr: "Password: "   → write password + '\n' to stdin
    //   stderr: "2FA Token: "  → optional; emit twoFactorRequired()

    if (m_signinProcess)
    {
        m_signinProcess->kill();
        m_signinProcess->deleteLater();
        m_signinProcess = nullptr;
    }

    m_signinProcess = new QProcess(this);
    QProcess* process = m_signinProcess;

    struct State
    {
        QString accumulated;
        bool passwordSent = false;
        bool twoFAEmitted = false;
    };
    auto* state = new State();

    auto processOutput = [this, process, state, password]()
    {
        if (!state->passwordSent &&
            state->accumulated.contains(QStringLiteral("Password:")))
        {
            state->passwordSent = true;
            process->write((password + QLatin1Char('\n')).toUtf8());
        }

        if (!state->twoFAEmitted &&
            state->accumulated.contains(QStringLiteral("2FA Token:")))
        {
            state->twoFAEmitted = true;
            emit twoFactorRequired();
        }
    };

    connect(process, &QProcess::readyReadStandardOutput, this,
            [process, state, processOutput]()
            {
                state->accumulated.append(QString::fromUtf8(process->readAllStandardOutput()));
                processOutput();
            });

    connect(process, &QProcess::readyReadStandardError, this,
            [process, state, processOutput]()
            {
                state->accumulated.append(QString::fromUtf8(process->readAllStandardError()));
                processOutput();
            });

    connect(process, &QProcess::finished,
            this, [this, process, state](const int exitCode, QProcess::ExitStatus)
            {
                const QString combined = state->accumulated.trimmed();
                delete state;

                if (process == m_signinProcess)
                    m_signinProcess = nullptr;
                process->deleteLater();

                const bool ok = exitCode == 0;
                QString errorMsg;
                if (!ok)
                {
                    const QStringList lines = combined.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                    for (const auto& line : std::ranges::reverse_view(lines))
                    {
                        const QString l = line.trimmed();
                        if (!l.isEmpty()
                            && !l.startsWith(QStringLiteral("Password:"))
                            && !l.startsWith(QStringLiteral("2FA")))
                        {
                            errorMsg = l;
                            break;
                        }
                    }
                    if (errorMsg.isEmpty()) errorMsg = combined;
                }
                else
                {
                    startStatusMonitor();
                    fetchAccountType();
                }
                emit loginFinished(ok, errorMsg);
            });

    process->start(QStringLiteral("protonvpn"),
                   QStringList{QStringLiteral("signin"), username});
}

void VpnManager::submit2FA(const QString& token) const
{
    if (m_signinProcess && m_signinProcess->state() == QProcess::Running)
        m_signinProcess->write((token + QStringLiteral("\n")).toUtf8());
}

void VpnManager::signOut()
{
    stopStatusMonitor();
    runCommand({QStringLiteral("signout")}, [this](int exitCode, const QString&, const QString&)
    {
        m_state = VpnState::Disconnected;
        emit signOutFinished(exitCode == 0);
    });
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

void VpnManager::connectVpn(const QString& country, const QString& city)
{
    m_lastConnectCountry = country;
    m_lastConnectCity    = city;
    m_connectedServer.clear();

    m_state = VpnState::Connecting;
    emit connectionStateChanged(m_state, QString());

    QStringList args{QStringLiteral("connect")};
    if (!country.isEmpty())
        args << QStringLiteral("--country") << country;
    if (!city.isEmpty())
        args << QStringLiteral("--city") << city;

    runCommand(args, [this](int exitCode, const QString& out, const QString& err)
    {
        if (exitCode == 0)
        {
            m_state = VpnState::Connected;
            // Strip noise / port-forwarding guide lines from CLI output.
            QStringList lines = out.split(QLatin1Char('\n'));
            lines.erase(std::ranges::remove_if(lines, [](const QString& l)
            {
                const QString ll = l.toLower();
                return ll.contains(QLatin1String("outdated")) ||
                    ll.contains(QLatin1String("updating")) ||
                    ll.contains(QLatin1String("this may take")) ||
                    ll.contains(QLatin1String("to get your forwarded port")) ||
                    ll.contains(QLatin1String("natpmpc")) ||
                    (ll.startsWith(QLatin1String("guide:")) && ll.contains(QLatin1String("http")));
            }).begin(), lines.end());
            while (!lines.isEmpty() && lines.first().trimmed().isEmpty())
                lines.removeFirst();
            emit connectionStateChanged(m_state, lines.join(QLatin1Char('\n')));
        }
        else
        {
            m_state = VpnState::Error;
            emit connectionStateChanged(m_state, err.isEmpty() ? out : err);
            emit errorOccurred(err.isEmpty() ? out : err);
        }
    });
}

void VpnManager::disconnectVpn()
{
    m_state = VpnState::Disconnecting;
    emit connectionStateChanged(m_state, QString());

    runCommand({QStringLiteral("disconnect")}, [this](const int exitCode, const QString& out, const QString& err)
    {
        if (exitCode == 0)
        {
            m_state = VpnState::Disconnected;
            emit connectionStateChanged(m_state, out);
        }
        else
        {
            m_state = VpnState::Error;
            emit connectionStateChanged(m_state, err.isEmpty() ? out : err);
            emit errorOccurred(err.isEmpty() ? out : err);
        }
    });
}

void VpnManager::disconnectVpnSync()
{
    // Used at application exit — run synchronously so the event loop does not
    // need to stay alive.
    QProcess process;
    process.start(QStringLiteral("protonvpn"), QStringList{QStringLiteral("disconnect")});
    process.waitForFinished(10000); // up to 10 s
}

void VpnManager::applyConfigValueAndReconnect(const QString& key, const QString& value)
{
    const QString country = m_lastConnectCountry;
    const QString city    = m_lastConnectCity;

    m_state = VpnState::Disconnecting;
    emit connectionStateChanged(m_state, QString());

    runCommand({QStringLiteral("disconnect")},
               [this, key, value, country, city](int exitCode, const QString&, const QString& err)
    {
        if (exitCode != 0)
        {
            m_state = VpnState::Error;
            emit connectionStateChanged(m_state, err);
            emit errorOccurred(err);
            return;
        }

        m_state = VpnState::Disconnected;
        emit connectionStateChanged(m_state, QString());

        QStringList args{QStringLiteral("config"), QStringLiteral("set")};
        args << key;
        args << value.split(QLatin1Char(' '), Qt::SkipEmptyParts);

        runCommand(args, [this, country, city](int, const QString& out, const QString& err2)
        {
            emit configApplied((out + QLatin1Char('\n') + err2).trimmed());
            connectVpn(country, city);
        });
    });
}

// ---------------------------------------------------------------------------
// Server / city / account queries
// ---------------------------------------------------------------------------

void VpnManager::fetchCountries()
{
    runCommand({QStringLiteral("countries"), QStringLiteral("list")},
               [this](int, const QString& out, const QString& err)
    {
        const QString combined = out + QLatin1Char('\n') + err;
        QMap<QString, QString> countries;
        const QStringList lines = combined.split(QLatin1Char('\n'));
        // Output format: separator line starting with "--", then "Name   Code" rows.
        bool pastSeparator = false;
        for (const QString& line : lines)
        {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;
            if (trimmed.startsWith(QStringLiteral("--"))) { pastSeparator = true; continue; }
            if (!pastSeparator) continue;
            const QStringList parts = line.split(QStringLiteral("  "), Qt::SkipEmptyParts);
            if (parts.size() < 2) continue;
            const QString name = parts.first().trimmed();
            const QString code = parts.last().trimmed();
            if (!name.isEmpty() && !code.isEmpty())
                countries.insert(name, code);
        }
        emit countriesReady(countries);
    });
}

void VpnManager::fetchCities(const QString& countryCode)
{
    runCommand({QStringLiteral("cities"), QStringLiteral("list"), countryCode},
               [this, countryCode](int, const QString& out, const QString& err)
               {
                   const QString combined = out + QLatin1Char('\n') + err;
                   QList<QPair<QString, QString>> cities;
                   const QStringList lines = combined.split(QLatin1Char('\n'));
                   // Output format: separator line, then "City   Features" rows.
                   bool pastSeparator = false;
                   for (const QString& line : lines)
                   {
                       const QString trimmed = line.trimmed();
                       if (trimmed.isEmpty()) continue;
                       if (trimmed.startsWith(QStringLiteral("--"))) { pastSeparator = true; continue; }
                       if (!pastSeparator) continue;
                       const QStringList parts = line.split(QStringLiteral("  "), Qt::SkipEmptyParts);
                       const QString city     = parts.value(0).trimmed();
                       const QString features = parts.value(1).trimmed();
                       if (!city.isEmpty())
                           cities.append({city, features});
                   }
                   emit citiesReady(countryCode, cities);
               });
}

void VpnManager::fetchCityFeatures(const QString& countryCode, const QString& city,
                                    std::function<void(const QString& features)> callback)
{
    runCommand({QStringLiteral("cities"), QStringLiteral("list"), countryCode},
               [city, callback](int, const QString& out, const QString& err)
               {
                   const QString combined = out + QLatin1Char('\n') + err;
                   const QStringList lines = combined.split(QLatin1Char('\n'));
                   bool pastSeparator = false;
                   for (const QString& line : lines)
                   {
                       const QString trimmed = line.trimmed();
                       if (trimmed.isEmpty()) continue;
                       if (trimmed.startsWith(QStringLiteral("--"))) { pastSeparator = true; continue; }
                       if (!pastSeparator) continue;
                       const QStringList parts = line.split(QStringLiteral("  "), Qt::SkipEmptyParts);
                       const QString cityName = parts.value(0).trimmed();
                       if (cityName.compare(city, Qt::CaseInsensitive) == 0)
                       {
                           callback(parts.value(1).trimmed());
                           return;
                       }
                   }
                   callback(QString()); // city not found
               });
}

void VpnManager::fetchInfo()
{
    runCommand({QStringLiteral("info")}, [this](int, const QString& out, const QString&)
    {
        QMap<QString, QString> result;
        const QRegularExpression re(QStringLiteral(R"((\w[\w ]*):\s*'([^']*)')"));
        QRegularExpressionMatchIterator it = re.globalMatch(out);
        while (it.hasNext())
        {
            const auto m = it.next();
            result.insert(m.captured(1).trimmed(), m.captured(2).trimmed());
        }
        emit infoReady(result);
    });
}

void VpnManager::fetchAccountType()
{
    runCommand({QStringLiteral("config"), QStringLiteral("list")},
               [this](int, const QString& out, const QString& err)
    {
        const QString combined = out + QLatin1Char('\n') + err;
        const AccountType type =
            combined.contains(QStringLiteral("To upgrade to VPN Plus"), Qt::CaseInsensitive)
            ? AccountType::Free
            : AccountType::Plus;
        m_accountType = type;
        emit accountTypeReady(type);
    });
}

void VpnManager::fetchCliVersion()
{
    // `protonvpn` with no args prints the ASCII banner; the version number
    // (semver) appears on the last banner line.
    runCommand({}, [this](int, const QString& out, const QString& err)
    {
        const QString combined = out + QLatin1Char('\n') + err;
        const QRegularExpression re(QStringLiteral(R"(\b(\d+\.\d+\.\d+)\b)"));
        const QStringList lines = combined.split(QLatin1Char('\n'));
        for (const QString& line : std::ranges::reverse_view(lines))
        {
            const QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch())
            {
                emit cliVersionReady(match.captured(1));
                return;
            }
        }
        emit cliVersionReady(QString());
    });
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

void VpnManager::applyConfig(const QString& key, bool enabled)
{
    applyConfigValue(key, enabled ? QStringLiteral("on") : QStringLiteral("off"));
}

void VpnManager::applyConfigValue(const QString& key, const QString& value)
{
    QStringList args{QStringLiteral("config"), QStringLiteral("set"), key};
    args << value.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    runCommand(args, [this](int, const QString& out, const QString& err)
    {
        const QString combined = (out + QLatin1Char('\n') + err).trimmed();
        emit configApplied(combined);
    });
}

