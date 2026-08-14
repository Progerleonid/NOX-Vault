import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property alias label: label.text
    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias echoMode: field.echoMode
    property alias validator: field.validator
    property alias inputMethodHints: field.inputMethodHints
    readonly property alias acceptableInput: field.acceptableInput
    signal accepted()
    spacing: 7
    Text { id: label; color: "#aaaab8"; font.pixelSize: 12; font.weight: Font.DemiBold }
    TextField {
        id: field
        Layout.fillWidth: true
        implicitHeight: 42
        color: "#f2f2f7"
        placeholderTextColor: "#72727f"
        selectionColor: "#6257d9"
        selectedTextColor: "white"
        font.pixelSize: 14
        onAccepted: root.accepted()
        background: Rectangle {
            radius: 8; color: "#17171d"
            border.color: field.activeFocus ? "#7165e8" : "#34343f"
            border.width: field.activeFocus ? 2 : 1
            Behavior on border.color { ColorAnimation { duration: 120 } }
        }
    }
}
