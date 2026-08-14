import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    property bool registration: false
    Rectangle {
        width: Math.min(430, parent.width - 48); height: authColumn.implicitHeight + 68; anchors.centerIn: parent
        radius: 12; color: "#19191f"; border.color: "#30303a"
        Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        ColumnLayout {
            id: authColumn; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: 34; spacing: 16
            Text { text: registration ? "Create your account" : "Welcome back"; color: "#f5f5f8"; font.pixelSize: 25; font.bold: true }
            Text { text: registration ? "Create an account for your encrypted vault." : "Sign in to access your encrypted vault."; color: "#9292a0"; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            FormField { id: email; label: "Email"; placeholderText: "you@example.com"; inputMethodHints: Qt.ImhEmailCharactersOnly; Layout.fillWidth: true }
            FormField { id: password; label: "Account password"; placeholderText: "At least 12 characters"; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText; Layout.fillWidth: true; onAccepted: submit() }
            AppButton { Layout.fillWidth: true; text: registration ? "Create account" : "Sign in"; enabled: !appController.busy && email.text.length > 0 && password.text.length >= 12; onClicked: submit() }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 8
                BusyIndicator { running: appController.busy; visible: running; implicitWidth: 24; implicitHeight: 24 }
                Text { visible: appController.busy; text: registration ? "Creating account…" : "Signing in…"; color: "#a8a8b5"; font.pixelSize: 12 }
            }
            AppButton {
                secondary: true; Layout.alignment: Qt.AlignHCenter
                text: registration ? "Already have an account? Sign in" : "New to NOX Vault? Register"
                onClicked: registration = !registration
            }
            AppButton { visible: appController.accounts.length > 0; secondary: true; text: "Back to current account"; Layout.alignment: Qt.AlignHCenter; onClicked: appController.cancelAddAccount() }
        }
        opacity: 0
        Component.onCompleted: opacity = 1
        Behavior on opacity { NumberAnimation { duration: 220 } }
    }
    function submit() {
        appController.authenticate(email.text, password.text, registration)
        password.text = ""
    }
}
