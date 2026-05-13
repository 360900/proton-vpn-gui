#include "errordetailsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFont>
#include <QGuiApplication>
#include <QClipboard>

ErrorDetailsDialog::ErrorDetailsDialog(const QString& errorText, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Error Details"));
    setMinimumSize(640, 400);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* textEdit = new QPlainTextEdit(this);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(errorText);
    textEdit->setFont(QFont(QStringLiteral("Monospace"), 9));
    layout->addWidget(textEdit);

    auto* btnRow = new QHBoxLayout();
    auto* copyBtn = new QPushButton(QStringLiteral("Copy to Clipboard"), this);
    connect(copyBtn, &QPushButton::clicked, this, [errorText]()
    {
        QGuiApplication::clipboard()->setText(errorText);
    });
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(copyBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);
}

