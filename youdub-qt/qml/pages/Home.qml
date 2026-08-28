import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import YouDub

// 阶段1占位 Home。后续阶段替换为任务列表 + 创建表单。
Page {
    background: Item {}

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Theme.spaceMd

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("已登录")
            font.pixelSize: Theme.fontXl
            color: Theme.textPrimary
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("阶段 1 脚手架就绪：任务列表将在阶段 2 实现。")
            font.pixelSize: Theme.fontSm
            color: Theme.textSecondary
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("退出登录")
            onClicked: AuthManager.logout()
        }
    }
}
