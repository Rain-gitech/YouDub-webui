import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import YouDub

// 设置页。对齐 apps/web/src/components/settings-dialog.tsx：
// YouTube Cookie、OpenAI（含 get models）、yt-dlp 代理。
Page {
    id: page
    background: Item {}

    signal backRequested()

    SettingsVm {
        id: vm
        authManager: AuthManager
    }

    Component.onCompleted: vm.loadAll()

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Theme.spaceLg

            // 顶栏
            RowLayout {
                Layout.leftMargin: Theme.spaceXl
                Layout.rightMargin: Theme.spaceXl
                Layout.topMargin: Theme.spaceLg
                spacing: Theme.spaceMd
                Button {
                    text: "‹ " + qsTr("返回")
                    flat: true
                    onClicked: page.backRequested()
                }
                Text {
                    text: qsTr("设置")
                    font.pixelSize: Theme.fontXl
                    font.bold: true
                    color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
            }

            // 语言
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spaceXl
                Layout.rightMargin: Theme.spaceXl
                color: Theme.surface
                radius: Theme.radiusLg
                border.color: Theme.border
                border.width: 1
                implicitHeight: langCol.implicitHeight + Theme.spaceLg * 2
                ColumnLayout {
                    id: langCol
                    anchors.fill: parent
                    anchors.margins: Theme.spaceLg
                    spacing: Theme.spaceSm
                    Text {
                        text: qsTr("界面语言")
                        font.pixelSize: Theme.fontLg
                        font.bold: true
                        color: Theme.textPrimary
                    }
                    RowLayout {
                        spacing: Theme.spaceSm
                        ComboBox {
                            id: langCombo
                            model: [ { key: "zh", label: qsTr("中文") }, { key: "en", label: "English" } ]
                            textRole: "label"
                            valueRole: "key"
                            currentIndex: indexOfValue(vm.language)
                            onActivated: vm.language = currentValue
                        }
                        Text {
                            text: qsTr("切换后重启生效")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontXs
                            visible: langCombo.currentValue !== vm.language
                        }
                    }
                }
            }

            // YouTube Cookie
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spaceXl
                Layout.rightMargin: Theme.spaceXl
                color: Theme.surface
                radius: Theme.radiusLg
                border.color: Theme.border
                border.width: 1
                implicitHeight: cookieCol.implicitHeight + Theme.spaceLg * 2
                ColumnLayout {
                    id: cookieCol
                    anchors.fill: parent
                    anchors.margins: Theme.spaceLg
                    spacing: Theme.spaceSm
                    Text {
                        text: qsTr("YouTube Cookie")
                        font.pixelSize: Theme.fontLg
                        font.bold: true
                        color: Theme.textPrimary
                    }
                    Text {
                        text: vm.cookieInfo
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontXs
                        visible: text.length > 0
                    }
                    Text {
                        text: qsTr("粘贴 Netscape 格式 cookie 内容。留空则不修改。")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontXs
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    TextArea {
                        id: cookieField
                        Layout.fillWidth: true
                        implicitHeight: 120
                        text: vm.cookieContent
                        onTextChanged: vm.cookieContent = text
                        font.family: "Consolas"
                        font.pixelSize: Theme.fontXs
                        color: Theme.textPrimary
                        background: Rectangle { color: Theme.surfaceMuted; radius: Theme.radiusSm }
                        wrapMode: TextEdit.Wrap
                    }
                }
            }

            // OpenAI
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spaceXl
                Layout.rightMargin: Theme.spaceXl
                color: Theme.surface
                radius: Theme.radiusLg
                border.color: Theme.border
                border.width: 1
                implicitHeight: openaiCol.implicitHeight + Theme.spaceLg * 2
                ColumnLayout {
                    id: openaiCol
                    anchors.fill: parent
                    anchors.margins: Theme.spaceLg
                    spacing: Theme.spaceSm
                    Text {
                        text: qsTr("OpenAI")
                        font.pixelSize: Theme.fontLg
                        font.bold: true
                        color: Theme.textPrimary
                    }
                    Text {
                        text: qsTr("Base URL")
                        font.pixelSize: Theme.fontXs
                        color: Theme.textSecondary
                    }
                    TextField {
                        Layout.fillWidth: true
                        text: vm.openaiBaseUrl
                        onTextEdited: vm.openaiBaseUrl = text
                        font.pixelSize: Theme.fontSm
                        placeholderText: "https://api.openai.com/v1"
                    }
                    Text {
                        text: qsTr("API Key") + (vm.openaiHasApiKey ? qsTr("（已配置，掩码显示）") : "")
                        font.pixelSize: Theme.fontXs
                        color: Theme.textSecondary
                    }
                    RowLayout {
                        spacing: Theme.spaceSm
                        TextField {
                            id: keyField
                            Layout.fillWidth: true
                            text: vm.openaiApiKey
                            onTextEdited: vm.openaiApiKey = text
                            font.pixelSize: Theme.fontSm
                            echoMode: showKey.checked ? TextInput.Normal : TextInput.Password
                            placeholderText: "sk-..."
                        }
                        CheckBox {
                            id: showKey
                            text: qsTr("显示")
                        }
                    }
                    Text {
                        text: qsTr("Model")
                        font.pixelSize: Theme.fontXs
                        color: Theme.textSecondary
                    }
                    RowLayout {
                        spacing: Theme.spaceSm
                        ComboBox {
                            id: modelCombo
                            Layout.fillWidth: true
                            editable: true
                            model: vm.openaiModels
                            editText: vm.openaiModel
                            onActivated: vm.openaiModel = currentText
                            onAccepted: vm.openaiModel = editText
                            Connections {
                                target: vm
                                function onOpenaiModelChanged() {
                                    if (modelCombo.editText !== vm.openaiModel)
                                        modelCombo.editText = vm.openaiModel
                                }
                            }
                        }
                        Button {
                            text: vm.modelsLoading ? qsTr("获取中…") : qsTr("获取模型")
                            enabled: !vm.modelsLoading
                            onClicked: vm.fetchModels()
                        }
                    }
                    Text {
                        text: qsTr("翻译并发数")
                        font.pixelSize: Theme.fontXs
                        color: Theme.textSecondary
                    }
                    TextField {
                        Layout.fillWidth: true
                        text: vm.openaiTranslateConcurrency
                        onTextEdited: vm.openaiTranslateConcurrency = text
                        font.pixelSize: Theme.fontSm
                        placeholderText: "1"
                    }
                }
            }

            // yt-dlp
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spaceXl
                Layout.rightMargin: Theme.spaceXl
                color: Theme.surface
                radius: Theme.radiusLg
                border.color: Theme.border
                border.width: 1
                implicitHeight: ytdlpCol.implicitHeight + Theme.spaceLg * 2
                ColumnLayout {
                    id: ytdlpCol
                    anchors.fill: parent
                    anchors.margins: Theme.spaceLg
                    spacing: Theme.spaceSm
                    Text {
                        text: qsTr("yt-dlp 代理")
                        font.pixelSize: Theme.fontLg
                        font.bold: true
                        color: Theme.textPrimary
                    }
                    Text {
                        text: qsTr("代理端口（留空禁用）")
                        font.pixelSize: Theme.fontXs
                        color: Theme.textSecondary
                    }
                    TextField {
                        Layout.fillWidth: true
                        text: vm.ytdlpProxyPort
                        onTextEdited: vm.ytdlpProxyPort = text
                        font.pixelSize: Theme.fontSm
                        placeholderText: "7890"
                    }
                }
            }

            // 消息 + 保存
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spaceXl
                Layout.rightMargin: Theme.spaceXl
                Layout.bottomMargin: Theme.spaceXl
                spacing: Theme.spaceSm
                Text {
                    text: vm.errorMessage
                    color: Theme.statusFailed
                    font.pixelSize: Theme.fontXs
                    visible: text.length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                Text {
                    text: vm.infoMessage
                    color: Theme.statusSucceeded
                    font.pixelSize: Theme.fontXs
                    visible: text.length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                Item { Layout.fillWidth: true; visible: vm.errorMessage.length === 0 && vm.infoMessage.length === 0 }
                Button {
                    text: vm.loading ? qsTr("加载中…") : (vm.saving ? qsTr("保存中…") : qsTr("保存"))
                    enabled: !vm.loading && !vm.saving
                    onClicked: vm.saveAll()
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
        }
    }
}
