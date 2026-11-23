import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami

Item {
    id: root

    signal backRequested()
    signal fullScreenRequested(bool enter)

    property bool previewFullScreen: false
    property bool activePage: false

    // 文本框获得焦点时不抢占 Space / 方向键
    readonly property bool textEditing: {
        const item = Window.activeFocusItem
        return item !== null && item !== undefined
                && (item instanceof TextInput || item instanceof TextEdit
                    || item instanceof TextField || item instanceof TextArea
                    || item instanceof SpinBox)
    }

    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ToolBar {
            id: editorToolbar
            Layout.fillWidth: true
            visible: !root.previewFullScreen

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                ToolButton {
                    icon.name: "go-previous"
                    icon.width: 20
                    icon.height: 20
                    text: qsTr("返回")
                    display: AbstractButton.TextBesideIcon
                    onClicked: root.backRequested()
                }
                ToolButton {
                    icon.name: "edit-undo"
                    icon.width: 20
                    icon.height: 20
                    text: qsTr("撤销")
                    display: AbstractButton.TextBesideIcon
                    enabled: editor.canUndo
                    onClicked: editor.undo()
                }
                ToolButton {
                    icon.name: "edit-redo"
                    icon.width: 20
                    icon.height: 20
                    text: qsTr("恢复")
                    display: AbstractButton.TextBesideIcon
                    enabled: editor.canRedo
                    onClicked: editor.redo()
                }
                ToolButton {
                    icon.name: "document-save"
                    icon.width: 20
                    icon.height: 20
                    text: qsTr("保存")
                    display: AbstractButton.TextBesideIcon
                    enabled: editor.dirty
                    onClicked: editor.save()
                }
                ToolButton {
                    icon.name: "document-export"
                    icon.width: 20
                    icon.height: 20
                    text: qsTr("导出")
                    display: AbstractButton.TextBesideIcon
                    visible: !exporter.running
                    enabled: editor.currentDraftPath.length > 0
                    onClicked: exportDialog.open()
                }
                ToolButton {
                    icon.name: "system-run"
                    icon.width: 20
                    icon.height: 20
                    text: qsTr("渲染中 %1%").arg(Math.round(exporter.progress * 100))
                    display: AbstractButton.TextBesideIcon
                    visible: exporter.running
                    onClicked: exportDialog.open()
                }
                Kirigami.Heading {
                    level: 3
                    text: editor.currentDraftName + (editor.dirty ? qsTr("（未保存）") : "")
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                }
            }
        }

        ColumnLayout {
            id: statusColumn
            Layout.fillWidth: true
            Layout.maximumHeight: implicitHeight
            visible: !root.previewFullScreen
            spacing: 0

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: editor.lastError.length > 0
                type: Kirigami.MessageType.Error
                text: editor.lastError
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: assets.lastError.length > 0
                type: Kirigami.MessageType.Error
                text: assets.lastError
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: playback.statusText.length > 0
                type: Kirigami.MessageType.Information
                text: playback.statusText
            }
        }

        SplitView {
            id: verticalSplit
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            SplitView {
                id: horizontalSplit
                SplitView.fillHeight: true
                SplitView.minimumHeight: 300

                AssetPanel {
                    visible: !root.previewFullScreen
                    SplitView.preferredWidth: 300
                    SplitView.minimumWidth: 240
                    SplitView.maximumWidth: 440
                }

                PreviewPane {
                    id: preview
                    fullScreen: root.previewFullScreen
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 400
                    onFullScreenRequested: function(enter) { root.fullScreenRequested(enter) }
                }

                Inspector {
                    visible: !root.previewFullScreen
                    SplitView.preferredWidth: 300
                    SplitView.minimumWidth: 260
                    SplitView.maximumWidth: 420
                }
            }

            Timeline {
                visible: !root.previewFullScreen
                SplitView.preferredHeight: 250
                SplitView.minimumHeight: 190
                SplitView.maximumHeight: 430
            }
        }
    }

    ExportDialog {
        id: exportDialog
    }

    Dialog {
        id: reverseDialog
        modal: true
        title: qsTr("处理中")
        parent: Overlay.overlay
        anchors.centerIn: parent
        visible: editor.reverseProcessing
        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.NoButton

        ColumnLayout {
            spacing: Kirigami.Units.mediumSpacing
            RowLayout {
                spacing: Kirigami.Units.mediumSpacing
                BusyIndicator { running: true }
                Label {
                    text: editor.reverseStatusText
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+S"
        enabled: root.activePage
        onActivated: editor.save()
    }
    Shortcut {
        sequence: "Ctrl+Z"
        enabled: root.activePage && editor.canUndo
        onActivated: editor.undo()
    }
    Shortcut {
        sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]
        enabled: root.activePage && editor.canRedo
        onActivated: editor.redo()
    }
    Shortcut {
        sequences: ["Ctrl+=", "Ctrl++"]
        enabled: root.activePage
        onActivated: timeline.pixelsPerSecond = Math.min(500, timeline.pixelsPerSecond * 1.25)
    }
    Shortcut {
        sequence: "Ctrl+-"
        enabled: root.activePage
        onActivated: timeline.pixelsPerSecond = Math.max(10, timeline.pixelsPerSecond / 1.25)
    }
    Shortcut {
        sequence: "Ctrl+0"
        enabled: root.activePage
        onActivated: timeline.pixelsPerSecond = 100
    }
    Shortcut {
        sequence: "Space"
        enabled: root.activePage && !root.textEditing
        onActivated: playback.togglePlayback()
    }
    Shortcut {
        sequence: "Left"
        enabled: root.activePage && !root.textEditing
        onActivated: playback.seekBackward()
    }
    Shortcut {
        sequence: "Right"
        enabled: root.activePage && !root.textEditing
        onActivated: playback.seekForward()
    }
}
