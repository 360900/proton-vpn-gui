import QtQuick
import ProtonVpnGui

// Indeterminate activity spinner - a rotating arc drawn with Canvas,
// DPI-crisp at any size.
Item {
    id: control

    property int size: 22
    property color color: Theme.accent
    property int lineWidth: Math.max(2, Math.round(size / 9))

    implicitWidth: size
    implicitHeight: size
    visible: true

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const c = width / 2
            const r = (width - control.lineWidth) / 2
            ctx.strokeStyle = String(control.color)
            ctx.lineWidth = control.lineWidth
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(c, c, r, 0, Math.PI * 1.4)
            ctx.stroke()
        }

        Connections {
            target: control
            function onColorChanged() { canvas.requestPaint() }
        }
    }

    RotationAnimation on rotation {
        running: control.visible
        loops: Animation.Infinite
        from: 0
        to: 360
        duration: 900
    }
}
