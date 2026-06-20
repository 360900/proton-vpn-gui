#include "updateAvailableDialog.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
constexpr int UPDATE_MIN_WIDTH   = 420;
constexpr int UPDATE_SPACING     = 12;
constexpr int UPDATE_H_MARGIN    = 24;
constexpr int UPDATE_TOP_MARGIN  = 20;
constexpr int UPDATE_BOT_MARGIN  = 16;
constexpr int UPDATE_BTN_SPACING = 8;
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
            .arg(tr("A new version of ProtonVPN Qt App is available.").toHtmlEscaped(),
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
    connect(downloadBtn, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/wheat32/proton-vpn-qt-app/releases")));
    });
    connect(downloadBtn, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(laterBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(downloadBtn);
    layout->addLayout(btnLayout);
}
