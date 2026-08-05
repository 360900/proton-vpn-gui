import QtQuick
import QtQuick.Controls.Basic
import Vela

// Favorite toggle star.
AbstractButton {
    id: control

    property bool starred: false

    implicitWidth: 26
    implicitHeight: 26
    hoverEnabled: true

    contentItem: Text {
        text: control.starred ? "★" : "☆"
        color: control.starred ? "#FFD24A"
             : control.hovered ? Theme.textPrimary : Theme.textHint
        font.pixelSize: 15
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.hovered ? Theme.surfaceHover : "transparent"
    }

    ToolTip.visible: hovered
    ToolTip.delay: 600
    ToolTip.text: starred ? qsTr("Remove from favorites") : qsTr("Add to favorites")
}
