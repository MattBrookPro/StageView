import QtQuick

// The top-down stage. Sources are laid out by their audio params: height = level,
// horizontal = pan. The decorations (centre line, level guides, axis labels) exist
// only to make that mapping legible - the field itself is the coordinate space the
// SourceNode delegates position themselves within.

Item {
    id: canvas

    Rectangle {
        id: field
        anchors.fill: parent
        anchors.leftMargin: 46     // room for the LOUD/QUIET scale
        anchors.rightMargin: 16
        anchors.topMargin: 12
        anchors.bottomMargin: 30   // room for the L / C / R pan labels
        radius: 12
        border.color: "#26323f"
        border.width: 1
        // Back of the stage darker, front (near audience) lighter - a faint sense
        // of depth that also reinforces "up = further back / louder".
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#11161e" }
            GradientStop { position: 1.0; color: "#172230" }
        }
        clip: false

        // Faint horizontal level guides at 25/50/75%.
        Repeater {
            model: [0.25, 0.5, 0.75]
            delegate: Rectangle {
                required property real modelData
                width: field.width
                height: 1
                y: field.height * (1.0 - modelData)
                color: "#1b2630"
            }
        }

        // Pan centre line.
        Rectangle {
            x: field.width / 2
            width: 1
            height: field.height
            color: "#1f2b37"
        }

        // One delegate per channel; each positions itself from level/pan.
        Repeater {
            model: Stage
            delegate: SourceNode {
                fieldWidth: field.width
                fieldHeight: field.height
            }
        }
    }

    // --- Vertical (level) scale ---
    Text {
        anchors.left: parent.left
        anchors.top: field.top
        text: "LOUD"
        color: "#52606e"
        font.pixelSize: 9
        font.letterSpacing: 1
        rotation: -90
        transformOrigin: Item.TopLeft
        x: 8
        y: field.y + 34
    }
    Text {
        anchors.left: parent.left
        text: "QUIET"
        color: "#52606e"
        font.pixelSize: 9
        font.letterSpacing: 1
        rotation: -90
        transformOrigin: Item.TopLeft
        x: 8
        y: field.y + field.height - 4
    }

    // --- Horizontal (pan) scale ---
    Row {
        anchors.top: field.bottom
        anchors.topMargin: 8
        anchors.left: field.left
        anchors.right: field.right
        Text { text: "L";  color: "#52606e"; font.pixelSize: 10; width: field.width / 2 }
        Text { text: "C";  color: "#52606e"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; width: 1 }
        Text { text: "R";  color: "#52606e"; font.pixelSize: 10; horizontalAlignment: Text.AlignRight; width: field.width / 2 - 1 }
    }
}
