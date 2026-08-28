import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import YouDub

// 任务详情页。阶段进度 + 日志 + 动作。
Page {
    id: page
    background: Item {}

    property string taskId: ""
    signal backRequested()
    signal deletedRequested()

    TaskDetailVm {
        id: vm
        authManager: AuthManager
        taskId: page.taskId
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 顶栏
        RowLayout {
            Layout.leftMargin: Theme.spaceLg
            Layout.rightMargin: Theme.spaceLg
            Layout.topMargin: Theme.spaceMd
            spacing: Theme.spaceSm
            Button {
                text: "‹ " + qsTr("返回")
                flat: true
                onClicked: page.backRequested()
            }
            Item { Layout.fillWidth: true }
            StatusBadge { status: vm.status }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.spaceLg
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: Theme.spaceLg

                // 概览卡片
                Rectangle {
                    Layout.fillWidth: true
                    color: Theme.surface
                    radius: Theme.radiusLg
                    border.color: Theme.border
                    border.width: 1
                    implicitHeight: infoCol.implicitHeight + Theme.spaceLg * 2
                    ColumnLayout {
                        id: infoCol
                        anchors.fill: parent
                        anchors.margins: Theme.spaceLg
                        spacing: Theme.spaceXs
                        Text {
                            Layout.fillWidth: true
                            text: vm.title || vm.url
                            font.pixelSize: Theme.fontLg
                            font.bold: true
                            color: Theme.textPrimary
                            wrapMode: Text.Wrap
                        }
                        Text {
                            text: vm.url
                            font.pixelSize: Theme.fontXs
                            color: Theme.textMuted
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            visible: vm.currentStage.length > 0
                            text: qsTr("当前阶段：") + vm.currentStage
                            font.pixelSize: Theme.fontXs
                            color: Theme.textSecondary
                        }
                        Text {
                            visible: vm.errorMessage.length > 0
                            text: vm.errorMessage
                            color: Theme.statusFailed
                            font.pixelSize: Theme.fontXs
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }
                }

                // 阶段列表
                Rectangle {
                    Layout.fillWidth: true
                    color: Theme.surface
                    radius: Theme.radiusLg
                    border.color: Theme.border
                    border.width: 1
                    implicitHeight: stageCol.implicitHeight + Theme.spaceLg * 2
                    ColumnLayout {
                        id: stageCol
                        anchors.fill: parent
                        anchors.margins: Theme.spaceLg
                        spacing: Theme.spaceSm
                        Text {
                            text: qsTr("处理阶段")
                            font.pixelSize: Theme.fontMd
                            font.bold: true
                            color: Theme.textPrimary
                        }
                        Repeater {
                            model: vm.stages
                            delegate: ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                RowLayout {
                                    spacing: Theme.spaceSm
                                    StatusBadge { status: model.stageStatus }
                                    Text {
                                        text: model.stageLabel
                                        font.pixelSize: Theme.fontSm
                                        color: Theme.textPrimary
                                    }
                                    Item { Layout.fillWidth: true }
                                    // manual 模式下，已完成阶段可重做
                                    Button {
                                        visible: vm.executionMode === "manual"
                                                 && (model.stageStatus === "succeeded" || model.stageStatus === "failed")
                                                 && vm.status !== "running" && vm.status !== "queued"
                                        text: qsTr("重做")
                                        flat: true
                                        enabled: !vm.busy
                                        onClicked: vm.redoStage(model.stageName)
                                    }
                                }
                                ProgressBar {
                                    Layout.fillWidth: true
                                    visible: model.stageProgress >= 0
                                    from: 0; to: 1.0
                                    value: model.stageProgress >= 0 ? model.stageProgress : 0
                                }
                                Text {
                                    visible: model.stageMessage.length > 0
                                    text: model.stageMessage
                                    font.pixelSize: Theme.fontXs
                                    color: Theme.textMuted
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                // 日志
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220
                    color: Theme.surfaceMuted
                    radius: Theme.radiusMd
                    border.color: Theme.border
                    border.width: 1
                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: Theme.spaceSm
                        TextArea {
                            text: vm.log
                            readOnly: true
                            font.family: "Consolas"
                            font.pixelSize: Theme.fontXs
                            color: Theme.textPrimary
                            background: Item {}
                            textFormat: TextEdit.PlainText
                        }
                    }
                }
            }
        }

        // 动作栏
        RowLayout {
            Layout.leftMargin: Theme.spaceLg
            Layout.rightMargin: Theme.spaceLg
            Layout.bottomMargin: Theme.spaceLg
            spacing: Theme.spaceSm
            Text {
                text: vm.actionError
                color: Theme.statusFailed
                font.pixelSize: Theme.fontXs
                visible: text.length > 0
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            Item { Layout.fillWidth: true; visible: vm.actionError.length === 0 }
            Button {
                visible: vm.status === "failed"
                text: qsTr("从失败继续")
                enabled: !vm.busy
                onClicked: vm.resume()
            }
            Button {
                visible: vm.status === "paused"
                text: qsTr("继续")
                enabled: !vm.busy
                onClicked: vm.continueTask()
            }
            Button {
                visible: vm.status !== "running" && vm.status !== "queued"
                text: qsTr("重跑")
                enabled: !vm.busy
                onClicked: vm.rerun()
            }
            Button {
                text: qsTr("删除")
                enabled: !vm.busy && vm.status !== "running"
                onClicked: vm.deleteTask()
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: Theme.fontSm; horizontalAlignment: Text.AlignHCenter }
                background: Rectangle { color: parent.enabled ? Theme.statusFailed : "#cbd5e1"; radius: Theme.radiusSm }
            }
        }
    }

    Connections {
        target: vm
        function onDeleted() { page.deletedRequested() }
    }
}
