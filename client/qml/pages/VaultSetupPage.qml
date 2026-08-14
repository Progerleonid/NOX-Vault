import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    Rectangle {
        width: 480; height: 480; anchors.centerIn: parent; radius: 12; color: "#19191f"; border.color: "#30303a"
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 34; spacing: 16
            Text { text: "Create your encrypted vault"; color: "#f5f5f8"; font.pixelSize: 24; font.bold: true }
            Text { text: "Your master password never leaves this device. It cannot be recovered."; color: "#9292a0"; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            FormField { id: password; label: "Master password"; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText; Layout.fillWidth: true }
            FormField { id: confirm; label: "Confirm master password"; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText; Layout.fillWidth: true }
            AppCheckBox { id: privateMetadata; text: "Encrypt secret names"; checked: true }
            Text { text: "Private metadata hides secret names from the server."; color: "#777784"; font.pixelSize: 11; Layout.fillWidth: true }
            AppButton { Layout.fillWidth: true; text: "Initialize vault"; enabled: !appController.busy && password.text.length >= 8 && confirm.text.length > 0; onClicked: { appController.initializeVault(password.text, confirm.text, privateMetadata.checked); password.text = ""; confirm.text = "" } }
            BusyIndicator { running: appController.busy; visible: running; Layout.alignment: Qt.AlignHCenter }
        }
    }
}
