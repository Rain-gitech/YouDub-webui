pragma Singleton
import QtQuick

// 全局主题：颜色 / 间距 / 字体。对齐现有 WebUI 的柔和渐变风格。
QtObject {
    // 背景（柔和渐变，近似 WebUI 的 #fff5f5 -> #f2fbff -> #fff4fa）
    readonly property color bgStart: "#fff5f5"
    readonly property color bgMid: "#f2fbff"
    readonly property color bgEnd: "#fff4fa"

    // 表面 / 边框
    readonly property color surface: "#ffffff"
    readonly property color surfaceMuted: "#f7f7f8"
    readonly property color border: "#e4e4e7"
    readonly property color borderMuted: "#ececef"

    // 文本
    readonly property color textPrimary: "#18181b"
    readonly property color textSecondary: "#71717a"
    readonly property color textMuted: "#a1a1aa"

    // 品牌
    readonly property color primary: "#e11d48"      // 玫红，呼应 logo
    readonly property color primaryHover: "#be123c"

    // 状态色（对齐 statusBadgeClass）
    readonly property color statusQueued: "#6b7280"
    readonly property color statusRunning: "#2563eb"
    readonly property color statusPaused: "#d97706"
    readonly property color statusSucceeded: "#16a34a"
    readonly property color statusFailed: "#dc2626"

    // 间距
    readonly property int spaceXs: 4
    readonly property int spaceSm: 8
    readonly property int spaceMd: 12
    readonly property int spaceLg: 16
    readonly property int spaceXl: 24

    // 圆角
    readonly property int radiusSm: 6
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14

    // 字体大小
    readonly property int fontXs: 11
    readonly property int fontSm: 13
    readonly property int fontMd: 15
    readonly property int fontLg: 18
    readonly property int fontXl: 24
}
