#pragma once

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include "../vpnmanager.h"

class AccountPage : public QWidget
{
    Q_OBJECT

public:
    explicit AccountPage(VpnManager* manager, QWidget* parent = nullptr);

    void refresh();

signals:
    void signOutRequested();

private:
    VpnManager* m_manager;
    QLabel* m_nameLabel;
    QPushButton* m_refreshBtn;
    QTimer* m_spinnerTimer;
    int m_spinnerFrame = 0;

    void onInfoReady(const QMap<QString, QString>& info) const;
};

