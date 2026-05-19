#include "debugpage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QScrollArea>
#include <QMessageBox>
#include <QFile>
#include <functional>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>

#include "../appconfig.h"
#include "../dialogs/whatsnewdialog.h"

// ── Small helpers ────────────────────────────────────────────────────────────

static QLabel* makeSectionHeader(const QString& text, QWidget* parent)
{
    QLabel* lbl = new QLabel(text, parent);
    QFont f = lbl->font();
    f.setBold(true);
    lbl->setFont(f);
    return lbl;
}

static QFrame* makeDivider(QWidget* parent)
{
    QFrame* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setObjectName(QStringLiteral("sidebarDivider"));
    return line;
}

static QString boolStr(bool v)
{
    return v ? QStringLiteral("true") : QStringLiteral("false");
}

static QString themeStr(AppConfig::Theme t)
{
    switch (t)
    {
    case AppConfig::Theme::Dark:
        return QStringLiteral("Dark");
    case AppConfig::Theme::Light:
        return QStringLiteral("Light");
    default:
        return QStringLiteral("System");
    }
}

// ── DebugPage constructor ────────────────────────────────────────────────────

DebugPage::DebugPage(QWidget* parent)
    : QWidget(parent)
{
    // Outer layout holds only the scroll area so the page itself never clips.
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outerLayout->addWidget(scroll);

    QWidget* content = new QWidget(scroll);
    scroll->setWidget(content);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setAlignment(Qt::AlignTop);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 20, 24, 20);

    // ── Page header ──────────────────────────────────────────────────────────
    QLabel* titleLabel = new QLabel(tr("Debug Tools"), content);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    QLabel* subtitleLabel = new QLabel(
        tr("This page is only available in debug builds. Press F11 to toggle the sidebar button."),
        content);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addWidget(subtitleLabel);

    layout->addWidget(makeDivider(content));

    // ── Dialogs section ──────────────────────────────────────────────────────
    layout->addWidget(makeSectionHeader(tr("Dialogs"), content));

    QPushButton* whatsNewBtn = new QPushButton(tr("Test \u201cWhat\u2019s New\u201d Dialog"), content);
    whatsNewBtn->setObjectName(QStringLiteral("secondaryButton"));
    whatsNewBtn->setCursor(Qt::PointingHandCursor);
    connect(whatsNewBtn, &QPushButton::clicked, this, [this]()
    {
        QString version = QStringLiteral("?.?.?");
        QFile vf(QStringLiteral(":/version.json"));
        if (vf.open(QIODevice::ReadOnly))
        {
            const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
            vf.close();
            version = obj.value(QStringLiteral("app_version")).toString(version);
        }
        auto* dlg = new WhatsNewDialog(version, this);
        dlg->setModal(true);
        dlg->show();
    });
    layout->addWidget(whatsNewBtn, 0, Qt::AlignLeft);

    layout->addWidget(makeDivider(content));

    // ── Settings section ─────────────────────────────────────────────────────
    layout->addWidget(makeSectionHeader(tr("Settings"), content));

    // "Clear All Settings" row
    QHBoxLayout* clearRow = new QHBoxLayout();
    clearRow->setSpacing(12);

    QLabel* clearDesc = new QLabel(
        tr("Delete the config file and reset every value to its default."),
        content);
    clearDesc->setWordWrap(true);
    clearDesc->setStyleSheet(QStringLiteral("color: #888;"));
    clearRow->addWidget(clearDesc, 1);

    QPushButton* clearAllBtn = new QPushButton(tr("Clear All Settings"), content);
    clearAllBtn->setObjectName(QStringLiteral("dangerButton"));
    clearAllBtn->setCursor(Qt::PointingHandCursor);
    clearAllBtn->setFixedWidth(160);
    connect(clearAllBtn, &QPushButton::clicked, this, [this]()
    {
        QMessageBox mb(this);
        mb.setWindowTitle(tr("Clear All Settings"));
        mb.setText(tr("This will delete the config file and reset every setting to its default. "
                      "The change takes effect immediately in memory."));
        mb.setIcon(QMessageBox::Warning);
        mb.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        mb.setDefaultButton(QMessageBox::Cancel);
        if (mb.exec() == QMessageBox::Yes)
        {
            AppConfig::instance().resetToDefaults();
            refreshValues();
        }
    });
    clearRow->addWidget(clearAllBtn, 0, Qt::AlignRight);
    layout->addLayout(clearRow);

    // ── Per-key grid ─────────────────────────────────────────────────────────
    QWidget* gridWidget = new QWidget(content);
    QGridLayout* grid = new QGridLayout(gridWidget);
    grid->setContentsMargins(0, 4, 0, 4);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(1, 1); // value column stretches

    // Helper: appends one row, wires the Reset button via onReset, and returns
    // the value label so the caller can update it in refreshValues().
    int row = 0;
    auto addRow = [&](const QString& key,
                      const QString& defaultHint,
                      std::function<void()> onReset) -> QLabel*
    {
        QLabel* keyLbl = new QLabel(
            QStringLiteral("<b>%1</b>").arg(key.toHtmlEscaped()),
            gridWidget);
        keyLbl->setTextFormat(Qt::RichText);
        keyLbl->setMinimumWidth(180);

        QLabel* valLbl = new QLabel(gridWidget);
        valLbl->setTextFormat(Qt::PlainText);
        valLbl->setStyleSheet(QStringLiteral("font-family: monospace;"));

        QPushButton* resetBtn = new QPushButton(tr("Reset"), gridWidget);
        resetBtn->setObjectName(QStringLiteral("secondaryButton"));
        resetBtn->setCursor(Qt::PointingHandCursor);
        resetBtn->setToolTip(tr("Reset to default: %1").arg(defaultHint));
        resetBtn->setFixedWidth(64);
        connect(resetBtn, &QPushButton::clicked, this, [this, onReset]()
        {
            onReset();
            refreshValues();
        });

        grid->addWidget(keyLbl,   row, 0);
        grid->addWidget(valLbl,   row, 1);
        grid->addWidget(resetBtn, row, 2);
        ++row;

        return valLbl;
    };

    m_valAutoConnect = addRow(
        QStringLiteral("auto_connect"),
        QStringLiteral("false"),
        []() { AppConfig::instance().setAutoConnect(false); });

    m_valNotifications = addRow(
        QStringLiteral("notifications"),
        QStringLiteral("true"),
        []() { AppConfig::instance().setNotifications(true); });

    m_valRecentCount = addRow(
        QStringLiteral("recent_connections_count"),
        QStringLiteral("5"),
        []() { AppConfig::instance().setRecentConnectionsCount(5); });

    m_valStartHidden = addRow(
        QStringLiteral("start_hidden"),
        QStringLiteral("false"),
        []() { AppConfig::instance().setStartHidden(false); });

    m_valShowPicker = addRow(
        QStringLiteral("show_location_picker"),
        QStringLiteral("true"),
        []() { AppConfig::instance().setShowLocationPicker(true); });

    m_valTheme = addRow(
        QStringLiteral("theme"),
        QStringLiteral("System"),
        []() { AppConfig::instance().setTheme(AppConfig::Theme::System); });

    m_valLastSeenVersion = addRow(
        QStringLiteral("last_seen_version"),
        QStringLiteral("(empty)"),
        []() { AppConfig::instance().setLastSeenVersion(QString()); });

    layout->addWidget(gridWidget);

    layout->addStretch();

    // Populate value labels on first show.
    refreshValues();
}

void DebugPage::refreshValues()
{
    const AppConfig& cfg = AppConfig::instance();

    m_valAutoConnect->setText(boolStr(cfg.autoConnect()));
    m_valNotifications->setText(boolStr(cfg.notifications()));
    m_valRecentCount->setText(QString::number(cfg.recentConnectionsCount()));
    m_valStartHidden->setText(boolStr(cfg.startHidden()));
    m_valShowPicker->setText(boolStr(cfg.showLocationPicker()));
    m_valTheme->setText(themeStr(cfg.theme()));

    const QString lsv = cfg.lastSeenVersion();
    m_valLastSeenVersion->setText(lsv.isEmpty() ? QStringLiteral("(empty)") : lsv);
}
