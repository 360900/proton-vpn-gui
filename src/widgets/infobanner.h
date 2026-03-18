#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

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
        static const std::string kColour       = "#7a5c00";
        static const std::string kBgColour     = "#fff3cd";
        static const std::string kBorderColour = "#e6ac00";

        setStyleSheet(
            QStringLiteral(
                "QFrame { background-color: %1; border: 1px solid %2; border-radius: 6px; } "
                "QLabel { color: %3; background: transparent; }")
            .arg(QLatin1String(kBgColour),
                 QLatin1String(kBorderColour),
                 QLatin1String(kColour)));

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 8, 8, 8);
        layout->setSpacing(8);

        auto* iconLabel = new QLabel(QStringLiteral("⚠"), this);
        iconLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
        layout->addWidget(iconLabel, 0, Qt::AlignTop);

        auto* msgLabel = new QLabel(htmlMessage, this);
        msgLabel->setWordWrap(true);
        msgLabel->setTextFormat(Qt::RichText);
        msgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(msgLabel, 1);

        auto* dismissBtn = new QPushButton(QStringLiteral("✕"), this);
        dismissBtn->setFixedSize(22, 22);
        dismissBtn->setFlat(true);
        dismissBtn->setCursor(Qt::PointingHandCursor);
        dismissBtn->setStyleSheet(
            QStringLiteral("QPushButton { color: %1; font-weight: bold; border: none; "
                           "background: transparent; } QPushButton:hover { opacity: 0.7; }")
                .arg(QLatin1String(kColour)));
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

