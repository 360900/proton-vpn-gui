import QtQuick
import QtQuick.Layouts
import Vela

// Shown while the install / login checks run at startup.
Item {
    ColumnLayout {
        anchors.centerIn: parent
        spacing: Theme.spacingLg

        Image {
            source: Theme.dark
                    ? "qrc:/assets/vela-logo.svg"
                    : "qrc:/assets/vela-logo-light.svg"
            sourceSize: Qt.size(220, 56)
            fillMode: Image.PreserveAspectFit
            Layout.alignment: Qt.AlignHCenter
        }

        PSpinner {
            Layout.alignment: Qt.AlignHCenter
            size: 28
        }

        Text {
            text: qsTr("Starting…")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
