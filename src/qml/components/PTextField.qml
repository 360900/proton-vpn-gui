import QtQuick
import QtQuick.Controls.Basic
import ProtonVpnGui

// Standard single-line input. `secret: true` masks input and shows a
// reveal toggle.
TextField {
    id: control

    property bool secret: false
    property bool revealed: false

    echoMode: secret && !revealed ? TextInput.Password : TextInput.Normal
    color: Theme.textPrimary
    placeholderTextColor: Theme.textHint
    font.pixelSize: Theme.fontBody
    selectionColor: Theme.accent
    selectedTextColor: Theme.textOnAccent
    implicitHeight: 38
    leftPadding: Theme.spacingMd
    rightPadding: secret ? implicitHeight + Theme.spacingSm : Theme.spacingMd
    verticalAlignment: TextInput.AlignVCenter

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.dark ? Qt.darker(Theme.surface, 1.25) : Theme.bgPage
        border.width: 1
        border.color: control.activeFocus ? Theme.accent
                     : control.hovered ? Theme.borderStrong : Theme.border

        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }

    PIconButton {
        visible: control.secret
        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        iconName: control.revealed ? "eye-hide" : "eye-show"
        iconSize: 18
        tooltip: control.revealed ? qsTr("Hide password") : qsTr("Show password")
        onClicked: control.revealed = !control.revealed
    }
}
