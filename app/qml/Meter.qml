import QtQuick

// A slim vertical channel meter. `level` (0..1) reveals a fixed green->amber->red
// gradient from the bottom up, so the colour reflects the *absolute* level (green
// low, red hot) the way a real meter does - achieved by clipping a full-height
// gradient rather than scaling it. A short Behavior smooths the ~50 Hz feed into
// natural meter ballistics instead of visible stepping.

Item {
    id: root
    property real level: 0.0

    implicitWidth: 7
    implicitHeight: 56

    Rectangle {
        id: track
        anchors.fill: parent
        radius: width / 2
        color: "#0b0e12"
        border.color: "#222b35"
        border.width: 1
    }

    Item {
        id: clipRegion
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 1
        }
        height: Math.max(0, Math.min(1, root.level)) * (track.height - 2)
        clip: true
        Behavior on height { NumberAnimation { duration: 70; easing.type: Easing.OutQuad } }

        Rectangle {
            // Full-height gradient pinned to the bottom; the clip above only reveals
            // the lowest `level` fraction, so each colour stays at its true height.
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: track.height - 2
            radius: width / 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#e8514f" } // top: hot / red
                GradientStop { position: 0.30; color: "#e6b23c" } // amber
                GradientStop { position: 1.0; color: "#39c98c" } // bottom: green
            }
        }
    }
}
