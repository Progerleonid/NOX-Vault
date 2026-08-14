import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    onVisibleChanged: if (visible && appController.diagnostics.length === 0) appController.runDiagnostics()
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 28; spacing: 18
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Diagnostics"; color: "#f4f4f7"; font.pixelSize: 25; font.bold: true }
            Item { Layout.fillWidth: true }
            BusyIndicator { running: appController.busy; visible: running; implicitWidth: 28; implicitHeight: 28 }
            AppButton { text: appController.busy ? "Running…" : "Run diagnostics"; secondary: true; enabled: !appController.busy; onClicked: appController.runDiagnostics() }
        }
        Text { text: "Safe connectivity and client status. Credentials and decrypted data are never shown."; color: "#858592"; font.pixelSize: 12 }
        Rectangle {
            id: diagnosticsCard
            Layout.fillWidth: true; Layout.fillHeight: true; radius: 10; color: "#19191f"; border.color: "#30303a"
            Text { anchors.fill: parent; anchors.margins: 24; text: appController.diagnostics || "Run diagnostics to inspect status."; color: "#d7d7df"; font.pixelSize: 14; lineHeight: 1.25 }
            Rectangle {
                visible: appController.busy; y: 0; width: parent.width / 3; height: 3; color: "#6257dc"
                SequentialAnimation on x { running: appController.busy; loops: Animation.Infinite; NumberAnimation { from: 0; to: diagnosticsCard.width * 0.66; duration: 900 } }
            }
        }
    }
}
