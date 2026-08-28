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

    background: Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.bgStart }
            GradientStop { position: 0.5; color: Theme.bgMid }
            GradientStop { position: 1.0; color: Theme.bgEnd }
        }
    }

    Component.onCompleted: AuthManager.checkSession()

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: AuthManager.authenticated ? homeComponent : loginComponent
    }

    Connections {
        target: AuthManager
        function onAuthenticatedChanged() {
            if (AuthManager.authenticated) {
                stack.clear();
                stack.replace(homeComponent);
            } else {
                stack.clear();
                stack.replace(loginComponent);
            }
        }
    }

    Component {
        id: loginComponent
        Login {}
    }

    Component {
        id: homeComponent
        Home {
            onOpenTaskRequested: function(taskId) {
                stack.push(detailComponent, { taskId: taskId });
            }
        }
    }

    Component {
        id: detailComponent
        TaskDetail {
            onBackRequested: stack.pop()
            onDeletedRequested: stack.pop()
        }
    }
}
