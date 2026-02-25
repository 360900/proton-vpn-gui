#include "notinstalledpage.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSvgWidget>
#include <QDesktopServices>
#include <QUrl>

NotInstalledPage::NotInstalledPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    auto *icon = new QSvgWidget(QStringLiteral(":/assets/no-app-icon.svg"), this);
    icon->setFixedSize(96, 96);
    layout->addWidget(icon, 0, Qt::AlignCenter);

    auto *titleLabel = new QLabel(QStringLiteral("ProtonVPN CLI Not Found"), this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto *descLabel = new QLabel(
        QStringLiteral("The <b>protonvpn</b> command-line tool could not be found on your system.<br>"
                       "Please install it to use this application."),
        this);
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(descLabel);

    auto *installBtn = new QPushButton(QStringLiteral("View Installation Instructions"), this);
    installBtn->setObjectName(QStringLiteral("primaryButton"));
    installBtn->setCursor(Qt::PointingHandCursor);
    connect(installBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://protonvpn.com/support/linux-vpn-setup/")));
    });
    layout->addWidget(installBtn, 0, Qt::AlignCenter);
}

