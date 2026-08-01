#include "updateAvailableDialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

#include "../cli/flatpakUtils.h"
#include "../debug.h"

namespace
{
constexpr int UPDATE_MIN_WIDTH   = 420;
constexpr int UPDATE_SPACING     = 12;
constexpr int UPDATE_H_MARGIN    = 24;
constexpr int UPDATE_TOP_MARGIN  = 20;
constexpr int UPDATE_BOT_MARGIN  = 16;
constexpr int UPDATE_BTN_SPACING = 8;
const QString RELEASES_URL = QStringLiteral("https://github.com/360900/proton-vpn-gui/releases");
} // namespace

UpdateAvailableDialog::UpdateAvailableDialog(const QString& currentVersion,
                                             const QString& newVersion,
                                             QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Update Available"));
    setMinimumWidth(UPDATE_MIN_WIDTH);
    setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(UPDATE_SPACING);
    layout->setContentsMargins(UPDATE_H_MARGIN, UPDATE_TOP_MARGIN,
                               UPDATE_H_MARGIN, UPDATE_BOT_MARGIN);

    // Header
    QLabel* titleLabel = new QLabel(
        QStringLiteral("<h2 style=\"margin-bottom:2px;\">%1</h2>")
            .arg(tr("Update Available").toHtmlEscaped()),
        this);
    titleLabel->setTextFormat(Qt::RichText);
    layout->addWidget(titleLabel);

    QFrame* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setObjectName(QStringLiteral("sidebarDivider"));
    layout->addWidget(divider);

    // Body
    QLabel* bodyLabel = new QLabel(
        QStringLiteral("<p>%1</p><p>%2<br>%3</p>")
            .arg(tr("A new version of Proton VPN GUI is available.").toHtmlEscaped(),
                 tr("Current version: <b>v%1</b>").arg(currentVersion.toHtmlEscaped()),
                 tr("New version: <b>v%1</b>").arg(newVersion.toHtmlEscaped())),
        this);
    bodyLabel->setTextFormat(Qt::RichText);
    bodyLabel->setWordWrap(true);
    layout->addWidget(bodyLabel);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(UPDATE_BTN_SPACING);

    QPushButton* laterBtn = new QPushButton(tr("Later"), this);
    laterBtn->setObjectName(QStringLiteral("secondaryButton"));
    connect(laterBtn, &QPushButton::clicked, this, &QDialog::reject);

    QPushButton* downloadBtn = new QPushButton(tr("Download"), this);
    downloadBtn->setDefault(true);
    connect(downloadBtn, &QPushButton::clicked, this, [this]()
    {
        // QDesktopServices::openUrl() goes through the desktop portal and silently
        // no-ops from a windowed app on some Wayland/KDE setups (returns true but
        // never launches anything). Spawning xdg-open directly is reliable here.
        const auto [program, args] = buildHostCommand(QStringLiteral("xdg-open"), {RELEASES_URL});
        if (QProcess::startDetached(program, args) == false)
        {
            DBG_APP(QStringLiteral("Failed to open browser for update download: ") + RELEASES_URL);
            QMessageBox::information(
                this,
                tr("Open Manually"),
                tr("Couldn't open your browser automatically. Please visit this page to download the update:\n\n%1")
                    .arg(RELEASES_URL));
        }
    });
    connect(downloadBtn, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(laterBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(downloadBtn);
    layout->addLayout(btnLayout);
}
