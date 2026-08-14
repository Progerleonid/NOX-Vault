import QtQuick

Rectangle {
    id: root
    property bool healthy: false
    property string label: healthy ? "Online" : "Offline"
    implicitWidth: row.implicitWidth + 18
    implicitHeight: 28
    radius: 14
    color: healthy ? "#173329" : "#3a2024"
    border.color: healthy ? "#295844" : "#693038"
    Row {
        id: row; anchors.centerIn: parent; spacing: 7
        Rectangle { width: 7; height: 7; radius: 4; color: root.healthy ? "#4cd18b" : "#ef6671"; anchors.verticalCenter: parent.verticalCenter }
        Text { text: root.label; color: root.healthy ? "#8de1b4" : "#f49aa2"; font.pixelSize: 12 }
    }
}
