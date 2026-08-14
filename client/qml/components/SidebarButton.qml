import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool selected: false
    implicitHeight: 42
    leftPadding: 14
    contentItem: Text {
        text: control.text; color: control.selected ? "#ffffff" : "#aaaab8"
        font.pixelSize: 14; font.weight: control.selected ? Font.DemiBold : Font.Normal
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 8
        color: control.selected ? "#302d57" : control.hovered ? "#24242c" : "transparent"
        border.color: control.selected ? "#4a457c" : "transparent"
    }
}
