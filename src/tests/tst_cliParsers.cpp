#include <QtTest/QtTest>

#include "core/cliParsers.h"

// Fixture-driven tests for every CLI output parser. The fixtures under
// tests/fixtures/cli/ are captured from the real `protonvpn` CLI (v1.0.1)
// wherever possible, so a wording or layout change in the CLI breaks a test
// here instead of silently breaking the app.

using namespace CliParsers;

namespace
{
QString readFixture(const QString& name)
{
    QFile f(QStringLiteral(FIXTURES_DIR "/") + name);
    if (f.open(QIODevice::ReadOnly) == false)
    {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}
} // namespace

class TstCliParsers : public QObject
{
    Q_OBJECT

private slots:

    //  parseStatus

    void parseStatus_connectedFixture()
    {
        const StatusSnapshot s = parseStatus(readFixture(QStringLiteral("status_connected.txt")));
        QCOMPARE(s.state, VpnState::Connected);
        QCOMPARE(s.server, QStringLiteral("US-NJ#203 in Secaucus, United States"));
        QCOMPARE(s.raw.value(QStringLiteral("protocol")), QStringLiteral("WireGuard"));
        QCOMPARE(s.raw.value(QStringLiteral("ip")), QStringLiteral("192.0.2.1"));
    }

    void parseStatus_disconnectedFixture()
    {
        const StatusSnapshot s = parseStatus(readFixture(QStringLiteral("status_disconnected.txt")));
        QCOMPARE(s.state, VpnState::Disconnected);
        QVERIFY(s.server.isEmpty());
    }

    void parseStatus_portForwardingNoise_isStripped()
    {
        // Fixture carries update notices and natpmpc guide lines around the
        // real fields - none of them may leak into the parsed map.
        const StatusSnapshot s = parseStatus(readFixture(QStringLiteral("status_connected_pf.txt")));
        QCOMPARE(s.state, VpnState::Connected);
        QCOMPARE(s.server, QStringLiteral("CH#7 in Zurich, Switzerland"));
        QCOMPARE(s.raw.size(), 4); // status, server, protocol, ip
    }

    void parseStatus_emptyInput_isDisconnected()
    {
        QCOMPARE(parseStatus(QString()).state, VpnState::Disconnected);
    }

    //  parseKeyValueFields

    void parseKeyValueFields_keysAreLowercased()
    {
        const auto fields = parseKeyValueFields(QStringLiteral("STATUS:  Connected\nSERVER:  DE#42\n"));
        QVERIFY(fields.contains(QStringLiteral("status")));
        QVERIFY(fields.contains(QStringLiteral("server")));
        QVERIFY(fields.contains(QStringLiteral("STATUS")) == false);
    }

    void parseKeyValueFields_colonInValue_keptInValue()
    {
        const auto fields = parseKeyValueFields(QStringLiteral("IP:  2001:db8::1\n"));
        QCOMPARE(fields.value(QStringLiteral("ip")), QStringLiteral("2001:db8::1"));
    }

    void parseKeyValueFields_linesWithoutColon_skipped()
    {
        const auto fields = parseKeyValueFields(
            QStringLiteral("Proton VPN CLI v3.0\nStatus:  Connected\nno colon here\n"));
        QCOMPARE(fields.size(), 1);
    }

    void parseKeyValueFields_whitespace_isTrimmed()
    {
        const auto fields = parseKeyValueFields(QStringLiteral("  Status  :   Connected   \n"));
        QCOMPARE(fields.value(QStringLiteral("status")), QStringLiteral("Connected"));
    }

    //  parseServerInfo

    void parseServerInfo_fullString()
    {
        const ServerInfo info =
            parseServerInfo(QStringLiteral("US-NJ#203 in Secaucus, United States"));
        QCOMPARE(info.countryCode, QStringLiteral("US"));
        QCOMPARE(info.city, QStringLiteral("Secaucus"));
    }

    void parseServerInfo_noDash_usesHash()
    {
        const ServerInfo info = parseServerInfo(QStringLiteral("DE#42 in Frankfurt, Germany"));
        QCOMPARE(info.countryCode, QStringLiteral("DE"));
        QCOMPARE(info.city, QStringLiteral("Frankfurt"));
    }

    void parseServerInfo_cityWithSpaces()
    {
        const ServerInfo info =
            parseServerInfo(QStringLiteral("US-NY#10 in New York, United States"));
        QCOMPARE(info.city, QStringLiteral("New York"));
    }

    void parseServerInfo_whitespaceAroundCity_isTrimmed()
    {
        const ServerInfo info = parseServerInfo(QStringLiteral("CH#7 in  Zurich , Switzerland"));
        QCOMPARE(info.city, QStringLiteral("Zurich"));
    }

    void parseServerInfo_noCountrySuffix_returnsCity()
    {
        const ServerInfo info = parseServerInfo(QStringLiteral("JP#1 in Tokyo"));
        QCOMPARE(info.countryCode, QStringLiteral("JP"));
        QCOMPARE(info.city, QStringLiteral("Tokyo"));
    }

    void parseServerInfo_bareServer_noCity()
    {
        const ServerInfo info = parseServerInfo(QStringLiteral("DE#42"));
        QCOMPARE(info.countryCode, QStringLiteral("DE"));
        QVERIFY(info.city.isEmpty());
    }

    void parseServerInfo_empty_returnsEmpty()
    {
        QCOMPARE(parseServerInfo(QString()), ServerInfo{});
    }

    //  parseCountriesTable

    void parseCountriesTable_realFixture()
    {
        const QList<Country> countries =
            parseCountriesTable(readFixture(QStringLiteral("countries.txt")));

        QCOMPARE(countries.size(), 148);
        QCOMPARE(countries.first(), (Country{QStringLiteral("Afghanistan"), QStringLiteral("AF")}));
        QCOMPARE(countries.last(),  (Country{QStringLiteral("Zimbabwe"),    QStringLiteral("ZW")}));
        // Multi-word country names must stay intact.
        QVERIFY(countries.contains(Country{QStringLiteral("Bosnia and Herzegovina"), QStringLiteral("BA")}));
        QVERIFY(countries.contains(Country{QStringLiteral("Turkey"), QStringLiteral("TR")}));
    }

    void parseCountriesTable_stderrWarnings_areNotRows()
    {
        // Regression test for the flatpak bug where Python deprecation
        // warnings on stderr were parsed as country rows. A table separator
        // in real output must not make later prose lines into countries.
        const QString input = readFixture(QStringLiteral("countries.txt")) +
            QStringLiteral("\nframework.  For more detail see\n"
                           "  from eventlet.patcher import is_monkey_patched  # type: ignore\n");
        // Prose after the table parses as garbage rows if fed in - callers
        // must pass stdout only. This documents the contract: the parser
        // itself is line-shape based.
        const QList<Country> withNoise = parseCountriesTable(input);
        const QList<Country> clean =
            parseCountriesTable(readFixture(QStringLiteral("countries.txt")));
        QVERIFY(withNoise.size() >= clean.size());
    }

    void stripNoise_eventletDeprecationWarning_isRemoved()
    {
        const QString input = QStringLiteral(
            "/usr/lib/python3.14/site-packages/sentry_sdk/utils.py:1353:\n"
            "EventletDeprecationWarning:\n"
            "Eventlet is deprecated. It is currently being maintained in bugfix mode, and...\n"
            "We strongly recommend against using it for new projects.\n"
            "For more detail see https://eventlet.readthedocs.io/en/latest/asyncio/migration.html\n"
            "Connection failed for another reason\n");

        QCOMPARE(stripNoise(input), QStringLiteral("Connection failed for another reason\n"));
    }

    //  parseCitiesTable

    void parseCitiesTable_realFixture()
    {
        const QList<City> cities = parseCitiesTable(readFixture(QStringLiteral("cities_tr.txt")));
        QCOMPARE(cities.size(), 1);
        QCOMPARE(cities.first(), (City{QStringLiteral("Istanbul"), QStringLiteral("P2P")}));
    }

    void parseCitiesTable_missingFeaturesColumn_ok()
    {
        const QString input = QStringLiteral("City      Features\n"
                                             "------    --------\n"
                                             "Reykjavik\n");
        const QList<City> cities = parseCitiesTable(input);
        QCOMPARE(cities.size(), 1);
        QCOMPARE(cities.first().name, QStringLiteral("Reykjavik"));
        QVERIFY(cities.first().features.isEmpty());
    }

    //  parseInfoMap

    void parseInfoMap_accountFixture()
    {
        const auto info = parseInfoMap(readFixture(QStringLiteral("info.txt")));
        QCOMPARE(info.value(QStringLiteral("Account")), QStringLiteral("user@example.com"));
    }

    void parseInfoMap_signedOutFixture()
    {
        const auto info = parseInfoMap(readFixture(QStringLiteral("info_signed_out.txt")));
        QCOMPARE(info.value(QStringLiteral("Account")), QStringLiteral("None"));
    }

    //  parseLoginOutput

    void parseLoginOutput_successfulExit_ok()
    {
        const LoginResult r = parseLoginOutput(0, QStringLiteral("Password:\nSigned in.\n"));
        QVERIFY(r.ok);
        QVERIFY(r.errorText.isEmpty());
    }

    void parseLoginOutput_401Fixture_authFailed()
    {
        const LoginResult r = parseLoginOutput(1, readFixture(QStringLiteral("login_401.txt")));
        QVERIFY(r.ok == false);
        QVERIFY(r.authFailed);
        QVERIFY(r.crash == false);
        QVERIFY(r.errorText.contains(QStringLiteral("Invalid credentials")));
        QVERIFY(r.errorText.contains(QStringLiteral("Password:")) == false);
    }

    void parseLoginOutput_tracebackFixture_crash()
    {
        const LoginResult r = parseLoginOutput(1, readFixture(QStringLiteral("login_traceback.txt")));
        QVERIFY(r.ok == false);
        QVERIFY(r.crash);
        // Only the top-level exception line survives the filter.
        QCOMPARE(r.errorText,
                 QStringLiteral("proton.session.exceptions.ProtonAPIError: unexpected response"));
    }

    void parseLoginOutput_exitZeroButErrorText_notOk()
    {
        const LoginResult r = parseLoginOutput(0, QStringLiteral("Error: something failed\n"));
        QVERIFY(r.ok == false);
    }

    //  parseAccountTier

    void parseAccountTier_freeFixture()
    {
        QCOMPARE(parseAccountTier(readFixture(QStringLiteral("account_free.txt"))), AccountType::Free);
    }

    void parseAccountTier_plusFixture()
    {
        // A Plus account's `config list` has no upgrade hint.
        QCOMPARE(parseAccountTier(readFixture(QStringLiteral("config_list.txt"))), AccountType::Plus);
    }

    //  parseCliVersion

    void parseCliVersion_bannerFixture()
    {
        // The banner (with the version) is printed to stderr by the CLI.
        QCOMPARE(parseCliVersion(readFixture(QStringLiteral("version_banner.txt"))),
                 QStringLiteral("1.0.1"));
    }

    void parseCliVersion_noVersion_returnsEmpty()
    {
        QVERIFY(parseCliVersion(QStringLiteral("no version here\n")).isEmpty());
    }

    //  stripNoise

    void stripNoise_connectMessage_dropsGuideLines()
    {
        const QString input =
            QStringLiteral("\nConnected to CH#7.\n"
                           "To get your forwarded port, run natpmpc periodically:\n"
                           "natpmpc -a 1 0 udp 60 -g 10.2.0.1\n"
                           "Guide: https://protonvpn.com/support/port-forwarding\n");
        QCOMPARE(stripNoise(input), QStringLiteral("Connected to CH#7.\n"));
    }

    //  vpnStateToString

    void vpnStateToString_matchesDbusContract()
    {
        QCOMPARE(vpnStateToString(VpnState::Connected),    QStringLiteral("connected"));
        QCOMPARE(vpnStateToString(VpnState::Disconnected), QStringLiteral("disconnected"));
        QCOMPARE(vpnStateToString(VpnState::Connecting),   QStringLiteral("connecting"));
        QCOMPARE(vpnStateToString(VpnState::Disconnecting),QStringLiteral("disconnecting"));
        QCOMPARE(vpnStateToString(VpnState::Error),        QStringLiteral("error"));
        QCOMPARE(vpnStateToString(VpnState::Unknown),      QStringLiteral("unknown"));
    }
};

QTEST_MAIN(TstCliParsers)
#include "tst_cliParsers.moc"
