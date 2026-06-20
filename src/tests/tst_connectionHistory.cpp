#include <QtTest/QtTest>
#include <QStandardPaths>
#include "connectionHistory.h"
#include "appConfig.h"

// ConnectionHistory is a singleton. We use test mode paths so file I/O is
// isolated from the user's real data directory. All test methods operate on
// the shared singleton instance; each method calls clear() first so they are
// independent of execution order.

class TstConnectionHistory : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Ensure the history feature is enabled for all tests.
        AppConfig::instance().setRecentConnectionsCount(5);
        ConnectionHistory::instance().clear();
    }

    void cleanupTestCase()
    {
        ConnectionHistory::instance().clear();
        QStandardPaths::setTestModeEnabled(false);
    }

    //  Basic record
    void record_newEntry_appearsInEntries()
    {
        ConnectionHistory::instance().clear();
        ConnectionHistory::instance().record(
            QStringLiteral("US"), QStringLiteral("United States"), QString());

        const QList<ConnectionEntry> entries = ConnectionHistory::instance().entries();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().countryCode, QStringLiteral("US"));
        QCOMPARE(entries.first().countryName, QStringLiteral("United States"));
    }

    void record_multipleEntries_newestIsFirst()
    {
        ConnectionHistory::instance().clear();
        ConnectionHistory::instance().record(
            QStringLiteral("DE"), QStringLiteral("Germany"), QString());
        ConnectionHistory::instance().record(
            QStringLiteral("FR"), QStringLiteral("France"), QString());

        const QList<ConnectionEntry> entries = ConnectionHistory::instance().entries();
        QCOMPARE(entries.size(), 2);
        // FR was recorded last so it must be at index 0.
        QCOMPARE(entries.at(0).countryCode, QStringLiteral("FR"));
        QCOMPARE(entries.at(1).countryCode, QStringLiteral("DE"));
    }

    //  Deduplication
    void record_duplicateCountryAndCity_movesToFront()
    {
        ConnectionHistory::instance().clear();
        ConnectionHistory::instance().record(
            QStringLiteral("US"), QStringLiteral("United States"), QString());
        ConnectionHistory::instance().record(
            QStringLiteral("DE"), QStringLiteral("Germany"), QString());

        // Re-record US - it must jump back to position 0 without duplication.
        ConnectionHistory::instance().record(
            QStringLiteral("US"), QStringLiteral("United States"), QString());

        const QList<ConnectionEntry> entries = ConnectionHistory::instance().entries();
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).countryCode, QStringLiteral("US"));
    }

    void record_sameCountryDifferentCity_treatedAsSeparateEntry()
    {
        ConnectionHistory::instance().clear();
        ConnectionHistory::instance().record(
            QStringLiteral("US"), QStringLiteral("United States"), QStringLiteral("New York"));
        ConnectionHistory::instance().record(
            QStringLiteral("US"), QStringLiteral("United States"), QStringLiteral("Los Angeles"));

        const QList<ConnectionEntry> entries = ConnectionHistory::instance().entries();
        QCOMPARE(entries.size(), 2);
    }

    //  Capacity trimming
    void record_exceedsMaxCount_trimsOldest()
    {
        AppConfig::instance().setRecentConnectionsCount(3);
        ConnectionHistory::instance().clear();

        ConnectionHistory::instance().record(QStringLiteral("US"), QStringLiteral("United States"), QString());
        ConnectionHistory::instance().record(QStringLiteral("DE"), QStringLiteral("Germany"), QString());
        ConnectionHistory::instance().record(QStringLiteral("FR"), QStringLiteral("France"), QString());
        // This fourth entry should push "US" off the list.
        ConnectionHistory::instance().record(QStringLiteral("JP"), QStringLiteral("Japan"), QString());

        const QList<ConnectionEntry> entries = ConnectionHistory::instance().entries();
        QCOMPARE(entries.size(), 3);
        // US should have been trimmed.
        const bool hasUs = std::any_of(entries.begin(), entries.end(),
            [](const ConnectionEntry& e){ return e.countryCode == QStringLiteral("US"); });
        QVERIFY(!hasUs);

        // Restore
        AppConfig::instance().setRecentConnectionsCount(5);
    }

    //  Feature disabled (count = 0)
    void record_countIsZero_nothingStored()
    {
        AppConfig::instance().setRecentConnectionsCount(0);
        ConnectionHistory::instance().clear();

        ConnectionHistory::instance().record(
            QStringLiteral("US"), QStringLiteral("United States"), QString());

        QCOMPARE(ConnectionHistory::instance().entries().size(), 0);

        // Restore
        AppConfig::instance().setRecentConnectionsCount(5);
    }

    void entries_countIsZero_returnsEmpty()
    {
        AppConfig::instance().setRecentConnectionsCount(5);
        ConnectionHistory::instance().clear();
        ConnectionHistory::instance().record(
            QStringLiteral("US"), QStringLiteral("United States"), QString());

        AppConfig::instance().setRecentConnectionsCount(0);
        QCOMPARE(ConnectionHistory::instance().entries().size(), 0);

        // Restore
        AppConfig::instance().setRecentConnectionsCount(5);
    }

    //  clear()
    void clear_removesAllEntries()
    {
        ConnectionHistory::instance().record(
            QStringLiteral("US"), QStringLiteral("United States"), QString());
        ConnectionHistory::instance().clear();

        QCOMPARE(ConnectionHistory::instance().entries().size(), 0);
        QVERIFY(!ConnectionHistory::instance().hasAnyEntries());
    }

    //  hasAnyEntries
    void hasAnyEntries_afterRecord_isTrue()
    {
        ConnectionHistory::instance().clear();
        QVERIFY(!ConnectionHistory::instance().hasAnyEntries());

        ConnectionHistory::instance().record(
            QStringLiteral("US"), QStringLiteral("United States"), QString());
        QVERIFY(ConnectionHistory::instance().hasAnyEntries());
    }

    //  changed() signal
    void record_emitsChangedSignal()
    {
        ConnectionHistory::instance().clear();
        QSignalSpy spy(&ConnectionHistory::instance(), &ConnectionHistory::changed);
        ConnectionHistory::instance().record(
            QStringLiteral("US"), QStringLiteral("United States"), QString());
        QCOMPARE(spy.count(), 1);
    }

    void clear_emitsChangedSignal()
    {
        ConnectionHistory::instance().record(
            QStringLiteral("US"), QStringLiteral("United States"), QString());
        QSignalSpy spy(&ConnectionHistory::instance(), &ConnectionHistory::changed);
        ConnectionHistory::instance().clear();
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TstConnectionHistory)
#include "tst_connectionHistory.moc"

