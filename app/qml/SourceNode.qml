import QtQuick

// A single source on the stage: a draggable puck plus a live meter.
//
// This delegate is where the spatial control idea actually lives. The model only
// knows level/pan; here we map them to a position - vertical = level (top louder),
// horizontal = pan (left/right) - and turn a drag back into level/pan that we push
// to the model. The trick for combining a model-driven position with a free drag
// is the pair of Binding elements below: they drive x/y from the model EXCEPT
// while dragging, when the drag owns the position and we write the result back.

Item {
    id: node

    // --- model roles, bound by name from the StageModel ---
    required property int index
    required property string name
    required property real level
    required property real pan
    required property bool mute
    required property real meter

    // --- geometry context supplied by the canvas ---
    property real fieldWidth: 100
    property real fieldHeight: 100

    // highlighted when this source is the one the DSP panel is editing
    readonly property bool selected: node.index === Stage.selectedChannel

    width: 60
    height: 60

    // position (node centre) <-> audio parameter, the entire mapping in 4 lines.
    function panToX(p)   { return (p * 0.5 + 0.5) * fieldWidth }
    function levelToY(l) { return (1.0 - l) * fieldHeight }
    function panFromX(x)   { return Math.max(-1, Math.min(1, (x / fieldWidth) * 2 - 1)) }
    function levelFromY(y) { return Math.max(0,  Math.min(1, 1 - y / fieldHeight)) }

    // Model drives position - but only when the user isn't dragging. On release the
    // binding re-engages against the just-updated model value, so there is no jump.
    Binding {
        target: node; property: "x"
        value: node.panToX(node.pan) - node.width / 2
        when: !ma.drag.active
    }
    Binding {
        target: node; property: "y"
        value: node.levelToY(node.level) - node.height / 2
        when: !ma.drag.active
    }

    readonly property color accent: "#39d2c0"

    // Glow behind the puck: grows and brightens with the live meter, so an active
    // source visibly pulses on the stage at a glance.
    Rectangle {
        anchors.centerIn: puck
        width: 60; height: 60; radius: 30
        color: node.accent
        visible: !node.mute
        opacity: node.meter * 0.45
        scale: 1.0 + node.meter * 0.35
        Behavior on opacity { NumberAnimation { duration: 90 } }
        Behavior on scale   { NumberAnimation { duration: 90 } }
    }

    // The puck itself.
    Rectangle {
        id: puck
        anchors.fill: parent
        radius: width / 2
        color: "#161b23"
        border.width: ma.drag.active ? 2 : 1
        border.color: node.mute ? "#3a424d"
                                 : (ma.drag.active ? Qt.lighter(node.accent, 1.2)
                                                   : Qt.rgba(0.22, 0.82, 0.75, 0.5))

        // Loudness wash: brighter with level, near-off when muted.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: node.accent
            opacity: node.mute ? 0.05 : (0.18 + node.level * 0.55)
        }

        Text {
            anchors.centerIn: parent
            text: node.name
            color: node.mute ? "#7c8794" : "#eaf1f6"
            font.pixelSize: 13
            font.bold: true
        }

        // Tiny "muted" marker.
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            visible: node.mute
            text: "MUTE"
            color: "#e8514f"
            font.pixelSize: 8
            font.letterSpacing: 1
        }
    }

    // Selection ring - shows which source the DSP panel is editing.
    Rectangle {
        anchors.centerIn: puck
        width: 74; height: 74; radius: 37
        color: "transparent"
        visible: node.selected
        border.color: node.accent
        border.width: 2
        opacity: 0.85
    }

    // Live meter, hanging just to the right of the puck.
    Meter {
        anchors.left: parent.right
        anchors.leftMargin: 7
        anchors.verticalCenter: parent.verticalCenter
        height: 54
        level: node.meter
    }

    // Drag = move the puck; we translate its centre into level/pan and push to the
    // model on every move so the engine tracks the gesture in real time. Double-tap
    // toggles mute.
    MouseArea {
        id: ma
        anchors.fill: puck
        cursorShape: Qt.SizeAllCursor
        drag.target: node
        drag.minimumX: -node.width / 2
        drag.maximumX: node.fieldWidth - node.width / 2
        drag.minimumY: -node.height / 2
        drag.maximumY: node.fieldHeight - node.height / 2

        onPositionChanged: if (drag.active) {
            const cx = node.x + node.width / 2
            const cy = node.y + node.height / 2
            Stage.setLevelPan(node.index, node.levelFromY(cy), node.panFromX(cx))
        }
        onClicked: Stage.selectedChannel = node.index   // select -> opens the DSP panel
        onDoubleClicked: Stage.setMute(node.index, !node.mute)
    }
}
