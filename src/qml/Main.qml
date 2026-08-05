import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Vela

ApplicationWindow {
    id: root

    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 600
    title: Qt.application.name
    color: Theme.bgPage

    // Closing the window hides to tray; quitting goes through the tray menu.
    onClosing: function(close) {
        close.accepted = false
        root.hide()
    }

    Connections {
        target: VpnFacade
        function onToast(message, success) {
            toastHost.show(message, success)
        }
    }

    // Invoked from C++ when Quit is chosen from the tray while connected.
    function openQuitDialog() {
        quitDialog.open()
    }

    // Invoked from C++ when a newer release exists.
    function openUpdateDialog(currentVersion, newVersion) {
        updateDialog.currentVersion = currentVersion
        updateDialog.newVersion = newVersion
        updateDialog.open()
    }

    // Invoked from C++ on first launch after an update.
    function openWhatsNewDialog(version) {
        whatsNewDialog.version = version
        whatsNewDialog.open()
    }

    PDialog {
        id: updateDialog
        property string currentVersion: ""
        property string newVersion: ""
        width: 400
        dialogTitle: qsTr("Update available")

        Text {
            text: qsTr("Version %1 is available (you have %2).")
                      .arg(updateDialog.newVersion).arg(updateDialog.currentVersion)
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Theme.spacingSm
            Layout.alignment: Qt.AlignRight

            PButton {
                text: qsTr("Later")
                variant: "ghost"
                onClicked: updateDialog.close()
            }
            PButton {
                text: qsTr("View release")
                onClicked: {
                    Qt.openUrlExternally(
                        "https://github.com/360900/vela/releases/latest")
                    updateDialog.close()
                }
            }
        }
    }

    PDialog {
        id: whatsNewDialog
        property string version: ""
        width: 400
        dialogTitle: qsTr("Updated to version %1").arg(whatsNewDialog.version)

        Text {
            textFormat: Text.RichText
            text: qsTr("See what changed in the " +
                       "<a href='https://github.com/360900/vela/releases'>" +
                       "release notes</a>.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
            linkColor: Theme.accent
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            onLinkActivated: link => Qt.openUrlExternally(link)
        }

        PButton {
            text: qsTr("Close")
            variant: "secondary"
            Layout.alignment: Qt.AlignRight
            onClicked: whatsNewDialog.close()
        }
    }

    PDialog {
        id: quitDialog
        width: Math.min(parent.width - Theme.spacingXl * 2, 440)
        dialogTitle: qsTr("Quit Vela?")

        Text {
            text: VpnFacade.forwardedPort > 0
                  ? qsTr("The VPN connection is active and a forwarded port is in use. " +
                         "Disconnecting will also stop the port-forwarding keep-alive.")
                  : qsTr("The VPN connection is active. Disconnect it before quitting " +
                         "the app.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Theme.spacingSm
            Layout.alignment: Qt.AlignRight

            PButton {
                text: qsTr("Keep app open")
                variant: "ghost"
                onClicked: quitDialog.close()
            }
            PButton {
                text: qsTr("Disconnect and Quit")
                onClicked: {
                    quitDialog.close()
                    root.hide()
                    VpnFacade.disconnectThenQuit()
                }
            }
        }
    }

    CrossfadeStack {
        anchors.fill: parent
        currentIndex: {
            switch (VpnFacade.uiState) {
            case VpnFacade.NotInstalled: return 1
            case VpnFacade.Login:        return 2
            case VpnFacade.Main:         return 3
            default:                     return 0 // Loading
            }
        }

        LoadingView {}
        NotInstalledView {}
        LoginView {}
        MainView {}
    }

    PToast {
        id: toastHost
        anchors.fill: parent
        z: 100
    }
}
