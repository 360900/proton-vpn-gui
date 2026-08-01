// cliParsers.cpp
// See cliParsers.h. Every function here is pure and fixture-tested.

#include "cliParsers.h"

#include <QRegularExpression>
#include <ranges>

namespace
{
// Rows of the CLI's list tables need at least a name and a code/features
// column to be meaningful.
constexpr int MIN_TABLE_COLUMNS = 2;

// Table rows are separated from their header by a line of dashes; columns
// are aligned with runs of two or more spaces.
const QRegularExpression& columnSeparator()
{
    static const QRegularExpression re(QStringLiteral(R"(\s{2,})"));
    return re;
}

// Split a table body (everything after the "--" separator line) into rows of
// column values. Shared by the countries and cities parsers.
QList<QStringList> parseTableRows(const QString& text)
{
    QList<QStringList> rows;
    bool pastSeparator = false;
    for (const QString& line : text.split(QLatin1Char('\n')))
    {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
        {
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("--")))
        {
            pastSeparator = true;
            continue;
        }
        if (pastSeparator == false)
        {
            continue;
        }
        const QStringList parts = trimmed.split(columnSeparator(), Qt::SkipEmptyParts);
        if (parts.isEmpty() == false)
        {
            rows.append(parts);
        }
    }
    return rows;
}
} // namespace

namespace CliParsers
{

QStringList stripNoiseLines(const QString& text)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    lines.erase(std::ranges::remove_if(lines, [](const QString& l)
    {
        const QString ll = l.toLower();
        return ll.contains(QLatin1String("outdated"))                   ||
               ll.contains(QLatin1String("updating"))                   ||
               ll.contains(QLatin1String("this may take"))              ||
               ll.contains(QLatin1String("to get your forwarded port")) ||
               ll.contains(QLatin1String("natpmpc"))                    ||
               ll.contains(QLatin1String("eventlet"))                  ||
               ll.contains(QLatin1String("sentry_sdk/utils.py"))        ||
               ll.contains(QLatin1String("bugfix mode"))               ||
               ll.contains(QLatin1String("strongly recommend against")) ||
               ll.contains(QLatin1String("eventlet.readthedocs"))       ||
               (ll.startsWith(QLatin1String("guide:")) &&
                ll.contains(QLatin1String("http")));
    }).begin(), lines.end());
    return lines;
}

QString stripNoise(const QString& text)
{
    QStringList lines = stripNoiseLines(text);
    while (lines.isEmpty() == false && lines.first().trimmed().isEmpty())
    {
        lines.removeFirst();
    }
    return lines.join(QLatin1Char('\n'));
}

QMap<QString, QString> parseKeyValueFields(const QString& text)
{
    QMap<QString, QString> fields;
    for (const QString& line : stripNoiseLines(text))
    {
        const int colonPos = line.indexOf(QLatin1Char(':'));
        if (colonPos < 0)
        {
            continue;
        }
        const QString key   = line.left(colonPos).trimmed().toLower();
        const QString value = line.mid(colonPos + 1).trimmed();
        if (key.isEmpty() == false && value.isEmpty() == false)
        {
            fields.insert(key, value);
        }
    }
    return fields;
}

StatusSnapshot parseStatus(const QString& text)
{
    StatusSnapshot snapshot;
    snapshot.raw = parseKeyValueFields(text);

    const QString statusVal = snapshot.raw.value(QStringLiteral("status")).toLower();
    snapshot.state = (statusVal == QStringLiteral("connected"))
                     ? VpnState::Connected
                     : VpnState::Disconnected;
    if (snapshot.state == VpnState::Connected)
    {
        snapshot.server = snapshot.raw.value(QStringLiteral("server"));
    }
    return snapshot;
}

ServerInfo parseServerInfo(const QString& server)
{
    ServerInfo info;
    if (server.isEmpty())
    {
        return info;
    }

    // Country code: leading characters up to the first '-' or '#',
    // e.g. "US" from "US-NJ#203 ..." or "CH" from "CH#7 ...".
    const int dashPos = server.indexOf(QLatin1Char('-'));
    const int hashPos = server.indexOf(QLatin1Char('#'));
    const int endPos  = (dashPos >= 0 && (hashPos < 0 || dashPos < hashPos))
                        ? dashPos : hashPos;
    if (endPos > 0)
    {
        info.countryCode = server.left(endPos).toUpper();
    }

    // City: the segment between " in " and the following comma,
    // e.g. "Secaucus" from "US-NJ#203 in Secaucus, United States".
    const int inPos = server.indexOf(QStringLiteral(" in "));
    if (inPos >= 0)
    {
        const QString rest     = server.mid(inPos + 4);
        const int      commaPos = rest.indexOf(QLatin1Char(','));
        info.city = (commaPos >= 0 ? rest.left(commaPos) : rest).trimmed();
    }
    return info;
}

QList<Country> parseCountriesTable(const QString& text)
{
    QList<Country> countries;
    for (const QStringList& parts : parseTableRows(text))
    {
        if (parts.size() < MIN_TABLE_COLUMNS)
        {
            continue;
        }
        Country c{.name = parts.first().trimmed(), .code = parts.last().trimmed()};
        if (c.name.isEmpty() == false && c.code.isEmpty() == false)
        {
            countries.append(c);
        }
    }
    return countries;
}

QList<City> parseCitiesTable(const QString& text)
{
    QList<City> cities;
    for (const QStringList& parts : parseTableRows(text))
    {
        City c{.name = parts.value(0).trimmed(), .features = parts.value(1).trimmed()};
        if (c.name.isEmpty() == false)
        {
            cities.append(c);
        }
    }
    return cities;
}

QMap<QString, QString> parseInfoMap(const QString& text)
{
    QMap<QString, QString> result;
    static const QRegularExpression re(QStringLiteral(R"((\w[\w ]*):\s*'([^']*)')"));
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        result.insert(m.captured(1).trimmed(), m.captured(2).trimmed());
    }
    return result;
}

LoginResult parseLoginOutput(const int exitCode, const QString& combined)
{
    LoginResult result;
    result.crash      = combined.contains(QStringLiteral("Traceback"));
    result.authFailed = combined.contains(QStringLiteral("401"));
    result.ok         = exitCode == 0 &&
                        combined.contains(QStringLiteral("error"), Qt::CaseInsensitive) == false;
    if (result.ok)
    {
        return result;
    }

    // Reconstruct a human-readable error: drop prompts, warnings, and Python
    // traceback internals, keeping only top-level message lines.
    QStringList errorLines;
    for (const QString& line : combined.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
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
    result.errorText = errorLines.isEmpty() ? combined.trimmed()
                                            : errorLines.join(QLatin1Char('\n'));
    return result;
}

AccountType parseAccountTier(const QString& combined)
{
    return combined.contains(QStringLiteral("To upgrade to VPN Plus"), Qt::CaseInsensitive)
           ? AccountType::Free
           : AccountType::Plus;
}

QString parseCliVersion(const QString& combined)
{
    static const QRegularExpression re(QStringLiteral(R"(\b(\d+\.\d+\.\d+)\b)"));
    const QStringList lines = combined.split(QLatin1Char('\n'));
    for (const QString& line : std::ranges::reverse_view(lines))
    {
        const QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch())
        {
            return match.captured(1);
        }
    }
    return {};
}

QString vpnStateToString(const VpnState state)
{
    // Lowercase strings are part of the public D-Bus contract
    // (io.github._360900.ProtonVpnGui.Status, consumed by e.g. waybar).
    switch (state)
    {
        case VpnState::Disconnected:
            return QStringLiteral("disconnected");
        case VpnState::Connecting:
            return QStringLiteral("connecting");
        case VpnState::Connected:
            return QStringLiteral("connected");
        case VpnState::Disconnecting:
            return QStringLiteral("disconnecting");
        case VpnState::Error:
            return QStringLiteral("error");
        default:
            return QStringLiteral("unknown");
    }
}

} // namespace CliParsers
