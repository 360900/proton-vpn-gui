#pragma once

#include <QAbstractButton>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMouseEvent>
#include <QVBoxLayout>

#include "elidalabel.h"

// ============================================================
// PickerBase – shared base for LocationPicker and RecentPicker.
//
// Provides:
//   • The floating Qt::Popup frame + QListWidget plumbing.
//   • installOnRowWidget() — recursively enables hover tracking.
//   • Row hover / leave event-filter logic.
//   • togglePopup() / closePopup() / resizeList() helpers.
// ============================================================
class PickerBase : public QFrame
{
    Q_OBJECT
public:
    static constexpr int kCollapsedPickerW = 48;
    static constexpr int kExpandedPickerW  = 260;

    explicit PickerBase(QWidget* parent = nullptr) : QFrame(parent) {}

    // Switches between compact icon-only (collapsed) and full label (expanded) mode.
    void setCollapsed(bool c)
    {
        m_collapsed = c;
        if (m_topLine)    m_topLine->setVisible(!c);
        if (m_bottomLine) m_bottomLine->setVisible(!c);
        if (m_chevron)    m_chevron->setVisible(!c && m_list && m_list->count() > 0);
        if (m_header) {
            if (auto* hl = qobject_cast<QHBoxLayout*>(m_header->layout())) {
                // Collapsed: 16 px each side of the 28 px icon fills the full 60 px button.
                // No inter-item spacing so hidden stretch items steal nothing.
                hl->setContentsMargins(10, c ? 10 : 8, 10, c ? 10 : 8);
                hl->setSpacing(c ? 0 : 10);
            }
        }
        setFixedWidth(c ? kCollapsedPickerW : kExpandedPickerW);
    }

protected:
    // Must be called by the subclass constructor after building the header.
    void initPopup()
    {
        m_popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
        m_popup->setObjectName(QStringLiteral("locationPickerPopup"));

        QVBoxLayout* popupLayout = new QVBoxLayout(m_popup);
        popupLayout->setContentsMargins(0, 0, 0, 0);
        popupLayout->setSpacing(0);

        m_list = new QListWidget(m_popup);
        m_list->setObjectName(QStringLiteral("locationPickerList"));
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_list->setCursor(Qt::PointingHandCursor);
        m_list->setMouseTracking(true);
        m_list->viewport()->setMouseTracking(true);
        m_list->viewport()->installEventFilter(this);
        popupLayout->addWidget(m_list);

        m_popup->installEventFilter(this);
    }

    void togglePopup() const
    {
        if (m_list->count() == 0) return;
        if (m_popup->isVisible()) { closePopup(); return; }
        resizeList();
        const QPoint globalBottomLeft = mapToGlobal(QPoint(0, height()));
        // Always open popup at full expanded width so it's readable even when collapsed.
        m_popup->setFixedWidth(qMax(width(), kExpandedPickerW));
        m_popup->move(globalBottomLeft);
        m_popup->show();
        if (m_chevron != nullptr) m_chevron->setText(QStringLiteral("▴"));
    }

    void closePopup() const
    {
        m_popup->hide();
        if (m_chevron != nullptr) m_chevron->setText(QStringLiteral("▾"));
    }

    void resizeList() const
    {
        const int count = m_list->count();
        if (count == 0) return;
        const int rowH = m_list->sizeHintForRow(0);
        const int listH = qMin(count, 8) * rowH + 2;
        m_list->setFixedHeight(listH);
        m_popup->setFixedHeight(listH);
    }

    void installOnRowWidget(QWidget* w)
    {
        // Skip buttons so they can handle their own click events (e.g. star buttons).
        if (qobject_cast<QAbstractButton*>(w)) return;
        w->setMouseTracking(true);
        w->installEventFilter(this);
        for (QObject* child : w->children())
            if (QWidget* cw = qobject_cast<QWidget*>(child))
                installOnRowWidget(cw);
    }

    // Handles:
    //   • Popup Hide → reset chevron
    //   • Header click → togglePopup
    //   • Row Enter/Leave/Click → hover highlight + item selection callback
    //
    // Subclasses override eventFilter, call this first, and if it returns false
    // they handle any extra cases themselves.
    bool handleCommonEvents(QObject* obj, QEvent* ev)
    {
        // Popup hidden by Qt (outside click auto-dismiss) → reset chevron
        if (obj == m_popup && ev->type() == QEvent::Hide)
        {
            if (m_chevron != nullptr) m_chevron->setText(QStringLiteral("▾"));
            return false;
        }

        // Header click → toggle
        if (obj->isWidgetType() && ev->type() == QEvent::MouseButtonRelease)
        {
            const QWidget* w = dynamic_cast<QWidget*>(obj);
            if (w->objectName() == QLatin1String("locationPickerHeader"))
            {
                const QWidget* p = w;
                while (p != nullptr)
                {
                    if (p == this) { togglePopup(); return true; }
                    p = p->parentWidget();
                }
            }
        }

        // Row hover / click
        if (QWidget* w = qobject_cast<QWidget*>(obj))
        {
            QWidget* rowRoot = nullptr;
            QWidget* cur = w;
            while (cur != nullptr)
            {
                if (cur->parent() == m_list->viewport()) { rowRoot = cur; break; }
                cur = qobject_cast<QWidget*>(cur->parent());
            }

            if (rowRoot != nullptr)
            {
                if (ev->type() == QEvent::Enter || ev->type() == QEvent::MouseMove)
                {
                    for (QObject* child : m_list->viewport()->children())
                    {
                        QWidget* cw = qobject_cast<QWidget*>(child);
                        if (cw != nullptr && cw != rowRoot)
                        {
                            cw->setStyleSheet(QStringLiteral("background-color: transparent;"));
                        }
                    }
                    rowRoot->setStyleSheet(QStringLiteral("background-color: #2d2d4a;"));
                    return false;
                }
                if (ev->type() == QEvent::Leave)
                {
                    const QPoint globalPos = QCursor::pos();
                    if (rowRoot->rect().contains(rowRoot->mapFromGlobal(globalPos)) == false)
                    {
                        rowRoot->setStyleSheet(QStringLiteral("background-color: transparent;"));
                    }
                    return false;
                }
                if (ev->type() == QEvent::MouseButtonRelease)
                {
                    const QPoint vp = m_list->viewport()->mapFromGlobal(
                        w->mapToGlobal(dynamic_cast<QMouseEvent*>(ev)->pos()));
                    QListWidgetItem* item = m_list->itemAt(vp);
                    if (item != nullptr)
                    {
                        onRowClicked(item); return true;
                    }
                }
            }
        }
        return false;
    }

    // Subclasses implement this to react to a row click.
    virtual void onRowClicked(QListWidgetItem* item) = 0;

    QFrame*      m_popup   = nullptr;
    QListWidget* m_list    = nullptr;
    QLabel*      m_chevron = nullptr;
    QFrame*      m_header  = nullptr; // set by each subclass after building the header widget

    ElideLabel*  m_topLine    = nullptr;
    ElideLabel*  m_bottomLine = nullptr;
    bool         m_collapsed  = false;
};

