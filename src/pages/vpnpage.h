#pragma once

#include <optional>
#include <QTimer>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QVersionNumber>
#include <QGraphicsDropShadowEffect>
#include "../vpnmanager.h"
#include "../cli/natpmpmanager.h"
#include "../widgets/pickerbase.h"
#include "../widgets/pickerdrawer.h"
#include "../widgets/infobanner.h"
#include "../widgets/flatpakbetabanner.h"
#include "../dialogs/errordetailsdialog.h"

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
class LocationPicker : public PickerBase
{
    Q_OBJECT
public:
    explicit LocationPicker(const QString& countryCode, const QString& countryName, QWidget* parent = nullptr);

    void populate(const QList<QPair<QString, QString>>& cities);
    void setLoading(bool loading);
    void setSelectedCity(const QString& city);
    // Tries to select city in the populated list.
    // Returns true if found; false if not found (falls back to "Active connection").
    bool trySelectCity(const QString& city);
    void setUnknownConnection(bool unknown);
    // Disables interaction for free-plan users (no popup, forbidden cursor, tooltip).
    void setFreeMode(bool free);

    [[nodiscard]] QString selectedCity() const { return m_selectedCity; }

signals:
    void selectionChanged(const QString& city);
    void changeCountryRequested();

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void onRowClicked(QListWidgetItem* item) override;

private:
    void updateHeader() const;

    QString m_countryCode;
    QString m_countryName;
    QString m_selectedCity;
    bool    m_unknownConnection = false;
    bool    m_freeMode = false;

    QLabel*  m_flagLabel;
    QTimer*  m_loadingTimer = nullptr;
    int      m_loadingFrame = 0;
};

// ---------------------------------------------------------------------------
// RecentPicker – shows recent connections as a dropdown
// ---------------------------------------------------------------------------
class RecentPicker : public PickerBase
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
    void onRowClicked(QListWidgetItem* item) override;
};

// ---------------------------------------------------------------------------
// FavoritesPicker – shows favorited connections as a dropdown
// ---------------------------------------------------------------------------
class FavoritesPicker : public PickerBase
{
    Q_OBJECT
public:
    explicit FavoritesPicker(QWidget* parent = nullptr);

    // Reload from FavoritesManager and rebuild popup list.
    void refresh();

signals:
    void connectionSelected(const QString& countryCode, const QString& city);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void onRowClicked(QListWidgetItem* item) override;
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
    void refreshRecentPicker() const;
    void refreshFavoritesPicker() const;
    // Called when VpnManager has parsed a city from `protonvpn status`.
    void onStatusCityKnown(const QString& city);
    // Shows or hides the "Selected Location" picker.
    void setLocationPickerVisible(bool visible);
    // Shows or hides the favorites picker (respects hasFavorites + setting).
    void setFavoritesDropdownVisible(bool visible);
    // Called when the favorites-enabled setting changes.
    void setFavoritesEnabled(bool enabled);
    // Returns true while the natpmpc keep-alive loop is running (port forwarding active).
    bool isPortForwardingActive() const { return m_natPmpManager != nullptr && m_natPmpManager->isRunning(); }
    NatPmpManager* natPmpManager() const { return m_natPmpManager; }

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
    bool m_isFreeUser = false;

    PowerButton*    m_powerBtn;
    QLabel*         m_statusLabel;
    QLabel*         m_infoLabel;
    QLabel*         m_signOutHintLabel;
    QPushButton*    m_errorDetailsBtn;
    QLabel*         m_timerLabel;
    LocationPicker* m_locationPicker;
    RecentPicker*     m_recentPicker     = nullptr;
    FavoritesPicker*  m_favoritesPicker  = nullptr;
    PickerDrawer*     m_drawer           = nullptr;
    QFrame*           m_drawerNotch      = nullptr;
    QLabel*           m_drawerNotchIcon  = nullptr;
    QVBoxLayout*      m_outerLayout      = nullptr;

    // Wide-mode layout widgets
    QWidget*          m_logoRow              = nullptr;
    QWidget*          m_topContentWidget     = nullptr;
    QScrollArea*      m_scrollArea           = nullptr;
    // Narrow-mode scroll offset wrapper - carries the kCollapsedW left margin so
    // only the scroll area is pushed right (logo/power remain full-width centred).
    QWidget*          m_scrollOffsetWidget   = nullptr;
    QVBoxLayout*      m_scrollOffsetLayout   = nullptr;
    QWidget*          m_narrowContent        = nullptr;
    QVBoxLayout*      m_narrowContentLayout  = nullptr;
    QWidget*          m_wideContent          = nullptr;
    QWidget*          m_pickerSidebar        = nullptr;
    QVBoxLayout*      m_pickerSidebarLayout  = nullptr;
    QWidget*          m_rightContent         = nullptr;
    QVBoxLayout*      m_rightContentLayout   = nullptr;
    bool              m_wideMode             = false;

    static constexpr int kWideThreshold = 700;
    static constexpr int kWideSidebarW  = 300;
    bool            m_showFavoritesDropdown = true; // cached from AppConfig
    InfoBanner*     m_versionBanner = nullptr;
    InfoBanner*     m_prereleaseBanner = nullptr;
    FlatpakBetaBanner* m_flatpakBetaBanner = nullptr;
    QTimer*         m_elapsedTimer;
    QTimer*         m_checkingSpinnerTimer;
    int   m_elapsedSeconds = 0;
    int   m_checkingSpinnerFrame = 0;
    QString m_rawError;

    // Port forwarding (NatPmpManager keep-alive loop)
    QWidget*        m_portRow       = nullptr;
    QLabel*         m_portLabel     = nullptr;
    NatPmpManager*  m_natPmpManager = nullptr;
    InfoBanner*     m_natpmpcBanner = nullptr;

    VpnState m_currentState = VpnState::Unknown;
    QString  m_activeCity;
    QString  m_connectedCountryCode; // country code of the currently connected server (e.g. "US")
    QString  m_pendingStatusCity; // city from protonvpn status, applied after cities populate
    QString  m_currentCityFeatures; // features string (e.g. "P2P") for the currently active city
    QString  m_lastConnectedInfo;   // base info text from the last Connected state change
    bool     m_hadUnknownConnection = false;
    bool     m_stateKnown = false;
    // nullopt = citiesReady has not yet fired for the local country;
    // empty list = fired but the CLI returned no cities.
    std::optional<QList<QPair<QString, QString>>> m_pendingCities;

    // kWideThreshold removed; threshold is now computed dynamically in relayoutPickers()

    void updateUi(VpnState state, const QString& info);
    void startElapsedTimer();
    void stopElapsedTimer() const;
    void showErrorDetails() const;
    void relayoutPickers(int width = 0) const; // delegates to drawer syncVisibility
    void applyWideMode(bool wide);
    void repositionDrawer();
    void repositionDrawerNotch(int drawerW);
    void updateDrawerNotchIcon();
    void checkPrereleaseBanner();
    void checkFlatpakBetaBanner();
    void applyFreeUserMode() const;
    void startNatPmpLoop();
    void stopNatPmpLoop();
    void showNatpmpcBanner();
    void refreshConnectedInfoLabel() const;
    // After populate(), try to select m_pendingStatusCity; falls back to
    void applyPendingStatusCity();
};
