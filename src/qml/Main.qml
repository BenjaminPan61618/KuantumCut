import QtQuick
import QtQuick.Window
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: qsTr("KuantumCut")
    width: 1440
    height: 900
    minimumWidth: 1100
    minimumHeight: 700
    visible: false

    property int pageIndex: 0
    property bool previewFullScreen: false
    property int previousVisibility: Window.Windowed
    property string pendingAction: ""
    property bool allowClose: false
    property bool splashFinished: false
    readonly property bool darkTheme: settings.darkMode

    function startProbe() {
        probe.start(root)
    }

    function enterMain() {
        if (splashFinished)
            return
        splashFinished = true
        splashWindow.visible = false
        visible = true
        startProbe()
    }

    function reportPerformance() {
        console.info("KuantumCut 渲染检测：API=" + probe.renderApi
                     + "，渲染器=" + probe.renderer
                     + "，平均帧率=" + probe.framesPerSecond.toFixed(1) + " FPS"
                     + "，等级=" + probe.level
                     + "，分数=" + probe.score)
        if (probe.level !== "smooth" && probe.level !== "unknown")
            performanceDialog.open()
    }

    function showDrafts() {
        if (previewFullScreen)
            setPreviewFullScreen(false)
        if (pageIndex === 2)
            playback.pause()
        pageIndex = 0
        drafts.refresh()
    }

    function requestDrafts() {
        if (pageIndex !== 2 || !editor.dirty) {
            showDrafts()
            return
        }
        pendingAction = "drafts"
        dirtyDialog.open()
    }

    function openDraft(id) {
        if (editor.openDraft(id))
            pageIndex = 2
    }

    function newDraft(name, width, height) {
        if (editor.newDraft(name, width, height))
            pageIndex = 2
    }

    function finishPendingAction() {
        const action = pendingAction
        pendingAction = ""
        if (action === "drafts") {
            showDrafts()
        } else if (action === "close") {
            allowClose = true
            root.close()
        }
    }

    function setPreviewFullScreen(enter) {
        if (enter && pageIndex !== 2)
            return
        if (previewFullScreen === enter)
            return
        previewFullScreen = enter
        if (enter) {
            previousVisibility = visibility
            showFullScreen()
        } else if (previousVisibility === Window.Maximized) {
            showMaximized()
        } else {
            showNormal()
        }
    }

    onPageIndexChanged: {
        if (pageIndex !== 2 && previewFullScreen)
            setPreviewFullScreen(false)
    }

    onClosing: function(close) {
        if (editor.dirty && !allowClose) {
            close.accepted = false
            pendingAction = "close"
            dirtyDialog.open()
        }
    }

    // 启动时先展示 Splash，随后进入主界面并在主窗口上采样帧率
    Timer {
        interval: 900
        running: true
        onTriggered: root.enterMain()
    }

    // 兜底：若检测未能回调，最多 8 秒后也进入主界面
    Timer {
        interval: 8000
        running: true
        onTriggered: root.enterMain()
    }

    Connections {
        target: probe
        function onBusyChanged() {
            if (!probe.busy && root.splashFinished)
                root.reportPerformance()
        }
    }

    // 启动时的无边框 Logo 窗口，短暂展示后自动关闭
    Window {
        id: splashWindow
        width: 420
        height: 300
        visible: true
        flags: Qt.SplashScreen | Qt.FramelessWindowHint
        color: root.darkTheme ? "#18181b" : "#f4f4f5"
        x: Math.round((Screen.width - width) / 2)
        y: Math.round((Screen.height - height) / 2)

        ColumnLayout {
            anchors.centerIn: parent
            spacing: Kirigami.Units.largeSpacing

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 84
                Layout.preferredHeight: 84
                radius: 22
                gradient: Gradient {
                    GradientStop { position: 0; color: "#3b82f6" }
                    GradientStop { position: 1; color: "#6366f1" }
                }
                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: 48
                    height: 48
                    source: "video-x-generic"
                    color: "#ffffff"
                }
            }

            Controls.Label {
                text: qsTr("KuantumCut")
                font.pointSize: 18
                font.bold: true
                color: root.darkTheme ? "#f4f4f5" : "#18181b"
                Layout.alignment: Qt.AlignHCenter
            }

            Controls.Label {
                text: qsTr("正在启动…")
                color: root.darkTheme ? "#a1a1aa" : "#71717a"
                Layout.alignment: Qt.AlignHCenter
            }

            Controls.BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: splashWindow.visible
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
            }
        }
    }

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None
    pageStack.initialPage: Kirigami.Page {
        padding: 0
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

        Kirigami.Theme.inherit: false
        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        Kirigami.Theme.textColor: root.darkTheme ? "#f4f4f5" : "#18181b"
        Kirigami.Theme.disabledTextColor: root.darkTheme ? "#a1a1aa" : "#71717a"
        Kirigami.Theme.backgroundColor: root.darkTheme ? "#18181b" : "#f4f4f5"
        Kirigami.Theme.alternateBackgroundColor: root.darkTheme ? "#27272a" : "#e4e4e7"
        Kirigami.Theme.highlightColor: root.darkTheme ? "#3b82f6" : "#2563eb"
        Kirigami.Theme.highlightedTextColor: "#ffffff"

        palette.window: root.darkTheme ? "#18181b" : "#f4f4f5"
        palette.windowText: root.darkTheme ? "#f4f4f5" : "#18181b"
        palette.base: root.darkTheme ? "#111113" : "#ffffff"
        palette.alternateBase: root.darkTheme ? "#27272a" : "#e4e4e7"
        palette.text: root.darkTheme ? "#f4f4f5" : "#18181b"
        palette.button: root.darkTheme ? "#27272a" : "#e4e4e7"
        palette.buttonText: root.darkTheme ? "#f4f4f5" : "#18181b"
        palette.highlight: root.darkTheme ? "#3b82f6" : "#2563eb"
        palette.highlightedText: "#ffffff"
        palette.placeholderText: root.darkTheme ? "#a1a1aa" : "#71717a"

        StackLayout {
            anchors.fill: parent
            currentIndex: root.pageIndex

            DraftManagerPage {
                onOpenRequested: function(id) { root.openDraft(id) }
                onCreateRequested: function(name, width, height) {
                    root.newDraft(name, width, height)
                }
                onSettingsRequested: root.pageIndex = 1
            }

            SettingsPage {
                onBackRequested: root.showDrafts()
                onRetestRequested: root.startProbe()
            }

            EditorPage {
                activePage: root.pageIndex === 2
                previewFullScreen: root.previewFullScreen
                onBackRequested: root.requestDrafts()
                onFullScreenRequested: function(enter) { root.setPreviewFullScreen(enter) }
            }
        }
    }

    Controls.Dialog {
        id: performanceDialog
        title: qsTr("性能提示")
        modal: true
        anchors.centerIn: parent
        width: 480
        standardButtons: Controls.Dialog.Ok

        ColumnLayout {
            width: parent.width
            Controls.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: probe.softwareRendering
                      ? qsTr("当前系统未启用硬件加速（渲染器：%1），实时预览与多轨剪辑可能会不流畅。\n\n建议减少叠加轨道数量、缩短预览片段；可在“设置”中随时重新检测。").arg(probe.renderer)
                      : qsTr("当前系统的渲染性能一般（%1 FPS），复杂时间线在实时预览时可能卡顿。\n\n建议减少叠加轨道数量；可在“设置”中随时重新检测。").arg(probe.framesPerSecond.toFixed(1))
            }
        }
    }

    Controls.Dialog {
        id: dirtyDialog
        title: qsTr("草稿尚未保存")
        modal: true
        width: 460
        anchors.centerIn: parent
        standardButtons: Controls.Dialog.Save | Controls.Dialog.Discard | Controls.Dialog.Cancel

        ColumnLayout {
            width: parent.width
            Controls.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("当前草稿有未保存的修改。保存后继续，还是放弃这些修改？")
            }
        }

        onAccepted: {
            if (editor.save())
                root.finishPendingAction()
            else
                root.pendingAction = ""
        }
        onDiscarded: root.finishPendingAction()
        onRejected: root.pendingAction = ""
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.previewFullScreen
        onActivated: root.setPreviewFullScreen(false)
    }

    Shortcut {
        sequence: "F11"
        enabled: root.pageIndex === 2
        onActivated: root.setPreviewFullScreen(!root.previewFullScreen)
    }
}
