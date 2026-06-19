#pragma once

#include <QDialog>

// Shown once per app version on first launch after an update.
class WhatsNewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WhatsNewDialog(const QString& version, QWidget* parent = nullptr);
};

