#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QVBoxLayout>
#include <QTimer>
#include <QMap>
#include <QTabWidget>
#include "../vpnmanager.h"

// ---------------------------------------------------------------------------
// ToggleSwitch – animated on/off switch
// ---------------------------------------------------------------------------
class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal knobPos READ knobPos WRITE setKnobPos)
public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    bool  isOn()    const { return m_on; }
    void  setOn(bool on, bool animate = true);

    qreal knobPos() const { return m_knobPos; }
    void  setKnobPos(qreal v) { m_knobPos = v; update(); }

    QSize sizeHint() const override { return {44, 24}; }

signals:
    void toggled(bool on);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

private:
    bool   m_on      = false;
    qreal  m_knobPos = 0.0;
    class QPropertyAnimation *m_anim;
};

// ---------------------------------------------------------------------------
// SettingsPage
// ---------------------------------------------------------------------------
class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(VpnManager *manager, QWidget *parent = nullptr);
    void refresh();

private:
    // A simple on/off toggle row
    struct ToggleRow {
        QString       cliKey;
        ToggleSwitch *toggle   = nullptr;
        bool          needsReconnect = false;
    };
    // A combo-box row (multi-value setting)
    struct ComboRow {
        QString    cliKey;
        QComboBox *combo       = nullptr;
        QStringList cliValues; // parallel to combo items
        bool        needsReconnect = false;
    };

    VpnManager        *m_manager;
    QList<ToggleRow>   m_toggleRows;
    QList<ComboRow>    m_comboRows;

    // Custom DNS widgets
    ToggleSwitch      *m_dnsToggle      = nullptr;
    QLineEdit         *m_dnsEdit        = nullptr;
    QPushButton       *m_dnsApplyBtn    = nullptr;

    // Auto-start (systemd user service)
    ToggleSwitch      *m_autoStartToggle  = nullptr;
    QWidget           *m_autoStartRow     = nullptr;

    // Auto-connect on startup (shown only when auto-start is on)
    ToggleSwitch      *m_autoConnectToggle = nullptr;
    QWidget           *m_autoConnectRow    = nullptr;

    // Desktop notifications
    ToggleSwitch      *m_notificationsToggle = nullptr;

    QPushButton       *m_refreshBtn;
    QLabel            *m_statusLabel;
    QTimer            *m_spinnerTimer;
    int                m_spinnerFrame = 0;
    bool               m_loading      = false;

    // Helpers
    QWidget *makeToggleRow(QWidget *parent, const QString &label, const QString &desc,
                           const QString &cliKey, bool needsReconnect);
    QWidget *makeComboRow(QWidget *parent, const QString &label, const QString &desc,
                          const QString &cliKey, const QStringList &labels,
                          const QStringList &cliValues, bool needsReconnect);
    void addDivider(QVBoxLayout *layout, QWidget *parent);
    void maybeWarnReconnect(bool needsReconnect);

    // Auto-start helpers
    static QString serviceFilePath();
    static bool    systemdAvailable();
    static bool    autoStartEnabled();
    static bool    setAutoStart(bool enable, QString &errorOut);
    void           updateAutoConnectRowVisibility();

    void onSettingsReady(const QMap<QString, QString> &settings);
    void showAboutDialog();
    void setLoading(bool loading);
};
