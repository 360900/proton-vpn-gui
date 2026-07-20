#pragma once

#include <QDialog>

// Shown from the tray "Quit" action when the VPN is active, letting the user
// choose whether to leave the VPN connected, disconnect first, or cancel.
class QuitDialog : public QDialog
{
    Q_OBJECT

public:
    // Custom exec() result for "disconnect then quit", distinct from the
    // QDialog::Accepted ("leave VPN on") / QDialog::Rejected ("cancel") the
    // Cancel and Leave-VPN-on buttons already produce.
    static constexpr int DisconnectResult = QDialog::Accepted + 1;

    explicit QuitDialog(bool portForwardingActive, QWidget* parent = nullptr);
};
