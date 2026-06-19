#pragma once

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include "pickerBase.h"

// ---------------------------------------------------------------------------
// PickerDrawer – animated left-side overlay drawer containing picker dropdowns.
//
// Accepts any three PickerBase-derived widgets (location, recent, favorites).
// The favorites picker may be nullptr.
// ---------------------------------------------------------------------------
class PickerDrawer : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(int drawerW READ drawerW WRITE setDrawerW)

public:
    static constexpr int COLLAPSED_DRAWER_WIDTH = 60;   // width when collapsed (icon-only)
    static constexpr int EXPANDED_DRAWER_WIDTH  = 292;  // 16px pad + 260px picker + 16px pad

    explicit PickerDrawer(PickerBase* loc, PickerBase* rec,
                          PickerBase* fav, QWidget* parent = nullptr);

    [[nodiscard]] int  drawerW() const { return width(); }
    void setDrawerW(int w);

    void toggle();
    [[nodiscard]] bool isExpanded() const { return m_expanded; }

    // Refresh which pickers are shown based on current state.
    void syncVisibility(bool isFreeUser, bool showFavDropdown, bool favEnabled);
    // Re-evaluate and emit pickerAvailabilityChanged.
    void notifyAvailability();
    // Remove pickers from this drawer's layout so they can be placed elsewhere.
    void releasePickers();
    // Re-add previously released pickers back into this drawer's layout.
    void reclaimPickers();
    // Returns true if at least one picker widget is currently not hidden.
    [[nodiscard]] bool hasAnyVisiblePicker() const;

signals:
    void drawerWidthChanged(int w);
    void pickerAvailabilityChanged(bool hasAny);

private:
    PickerBase*   m_locationPicker;
    PickerBase*   m_recentPicker;
    PickerBase*   m_favoritesPicker;   // may be nullptr
    QVBoxLayout*  m_contentLayout      = nullptr;
    QGraphicsDropShadowEffect* m_shadow = nullptr;
    bool          m_expanded           = false;
    // Set to true while pickers live outside the drawer (wide-mode sidebar).
    // Prevents syncVisibility / setAllCollapsed from resetting their collapsed state.
    bool          m_pickersReleased    = false;
    QPropertyAnimation* m_anim        = nullptr;

    void setAllCollapsed(bool c) const;
};
