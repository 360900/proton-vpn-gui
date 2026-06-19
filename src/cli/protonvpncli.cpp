// protonvpncli.cpp
// All VpnManager methods that interact with the protonvpn CLI by spawning
// a QProcess.  State management, settings, and polling infrastructure live
// in vpnmanager.cpp.

#include "../vpnmanager.h"

#include "../debug.h"
#include "flatpakutils.h"
#include "statusmonitor.h"

#include <QProcess>
#include <QRegularExpression>
#include <functional>
#include <ranges>

// ---------------------------------------------------------------------------
// Flatpak helper - when running inside a Flatpak sandbox, all CLI calls must
// be forwarded to the host via flatpak-spawn.
// ---------------------------------------------------------------------------

namespace
{
constexpr int CLI_START_TIMEOUT_MS       = 2000;
constexpr int DISCONNECT_SYNC_TIMEOUT_MS = 10000;
constexpr int MIN_COUNTRY_PARTS          = 2;

// Returns {program, fullArgs} ready for QProcess::start.
std::pair<QString, QStringList> buildCliCommand(const QStringList& args)
{
    return buildHostCommand(QStringLiteral("protonvpn"), args);
}
} // namespace

// ---------------------------------------------------------------------------
// Internal helper - run any protonvpn sub-command asynchronously.
// ---------------------------------------------------------------------------

void VpnManager::runCommand(const QStringList& args,
                            const std::function<void(int, const QString&, const QString&)>& callback)
{
    QProcess* process = new QProcess(this);
    const QString cmdLine = QStringLiteral("protonvpn ") + args.join(QLatin1Char(' '));
    DBG_CLI(QStringLiteral(">>> ") + cmdLine);
    connect(process, &QProcess::finished,
            this, [process, callback, cmdLine](int exitCode, QProcess::ExitStatus)
            {
                const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                const QString err = QString::fromUtf8(process->readAllStandardError()).trimmed();
                DBG_CLI(QStringLiteral("<<< ") + cmdLine +
                        QStringLiteral(" [exit=") + QString::number(exitCode) + QStringLiteral("]"));
                if (out.isEmpty() == false)
                {
                    DBG_CLI(QStringLiteral("    stdout: ") + out);
                }
                if (err.isEmpty() == false)
                {
                    DBG_CLI(QStringLiteral("    stderr: ") + err);
                }
                callback(exitCode, out, err);
                process->deleteLater();
            });
    auto [program, fullArgs] = buildCliCommand(args);
    process->start(program, fullArgs);
}

// ---------------------------------------------------------------------------
// Installation / login
// ---------------------------------------------------------------------------

void VpnManager::checkInstalled()
{
    DBG_CLI(QStringLiteral("Checking if protonvpn CLI is installed..."));
    QProcess* process = new QProcess(this);
    connect(process, &QProcess::finished,
            this, [this, process](const int exitCode, QProcess::ExitStatus)
            {
                Q_UNUSED(exitCode)
                const QString out = QString::fromUtf8(process->readAllStandardOutput());
                const QString err = QString::fromUtf8(process->readAllStandardError());
                const bool installed = out.isEmpty() == false || err.isEmpty() == false;
                process->deleteLater();
                DBG_CLI(installed ? QStringLiteral("protonvpn CLI found.") : QStringLiteral("protonvpn CLI NOT found."));
                emit installedResult(installed);
            });
    auto [program, fullArgs] = buildCliCommand({QStringLiteral("--help")});
    process->start(program, fullArgs);
    if (process->waitForStarted(CLI_START_TIMEOUT_MS) == false)
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
        const QRegularExpressionMatch match = re.match(out);
        if (match.hasMatch())
        {
            const QString accountVal = match.captured(1).trimmed();
            if (accountVal != QStringLiteral("None") && accountVal.isEmpty() == false)
            {
                loggedIn = true;
                username = accountVal;
            }
        }

        if (loggedIn == true)
        {
            startStatusMonitor();
            fetchAccountType();
        }

        emit loginStatusResult(loggedIn, username);
    });
}

void VpnManager::login(const QString& username, const QString& password)
{
    DBG_CLI(QStringLiteral("Login attempt for user: ") + username);
    // CLI flow: protonvpn signin <username>
    //   stderr: "Password: "   -> write password + '\n' to stdin
    //   stderr: "2FA Token: "  -> optional; emit twoFactorRequired()

    if (m_signinProcess != nullptr)
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
    State* state = new State();

    auto processOutput = [this, process, state, password]()
    {
        if (state->passwordSent == false &&
            state->accumulated.contains(QStringLiteral("Password:")))
        {
            state->passwordSent = true;
            process->write((password + QLatin1Char('\n')).toUtf8());
        }

        if (state->twoFAEmitted == false &&
            state->accumulated.contains(QStringLiteral("2FA Token:")))
        {
            state->twoFAEmitted = true;
            DBG_CLI(QStringLiteral("Two-factor authentication required."));
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

                // If cancelLogin() was called first, m_signinProcess is already
                // nullptr - don't emit loginFinished for a deliberate cancellation.
                if (process != m_signinProcess)
                {
                    process->deleteLater();
                    return;
                }
                m_signinProcess = nullptr;
                process->deleteLater();

                const bool outputHasError = combined.contains(QStringLiteral("error"), Qt::CaseInsensitive);
                const bool ok = exitCode == 0 && outputHasError == false;
                DBG_CLI(ok ? QStringLiteral("Login succeeded.") : QStringLiteral("Login failed (exit=") + QString::number(exitCode) + QStringLiteral(")."));
                QString errorMsg;
                if (ok == false)
                {
                    const QStringList lines = combined.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                    QStringList errorLines;
                    for (const QString& line : lines)
                    {
                        const QString l = line.trimmed();
                        if (l.isEmpty() == false
                            && l.startsWith(QStringLiteral("Password:")) == false
                            && l.startsWith(QStringLiteral("2FA")) == false
                            && l.startsWith(QStringLiteral("Warning:")) == false
                            && l.startsWith(QStringLiteral("Traceback")) == false
                            && l.contains(QStringLiteral(".py:")) == false
                            && line.front() != QLatin1Char(' ')
                            && line.front() != QLatin1Char('\t'))
                        {
                            errorLines.append(l);
                        }
                    }
                    errorMsg = errorLines.isEmpty() ? combined : errorLines.join(QLatin1Char('\n'));
                }
                else
                {
                    startStatusMonitor();
                    fetchAccountType();
                }
                emit loginFinished(ok, errorMsg);
            });

    auto [program, fullArgs] = buildCliCommand({QStringLiteral("signin"), username});
    process->start(program, fullArgs);
}

void VpnManager::cancelLogin()
{
    if (m_signinProcess != nullptr)
    {
        m_signinProcess->kill();
        m_signinProcess->deleteLater();
        m_signinProcess = nullptr;
    }
}

void VpnManager::submit2FA(const QString& token) const
{
    if (m_signinProcess != nullptr && m_signinProcess->state() == QProcess::Running)
    {
        m_signinProcess->write((token + QStringLiteral("\n")).toUtf8());
    }
}

void VpnManager::signOut()
{
    DBG_CLI(QStringLiteral("Signing out..."));
    stopStatusMonitor();
    runCommand({QStringLiteral("signout")}, [this](int exitCode, const QString&, const QString&)
    {
        DBG_CLI(exitCode == 0 ? QStringLiteral("Sign-out succeeded.") : QStringLiteral("Sign-out failed (exit=") + QString::number(exitCode) + QStringLiteral(")."));
        m_state = VpnState::Disconnected;
        emit signOutFinished(exitCode == 0);
    });
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

void VpnManager::connectVpn(const QString& country, const QString& city)
{
    DBG_CLI(QStringLiteral("Connecting to VPN - country: '") + (country.isEmpty() ? QStringLiteral("(fastest)") : country) +
            QStringLiteral("'  city: '") + (city.isEmpty() ? QStringLiteral("(any)") : city) + QStringLiteral("'"));
    m_lastConnectCountry = country;
    m_lastConnectCity    = city;
    m_connectedServer.clear();

    m_state = VpnState::Connecting;
    emit connectionStateChanged(m_state, QString());

    QStringList args{QStringLiteral("connect")};
    if (country.isEmpty() == false)
    {
        args << QStringLiteral("--country") << country;
    }
    if (city.isEmpty() == false)
    {
        args << QStringLiteral("--city") << city;
    }

    runCommand(args, [this](int exitCode, const QString& out, const QString& err)
    {
        if (exitCode == 0)
        {
            DBG_CLI(QStringLiteral("VPN connected successfully."));
            m_state = VpnState::Connected;
            // Strip noise / port-forwarding guidelines from CLI output.
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

            while (lines.isEmpty() == false && lines.first().trimmed().isEmpty())
            {
                lines.removeFirst();
            }

            emit connectionStateChanged(m_state, lines.join(QLatin1Char('\n')));
        }
        else
        {
            DBG_CLI(QStringLiteral("VPN connection failed (exit=") + QString::number(exitCode) + QStringLiteral("): ") + (err.isEmpty() ? out : err));
            m_state = VpnState::Error;
            emit connectionStateChanged(m_state, err.isEmpty() ? out : err);
            emit errorOccurred(err.isEmpty() ? out : err);
        }
    });
}

void VpnManager::disconnectVpn()
{
    DBG_CLI(QStringLiteral("Disconnecting VPN..."));
    m_state = VpnState::Disconnecting;
    emit connectionStateChanged(m_state, QString());

    runCommand({QStringLiteral("disconnect")}, [this](const int exitCode, const QString& out, const QString& err)
    {
        if (exitCode == 0)
        {
            DBG_CLI(QStringLiteral("VPN disconnected successfully."));
            m_state = VpnState::Disconnected;
            emit connectionStateChanged(m_state, out);
        }
        else
        {
            DBG_CLI(QStringLiteral("VPN disconnect failed (exit=") + QString::number(exitCode) + QStringLiteral("): ") + (err.isEmpty() ? out : err));
            m_state = VpnState::Error;
            emit connectionStateChanged(m_state, err.isEmpty() ? out : err);
            emit errorOccurred(err.isEmpty() ? out : err);
        }
    });
}

void VpnManager::disconnectVpnSync()
{
    // Used at application exit - run synchronously so the event loop does not
    // need to stay alive.
    QProcess process;
    auto [program, fullArgs] = buildCliCommand({QStringLiteral("disconnect")});
    process.start(program, fullArgs);
    process.waitForFinished(DISCONNECT_SYNC_TIMEOUT_MS); // up to 10 s
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
            if (trimmed.startsWith(QStringLiteral("--")))
            {
                pastSeparator = true;
                continue;
            }
            if (pastSeparator == false) continue;
            const QStringList parts = line.split(QStringLiteral("  "), Qt::SkipEmptyParts);
            if (parts.size() < MIN_COUNTRY_PARTS) continue;
            const QString name = parts.first().trimmed();
            const QString code = parts.last().trimmed();
            if (name.isEmpty() == false && code.isEmpty() == false)
            {
                countries.insert(name, code);
            }
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
                       if (trimmed.startsWith(QStringLiteral("--")))
                       {
                           pastSeparator = true;
                           continue;
                       }
                       if (pastSeparator == false) continue;
                       const QStringList parts = line.split(QStringLiteral("  "), Qt::SkipEmptyParts);
                       const QString city     = parts.value(0).trimmed();
                       const QString features = parts.value(1).trimmed();
                       if (city.isEmpty() == false)
                       {
                           cities.append({city, features});
                       }
                   }
                   emit citiesReady(countryCode, cities);
               });
}

void VpnManager::fetchCityFeatures(const QString& countryCode, const QString& city,
                                   const std::function<void(const QString& features)>& callback)
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
                       if (trimmed.startsWith(QStringLiteral("--")))
                       {
                           pastSeparator = true;
                           continue;
                       }
                       if (pastSeparator == false) continue;
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
            const QRegularExpressionMatch m = it.next();
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
    // "protonvpn" in the terminal with no args prints the ASCII banner; the version number
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

void VpnManager::applyConfig(const QString& key, const bool enabled)
{
    applyConfigValue(key, enabled == true ? QStringLiteral("on") : QStringLiteral("off"));
}

void VpnManager::applyConfigValue(const QString& key, const QString& value)
{
    DBG_CLI(QStringLiteral("Applying CLI config: ") + key + QStringLiteral(" = ") + value);
    QStringList args{QStringLiteral("config"), QStringLiteral("set"), key};
    args << value.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    runCommand(args, [this](int, const QString& out, const QString& err)
    {
        const QString combined = (out + QLatin1Char('\n') + err).trimmed();
        emit configApplied(combined);
    });
}

