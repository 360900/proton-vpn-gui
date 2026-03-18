#pragma once

#include <QLabel>
#include <QResizeEvent>
#include <utility>

// ============================================================
// ElideLabel – a QLabel that elides its text with "…" at the
// right edge whenever it is too narrow to show the text in full.
// ============================================================
class ElideLabel : public QLabel
{
public:
    explicit ElideLabel(QString  text, QWidget* parent = nullptr)
        : QLabel(parent), m_fullText(std::move(text))
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setMinimumWidth(0);
        QLabel::setText(elided());
    }

    // Override setText so callers can use it normally.
    void setText(const QString& text)
    {
        m_fullText = text;
        QLabel::setText(elided());
    }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QLabel::resizeEvent(e);
        QLabel::setText(elided());
    }

private:
    [[nodiscard]] QString elided() const
    {
        return fontMetrics().elidedText(m_fullText, Qt::ElideRight, width() > 0 ? width() : 9999);
    }

    QString m_fullText;
};

