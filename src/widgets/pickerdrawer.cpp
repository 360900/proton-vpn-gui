#include "pickerdrawer.h"
#include "../connectionhistory.h"
#include "../favoritesmanager.h"

#include <QColor>
#include <QEasingCurve>

PickerDrawer::PickerDrawer(PickerBase* loc, PickerBase* rec,
                           PickerBase* fav, QWidget* parent)
    : QFrame(parent), m_locationPicker(loc), m_recentPicker(rec), m_favoritesPicker(fav)
{
    setObjectName(QStringLiteral("pickerDrawer"));
    setAttribute(Qt::WA_StyledBackground);

    m_shadow = new QGraphicsDropShadowEffect(this);
    m_shadow->setBlurRadius(20);
    m_shadow->setOffset(6, 0);
    m_shadow->setColor(QColor(0, 0, 0, 130));
    m_shadow->setEnabled(false); // only enabled when expanded
    setGraphicsEffect(m_shadow);

    m_contentLayout = new QVBoxLayout(this);
    // SetNoConstraint: lets the drawer resize freely even when pickers are larger.
    m_contentLayout->setSizeConstraint(QLayout::SetNoConstraint);
    m_contentLayout->setContentsMargins(0, 20, 0, 20);
    m_contentLayout->setSpacing(8);

    // Re-parent pickers into the drawer
    m_locationPicker->setParent(this);
    m_recentPicker->setParent(this);
    if (m_favoritesPicker != nullptr)
    {
        m_favoritesPicker->setParent(this);
    }

    m_contentLayout->addWidget(m_locationPicker);
    m_contentLayout->setAlignment(m_locationPicker, Qt::AlignHCenter);
    m_contentLayout->addWidget(m_recentPicker);
    m_contentLayout->setAlignment(m_recentPicker, Qt::AlignHCenter);
    if (m_favoritesPicker != nullptr)
    {
        m_contentLayout->addWidget(m_favoritesPicker);
        m_contentLayout->setAlignment(m_favoritesPicker, Qt::AlignHCenter);
    }

    setAllCollapsed(true);
    setDrawerW(kCollapsedW);
}

void PickerDrawer::setDrawerW(int w)
{
    QWidget::setMinimumWidth(w);
    QWidget::setMaximumWidth(w);
    emit drawerWidthChanged(w);
}

void PickerDrawer::toggle()
{
    if (m_anim != nullptr)
    {
        m_anim->stop(); // triggers destroyed → m_anim = nullptr
    }

    const int fromW = m_expanded ? kExpandedW : kCollapsedW;
    const int toW   = m_expanded ? kCollapsedW : kExpandedW;
    m_expanded = !m_expanded;

    if (m_expanded == false)
    {
        // Collapsing: shrink content immediately, then slide background in.
        setAllCollapsed(true);
        if (m_shadow != nullptr)
        {
            m_shadow->setEnabled(false);
        }
    }

    m_anim = new QPropertyAnimation(this, "drawerW", this);
    m_anim->setStartValue(fromW);
    m_anim->setEndValue(toW);
    m_anim->setDuration(220);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    if (m_expanded == true)
    {
        // Expanding: reveal full content only after slide animation completes.
        connect(m_anim, &QPropertyAnimation::finished, this, [this]()
        {
            setAllCollapsed(false);
            if (m_shadow != nullptr)
            {
                m_shadow->setEnabled(true);
            }
        });
    }

    connect(m_anim, &QPropertyAnimation::destroyed, this, [this]()
    {
        m_anim = nullptr;
    });

    m_anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void PickerDrawer::setAllCollapsed(bool c)
{
    // Do not touch collapse state while pickers live in the wide-mode sidebar.
    if (m_pickersReleased == true)
    {
        return;
    }

    m_locationPicker->setCollapsed(c);
    if (m_recentPicker->isVisible() == true)
    {
        m_recentPicker->setCollapsed(c);
    }
    if (m_favoritesPicker != nullptr && m_favoritesPicker->isVisible() == true)
    {
        m_favoritesPicker->setCollapsed(c);
    }

    m_contentLayout->setContentsMargins(c ? 0 : 16, 20, c ? 2 : 16, 20);
}

void PickerDrawer::syncVisibility(bool isFreeUser, bool showFavDropdown, bool favEnabled)
{
    const bool hasHistory   = (isFreeUser == false)
                              && (ConnectionHistory::instance().entries().isEmpty() == false);
    const bool hasFavorites = (isFreeUser == false) && (favEnabled == true)
                              && (showFavDropdown == true)
                              && FavoritesManager::instance().hasAnyEntries();

    m_recentPicker->setVisible(hasHistory);
    if (m_favoritesPicker != nullptr)
    {
        m_favoritesPicker->setVisible(hasFavorites);
    }

    // Do not override collapsed state when pickers live in the wide-mode sidebar.
    if (m_pickersReleased == false)
    {
        if (m_expanded == false)
        {
            if (hasHistory == true)
            {
                m_recentPicker->setCollapsed(true);
            }
            if (hasFavorites == true && m_favoritesPicker != nullptr)
            {
                m_favoritesPicker->setCollapsed(true);
            }
        }
        else
        {
            if (hasHistory == true)
            {
                m_recentPicker->setCollapsed(false);
            }
            if (hasFavorites == true && m_favoritesPicker != nullptr)
            {
                m_favoritesPicker->setCollapsed(false);
            }
        }
    }

    notifyAvailability();
}

bool PickerDrawer::hasAnyVisiblePicker() const
{
    // Use isHidden() so the check reflects each widget's own show/hide state,
    // independent of whether the drawer parent is currently hidden.
    if (m_locationPicker != nullptr && m_locationPicker->isHidden() == false)
    {
        return true;
    }
    if (m_recentPicker != nullptr && m_recentPicker->isHidden() == false)
    {
        return true;
    }
    if (m_favoritesPicker != nullptr && m_favoritesPicker->isHidden() == false)
    {
        return true;
    }
    return false;
}

void PickerDrawer::notifyAvailability()
{
    emit pickerAvailabilityChanged(hasAnyVisiblePicker());
}

void PickerDrawer::releasePickers()
{
    m_pickersReleased = true;
    m_contentLayout->removeWidget(m_locationPicker);
    m_contentLayout->removeWidget(m_recentPicker);
    if (m_favoritesPicker != nullptr)
    {
        m_contentLayout->removeWidget(m_favoritesPicker);
    }
}

void PickerDrawer::reclaimPickers()
{
    m_pickersReleased = false;
    m_contentLayout->addWidget(m_locationPicker);
    m_contentLayout->setAlignment(m_locationPicker, Qt::AlignHCenter);
    m_contentLayout->addWidget(m_recentPicker);
    m_contentLayout->setAlignment(m_recentPicker, Qt::AlignHCenter);
    if (m_favoritesPicker != nullptr)
    {
        m_contentLayout->addWidget(m_favoritesPicker);
        m_contentLayout->setAlignment(m_favoritesPicker, Qt::AlignHCenter);
    }
}

