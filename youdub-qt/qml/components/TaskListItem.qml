import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import YouDub

// 任务列表行。model 数据来自 TaskListModel 角色。
Item {
    id: root
    width: parent ? parent.width : 0
    height: 56

    signal clicked()

    Rectangle {
        anchors.fill: parent
        color: parent.ListView.isCurrentItem ? Theme.surfaceMuted : (mouse.contains ? Theme.surfaceMuted : "transparent")
        opacity: 0.7
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spaceLg
        anchors.rightMargin: Theme.spaceLg
        spacing: Theme.spaceMd

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text {
                Layout.fillWidth: true
                text: model.title || model.url
                elide: Text.ElideRight
                font.pixelSize: Theme.fontSm
                font.bold: true
                color: Theme.textPrimary
            }
            RowLayout {
                spacing: Theme.spaceXs
                StatusBadge { status: model.status }
                Text {
                    text: model.createdAt
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                    elide: Text.ElideRight
                }
                Text {
                    visible: (model.status === "queued" || model.status === "running") && model.currentStage.length > 0
                    text: "· " + model.currentStage
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                    elide: Text.ElideRight
                }
            }
        }

        Text {
            text: "›"
            font.pixelSize: Theme.fontLg
            color: Theme.textMuted
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
