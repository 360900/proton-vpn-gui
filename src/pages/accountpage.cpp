#include "accountpage.h"

#include <QHBoxLayout>
#include <QSvgWidget>

static QWidget* makeInfoRow(const QString& labelText, QLabel*& valueLabel, QWidget* parent)
{
    auto* row = new QWidget(parent);
    row->setObjectName(QStringLiteral("infoRow"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 10, 16, 10);
    auto* label = new QLabel(labelText, row);
    label->setObjectName(QStringLiteral("infoKey"));
    layout->addWidget(label);
    layout->addStretch();
    valueLabel = new QLabel(QStringLiteral("—"), row);
    valueLabel->setObjectName(QStringLiteral("infoValue"));
    layout->addWidget(valueLabel);
    return row;
}

AccountPage::AccountPage(VpnManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // Header
    auto* headerRow = new QHBoxLayout();
    auto* titleLabel = new QLabel(QStringLiteral("Account"), this);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    headerRow->addWidget(titleLabel);
    headerRow->addStretch();
    m_refreshBtn = new QPushButton(QStringLiteral("↻ Refresh"), this);
    m_refreshBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_refreshBtn->setFixedHeight(30);
    connect(m_refreshBtn, &QPushButton::clicked, this, &AccountPage::refresh);
    headerRow->addWidget(m_refreshBtn);
    layout->addLayout(headerRow);

    // Info card
    auto* card = new QWidget(this);
    card->setObjectName(QStringLiteral("infoCard"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    cardLayout->addWidget(makeInfoRow(QStringLiteral("Username"), m_nameLabel, card));
    cardLayout->addWidget(makeInfoRow(QStringLiteral("Plan"),     m_planLabel, card));

    layout->addWidget(card);

    // Upgrade prompt — shown only for Free accounts.
    m_upgradeLabel = new QLabel(
        QStringLiteral("To upgrade to VPN Plus visit: "
                       "<a href=\"https://account.protonvpn.com/pricing\">"
                       "https://account.protonvpn.com/pricing</a>"),
        this);
    m_upgradeLabel->setTextFormat(Qt::RichText);
    m_upgradeLabel->setOpenExternalLinks(true);
    m_upgradeLabel->setWordWrap(true);
    m_upgradeLabel->setObjectName(QStringLiteral("upgradeLabel"));
    m_upgradeLabel->hide();
    layout->addWidget(m_upgradeLabel);

    layout->addStretch();

    // Sign out button
    auto* signOutBtn = new QPushButton(QStringLiteral("Sign Out"), this);
    signOutBtn->setObjectName(QStringLiteral("dangerButton"));
    signOutBtn->setCursor(Qt::PointingHandCursor);
    connect(signOutBtn, &QPushButton::clicked, this, &AccountPage::signOutRequested);
    layout->addWidget(signOutBtn);

    connect(m_manager, &VpnManager::infoReady,        this, &AccountPage::onInfoReady);
    connect(m_manager, &VpnManager::accountTypeReady, this, &AccountPage::onAccountTypeReady);

    // Show the account type we already know (fetched at startup / login).
    if (m_manager->accountType() != AccountType::Unknown)
        onAccountTypeReady(m_manager->accountType());

    static constexpr const char* frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    static constexpr int frameCount = 10;
    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(200);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_spinnerFrame = (m_spinnerFrame + 1) % frameCount;
        m_nameLabel->setText(
            QStringLiteral("%1 Loading…").arg(QString::fromUtf8(frames[m_spinnerFrame])));
    });
}

void AccountPage::refresh()
{
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(QStringLiteral("Loading…"));
    m_spinnerFrame = 0;
    m_nameLabel->setText(QStringLiteral("⠋ Loading…"));
    m_spinnerTimer->start();
    m_manager->fetchInfo();
    m_manager->fetchAccountType();
}

void AccountPage::onInfoReady(const QMap<QString, QString>& info) const
{
    m_spinnerTimer->stop();
    m_refreshBtn->setEnabled(true);
    m_refreshBtn->setText(QStringLiteral("↻ Refresh"));

    auto get = [&](const QString& key) -> QString
    {
        QString val = info.value(key, QStringLiteral("—"));
        return (val == QStringLiteral("None") || val.isEmpty()) ? QStringLiteral("—") : val;
    };

    m_nameLabel->setText(get(QStringLiteral("name")));
}

void AccountPage::onAccountTypeReady(AccountType type) const
{
    switch (type)
    {
        case AccountType::Free:
            m_planLabel->setText(QStringLiteral("Free"));
            m_planLabel->setStyleSheet(QStringLiteral("color: #aaaaaa;"));
            m_upgradeLabel->show();
            break;
        case AccountType::Plus:
            m_planLabel->setText(QStringLiteral("VPN Plus"));
            m_planLabel->setStyleSheet(QStringLiteral("color: #7B61FF; font-weight: bold;"));
            m_upgradeLabel->hide();
            break;
        default:
            m_planLabel->setText(QStringLiteral("—"));
            m_planLabel->setStyleSheet(QString());
            m_upgradeLabel->hide();
            break;
    }
}
