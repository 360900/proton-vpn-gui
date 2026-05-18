#include "accountpage.h"

#include <QHBoxLayout>
#include <QSvgWidget>

static QWidget* makeInfoRow(const QString& labelText, QLabel*& valueLabel, QWidget* parent)
{
    QWidget* row = new QWidget(parent);
    row->setObjectName(QStringLiteral("infoRow"));
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 10, 16, 10);
    QLabel* label = new QLabel(labelText, row);
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
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // Header
    QHBoxLayout* headerRow = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(tr("Account"), this);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    headerRow->addWidget(titleLabel);
    headerRow->addStretch();
    m_refreshBtn = new QPushButton(tr("\u21bb Refresh"), this);
    m_refreshBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_refreshBtn->setFixedHeight(30);
    connect(m_refreshBtn, &QPushButton::clicked, this, &AccountPage::refresh);
    headerRow->addWidget(m_refreshBtn);
    layout->addLayout(headerRow);

    // Info card
    QWidget* card = new QWidget(this);
    card->setObjectName(QStringLiteral("infoCard"));
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    cardLayout->addWidget(makeInfoRow(tr("Username"), m_nameLabel, card));
    cardLayout->addWidget(makeInfoRow(tr("Plan"),     m_planLabel, card));

    layout->addWidget(card);

    // Upgrade prompt — shown only for Free accounts.
    m_upgradeLabel = new QLabel(
        tr("To upgrade to VPN Plus visit: "
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
    QPushButton* signOutBtn = new QPushButton(tr("Sign Out"), this);
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
            tr("%1 Loading\u2026").arg(QString::fromUtf8(frames[m_spinnerFrame])));
    });
}

void AccountPage::refresh()
{
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(tr("Loading\u2026"));
    m_spinnerFrame = 0;
    m_nameLabel->setText(tr("\u280b Loading\u2026"));
    m_spinnerTimer->start();
    m_manager->fetchInfo();
    m_manager->fetchAccountType();
}

void AccountPage::onInfoReady(const QMap<QString, QString>& info) const
{
    m_spinnerTimer->stop();
    m_refreshBtn->setEnabled(true);
    m_refreshBtn->setText(tr("\u21bb Refresh"));

    auto get = [&](const QString& key) -> QString
    {
        const QString val = info.value(key, QStringLiteral("—"));
        return (val == QStringLiteral("None") || val.isEmpty()) ? QStringLiteral("—") : val;
    };

    m_nameLabel->setText(get(QStringLiteral("Account")));
}

void AccountPage::onAccountTypeReady(AccountType type) const
{
    switch (type)
    {
        case AccountType::Free:
            m_planLabel->setText(tr("Free"));
            m_planLabel->setStyleSheet(QStringLiteral("color: #aaaaaa;"));
            m_upgradeLabel->show();
            break;
        case AccountType::Plus:
            m_planLabel->setText(tr("VPN Plus"));
            m_planLabel->setStyleSheet(QStringLiteral("color: #7B61FF; font-weight: bold;"));
            m_upgradeLabel->hide();
            break;
        default:
            m_planLabel->setText(QStringLiteral("\u2014"));
            m_planLabel->setStyleSheet(QString());
            m_upgradeLabel->hide();
            break;
    }
}
