import QtQuick
import QtQuick.Controls.Basic

// Per-channel DSP panel: appears when a source is selected. Drives the engine's
// zero-latency EQ + compressor over OSC. Slider values initialise from the engine
// snapshot (Stage.getParam) and write live (Stage.setParam) as you move them.

Rectangle {
    id: panel
    property int ch: Stage.selectedChannel
    readonly property color accent: "#39d2c0"

    visible: ch >= 0
    color: "#0f141b"
    border.color: "#27333f"
    border.width: 1
    radius: 10

    onChChanged: refresh()
    Component.onCompleted: refresh()
    function refresh() {
        if (ch < 0)
            return
        eqLow.value = Stage.getParam(ch, "eq/low")
        eqMid.value = Stage.getParam(ch, "eq/mid")
        eqHigh.value = Stage.getParam(ch, "eq/high")
        eqSwitch.checked = Stage.getParam(ch, "eq/on") > 0.5
        thr.value = Stage.getParam(ch, "comp/thresh")
        ratio.value = Stage.getParam(ch, "comp/ratio")
        makeup.value = Stage.getParam(ch, "comp/makeup")
        compSwitch.checked = Stage.getParam(ch, "comp/on") > 0.5
    }

    // A vertical EQ band: label, dB slider (-18..+18), readout.
    component EqBand: Column {
        property string label
        property string pkey
        property alias value: s.value
        width: 52
        spacing: 6
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: label; color: "#9aa7b4"; font.pixelSize: 10; font.letterSpacing: 1
        }
        Slider {
            id: s
            anchors.horizontalCenter: parent.horizontalCenter
            orientation: Qt.Vertical
            from: -18; to: 18; value: 0
            implicitHeight: 96
            onMoved: Stage.setParam(panel.ch, pkey, value)
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: (s.value >= 0 ? "+" : "") + s.value.toFixed(1)
            color: "#cfd8e0"; font.pixelSize: 11
        }
    }

    // A horizontal compressor control: label, slider, readout.
    component CompCtrl: Row {
        property string label
        property string pkey
        property real lo: 0
        property real hi: 1
        property string suffix: ""
        property alias value: s.value
        spacing: 10
        Text {
            width: 66; text: label; color: "#9aa7b4"; font.pixelSize: 11
            anchors.verticalCenter: parent.verticalCenter
        }
        Slider {
            id: s
            implicitWidth: 150
            anchors.verticalCenter: parent.verticalCenter
            from: lo; to: hi
            onMoved: Stage.setParam(panel.ch, pkey, value)
        }
        Text {
            width: 56; text: s.value.toFixed(1) + suffix; color: "#cfd8e0"; font.pixelSize: 11
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // --- header: channel name + EQ/Comp enables + deselect ---
    Column {
        id: header
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 16 }
        width: 132
        spacing: 10
        Text {
            text: panel.ch >= 0 ? Stage.nameOf(panel.ch) : ""
            color: "#eaf1f6"; font.pixelSize: 20; font.bold: true; elide: Text.ElideRight
            width: parent.width
        }
        Text { text: qsTr("channel DSP"); color: "#5b6b7a"; font.pixelSize: 11 }
        Row {
            spacing: 8
            Switch { id: eqSwitch; onToggled: Stage.setParam(panel.ch, "eq/on", checked ? 1 : 0) }
            Text { text: qsTr("EQ"); color: "#cfd8e0"; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
        }
        Row {
            spacing: 8
            Switch { id: compSwitch; onToggled: Stage.setParam(panel.ch, "comp/on", checked ? 1 : 0) }
            Text { text: qsTr("Comp"); color: "#cfd8e0"; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
        }
    }

    Text {
        anchors { right: parent.right; top: parent.top; margins: 12 }
        text: "✕"; color: "#7d8b99"; font.pixelSize: 15
        MouseArea {
            anchors.fill: parent; anchors.margins: -8
            cursorShape: Qt.PointingHandCursor
            onClicked: Stage.selectedChannel = -1
        }
    }

    // --- EQ: three vertical bands ---
    Column {
        id: eqBlock
        anchors { left: header.right; leftMargin: 28; verticalCenter: parent.verticalCenter }
        spacing: 6
        Text { text: qsTr("EQUALISER"); color: "#52606e"; font.pixelSize: 10; font.letterSpacing: 2 }
        Row {
            spacing: 16
            opacity: eqSwitch.checked ? 1.0 : 0.4
            EqBand { id: eqLow;  label: "LOW";  pkey: "eq/low" }
            EqBand { id: eqMid;  label: "MID";  pkey: "eq/mid" }
            EqBand { id: eqHigh; label: "HIGH"; pkey: "eq/high" }
        }
    }

    // --- Compressor ---
    Column {
        anchors { left: eqBlock.right; leftMargin: 40; verticalCenter: parent.verticalCenter }
        spacing: 10
        opacity: compSwitch.checked ? 1.0 : 0.4
        Text { text: qsTr("COMPRESSOR"); color: "#52606e"; font.pixelSize: 10; font.letterSpacing: 2 }
        CompCtrl { id: thr;    label: qsTr("Threshold"); pkey: "comp/thresh"; lo: -48; hi: 0;  suffix: " dB" }
        CompCtrl { id: ratio;  label: qsTr("Ratio");     pkey: "comp/ratio";  lo: 1;   hi: 20; suffix: ":1" }
        CompCtrl { id: makeup; label: qsTr("Makeup");    pkey: "comp/makeup"; lo: 0;   hi: 24; suffix: " dB" }
    }
}
