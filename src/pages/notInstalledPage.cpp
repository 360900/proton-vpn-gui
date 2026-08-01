#include "notInstalledPage.h"

#include <QDesktopServices>
#include <QLabel>
#include <QPushButton>
#include <QSvgWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
constexpr int NOT_INSTALLED_LAYOUT_SPACING = 20;
constexpr int NOT_INSTALLED_ICON_SIZE      = 96;
constexpr int NOT_INSTALLED_TITLE_FONT_SIZE = 16;
} // namespace

NotInstalledPage::NotInstalledPage(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(NOT_INSTALLED_LAYOUT_SPACING);

    QSvgWidget* icon = new QSvgWidget(QStringLiteral(":/assets/no-app-icon.svg"), this);
    icon->setFixedSize(NOT_INSTALLED_ICON_SIZE, NOT_INSTALLED_ICON_SIZE);
    layout->addWidget(icon, 0, Qt::AlignCenter);

    QLabel* titleLabel = new QLabel(tr("Proton VPN CLI Not Found"), this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(NOT_INSTALLED_TITLE_FONT_SIZE);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    QLabel* descLabel = new QLabel(
        tr("The <b>protonvpn</b> command-line tool could not be found on your system.<br>"
            "Please install it to use this application."),
        this);
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(descLabel);

    QPushButton* installBtn = new QPushButton(tr("View Installation Instructions"), this);
    installBtn->setObjectName(QStringLiteral("primaryButton"));
    installBtn->setCursor(Qt::PointingHandCursor);
    connect(installBtn, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://protonvpn.com/support/linux-cli")));
    });
    layout->addWidget(installBtn, 0, Qt::AlignCenter);

    // Lets the user continue without restarting the app after installing the CLI.
    QPushButton* recheckBtn = new QPushButton(tr("Check Again"), this);
    recheckBtn->setCursor(Qt::PointingHandCursor);
    connect(recheckBtn, &QPushButton::clicked, this, &NotInstalledPage::recheckRequested);
    layout->addWidget(recheckBtn, 0, Qt::AlignCenter);
}
