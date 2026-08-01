import QtQuick
import QtQuick.Controls.Basic
import ProtonVpnGui

// Sidebar search input with a leading search glyph and clear button.
TextField {
    id: control

    placeholderText: qsTr("Search countries")
    color: Theme.textPrimary
    placeholderTextColor: Theme.textHint
    font.pixelSize: Theme.fontBody
    selectionColor: Theme.accent
    selectedTextColor: Theme.textOnAccent
    implicitHeight: 34
    leftPadding: 30
    rightPadding: 30
    verticalAlignment: TextInput.AlignVCenter

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.dark ? Qt.darker(Theme.surface, 1.2) : Theme.bgPage
        border.width: 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }

    // Simple magnifier glyph drawn with two primitives - no asset needed.
    Item {
        width: 14; height: 14
        anchors.left: parent.left
        anchors.leftMargin: 9
        anchors.verticalCenter: parent.verticalCenter
        opacity: 0.7

        Rectangle {
            width: 9; height: 9; radius: 4.5
            color: "transparent"
            border.color: Theme.textHint
            border.width: 1.5
        }
        Rectangle {
            width: 5; height: 1.5; radius: 1
            color: Theme.textHint
            rotation: 45
            x: 8; y: 9
        }
    }

    PIconButton {
        visible: control.text.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        iconSize: 12
        onClicked: control.text = ""

        contentItem: Text {
            text: "✕"
            color: Theme.textSecondary
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
