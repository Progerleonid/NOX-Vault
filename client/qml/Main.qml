import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "pages"

ApplicationWindow {
    id: window
    width: 1180; height: 760
    minimumWidth: 900; minimumHeight: 600
    visible: !smokeTest
    title: "NOX Vault"
    color: "#101014"

    Component.onCompleted: if (!smokeTest) appController.start()
    onClosing: appController.hideSensitive()

    Shortcut { sequence: "Ctrl+L"; enabled: appController.screen === "app"; onActivated: appController.lock() }
    Shortcut { sequence: "Ctrl+,"; enabled: appController.screen === "app"; onActivated: appController.currentPage = "settings" }
    Shortcut { sequence: "Escape"; onActivated: appController.hideSensitive() }

    header: Rectangle {
        height: 62; color: "#15151b"; border.color: "#292932"
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 22
            Text { text: "NOX"; color: "#8f84ff"; font.pixelSize: 19; font.bold: true }
            Text { text: "Vault"; color: "#f0f0f5"; font.pixelSize: 19; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            StatusPill { healthy: appController.serverHealthy; label: healthy ? "Server online" : "Server offline" }
        }
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            visible: appController.errorMessage.length > 0 || appController.notice.length > 0
            Layout.fillWidth: true; implicitHeight: visible ? 42 : 0
            color: appController.errorMessage.length > 0 ? "#3a2024" : "#173329"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 12
                Text { Layout.fillWidth: true; text: appController.errorMessage || appController.notice; color: "#f1f1f5"; font.pixelSize: 13 }
                Rectangle {
                    Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 7
                    color: closeMouse.containsMouse ? "#ffffff18" : "transparent"
                    Text { anchors.centerIn: parent; text: "×"; color: "#e5e5eb"; font.pixelSize: 19 }
                    MouseArea { id: closeMouse; anchors.fill: parent; hoverEnabled: true; onClicked: appController.clearMessage() }
                    Behavior on color { ColorAnimation { duration: 100 } }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                visible: appController.screen === "app"
                Layout.preferredWidth: 210; Layout.fillHeight: true
                color: "#15151b"; border.color: "#292932"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 4
                    SidebarButton { Layout.fillWidth: true; text: appController.text("secrets", appController.language); selected: appController.currentPage === "secrets"; onClicked: appController.currentPage = "secrets" }
                    SidebarButton { Layout.fillWidth: true; text: appController.text("generator", appController.language); selected: appController.currentPage === "generator"; onClicked: appController.currentPage = "generator" }
                    SidebarButton { Layout.fillWidth: true; text: appController.text("backup", appController.language); selected: appController.currentPage === "backup"; onClicked: appController.currentPage = "backup" }
                    SidebarButton { Layout.fillWidth: true; text: appController.text("security", appController.language); selected: appController.currentPage === "security"; onClicked: appController.currentPage = "security" }
                    SidebarButton { Layout.fillWidth: true; text: appController.text("diagnostics", appController.language); selected: appController.currentPage === "diagnostics"; onClicked: appController.currentPage = "diagnostics" }
                    SidebarButton { Layout.fillWidth: true; text: appController.text("settings", appController.language); selected: appController.currentPage === "settings"; onClicked: appController.currentPage = "settings" }
                    Item { Layout.fillHeight: true }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 5
                        AppButton { id: accountButton; Layout.fillWidth: true; secondary: true; text: appController.email + "  ▴"; onClicked: accountPopup.open() }
                    }
                }
            }
            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                BusyIndicator { anchors.centerIn: parent; running: appController.busy && appController.screen === "loading"; visible: running }
                Loader {
                    id: pageLoader
                    anchors.fill: parent
                    active: appController.screen !== "loading" || !appController.busy
                    sourceComponent: appController.screen === "loading" ? unavailableComponent
                                   : appController.screen === "login" ? authComponent
                                   : appController.screen === "setup" ? setupComponent
                                   : appController.screen === "unlock" ? unlockComponent : appComponent
                    opacity: status === Loader.Ready ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                }
            }
        }
        Rectangle {
            visible: appController.screen === "app"
            Layout.fillWidth: true; implicitHeight: 34; color: "#15151b"; border.color: "#292932"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16
                Text { text: "Vault: " + (appController.vaultUnlocked ? "Unlocked" : "Locked"); color: "#aaaab8"; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Text { text: "Version " + appController.version; color: "#777784"; font.pixelSize: 11 }
            }
        }
    }

    Popup {
        id: accountPopup
        parent: Overlay.overlay
        x: Math.max(12, accountButton.mapToItem(Overlay.overlay, 0, 0).x)
        y: Math.max(12, accountButton.mapToItem(Overlay.overlay, 0, 0).y - height - 8)
        implicitWidth: 260; padding: 8
        background: Rectangle { color: "#202027"; radius: 10; border.color: "#3a3a46" }
        contentItem: ColumnLayout {
            spacing: 5
            Repeater {
                model: appController.accounts
                AppButton {
                    required property var modelData
                    Layout.fillWidth: true; secondary: true
                    text: (modelData.active ? "✓  " : "   ") + modelData.email
                    onClicked: { accountPopup.close(); appController.switchAccount(modelData.userId) }
                }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#383843" }
            AppButton { Layout.fillWidth: true; secondary: true; text: "+ " + appController.text("addAccount", appController.language); onClicked: { accountPopup.close(); appController.addAccount() } }
        }
    }

    Component { id: authComponent; AuthPage {} }
    Component {
        id: unavailableComponent
        Item {
            ColumnLayout {
                anchors.centerIn: parent; spacing: 14
                Text { text: "NOX Vault is unavailable"; color: "#f4f4f7"; font.pixelSize: 22; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                Text { text: "Check the server URL and your connection, then try again."; color: "#858592"; Layout.alignment: Qt.AlignHCenter }
                AppButton { text: "Retry"; enabled: !appController.busy; onClicked: appController.retry(); Layout.alignment: Qt.AlignHCenter }
            }
        }
    }
    Component { id: setupComponent; VaultSetupPage {} }
    Component { id: unlockComponent; UnlockPage {} }
    Component {
        id: appComponent
        Item {
            SecretsPage { id: secretsPage; anchors.fill: parent; visible: appController.currentPage === "secrets" }
            GeneratorPage { anchors.fill: parent; visible: appController.currentPage === "generator" }
            BackupPage { anchors.fill: parent; visible: appController.currentPage === "backup" }
            SecurityPage { anchors.fill: parent; visible: appController.currentPage === "security" }
            DiagnosticsPage { anchors.fill: parent; visible: appController.currentPage === "diagnostics" }
            SettingsPage { anchors.fill: parent; visible: appController.currentPage === "settings" }
        }
    }
}
