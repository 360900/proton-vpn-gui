#include "whatsnewdialog.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
constexpr int WHATS_NEW_MIN_WIDTH   = 480;
constexpr int WHATS_NEW_MIN_HEIGHT  = 280;
constexpr int WHATS_NEW_SPACING     = 12;
constexpr int WHATS_NEW_H_MARGIN    = 24;
constexpr int WHATS_NEW_TOP_MARGIN  = 20;
constexpr int WHATS_NEW_BOT_MARGIN  = 16;
constexpr int WHATS_NEW_BTN_SPACING = 8;
constexpr int BROWSER_STRETCH       = 1;
} // namespace

WhatsNewDialog::WhatsNewDialog(const QString& version, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("What's New in ProtonVPN Qt App"));
    setMinimumSize(WHATS_NEW_MIN_WIDTH, WHATS_NEW_MIN_HEIGHT);
    setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(WHATS_NEW_SPACING);
    layout->setContentsMargins(WHATS_NEW_H_MARGIN, WHATS_NEW_TOP_MARGIN, WHATS_NEW_H_MARGIN, WHATS_NEW_BOT_MARGIN);

    //  Header
    QLabel* titleLabel = new QLabel(
        QStringLiteral("<h2 style=\"margin-bottom:2px;\">%1</h2>")
            .arg(tr("What\u2019s New").toHtmlEscaped()),
        this);
    titleLabel->setTextFormat(Qt::RichText);
    layout->addWidget(titleLabel);

    QLabel* versionLabel = new QLabel(
        QStringLiteral("<span style=\"color:#888;\">%1</span>")
            .arg(tr("Version %1").arg(version).toHtmlEscaped()),
        this);
    versionLabel->setTextFormat(Qt::RichText);
    layout->addWidget(versionLabel);

    QFrame* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setObjectName(QStringLiteral("sidebarDivider"));
    layout->addWidget(divider);

    //  Release notes
    QTextBrowser* browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setHtml(
        QStringLiteral("<p>%1</p>")
        .arg(tr("Thanks for keeping ProtonVPN Qt App up to date! "
                "For the full list of changes, bug fixes, and new features, visit the release page on GitHub: "
                "<a href='https://github.com/wheat32/proton-vpn-qt-app/releases'>"
                "github.com/wheat32/proton-vpn-qt-app/releases</a>.")));
    layout->addWidget(browser, BROWSER_STRETCH);

    //  Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(WHATS_NEW_BTN_SPACING);

    QPushButton* releasePageBtn = new QPushButton(tr("View Release Notes"), this);
    releasePageBtn->setObjectName(QStringLiteral("secondaryButton"));
    connect(releasePageBtn, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/wheat32/proton-vpn-qt-app/releases")));
    });

    QPushButton* closeBtn = new QPushButton(tr("Got It!"), this);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(releasePageBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);
}
