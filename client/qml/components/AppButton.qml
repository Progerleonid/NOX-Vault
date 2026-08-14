import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool destructive: false
    property bool secondary: false
    implicitHeight: 38
    padding: 12
    font.pixelSize: 13
    font.weight: Font.DemiBold
    contentItem: Text {
        text: control.text
        color: control.enabled ? (control.secondary ? "#d7d7e3" : "#ffffff") : "#777783"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 8
        color: !control.enabled ? "#24242c" : control.down ? "#4338a8" : control.hovered
               ? (control.destructive ? "#bd313d" : control.secondary ? "#30303b" : "#675be8")
               : (control.destructive ? "#9f2733" : control.secondary ? "#272730" : "#584dcc")
        border.color: control.secondary ? "#3a3a46" : "transparent"
        Behavior on color { ColorAnimation { duration: 110 } }
        scale: control.down ? 0.98 : 1
        Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
    }
}
