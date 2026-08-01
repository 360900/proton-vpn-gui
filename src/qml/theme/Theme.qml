pragma Singleton
import QtQuick
import ProtonVpnGui

// Design tokens for the entire UI. Every color, spacing, radius, type size,
// and duration comes from here - components never hard-code visual values.
// The palette follows Proton's visual language: deep purple-tinted darks,
// #6D4AFF accent, generous rounding.

QtObject {
    id: theme

    readonly property bool dark: ThemeController.dark

    // Brand ---------------------------------------------------------------

    readonly property color accent:        "#6D4AFF"
    readonly property color accentHover:   "#7D5EFF"
    readonly property color accentPressed: "#5B38E6"
    readonly property color accentMuted:   dark ? "#3A2E75" : "#E4DCFF"
    readonly property color textOnAccent:  "#FFFFFF"

    // Semantic ------------------------------------------------------------

    readonly property color success: "#1EA885"
    readonly property color warning: "#F5A623"
    readonly property color danger:  "#DC3251"

    // Surfaces ------------------------------------------------------------

    readonly property color bgPage:       dark ? "#16141C" : "#F7F6FA"
    readonly property color bgSidebar:    dark ? "#1C1A24" : "#FFFFFF"
    readonly property color surface:      dark ? "#24222E" : "#FFFFFF"
    readonly property color surfaceHover: dark ? "#2C2A38" : "#EFEDF5"
    readonly property color surfaceActive:dark ? "#343147" : "#E7E2F6"
    readonly property color border:       dark ? "#35323F" : "#DEDAE8"
    readonly property color borderStrong: dark ? "#4A4658" : "#C8C3D6"
    readonly property color overlay:      dark ? "#000000CC" : "#3E3A4EAA"

    // Text ----------------------------------------------------------------

    readonly property color textPrimary:   dark ? "#FFFFFF" : "#1B1340"
    readonly property color textSecondary: dark ? "#A7A4B5" : "#5C5769"
    readonly property color textHint:      dark ? "#6D6A76" : "#8F8BA0"
    readonly property color textOnDark:    "#FFFFFF"

    // Spacing scale (px) --------------------------------------------------

    readonly property int spacingXs:  4
    readonly property int spacingSm:  8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 24
    readonly property int spacingXxl: 32

    // Radii ---------------------------------------------------------------

    readonly property int radiusSm:   4
    readonly property int radiusMd:   8
    readonly property int radiusLg:  12
    readonly property int radiusXl: 16
    readonly property int radiusPill: 999

    // Type scale ----------------------------------------------------------

    readonly property int fontCaption:  11
    readonly property int fontBody:     13
    readonly property int fontSubtitle: 15
    readonly property int fontTitle:    18
    readonly property int fontDisplay:  24
    readonly property int fontDisplay2: 30

    // Motion --------------------------------------------------------------
    // "Proton calm": motion exists only to explain state change. Nothing
    // loops except a single activity indicator per context. All durations
    // route through dur() so the Reduce Motion setting zeroes them app-wide.

    readonly property int durMicro:  90   // hover / press feedback
    readonly property int durFast:   140  // toggles, fades, exits
    readonly property int durNormal: 220  // panels, layout changes, entrances
    readonly property int durSlow:   300  // view-level entrances only

    readonly property int easeStandard: Easing.OutCubic  // entering / emerging
    readonly property int easeState:    Easing.InOutQuad // bidirectional state

    // Accessibility: when on, translations/scales/fades collapse to instant
    // and all looping motion stops. Activity spinners keep spinning (they
    // communicate "work in progress", not decoration).
    readonly property bool reducedMotion: AppSettings.reduceMotion

    // Single funnel for every animation duration in the app.
    function dur(base) {
        return reducedMotion ? 0 : base
    }
}
