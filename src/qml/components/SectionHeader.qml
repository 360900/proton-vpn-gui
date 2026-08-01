import QtQuick
import ProtonVpnGui

// Uppercase section label used to group settings rows.
Text {
    property string label: ""

    text: label
    color: Theme.textHint
    font.pixelSize: Theme.fontCaption
    font.weight: Font.DemiBold
    font.capitalization: Font.AllUppercase
    font.letterSpacing: 1
    topPadding: Theme.spacingLg
    bottomPadding: Theme.spacingXs
    leftPadding: Theme.spacingLg
}
