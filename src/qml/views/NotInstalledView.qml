import QtQuick
import QtQuick.Layouts
import Vela

// Shown when the protonvpn CLI is not found on the host.
Item {
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacingXxl * 2, 440)
        spacing: Theme.spacingLg

        // Entrance: fade and scale in.
        opacity: 0
        scale: 0.96
        Component.onCompleted: {
            opacity = 1
            scale = 1
        }
        Behavior on opacity { NumberAnimation { duration: Theme.durSlow; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: Theme.durSlow; easing.type: Easing.OutCubic } }

        Image {
            // no-app-icon.svg is drawn in black; tint it with the theme's
            // secondary text color so it reads correctly on dark surfaces.
            source: "image://icon/no-app-icon?" + String(Theme.textSecondary)
            sourceSize: Qt.size(96, 96)
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            text: qsTr("Proton VPN CLI Not Found")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontTitle
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            text: qsTr("This app is a graphical frontend for Proton's official " +
                       "ProtonVPN command-line tool, which does not appear to be " +
                       "installed. Install it first, then check again.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        PButton {
            text: qsTr("View Installation Instructions")
            Layout.alignment: Qt.AlignHCenter
            onClicked: Qt.openUrlExternally("https://protonvpn.com/support/linux-cli")
        }

        PButton {
            text: qsTr("Check Again")
            variant: "secondary"
            Layout.alignment: Qt.AlignHCenter
            onClicked: VpnFacade.recheckInstalled()
        }
    }
}
