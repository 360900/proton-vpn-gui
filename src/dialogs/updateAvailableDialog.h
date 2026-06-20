#pragma once

#include <QDialog>
#include <QString>

// Shown when a newer version of the app is found on startup.
class UpdateAvailableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateAvailableDialog(const QString& currentVersion,
                                   const QString& newVersion,
                                   QWidget* parent = nullptr);
};
