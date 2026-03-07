#pragma once

#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QPropertyAnimation>
#include <QDialog>
#include <QPlainTextEdit>
#include <QClipboard>
#include <QGuiApplication>
#include "../vpnmanager.h"

// ---------------------------------------------------------------------------
// PowerButton – circular power-icon button with an animated ring
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

    void setSpinAngle(qreal a)
    {
        m_spinAngle = a;
        update();
    }

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
// LocationPicker – custom styled dropdown replacing QComboBox
// ---------------------------------------------------------------------------
class LocationPicker : public QFrame
{
    Q_OBJECT
public:
    explicit LocationPicker(const QString& countryCode, QWidget* parent = nullptr);

    // Populate with cities; call once after fetchCities() responds.
    // Each entry: (cityName, featureString).  Empty cityName = "Fastest server".
    void populate(const QList<QPair<QString, QString>>& cities);
    void setLoading(bool loading);

    // Currently selected city string (empty = fastest server / no --city flag).
    [[nodiscard]] QString selectedCity() const { return m_selectedCity; }

signals:
    void selectionChanged(const QString& city);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    void togglePopup();
    void closePopup();
    void onItemClicked(QListWidgetItem* item);
    void updateHeader();
    void resizeList();

    QString m_countryCode;
    QString m_selectedCity;       // empty = fastest server

    QLabel*      m_flagLabel;
    QLabel*      m_topLine;       // "Selected Location"
    QLabel*      m_bottomLine;    // city name or "Fastest server"
    QLabel*      m_chevron;
    QFrame*      m_popup;
    QListWidget* m_list;
    QTimer*      m_loadingTimer = nullptr;
    int          m_loadingFrame = 0;
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

signals:
    void connectRequested(const QString& country, const QString& city);
    void disconnectRequested();

private slots:
    void onCitiesReady(const QString& countryCode, const QList<QPair<QString, QString>>& cities);

private:
    VpnManager* m_manager;
    QString m_localCountryCode;

    PowerButton*    m_powerBtn;
    QLabel*         m_statusLabel;
    QLabel*         m_infoLabel;
    QPushButton*    m_errorDetailsBtn;
    QLabel*         m_timerLabel;
    LocationPicker* m_locationPicker;
    QTimer*         m_elapsedTimer;
    QTimer*         m_checkingSpinnerTimer;
    int   m_elapsedSeconds = 0;
    int   m_checkingSpinnerFrame = 0;
    QString m_rawError;

    VpnState m_currentState = VpnState::Unknown;
    QString  m_activeCity;   // city of the current or in-progress connection

    void updateUi(VpnState state, const QString& info);
    void startElapsedTimer();
    void stopElapsedTimer() const;
    void showErrorDetails() const;
};
