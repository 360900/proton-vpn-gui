#include "errorDetailsDialog.h"

#include <QClipboard>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
// ReSharper disable once CppUnusedIncludeDirective
#include <QVBoxLayout>

namespace
{
constexpr int ERROR_DIALOG_MIN_WIDTH  = 640;
constexpr int ERROR_DIALOG_MIN_HEIGHT = 400;
constexpr int ERROR_DIALOG_SPACING    = 10;
constexpr int MONO_FONT_SIZE          = 9;
} // namespace

ErrorDetailsDialog::ErrorDetailsDialog(const QString& errorText, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Error Details"));
    setMinimumSize(ERROR_DIALOG_MIN_WIDTH, ERROR_DIALOG_MIN_HEIGHT);
    setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(ERROR_DIALOG_SPACING);

    QPlainTextEdit* textEdit = new QPlainTextEdit(this);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(errorText);
    textEdit->setFont(QFont(QStringLiteral("Monospace"), MONO_FONT_SIZE));
    layout->addWidget(textEdit, 1);

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
