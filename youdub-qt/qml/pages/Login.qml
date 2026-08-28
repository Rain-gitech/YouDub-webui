import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import YouDub

// 登录页。对齐 WebUI 的登录交互：密码 -> 错误提示 / 成功跳转。
Page {
    background: Item {}

    ColumnLayout {
        anchors.centerIn: parent
        width: 360
        spacing: Theme.spaceLg

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("YouDub")
            font.pixelSize: Theme.fontXl
            font.bold: true
            color: Theme.textPrimary
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("请输入访问密码")
            font.pixelSize: Theme.fontSm
            color: Theme.textSecondary
        }

        TextField {
            id: passwordField
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: qsTr("密码")
            font.pixelSize: Theme.fontMd
            onAccepted: AuthManager.login(text)
            enabled: !AuthManager.busy
            background: Rectangle {
                color: Theme.surface
                radius: Theme.radiusSm
                border.color: passwordField.activeFocus ? Theme.primary : Theme.border
                border.width: 1
            }
        }

        Button {
            Layout.fillWidth: true
            text: AuthManager.busy ? qsTr("登录中…") : qsTr("登录")
            enabled: !AuthManager.busy && passwordField.text.length > 0
            onClicked: AuthManager.login(passwordField.text)
            contentItem: Text {
                text: parent.text
                color: "white"
                font.pixelSize: Theme.fontMd
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.enabled ? (parent.down ? Theme.primaryHover : Theme.primary) : "#cbd5e1"
                radius: Theme.radiusSm
            }
        }

        Text {
            Layout.fillWidth: true
            text: AuthManager.errorMessage
            color: Theme.statusFailed
            font.pixelSize: Theme.fontXs
            visible: text.length > 0
            wrapMode: Text.Wrap
        }
    }
}
