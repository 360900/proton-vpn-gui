import QtQuick
import QtQuick.Controls.Basic
import Vela

// Standard button. Variants: "primary" (accent fill), "secondary" (outline),
// "ghost" (borderless), "danger" (red fill).
Button {
    id: control

    property string variant: "primary"
    property bool busy: false

    readonly property bool isPrimary: variant === "primary"
    readonly property bool isDanger:  variant === "danger"
    readonly property bool isGhost:   variant === "ghost"

    implicitHeight: 38
    font.pixelSize: Theme.fontBody
    font.weight: Font.DemiBold
    hoverEnabled: true

    contentItem: Row {
        spacing: Theme.spacingSm
        anchors.centerIn: undefined

        PSpinner {
            visible: control.busy
            size: 16
            anchors.verticalCenter: parent.verticalCenter
            color: control.isPrimary || control.isDanger ? Theme.textOnAccent : Theme.accent
        }

            Text {
                text: control.text
                font: control.font
                color: {
                    if (!control.enabled)
                        return Theme.textHint
                    if (control.isPrimary || control.isDanger)
                        return Theme.textOnAccent
                    return control.isGhost ? Theme.textSecondary : Theme.textPrimary
                }
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: {
            if (control.isPrimary) {
                if (!control.enabled) return Theme.accentMuted
                if (control.down)     return Theme.accentPressed
                if (control.hovered)  return Theme.accentHover
                return Theme.accent
            }
            if (control.isDanger) {
                if (!control.enabled) return Qt.alpha(Theme.danger, 0.4)
                if (control.down)     return Qt.darker(Theme.danger, 1.2)
                if (control.hovered)  return Qt.lighter(Theme.danger, 1.08)
                return Theme.danger
            }
            // secondary / ghost
            if (control.down)    return Theme.surfaceActive
            if (control.hovered) return Theme.surfaceHover
            return "transparent"
        }
        border.width: control.variant === "secondary" ? 1 : 0
        border.color: control.hovered ? Theme.borderStrong : Theme.border

        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }

    leftPadding: Theme.spacingLg
    rightPadding: Theme.spacingLg
}
