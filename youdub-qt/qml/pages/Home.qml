import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import YouDub

// 首页：任务创建表单 + 任务历史列表。
// 阶段2：URL 创建 + 列表/分页/过滤/排序/轮询。本地上传留到阶段4。
Page {
    id: page
    background: Item {}

    // 供 Main 注入导航回调（阶段3接入详情页）。
    signal openTaskRequested(string taskId)

    TaskListVm {
        id: vm
        authManager: AuthManager
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Theme.spaceLg

            // 顶部标题栏
            RowLayout {
                Layout.leftMargin: Theme.spaceXl
                Layout.rightMargin: Theme.spaceXl
                Layout.topMargin: Theme.spaceLg
                spacing: Theme.spaceMd
                Text {
                    text: qsTr("YouDub")
                    font.pixelSize: Theme.fontXl
                    font.bold: true
                    color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("退出登录")
                    flat: true
                    onClicked: AuthManager.logout()
                }
            }

            // 创建任务卡片
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spaceXl
                Layout.rightMargin: Theme.spaceXl
                color: Theme.surface
                radius: Theme.radiusLg
                border.color: Theme.border
                border.width: 1
                implicitHeight: col.implicitHeight + Theme.spaceLg * 2
                ColumnLayout {
                    id: col
                    anchors.fill: parent
                    anchors.margins: Theme.spaceLg
                    spacing: Theme.spaceMd
                    Text {
                        text: qsTr("创建任务")
                        font.pixelSize: Theme.fontLg
                        font.bold: true
                        color: Theme.textPrimary
                    }
                    RowLayout {
                        spacing: Theme.spaceSm
                        TextField {
                            id: urlField
                            Layout.fillWidth: true
                            placeholderText: qsTr("YouTube 或 Bilibili 链接")
                            font.pixelSize: Theme.fontSm
                            onAccepted: submit()
                            enabled: !vm.creating
                        }
                        ComboBox {
                            id: modeCombo
                            model: [ { key: "auto", label: qsTr("自动") }, { key: "manual", label: qsTr("手动") } ]
                            textRole: "label"
                            valueRole: "key"
                            enabled: !vm.creating
                        }
                        Button {
                            text: vm.creating ? qsTr("创建中…") : qsTr("创建")
                            enabled: !vm.creating && urlField.text.trim().length > 0
                            onClicked: submit()
                            contentItem: Text {
                                text: parent.text
                                color: "white"
                                font.pixelSize: Theme.fontSm
                                horizontalAlignment: Text.AlignHCenter
                            }
                            background: Rectangle {
                                color: parent.enabled ? (parent.down ? Theme.primaryHover : Theme.primary) : "#cbd5e1"
                                radius: Theme.radiusSm
                            }
                        }
                    }
                    Text {
                        text: vm.errorMessage
                        color: Theme.statusFailed
                        font.pixelSize: Theme.fontXs
                        visible: text.length > 0
                    }
                }
            }

            // 任务历史卡片
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spaceXl
                Layout.rightMargin: Theme.spaceXl
                Layout.bottomMargin: Theme.spaceXl
                color: Theme.surface
                radius: Theme.radiusLg
                border.color: Theme.border
                border.width: 1
                implicitHeight: historyCol.implicitHeight + Theme.spaceLg * 2

                ColumnLayout {
                    id: historyCol
                    anchors.fill: parent
                    anchors.margins: Theme.spaceLg
                    spacing: Theme.spaceMd

                    RowLayout {
                        spacing: Theme.spaceMd
                        Text {
                            text: qsTr("任务历史") + " (" + vm.total + ")"
                            font.pixelSize: Theme.fontLg
                            font.bold: true
                            color: Theme.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        ComboBox {
                            model: [
                                { key: "all", label: qsTr("全部状态") },
                                { key: "queued", label: qsTr("排队") },
                                { key: "running", label: qsTr("运行中") },
                                { key: "paused", label: qsTr("已暂停") },
                                { key: "succeeded", label: qsTr("成功") },
                                { key: "failed", label: qsTr("失败") }
                            ]
                            textRole: "label"
                            valueRole: "key"
                            onActivated: vm.statusFilter = currentValue
                            Component.onCompleted: currentIndex = indexOfValue(vm.statusFilter)
                        }
                        ComboBox {
                            model: [
                                { key: "created_desc", label: qsTr("最新优先") },
                                { key: "created_asc", label: qsTr("最早优先") },
                                { key: "status_asc", label: qsTr("按状态") }
                            ]
                            textRole: "label"
                            valueRole: "key"
                            onActivated: vm.sort = currentValue
                            Component.onCompleted: currentIndex = indexOfValue(vm.sort)
                        }
                    }

                    // 任务列表
                    ListView {
                        id: list
                        Layout.fillWidth: true
                        implicitHeight: Math.min(420, contentHeight)
                        clip: true
                        model: vm.model
                        spacing: 0
                        delegate: TaskListItem {
                            width: ListView.view.width
                            onClicked: page.openTaskRequested(model.taskId)
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: list.count === 0
                            text: qsTr("暂无任务")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                        }
                    }

                    // 分页
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("第 %1 页").arg(vm.page)
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontXs
                        }
                        Button {
                            text: "‹"
                            enabled: vm.page > 1
                            onClicked: vm.page = vm.page - 1
                        }
                        Button {
                            text: "›"
                            enabled: list.count >= vm.pageSize
                            onClicked: vm.page = vm.page + 1
                        }
                    }
                }
            }
        }
    }

    function submit() {
        const u = urlField.text.trim()
        if (u.length === 0) return
        vm.createTask(u, modeCombo.currentValue)
        urlField.text = ""
    }

    // 创建成功后（阶段2）刷新已在 VM 内执行；阶段3接入跳转详情。
    Connections {
        target: vm
        function onTaskCreated(taskId) { /* 阶段3：openTaskRequested(taskId) */ }
    }
}
