import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root
    readonly property var languageEntries: [
        {label: "English", code: "en"}, {label: "Русский", code: "ru"},
        {label: "Polski", code: "pl"}, {label: "Deutsch", code: "de"},
        {label: "Čeština", code: "cs"}
    ]
    function languageIndex(code) {
        for (let i = 0; i < languageEntries.length; ++i)
            if (languageEntries[i].code === code) return i
        return 0
    }
    Flickable {
        anchors.fill: parent; contentHeight: form.implicitHeight + 56; clip: true
        ColumnLayout {
            id: form; x: 28; y: 28; width: Math.min(parent.width - 56, 680); spacing: 16
            Text { text: appController.text("settings", appController.language); color: "#f4f4f7"; font.pixelSize: 25; font.bold: true }
            Text { text: appController.text("sharedSettings", appController.language); color: "#858592"; font.pixelSize: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: settingsColumn.implicitHeight + 48; radius: 10; color: "#19191f"; border.color: "#30303a"
                ColumnLayout {
                    id: settingsColumn; anchors.fill: parent; anchors.margins: 24; spacing: 14
                    FormField { id: server; label: "Server URL"; text: appController.serverUrl; placeholderText: "https://api.noxvault.tech"; Layout.fillWidth: true }
                    FormField { id: request; label: "Request timeout (seconds)"; text: appController.requestTimeout.toString(); validator: IntValidator { bottom: 1; top: 300 } Layout.fillWidth: true }
                    FormField { id: autoLock; label: "Auto-lock timeout (seconds)"; text: appController.autoLockTimeout.toString(); validator: IntValidator { bottom: 1; top: 86400 } Layout.fillWidth: true }
                    FormField { id: clipboard; label: "Clipboard timeout (seconds)"; text: appController.clipboardTimeout.toString(); validator: IntValidator { bottom: 1; top: 3600 } Layout.fillWidth: true }
                    Text { text: appController.text("language", appController.language); color: "#aaaab8"; font.pixelSize: 12; font.bold: true }
                    ComboBox {
                        id: language; Layout.fillWidth: true; implicitHeight: 42; textRole: "label"
                        model: root.languageEntries
                        Component.onCompleted: currentIndex = root.languageIndex(appController.language)
                        onActivated: appController.setLanguage(root.languageEntries[index].code)
                        contentItem: Text { leftPadding: 12; text: language.displayText; color: "#ededf2"; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { radius: 8; color: "#17171d"; border.color: language.activeFocus ? "#7165e8" : "#34343f" }
                        delegate: ItemDelegate {
                            width: language.width
                            contentItem: Text { text: modelData.label; color: "#ededf2"; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: highlighted ? "#302d57" : "#202027" }
                        }
                        popup: Popup {
                            y: language.height + 4; width: language.width; padding: 4
                            implicitHeight: contentItem.implicitHeight + 8
                            contentItem: ListView {
                                clip: true; implicitHeight: contentHeight
                                model: language.popup.visible ? language.delegateModel : null
                                currentIndex: language.highlightedIndex
                            }
                            background: Rectangle { color: "#202027"; border.color: "#3a3a46"; radius: 8 }
                        }
                    }
                    AppCheckBox { id: startLocked; text: appController.text("startLocked", appController.language); checked: appController.startLocked }
                    AppButton { text: appController.text("saveSettings", appController.language); enabled: !appController.busy && server.text.length > 0 && request.acceptableInput && autoLock.acceptableInput && clipboard.acceptableInput; onClicked: appController.saveSettings(server.text, parseInt(request.text), parseInt(autoLock.text), parseInt(clipboard.text), startLocked.checked, root.languageEntries[language.currentIndex].code) }
                }
            }
            Text { text: "Only HTTPS endpoints are accepted, except localhost development URLs."; color: "#777784"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
    Connections {
        target: appController
        function onStateChanged() {
            const wanted = root.languageIndex(appController.language)
            if (language.currentIndex !== wanted) language.currentIndex = wanted
        }
    }
}
