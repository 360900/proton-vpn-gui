import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Vela

// A favorites / recents row. Same shape and behavior as CountryDelegate's
// header row, scoped to a single destination instead of a country list.
Rectangle {
    id: control

    property string countryCode: ""
    property string countryName: ""
    property string city: ""
    property string subtitle: ""
    property bool freeUser: false
    property bool starred: false

    signal connectRequested(string countryCode, string city)
    signal toggleFavoriteRequested(string countryCode, string city)

    width: ListView.view ? ListView.view.width : implicitWidth
    height: 44
    radius: Theme.radiusMd
    color: rowHover.hovered ? Theme.surfaceHover : "transparent"
    Behavior on color { ColorAnimation { duration: Theme.durFast } }

    HoverHandler {
        id: rowHover
        cursorShape: control.freeUser ? Qt.ForbiddenCursor : Qt.PointingHandCursor
    }

    // Click anywhere on the row body (except the trailing controls) connects.
    TapHandler {
        onTapped: {
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
                text: control.countryName
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            // Secondary line: city for city-scoped pins, or a date stamp on
            // recents. Empty for "fastest in country" favorites.
            Text {
                visible: control.city.length > 0 || control.subtitle.length > 0
                text: control.subtitle.length > 0
                      ? "%1 - %2".arg(control.city).arg(control.subtitle)
                      : control.city
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

        StarButton {
            // Opacity (not visible) keeps the slot occupied - no shifts.
            opacity: rowHover.hovered || control.starred ? 1 : 0
            enabled: opacity > 0
            starred: control.starred
            onClicked: control.toggleFavoriteRequested(control.countryCode, control.city)
            Behavior on opacity { NumberAnimation { duration: Theme.durFast } }
        }

        // Fixed-width slot: Connect while hovered.
        Item {
            Layout.preferredWidth: 76
            Layout.preferredHeight: 30

            PButton {
                anchors.fill: parent
                visible: rowHover.hovered && !control.freeUser
                text: qsTr("Connect")
                variant: "secondary"
                leftPadding: Theme.spacingSm
                rightPadding: Theme.spacingSm
                onClicked: control.connectRequested(control.countryCode, control.city)
            }
        }
    }
}
