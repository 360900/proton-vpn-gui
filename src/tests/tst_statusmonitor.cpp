#include <QtTest/QtTest>
#include "cli/statusmonitor.h"

// Tests cover the two public static parsing helpers:
//   StatusMonitor::parseStatusFields()
//   StatusMonitor::parseCityFromServer()
// These are pure-function text parsers that drive all VPN state decisions in
// the app, making them critical to correctness.

class TstStatusMonitor : public QObject
{
    Q_OBJECT

private slots:

    //  parseStatusFields

    void parseStatusFields_connectedOutput_returnsStatusAndServer()
    {
        const QString input =
            QStringLiteral("Status:  Connected\n"
                           "Server:  US-NJ#203 in Secaucus, United States\n"
                           "Country: United States\n"
                           "IP:      192.0.2.1\n");

        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);

        QCOMPARE(fields.value(QStringLiteral("status")),  QStringLiteral("Connected"));
        QCOMPARE(fields.value(QStringLiteral("server")),
                 QStringLiteral("US-NJ#203 in Secaucus, United States"));
        QCOMPARE(fields.value(QStringLiteral("country")), QStringLiteral("United States"));
        QCOMPARE(fields.value(QStringLiteral("ip")),      QStringLiteral("192.0.2.1"));
    }

    void parseStatusFields_disconnectedOutput_returnsDisconnectedStatus()
    {
        const QString input = QStringLiteral("Status:  Disconnected\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.value(QStringLiteral("status")), QStringLiteral("Disconnected"));
    }

    void parseStatusFields_keysAreLowercased()
    {
        // Keys must be stored in lowercase regardless of how the CLI capitalises them.
        const QString input = QStringLiteral("STATUS:  Connected\nSERVER:  DE#42\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QVERIFY(fields.contains(QStringLiteral("status")));
        QVERIFY(fields.contains(QStringLiteral("server")));
        // The original-case keys must NOT be present.
        QVERIFY(!fields.contains(QStringLiteral("STATUS")));
    }

    void parseStatusFields_valuesPreserveCase()
    {
        // Values are NOT normalised — they are kept exactly as output by the CLI.
        const QString input = QStringLiteral("Status:  Connected\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.value(QStringLiteral("status")), QStringLiteral("Connected"));
    }

    void parseStatusFields_leadingTrailingWhitespace_isTrimmed()
    {
        const QString input = QStringLiteral("  Status  :   Connected   \n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.value(QStringLiteral("status")), QStringLiteral("Connected"));
    }

    void parseStatusFields_linesWithoutColon_areSkipped()
    {
        const QString input =
            QStringLiteral("ProtonVPN CLI v3.0\n"
                           "Status:  Connected\n"
                           "Some random line without a colon\n");

        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.size(), 1);
        QVERIFY(fields.contains(QStringLiteral("status")));
    }

    void parseStatusFields_emptyInput_returnsEmptyMap()
    {
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(QString());
        QVERIFY(fields.isEmpty());
    }

    void parseStatusFields_noiseLines_areRemoved()
    {
        // Lines containing noise keywords must be stripped before parsing.
        const QString input =
            QStringLiteral("Status:  Connected\n"
                           "This version is outdated, please update.\n"
                           "Updating server list, this may take a moment...\n"
                           "Guide: https://protonvpn.com/support/port-forwarding\n"
                           "To get your forwarded port, run natpmpc.\n"
                           "natpmpc -a 1 0 udp 60\n"
                           "Server:  DE#42\n");

        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);

        // Only "status" and "server" should survive — all noise must be gone.
        QCOMPARE(fields.size(), 2);
        QVERIFY(fields.contains(QStringLiteral("status")));
        QVERIFY(fields.contains(QStringLiteral("server")));
    }

    void parseStatusFields_noiseKeyword_outdated_filtered()
    {
        const QString input =
            QStringLiteral("Note: This is outdated.\nStatus: Connected\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        // "Note" line contained "outdated" → must be removed; only status survives.
        QCOMPARE(fields.size(), 1);
    }

    void parseStatusFields_multipleValuesWithColonsInValue_parsedCorrectly()
    {
        // A value may itself contain a colon (e.g. an IPv6 address or timestamp).
        // Only the FIRST colon is the separator — the rest belongs to the value.
        const QString input =
            QStringLiteral("IP:  2001:db8::1\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.value(QStringLiteral("ip")), QStringLiteral("2001:db8::1"));
    }

    //  parseCityFromServer

    void parseCityFromServer_fullServerString_returnsCity()
    {
        const QString city =
            StatusMonitor::parseCityFromServer(
                QStringLiteral("US-NJ#203 in Secaucus, United States"));
        QCOMPARE(city, QStringLiteral("Secaucus"));
    }

    void parseCityFromServer_differentCity_returnsCorrectCity()
    {
        const QString city =
            StatusMonitor::parseCityFromServer(
                QStringLiteral("DE#42 in Frankfurt, Germany"));
        QCOMPARE(city, QStringLiteral("Frankfurt"));
    }

    void parseCityFromServer_noInKeyword_returnsEmpty()
    {
        const QString city = StatusMonitor::parseCityFromServer(QStringLiteral("DE#42"));
        QVERIFY(city.isEmpty());
    }

    void parseCityFromServer_emptyString_returnsEmpty()
    {
        QVERIFY(StatusMonitor::parseCityFromServer(QString()).isEmpty());
    }

    void parseCityFromServer_cityWithSpaces_returnsFullCity()
    {
        // "New York" has a space inside the city name — must be returned in full.
        const QString city =
            StatusMonitor::parseCityFromServer(
                QStringLiteral("US-NY#10 in New York, United States"));
        QCOMPARE(city, QStringLiteral("New York"));
    }

    void parseCityFromServer_whitespaceAroundCity_isTrimmed()
    {
        const QString city =
            StatusMonitor::parseCityFromServer(
                QStringLiteral("CH#7 in  Zurich , Switzerland"));
        QCOMPARE(city, QStringLiteral("Zurich"));
    }

    void parseCityFromServer_serverStringWithNoCountry_returnsCity()
    {
        // No comma → entire remainder after " in " is the city.
        const QString city =
            StatusMonitor::parseCityFromServer(QStringLiteral("JP#1 in Tokyo"));
        QCOMPARE(city, QStringLiteral("Tokyo"));
    }
};

QTEST_MAIN(TstStatusMonitor)
#include "tst_statusmonitor.moc"

