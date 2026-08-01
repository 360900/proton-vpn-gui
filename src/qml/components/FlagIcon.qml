import QtQuick
import ProtonVpnGui

// Country flag from the embedded flag set (4:3), DPI-crisp via the C++
// image provider, with a subtle border and a lettered fallback.
Item {
    id: control

    property string countryCode: ""
    property int flagWidth: 24

    readonly property int flagHeight: Math.round(flagWidth * 3 / 4)
    readonly property bool flagMissing: img.status === Image.Error || countryCode.length === 0

    implicitWidth: flagWidth
    implicitHeight: flagHeight

    Image {
        id: img
        anchors.fill: parent
        source: control.countryCode.length > 0
                ? "image://flag/" + control.countryCode.toLowerCase()
                : ""
        sourceSize: Qt.size(control.flagWidth, control.flagHeight)
        fillMode: Image.PreserveAspectFit
        visible: !control.flagMissing
    }

    Rectangle {
        anchors.fill: parent
        radius: 2
        color: control.flagMissing ? Theme.surfaceHover : "transparent"
        border.color: Qt.alpha(Theme.border, 0.8)
        border.width: 1

        Text {
            anchors.centerIn: parent
            visible: control.flagMissing
            text: control.countryCode
            color: Theme.textHint
            font.pixelSize: Math.max(8, control.flagHeight - 10)
        }
    }
}
