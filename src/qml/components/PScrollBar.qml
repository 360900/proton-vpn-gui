import QtQuick
import QtQuick.Controls.Basic
import ProtonVpnGui

// PScrollBar - the standard slim scrollbar: rounded handle that fades in
// while scrolling or hovered, and fades out when idle (Basic style drives
// `active`; we only supply the visuals). Use in place of plain ScrollBar.
ScrollBar {
    id: control

    orientation: Qt.Vertical
    policy: ScrollBar.AsNeeded
    hoverEnabled: true

    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 100
        radius: 3
        color: control.pressed ? Theme.textSecondary
             : control.hovered ? Theme.textHint
             : Theme.borderStrong
        opacity: control.active ? 0.9 : 0.0

        Behavior on opacity { NumberAnimation { duration: Theme.dur(Theme.durFast) } }
        Behavior on color { ColorAnimation { duration: Theme.dur(Theme.durMicro) } }
    }

    background: Item {}
}
