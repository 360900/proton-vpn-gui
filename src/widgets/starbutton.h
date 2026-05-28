#pragma once

// ---------------------------------------------------------------------------
// makeStarButton – creates a small QToolButton that shows an empty or filled
// star depending on whether (countryCode, city) is in FavoritesManager.
// Clicking the button toggles the favorite state.
// The button also auto-updates when FavoritesManager::changed() fires.
// ---------------------------------------------------------------------------

#include "../favoritesmanager.h"
#include "../geoutils.h"

#include <QCoreApplication>
#include <QIcon>
#include <QObject>
#include <QPixmap>
#include <QToolButton>

inline QToolButton* makeStarButton(const QString& countryCode,
                                    const QString& countryName,
                                    const QString& city,
                                    QWidget*        parent = nullptr)
{
    auto* btn = new QToolButton(parent);
    btn->setFixedSize(22, 22);
    btn->setIconSize(QSize(13, 13));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setAutoRaise(true);
    btn->setObjectName(QStringLiteral("starButton"));
    // Base + hover styles are defined in style.qss / style_light.qss via QToolButton#starButton

    // Lambda that refreshes the button icon + tooltip.
    auto updateIcon = [btn, countryCode, city]()
    {
        const bool fav = FavoritesManager::instance().isFavorite(countryCode, city);
        if (fav)
        {
            const QPixmap px = GeoUtils::svgPixmap(
                QStringLiteral(":/assets/star-fill.svg"), 13, QColor(0xFF, 0xD2, 0x4A));
            btn->setIcon(QIcon(px));
            btn->setToolTip(QCoreApplication::translate("StarButton", "Remove from Favorites"));
        }
        else
        {
            const QPixmap px = GeoUtils::svgPixmap(
                QStringLiteral(":/assets/star.svg"), 13, QColor(0x77, 0x77, 0x99));
            btn->setIcon(QIcon(px));
            btn->setToolTip(QCoreApplication::translate("StarButton", "Add to Favorites"));
        }
    };

    updateIcon(); // set initial state

    QObject::connect(btn, &QToolButton::clicked, [countryCode, countryName, city]()
    {
        FavoritesManager::instance().toggle(countryCode, countryName, city);
    });

    // Keep icon in sync with any global favorites change.
    QObject::connect(&FavoritesManager::instance(), &FavoritesManager::changed,
                     btn, updateIcon);

    return btn;
}

