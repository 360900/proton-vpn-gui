import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ProtonVpnGui

// Account details + sign-out.
Item {
    id: accountView
    property string title: qsTr("Account")

    ColumnLayout {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: Theme.spacingXl
        width: Math.min(parent.width - Theme.spacingXxl * 2, 460)
        spacing: Theme.spacingLg

        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            implicitHeight: infoCol.implicitHeight + Theme.spacingXl * 2

            ColumnLayout {
                id: infoCol
                anchors.fill: parent
                anchors.margins: Theme.spacingXl
                spacing: Theme.spacingMd

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Username")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                        Layout.preferredWidth: 110
                    }
                    Text {
                        text: VpnFacade.username.length > 0 ? VpnFacade.username : "-"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }

                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Plan")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                        Layout.preferredWidth: 110
                    }
                    // The CLI only reveals free vs paid - never the exact plan
                    // (Plus, Unlimited, ...), so don't claim a specific one.
                    Text {
                        text: {
                            switch (VpnFacade.plan) {
                            case VpnFacade.Free: return qsTr("Proton Free")
                            case VpnFacade.Paid: return qsTr("Paid plan")
                            default:             return qsTr("Checking…")
                            }
                        }
                        color: VpnFacade.plan === VpnFacade.Paid ? Theme.success : Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }
                    PBadge {
                        visible: VpnFacade.plan === VpnFacade.Paid
                        text: qsTr("All features")
                        badgeColor: Theme.success
                    }
                    Item { Layout.fillWidth: true }
                }

                Text {
                    visible: VpnFacade.plan === VpnFacade.Paid
                    text: qsTr("The command-line tool this app is built on reports " +
                               "whether a plan is paid, but not which one (Plus, " +
                               "Unlimited, …). All VPN features are available either way.")
                    color: Theme.textHint
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Text {
                    visible: VpnFacade.plan === VpnFacade.Free
                    textFormat: Text.RichText
                    text: qsTr("Upgrade to unlock all locations and features: " +
                               "<a href='https://protonvpn.com/pricing'>protonvpn.com/pricing</a>")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    linkColor: Theme.accent
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
            }
        }

        PButton {
            text: qsTr("Sign Out")
            variant: "danger"
            Layout.alignment: Qt.AlignHCenter
            onClicked: VpnFacade.signOut()
        }
    }
}
