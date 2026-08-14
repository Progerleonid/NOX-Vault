import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../components"

Item {
    property bool importing: false
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 28; spacing: 18
        Text { text: "Encrypted backup"; color: "#f4f4f7"; font.pixelSize: 25; font.bold: true }
        Text { text: "Backups retain the existing encrypted NOX Vault format."; color: "#858592"; font.pixelSize: 12 }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 18
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.maximumHeight: 300
                radius: 10; color: "#19191f"; border.color: "#30303a"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 24; spacing: 14
                    Text { text: "Export"; color: "#eeeeF3"; font.pixelSize: 18; font.bold: true }
                    Text { text: "Create an encrypted backup of this vault."; color: "#898996"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Item { Layout.fillHeight: true }
                    AppButton { text: "Export backup"; enabled: !appController.busy; onClicked: { importing = false; exportDialog.open() } }
                }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.maximumHeight: 300
                radius: 10; color: "#19191f"; border.color: "#493035"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 24; spacing: 14
                    Text { text: "Restore"; color: "#eeeeF3"; font.pixelSize: 18; font.bold: true }
                    Text { text: "Replace the current vault with an encrypted backup."; color: "#898996"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "This action replaces the current encrypted vault."; color: "#e77d86"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Item { Layout.fillHeight: true }
                    AppButton { text: "Import backup"; destructive: true; enabled: !appController.busy; onClicked: { importing = true; importDialog.open() } }
                }
            }
        }
        Item { Layout.fillHeight: true }
    }
    FileDialog { id: exportDialog; title: "Export encrypted backup"; fileMode: FileDialog.SaveFile; nameFilters: ["NOX Vault backup (*.nox)", "All files (*)"]; onAccepted: { passwordDialog.backupPath = selectedFile; passwordDialog.open() } }
    FileDialog { id: importDialog; title: "Import encrypted backup"; fileMode: FileDialog.OpenFile; nameFilters: ["NOX Vault backup (*.nox)", "All files (*)"]; onAccepted: { passwordDialog.backupPath = selectedFile; confirmDialog.open() } }
    AppDialog {
        id: confirmDialog; width: 440; heading: "Replace current vault?"
        contentItem: ColumnLayout {
            Text { text: "The encrypted backup will replace the current vault and its secrets."; color: "#d8d8e0"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Item { Layout.fillWidth: true }
                AppButton { text: "Cancel"; secondary: true; onClicked: confirmDialog.close() }
                AppButton { text: "Continue"; destructive: true; onClicked: { confirmDialog.close(); passwordDialog.open() } }
            }
        }
    }
    AppDialog {
        id: passwordDialog; width: 440; heading: importing ? "Restore backup" : "Export backup"
        property string backupPath: ""
        contentItem: ColumnLayout {
            FormField { id: master; label: "Master password"; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText; Layout.fillWidth: true }
            RowLayout {
                Item { Layout.fillWidth: true }
                AppButton { text: "Cancel"; secondary: true; onClicked: { master.text = ""; passwordDialog.close() } }
                AppButton { text: importing ? "Replace vault" : "Export"; destructive: importing; enabled: master.text.length > 0 && !appController.busy; onClicked: { if (importing) appController.importBackup(passwordDialog.backupPath, master.text); else appController.exportBackup(passwordDialog.backupPath, master.text); master.text = ""; passwordDialog.close() } }
            }
        }
    }
}
