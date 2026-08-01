import QtQuick
import ProtonVpnGui

// CrossfadeStack - a page container that switches between its children with
// a fade-through instead of StackLayout's hard cut: the whole page layer
// fades out (durFast), the index swaps invisibly, then the layer fades back
// in while rising slightly (durNormal).
//
// Children are created eagerly (like StackLayout) so bindings stay live.
// Pages may declare `property bool isCurrent` to run entrance hooks when
// they actually become visible (Component.onCompleted fires at app start
// for every page, so it cannot be used for entrances).
Item {
    id: stack

    property int currentIndex: 0

    default property alias pages: pageHost.data

    // The moving layer. Not anchored to `stack` so `y` stays animatable.
    Item {
        id: pageHost
        width: stack.width
        height: stack.height
    }

    Component.onCompleted: applyIndex()
    onPagesChanged: applyIndex()

    onCurrentIndexChanged: {
        if (Theme.reducedMotion || pageHost.opacity === 0) {
            applyIndex()
            pageHost.opacity = 1
            pageHost.y = 0
            return
        }
        exitAnim.restart()
    }

    // Swaps which child is visible/enabled. Never touches pageHost's
    // opacity/y - those belong to the transition animation.
    function applyIndex() {
        const kids = pageHost.children
        for (let i = 0; i < kids.length; ++i) {
            const current = (i === stack.currentIndex)
            kids[i].anchors.fill = pageHost
            kids[i].visible = current
            kids[i].enabled = current
            if (kids[i].isCurrent !== undefined) {
                kids[i].isCurrent = current
            }
        }
    }

    SequentialAnimation {
        id: exitAnim
        NumberAnimation {
            target: pageHost
            property: "opacity"
            to: 0
            duration: Theme.dur(Theme.durFast)
        }
        ScriptAction { script: stack.applyIndex() }
        ParallelAnimation {
            NumberAnimation {
                target: pageHost
                property: "opacity"
                to: 1
                duration: Theme.dur(Theme.durNormal)
            }
            NumberAnimation {
                target: pageHost
                property: "y"
                from: 10
                to: 0
                duration: Theme.dur(Theme.durNormal)
                easing.type: Theme.easeStandard
            }
        }
    }
}
