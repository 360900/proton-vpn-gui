#pragma once

#include <QCursor>
#include <QEvent>
#include <QFrame>
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
    explicit PickerBase(QWidget* parent = nullptr) : QFrame(parent) {}

protected:
    // Must be called by the subclass constructor after building the header.
    void initPopup()
    {
        m_popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
        m_popup->setObjectName(QStringLiteral("locationPickerPopup"));

        auto* popupLayout = new QVBoxLayout(m_popup);
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
        m_popup->setFixedWidth(width());
        m_popup->move(globalBottomLeft);
        m_popup->show();
        if (m_chevron) m_chevron->setText(QStringLiteral("▴"));
    }

    void closePopup() const
    {
        m_popup->hide();
        if (m_chevron) m_chevron->setText(QStringLiteral("▾"));
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
        w->setMouseTracking(true);
        w->installEventFilter(this);
        for (QObject* child : w->children())
            if (auto* cw = qobject_cast<QWidget*>(child))
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
            if (m_chevron) m_chevron->setText(QStringLiteral("▾"));
            return false;
        }

        // Header click → toggle
        if (obj->isWidgetType() && ev->type() == QEvent::MouseButtonRelease)
        {
            auto* w = dynamic_cast<QWidget*>(obj);
            if (w->objectName() == QLatin1String("locationPickerHeader"))
            {
                QWidget* p = w;
                while (p)
                {
                    if (p == this) { togglePopup(); return true; }
                    p = p->parentWidget();
                }
            }
        }

        // Row hover / click
        if (auto* w = qobject_cast<QWidget*>(obj))
        {
            QWidget* rowRoot = nullptr;
            QWidget* cur = w;
            while (cur)
            {
                if (cur->parent() == m_list->viewport()) { rowRoot = cur; break; }
                cur = qobject_cast<QWidget*>(cur->parent());
            }

            if (rowRoot)
            {
                if (ev->type() == QEvent::Enter || ev->type() == QEvent::MouseMove)
                {
                    for (QObject* child : m_list->viewport()->children())
                        if (auto* cw = qobject_cast<QWidget*>(child); cw && cw != rowRoot)
                            cw->setStyleSheet(QStringLiteral("background-color: transparent;"));
                    rowRoot->setStyleSheet(QStringLiteral("background-color: #2d2d4a;"));
                    return false;
                }
                if (ev->type() == QEvent::Leave)
                {
                    const QPoint globalPos = QCursor::pos();
                    if (!rowRoot->rect().contains(rowRoot->mapFromGlobal(globalPos)))
                        rowRoot->setStyleSheet(QStringLiteral("background-color: transparent;"));
                    return false;
                }
                if (ev->type() == QEvent::MouseButtonRelease)
                {
                    const QPoint vp = m_list->viewport()->mapFromGlobal(
                        w->mapToGlobal(dynamic_cast<QMouseEvent*>(ev)->pos()));
                    QListWidgetItem* item = m_list->itemAt(vp);
                    if (item) { onRowClicked(item); return true; }
                }
            }
        }
        return false;
    }

    // Subclasses implement this to react to a row click.
    virtual void onRowClicked(QListWidgetItem* item) = 0;

    QFrame*      m_popup  = nullptr;
    QListWidget* m_list   = nullptr;
    QLabel*      m_chevron = nullptr; // set by subclass before initPopup()

    ElideLabel*  m_topLine    = nullptr;
    ElideLabel*  m_bottomLine = nullptr;
};

