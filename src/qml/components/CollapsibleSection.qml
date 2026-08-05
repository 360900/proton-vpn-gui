import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Vela

// CollapsibleSection - a labelled, expandable/collapsible vertical group.
// Replaces the old side-panel "tab" row with three independent foldable
// sections (All / Favorites / Recents). The header is a wide clickable row
// with a rotating chevron; the body animates its height between natural and
// zero. `expanded` drives the state and is two-way bindable from the parent.
Item {
    id: section

    property string label: ""
    property bool expanded: true
    // When false the section is pinned open: the header shows no chevron and
    // cannot collapse the body (used for the permanent "All countries" list).
    property bool collapsible: true
    property int collapsedHeight: 32
    default property alias body: bodyHolder.data

    // Make the section participate in ColumnLayout sizing with an explicit
    // preferred height so the height animation is layout-driven and smooth.
    implicitHeight: expanded ? (header.implicitHeight + Math.max(bodyHolder.implicitHeight, 0)) : header.implicitHeight
    Behavior on implicitHeight { NumberAnimation { duration: Theme.durNormal; easing.type: Easing.OutCubic } }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        AbstractButton {
            id: header
            Layout.fillWidth: true
            implicitHeight: section.collapsedHeight
            hoverEnabled: true
            enabled: section.collapsible
            onClicked: section.expanded = !section.expanded

            background: Rectangle {
                radius: Theme.radiusSm
                color: section.collapsible
                     ? (header.down ? Theme.surfaceActive
                        : header.hovered ? Theme.surfaceHover : "transparent")
                     : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.durFast } }
            }

            contentItem: RowLayout {
                spacing: Theme.spacingSm

                PIcon {
                    name: "chevron-down"
                    size: 12
                    visible: section.collapsible
                    color: Theme.textSecondary
                    rotation: section.expanded ? 0 : -90
                    Behavior on rotation { NumberAnimation { duration: Theme.durFast; easing.type: Easing.OutCubic } }
                }

                Text {
                    text: section.label
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontCaption + 1
                    font.weight: Font.DemiBold
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 1
                    Layout.fillWidth: true
                }
            }
        }

        // Body holder - clips its children and animates height. We bind height
        // to a NumberAnimation for the collapse; Layout.fillHeight is driven
        // by the parent when this section is the "open" one.
        ColumnLayout {
            id: bodyHolder
            Layout.fillWidth: true
            Layout.fillHeight: section.expanded
            spacing: 0
            clip: true
            opacity: section.expanded ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: Theme.durFast } }
        }
    }
}