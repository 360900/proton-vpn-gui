#include "errordetailsdialog.h"

// ReSharper disable once CppUnusedIncludeDirective
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
    setWindowTitle(tr("Error Details"));
    setMinimumSize(640, 400);
    setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    QPlainTextEdit* textEdit = new QPlainTextEdit(this);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(errorText);
    textEdit->setFont(QFont(QStringLiteral("Monospace"), 9));
    layout->addWidget(textEdit);

    QHBoxLayout* btnRow = new QHBoxLayout();
    QPushButton* copyBtn = new QPushButton(tr("Copy to Clipboard"), this);
    connect(copyBtn, &QPushButton::clicked, this, [errorText]()
    {
        QGuiApplication::clipboard()->setText(errorText);
    });
    QPushButton* closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(copyBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);
}
