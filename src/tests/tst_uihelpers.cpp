#include <QtTest/QtTest>
#include "uihelpers.h"

// Tests for the inline helpers in uihelpers.h.
// isOnString() is used throughout SettingsPage to interpret CLI on/off values.

class TstUiHelpers : public QObject
{
    Q_OBJECT

private slots:

    //  isOnString

    void isOnString_on_returnsTrue()
    {
        QVERIFY(isOnString(QStringLiteral("on")));
    }

    void isOnString_true_returnsTrue()
    {
        QVERIFY(isOnString(QStringLiteral("true")));
    }

    void isOnString_one_returnsTrue()
    {
        QVERIFY(isOnString(QStringLiteral("1")));
    }

    void isOnString_enabled_returnsTrue()
    {
        QVERIFY(isOnString(QStringLiteral("enabled")));
    }

    void isOnString_off_returnsFalse()
    {
        QVERIFY(!isOnString(QStringLiteral("off")));
    }

    void isOnString_false_returnsFalse()
    {
        QVERIFY(!isOnString(QStringLiteral("false")));
    }

    void isOnString_zero_returnsFalse()
    {
        QVERIFY(!isOnString(QStringLiteral("0")));
    }

    void isOnString_disabled_returnsFalse()
    {
        QVERIFY(!isOnString(QStringLiteral("disabled")));
    }

    void isOnString_emptyString_returnsFalse()
    {
        QVERIFY(!isOnString(QString()));
    }

    void isOnString_arbitraryString_returnsFalse()
    {
        QVERIFY(!isOnString(QStringLiteral("yes")));
        QVERIFY(!isOnString(QStringLiteral("ON")));    // case-sensitive
        QVERIFY(!isOnString(QStringLiteral("True")));  // case-sensitive
    }

    //  kSpinnerFrames sanity

    void spinnerFrames_countMatchesConstant()
    {
        // The spinner frame array size must equal kSpinnerFrameCount so any
        // frame-index loop using `% kSpinnerFrameCount` stays in bounds.
        QCOMPARE(static_cast<int>(std::size(kSpinnerFrames)), kSpinnerFrameCount);
    }

    //  kServerFeatures sanity

    void serverFeatures_allFieldsNonNull()
    {
        for (int i = 0; i < kServerFeatureCount; ++i)
        {
            QVERIFY(kServerFeatures[i].keyword  != nullptr);
            QVERIFY(kServerFeatures[i].resource != nullptr);
            QVERIFY(kServerFeatures[i].tooltip  != nullptr);
        }
    }

    void serverFeatures_countMatchesConstant()
    {
        QCOMPARE(static_cast<int>(std::size(kServerFeatures)), kServerFeatureCount);
    }
};

QTEST_MAIN(TstUiHelpers)
#include "tst_uihelpers.moc"

