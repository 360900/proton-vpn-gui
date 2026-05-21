#include "aboutdialog.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFile>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include <QVersionNumber>
#include <QProcessEnvironment>

AboutDialog::AboutDialog(const QString& installedCliVersion, QWidget* parent)
    : QDialog(parent)
{
    // Load versions from the embedded version.json resource
    QString appVersion       = QStringLiteral("unknown");
    QString testedVersionStr = QStringLiteral("unknown");
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly))
    {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        if (obj.contains(QStringLiteral("app_version")))
        {
            appVersion = obj[QStringLiteral("app_version")].toString();
        }
        if (obj.contains(QStringLiteral("cli_version_tested")))
        {
            testedVersionStr = obj[QStringLiteral("cli_version_tested")].toString();
        }
    }

    setWindowTitle(tr("About ProtonVPN Qt App"));
    setMinimumSize(520, 480);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    // ── Title / subtitle ─────────────────────────────────────
    auto* titleLabel = new QLabel(
        QStringLiteral("<h2 style=\"margin-bottom:2px;\">%1</h2>")
            .arg(tr("ProtonVPN Qt App").toHtmlEscaped()),
        this);
    titleLabel->setTextFormat(Qt::RichText);
    layout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel(
        QStringLiteral("<span style=\"color:#888;\">%1</span>")
            .arg(tr("A community-built Qt front-end for the Proton VPN CLI.").toHtmlEscaped()),
        this);
    subtitleLabel->setTextFormat(Qt::RichText);
    layout->addWidget(subtitleLabel);

    // ── Version table ─────────────────────────────────────────
    auto* versionWidget = new QWidget(this);
    auto* versionGrid   = new QGridLayout(versionWidget);
    versionGrid->setContentsMargins(0, 4, 0, 8);
    versionGrid->setHorizontalSpacing(8);
    versionGrid->setVerticalSpacing(4);
    versionGrid->setColumnStretch(1, 1);

    auto makeKey = [&](const QString& text) -> QLabel*
    {
        auto* l = new QLabel(QStringLiteral("<b>%1</b>").arg(text.toHtmlEscaped()), versionWidget);
        l->setTextFormat(Qt::RichText);
        return l;
    };

    versionGrid->addWidget(makeKey(tr("App version:")),        0, 0);
    versionGrid->addWidget(new QLabel(appVersion, versionWidget),          0, 1);

    versionGrid->addWidget(makeKey(tr("Tested against CLI:")), 1, 0);
    versionGrid->addWidget(new QLabel(testedVersionStr, versionWidget),    1, 1);

    versionGrid->addWidget(makeKey(tr("Qt version:")),         2, 0);
    versionGrid->addWidget(new QLabel(QStringLiteral(QT_VERSION_STR),      versionWidget), 2, 1);

    const bool isFlatpak = qEnvironmentVariableIsSet("FLATPAK_ID");
    versionGrid->addWidget(makeKey(tr("Package:")),            3, 0);
    versionGrid->addWidget(new QLabel(isFlatpak ? tr("Flatpak") : tr("System"), versionWidget), 3, 1);

    // Installed CLI version – highlighted only when it differs from tested
    if (installedCliVersion.isEmpty() == false)
    {
        const QVersionNumber installed = QVersionNumber::fromString(installedCliVersion);
        const QVersionNumber tested    = QVersionNumber::fromString(testedVersionStr);

        if (installed.isNull() == false
            && tested.isNull() == false
            && installed != tested)
        {
            const bool newer  = installed > tested;
            const QString arrow   = newer ? QStringLiteral(" \u25b2") : QStringLiteral(" \u25bc");
            const QString color   = newer ? QStringLiteral("#f59e0b") : QStringLiteral("#ef4444");
            const QString tooltip = newer
                ? tr("Your installed CLI (v%1) is newer than the version this app was "
                     "tested against (v%2). Things may work fine, but you could "
                     "encounter unexpected behavior.")
                      .arg(installedCliVersion, testedVersionStr)
                : tr("Your installed CLI (v%1) is older than the version this app was "
                     "tested against (v%2). Some features may not work correctly. "
                     "Consider upgrading the CLI.")
                      .arg(installedCliVersion, testedVersionStr);

            QLabel* valLabel = new QLabel(installedCliVersion + arrow, versionWidget);
            valLabel->setStyleSheet(
                QStringLiteral("color: %1; font-weight: bold;").arg(color));
            valLabel->setToolTip(tooltip);

            versionGrid->addWidget(makeKey(tr("Installed CLI:")), 4, 0);
            versionGrid->addWidget(valLabel,                      4, 1);
        }
    }

    layout->addWidget(versionWidget);

    // ── Disclaimer + Credits ──────────────────────────────────
    auto* browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setHtml(
        QStringLiteral(
            "<p><b>\u26a0 %1</b> %2</p>"
            "<hr/>"
            "<h3>%3</h3>"
            "<ul><li>Nicholas Page (<a href='https://github.com/wheat32'>wheat32</a>)</li></ul>"
            "<h3>%4</h3>"
            "<ul>"
            "<li>%5</li>"
            "<li>%6</li>"
            "<li>%7</li>"
            "<li>%8</li>"
            "</ul>"
            "<hr/>"
            "<p style='color:#888;font-size:small;'>%9</p>")
        .arg(
            tr("Disclaimer:"),
            tr("This project is <b>not affiliated with, endorsed by, or supported by Proton AG</b> "
               "in any way. ProtonVPN and the Proton logo are trademarks of Proton AG."),
            tr("Author"),
            tr("Credits & Acknowledgements"),
            tr("Built with <a href='https://www.qt.io/'>Qt 6</a>"),
            tr("Uses the <a href='https://protonvpn.com/support/linux-vpn-tool/'>ProtonVPN Linux CLI</a>"),
            tr("Icons from <a href='https://icons.getbootstrap.com/'>Bootstrap Icons</a> (MIT License)"),
            tr("Country flag SVGs from <a href='https://github.com/lipis/flag-icons'>flag-icons</a>"
               " by Panayiotis Lipiridis (MIT License)"),
            tr("This software is provided as-is, without warranty of any kind. Use at your own risk.")));
    layout->addWidget(browser);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(btns);
}
