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

    layout->addWidget(card);
    layout->addStretch();

    // Sign out button
    auto* signOutBtn = new QPushButton(QStringLiteral("Sign Out"), this);
    signOutBtn->setObjectName(QStringLiteral("dangerButton"));
    signOutBtn->setCursor(Qt::PointingHandCursor);
    connect(signOutBtn, &QPushButton::clicked, this, &AccountPage::signOutRequested);
    layout->addWidget(signOutBtn);

    connect(m_manager, &VpnManager::infoReady, this, &AccountPage::onInfoReady);

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

