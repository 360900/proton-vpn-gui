#include "vpnmanager.h"

#include <QProcess>
#include <QTimer>
#include <QDir>
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <functional>
#include <ranges>

// Path to the ProtonVPN settings file, relative to the user's home directory.
// Change this constant if the CLI ever moves the file.
static const QString kSettingsPath =
    QDir::homePath() + QStringLiteral("/.config/Proton/VPN/settings.json");

VpnManager::VpnManager(QObject* parent)
    : QObject(parent)
{
}

void VpnManager::runCommand(const QStringList& args,
                            std::function<void(int, const QString&, const QString&)> callback)
{
    auto* process = new QProcess(this);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [process, callback](int exitCode, QProcess::ExitStatus)
            {
                const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                const QString err = QString::fromUtf8(process->readAllStandardError()).trimmed();
                callback(exitCode, out, err);
                process->deleteLater();
            });
    process->start(QStringLiteral("protonvpn"), args);
}

void VpnManager::checkInstalled()
{
    auto* process = new QProcess(this);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus)
            {
                Q_UNUSED(exitCode)
                QString out = QString::fromUtf8(process->readAllStandardOutput());
                QString err = QString::fromUtf8(process->readAllStandardError());
                // If we got any output at all, the tool is installed
                bool installed = !out.isEmpty() || !err.isEmpty();
                process->deleteLater();
                emit installedResult(installed);
            });
    // Use --help which always exits 0 and prints usage
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
        // New CLI format: "Account: 'username@example.com'"
        // Not logged in → no match (or the value is empty / None).
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

        // Kick off an async `protonvpn status` check so the VPN page can show
        // the correct connected/disconnected state as soon as the result arrives.
        if (loggedIn)
        {
            checkConnectionStatus();
            startPolling();
            fetchAccountType();
        }

        emit loginStatusResult(loggedIn, username);
    });
}

void VpnManager::login(const QString& username, const QString& password)
{
    // CLI flow: protonvpn signin <username>
    //   stderr line 1: "Password: "   → we write password + '\n' to stdin
    //   stderr line 2: "2FA Token: "  → optional; we emit twoFactorRequired()
    //                                   and later submit2FA() writes the token
    //
    // Python's getpass.unix_getpass() tries /dev/tty first. Under QProcess
    // there is no controlling TTY, so it falls back to writing the prompt on
    // sys.stderr and reading the answer from sys.stdin — both of which are
    // the pipes QProcess controls. That means we CAN see the prompts; we just
    // have to watch stderr (not stdout).

    if (m_signinProcess)
    {
        m_signinProcess->kill();
        m_signinProcess->deleteLater();
        m_signinProcess = nullptr;
    }

    m_signinProcess = new QProcess(this);
    QProcess* process = m_signinProcess;

    // Shared state — heap-allocated so lambdas can share across two signals.
    struct State
    {
        QString accumulated;
        bool passwordSent = false;
        bool twoFAEmitted = false;
    };
    auto* state = new State();

    // Helper that processes whatever has arrived so far.
    // Called from both readyReadStandardOutput and readyReadStandardError.
    auto processOutput = [this, process, state, password]()
    {
        // "Password: " prompt arrives on stderr (getpass fallback).
        if (!state->passwordSent &&
            state->accumulated.contains(QStringLiteral("Password:")))
        {
            state->passwordSent = true;
            process->write((password + QLatin1Char('\n')).toUtf8());
        }

        // "2FA Token: " prompt also arrives on stderr, after the password
        // has been accepted.
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

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
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
                    // Return the last meaningful line, skipping echoed prompt lines.
                    const QStringList lines = combined.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                    for (const auto & line : std::ranges::reverse_view(lines))
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
                    // Start background polling now that we're logged in.
                    checkConnectionStatus();
                    startPolling();
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
    {
        m_signinProcess->write((token + QStringLiteral("\n")).toUtf8());
    }
}

void VpnManager::signOut()
{
    stopPolling();
    runCommand({QStringLiteral("signout")}, [this](int exitCode, const QString&, const QString&)
    {
        m_state = VpnState::Disconnected;
        emit signOutFinished(exitCode == 0);
    });
}

void VpnManager::connectVpn(const QString& country, const QString& city)
{
    // Clear the tracked server so the first poll after this app-initiated
    // connection silently learns the new server instead of treating the change
    // as an external location switch (which would spuriously re-fire the
    // connectionStateChanged / connectionCityKnown signals).
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
            // Strip any "server list is outdated" / "updating" noise lines the
            // CLI sometimes prepends before the actual connection info.
            QStringList lines = out.split(QLatin1Char('\n'));
            lines.erase(std::ranges::remove_if(lines, [](const QString& l)
            {
                const QString ll = l.toLower();
                return ll.contains(QLatin1String("outdated")) ||
                    ll.contains(QLatin1String("updating")) ||
                    ll.contains(QLatin1String("this may take"));
            }).begin(), lines.end());
            // Remove leading/trailing blank lines left after filtering
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
    // Used at application exit — we need the process to complete before the
    // event loop tears down, so run it synchronously.
    QProcess process;
    process.start(QStringLiteral("protonvpn"), QStringList{QStringLiteral("disconnect")});
    process.waitForFinished(10000); // up to 10 s
}

void VpnManager::fetchCountries()
{
    runCommand({QStringLiteral("countries"), QStringLiteral("list")},
               [this](int, const QString& out, const QString& err)
    {
        // Combine stdout + stderr — the "Server list is outdated, updating..."
        // notice can appear on either stream.
        const QString combined = out + QLatin1Char('\n') + err;
        QMap<QString, QString> countries; // name → code
        const QStringList lines = combined.split(QLatin1Char('\n'));
        // Output format (after any update notice lines):
        //   Country                 Code
        //   ----------------------  ------
        //   Afghanistan             AF
        // We anchor to the separator line (starts with "--") and only parse
        // lines that come after it, skipping any status/notice messages.
        bool pastSeparator = false;
        for (const QString& line : lines)
        {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty())
                continue;
            if (trimmed.startsWith(QStringLiteral("--")))
            {
                pastSeparator = true;
                continue;
            }
            if (!pastSeparator)
                continue; // skip header and any update-notice lines above it
            // Name and code are separated by 2+ spaces.
            const QStringList parts = line.split(QStringLiteral("  "), Qt::SkipEmptyParts);
            if (parts.size() < 2)
                continue;
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
                   // Combine stdout + stderr to catch any "updating..." notices.
                   const QString combined = out + QLatin1Char('\n') + err;
                   QList<QPair<QString, QString>> cities;
                   const QStringList lines = combined.split(QLatin1Char('\n'));
                   // Output format (after any update notice lines):
                   //   Cities in United States:
                   //   City            Features
                   //   --------------  ----------------
                   //   Ashburn         P2P
                   //   New York        P2P, Secure Core
                   // We anchor to the separator line (starts with "--") and only
                   // parse lines that come after it.
                   bool pastSeparator = false;
                   for (const QString& line : lines)
                   {
                       const QString trimmed = line.trimmed();
                       if (trimmed.isEmpty())
                           continue;
                       if (trimmed.startsWith(QStringLiteral("--")))
                       {
                           pastSeparator = true;
                           continue;
                       }
                       if (!pastSeparator)
                           continue; // skip intro/header/notice lines
                       // City and Features are separated by 2+ spaces.
                       const QStringList parts = line.split(QStringLiteral("  "), Qt::SkipEmptyParts);
                       const QString city = parts.value(0).trimmed();
                       const QString features = parts.value(1).trimmed();
                       if (!city.isEmpty())
                           cities.append({city, features});
                   }
                   emit citiesReady(countryCode, cities);
               });
}

void VpnManager::fetchInfo()
{
    runCommand({QStringLiteral("info")}, [this](int, const QString& out, const QString&)
    {
        // New CLI format: lines like "Account: 'username@example.com'"
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
        // If the output contains the "upgrade to VPN Plus" text, the user is on
        // the Free plan.  Otherwise they are on Plus (or higher).
        const QString combined = out + QLatin1Char('\n') + err;
        const AccountType type =
            combined.contains(QStringLiteral("To upgrade to VPN Plus"), Qt::CaseInsensitive)
            ? AccountType::Free
            : AccountType::Plus;

        m_accountType = type;
        emit accountTypeReady(type);
    });
}

void VpnManager::checkConnectionStatus()
{
    auto* process = new QProcess(this);
    // We connect to `finished`, not `readyRead`, so we receive the complete
    // output in one go.  When the CLI needs to refresh its server list it
    // prints "Server list is outdated, updating…" and pauses for a second or
    // two before printing the actual status lines.  Because the process does
    // not exit until all output has been written, `finished` fires only once
    // everything — including the post-update status — is available.
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int, QProcess::ExitStatus)
            {
                // Combine stdout + stderr — the "Server list is outdated" notice
                // can appear on either stream.
                const QString combined = QString::fromUtf8(process->readAllStandardOutput())
                                         + QLatin1Char('\n')
                                         + QString::fromUtf8(process->readAllStandardError());
                process->deleteLater();

                // Strip noise lines (same filter used in connectVpn).
                QStringList lines = combined.split(QLatin1Char('\n'));
                lines.erase(std::ranges::remove_if(lines, [](const QString& l)
                {
                    const QString ll = l.toLower();
                    return ll.contains(QLatin1String("outdated"))    ||
                           ll.contains(QLatin1String("updating"))    ||
                           ll.contains(QLatin1String("this may take"));
                }).begin(), lines.end());

                // Parse "Key: Value" lines into a map.
                QMap<QString, QString> fields;
                for (const QString& line : std::as_const(lines))
                {
                    const int colonPos = line.indexOf(QLatin1Char(':'));
                    if (colonPos < 0) continue;
                    const QString key   = line.left(colonPos).trimmed().toLower();
                    const QString value = line.mid(colonPos + 1).trimmed();
                    if (!key.isEmpty() && !value.isEmpty())
                        fields.insert(key, value);
                }

                // "Status: Connected" / "Status: Disconnected"
                const QString statusVal = fields.value(QStringLiteral("status")).toLower();

                if (statusVal == QStringLiteral("connected"))
                {
                    m_state = VpnState::Connected;

                    // "Server: US-NJ#203 in Secaucus, United States"
                    const QString server = fields.value(QStringLiteral("server"));

                    // Parse city: everything between " in " and the first comma.
                    // e.g. "US-NJ#203 in Secaucus, United States" → "Secaucus"
                    QString city;
                    const int inPos = server.indexOf(QStringLiteral(" in "));
                    if (inPos >= 0)
                    {
                        const QString rest     = server.mid(inPos + 4); // "Secaucus, United States"
                        const int    commaPos  = rest.indexOf(QLatin1Char(','));
                        city = (commaPos >= 0 ? rest.left(commaPos) : rest).trimmed();
                    }

                    // Emit city first so the VPN page can store it before
                    // updateUi() runs in response to connectionStateChanged.
                    if (!city.isEmpty())
                        emit connectionCityKnown(city);

                    const QString info = server.isEmpty()
                        ? QString()
                        : QStringLiteral("Connected to %1.").arg(server);

                    emit connectionStateChanged(m_state, info);
                }
                else
                {
                    m_state = VpnState::Disconnected;
                    emit connectionStateChanged(m_state, QString());
                }
            });
    process->start(QStringLiteral("protonvpn"), QStringList{QStringLiteral("status")});
}


// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

void VpnManager::fetchSettings()
{
    QMap<QString, QString> settings;

    QFile f(kSettingsPath);
    if (!f.open(QIODevice::ReadOnly))
    {
        // File not found or unreadable — emit empty map so the UI
        // can still display (all defaults / unknown state).
        emit settingsReady(settings);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (!doc.isObject())
    {
        emit settingsReady(settings);
        return;
    }

    const QJsonObject root = doc.object();

    // Helper: convert a JSON value to a normalised string ("on"/"off"/value).
    auto boolStr = [](bool b) -> QString
    {
        return b ? QStringLiteral("on") : QStringLiteral("off");
    };

    // ── top-level keys ────────────────────────────────────────────────────
    // killswitch: 0 = off, 1 = standard, 2 = permanent/full
    if (root.contains(QStringLiteral("killswitch")))
    {
        const int ks = root[QStringLiteral("killswitch")].toInt(0);
        QString ksVal;
        switch (ks)
        {
        case 1: ksVal = QStringLiteral("standard");
            break;
        case 2: ksVal = QStringLiteral("full");
            break;
        default: ksVal = QStringLiteral("off");
            break;
        }
        settings.insert(QStringLiteral("kill-switch"), ksVal);
    }

    if (root.contains(QStringLiteral("ipv6")))
        settings.insert(QStringLiteral("ipv6"),
                        boolStr(root[QStringLiteral("ipv6")].toBool()));

    if (root.contains(QStringLiteral("anonymous_crash_reports")))
        settings.insert(QStringLiteral("anonymous-crash-reports"),
                        boolStr(root[QStringLiteral("anonymous_crash_reports")].toBool()));

    // ── custom_dns ────────────────────────────────────────────────────────
    if (root.contains(QStringLiteral("custom_dns")))
    {
        const QJsonObject dns = root[QStringLiteral("custom_dns")].toObject();
        const bool dnsEnabled = dns[QStringLiteral("enabled")].toBool(false);
        if (dnsEnabled)
        {
            const QJsonArray ipList = dns[QStringLiteral("ip_list")].toArray();
            QStringList ips;
            for (const auto& v : ipList) ips << v.toString();
            settings.insert(QStringLiteral("custom-dns"),
                            ips.isEmpty() ? QStringLiteral("on") : ips.join(QLatin1Char(',')));
        }
        else
        {
            settings.insert(QStringLiteral("custom-dns"), QStringLiteral("off"));
        }
    }

    // ── features object ───────────────────────────────────────────────────
    if (root.contains(QStringLiteral("features")))
    {
        const QJsonObject feat = root[QStringLiteral("features")].toObject();

        // netshield: 0 = off, 1 = malware-only, 2 = malware-ads-trackers
        if (feat.contains(QStringLiteral("netshield")))
        {
            const int ns = feat[QStringLiteral("netshield")].toInt(0);
            QString nsVal;
            switch (ns)
            {
            case 1: nsVal = QStringLiteral("malware-only");
                break;
            case 2: nsVal = QStringLiteral("malware-ads-trackers");
                break;
            default: nsVal = QStringLiteral("off");
                break;
            }
            settings.insert(QStringLiteral("netshield"), nsVal);
        }

        if (feat.contains(QStringLiteral("moderate_nat")))
            settings.insert(QStringLiteral("moderate-nat"),
                            boolStr(feat[QStringLiteral("moderate_nat")].toBool()));

        if (feat.contains(QStringLiteral("vpn_accelerator")))
            settings.insert(QStringLiteral("vpn-accelerator"),
                            boolStr(feat[QStringLiteral("vpn_accelerator")].toBool()));

        if (feat.contains(QStringLiteral("port_forwarding")))
            settings.insert(QStringLiteral("port-forwarding"),
                            boolStr(feat[QStringLiteral("port_forwarding")].toBool()));
    }

    emit settingsReady(settings);
}

void VpnManager::applyConfig(const QString& key, bool enabled)
{
    applyConfigValue(key, enabled ? QStringLiteral("on") : QStringLiteral("off"));
}

void VpnManager::applyConfigValue(const QString& key, const QString& value)
{
    // Split value on whitespace so callers can pass e.g. "--dns 1.1.1.1 on"
    // and have each token become a separate CLI argument.
    QStringList args{QStringLiteral("config"), QStringLiteral("set"), key};
    const QStringList valueParts = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    args << valueParts;

    runCommand(args, [this](int, const QString& out, const QString& err)
    {
        // Combine stdout and stderr — the CLI prints reconnect notices like
        // "please establish a new VPN connection for changes to take effect"
        // to stdout; emit so listeners can act on it.
        const QString combined = (out + QLatin1Char('\n') + err).trimmed();
        emit configApplied(combined);
    });
}

void VpnManager::fetchCliVersion()
{
    // Running `protonvpn` with no args prints the ASCII banner which ends with
    // the version number on the last banner line, e.g.:
    //   |_|   |_|  \___/ \__\___/|_| |_|    \_/  |_|   |_| \_| 0.1.7
    // We scan all output lines for a semver-looking token (digits.digits.digits).
    runCommand({}, [this](int, const QString& out, const QString& err)
    {
        const QString combined = out + QLatin1Char('\n') + err;
        const QRegularExpression re(QStringLiteral(R"(\b(\d+\.\d+\.\d+)\b)"));
        // Walk lines in reverse — the version is on the last banner line
        const QStringList lines = combined.split(QLatin1Char('\n'));
        for (int i = lines.size() - 1; i >= 0; --i)
        {
            const auto match = re.match(lines[i]);
            if (match.hasMatch())
            {
                emit cliVersionReady(match.captured(1));
                return;
            }
        }
        emit cliVersionReady(QString()); // not found
    });
}

// ---------------------------------------------------------------------------
// Background polling
// ---------------------------------------------------------------------------

void VpnManager::startPolling()
{
    if (m_pollTimer)
        return; // already running

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(15'000); // 15 s
    connect(m_pollTimer, &QTimer::timeout, this, &VpnManager::pollStatus);
    m_pollTimer->start();
}

void VpnManager::stopPolling()
{
    if (m_pollTimer)
    {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }
    m_pollActive = false;
}

void VpnManager::pollStatus()
{
    if (m_pollActive)
        return; // previous poll still in flight

    m_pollActive = true;

    auto* process = new QProcess(this);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int, QProcess::ExitStatus)
            {
                m_pollActive = false;

                const QString combined =
                    QString::fromUtf8(process->readAllStandardOutput()) + QLatin1Char('\n') +
                    QString::fromUtf8(process->readAllStandardError());
                process->deleteLater();

                // Don't let a background poll override an in-progress connect
                // or disconnect initiated by the user.
                if (m_state == VpnState::Connecting || m_state == VpnState::Disconnecting)
                    return;

                // Strip noise lines
                QStringList lines = combined.split(QLatin1Char('\n'));
                lines.erase(std::ranges::remove_if(lines, [](const QString& l)
                {
                    const QString ll = l.toLower();
                    return ll.contains(QLatin1String("outdated"))    ||
                           ll.contains(QLatin1String("updating"))    ||
                           ll.contains(QLatin1String("this may take"));
                }).begin(), lines.end());

                QMap<QString, QString> fields;
                for (const QString& line : std::as_const(lines))
                {
                    const int colonPos = line.indexOf(QLatin1Char(':'));
                    if (colonPos < 0) continue;
                    const QString key   = line.left(colonPos).trimmed().toLower();
                    const QString value = line.mid(colonPos + 1).trimmed();
                    if (!key.isEmpty() && !value.isEmpty())
                        fields.insert(key, value);
                }

                const QString statusVal = fields.value(QStringLiteral("status")).toLower();
                const VpnState newState = (statusVal == QStringLiteral("connected"))
                                          ? VpnState::Connected
                                          : VpnState::Disconnected;

                // Parse server / city / info string regardless of which branch fires below.
                QString server;
                QString city;
                QString info;
                if (newState == VpnState::Connected)
                {
                    server = fields.value(QStringLiteral("server"));
                    const int inPos = server.indexOf(QStringLiteral(" in "));
                    if (inPos >= 0)
                    {
                        const QString rest    = server.mid(inPos + 4);
                        const int    commaPos = rest.indexOf(QLatin1Char(','));
                        city = (commaPos >= 0 ? rest.left(commaPos) : rest).trimmed();
                    }
                    info = server.isEmpty()
                        ? QString()
                        : QStringLiteral("Connected to %1.").arg(server);
                }

                // Capture pre-update values so the debug report below can show what changed.
#ifdef QT_DEBUG
                const VpnState dbgPrevState  = m_state;
                const QString  dbgPrevServer = m_connectedServer;
#endif

                // Also fire when the server/city changed while we stay Connected.
                // Guard on m_connectedServer being non-empty so the first poll after
                // an app-initiated connect silently learns the server without causing
                // a spurious re-emit (which would restart the elapsed timer too soon).
                if (newState != m_state ||
                    (newState == VpnState::Connected &&
                     !m_connectedServer.isEmpty() &&
                     server != m_connectedServer))
                {
                    m_state = newState;

                    if (newState == VpnState::Connected)
                    {
                        m_connectedServer = server;
                        if (!city.isEmpty())
                        {
                            emit connectionCityKnown(city);
                        }
                        emit connectionStateChanged(m_state, info);
                    }
                    else
                    {
                        m_connectedServer.clear();
                        emit connectionStateChanged(m_state, QString());
                    }
                }
                else if (newState == VpnState::Connected && m_connectedServer.isEmpty())
                {
                    // Silently record the current server so future polls can diff against it.
                    m_connectedServer = server;
                }

#ifdef QT_DEBUG
                {
                    auto stateToStr = [](const VpnState s) -> const char*
                    {
                        switch (s)
                        {
                            case VpnState::Connected:     return "Connected";
                            case VpnState::Disconnected:  return "Disconnected";
                            case VpnState::Connecting:    return "Connecting";
                            case VpnState::Disconnecting: return "Disconnecting";
                            default:                      return "Error";
                        }
                    };
                    const bool stateChanged  =  newState != dbgPrevState;
                    const bool serverChanged =  !stateChanged &&
                                                newState == VpnState::Connected &&
                                                !dbgPrevServer.isEmpty() &&
                                                server != dbgPrevServer;
                    if (stateChanged)
                    {
                        qDebug("[Status Polling] State changed:  %s → %s",
                               stateToStr(dbgPrevState), stateToStr(newState));
                    }
                    else if (serverChanged)
                    {
                        qDebug("[Status Polling] Server changed: \"%s\" → \"%s\"",
                               qUtf8Printable(dbgPrevServer), qUtf8Printable(server));
                    }
                }
#endif
            });
    process->start(QStringLiteral("protonvpn"), QStringList{QStringLiteral("status")});
}
