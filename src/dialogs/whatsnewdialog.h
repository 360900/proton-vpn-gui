#pragma once

#include <QDialog>

// ---------------------------------------------------------------------------
// WhatsNewDialog – shown once per app version on first launch after an update.
// Displays a short "What's new" summary and a link to the GitHub release page.
// ---------------------------------------------------------------------------
class WhatsNewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WhatsNewDialog(const QString& version, QWidget* parent = nullptr);
};

