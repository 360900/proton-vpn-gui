#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace
{
constexpr int BANNER_LEFT_MARGIN = 12;
constexpr int BANNER_MARGIN      = 8;
constexpr int DISMISS_BTN_SIZE   = 22;
} // namespace

// ============================================================
// InfoBanner – a dismissable horizontal warning/info banner.
//
// Usage:
//   auto* banner = new InfoBanner("Message text", parent);
//   layout->insertWidget(0, banner);
//
// The banner uses the standard amber warning palette by default.
// Call dismiss() programmatically or connect to dismissed() signal.
// The widget deletes itself after being dismissed.
// ============================================================
class InfoBanner : public QFrame
{
    Q_OBJECT
public:
    explicit InfoBanner(const QString& htmlMessage, QWidget* parent = nullptr)
        : QFrame(parent)
    {
        static constexpr const char* kColor       = "#7a5c00";
        static constexpr const char* kBgColor     = "#fff3cd";
        static constexpr const char* kBorderColor = "#e6ac00";

        setStyleSheet(
            QStringLiteral(
                "QFrame { background-color: %1; border: 1px solid %2; border-radius: 6px; } "
                "QLabel { color: %3; background: transparent; }")
            .arg(QLatin1String(kBgColor),
                 QLatin1String(kBorderColor),
                 QLatin1String(kColor)));

        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(BANNER_LEFT_MARGIN, BANNER_MARGIN, BANNER_MARGIN, BANNER_MARGIN);
        layout->setSpacing(BANNER_MARGIN);

        QLabel* iconLabel = new QLabel(QStringLiteral("⚠"), this);
        iconLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
        layout->addWidget(iconLabel, 0, Qt::AlignTop);

        QLabel* msgLabel = new QLabel(htmlMessage, this);
        msgLabel->setWordWrap(true);
        msgLabel->setTextFormat(Qt::RichText);
        msgLabel->setOpenExternalLinks(true);
        msgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(msgLabel, 1);

        QPushButton* dismissBtn = new QPushButton(QStringLiteral("✕"), this);
        dismissBtn->setFixedSize(DISMISS_BTN_SIZE, DISMISS_BTN_SIZE);
        dismissBtn->setFlat(true);
        dismissBtn->setCursor(Qt::PointingHandCursor);
        dismissBtn->setStyleSheet(
            QStringLiteral("QPushButton { color: %1; font-weight: bold; border: none; "
                           "background: transparent; } QPushButton:hover { opacity: 0.7; }")
                .arg(QLatin1String(kColor)));
        connect(dismissBtn, &QPushButton::clicked, this, &InfoBanner::dismiss);
        layout->addWidget(dismissBtn, 0, Qt::AlignTop);
    }

signals:
    void dismissed();

public slots:
    void dismiss()
    {
        emit dismissed();
        setVisible(false);
        deleteLater();
    }
};
