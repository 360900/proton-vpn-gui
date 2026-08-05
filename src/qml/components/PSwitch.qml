import QtQuick
import QtQuick.Controls.Basic
import Vela

// Toggle switch with animated knob.
Switch {
    id: control

    implicitWidth: 40
    implicitHeight: 22
    hoverEnabled: true

    indicator: Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: control.checked ? Theme.accent
             : (Theme.dark ? "#4A4658" : "#C8C3D6")
        opacity: control.enabled ? 1.0 : 0.45
        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        Rectangle {
            width: 16
            height: 16
            radius: 8
            color: "#FFFFFF"
            anchors.verticalCenter: parent.verticalCenter
            x: control.checked ? parent.width - width - 3 : 3
            Behavior on x { NumberAnimation { duration: Theme.durFast; easing.type: Easing.OutCubic } }
        }
    }
}
