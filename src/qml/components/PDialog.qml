import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Vela

// PDialog - the standard modal dialog: themed surface, consistent padding,
// and a gentle fade + settle entrance/exit (the Basic style has none).
// Put content in the default property; it lands in a ColumnLayout.
Dialog {
    id: control

    // Convenience title row; leave empty to omit.
    property string dialogTitle: ""

    default property alias body: bodyCol.data

    anchors.centerIn: parent
    modal: true
    padding: Theme.spacingXl

    // Fade + settle in; plain fade out. Popup transitions target the popup item.
    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: Theme.dur(Theme.durNormal)
            }
            NumberAnimation {
                property: "scale"
                from: 0.97
                to: 1
                duration: Theme.dur(Theme.durNormal)
                easing.type: Theme.easeStandard
            }
        }
    }
    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: Theme.dur(Theme.durFast)
        }
    }

    Overlay.modal: Rectangle {
        color: Theme.overlay
        Behavior on opacity { NumberAnimation { duration: Theme.dur(Theme.durFast) } }
    }

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.surface
        border.color: Theme.borderStrong
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingLg

        Text {
            visible: control.dialogTitle.length > 0
            text: control.dialogTitle
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSubtitle
            font.bold: true
            Layout.fillWidth: true
        }

        ColumnLayout {
            id: bodyCol
            spacing: Theme.spacingLg
            Layout.fillWidth: true
        }
    }
}
