#include "loginpage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSvgWidget>
#include <QPixmap>
#include <QSvgRenderer>
#include <QPainter>

static QIcon svgIcon(const QString &path, const QSize &size = {20, 20})
{
    QPixmap pix(size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QSvgRenderer renderer(path);
    renderer.render(&p);
    return QIcon(pix);
}

LoginPage::LoginPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setAlignment(Qt::AlignCenter);

    auto *card = new QWidget(this);
    card->setObjectName(QStringLiteral("loginCard"));
    card->setFixedWidth(360);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(0);
    cardLayout->setContentsMargins(0, 0, 0, 0);

    // Inner stack: 0 = credentials, 1 = 2FA
    m_stack = new QStackedWidget(card);

    buildCredsWidget();
    buildTFAWidget();

    m_stack->addWidget(m_credsWidget);   // index 0
    m_stack->addWidget(m_tfaWidget);     // index 1
    cardLayout->addWidget(m_stack);

    // Shared error label below the stack
    m_errorLabel = new QLabel(card);
    m_errorLabel->setObjectName(QStringLiteral("errorLabel"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setVisible(false);
    m_errorLabel->setContentsMargins(32, 0, 32, 16);
    cardLayout->addWidget(m_errorLabel);

    outerLayout->addWidget(card, 0, Qt::AlignCenter);
}

void LoginPage::buildCredsWidget()
{
    m_credsWidget = new QWidget();
    auto *layout = new QVBoxLayout(m_credsWidget);
    layout->setSpacing(16);
    layout->setContentsMargins(32, 32, 32, 24);

    // Logo
    auto *logo = new QSvgWidget(QStringLiteral(":/assets/proton-vpn-logo.svg"), m_credsWidget);
    logo->setFixedSize(180, 40);
    layout->addWidget(logo, 0, Qt::AlignCenter);

    layout->addSpacing(8);

    auto *titleLabel = new QLabel(QStringLiteral("Sign in to Proton VPN"), m_credsWidget);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    layout->addSpacing(4);

    // Username
    auto *usernameLabel = new QLabel(QStringLiteral("Username"), m_credsWidget);
    usernameLabel->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(usernameLabel);

    m_usernameEdit = new QLineEdit(m_credsWidget);
    m_usernameEdit->setObjectName(QStringLiteral("inputField"));
    m_usernameEdit->setPlaceholderText(QStringLiteral("Enter your username"));
    layout->addWidget(m_usernameEdit);

    // Password
    auto *passwordLabel = new QLabel(QStringLiteral("Password"), m_credsWidget);
    passwordLabel->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(passwordLabel);

    auto *passwordRow = new QHBoxLayout();
    m_passwordEdit = new QLineEdit(m_credsWidget);
    m_passwordEdit->setObjectName(QStringLiteral("inputField"));
    m_passwordEdit->setPlaceholderText(QStringLiteral("Enter your password"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    passwordRow->addWidget(m_passwordEdit);

    m_togglePasswordBtn = new QPushButton(m_credsWidget);
    m_togglePasswordBtn->setObjectName(QStringLiteral("iconButton"));
    m_togglePasswordBtn->setIcon(svgIcon(QStringLiteral(":/assets/eye-show.svg")));
    m_togglePasswordBtn->setIconSize({20, 20});
    m_togglePasswordBtn->setFixedSize(36, 36);
    m_togglePasswordBtn->setCheckable(true);
    m_togglePasswordBtn->setToolTip(QStringLiteral("Show/hide password"));
    m_togglePasswordBtn->setCursor(Qt::PointingHandCursor);
    connect(m_togglePasswordBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_passwordVisible = checked;
        togglePasswordVisibility();
    });
    passwordRow->addWidget(m_togglePasswordBtn);
    layout->addLayout(passwordRow);

    // Sign In button
    m_loginBtn = new QPushButton(QStringLiteral("Sign In"), m_credsWidget);
    m_loginBtn->setObjectName(QStringLiteral("primaryButton"));
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    connect(m_loginBtn, &QPushButton::clicked, this, [this]() {
        emit loginRequested(m_usernameEdit->text().trimmed(), m_passwordEdit->text());
    });
    connect(m_passwordEdit, &QLineEdit::returnPressed, m_loginBtn, &QPushButton::click);
    connect(m_usernameEdit, &QLineEdit::returnPressed, m_passwordEdit,
            QOverload<>::of(&QLineEdit::setFocus));
    layout->addWidget(m_loginBtn);
}

void LoginPage::buildTFAWidget()
{
    m_tfaWidget = new QWidget();
    auto *layout = new QVBoxLayout(m_tfaWidget);
    layout->setSpacing(16);
    layout->setContentsMargins(32, 32, 32, 24);

    // Logo
    auto *logo = new QSvgWidget(QStringLiteral(":/assets/proton-vpn-logo.svg"), m_tfaWidget);
    logo->setFixedSize(180, 40);
    layout->addWidget(logo, 0, Qt::AlignCenter);

    layout->addSpacing(8);

    auto *titleLabel = new QLabel(QStringLiteral("Two-Factor Authentication"), m_tfaWidget);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto *descLabel = new QLabel(
        QStringLiteral("Enter the 6-digit code from your authenticator app."),
        m_tfaWidget);
    descLabel->setObjectName(QStringLiteral("fieldLabel"));
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(descLabel);

    layout->addSpacing(4);

    auto *tfaLabel = new QLabel(QStringLiteral("2FA Token"), m_tfaWidget);
    tfaLabel->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(tfaLabel);

    m_tfaEdit = new QLineEdit(m_tfaWidget);
    m_tfaEdit->setObjectName(QStringLiteral("inputField"));
    m_tfaEdit->setPlaceholderText(QStringLiteral("e.g. 123456"));
    m_tfaEdit->setMaxLength(8);   // TOTP codes are 6 digits; allow 8 for recovery codes
    layout->addWidget(m_tfaEdit);

    m_tfaSubmitBtn = new QPushButton(QStringLiteral("Verify"), m_tfaWidget);
    m_tfaSubmitBtn->setObjectName(QStringLiteral("primaryButton"));
    m_tfaSubmitBtn->setCursor(Qt::PointingHandCursor);
    connect(m_tfaSubmitBtn, &QPushButton::clicked, this, [this]() {
        emit twoFASubmitted(m_tfaEdit->text().trimmed());
    });
    connect(m_tfaEdit, &QLineEdit::returnPressed, m_tfaSubmitBtn, &QPushButton::click);
    layout->addWidget(m_tfaSubmitBtn);
}

void LoginPage::show2FAPrompt()
{
    setError(QString());
    m_tfaEdit->clear();
    m_tfaSubmitBtn->setEnabled(true);
    m_tfaSubmitBtn->setText(QStringLiteral("Verify"));
    m_tfaEdit->setEnabled(true);
    m_stack->setCurrentIndex(1);
    m_tfaEdit->setFocus();
}

void LoginPage::reset()
{
    m_stack->setCurrentIndex(0);
    setError(QString());
    m_passwordEdit->clear();
    m_tfaEdit->clear();
    m_loginBtn->setEnabled(true);
    m_loginBtn->setText(QStringLiteral("Sign In"));
    m_usernameEdit->setEnabled(true);
    m_passwordEdit->setEnabled(true);
    m_togglePasswordBtn->setEnabled(true);
}

void LoginPage::setError(const QString &error)
{
    if (error.isEmpty()) {
        m_errorLabel->setVisible(false);
    } else {
        m_errorLabel->setText(error);
        m_errorLabel->setVisible(true);
    }
}

void LoginPage::setLoading(bool loading)
{
    if (m_stack->currentIndex() == 0) {
        m_loginBtn->setEnabled(!loading);
        m_loginBtn->setText(loading ? QStringLiteral("Signing in…") : QStringLiteral("Sign In"));
        m_usernameEdit->setEnabled(!loading);
        m_passwordEdit->setEnabled(!loading);
        m_togglePasswordBtn->setEnabled(!loading);
    } else {
        m_tfaSubmitBtn->setEnabled(!loading);
        m_tfaSubmitBtn->setText(loading ? QStringLiteral("Verifying…") : QStringLiteral("Verify"));
        m_tfaEdit->setEnabled(!loading);
    }
}

void LoginPage::togglePasswordVisibility()
{
    if (m_passwordVisible) {
        m_passwordEdit->setEchoMode(QLineEdit::Normal);
        m_togglePasswordBtn->setIcon(svgIcon(QStringLiteral(":/assets/eye-hide.svg")));
    } else {
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_togglePasswordBtn->setIcon(svgIcon(QStringLiteral(":/assets/eye-show.svg")));
    }
}
