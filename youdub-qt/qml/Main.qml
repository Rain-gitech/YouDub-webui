import QtQuick
import QtQuick.Controls.Basic
import YouDub

// 顶层窗口。根据 AuthManager.authenticated 在 Login 与主内容之间切换。
ApplicationWindow {
    id: window
    width: 1100
    height: 760
    minimumWidth: 720
    minimumHeight: 560
    visible: true
    title: qsTr("YouDub")

    // 柔和渐变背景（近似 WebUI）。
    background: Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.bgStart }
            GradientStop { position: 0.5; color: Theme.bgMid }
            GradientStop { position: 1.0; color: Theme.bgEnd }
        }
    }

    // 启动时探测会话。
    Component.onCompleted: AuthManager.checkSession()

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: AuthManager.authenticated ? homeComponent : loginComponent
    }

    // 认证状态变化时切换根页面。
    Connections {
        target: AuthManager
        function onAuthenticatedChanged() {
            if (AuthManager.authenticated) {
                stack.replace(homeComponent)
            } else {
                stack.replace(loginComponent)
            }
        }
    }

    Component { id: loginComponent; Login {} }
    Component { id: homeComponent; Home {} }
}
