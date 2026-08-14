import QtQuick
import QtQuick.Controls

Dialog {
    id: control
    property string heading: ""
    modal: true
    anchors.centerIn: parent
    padding: 22
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape
    Overlay.modal: Rectangle { color: "#9909090d" }
    header: Rectangle {
        implicitHeight: control.heading.length ? 54 : 0
        color: "#202027"
        radius: 11
        Text {
            anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 22
            verticalAlignment: Text.AlignVCenter
            text: control.heading; color: "#f4f4f8"; font.pixelSize: 17; font.bold: true
        }
    }
    background: Rectangle {
        radius: 11; color: "#202027"
        border.color: "#3a3a46"; border.width: 1
    }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: 160; easing.type: Easing.OutCubic }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: 100 } }
}
