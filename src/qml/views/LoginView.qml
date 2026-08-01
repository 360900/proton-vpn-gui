import QtQuick
import QtQuick.Layouts
import ProtonVpnGui

// Sign-in flow: credentials, then an inline 2FA step when required.
Item {
    id: loginRoot

    // Clear fields whenever we land back on this view.
    Connections {
        target: VpnFacade
        function onUiStateChanged() {
            if (VpnFacade.uiState === VpnFacade.Login) {
                passwordField.text = ""
                tokenField.text = ""
            }
        }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 380
        height: cardContent.implicitHeight + Theme.spacingXxl * 2
        radius: Theme.radiusLg
        color: Theme.surface
        border.color: Theme.border
        border.width: 1

        // Entrance: fade and scale in when the view appears.
        opacity: 0
        scale: 0.96
        Component.onCompleted: {
            card.opacity = 1
            card.scale = 1
        }
        Behavior on opacity { NumberAnimation { duration: Theme.durSlow; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: Theme.durSlow; easing.type: Easing.OutCubic } }

        ColumnLayout {
            id: cardContent
            anchors.centerIn: parent
            width: card.width - Theme.spacingXxl * 2
            spacing: Theme.spacingMd

            Image {
                source: "qrc:/assets/proton-vpn-logo.svg"
                sourceSize: Qt.size(190, 48)
                fillMode: Image.PreserveAspectFit
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: Theme.spacingSm
            }

            //  Credentials step
            ColumnLayout {
                visible: !VpnFacade.twoFactorPending
                spacing: Theme.spacingMd
                Layout.fillWidth: true

                Text {
                    text: qsTr("Sign in")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                PTextField {
                    id: usernameField
                    placeholderText: qsTr("Username or email")
                    enabled: !VpnFacade.loginBusy
                    Layout.fillWidth: true
                    onAccepted: passwordField.forceActiveFocus()
                }

                PTextField {
                    id: passwordField
                    placeholderText: qsTr("Password")
                    secret: true
                    enabled: !VpnFacade.loginBusy
                    Layout.fillWidth: true
                    onAccepted: signInBtn.clicked()
                }

                PButton {
                    id: signInBtn
                    text: VpnFacade.loginBusy ? qsTr("Signing in…") : qsTr("Sign in")
                    busy: VpnFacade.loginBusy
                    enabled: !VpnFacade.loginBusy &&
                             usernameField.text.length > 0 && passwordField.text.length > 0
                    Layout.fillWidth: true
                    onClicked: VpnFacade.login(usernameField.text, passwordField.text)
                }
            }

            //  Two-factor step
            ColumnLayout {
                visible: VpnFacade.twoFactorPending
                spacing: Theme.spacingMd
                Layout.fillWidth: true

                Text {
                    text: qsTr("Two-factor authentication")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: qsTr("Enter the 6-digit code from your authenticator app.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }

                PTextField {
                    id: tokenField
                    placeholderText: qsTr("2FA code")
                    inputMethodHints: Qt.ImhDigitsOnly
                    Layout.fillWidth: true
                    horizontalAlignment: TextInput.AlignHCenter
                    onAccepted: verifyBtn.clicked()
                    onVisibleChanged: if (visible) forceActiveFocus()
                }

                PButton {
                    id: verifyBtn
                    text: qsTr("Verify")
                    enabled: tokenField.text.length >= 6
                    Layout.fillWidth: true
                    onClicked: VpnFacade.submit2fa(tokenField.text)
                }

                PButton {
                    text: qsTr("Go back")
                    variant: "ghost"
                    Layout.fillWidth: true
                    onClicked: VpnFacade.cancelLogin()
                }
            }

            //  Error area
            Rectangle {
                visible: VpnFacade.loginError.length > 0
                color: Qt.alpha(Theme.danger, 0.12)
                border.color: Theme.danger
                border.width: 1
                radius: Theme.radiusMd
                Layout.fillWidth: true
                implicitHeight: errorText.implicitHeight + Theme.spacingMd * 2

                Text {
                    id: errorText
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMd
                    text: VpnFacade.loginError
                    color: Theme.danger
                    font.pixelSize: Theme.fontCaption + 1
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    maximumLineCount: 6
                    elide: Text.ElideRight
                }
            }

            Text {
                visible: VpnFacade.cliVersion.length > 0
                text: qsTr("Proton VPN CLI %1").arg(VpnFacade.cliVersion)
                color: Theme.textHint
                font.pixelSize: Theme.fontCaption
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Theme.spacingSm
            }
        }
    }
}
