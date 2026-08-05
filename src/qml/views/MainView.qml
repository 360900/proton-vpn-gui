import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Vela

// The signed-in shell: a collapsible server-list sidebar on the left, and a
// right pane hosting the connection card with Settings/Account pushed on top.
// The sidebar folds to a slim icon rail; the three filters (All / Favorites /
// Recents) are independent collapsible sections inside the expanded sidebar.
Item {
    id: mainView

    readonly property bool freeUser: VpnFacade.plan === VpnFacade.Free
    readonly property bool sidebarCollapsed: AppSettings.sidebarCollapsed
    readonly property bool loggedIn: VpnFacade.uiState === VpnFacade.Main

    // The expanded sidebar tracks the window width (5/16 of it) so country
    // names keep their full width on larger windows, clamped to keep the
    // layout sane at the minimum size.
    readonly property int sidebarFractionBase: 5
    readonly property int sidebarFractionDiv: 16
    readonly property int sidebarExpandedMin: 300
    readonly property int sidebarExpandedMax: 460
    readonly property int sidebarExpandedWidth: Math.max(sidebarExpandedMin,
        Math.min(sidebarExpandedMax,
                 Math.round(mainView.width * sidebarFractionBase / sidebarFractionDiv)))
    readonly property int sidebarCollapsedWidth: 64

    ServerListModel {
        id: serverModel
    }

    // Load the country list the first time this view becomes active.
    Connections {
        target: VpnFacade
        function onUiStateChanged() {
            if (VpnFacade.uiState === VpnFacade.Main)
                rightStack.pop(null)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        //  Sidebar ---------------------------------------------------------
        Rectangle {
            id: sidebar
            Layout.preferredWidth: mainView.sidebarCollapsed
                                    ? mainView.sidebarCollapsedWidth
                                    : mainView.sidebarExpandedWidth
            Layout.fillHeight: true
            color: Theme.bgSidebar

            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: Theme.durNormal; easing.type: Easing.OutCubic }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: mainView.sidebarCollapsed ? Theme.spacingSm : Theme.spacingMd
                spacing: mainView.sidebarCollapsed ? Theme.spacingSm : Theme.spacingMd

                //  Logo + collapse toggle
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    Image {
                        // vela-logo.svg is a white sail (dark themes),
                        // vela-logo-light.svg a dark sail (light themes).
                        source: mainView.sidebarCollapsed
                                ? "qrc:/assets/vela-icon.svg"
                                : Theme.dark
                                  ? "qrc:/assets/vela-logo.svg"
                                  : "qrc:/assets/vela-logo-light.svg"
                        sourceSize: mainView.sidebarCollapsed
                                    ? Qt.size(28, 28)
                                    : Qt.size(150, 38)
                        fillMode: Image.PreserveAspectFit
                        Layout.alignment: Qt.AlignVCenter | (mainView.sidebarCollapsed ? Qt.AlignHCenter : Qt.AlignLeft)
                        Layout.fillWidth: mainView.sidebarCollapsed
                        Layout.leftMargin: mainView.sidebarCollapsed ? 0 : Theme.spacingSm
                        Layout.topMargin: mainView.sidebarCollapsed ? Theme.spacingSm : Theme.spacingSm
                    }

                    Item { Layout.fillWidth: true; visible: !mainView.sidebarCollapsed }

                    PIconButton {
                        visible: !mainView.sidebarCollapsed
                        iconName: "arrow-bar-left"
                        iconSize: 18
                        tooltip: qsTr("Collapse sidebar")
                        onClicked: AppSettings.sidebarCollapsed = true
                    }
                }

                //  Collapse-rail expand button (visible when collapsed)
                PIconButton {
                    visible: mainView.sidebarCollapsed
                    iconName: "arrow-bar-right"
                    iconSize: 20
                    tooltip: qsTr("Expand sidebar")
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: AppSettings.sidebarCollapsed = false
                }

                //  Search (expanded only)
                SearchField {
                    id: search
                    visible: !mainView.sidebarCollapsed
                    Layout.fillWidth: true
                    onTextChanged: serverModel.searchText = text
                }

                //  Filter sections (expanded only)
                ColumnLayout {
                    visible: !mainView.sidebarCollapsed
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.spacingSm

                    //  All countries
                    CollapsibleSection {
                        id: allSection
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        label: qsTr("All countries")
                        expanded: true
                        collapsible: false

                        ListView {
                            id: countryList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: serverModel
                            spacing: 2
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}

                            delegate: CountryDelegate {
                                freeUser: mainView.freeUser
                                onConnectCountry: code => VpnFacade.connectTo(code, "")
                                onConnectCity: (code, city) => VpnFacade.connectTo(code, city)
                            }

                            PSpinner {
                                x: countryList.width / 2 - width / 2
                                y: countryList.height / 2 - height / 2
                                visible: countryList.count === 0 &&
                                         (serverModel.loading || serverModel.totalCount === 0)
                            }
                        }
                    }

                    //  Favorites
                    CollapsibleSection {
                        id: favoritesSection
                        Layout.fillWidth: true
                        label: qsTr("Favorites")
                        expanded: false

                        ListView {
                            id: favoritesList
                            Layout.fillWidth: true
                            Layout.preferredHeight: favoritesSection.expanded ? 220 : 0
                            clip: true
                            model: serverModel.favorites
                            spacing: 2
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}

                            delegate: LocationRow {
                                required property var modelData
                                countryCode: modelData.countryCode
                                countryName: modelData.countryName
                                city: modelData.city
                                starred: AppSettings.isFavorite(modelData.countryCode,
                                                                modelData.city)
                                freeUser: mainView.freeUser
                                onConnectRequested: (cc, city) => VpnFacade.connectTo(cc, city)
                                onToggleFavoriteRequested: (cc, city) =>
                                    AppSettings.toggleFavorite(cc, city)
                            }

                            Text {
                                x: favoritesList.width / 2 - width / 2
                                y: favoritesList.height / 2 - height / 2
                                visible: favoritesList.count === 0
                                text: qsTr("No favorites yet.\nHover a location and press ☆.")
                                color: Theme.textHint
                                font.pixelSize: Theme.fontBody
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }

                    //  Recents
                    CollapsibleSection {
                        id: recentsSection
                        Layout.fillWidth: true
                        label: qsTr("Recents")
                        expanded: false

                        ListView {
                            id: recentsList
                            Layout.fillWidth: true
                            Layout.preferredHeight: recentsSection.expanded ? 180 : 0
                            clip: true
                            model: serverModel.recents
                            spacing: 2
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}

                            delegate: LocationRow {
                                required property var modelData
                                countryCode: modelData.countryCode
                                countryName: modelData.countryName
                                city: modelData.city
                                starred: modelData.city.length > 0
                                         && AppSettings.isFavorite(modelData.countryCode,
                                                                    modelData.city)
                                subtitle: Qt.formatDateTime(modelData.when, "MMM d, hh:mm")
                                freeUser: mainView.freeUser
                                onConnectRequested: (cc, city) => VpnFacade.connectTo(cc, city)
                                onToggleFavoriteRequested: (cc, city) =>
                                    AppSettings.toggleFavorite(cc, city)
                            }

                            Text {
                                x: recentsList.width / 2 - width / 2
                                y: recentsList.height / 2 - height / 2
                                visible: recentsList.count === 0
                                text: qsTr("No recent connections.")
                                color: Theme.textHint
                                font.pixelSize: Theme.fontBody
                            }
                        }
                    }
                }

                //  Free-plan upsell (expanded only)
                Rectangle {
                    visible: mainView.freeUser && !mainView.sidebarCollapsed
                    Layout.fillWidth: true
                    radius: Theme.radiusMd
                    color: Qt.alpha(Theme.accent, 0.12)
                    implicitHeight: upsell.implicitHeight + Theme.spacingMd * 2

                    Text {
                        id: upsell
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMd
                        textFormat: Text.RichText
                        text: qsTr("Free plans connect to the fastest server. " +
                                   "<a href='https://protonvpn.com/pricing'>Upgrade</a> " +
                                   "to choose locations.")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        wrapMode: Text.WordWrap
                        linkColor: Theme.accent
                        onLinkActivated: link => Qt.openUrlExternally(link)
                    }
                }

                Item { Layout.fillHeight: mainView.sidebarCollapsed }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: Theme.border
        }

        //  Right pane ------------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            //  Top bar
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                Layout.leftMargin: Theme.spacingXl
                Layout.rightMargin: Theme.spacingLg
                spacing: Theme.spacingSm

                Text {
                    text: rightStack.depth > 1 && rightStack.currentItem
                          ? (rightStack.currentItem.title ?? "") : ""
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSubtitle
                    font.bold: true
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    elide: Text.ElideRight
                }

                //  Account / profile icon: tinted + badged when logged in.
                Item {
                    implicitWidth: accountBtn.implicitWidth
                    implicitHeight: accountBtn.implicitHeight
                    Layout.alignment: Qt.AlignVCenter

                    PIconButton {
                        id: accountBtn
                        iconName: "person-circle"
                        iconSize: 20
                        iconColor: mainView.loggedIn
                                    ? (VpnFacade.plan === VpnFacade.Paid ? Theme.success : Theme.accent)
                                    : Theme.textSecondary
                        tooltip: qsTr("Account")
                        Behavior on iconColor { ColorAnimation { duration: Theme.durNormal } }
                        onClicked: {
                            if (rightStack.currentItem?.title === qsTr("Account"))
                                rightStack.pop(null)
                            else {
                                rightStack.pop(null)
                                rightStack.push(accountViewComponent)
                            }
                        }
                    }

                    // Logged-in indicator dot at the icon's top-right corner.
                    Rectangle {
                        visible: mainView.loggedIn
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.rightMargin: 2
                        anchors.topMargin: 2
                        implicitWidth: 7
                        implicitHeight: 7
                        radius: 3.5
                        color: VpnFacade.plan === VpnFacade.Paid ? Theme.success : Theme.accent
                        border.color: Theme.bgPage
                        border.width: 1.5
                        Behavior on color { ColorAnimation { duration: Theme.durNormal } }
                    }
                }

                PIconButton {
                    iconName: "gear"
                    iconSize: 20
                    tooltip: qsTr("Settings")
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: {
                        if (rightStack.currentItem?.title === qsTr("Settings"))
                            rightStack.pop(null)
                        else {
                            rightStack.pop(null)
                            rightStack.push(settingsViewComponent)
                        }
                    }
                }
            }

            // Subtle separator under the top bar.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            //  Content stack
            StackView {
                id: rightStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                initialItem: Item {
                    ConnectionCard {
                        favorites: serverModel.favorites
                        freeUser: mainView.freeUser
                        showFavoritesInCard: mainView.sidebarCollapsed
                    }
                }

                pushEnter: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durNormal }
                    NumberAnimation { property: "x"; from: 24; to: 0; duration: Theme.durNormal; easing.type: Easing.OutCubic }
                }
                pushExit:  Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durFast } }
                popEnter:  Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durNormal } }
                popExit:   Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durFast } }
            }
        }
    }

    Component { id: settingsViewComponent; SettingsView {} }
    Component { id: accountViewComponent;  AccountView {} }
}
