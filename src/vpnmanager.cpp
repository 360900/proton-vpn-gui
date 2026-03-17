#include "vpnmanager.h"

#include <QProcess>
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
        // CLI returns something like: {'name': None} when not logged in
        // or {'name': 'username', 'status': 'Connected', ...} when logged in
        bool loggedIn = false;
        QString username;

        const QRegularExpression re(QStringLiteral("'name'\\s*:\\s*'?([^'\\}]+)'?"));
        const auto match = re.match(out);
        if (match.hasMatch())
        {
            const QString nameVal = match.captured(1).trimmed();
            if (nameVal != QStringLiteral("None") && !nameVal.isEmpty())
            {
                loggedIn = true;
                username = nameVal;
            }
        }

        // Check actual connection state via ip a — the protonvpn CLI has no
        // reliable status command, but the VPN tunnel always creates a
        // "proton0" network interface when connected.
        if (loggedIn)
            checkConnectionStatus();

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
            this, [this, process, state](int exitCode, QProcess::ExitStatus)
            {
                const QString combined = state->accumulated.trimmed();
                delete state;

                if (process == m_signinProcess)
                    m_signinProcess = nullptr;
                process->deleteLater();

                bool ok = (exitCode == 0);
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
    runCommand({QStringLiteral("signout")}, [this](int exitCode, const QString&, const QString&)
    {
        m_state = VpnState::Disconnected;
        emit signOutFinished(exitCode == 0);
    });
}

void VpnManager::connectVpn(const QString& country, const QString& city)
{
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
        emit infoReady(parseDictOutput(out));
    });
}

void VpnManager::checkConnectionStatus()
{
    auto* process = new QProcess(this);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int, QProcess::ExitStatus)
            {
                const QString out = QString::fromUtf8(process->readAllStandardOutput());
                process->deleteLater();

                const bool connected = out.contains(QStringLiteral("proton0"));
                if (!connected)
                {
                    m_state = VpnState::Disconnected;
                    emit connectionStateChanged(m_state, QString());
                    return;
                }

                m_state = VpnState::Connected;

                // If nmcli is available, extract the ProtonVPN server name from the
                // active connection list (e.g. "ProtonVPN US-NY#618") and pass it as
                // the info string so the VPN page can display it.
                auto* nmcli = new QProcess(this);
                connect(nmcli, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this, nmcli](const int exitCode, QProcess::ExitStatus)
                        {
                            QString serverName;
                            if (exitCode == 0)
                            {
                                const QString nmOut = QString::fromUtf8(nmcli->readAllStandardOutput());
                                for (const QString& line : nmOut.split(QLatin1Char('\n')))
                                {
                                    if (line.trimmed().startsWith(QStringLiteral("ProtonVPN"), Qt::CaseInsensitive))
                                    {
                                        // Line format: "ProtonVPN US-NY#618   <uuid>   wireguard   proton0"
                                        // The name is everything up to the first run of 2+ spaces.
                                        const QStringList parts = line.split(QStringLiteral("  "), Qt::SkipEmptyParts);
                                        if (!parts.isEmpty())
                                            serverName = parts.first().trimmed();
                                        break;
                                    }
                                }
                            }
                            nmcli->deleteLater();

                            // Now fetch the public IP via curl ifconfig.me and compose the
                            // full info string, matching the format used when actively connecting.
                            auto* curl = new QProcess(this);
                            connect(curl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                                    this, [this, curl, serverName](int curlExit, QProcess::ExitStatus)
                                    {
                                        const QString ip = QString::fromUtf8(curl->readAllStandardOutput()).trimmed();
                                        curl->deleteLater();

                                        // Strip the redundant "ProtonVPN " prefix so we can use our own
                                        // "Connected to" wording, e.g. "Connected to US-NY#618."
                                        QString displayName = serverName;
                                        if (displayName.startsWith(QStringLiteral("ProtonVPN "), Qt::CaseInsensitive))
                                            displayName = displayName.mid(QString(QStringLiteral("ProtonVPN ")).length());

                                        QString info;
                                        if (!displayName.isEmpty() && !ip.isEmpty())
                                            info = QStringLiteral("Connected to %1. Your IP address is %2.").arg(displayName, ip);
                                        else if (!displayName.isEmpty())
                                            info = QStringLiteral("Connected to %1.").arg(displayName);
                                        else if (!ip.isEmpty())
                                            info = QStringLiteral("Your IP address is %1.").arg(ip);

                                        emit connectionStateChanged(m_state, info);
                                    });
                            curl->start(QStringLiteral("curl"),
                                        QStringList{
                                            QStringLiteral("-4"), // Show IPv4
                                            QStringLiteral("--silent"),
                                            QStringLiteral("--max-time"), QStringLiteral("5"),
                                            QStringLiteral("ifconfig.me")
                                        });
                            if (!curl->waitForStarted(1000))
                            {
                                curl->deleteLater();
                                emit connectionStateChanged(m_state, serverName);
                            }
                        });
                nmcli->start(QStringLiteral("nmcli"),
                             QStringList{
                                 QStringLiteral("connection"),
                                 QStringLiteral("show"),
                                 QStringLiteral("--active")
                             });
                // If nmcli is not installed, the process will fail to start — handle
                // that by emitting immediately with no info string.
                if (!nmcli->waitForStarted(1000))
                {
                    nmcli->deleteLater();
                    emit connectionStateChanged(m_state, QString());
                }
            });
    process->start(QStringLiteral("ip"), QStringList{QStringLiteral("a")});
}

QMap<QString, QString> VpnManager::parseDictOutput(const QString& output)
{
    QMap<QString, QString> result;
    // Parse Python-dict-like output: {'key': 'value', 'key2': None, ...}
    const QRegularExpression re(QStringLiteral("'([^']+)'\\s*:\\s*(?:'([^']*)'|(None|True|False|[\\d.]+))"));
    QRegularExpressionMatchIterator it = re.globalMatch(output);
    while (it.hasNext())
    {
        auto match = it.next();
        QString key = match.captured(1);
        QString val = match.captured(2).isEmpty() ? match.captured(3) : match.captured(2);
        result.insert(key, val);
    }
    return result;
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

