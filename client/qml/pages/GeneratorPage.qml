import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 28; spacing: 18
        Text { text: "Password generator"; color: "#f4f4f7"; font.pixelSize: 25; font.bold: true }
        Text { text: "Generate cryptographically secure values locally."; color: "#858592"; font.pixelSize: 12 }
        Rectangle {
            Layout.preferredWidth: 620; Layout.fillHeight: true; Layout.maximumHeight: 500
            radius: 10; color: "#19191f"; border.color: "#30303a"
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 28; spacing: 16
                FormField { id: length; label: "Length (8–256)"; text: "32"; validator: IntValidator { bottom: 8; top: 256 } Layout.preferredWidth: 180 }
                GridLayout {
                    columns: 2; columnSpacing: 34; rowSpacing: 8
                    AppCheckBox { id: upper; text: "Uppercase"; checked: true }
                    AppCheckBox { id: lower; text: "Lowercase"; checked: true }
                    AppCheckBox { id: numbers; text: "Numbers"; checked: true }
                    AppCheckBox { id: symbols; text: "Symbols"; checked: true }
                }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 58; radius: 8; color: "#111116"; border.color: "#30303a"
                    Text { anchors.fill: parent; anchors.margins: 14; verticalAlignment: Text.AlignVCenter; text: appController.generatedPassword || "Generate a new value"; color: appController.generatedPassword.length ? "#eeeeF3" : "#686875"; font.family: "Consolas"; elide: Text.ElideRight }
                }
                RowLayout {
                    AppButton { text: "Generate"; enabled: length.acceptableInput; onClicked: appController.generatePassword(parseInt(length.text), upper.checked, lower.checked, numbers.checked, symbols.checked) }
                    AppButton { text: "Copy"; secondary: true; enabled: appController.generatedPassword.length > 0; onClicked: appController.copyGeneratedPassword() }
                }
                Item { Layout.fillHeight: true }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
