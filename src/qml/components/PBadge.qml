import QtQuick
import ProtonVpnGui

// Small pill label for server features (P2P, TOR) and tier markers (PLUS).
Rectangle {
    id: control

    property string text: ""
    property color badgeColor: Theme.accent

    radius: Theme.radiusPill
    color: Qt.alpha(badgeColor, 0.16)
    implicitWidth: label.implicitWidth + Theme.spacingSm * 2
    implicitHeight: label.implicitHeight + 4

    Text {
        id: label
        anchors.centerIn: parent
        text: control.text
        color: control.badgeColor
        font.pixelSize: Theme.fontCaption - 1
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.5
    }
}
