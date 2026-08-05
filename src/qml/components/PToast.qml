import QtQuick
import QtQuick.Layouts
import Vela

// PToast - in-app toast notifications. Instantiate once at window level and
// call show(message, success). Toasts slide in with a gentle fade/settle,
// hold, then fade and collapse out. At most maxVisible stack up; older ones
// are dismissed early.
Item {
    id: host

    property int maxVisible: 3
    property int holdMs: 3500

    function show(message, success) {
        // Over capacity: ask the oldest toast to leave early (it animates
        // out on its own; no need to wait for removal here).
        if (col.children.length >= maxVisible) {
            col.children[0].dismissSoon()
        }
        toastComp.createObject(col, { "message": message, "ok": success })
    }

    Column {
        id: col
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacingXl
        spacing: Theme.spacingSm
    }

    Component {
        id: toastComp

        Rectangle {
            id: toast

            property string message: ""
            property bool ok: true

            readonly property int fullHeight: row.implicitHeight + Theme.spacingMd * 2

            width: Math.min(row.implicitWidth + Theme.spacingLg * 2, 420)
            height: fullHeight
            radius: Theme.radiusMd
            color: Theme.surface
            border.color: Theme.borderStrong
            border.width: 1
            opacity: 0
            scale: 0.96
            clip: true // keeps the text inside while the exit collapses height

            function dismissSoon() {
                life.stop()
                exitAnim.restart()
            }

            Component.onCompleted: life.start()

            SequentialAnimation {
                id: life
                ParallelAnimation {
                    NumberAnimation {
                        target: toast
                        property: "opacity"
                        to: 1
                        duration: Theme.dur(Theme.durNormal)
                    }
                    NumberAnimation {
                        target: toast
                        property: "scale"
                        to: 1
                        duration: Theme.dur(Theme.durNormal)
                        easing.type: Theme.easeStandard
                    }
                }
                PauseAnimation { duration: host.holdMs }
                // Auto-dismiss: after the hold, fade and collapse out.
                ScriptAction { script: exitAnim.restart() }
            }

            SequentialAnimation {
                id: exitAnim
                ParallelAnimation {
                    NumberAnimation {
                        target: toast
                        property: "opacity"
                        to: 0
                        duration: Theme.dur(Theme.durFast)
                    }
                    NumberAnimation {
                        target: toast
                        property: "height"
                        to: 0
                        duration: Theme.dur(Theme.durFast)
                    }
                }
                ScriptAction { script: toast.destroy() }
            }

            RowLayout {
                id: row
                anchors.centerIn: parent
                width: parent.width - Theme.spacingLg * 2
                spacing: Theme.spacingMd

                Rectangle {
                    implicitWidth: 8
                    implicitHeight: 8
                    radius: 4
                    color: toast.ok ? Theme.success : Theme.danger
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: toast.message
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}
