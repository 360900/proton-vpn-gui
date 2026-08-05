import QtQuick
import Vela

// GlobeView - a stylized wireframe globe that plots the user's country
// and the connected country and draws a great-circle arc between them.
// Motion contract ("Proton calm"):
//   - while `traveling` (connecting), a comet travels the arc one way
//   - once settled (connected), every animation stops: static arc,
//     destination ring, zero repaints until state/size/theme changes
//   - pulse() fires a single soft ring at the destination (connect edge)
// Approximate: orthographic projection of graticule lines + slerped arc.
Item {
    id: control

    // Destination country code (the connected server's country).
    property string countryCode: ""
    readonly property string originCountryCode: GeoCoords.userCountry()
    // True while connecting: the comet travels. False: fully static.
    property bool traveling: false
    // Disconnecting reverses the route, returning from the VPN endpoint to
    // the user's endpoint.
    property bool reverseTraveling: false

    // Fires the one-shot destination pulse (Connecting -> Connected edge).
    function pulse() {
        if (Theme.reducedMotion || control.hasEndpoints === false)
            return
        pulseAnim.restart()
    }

    opacity: 1.0
    visible: true

    Component.onCompleted: control.updateEndpoints()
    onCountryCodeChanged: control.updateEndpoints()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    onTravelingChanged: {
        beamAnim.restart()
        canvas.requestPaint()
    }
    onReverseTravelingChanged: {
        beamAnim.restart()
        canvas.requestPaint()
    }

    property real animProgress: 0
    property real pulseProgress: 1 // 1 = idle (nothing drawn)
    property real originLat: 0
    property real originLon: 0
    property real destLat: 0
    property real destLon: 0
    property real midLat: 0
    property real midLon: 0
    property bool hasEndpoints: false

    function updateEndpoints() {
        // A QPointF of (0, 0) is the sentinel for "unknown" from the C++ helper.
        const isValid = p => p && (p.x !== 0 || p.y !== 0)

        if (control.countryCode.length === 0) {
            control.hasEndpoints = false
            canvas.requestPaint()
            return
        }
        // Origin: user country centroid, if known.
        const user = GeoCoords.userCountry()
        const o = user.length === 2 ? GeoCoords.coordsFor(user) : null
        const d = GeoCoords.coordsFor(control.countryCode)

        if (!isValid(d)) {
            control.hasEndpoints = false
            canvas.requestPaint()
            return
        }

        control.originLat = isValid(o) ? o.x : 20.0
        control.originLon = isValid(o) ? o.y : 0.0
        control.destLat = d.x
        control.destLon = d.y

        // Midpoint via simple average; good enough visually.
        let mLat = (control.originLat + control.destLat) / 2
        let mLon = (control.originLon + control.destLon) / 2
        // Shortest longitude wrap-around for the view center.
        if (mLon > 180) mLon -= 360
        if (mLon < -180) mLon += 360
        control.midLat = mLat
        control.midLon = mLon
        control.hasEndpoints = true
        canvas.requestPaint()
    }

    Connections {
        target: control
        function onOriginLatChanged() { canvas.requestPaint() }
        function onDestLatChanged() { canvas.requestPaint() }
        function onMidLatChanged() { canvas.requestPaint() }
        function onAnimProgressChanged() { canvas.requestPaint() }
        function onPulseProgressChanged() { canvas.requestPaint() }
    }

    // Repaint on a theme flip - canvas colors don't bind automatically.
    Connections {
        target: Theme
        function onDarkChanged() { canvas.requestPaint() }
    }

    // Traveling comet: one-way along the arc while connecting. One-way reads
    // as "data flowing to the destination"; ping-pong reads as indecision.
    // Driving the loop off `running` is not enough on its own: if `traveling`
    // flips true while `animProgress` is already at the loop end, the prop
    // stays there. The `onTravelingChanged` and `onReverseTravelingChanged`
    // handlers above explicitly restart the animation on every edge.
    SequentialAnimation on animProgress {
        id: beamAnim
        running: control.traveling && control.visible && control.hasEndpoints
                 && Theme.reducedMotion === false
        loops: Animation.Infinite

        NumberAnimation {
            from: control.reverseTraveling ? 1 : 0
            to:   control.reverseTraveling ? 0 : 1
            duration: 1800
            easing.type: Easing.InOutSine
        }
    }

    // One-shot success pulse at the destination marker.
    NumberAnimation {
        id: pulseAnim
        target: control
        property: "pulseProgress"
        from: 0
        to: 1
        duration: 900
        easing.type: Theme.easeStandard
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        renderStrategy: Canvas.Threaded

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()

            const cx = width / 2
            const cy = height / 2
            const r = Math.min(width, height) / 2 - Theme.spacingXs
            if (r <= 0) return

            // Globe disc fill.
            const grad = ctx.createRadialGradient(cx - r * 0.3, cy - r * 0.3,
                                                  r * 0.1, cx, cy, r)
            grad.addColorStop(0, Theme.dark ? "#23223A" : "#3B3568")
            grad.addColorStop(1, Theme.dark ? "#12111B" : "#2A2550")
            ctx.fillStyle = grad
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2)
            ctx.fill()

            // Rim highlighting.
            ctx.strokeStyle = Theme.accentMuted.toString()
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2)
            ctx.stroke()

            if (!control.hasEndpoints) {
                // Just a faint graticule so the empty globe still reads as a globe.
                control.drawGraticule(ctx, cx, cy, r, 0, 0)
                return
            }

            // Project in orthographic projection centered on the midpoint,
            // tilted so the great-circle arc sits roughly crosswise.
            const lat0 = control.midLat * Math.PI / 180
            const lon0 = control.midLon * Math.PI / 180

            function project(latDeg, lonDeg) {
                const lat = latDeg * Math.PI / 180
                const lon = lonDeg * Math.PI / 180
                const dlon = lon - lon0
                const cosLat = Math.cos(lat)
                const x = cosLat * Math.sin(dlon)
                const y = Math.cos(lat0) * Math.sin(lat)
                        - Math.sin(lat0) * cosLat * Math.cos(dlon)
                const z = Math.sin(lat0) * Math.sin(lat)
                        + Math.cos(lat0) * cosLat * Math.cos(dlon)
                return { x: cx + x * r, y: cy - y * r, front: z >= -0.02 }
            }

            // Simplified continent silhouettes. They are intentionally
            // stylized rather than geographically exact, but make the globe
            // read as a world map instead of an empty wireframe.
            function drawLandmass(points) {
                ctx.beginPath()
                let started = false
                for (let i = 0; i < points.length; ++i) {
                    const p = project(points[i][0], points[i][1])
                    if (p.front) {
                        if (started === false) {
                            ctx.moveTo(p.x, p.y)
                            started = true
                        } else {
                            ctx.lineTo(p.x, p.y)
                        }
                    }
                }
                if (started) {
                    ctx.closePath()
                    ctx.fillStyle = Qt.alpha(Theme.accent, 0.12).toString()
                    ctx.fill()
                    ctx.strokeStyle = Qt.alpha(Theme.accent, 0.28).toString()
                    ctx.lineWidth = 0.8
                    ctx.stroke()
                }
            }

            drawLandmass([
                [72, -168], [70, -140], [60, -125], [52, -105],
                [48, -88], [30, -80], [15, -96], [25, -115],
                [35, -135], [50, -155]
            ])
            drawLandmass([
                [15, -82], [8, -60], [-10, -48], [-35, -55],
                [-55, -70], [-35, -78], [-10, -80]
            ])
            drawLandmass([
                [72, -55], [82, -25], [72, 5], [60, 15],
                [50, 5], [58, -35]
            ])
            drawLandmass([
                [70, 10], [68, 45], [62, 80], [68, 125],
                [55, 165], [38, 145], [25, 115], [20, 75],
                [30, 45], [38, 20]
            ])
            drawLandmass([
                [37, -18], [37, 15], [25, 35], [5, 45],
                [-35, 25], [-35, 5], [-5, -15], [20, -18]
            ])
            drawLandmass([
                [-10, 110], [-8, 155], [-25, 155], [-40, 142],
                [-38, 115], [-22, 108]
            ])
            drawLandmass([
                [-35, 165], [-42, 178], [-48, 170], [-45, 165]
            ])

            // Graticule (in the midpoint-centered frame).
            {
                // Latitude rings.
                for (let lat = -75; lat <= 75; lat += 30) {
                    ctx.beginPath()
                    let started = false
                    for (let lon = -180; lon <= 180; lon += 4) {
                        const p = project(lat, lon)
                        if (p.front) {
                            if (!started) { ctx.moveTo(p.x, p.y); started = true }
                            else ctx.lineTo(p.x, p.y)
                        } else {
                            started = false
                        }
                    }
                    ctx.strokeStyle = Qt.alpha(Theme.accent, 0.18).toString()
                    ctx.lineWidth = 0.8
                    ctx.stroke()
                }
                // Longitude meridians.
                for (let lon = -180; lon <= 180; lon += 30) {
                    ctx.beginPath()
                    let started = false
                    for (let lat = -89; lat <= 89; lat += 4) {
                        const p = project(lat, lon)
                        if (p.front) {
                            if (!started) { ctx.moveTo(p.x, p.y); started = true }
                            else ctx.lineTo(p.x, p.y)
                        } else {
                            started = false
                        }
                    }
                    ctx.strokeStyle = Qt.alpha(Theme.accent, 0.18).toString()
                    ctx.lineWidth = 0.8
                    ctx.stroke()
                }
            }

            // Great-circle arc via slerp parameter t in [0,1].
            const aLat = control.originLat * Math.PI / 180
            const aLon = control.originLon * Math.PI / 180
            const bLat = control.destLat * Math.PI / 180
            const bLon = control.destLon * Math.PI / 180

            // Convert to unit vectors. Handle antipodal-ish wrap.
            const ax = Math.cos(aLat) * Math.cos(aLon)
            const ay = Math.cos(aLat) * Math.sin(aLon)
            const az = Math.sin(aLat)
            const bx = Math.cos(bLat) * Math.cos(bLon)
            const by = Math.cos(bLat) * Math.sin(bLon)
            const bz = Math.sin(bLat)
            const dot = ax * bx + ay * by + az * bz
            const omega = Math.acos(Math.max(-1, Math.min(1, dot)))
            const sinOmega = Math.sin(omega)
            const sl = (t) => {
                if (sinOmega < 1e-6) return { x: ax, y: ay, z: az }
                const s0 = Math.sin((1 - t) * omega) / sinOmega
                const s1 = Math.sin(t * omega) / sinOmega
                return {
                    x: s0 * ax + s1 * bx,
                    y: s0 * ay + s1 * by,
                    z: s0 * az + s1 * bz,
                }
            }
            const toLatLon = (v) => {
                const lat = Math.asin(Math.max(-1, Math.min(1, v.z))) * 180 / Math.PI
                const lon = Math.atan2(v.y, v.x) * 180 / Math.PI
                return { lat, lon }
            }

            // Full arc path.
            ctx.beginPath()
            ctx.strokeStyle = Qt.alpha(Theme.accent, 0.55).toString()
            ctx.lineWidth = 1.6
            ctx.lineCap = "round"
            for (let i = 0; i <= 60; ++i) {
                const v = sl(i / 60)
                const ll = toLatLon(v)
                const p = project(ll.lat, ll.lon)
                if (p.front) {
                    if (i === 0) ctx.moveTo(p.x, p.y)
                    else ctx.lineTo(p.x, p.y)
                }
            }
            ctx.stroke()

            // Traveling comet head at animProgress (only while connecting).
            if (control.traveling && Theme.reducedMotion === false) {
                const v = sl(control.animProgress)
                const ll = toLatLon(v)
                const p = project(ll.lat, ll.lon)
                if (p.front) {
                    ctx.beginPath()
                    ctx.fillStyle = Theme.accent.toString()
                    ctx.arc(p.x, p.y, 3.2, 0, Math.PI * 2)
                    ctx.fill()
                    ctx.beginPath()
                    ctx.strokeStyle = Qt.alpha(Theme.accent, 0.4).toString()
                    ctx.lineWidth = 2
                    ctx.arc(p.x, p.y, 6, 0, Math.PI * 2)
                    ctx.stroke()
                }
            }

            // Endpoints: origin (dim) and destination (accent ring).
            {
                const po = project(control.originLat, control.originLon)
                if (po.front) {
                    ctx.beginPath()
                    ctx.fillStyle = Theme.textSecondary.toString()
                    ctx.arc(po.x, po.y, 2.4, 0, Math.PI * 2)
                    ctx.fill()
                }
                const pd = project(control.destLat, control.destLon)
                if (pd.front) {
                    ctx.beginPath()
                    ctx.strokeStyle = Theme.accent.toString()
                    ctx.lineWidth = 1.6
                    ctx.arc(pd.x, pd.y, 4, 0, Math.PI * 2)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.fillStyle = Theme.accent.toString()
                    ctx.arc(pd.x, pd.y, 1.6, 0, Math.PI * 2)
                    ctx.fill()

                    // One-shot connect pulse: expanding, fading ring.
                    if (control.pulseProgress < 1) {
                        const t = control.pulseProgress
                        ctx.beginPath()
                        ctx.strokeStyle = Qt.alpha(Theme.accent, 0.7 * (1 - t)).toString()
                        ctx.lineWidth = 1.6
                        ctx.arc(pd.x, pd.y, 4 + t * 14, 0, Math.PI * 2)
                        ctx.stroke()
                    }
                }
            }
        }

        // Repaint when the canvas becomes visible; endpoint changes are handled
        // by the Connections at the item level.
        onVisibleChanged: requestPaint()
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacingXs
        implicitWidth: legend.implicitWidth + Theme.spacingSm * 2
        implicitHeight: 22
        radius: Theme.radiusPill
        color: Qt.alpha(Theme.surface, 0.86)
        border.color: Qt.alpha(Theme.borderStrong, 0.75)
        border.width: 1

        Row {
            id: legend
            anchors.centerIn: parent
            spacing: Theme.spacingXs

            FlagIcon {
                countryCode: control.originCountryCode
                flagWidth: 18
            }
            Text {
                text: control.originCountryCode === control.countryCode ? "=" : "→"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                anchors.verticalCenter: parent.verticalCenter
            }
            FlagIcon {
                countryCode: control.countryCode
                flagWidth: 18
            }
        }
    }

    // Helper: draw a plain graticule for the empty-globe fallback.
    function drawGraticule(ctx, cx, cy, r, lat0, lon0) {
        const toRad = Math.PI / 180
        for (let lat = -60; lat <= 60; lat += 30) {
            ctx.beginPath()
            for (let lon = -180; lon <= 180; lon += 6) {
                const y = cy - Math.sin(lat * toRad) * r
                const x = cx + Math.sin((lon - lon0) * toRad) * Math.cos(lat * toRad) * r
                if (lon === -180) ctx.moveTo(x, y)
                else ctx.lineTo(x, y)
            }
            ctx.strokeStyle = Qt.alpha(Theme.accent, 0.12).toString()
            ctx.lineWidth = 0.6
            ctx.stroke()
        }
        for (let lon = -150; lon <= 150; lon += 30) {
            ctx.beginPath()
            for (let lat = -89; lat <= 89; lat += 6) {
                const y = cy - Math.sin(lat * toRad) * r
                const x = cx + Math.sin((lon - lon0) * toRad) * Math.cos(lat * toRad) * r
                if (lat === -89) ctx.moveTo(x, y)
                else ctx.lineTo(x, y)
            }
            ctx.strokeStyle = Qt.alpha(Theme.accent, 0.12).toString()
            ctx.lineWidth = 0.6
            ctx.stroke()
        }
    }
}
