#include "whatsnewdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QFrame>
#include <QDesktopServices>
#include <QUrl>

WhatsNewDialog::WhatsNewDialog(const QString& version, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("What's New in ProtonVPN Qt App"));
    setMinimumSize(500, 420);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 20, 24, 16);

    // ── Header ───────────────────────────────────────────────────────────────
    auto* titleLabel = new QLabel(
        QStringLiteral("<h2 style=\"margin-bottom:2px;\">%1</h2>")
            .arg(tr("What\u2019s New").toHtmlEscaped()),
        this);
    titleLabel->setTextFormat(Qt::RichText);
    layout->addWidget(titleLabel);

    auto* versionLabel = new QLabel(
        QStringLiteral("<span style=\"color:#888;\">%1</span>")
            .arg(tr("Version %1").arg(version).toHtmlEscaped()),
        this);
    versionLabel->setTextFormat(Qt::RichText);
    layout->addWidget(versionLabel);

    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setObjectName(QStringLiteral("sidebarDivider"));
    layout->addWidget(divider);

    // ── Release notes ────────────────────────────────────────────────────────
    auto* browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setHtml(
        QStringLiteral("<p>%1</p>")
        .arg(tr("Thanks for keeping ProtonVPN Qt App up to date! "
                "For the full list of changes, bug fixes, and new features, visit the release page on GitHub: "
                "<a href='https://github.com/wheat32/proton-vpn-qt-app/releases'>"
                "github.com/wheat32/proton-vpn-qt-app/releases</a>.")));
    layout->addWidget(browser, 1);

    // ── Buttons ──────────────────────────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    auto* releasePageBtn = new QPushButton(tr("View Release Notes"), this);
    releasePageBtn->setObjectName(QStringLiteral("secondaryButton"));
    connect(releasePageBtn, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/wheat32/proton-vpn-qt-app/releases")));
    });

    auto* closeBtn = new QPushButton(tr("Got It!"), this);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(releasePageBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);
}


