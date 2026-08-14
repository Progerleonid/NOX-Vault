import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 28; spacing: 18
        Text { text: "Security"; color: "#f4f4f7"; font.pixelSize: 25; font.bold: true }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 104; radius: 10; color: "#19191f"; border.color: "#30303a"
            RowLayout {
                anchors.fill: parent; anchors.margins: 20
                ColumnLayout {
                    Text { text: "Vault"; color: "#eeeeF3"; font.pixelSize: 16; font.bold: true }
                    Text { text: appController.vaultUnlocked ? "Unlocked through the shared local agent" : "Locked"; color: "#898996"; font.pixelSize: 12 }
                }
                Item { Layout.fillWidth: true }
                AppButton { text: "Lock vault"; onClicked: appController.lock() }
            }
        }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 104; radius: 10; color: "#19191f"; border.color: "#30303a"
            RowLayout {
                anchors.fill: parent; anchors.margins: 20
                ColumnLayout {
                    Text { text: "Master password"; color: "#eeeeF3"; font.pixelSize: 16; font.bold: true }
                    Text { text: "Re-wrap the existing vault key with a new password."; color: "#898996"; font.pixelSize: 12 }
                }
                Item { Layout.fillWidth: true }
                AppButton { text: "Change password"; secondary: true; onClicked: passwordDialog.open() }
            }
        }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 128; radius: 10; color: "#19191f"; border.color: "#30303a"
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 20
                Text { text: "Automatic protection"; color: "#eeeeF3"; font.pixelSize: 16; font.bold: true }
                Text { text: "Auto-lock after " + Math.round(appController.autoLockTimeout / 60) + " minutes of inactivity"; color: "#898996"; font.pixelSize: 12 }
                Text { text: "Clear copied secrets after " + appController.clipboardTimeout + " seconds"; color: "#898996"; font.pixelSize: 12 }
            }
        }
        Item { Layout.fillHeight: true }
        AppButton { text: "Sign out"; destructive: true; onClicked: appController.logout() }
    }
    AppDialog {
        id: passwordDialog; width: 460; heading: "Change master password"
        contentItem: ColumnLayout {
            spacing: 12
            FormField { id: current; label: "Current password"; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData; Layout.fillWidth: true }
            FormField { id: next; label: "New password"; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData; Layout.fillWidth: true }
            FormField { id: confirm; label: "Confirm new password"; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData; Layout.fillWidth: true }
            RowLayout {
                Item { Layout.fillWidth: true }
                AppButton { text: "Cancel"; secondary: true; onClicked: passwordDialog.close() }
                AppButton { text: "Change"; enabled: current.text.length && next.text.length && confirm.text.length && !appController.busy; onClicked: { appController.changeMasterPassword(current.text, next.text, confirm.text); current.text = ""; next.text = ""; confirm.text = ""; passwordDialog.close() } }
            }
        }
    }
}
