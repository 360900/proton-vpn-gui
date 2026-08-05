import QtQuick
import Vela

// Tinted monochrome icon from :/assets/<name>.svg, rendered by the C++
// image provider at the correct devicePixelRatio (crisp on HiDPI).
Image {
    id: control

    property string name: ""
    property int size: 18
    property color color: Theme.textSecondary

    source: name.length > 0
            ? "image://icon/" + name + "?" + String(color.toString())
            : ""
    sourceSize: Qt.size(size, size)
    width: size
    height: size
    fillMode: Image.PreserveAspectFit
}
