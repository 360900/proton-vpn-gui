import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ProtonVpnGui

// A favorites / recents row: flag, "Country - City" (or fastest), connect on
// click. Used by the Favorites and Recents sidebar tabs.
Rectangle {
    id: control

    property string countryCode: ""
    property string countryName: ""
    property string city: ""
    property string subtitle: ""
    property bool freeUser: false

    signal connectRequested(string countryCode, string city)

    width: ListView.view ? ListView.view.width : implicitWidth
    height: 44
    radius: Theme.radiusMd
    color: area.containsMouse ? Theme.surfaceHover : "transparent"
    Behavior on color { ColorAnimation { duration: Theme.durFast } }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: control.freeUser ? Qt.ForbiddenCursor : Qt.PointingHandCursor
        onClicked: {
            if (!control.freeUser)
                control.connectRequested(control.countryCode, control.city)
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingSm
        spacing: Theme.spacingMd

        FlagIcon {
            countryCode: control.countryCode
            flagWidth: 26
        }

        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true

            Text {
                text: control.city.length > 0
                      ? "%1 - %2".arg(control.countryName).arg(control.city)
                      : qsTr("%1 - Fastest").arg(control.countryName)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Text {
                visible: control.subtitle.length > 0
                text: control.subtitle
                color: Theme.textHint
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        PBadge {
            visible: control.freeUser
            text: qsTr("Plus")
        }
    }
}
