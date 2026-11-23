import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: root

    signal backRequested()
    signal retestRequested()

    property var audioDevices: []
    readonly property bool darkPreview: settings.darkMode

    // 当前默认分辨率匹配到的预设值；不匹配任何预设时为 "custom"。
    readonly property string resolutionPreset: {
        const w = settings.defaultWidth
        const h = settings.defaultHeight
        const presets = [
            { value: "1920x1080", w: 1920, h: 1080 },
            { value: "1280x720", w: 1280, h: 720 },
            { value: "2560x1440", w: 2560, h: 1440 },
            { value: "3840x2160", w: 3840, h: 2160 },
            { value: "1080x1920", w: 1080, h: 1920 },
            { value: "1080x1080", w: 1080, h: 1080 },
            { value: "2560x1080", w: 2560, h: 1080 }
        ]
        for (let i = 0; i < presets.length; ++i) {
            if (presets[i].w === w && presets[i].h === h)
                return presets[i].value
        }
        return "custom"
    }

    // 选择“自定义”或手动修改尺寸后为 true；选择预设后为 false。
    property bool customResolution: false

    function refreshAudioDevices() {
        audioDevices = settings.audioOutputDevices()
        let selected = 0
        for (let i = 0; i < audioDevices.length; ++i) {
            if (audioDevices[i].id === settings.audioOutput)
                selected = i
        }
        audioCombo.currentIndex = selected
    }

    Component.onCompleted: {
        customResolution = (resolutionPreset === "custom")
        refreshAudioDevices()
    }
    onVisibleChanged: {
        if (visible)
            refreshAudioDevices()
    }

    // 切换语言时 qsTr 会重新求值并重建 ComboBox 的模型，
    // ComboBox 内部会把 currentIndex 重置为 0；这里在模型或设置值变化后重新同步选中项。
    component SettingCombo: Controls.ComboBox {
        id: combo
        property string settingValue: ""

        function syncFromSetting() {
            const index = combo.indexOfValue(combo.settingValue)
            if (index >= 0 && index !== combo.currentIndex)
                combo.currentIndex = index
        }

        Component.onCompleted: Qt.callLater(syncFromSetting)
        onModelChanged: Qt.callLater(syncFromSetting)
        onSettingValueChanged: syncFromSetting()
    }

    Controls.ScrollView {
        id: view
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.max(320, view.availableWidth - 2 * Kirigami.Units.largeSpacing)
            x: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.gridUnit

            RowLayout {
                Layout.fillWidth: true
                Controls.ToolButton {
                    icon.name: "go-previous"
                    icon.width: 22
                    icon.height: 22
                    hoverEnabled: true
                    Controls.ToolTip.text: qsTr("返回")
                    onClicked: root.backRequested()
                }
                Kirigami.Heading {
                    text: qsTr("设置")
                    level: 1
                    Layout.fillWidth: true
                }
            }

            // 主题色预览条：窗口色 / 内容底色 / 强调色
            Controls.Frame {
                Layout.fillWidth: true
                implicitHeight: Kirigami.Units.gridUnit * 1.2
                palette.window: root.darkPreview ? "#18181b" : "#f4f4f5"
                palette.base: root.darkPreview ? "#111113" : "#ffffff"
                RowLayout {
                    anchors.fill: parent
                    spacing: Kirigami.Units.smallSpacing
                    Rectangle {
                        color: root.darkPreview ? "#18181b" : "#f4f4f5"
                        radius: 2
                        Layout.fillHeight: true
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 1.5
                    }
                    Rectangle {
                        color: root.darkPreview ? "#111113" : "#ffffff"
                        radius: 2
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        color: root.darkPreview ? "#3b82f6" : "#2563eb"
                        radius: 2
                        Layout.fillHeight: true
                        Layout.preferredWidth: Kirigami.Units.gridUnit
                    }
                }
            }

            Kirigami.FormLayout {
                Layout.fillWidth: true
                wideMode: true

                SettingCombo {
                    Kirigami.FormData.label: qsTr("语言")
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: qsTr("简体中文"), value: "zh_CN" },
                        { text: qsTr("English"), value: "en_US" }
                    ]
                    settingValue: settings.language
                    onActivated: settings.language = currentValue
                }

                SettingCombo {
                    Kirigami.FormData.label: qsTr("主题")
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: qsTr("跟随系统"), value: "system" },
                        { text: qsTr("浅色"), value: "light" },
                        { text: qsTr("深色"), value: "dark" }
                    ]
                    settingValue: settings.theme
                    onActivated: settings.theme = currentValue
                }

                Controls.CheckBox {
                    Kirigami.FormData.label: qsTr("自动保存")
                    Layout.fillWidth: true
                    text: qsTr("编辑时自动保存草稿")
                    checked: settings.autosaveEnabled
                    onToggled: settings.autosaveEnabled = checked
                }

                Controls.Label {
                    text: qsTr("建议保持开启；关闭后仅在手动保存时写入，异常退出可能丢失修改。")
                    wrapMode: Text.Wrap
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                }

                SettingCombo {
                    Kirigami.FormData.label: qsTr("默认分辨率")
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: qsTr("1080p · 16:9 · 1920 × 1080"), value: "1920x1080" },
                        { text: qsTr("720p · 16:9 · 1280 × 720"), value: "1280x720" },
                        { text: qsTr("2K · 16:9 · 2560 × 1440"), value: "2560x1440" },
                        { text: qsTr("4K · 16:9 · 3840 × 2160"), value: "3840x2160" },
                        { text: qsTr("竖屏 1080p · 9:16 · 1080 × 1920"), value: "1080x1920" },
                        { text: qsTr("方屏 · 1:1 · 1080 × 1080"), value: "1080x1080" },
                        { text: qsTr("超宽屏 1080p · 21:9 · 2560 × 1080"), value: "2560x1080" },
                        { text: qsTr("自定义"), value: "custom" }
                    ]
                    settingValue: root.customResolution ? "custom" : root.resolutionPreset
                    onActivated: {
                        if (currentValue === "custom") {
                            root.customResolution = true
                        } else {
                            root.customResolution = false
                            const parts = currentValue.split("x")
                            settings.defaultWidth = parseInt(parts[0], 10)
                            settings.defaultHeight = parseInt(parts[1], 10)
                        }
                    }
                }

                Controls.SpinBox {
                    Kirigami.FormData.label: qsTr("宽度（像素）")
                    Layout.fillWidth: true
                    visible: root.customResolution
                    from: 16
                    to: 16384
                    editable: true
                    value: settings.defaultWidth
                    onValueModified: settings.defaultWidth = value
                }

                Controls.SpinBox {
                    Kirigami.FormData.label: qsTr("高度（像素）")
                    Layout.fillWidth: true
                    visible: root.customResolution
                    from: 16
                    to: 16384
                    editable: true
                    value: settings.defaultHeight
                    onValueModified: settings.defaultHeight = value
                }

                SettingCombo {
                    Kirigami.FormData.label: qsTr("媒体后端")
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: qsTr("自动"), value: "auto" },
                        { text: qsTr("FFmpeg"), value: "ffmpeg" }
                    ]
                    settingValue: settings.mediaBackend
                    onActivated: settings.mediaBackend = currentValue
                }

                Controls.Label {
                    text: qsTr("媒体后端设置将在重启应用后生效。")
                    wrapMode: Text.Wrap
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                }

                SettingCombo {
                    Kirigami.FormData.label: qsTr("硬件解码")
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: qsTr("自动"), value: "auto" },
                        { text: qsTr("禁用"), value: "disabled" }
                    ]
                    settingValue: settings.hardwareDecode
                    onActivated: settings.hardwareDecode = currentValue
                }

                Controls.Label {
                    text: qsTr("硬件解码设置将在重启后生效；“自动”不代表当前设备一定支持硬件解码。")
                    wrapMode: Text.Wrap
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                }

                RowLayout {
                    Kirigami.FormData.label: qsTr("音频输出")
                    Layout.fillWidth: true
                    Controls.ComboBox {
                        id: audioCombo
                        Layout.fillWidth: true
                        model: root.audioDevices
                        textRole: "description"
                        valueRole: "id"
                        onActivated: settings.audioOutput = currentValue
                    }
                    Controls.Button {
                        text: qsTr("刷新设备")
                        onClicked: root.refreshAudioDevices()
                    }
                }
            }

            ProbeInfo {
                Layout.fillWidth: true
                onRetestRequested: root.retestRequested()
            }

            Item { Layout.preferredHeight: Kirigami.Units.largeSpacing }
        }
    }
}
