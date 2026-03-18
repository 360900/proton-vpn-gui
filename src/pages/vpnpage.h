#pragma once

#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QPropertyAnimation>
#include <QDialog>
#include <QPlainTextEdit>
#include <QClipboard>
#include <QGuiApplication>
#include <QVersionNumber>
#include <QHBoxLayout>
#include "../vpnmanager.h"

// Forward-declared here; defined as a file-local class in vpnpage.cpp.
class ElideLabel;

// ---------------------------------------------------------------------------
// PowerButton
// ---------------------------------------------------------------------------
class PowerButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal spinAngle READ spinAngle WRITE setSpinAngle)

public:
    explicit PowerButton(QWidget* parent = nullptr);
    enum class RingState { Unknown, Connected, Disconnected, Spinning };
    void setState(RingState s);
    [[nodiscard]] qreal spinAngle() const { return m_spinAngle; }
    void setSpinAngle(qreal a) { m_spinAngle = a; update(); }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    RingState m_state = RingState::Unknown;
    qreal m_spinAngle = 0.0;
    bool m_hovered = false;
    QPropertyAnimation* m_anim = nullptr;
    void startSpin() const;
    void stopSpin();
};

// ---------------------------------------------------------------------------
// LocationPicker – custom styled dropdown
// ---------------------------------------------------------------------------
class LocationPicker : public QFrame
{
    Q_OBJECT
public:
    explicit LocationPicker(const QString& countryCode, const QString& countryName, QWidget* parent = nullptr);

    void populate(const QList<QPair<QString, QString>>& cities);
    void setLoading(bool loading);
    void setSelectedCity(const QString& city);
    void setUnknownConnection(bool unknown);

    [[nodiscard]] QString selectedCity() const { return m_selectedCity; }

signals:
    void selectionChanged(const QString& city);
    void changeCountryRequested();

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    void togglePopup();
    void closePopup();
    void onItemClicked(QListWidgetItem* item);
    void updateHeader();
    void resizeList();
    void installOnRowWidget(QWidget* w);

    QString m_countryCode;
    QString m_countryName;
    QString m_selectedCity;
    bool    m_unknownConnection = false;

    QLabel*      m_flagLabel;
    ElideLabel*  m_topLine;
    ElideLabel*  m_bottomLine;
    QLabel*      m_chevron;
    QFrame*      m_popup;
    QListWidget* m_list;
    QTimer*      m_loadingTimer = nullptr;
    int          m_loadingFrame = 0;
};

// ---------------------------------------------------------------------------
// RecentPicker – shows recent connections as a dropdown
// ---------------------------------------------------------------------------
class RecentPicker : public QFrame
{
    Q_OBJECT
public:
    explicit RecentPicker(QWidget* parent = nullptr);

    // Reload from ConnectionHistory and rebuild popup list.
    void refresh();

signals:
    void connectionSelected(const QString& countryCode, const QString& city);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    void togglePopup();
    void closePopup();
    void resizeList();
    void installOnRowWidget(QWidget* w);

    ElideLabel*  m_topLine;
    ElideLabel*  m_bottomLine;
    QLabel*      m_chevron;
    QFrame*      m_popup;
    QListWidget* m_list;
};

// ---------------------------------------------------------------------------
// VpnPage
// ---------------------------------------------------------------------------
class VpnPage : public QWidget
{
    Q_OBJECT

public:
    explicit VpnPage(VpnManager* manager, QWidget* parent = nullptr);

    void onStateChanged(VpnState state, const QString& info);
    void notifyExternalConnect(const QString& city);

signals:
    void connectRequested(const QString& country, const QString& city);
    void disconnectRequested();
    void signOutRequested();
    void changeCountryRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onCitiesReady(const QString& countryCode, const QList<QPair<QString, QString>>& cities);
    void onCliVersionReady(const QString& version);

private:
    VpnManager* m_manager;
    QString m_localCountryCode;

    PowerButton*    m_powerBtn;
    QLabel*         m_statusLabel;
    QLabel*         m_infoLabel;
    QLabel*         m_signOutHintLabel;
    QPushButton*    m_errorDetailsBtn;
    QLabel*         m_timerLabel;
    LocationPicker* m_locationPicker;
    RecentPicker*   m_recentPicker = nullptr;
    QHBoxLayout*    m_pickerRow    = nullptr;   // holds both pickers side-by-side
    QFrame*         m_versionBanner = nullptr;
    QFrame*         m_prereleaseBanner = nullptr;
    QTimer*         m_elapsedTimer;
    QTimer*         m_checkingSpinnerTimer;
    int   m_elapsedSeconds = 0;
    int   m_checkingSpinnerFrame = 0;
    QString m_rawError;

    VpnState m_currentState = VpnState::Unknown;
    QString  m_activeCity;
    bool     m_hadUnknownConnection = false;
    bool     m_stateKnown = false;
    QList<QPair<QString, QString>> m_pendingCities;

    static constexpr int kWideThreshold = 580; // px

    void updateUi(VpnState state, const QString& info);
    void startElapsedTimer();
    void stopElapsedTimer() const;
    void showErrorDetails() const;
    void relayoutPickers(int width);
    void checkPrereleaseBanner();
};

