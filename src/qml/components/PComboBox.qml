import QtQuick
import QtQuick.Controls.Basic
import Vela

// Styled dropdown.
ComboBox {
    id: control

    implicitHeight: 34
    implicitWidth: 180
    font.pixelSize: Theme.fontBody
    hoverEnabled: true

    contentItem: Text {
        leftPadding: Theme.spacingMd
        rightPadding: 28
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.textPrimary : Theme.textHint
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - Theme.spacingMd
        anchors.verticalCenter: parent.verticalCenter
        text: "▾"
        color: Theme.textSecondary
        font.pixelSize: 11
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.dark ? Qt.darker(Theme.surface, 1.2) : Theme.bgPage
        border.width: 1
        border.color: control.activeFocus || control.popup.visible
                      ? Theme.accent
                      : control.hovered ? Theme.borderStrong : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }

    delegate: ItemDelegate {
        id: item
        required property var model
        required property int index
        width: control.width
        height: 32
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: item.model[control.textRole.length > 0 ? control.textRole : "modelData"]
                  ?? item.model.modelData ?? ""
            color: Theme.textPrimary
            font.pixelSize: Theme.fontBody
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: item.highlighted ? Theme.surfaceHover : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 280)
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
        }

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.surface
            border.color: Theme.borderStrong
            border.width: 1
        }
    }
}
