import QtQuick
import QtQuick.Window

// Root window: a slim header (identity + connection state + how-to hint) above the
// stage canvas. Kept intentionally quiet so the stage is the focus.

Window {
    id: root
    width: 1100
    height: 720
    minimumWidth: 760
    minimumHeight: 500
    visible: true
    title: qsTr("StageView")
    color: "#0e1116"

    // --- Header ---
    Item {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 64

        Column {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 20
            spacing: 2
            Text {
                text: qsTr("StageView")
                color: "#eaf1f6"
                font.pixelSize: 20
                font.bold: true
                font.letterSpacing: 1
            }
            Text {
                text: qsTr("spatial control surface  ·  drag a source: up = louder, sideways = pan, double-click = mute")
                color: "#5b6b7a"
                font.pixelSize: 11
            }
        }

        // Status: engine connection (bound to the model) plus the MIDI port, if a
        // controller is plugged in.
        Column {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 20
            spacing: 3

            Row {
                anchors.right: parent.right
                spacing: 8
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 9; height: 9; radius: 4.5
                    color: Stage.connected ? "#39c98c" : "#5a6675"
                    Behavior on color { ColorAnimation { duration: 200 } }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Stage.connected ? qsTr("engine connected") : qsTr("offline")
                    color: "#9aa7b4"
                    font.pixelSize: 12
                }
            }

            Text {
                anchors.right: parent.right
                visible: Stage.midiPort.length > 0
                text: qsTr("MIDI: ") + Stage.midiPort
                color: "#7d8b99"
                font.pixelSize: 11
            }
        }

        Rectangle { // hairline divider
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: "#1a212b"
        }
    }

    // --- Stage ---
    StageCanvas {
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 16
        }
        // Make room for the DSP panel when a source is selected.
        anchors.bottomMargin: dsp.visible ? dsp.height + 24 : 16
    }

    // --- Per-channel DSP panel (EQ + compressor); visible when a source is selected ---
    ControlPanel {
        id: dsp
        height: 188
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 16
        }
    }
}
