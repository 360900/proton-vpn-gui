import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Vela

// The central connection status card: state heading, server details,
// session timer, forwarded port, and the primary connect/disconnect action.
// While connecting / connected a stylized globe plots the path from the
// user's country to the connected country plus a traveling beam.
//
// Motion contract ("Proton calm"): exactly one activity indicator (the
// status dot crossfades into a compact spinner), a single one-shot success
// ripple on the connect edge, and conditional sections that reveal with a
// synchronized height + fade. Nothing loops while idle.
Rectangle {
    id: card

    // Favorites injected by the parent view (same source as the sidebar list).
    property var favorites: []
    // The sidebar is the primary favorites surface when it is expanded.
    property bool showFavoritesInCard: false
    // Free plans can only use the fastest server, so favorite rows are inert.
    property bool freeUser: false

    readonly property int conn: VpnFacade.connState
    readonly property bool isConnected:    conn === VpnFacade.Connected
    readonly property bool isTransitioning:conn === VpnFacade.Connecting ||
                                            conn === VpnFacade.Disconnecting
    readonly property bool isError:        conn === VpnFacade.Error

    // Session timer text, refreshed every second while connected.
    property string sessionTime: "00:00:00"
    // For the success ripple on the Connecting -> Connected transition.
    property bool wasTransitioning: false

    // Section reveal flags (drive the height + fade pattern below).
    readonly property bool showGlobe: (isTransitioning || isConnected)
                                      && (VpnFacade.connectedCountryCode.length === 2 ||
                                          VpnFacade.connectionTargetCountryCode.length === 2)
    readonly property bool showError: isError && VpnFacade.stateInfo.length > 0
    readonly property bool showDetails: isConnected
    readonly property bool showLastLocation: !isConnected && !isTransitioning &&
                                             VpnFacade.lastLocationCountryCode.length > 0
    readonly property bool showFavorites: showFavoritesInCard && favorites.length > 0

    Timer {
        interval: 1000
        running: card.isConnected
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            const since = VpnFacade.connectedSince
            const ms = isNaN(since.getTime()) ? 0 : Date.now() - since.getTime()
            const secs = Math.max(0, Math.floor(ms / 1000))
            const h = Math.floor(secs / 3600)
            const m = Math.floor((secs % 3600) / 60)
            const s = secs % 60
            card.sessionTime = String(h).padStart(2, "0") + ":" +
                               String(m).padStart(2, "0") + ":" +
                               String(s).padStart(2, "0")
        }
    }

    radius: Theme.radiusLg
    color: Theme.surface
    border.color: Theme.border
    border.width: 1
    implicitHeight: content.implicitHeight + Theme.spacingXl * 2
    // Center the card; cap its height so the favorites list can scroll
    // internally without pushing the card past the viewport.
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.verticalCenter: parent.verticalCenter
    width: Math.min((parent.width ?? 0) - Theme.spacingXxl * 2, 460)
    height: Math.min(implicitHeight, (parent.height ?? 0) - Theme.spacingXl * 2)
    clip: false

    // Track the Connecting -> Connected edge for the success ripple + globe pulse.
    Connections {
        target: VpnFacade
        function onConnStateChanged() {
            if (card.isTransitioning) {
                card.wasTransitioning = true
            } else if (card.wasTransitioning && card.isConnected) {
                card.wasTransitioning = false
                rippleAnim.restart()
                globe.pulse()
            } else {
                card.wasTransitioning = false
            }
        }
    }

    Flickable {
        id: contentViewport
        anchors.fill: parent
        anchors.margins: Theme.spacingXl
        contentWidth: width
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: PScrollBar {}

        ColumnLayout {
        id: content
            width: contentViewport.width
            spacing: Theme.spacingLg

        //  Status heading
        RowLayout {
            spacing: Theme.spacingMd
            Layout.fillWidth: true

            // Single activity indicator: the status dot crossfades into a
            // compact spinner while transitioning, plus the one-shot ripple.
            Item {
                implicitWidth: 18
                implicitHeight: 18
                Layout.alignment: Qt.AlignVCenter

                Rectangle {
                    id: statusDot
                    anchors.centerIn: parent
                    implicitWidth: 12
                    implicitHeight: 12
                    radius: 6
                    color: card.isConnected ? Theme.success
                         : card.isError ? Theme.danger : Theme.textHint
                    opacity: card.isTransitioning ? 0 : 1
                    Behavior on color { ColorAnimation { duration: Theme.dur(Theme.durFast) } }
                    Behavior on opacity { NumberAnimation { duration: Theme.dur(Theme.durFast) } }
                }

                PSpinner {
                    anchors.centerIn: parent
                    size: 18
                    color: Theme.accent
                    opacity: card.isTransitioning ? 1 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: Theme.dur(Theme.durFast) } }
                }

                // Expanding success ripple on connected (one shot, circular).
                Rectangle {
                    id: ripple
                    anchors.centerIn: parent
                    implicitWidth: 12
                    implicitHeight: 12
                    radius: 6
                    color: "transparent"
                    border.color: Theme.success
                    border.width: 2
                    opacity: 0
                    scale: 1

                    ParallelAnimation {
                        id: rippleAnim
                        NumberAnimation { target: ripple; property: "scale"; from: 1; to: 2.8; duration: Theme.dur(380); easing.type: Theme.easeStandard }
                        NumberAnimation { target: ripple; property: "opacity"; from: 0.7; to: 0.0; duration: Theme.dur(380) }
                        onStopped: {
                            ripple.scale = 1
                            ripple.opacity = 0
                        }
                    }
                }
            }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true

                Text {
                    text: {
                        switch (card.conn) {
                        case VpnFacade.Connected:     return qsTr("Protected")
                        case VpnFacade.Connecting:    return qsTr("Connecting…")
                        case VpnFacade.Disconnecting: return qsTr("Disconnecting…")
                        case VpnFacade.Error:         return qsTr("Connection failed")
                        default:                      return qsTr("Unprotected")
                        }
                    }
                    color: card.isConnected ? Theme.success
                         : card.isError ? Theme.danger : Theme.textPrimary
                    font.pixelSize: Theme.fontDisplay
                    font.bold: true
                    Behavior on color { ColorAnimation { duration: Theme.dur(Theme.durFast) } }
                }

                RowLayout {
                    visible: card.isConnected && VpnFacade.connectedServer.length > 0
                    spacing: Theme.spacingSm

                    FlagIcon {
                        countryCode: VpnFacade.connectedCountryCode
                        flagWidth: 22
                    }
                    Text {
                        text: VpnFacade.connectedServer
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
            }
        }

        //  Globe visualization (connecting / connected)
        GlobeView {
            id: globe
            Layout.fillWidth: true
            Layout.preferredHeight: card.showGlobe ? 168 : 0
            Layout.alignment: Qt.AlignHCenter
            opacity: card.showGlobe ? 1 : 0
            visible: opacity > 0
            traveling: card.isTransitioning
            reverseTraveling: card.conn === VpnFacade.Disconnecting
            countryCode: VpnFacade.connectedCountryCode.length === 2
                         ? VpnFacade.connectedCountryCode
                         : VpnFacade.connectionTargetCountryCode

            Behavior on opacity { NumberAnimation { duration: Theme.dur(Theme.durNormal) } }
            Behavior on Layout.preferredHeight { NumberAnimation { duration: Theme.dur(Theme.durNormal); easing.type: Theme.easeStandard } }
        }

        //  Error banner
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: card.showError ? implicitHeight : 0
            opacity: card.showError ? 1 : 0
            visible: opacity > 0
            radius: Theme.radiusMd
            color: Qt.alpha(Theme.danger, 0.1)
            border.color: Qt.alpha(Theme.danger, 0.5)
            border.width: 1
            implicitHeight: errorCol.implicitHeight + Theme.spacingMd * 2

            Behavior on opacity { NumberAnimation { duration: Theme.dur(Theme.durNormal) } }
            Behavior on Layout.preferredHeight { NumberAnimation { duration: Theme.dur(Theme.durNormal); easing.type: Theme.easeStandard } }

            ColumnLayout {
                id: errorCol
                anchors.fill: parent
                anchors.margins: Theme.spacingMd
                spacing: Theme.spacingSm

                Text {
                    text: VpnFacade.stateInfo
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontCaption + 1
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    maximumLineCount: 4
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        //  Details grid (connected only)
        GridLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: card.showDetails ? implicitHeight : 0
            opacity: card.showDetails ? 1 : 0
            visible: opacity > 0
            columns: 2
            columnSpacing: Theme.spacingXl
            rowSpacing: Theme.spacingSm

            Behavior on opacity { NumberAnimation { duration: Theme.dur(Theme.durNormal) } }
            Behavior on Layout.preferredHeight { NumberAnimation { duration: Theme.dur(Theme.durNormal); easing.type: Theme.easeStandard } }

            component DetailLabel: Text {
                color: Theme.textHint
                font.pixelSize: Theme.fontCaption
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.5
            }
            component DetailValue: Text {
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
            }

            DetailLabel { text: qsTr("Session time") }
            DetailValue { text: card.sessionTime; font.family: "monospace" }

            DetailLabel { visible: VpnFacade.ipAddress.length > 0; text: qsTr("VPN IP") }
            DetailValue { visible: VpnFacade.ipAddress.length > 0; text: VpnFacade.ipAddress }

            DetailLabel { visible: VpnFacade.protocol.length > 0; text: qsTr("Protocol") }
            DetailValue { visible: VpnFacade.protocol.length > 0; text: VpnFacade.protocol }

            DetailLabel {
                visible: VpnFacade.forwardedPort > 0
                text: qsTr("Forwarded port")
            }
            RowLayout {
                visible: VpnFacade.forwardedPort > 0
                spacing: Theme.spacingSm

                DetailValue { text: String(VpnFacade.forwardedPort) }
                PIconButton {
                    iconName: "clipboard2-plus"
                    iconSize: 14
                    tooltip: qsTr("Copy port to clipboard")
                    onClicked: {
                        portCopyHelper.text = String(VpnFacade.forwardedPort)
                        portCopyHelper.selectAll()
                        portCopyHelper.copy()
                    }
                }
                // Invisible helper for clipboard access without C++.
                TextInput { id: portCopyHelper; visible: false }
            }
        }

        //  Primary action
        PButton {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingSm
            implicitHeight: 44
            variant: card.isConnected ? "secondary" : "primary"
            busy: false
            enabled: !card.isTransitioning
            text: {
                switch (card.conn) {
                case VpnFacade.Connected:     return qsTr("Disconnect")
                case VpnFacade.Connecting:    return qsTr("Connecting…")
                case VpnFacade.Disconnecting: return qsTr("Disconnecting…")
                default:                      return qsTr("Quick Connect")
                }
            }
            onClicked: card.isConnected ? VpnFacade.disconnect() : VpnFacade.connectFastest()
        }

        //  Last location (only while disconnected and history exists)
        AbstractButton {
            id: lastLocationBtn
            Layout.fillWidth: true
            Layout.preferredHeight: card.showLastLocation ? 40 : 0
            opacity: card.showLastLocation ? 1 : 0
            visible: opacity > 0
            hoverEnabled: true
            onClicked: VpnFacade.connectLastLocation()

            Behavior on opacity { NumberAnimation { duration: Theme.dur(Theme.durNormal) } }
            Behavior on Layout.preferredHeight { NumberAnimation { duration: Theme.dur(Theme.durNormal); easing.type: Theme.easeStandard } }

            background: Rectangle {
                radius: Theme.radiusMd
                color: lastLocationBtn.down ? Theme.surfaceActive
                     : lastLocationBtn.hovered ? Theme.surfaceHover : "transparent"
                border.width: 1
                border.color: lastLocationBtn.hovered ? Theme.borderStrong : Theme.border
                Behavior on color { ColorAnimation { duration: Theme.dur(Theme.durMicro) } }
                Behavior on border.color { ColorAnimation { duration: Theme.dur(Theme.durMicro) } }
            }

            contentItem: RowLayout {
                spacing: Theme.spacingSm

                FlagIcon {
                    countryCode: VpnFacade.lastLocationCountryCode
                    flagWidth: 20
                    Layout.leftMargin: Theme.spacingMd
                }
                Text {
                    text: VpnFacade.lastLocationCity.length > 0
                          ? qsTr("Last location: %1, %2")
                                .arg(VpnFacade.lastLocationCountryName)
                                .arg(VpnFacade.lastLocationCity)
                          : qsTr("Last location: %1")
                                .arg(VpnFacade.lastLocationCountryName)
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        //  Favorites (shown under the connection buttons as quick actions).
        ColumnLayout {
            id: favoritesSection
            Layout.fillWidth: true
            Layout.preferredHeight: card.showFavorites ? 180 : 0
            opacity: card.showFavorites ? 1 : 0
            visible: opacity > 0
            spacing: Theme.spacingXs

            Behavior on opacity { NumberAnimation { duration: Theme.dur(Theme.durNormal) } }
            Behavior on Layout.preferredHeight { NumberAnimation { duration: Theme.dur(Theme.durNormal); easing.type: Theme.easeStandard } }

            Text {
                text: qsTr("Favorites")
                color: Theme.textHint
                font.pixelSize: Theme.fontCaption
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
                Layout.leftMargin: Theme.spacingXs
            }

            ListView {
                id: favoritesList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                spacing: 2
                model: card.favorites
                ScrollBar.vertical: PScrollBar {}

                delegate: LocationRow {
                    required property var modelData
                    countryCode: modelData.countryCode
                    countryName: modelData.countryName
                    city: modelData.city
                    starred: AppSettings.isFavorite(modelData.countryCode, modelData.city)
                    freeUser: card.freeUser
                    onConnectRequested: (cc, city) => VpnFacade.connectTo(cc, city)
                    onToggleFavoriteRequested: (cc, city) =>
                        AppSettings.toggleFavorite(cc, city)
                }
            }
        }
    }
}
}
