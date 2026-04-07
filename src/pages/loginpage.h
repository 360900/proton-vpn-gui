#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include "../widgets/infobanner.h"

class LoginPage : public QWidget
{
    Q_OBJECT

public:
    explicit LoginPage(QWidget* parent = nullptr);

    void setError(const QString& error) const;
    void setLoading(bool loading) const;
    void show2FAPrompt() const; // called when VpnManager emits twoFactorRequired()
    void reset() const; // return to username/password view
    void checkPrereleaseBanner();

public slots:
    void onCliVersionReady(const QString& version);

signals:
    void loginRequested(const QString& username, const QString& password);
    void twoFASubmitted(const QString& token);

private:
    // --- credentials view ---
    QWidget* m_credsWidget;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QPushButton* m_togglePasswordBtn;
    QPushButton* m_loginBtn;

    // --- 2FA view ---
    QWidget* m_tfaWidget;
    QLineEdit* m_tfaEdit;
    QPushButton* m_tfaSubmitBtn;

    // shared
    QStackedWidget* m_stack;
    QLabel* m_errorLabel;
    QVBoxLayout* m_outerLayout = nullptr;
    InfoBanner* m_versionBanner = nullptr;
    InfoBanner* m_prereleaseBanner = nullptr;

    bool m_passwordVisible = false;
    void togglePasswordVisibility() const;
    void buildCredsWidget();
    void buildTFAWidget();
};
