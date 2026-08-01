import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ProtonVpnGui

// One country row in the sidebar: header (flag, name, star, chevron) that
// expands to its city list. The header body toggles expansion; a Connect
// button replaces the chevron while the row is hovered (fixed-width slot,
// so nothing shifts). Clicking a city connects to that city.
//
// Hover state comes from HoverHandlers, which - unlike MouseArea's
// containsMouse - stay active while the pointer is over child controls, so
// buttons appearing on hover can never flicker.
Column {
    id: delegateRoot

    // Injected by the ListView delegate context.
    required property string name
    required property string code
    required property bool favorite
    required property var cities
    required property bool citiesLoaded
    required property bool citiesLoading

    property bool expanded: false
    property bool freeUser: false

    signal connectCountry(string code)
    signal connectCity(string code, string city)

    width: ListView.view ? ListView.view.width : implicitWidth

    Rectangle {
        id: headerRow
        width: parent.width
        height: 44
        radius: Theme.radiusMd
        color: headerHover.hovered ? Theme.surfaceHover : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        HoverHandler {
            id: headerHover
            cursorShape: Qt.PointingHandCursor
        }

        // Click on the row body (excluding the trailing controls) expands.
        TapHandler {
            onTapped: delegateRoot.expanded = !delegateRoot.expanded
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacingMd
            anchors.rightMargin: Theme.spacingSm
            spacing: Theme.spacingMd

            FlagIcon {
                countryCode: delegateRoot.code
                flagWidth: 26
            }

            Text {
                text: delegateRoot.name
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            PBadge {
                visible: delegateRoot.freeUser
                text: qsTr("Plus")
            }

            StarButton {
                // Opacity (not visible) keeps the slot occupied - no shifts.
                opacity: headerHover.hovered || delegateRoot.favorite ? 1 : 0
                enabled: opacity > 0
                starred: delegateRoot.favorite
                onClicked: delegateRoot.ListView.view.model.toggleFavorite(delegateRoot.code, "")
                Behavior on opacity { NumberAnimation { duration: Theme.durFast } }
            }

            // Fixed-width slot: chevron normally, Connect while hovered.
            Item {
                Layout.preferredWidth: 76
                Layout.preferredHeight: 30

                Text {
                    anchors.centerIn: parent
                    visible: !headerHover.hovered || delegateRoot.freeUser
                    text: delegateRoot.expanded ? "▴" : "▾"
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }

                PButton {
                    anchors.fill: parent
                    visible: headerHover.hovered && !delegateRoot.freeUser
                    text: qsTr("Connect")
                    variant: "secondary"
                    leftPadding: Theme.spacingSm
                    rightPadding: Theme.spacingSm
                    onClicked: delegateRoot.connectCountry(delegateRoot.code)
                }
            }
        }
    }

    onExpandedChanged: {
        if (expanded && !citiesLoaded && ListView.view)
            ListView.view.model.loadCities(code)
    }

    //  City list
    Column {
        id: cityColumn
        width: parent.width
        visible: delegateRoot.expanded
        padding: 0

        Item {
            visible: delegateRoot.citiesLoading
            width: parent.width
            height: 36
            PSpinner { anchors.centerIn: parent; size: 16 }
        }

        Repeater {
            model: delegateRoot.expanded ? delegateRoot.cities : []

            delegate: Rectangle {
                id: cityRow
                required property var modelData
                width: cityColumn.width
                height: 36
                radius: Theme.radiusMd
                color: cityHover.hovered ? Theme.surfaceHover : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.durFast } }

                HoverHandler {
                    id: cityHover
                    cursorShape: delegateRoot.freeUser ? Qt.ForbiddenCursor
                                                       : Qt.PointingHandCursor
                }

                TapHandler {
                    onTapped: {
                        if (!delegateRoot.freeUser)
                            delegateRoot.connectCity(delegateRoot.code, cityRow.modelData.name)
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingXl + Theme.spacingMd
                    anchors.rightMargin: Theme.spacingSm
                    spacing: Theme.spacingSm

                    Text {
                        text: cityRow.modelData.name
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Repeater {
                        model: cityRow.modelData.features.length > 0
                               ? cityRow.modelData.features.split(",") : []
                        delegate: PBadge {
                            required property var modelData
                            text: modelData.trim()
                            badgeColor: Theme.textSecondary
                        }
                    }

                    StarButton {
                        opacity: cityHover.hovered || cityRow.modelData.favorite ? 1 : 0
                        enabled: opacity > 0
                        starred: cityRow.modelData.favorite
                        onClicked: delegateRoot.ListView.view.model.toggleFavorite(
                                       delegateRoot.code, cityRow.modelData.name)
                        Behavior on opacity { NumberAnimation { duration: Theme.durFast } }
                    }
                }
            }
        }
    }
}
