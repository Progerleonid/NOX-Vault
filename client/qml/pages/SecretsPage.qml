import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root
    function forceSearch() { search.forceActiveFocus() }
    function openAdd() { addName.text = ""; addValue.text = ""; addDialog.open(); addName.forceActiveFocus() }
    onVisibleChanged: if (!visible) appController.hideSensitive()
    Shortcut { sequence: "Ctrl+F"; enabled: root.visible; onActivated: root.forceSearch() }
    Shortcut { sequence: "Ctrl+N"; enabled: root.visible; onActivated: root.openAdd() }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 16
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Text { text: "Secrets"; color: "#f4f4f7"; font.pixelSize: 25; font.bold: true }
                Text { text: "Encrypted credentials in your vault"; color: "#858592"; font.pixelSize: 12 }
            }
            Item { Layout.fillWidth: true }
            TextField {
                id: search; Layout.preferredWidth: 280; implicitHeight: 40
                placeholderText: "Search secrets…"; color: "#ededf2"; placeholderTextColor: "#747481"
                onTextChanged: appController.secretsModel.filter = text
                background: Rectangle { radius: 8; color: "#17171d"; border.color: search.activeFocus ? "#7165e8" : "#34343f" }
            }
            AppButton { text: "+ Add secret"; onClicked: root.openAdd() }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; radius: 10; color: "#17171d"; border.color: "#2c2c35"
            RowLayout {
                anchors.fill: parent; spacing: 0
                Item {
                    Layout.preferredWidth: parent.width * 0.46; Layout.fillHeight: true
                    Text { anchors.centerIn: parent; visible: list.count === 0 && !appController.busy; text: search.text.length ? "No matching secrets" : "No secrets yet\n\nStore your first API key, token or password securely."; color: "#777784"; horizontalAlignment: Text.AlignHCenter }
                    ListView {
                        id: list; anchors.fill: parent; anchors.margins: 8; clip: true
                        model: appController.secretsModel; spacing: 4
                        delegate: Rectangle {
                            required property string name
                            required property string updatedAt
                            width: list.width; height: 56; radius: 7
                            color: appController.selectedSecret === name ? "#302d57" : mouse.containsMouse ? "#24242c" : "transparent"
                            Column {
                                anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 30; spacing: 3
                                Text { text: name; color: "#ededf2"; font.pixelSize: 13; elide: Text.ElideRight; width: parent.width }
                                Text { text: updatedAt.length ? "Updated " + updatedAt : "Update time unavailable"; color: "#777784"; font.pixelSize: 10; elide: Text.ElideRight; width: parent.width }
                            }
                            MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: appController.selectedSecret = name }
                        }
                    }
                }
                Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: "#2c2c35" }
                Item {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Text { anchors.centerIn: parent; visible: appController.selectedSecret.length === 0; text: "Select a secret"; color: "#777784" }
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 28; spacing: 14
                        visible: appController.selectedSecret.length > 0
                        Text { text: appController.selectedSecret; color: "#f5f5f8"; font.pixelSize: 21; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: "Secret value"; color: "#9292a0"; font.pixelSize: 12 }
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: 54; radius: 8; color: "#111116"; border.color: "#30303a"
                            Text { anchors.fill: parent; anchors.margins: 14; verticalAlignment: Text.AlignVCenter; text: appController.revealedSecret.length ? appController.revealedSecret : "••••••••••••••••••••••••"; color: "#e8e8ee"; font.family: "Consolas"; elide: Text.ElideRight }
                        }
                        RowLayout {
                            AppButton { text: appController.revealedSecret.length ? "Hide" : "Reveal"; secondary: true; onClicked: appController.revealedSecret.length ? appController.hideSensitive() : appController.revealSecret(appController.selectedSecret) }
                            AppButton { text: "Copy"; onClicked: appController.copySecret(appController.selectedSecret) }
                            AppButton { text: "Edit"; secondary: true; onClicked: { editValue.text = ""; editDialog.open(); editValue.forceActiveFocus() } }
                        }
                        Item { Layout.fillHeight: true }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#30303a" }
                        Text { text: "Danger zone"; color: "#e77d86"; font.pixelSize: 12; font.bold: true }
                        AppButton { text: "Delete secret"; destructive: true; Layout.alignment: Qt.AlignLeft; onClicked: deleteDialog.open() }
                    }
                }
            }
        }
    }

    AppDialog {
        id: addDialog; width: 440; heading: "Add secret"
        contentItem: ColumnLayout {
            spacing: 14
            FormField { id: addName; label: "Name"; placeholderText: "Type name"; Layout.fillWidth: true }
            FormField { id: addValue; label: "Value"; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText; Layout.fillWidth: true }
            RowLayout {
                Item { Layout.fillWidth: true }
                AppButton { text: "Cancel"; secondary: true; onClicked: { addValue.text = ""; addDialog.close() } }
                AppButton { text: "Save"; enabled: addName.text.length > 0 && addValue.text.length > 0 && !appController.busy; onClicked: { appController.addSecret(addName.text, addValue.text); addValue.text = ""; addDialog.close() } }
            }
        }
    }
    AppDialog {
        id: editDialog; width: 440; heading: "Update secret"
        contentItem: ColumnLayout {
            spacing: 14
            Text { text: appController.selectedSecret; color: "#d8d8e0"; font.bold: true }
            FormField { id: editValue; label: "New value"; echoMode: TextInput.Password; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText; Layout.fillWidth: true }
            RowLayout {
                Item { Layout.fillWidth: true }
                AppButton { text: "Cancel"; secondary: true; onClicked: { editValue.text = ""; editDialog.close() } }
                AppButton { text: "Update"; enabled: editValue.text.length > 0 && !appController.busy; onClicked: { appController.updateSecret(appController.selectedSecret, editValue.text); editValue.text = ""; editDialog.close() } }
            }
        }
    }
    AppDialog {
        id: deleteDialog; width: 420; heading: "Delete secret?"
        contentItem: ColumnLayout {
            spacing: 18
            Text { text: appController.selectedSecret + " will be permanently removed."; color: "#d8d8e0"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Item { Layout.fillWidth: true }
                AppButton { text: "Cancel"; secondary: true; onClicked: deleteDialog.close() }
                AppButton { text: "Delete"; destructive: true; enabled: !appController.busy; onClicked: { appController.deleteSecret(appController.selectedSecret); deleteDialog.close() } }
            }
        }
    }
}
