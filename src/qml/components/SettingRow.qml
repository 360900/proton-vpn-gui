import QtQuick
import QtQuick.Layouts
import Vela

// One settings row: title + optional description on the left, an arbitrary
// control (switch, combo, button...) on the right.
Rectangle {
    id: control

    property string title: ""
    property string subtitle: ""
    default property alias controlItem: slot.data
    property bool interactive: true

    color: "transparent"
    implicitHeight: Math.max(textCol.implicitHeight, slot.implicitHeight) + Theme.spacingMd * 2
    opacity: interactive ? 1.0 : 0.5

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingLg
        anchors.rightMargin: Theme.spacingLg
        spacing: Theme.spacingLg

        ColumnLayout {
            id: textCol
            spacing: 2
            Layout.fillWidth: true

            Text {
                text: control.title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Text {
                visible: control.subtitle.length > 0
                text: control.subtitle
                textFormat: Text.RichText
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                wrapMode: Text.WordWrap
                linkColor: Theme.accent
                Layout.fillWidth: true
                onLinkActivated: link => Qt.openUrlExternally(link)
            }
        }

        Item {
            id: slot
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
