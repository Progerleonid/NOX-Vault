import QtQuick
import QtQuick.Controls

CheckBox {
    id: control
    spacing: 10
    implicitHeight: 30
    indicator: Rectangle {
        implicitWidth: 20; implicitHeight: 20; x: control.leftPadding
        y: (control.height - height) / 2; radius: 5
        color: control.checked ? "#6257dc" : "#17171d"
        border.color: control.activeFocus ? "#8f84ff" : control.checked ? "#7165e8" : "#42424e"
        Text { anchors.centerIn: parent; text: "✓"; visible: control.checked; color: "white"; font.pixelSize: 13; font.bold: true }
        Behavior on color { ColorAnimation { duration: 120 } }
    }
    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text; color: control.enabled ? "#d7d7df" : "#70707b"
        verticalAlignment: Text.AlignVCenter; font.pixelSize: 13
    }
}
