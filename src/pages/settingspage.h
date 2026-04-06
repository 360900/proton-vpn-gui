#pragma once

#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QVBoxLayout>
#include <QTimer>
#include "../vpnmanager.h"

// ---------------------------------------------------------------------------
// ToggleSwitch – animated on/off switch
// ---------------------------------------------------------------------------
class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal knobPos READ knobPos WRITE setKnobPos)

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    [[nodiscard]] bool isOn() const { return m_on; }
    void setOn(bool on, bool animate = true);

    [[nodiscard]] qreal knobPos() const { return m_knobPos; }

    void setKnobPos(const qreal v)
    {
        m_knobPos = v;
        update();
    }

    [[nodiscard]] QSize sizeHint() const override { return {44, 24}; }

signals:
    void toggled(bool on);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    bool m_on = false;
    qreal m_knobPos = 0.0;
    class QPropertyAnimation* m_anim;
};

// ---------------------------------------------------------------------------
// SettingsPage
// ---------------------------------------------------------------------------
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(VpnManager* manager, QWidget* parent = nullptr);
    void refresh();

signals:
    void recentConnectionsCleared();

private:
    // A simple on/off toggle row
    struct ToggleRow
    {
        QString cliKey;
        ToggleSwitch* toggle = nullptr;
    };

    // A combo-box row (multi-value setting)
    struct ComboRow
    {
        QString cliKey;
        QComboBox* combo = nullptr;
        QStringList cliValues; // parallel to combo items
    };

    VpnManager* m_manager;
    QList<ToggleRow> m_toggleRows;
    QList<ComboRow> m_comboRows;

    // Custom DNS widgets
    ToggleSwitch* m_dnsToggle = nullptr;
    QLineEdit* m_dnsEdit = nullptr;
    QPushButton* m_dnsApplyBtn = nullptr;

    // Auto-start (systemd user service)
    ToggleSwitch* m_autoStartToggle = nullptr;
    QWidget* m_autoStartRow = nullptr;

    // Auto-connect on startup (shown only when auto-start is on)
    ToggleSwitch* m_autoConnectToggle = nullptr;
    QWidget* m_autoConnectRow = nullptr;

    // Desktop notifications
    ToggleSwitch* m_notificationsToggle = nullptr;

    // Recent connections count (0 = disabled)
    class NumberSpinner* m_recentConnectionsSpinBox = nullptr;

    // "Clear history" row – shown only when history is non-empty
    QWidget* m_clearRecentRow = nullptr;

    // Plus Members Only section (VPN tab)
    QWidget* m_plusSection  = nullptr;
    QWidget* m_plusDivider  = nullptr;

    // Plus Members Only section (App tab)
    QWidget* m_appPlusSection = nullptr;
    QWidget* m_appPlusDivider = nullptr;

    QPushButton* m_refreshBtn;
    QLabel* m_statusLabel;
    QTimer* m_spinnerTimer;
    int m_spinnerFrame = 0;
    bool m_loading = false;

    // Helpers
    QWidget* makeToggleRow(QWidget* parent, const QString& label, const QString& desc,
                           const QString& cliKey);
    QWidget* makeComboRow(QWidget* parent, const QString& label, const QString& desc,
                          const QString& cliKey, const QStringList& labels,
                          const QStringList& cliValues);
    static void addDivider(QVBoxLayout* layout, QWidget* parent);
    static QWidget* makePlusDivider(QWidget* parent);
    void updatePlusSectionState();
    void maybeWarnReconnect(const QString& cliOutput);

    // Auto-start helpers
    static QString serviceFilePath();
    static bool systemdAvailable();
    static bool autoStartEnabled();
    static bool setAutoStart(bool enable, QString& errorOut);
    void updateAutoConnectRowVisibility() const;

    void onSettingsReady(const QMap<QString, QString>& settings);
    void showAboutDialog();
    void setLoading(bool loading);
};
