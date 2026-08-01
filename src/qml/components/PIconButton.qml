import QtQuick
import QtQuick.Controls.Basic
import ProtonVpnGui

// Small square icon-only button. `icon` is a name under :/assets/
// (without .svg), served tinted by the icon image provider.
AbstractButton {
    id: control

    property string iconName: ""
    property int iconSize: 18
    property color iconColor: hovered ? Theme.textPrimary : Theme.textSecondary
    property string tooltip: ""

    implicitWidth: iconSize + Theme.spacingSm * 2
    implicitHeight: implicitWidth
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.down ? Theme.surfaceActive
             : control.hovered ? Theme.surfaceHover : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }

    contentItem: Item {
        PIcon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            color: control.iconColor
        }
    }

    ToolTip.visible: hovered && tooltip.length > 0
    ToolTip.delay: 600
    ToolTip.text: tooltip
}
