#include "loginpage.h"
#include "../widgets/flatpakbetabanner.h"
#include "../widgets/svgbanner.h"

#include <QFile>
#include <QHBoxLayout>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QVersionNumber>

namespace
{
constexpr int LOGIN_CARD_WIDTH          = 360;
constexpr int LOGIN_LAYOUT_SPACING      = 16;
constexpr int LOGIN_H_MARGIN            = 32;
constexpr int LOGIN_TOP_MARGIN          = 32;
constexpr int LOGIN_BOT_MARGIN          = 24;
constexpr int LOGIN_LOGO_SPACING        = 8;
constexpr int LOGIN_FIELD_SPACING       = 4;
constexpr double LOGIN_LOGO_SCALE       = 4.0;
constexpr int PASSWORD_TOGGLE_ICON_SIZE = 20;
constexpr int PASSWORD_TOGGLE_BTN_SIZE  = 36;
constexpr int SVG_ICON_SIZE             = 20;
constexpr int TFA_MAX_LENGTH            = 8;
constexpr int ERROR_SCROLL_HEIGHT       = 100;
constexpr int ERROR_DETAILS_BTN_WIDTH   = 140;
constexpr int ERROR_CONTAINER_SPACING   = 6;
constexpr int ERROR_H_MARGIN            = 32;
constexpr int ERROR_BOT_MARGIN          = 16;
constexpr int STACK_INDEX_CREDS         = 0;
constexpr int STACK_INDEX_TFA           = 1;

QIcon svgIcon(const QString& path, const QSize& size = {SVG_ICON_SIZE, SVG_ICON_SIZE})
{
    QPixmap pix(size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QSvgRenderer renderer(path);
    renderer.render(&p);
    return QIcon(pix);
}
} // namespace

LoginPage::LoginPage(QWidget* parent)
    : QWidget(parent)
{
    m_outerLayout = new QVBoxLayout(this);
    m_outerLayout->setAlignment(Qt::AlignCenter);

    QWidget* card = new QWidget(this);
    card->setObjectName(QStringLiteral("loginCard"));
    card->setFixedWidth(LOGIN_CARD_WIDTH);

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(0);
    cardLayout->setContentsMargins(0, 0, 0, 0);

    // Inner stack: 0 = credentials, 1 = 2FA
    m_stack = new QStackedWidget(card);

    buildCredsWidget();
    buildTFAWidget();

    m_stack->addWidget(m_credsWidget); // index 0
    m_stack->addWidget(m_tfaWidget);   // index 1
    cardLayout->addWidget(m_stack);

    // Shared error section below the stack
    m_errorContainer = new QWidget(card);
    m_errorContainer->setVisible(false);
    QVBoxLayout* errorContainerLayout = new QVBoxLayout(m_errorContainer);
    errorContainerLayout->setSpacing(ERROR_CONTAINER_SPACING);
    errorContainerLayout->setContentsMargins(ERROR_H_MARGIN, 0, ERROR_H_MARGIN, ERROR_BOT_MARGIN);

    // Scrollable error label
    m_errorLabel = new QLabel();
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_errorLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_errorScrollArea = new QScrollArea(m_errorContainer);
    m_errorScrollArea->setWidget(m_errorLabel);
    m_errorScrollArea->setWidgetResizable(true);
    m_errorScrollArea->setFixedHeight(ERROR_SCROLL_HEIGHT);
    m_errorScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_errorScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_errorScrollArea->setObjectName(QStringLiteral("errorLabel"));
    m_errorScrollArea->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    errorContainerLayout->addWidget(m_errorScrollArea);

    // "View Details" button
    m_errorDetailsBtn = new QPushButton(tr("View Details"), m_errorContainer);
    m_errorDetailsBtn->setFixedWidth(ERROR_DETAILS_BTN_WIDTH);
    connect(m_errorDetailsBtn, &QPushButton::clicked, this, [this]()
    {
        ErrorDetailsDialog* dlg = new ErrorDetailsDialog(m_rawError, this);
        dlg->exec();
    });
    errorContainerLayout->addWidget(m_errorDetailsBtn, 0, Qt::AlignCenter);

    cardLayout->addWidget(m_errorContainer);

    m_outerLayout->addWidget(card, 0, Qt::AlignCenter);

    // Show a banner if this is a pre-release build
    checkPrereleaseBanner();
    checkFlatpakBetaBanner();
}

void LoginPage::buildCredsWidget()
{
    m_credsWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_credsWidget);
    layout->setSpacing(LOGIN_LAYOUT_SPACING);
    layout->setContentsMargins(LOGIN_H_MARGIN, LOGIN_TOP_MARGIN, LOGIN_H_MARGIN, LOGIN_BOT_MARGIN);

    // Logo
    SvgBanner* logo = new SvgBanner(QStringLiteral(":/assets/proton-vpn-logo.svg"), LOGIN_LOGO_SCALE, m_credsWidget);
    logo->setLightResource(QStringLiteral(":/assets/proton-vpn-logo-light.svg"));
    layout->addWidget(logo, 0, Qt::AlignCenter);

    layout->addSpacing(LOGIN_LOGO_SPACING);

    QLabel* titleLabel = new QLabel(tr("Sign in to Proton VPN"), m_credsWidget);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    layout->addSpacing(LOGIN_FIELD_SPACING);

    // Username
    QLabel* usernameLabel = new QLabel(tr("Username"), m_credsWidget);
    usernameLabel->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(usernameLabel);

    m_usernameEdit = new QLineEdit(m_credsWidget);
    m_usernameEdit->setObjectName(QStringLiteral("inputField"));
    m_usernameEdit->setPlaceholderText(tr("Enter your username"));
    layout->addWidget(m_usernameEdit);

    layout->addSpacing(LOGIN_FIELD_SPACING);

    // Password
    QLabel* passwordLabel = new QLabel(tr("Password"), m_credsWidget);
    passwordLabel->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(passwordLabel);

    QHBoxLayout* passwordRow = new QHBoxLayout();
    m_passwordEdit = new QLineEdit(m_credsWidget);
    m_passwordEdit->setObjectName(QStringLiteral("inputField"));
    m_passwordEdit->setPlaceholderText(tr("Enter your password"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    passwordRow->addWidget(m_passwordEdit);

    m_togglePasswordBtn = new QPushButton(m_credsWidget);
    m_togglePasswordBtn->setObjectName(QStringLiteral("iconButton"));
    m_togglePasswordBtn->setIcon(svgIcon(QStringLiteral(":/assets/eye-show.svg")));
    m_togglePasswordBtn->setIconSize({PASSWORD_TOGGLE_ICON_SIZE, PASSWORD_TOGGLE_ICON_SIZE});
    m_togglePasswordBtn->setFixedSize(PASSWORD_TOGGLE_BTN_SIZE, PASSWORD_TOGGLE_BTN_SIZE);
    m_togglePasswordBtn->setCheckable(true);
    m_togglePasswordBtn->setToolTip(tr("Show/hide password"));
    m_togglePasswordBtn->setCursor(Qt::PointingHandCursor);
    connect(m_togglePasswordBtn, &QPushButton::toggled, this, [this](const bool checked)
    {
        m_passwordVisible = checked;
        togglePasswordVisibility();
    });
    passwordRow->addWidget(m_togglePasswordBtn);
    layout->addLayout(passwordRow);

    layout->addSpacing(LOGIN_FIELD_SPACING);

    // Sign In button
    m_loginBtn = new QPushButton(tr("Sign In"), m_credsWidget);
    m_loginBtn->setObjectName(QStringLiteral("primaryButton"));
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    connect(m_loginBtn, &QPushButton::clicked, this, [this]()
    {
        emit loginRequested(m_usernameEdit->text().trimmed(), m_passwordEdit->text());
    });
    connect(m_passwordEdit, &QLineEdit::returnPressed, m_loginBtn, &QPushButton::click);
    connect(m_usernameEdit, &QLineEdit::returnPressed, m_passwordEdit,
            [this](){ m_passwordEdit->setFocus(); });
    layout->addWidget(m_loginBtn);
}

void LoginPage::buildTFAWidget()
{
    m_tfaWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_tfaWidget);
    layout->setSpacing(LOGIN_LAYOUT_SPACING);
    layout->setContentsMargins(LOGIN_H_MARGIN, LOGIN_TOP_MARGIN, LOGIN_H_MARGIN, LOGIN_BOT_MARGIN);

    // Logo
    SvgBanner* logo = new SvgBanner(QStringLiteral(":/assets/proton-vpn-logo.svg"), LOGIN_LOGO_SCALE, m_tfaWidget);
    logo->setLightResource(QStringLiteral(":/assets/proton-vpn-logo-light.svg"));
    layout->addWidget(logo, 0, Qt::AlignCenter);

    layout->addSpacing(LOGIN_LOGO_SPACING);

    QLabel* titleLabel = new QLabel(tr("Two-Factor Authentication"), m_tfaWidget);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    QLabel* descLabel = new QLabel(
        tr("Enter the 6-digit code from your authenticator app."),
        m_tfaWidget);
    descLabel->setObjectName(QStringLiteral("fieldLabel"));
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(descLabel);

    layout->addSpacing(LOGIN_FIELD_SPACING);

    QLabel* tfaLabel = new QLabel(tr("2FA Token"), m_tfaWidget);
    tfaLabel->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(tfaLabel);

    m_tfaEdit = new QLineEdit(m_tfaWidget);
    m_tfaEdit->setObjectName(QStringLiteral("inputField"));
    m_tfaEdit->setPlaceholderText(tr("e.g. 123456"));
    m_tfaEdit->setMaxLength(TFA_MAX_LENGTH); // TOTP codes are 6 digits; allow 8 for recovery codes
    layout->addWidget(m_tfaEdit);

    m_tfaSubmitBtn = new QPushButton(tr("Verify"), m_tfaWidget);
    m_tfaSubmitBtn->setObjectName(QStringLiteral("primaryButton"));
    m_tfaSubmitBtn->setCursor(Qt::PointingHandCursor);
    connect(m_tfaSubmitBtn, &QPushButton::clicked, this, [this]()
    {
        emit twoFASubmitted(m_tfaEdit->text().trimmed());
    });
    connect(m_tfaEdit, &QLineEdit::returnPressed, m_tfaSubmitBtn, &QPushButton::click);
    layout->addWidget(m_tfaSubmitBtn);

    m_tfaCancelBtn = new QPushButton(tr("Go Back"), m_tfaWidget);
    m_tfaCancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_tfaCancelBtn, &QPushButton::clicked, this, [this]()
    {
        emit loginCancelRequested();
    });
    layout->addWidget(m_tfaCancelBtn);
}

void LoginPage::show2FAPrompt() const
{
    setError(QString());
    m_tfaEdit->clear();
    m_tfaSubmitBtn->setEnabled(true);
    m_tfaSubmitBtn->setText(tr("Verify"));
    m_tfaEdit->setEnabled(true);
    m_stack->setCurrentIndex(STACK_INDEX_TFA);
    m_tfaEdit->setFocus();
}

void LoginPage::reset() const
{
    m_stack->setCurrentIndex(STACK_INDEX_CREDS);
    setError(QString());
    m_passwordEdit->clear();
    m_tfaEdit->clear();
    m_loginBtn->setEnabled(true);
    m_loginBtn->setText(tr("Sign In"));
    m_usernameEdit->setEnabled(true);
    m_passwordEdit->setEnabled(true);
    m_togglePasswordBtn->setEnabled(true);
}

void LoginPage::setError(const QString& error) const
{
    if (error.isEmpty())
    {
        m_errorContainer->setVisible(false);
        m_errorLabel->clear();
        m_rawError = QString();
    }
    else
    {
        m_rawError = error;
        m_errorLabel->setText(error);

        m_errorContainer->setVisible(true);
    }
}

void LoginPage::setLoading(const bool loading) const
{
    if (m_stack->currentIndex() == STACK_INDEX_CREDS)
    {
        m_loginBtn->setEnabled(loading == false);
        m_loginBtn->setText(loading == true ? tr("Signing in\u2026") : tr("Sign In"));
        m_usernameEdit->setEnabled(loading == false);
        m_passwordEdit->setEnabled(loading == false);
        m_togglePasswordBtn->setEnabled(loading == false);
    }
    else
    {
        m_tfaSubmitBtn->setEnabled(loading == false);
        m_tfaSubmitBtn->setText(loading == true ? tr("Verifying\u2026") : tr("Verify"));
        m_tfaEdit->setEnabled(loading == false);
        m_tfaCancelBtn->setEnabled(loading == false);
    }
}

void LoginPage::togglePasswordVisibility() const
{
    if (m_passwordVisible == true)
    {
        m_passwordEdit->setEchoMode(QLineEdit::Normal);
        m_togglePasswordBtn->setIcon(svgIcon(QStringLiteral(":/assets/eye-hide.svg")));
    }
    else
    {
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_togglePasswordBtn->setIcon(svgIcon(QStringLiteral(":/assets/eye-show.svg")));
    }
}

void LoginPage::checkPrereleaseBanner()
{
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly) == false) return;

    const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
    vf.close();

    if (obj.value(QStringLiteral("prerelease")).toBool(false) == false) return;

    const QString appVersion = obj.value(QStringLiteral("app_version")).toString();
    const QString msg = tr(
        "You are running a <b>pre-release</b> version of this app (<b>v%1</b>). "
        "It may contain bugs or incomplete features. Use with caution.")
        .arg(appVersion.toHtmlEscaped());

    m_prereleaseBanner = new InfoBanner(msg, this);
    connect(m_prereleaseBanner, &InfoBanner::dismissed, this, [this]() {
        m_prereleaseBanner = nullptr;
    });
    m_outerLayout->addWidget(m_prereleaseBanner);
}

void LoginPage::checkFlatpakBetaBanner()
{
    m_flatpakBetaBanner = FlatpakBetaBanner::createIfFlatpak(this);
    if (m_flatpakBetaBanner == nullptr) return;
    connect(m_flatpakBetaBanner, &FlatpakBetaBanner::dismissed, this, [this]()
    {
        m_flatpakBetaBanner = nullptr;
    });
    m_outerLayout->addWidget(m_flatpakBetaBanner);
}

void LoginPage::onCliVersionReady(const QString& version)
{
    QString cliVersionMin;
    QString cliVersionMax;
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly))
    {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        cliVersionMin = obj.value(QStringLiteral("cli_version_tested_min")).toString();
        cliVersionMax = obj.value(QStringLiteral("cli_version_tested_max")).toString();
    }

    if (version.isEmpty() || (cliVersionMin.isEmpty() && cliVersionMax.isEmpty())) return;

    const QVersionNumber installed = QVersionNumber::fromString(version);
    const QVersionNumber verMin    = QVersionNumber::fromString(cliVersionMin);
    const QVersionNumber verMax    = QVersionNumber::fromString(cliVersionMax);

    const bool tooOld = installed.isNull() == false && verMin.isNull() == false
                        && installed < verMin;
    const bool tooNew = installed.isNull() == false && verMax.isNull() == false
                        && installed > verMax;

    if (tooOld == false && tooNew == false) return;

    const QString rangeStr = (cliVersionMin.isEmpty() == false && cliVersionMax.isEmpty() == false)
        ? (cliVersionMin + QStringLiteral("-") + cliVersionMax)
        : (cliVersionMin.isEmpty() ? cliVersionMax : cliVersionMin);

    static const QString WORKAROUND_MSG = tr(
        " If you cannot log in due to this incompatibility, open a terminal and run "
        "<code>protonvpn connect</code> as a workaround until an update is released.");

    const QString msg = tooNew == true
        ? tr("Your Proton VPN CLI (<b>v%1</b>) is newer than the tested range (<b>%2</b>). "
             "Things may work fine, but you could encounter unexpected behavior.%3")
              .arg(version, rangeStr, WORKAROUND_MSG)
        : tr("Your Proton VPN CLI (<b>v%1</b>) is older than the tested range (<b>%2</b>). "
             "Some features may not work correctly. Consider upgrading the CLI.%3")
              .arg(version, rangeStr, WORKAROUND_MSG);

    m_versionBanner = new InfoBanner(msg, this);
    connect(m_versionBanner, &InfoBanner::dismissed, this, [this]() {
        m_versionBanner = nullptr;
    });
    m_outerLayout->addWidget(m_versionBanner);
}

