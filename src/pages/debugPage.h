#pragma once

#include <QWidget>
#include <QLabel>
#ifdef QT_DEBUG
#include <QLineEdit>
#endif

// ---------------------------------------------------------------------------
// DebugPage – only included in debug builds (QT_DEBUG).
// Provides developer-facing tools such as triggering dialogs manually and
// inspecting / resetting saved application settings.
// ---------------------------------------------------------------------------
class DebugPage : public QWidget
{
    Q_OBJECT

public:
    explicit DebugPage(QWidget* parent = nullptr);

private slots:
    void refreshValues() const;

private:
    // Value labels in the per-key settings grid (kept for live refresh)
    QLabel* m_valAutoConnect        = nullptr;
    QLabel* m_valNotifications      = nullptr;
    QLabel* m_valRecentCount        = nullptr;
    QLabel* m_valStartHidden        = nullptr;
    QLabel* m_valShowPicker         = nullptr;
    QLabel* m_valTheme              = nullptr;
    QLabel* m_valLastSeenVersion    = nullptr;

#ifdef QT_DEBUG
    QLineEdit* m_migrationVersionInput = nullptr;
#endif
};
