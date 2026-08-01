import QtQuick
import QtQuick.Layouts
import ProtonVpnGui

// PBanner - a slim inline banner for app-level notices (CLI version
// mismatch, prerelease builds). Reveals and dismisses with a gentle
// height + fade. Persistence of dismissal is the caller's job.
Rectangle {
    id: banner

    property string text: ""
    property string kind: "info" // "info" | "warning"
    property bool dismissible: true

    signal dismissed()

    readonly property color accentColor: kind === "warning" ? Theme.warning : Theme.accent

    radius: Theme.radiusMd
    color: Qt.alpha(accentColor, 0.10)
    border.color: Qt.alpha(accentColor, 0.35)
    border.width: 1
    clip: true

    // Reveal / dismiss motion.
    implicitHeight: row.implicitHeight + Theme.spacingMd * 2
    opacity: 1
    Behavior on implicitHeight { NumberAnimation { duration: Theme.dur(Theme.durNormal); easing.type: Theme.easeStandard } }
    Behavior on opacity { NumberAnimation { duration: Theme.dur(Theme.durFast) } }

    function dismiss() {
        opacity = 0
        implicitHeight = 0
        dismissTimer.start()
    }

    Timer {
        id: dismissTimer
        interval: Theme.dur(Theme.durNormal)
        onTriggered: banner.dismissed()
    }

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.spacingMd
        spacing: Theme.spacingMd

        Rectangle {
            implicitWidth: 8
            implicitHeight: 8
            radius: 4
            color: banner.accentColor
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            text: banner.text
            textFormat: Text.RichText
            color: Theme.textPrimary
            font.pixelSize: Theme.fontCaption + 1
            wrapMode: Text.WordWrap
            linkColor: Theme.accent
            Layout.fillWidth: true
            onLinkActivated: link => Qt.openUrlExternally(link)
        }

        PIconButton {
            visible: banner.dismissible
            iconSize: 12
            tooltip: qsTr("Dismiss")
            onClicked: banner.dismiss()

            contentItem: Text {
                text: "✕"
                color: Theme.textSecondary
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
