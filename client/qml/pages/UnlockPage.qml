import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    Rectangle {
        width: 420; height: 350; anchors.centerIn: parent; radius: 12; color: "#19191f"; border.color: "#30303a"
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 34; spacing: 18
            Text { text: "Vault locked"; color: "#f5f5f8"; font.pixelSize: 25; font.bold: true; Layout.alignment: Qt.AlignHCenter }
            Text { text: "Unlock locally with your master password."; color: "#9292a0"; font.pixelSize: 13; Layout.alignment: Qt.AlignHCenter }
            FormField { id: password; label: "Master password"; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText; Layout.fillWidth: true; onAccepted: submit() }
            AppButton { Layout.fillWidth: true; text: "Unlock"; enabled: !appController.busy && password.text.length > 0; onClicked: submit() }
            BusyIndicator { running: appController.busy; visible: running; Layout.alignment: Qt.AlignHCenter }
        }
    }
    function submit() { appController.unlock(password.text); password.text = "" }
}
