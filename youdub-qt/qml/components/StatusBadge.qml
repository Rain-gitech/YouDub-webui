import QtQuick
import YouDub

// 状态徽章。status 取值：queued/running/paused/succeeded/failed。
// 对齐 WebUI 的 statusBadgeClass 配色。
Rectangle {
    id: root
    property string status: ""
    property var _colorMap: ({
        "queued": Theme.statusQueued,
        "running": Theme.statusRunning,
        "paused": Theme.statusPaused,
        "succeeded": Theme.statusSucceeded,
        "failed": Theme.statusFailed
    })
    property var _labelMap: ({
        "queued": qsTr("排队"),
        "running": qsTr("运行中"),
        "paused": qsTr("已暂停"),
        "succeeded": qsTr("成功"),
        "failed": qsTr("失败")
    })

    color: _colorMap[status] ?? Theme.statusQueued
    radius: 10
    implicitWidth: label.implicitWidth + 14
    implicitHeight: label.implicitHeight + 6

    Text {
        id: label
        anchors.centerIn: parent
        text: _labelMap[status] ?? status
        color: "white"
        font.pixelSize: Theme.fontXs
        font.bold: true
    }
}
