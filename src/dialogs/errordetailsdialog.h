#pragma once

#include <QDialog>
#include <QString>

// ---------------------------------------------------------------------------
// ErrorDetailsDialog – shows raw VPN/CLI error text with a copy-to-clipboard
// button.
// ---------------------------------------------------------------------------
class ErrorDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ErrorDetailsDialog(const QString& errorText, QWidget* parent = nullptr);
};

