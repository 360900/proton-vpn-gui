import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Vela

// All settings, one scrollable page with grouped sections.
// VPN settings round-trip into `protonvpn config set`; changing one while
// connected offers an apply-&-reconnect flow (the CLI requires a reconnect
// for them to take effect).
Item {
    id: settingsView
    property string title: qsTr("Settings")

    readonly property bool freeUser: VpnFacade.plan === VpnFacade.Free
    readonly property bool vpnBusy: VpnFacade.connState === VpnFacade.Connecting ||
                                    VpnFacade.connState === VpnFacade.Disconnecting
    readonly property bool vpnConnected: VpnFacade.connState === VpnFacade.Connected

    // Latest CLI settings snapshot ("kill-switch" -> "standard", ...).
    property var cliSettings: ({})

    Component.onCompleted: VpnFacade.refreshSettings()

    Connections {
        target: VpnFacade
        function onSettingsChanged(settings) { settingsView.cliSettings = settings }
        function onConfigApplied(output) { VpnFacade.refreshSettings() }
        function onConnStateChanged() {
            if (!settingsView.vpnBusy) VpnFacade.refreshSettings()
        }
    }

    function settingOn(key, onValue) {
        return (cliSettings[key] ?? "off") === (onValue ?? "on")
    }

    // Apply a CLI setting, going through the reconnect dialog when needed.
    function applySetting(key, value, needsReconnect) {
        if (needsReconnect && vpnConnected) {
            reconnectDialog.pendingKey = key
            reconnectDialog.pendingValue = value
            reconnectDialog.open()
        } else {
            VpnFacade.applyConfigValue(key, value)
        }
    }

    //  Apply & Reconnect dialog
    PDialog {
        id: reconnectDialog
        property string pendingKey: ""
        property string pendingValue: ""
        width: 380
        dialogTitle: qsTr("Apply and reconnect?")

        Text {
            text: qsTr("This setting only takes effect after reconnecting. " +
                       "The VPN will disconnect, apply the change, and reconnect " +
                       "to your last location.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Theme.spacingSm
            Layout.alignment: Qt.AlignRight

            PButton {
                text: qsTr("Cancel")
                variant: "ghost"
                onClicked: {
                    reconnectDialog.close()
                    VpnFacade.refreshSettings() // snap toggles back
                }
            }
            PButton {
                text: qsTr("Apply and Reconnect")
                onClicked: {
                    VpnFacade.applyConfigValueAndReconnect(
                        reconnectDialog.pendingKey, reconnectDialog.pendingValue)
                    reconnectDialog.close()
                }
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: Math.min(settingsView.width - Theme.spacingXl * 2, 560)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 0

            //  VPN --------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                SectionHeader { label: qsTr("VPN") ; Layout.fillWidth: true }
                PIconButton {
                    id: refreshBtn
                    iconName: "arrow-clockwise"
                    iconSize: 16
                    tooltip: qsTr("Refresh settings")
                    onClicked: VpnFacade.refreshSettings()

                    RotationAnimation on rotation {
                        running: settingsView.vpnBusy
                        loops: Animation.Infinite
                        from: 0; to: 360
                        duration: 900
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
                implicitHeight: vpnCol.implicitHeight
                opacity: settingsView.vpnBusy ? 0.5 : 1.0
                enabled: !settingsView.vpnBusy

                ColumnLayout {
                    id: vpnCol
                    width: parent.width
                    spacing: 0

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Kill Switch")
                        subtitle: qsTr("Block all internet traffic if the VPN connection drops.")
                        PSwitch {
                            checked: settingsView.settingOn("kill-switch", "standard")
                            onToggled: settingsView.applySetting(
                                           "kill-switch", checked ? "standard" : "off", true)
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("IPv6")
                        subtitle: qsTr("Tunnel IPv6 traffic through the VPN.")
                        PSwitch {
                            checked: settingsView.settingOn("ipv6")
                            onToggled: settingsView.applySetting("ipv6", checked ? "on" : "off", true)
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Anonymous crash reports")
                        subtitle: qsTr("Help Proton fix CLI issues by sending anonymous crash reports.")
                        PSwitch {
                            checked: settingsView.settingOn("anonymous-crash-reports")
                            onToggled: settingsView.applySetting(
                                           "anonymous-crash-reports", checked ? "on" : "off", false)
                        }
                    }

                    //  Paid-only settings
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        interactive: !settingsView.freeUser
                        title: qsTr("NetShield")
                        subtitle: qsTr("Block malware, ads, and trackers at the DNS level.")
                        PComboBox {
                            enabled: !settingsView.freeUser
                            model: [qsTr("Off"), qsTr("Malware only"), qsTr("Malware, ads and trackers")]
                            currentIndex: {
                                const v = settingsView.cliSettings["netshield"] ?? "off"
                                return v === "malware-only" ? 1
                                     : v === "malware-ads-trackers" ? 2 : 0
                            }
                            onActivated: index => settingsView.applySetting(
                                "netshield",
                                index === 1 ? "malware-only"
                              : index === 2 ? "malware-ads-trackers" : "off",
                                true)
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        interactive: !settingsView.freeUser
                        title: qsTr("VPN Accelerator")
                        subtitle: qsTr("Increase connection speeds with performance-enhancing technologies.")
                        PSwitch {
                            enabled: !settingsView.freeUser
                            checked: settingsView.settingOn("vpn-accelerator")
                            onToggled: settingsView.applySetting(
                                           "vpn-accelerator", checked ? "on" : "off", true)
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        interactive: !settingsView.freeUser
                        title: qsTr("Moderate NAT")
                        subtitle: qsTr("Ease NAT strictness for online gaming and peer connections.")
                        PSwitch {
                            enabled: !settingsView.freeUser
                            checked: settingsView.settingOn("moderate-nat")
                            onToggled: settingsView.applySetting(
                                           "moderate-nat", checked ? "on" : "off", true)
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        interactive: !settingsView.freeUser
                        title: qsTr("Port Forwarding")
                        subtitle: qsTr("Bypass firewalls to connect to P2P servers and local devices. " +
                                       "<a href='https://protonvpn.com/support/port-forwarding'>Learn more</a>")
                        PSwitch {
                            enabled: !settingsView.freeUser
                            checked: settingsView.settingOn("port-forwarding")
                            onToggled: settingsView.applySetting(
                                           "port-forwarding", checked ? "on" : "off", true)
                        }
                    }

                    SettingRow {
                        visible: VpnFacade.forwardedPort > 0
                        Layout.fillWidth: true
                        title: qsTr("Active forwarded port")
                        Text {
                            text: String(VpnFacade.forwardedPort)
                            color: Theme.success
                            font.pixelSize: Theme.fontBody
                            font.family: "monospace"
                            font.weight: Font.DemiBold
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        interactive: !settingsView.freeUser
                        title: qsTr("Custom DNS")
                        subtitle: qsTr("Use your own DNS servers (comma-separated IPs), or leave " +
                                       "empty to only enable the feature.")
                        PSwitch {
                            id: dnsSwitch
                            enabled: !settingsView.freeUser
                            checked: (settingsView.cliSettings["custom-dns"] ?? "off") !== "off"
                            onToggled: {
                                if (!checked)
                                    settingsView.applySetting("custom-dns", "off", true)
                                else
                                    settingsView.applySetting("custom-dns", "on", true)
                            }
                        }
                    }

                    SettingRow {
                        visible: dnsSwitch.checked && !settingsView.freeUser
                        Layout.fillWidth: true
                        title: qsTr("DNS addresses")
                        RowLayout {
                            spacing: Theme.spacingSm
                            PTextField {
                                id: dnsField
                                implicitWidth: 200
                                placeholderText: qsTr("1.1.1.1, 8.8.8.8")
                                text: {
                                    const v = settingsView.cliSettings["custom-dns"] ?? ""
                                    return (v === "on" || v === "off") ? "" : v
                                }
                            }
                            PButton {
                                text: qsTr("Apply")
                                variant: "secondary"
                                implicitHeight: 34
                                onClicked: settingsView.applySetting(
                                               "custom-dns",
                                               dnsField.text.split(",").map(s => s.trim())
                                                   .filter(s => s.length > 0).join(" ") || "on",
                                               true)
                            }
                        }
                    }
                }
            }

            //  App --------------------------------------------------------
            SectionHeader { label: qsTr("App") }

            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
                implicitHeight: appCol.implicitHeight

                ColumnLayout {
                    id: appCol
                    width: parent.width
                    spacing: 0

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Start with system")
                        subtitle: AppSettings.autoStartError.length > 0
                                  ? AppSettings.autoStartError
                                  : qsTr("Launch the app automatically when you log in.")
                        PSwitch {
                            checked: AppSettings.autoStart
                            onToggled: AppSettings.autoStart = checked
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        interactive: AppSettings.autoStart
                        title: qsTr("Auto-connect on startup")
                        subtitle: qsTr("Connect to the fastest server when the app starts.")
                        PSwitch {
                            enabled: AppSettings.autoStart
                            checked: AppSettings.autoConnect
                            onToggled: AppSettings.autoConnect = checked
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Start hidden in tray")
                        subtitle: qsTr("Don't show the main window on launch.")
                        PSwitch {
                            checked: AppSettings.startHidden
                            onToggled: AppSettings.startHidden = checked
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Desktop notifications")
                        subtitle: qsTr("Notify on connect and disconnect.")
                        PSwitch {
                            checked: AppSettings.notifications
                            onToggled: AppSettings.notifications = checked
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Reduce motion")
                        subtitle: qsTr("Minimize non-essential transitions and effects.")
                        PSwitch {
                            checked: AppSettings.reduceMotion
                            onToggled: AppSettings.reduceMotion = checked
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Recent connections")
                        subtitle: qsTr("How many recent locations to remember (0 disables).")
                        PComboBox {
                            implicitWidth: 90
                            model: [0, 3, 5, 10]
                            currentIndex: Math.max(0, model.indexOf(AppSettings.recentConnectionsCount))
                            onActivated: index => AppSettings.recentConnectionsCount = model[index]
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Check for updates")
                        subtitle: qsTr("Look for new releases on startup.")
                        PSwitch {
                            checked: AppSettings.checkForUpdates
                            onToggled: AppSettings.checkForUpdates = checked
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Log to file")
                        subtitle: qsTr("Mirror diagnostic output to rotating log files.")
                        PSwitch {
                            checked: AppSettings.logToFile
                            onToggled: AppSettings.logToFile = checked
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Clear data")
                        RowLayout {
                            spacing: Theme.spacingSm
                            PButton {
                                text: qsTr("Clear history")
                                variant: "secondary"
                                implicitHeight: 30
                                enabled: AppSettings.hasHistory()
                                onClicked: AppSettings.clearHistory()
                            }
                            PButton {
                                text: qsTr("Clear favorites")
                                variant: "secondary"
                                implicitHeight: 30
                                enabled: AppSettings.hasFavorites()
                                onClicked: AppSettings.clearFavorites()
                            }
                        }
                    }
                }
            }

            //  Appearance -------------------------------------------------
            SectionHeader { label: qsTr("Appearance") }

            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
                implicitHeight: appearanceCol.implicitHeight

                ColumnLayout {
                    id: appearanceCol
                    width: parent.width
                    spacing: 0

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Theme")
                        subtitle: qsTr("Follow the system, or force dark / light.")
                        PComboBox {
                            model: [qsTr("System"), qsTr("Dark"), qsTr("Light")]
                            currentIndex: ThemeController.mode
                            onActivated: index => ThemeController.mode = index
                        }
                    }
                }
            }

            //  About ------------------------------------------------------
            SectionHeader { label: qsTr("About") }

            Rectangle {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingXxl
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
                implicitHeight: aboutCol.implicitHeight

                ColumnLayout {
                    id: aboutCol
                    width: parent.width
                    spacing: 0

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("App version")
                        Text {
                            text: VpnFacade.appVersion
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontBody
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Proton VPN CLI version")
                        Text {
                            text: VpnFacade.cliVersion.length > 0 ? VpnFacade.cliVersion : "-"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontBody
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    SettingRow {
                        Layout.fillWidth: true
                        title: qsTr("Project")
                        subtitle: qsTr("Unofficial community app. Not affiliated with Proton AG. " +
                                       "Flags by <a href='https://github.com/lipis/flag-icons'>flag-icons</a>.")
                        PButton {
                            text: qsTr("GitHub")
                            variant: "secondary"
                            implicitHeight: 30
                            onClicked: Qt.openUrlExternally(
                                           "https://github.com/360900/vela")
                        }
                    }
                }
            }
        }
    }
}
