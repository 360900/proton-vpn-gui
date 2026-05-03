#pragma once

#include <QDialog>
#include <QString>

// ---------------------------------------------------------------------------
// AboutDialog – shows app/CLI version info, disclaimer, and credits.
// ---------------------------------------------------------------------------
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    // installedCliVersion may be empty if the CLI version hasn't been read yet.
    explicit AboutDialog(const QString& installedCliVersion = QString(), QWidget* parent = nullptr);
};

