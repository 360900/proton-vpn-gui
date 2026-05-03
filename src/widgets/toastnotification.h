#pragma once

#include <QEvent>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QTimer>

// ============================================================
// ToastNotification – a floating success/info toast that
// auto-dismisses and can be manually closed.
//
// Usage:
//   ToastNotification::popup(this, "Recent connections cleared.");
//
// The toast attaches itself to the top-level window of the
// given anchor widget, positions at the bottom-centre, and
// deletes itself when dismissed.
// ============================================================
class ToastNotification : public QFrame
{
    Q_OBJECT
public:
    // Convenience factory – create & show in one call.
    static void popup(QWidget* anchor, const QString& message, const int durationMs = 3000)
    {
        // self-managing lifetime via deleteLater in dismiss()
        new ToastNotification(message, anchor, durationMs);
    }

    explicit ToastNotification(const QString& message, const QWidget* anchor,
                               const int durationMs = 3000)
        : QFrame(anchor ? anchor->window() : nullptr)
    {
        setObjectName(QStringLiteral("toastNotification"));

        auto* hLayout = new QHBoxLayout(this);
        hLayout->setContentsMargins(12, 8, 8, 8);
        hLayout->setSpacing(8);

        auto* iconLabel = new QLabel(QStringLiteral("✓"), this);
        iconLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold;"));
        hLayout->addWidget(iconLabel);

        auto* msgLabel = new QLabel(message, this);
        msgLabel->setWordWrap(false);
        hLayout->addWidget(msgLabel, 1);

        auto* closeBtn = new QPushButton(QStringLiteral("✕"), this);
        closeBtn->setObjectName(QStringLiteral("toastCloseBtn"));
        closeBtn->setFixedSize(20, 20);
        closeBtn->setFlat(true);
        closeBtn->setCursor(Qt::PointingHandCursor);
        connect(closeBtn, &QPushButton::clicked, this, &ToastNotification::dismiss);
        hLayout->addWidget(closeBtn);

        // Opacity effect for fade-out animation
        m_opacity = new QGraphicsOpacityEffect(this);
        m_opacity->setOpacity(1.0);
        setGraphicsEffect(m_opacity);

        m_fadeAnim = new QPropertyAnimation(m_opacity, "opacity", this);
        m_fadeAnim->setDuration(400);
        m_fadeAnim->setStartValue(1.0);
        m_fadeAnim->setEndValue(0.0);
        connect(m_fadeAnim, &QPropertyAnimation::finished, this, &QObject::deleteLater);

        // Auto-dismiss timer
        auto* autoTimer = new QTimer(this);
        autoTimer->setSingleShot(true);
        connect(autoTimer, &QTimer::timeout, this, &ToastNotification::dismiss);
        autoTimer->start(durationMs);

        // Track parent window resizes so we can reposition
        if (parentWidget())
            parentWidget()->installEventFilter(this);

        QFrame::show();
        raise();

        // Defer sizing/positioning so the layout has been resolved
        QTimer::singleShot(0, this, [this]() {
            adjustSize();
            reposition();
        });
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize)
            reposition();
        return QFrame::eventFilter(watched, event);
    }

public slots:
    void dismiss()
    {
        if (m_dismissed) return;
        m_dismissed = true;
        m_fadeAnim->start();
    }

private:
    void reposition()
    {
        if (!parentWidget()) return;
        const QSize ps = parentWidget()->size();
        move((ps.width() - width()) / 2, ps.height() - height() - 32);
    }

    QGraphicsOpacityEffect* m_opacity  = nullptr;
    QPropertyAnimation*     m_fadeAnim = nullptr;
    bool                    m_dismissed = false;
};

