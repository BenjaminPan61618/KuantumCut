pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: root

    signal openRequested(string id)
    signal createRequested(string name, int width, int height)
    signal settingsRequested()

    property string renameDraftId: ""
    property string deleteDraftId: ""
    property string deleteDraftName: ""
    property var selectedIds: []

    readonly property bool allSelected: drafts.count > 0 && selectedIds.length >= drafts.count

    function isSelected(id) {
        return selectedIds.indexOf(id) >= 0
    }

    function toggleSelection(id) {
        const next = selectedIds.slice()
        const position = next.indexOf(id)
        if (position >= 0)
            next.splice(position, 1)
        else
            next.push(id)
        selectedIds = next
    }

    function toggleSelectAll() {
        selectedIds = allSelected ? [] : drafts.draftIds()
    }

    function deleteSelected() {
        if (selectedIds.length === 0)
            return
        drafts.deleteDrafts(selectedIds)
        selectedIds = []
    }

    function openRename(id, name) {
        renameDraftId = id
        renameField.text = name
        renameDialog.open()
    }

    function openDelete(id, name) {
        deleteDraftId = id
        deleteDraftName = name
        deleteDialog.open()
    }

    Component.onCompleted: drafts.refresh()
    onVisibleChanged: {
        if (visible)
            drafts.refresh()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            Kirigami.Heading {
                text: qsTr("草稿管理")
                level: 1
                Layout.fillWidth: true
            }
            Controls.Button {
                text: root.allSelected ? qsTr("取消全选") : qsTr("全选")
                enabled: drafts.count > 0
                onClicked: root.toggleSelectAll()
            }
            Controls.Button {
                text: qsTr("删除选中（%1）").arg(root.selectedIds.length)
                enabled: root.selectedIds.length > 0
                onClicked: bulkDeleteDialog.open()
            }
            Controls.Button {
                text: qsTr("刷新")
                onClicked: drafts.refresh()
            }
            Controls.Button {
                text: qsTr("设置")
                onClicked: root.settingsRequested()
            }
            Controls.Button {
                text: qsTr("新建草稿")
                highlighted: true
                onClicked: newDialog.open()
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: drafts.lastError.length > 0
            type: Kirigami.MessageType.Error
            text: drafts.lastError
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: editor.lastError.length > 0
            type: Kirigami.MessageType.Error
            text: editor.lastError
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: draftList
                anchors.fill: parent
                clip: true
                model: drafts
                spacing: Kirigami.Units.smallSpacing

                delegate: Controls.Frame {
                    required property var model
                    width: ListView.view.width
                    padding: Kirigami.Units.largeSpacing

                    RowLayout {
                        width: parent.width
                        spacing: Kirigami.Units.largeSpacing

                        Item {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            Layout.alignment: Qt.AlignVCenter

                            Rectangle {
                                anchors.centerIn: parent
                                width: 18
                                height: 18
                                radius: 4
                                color: root.isSelected(model.id) ? Kirigami.Theme.highlightColor
                                                                 : Kirigami.Theme.backgroundColor
                                border.width: 1
                                border.color: root.isSelected(model.id) ? Kirigami.Theme.highlightColor
                                                                        : Kirigami.Theme.disabledTextColor

                                Kirigami.Icon {
                                    anchors.centerIn: parent
                                    width: 12
                                    height: 12
                                    visible: root.isSelected(model.id)
                                    source: "checkmark"
                                    color: Kirigami.Theme.highlightedTextColor
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.toggleSelection(model.id)
                            }
                        }

                        Item {
                            Layout.preferredWidth: 112
                            Layout.preferredHeight: 72
                            Layout.alignment: Qt.AlignVCenter

                            Rectangle {
                                anchors.fill: parent
                                radius: 4
                                color: Kirigami.Theme.alternateBackgroundColor
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    visible: model.thumbnailKind !== "color"
                                             && model.thumbnailSource.toString().length > 0
                                    source: visible ? model.thumbnailSource : ""
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    visible: model.thumbnailKind === "color"
                                    color: model.thumbnailColor.length > 0 ? model.thumbnailColor : "#000000"
                                }

                                Kirigami.Icon {
                                    anchors.centerIn: parent
                                    width: 32
                                    height: 32
                                    visible: model.thumbnailKind !== "color"
                                             && model.thumbnailSource.toString().length === 0
                                    source: model.thumbnailKind === "video" ? "video-x-generic"
                                            : model.thumbnailKind === "image" ? "image-x-generic"
                                            : "text-x-generic"
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Kirigami.Heading {
                                level: 3
                                text: model.name
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Controls.Label {
                                text: qsTr("修改于 %1").arg(Qt.formatDateTime(model.modified, "yyyy-MM-dd HH:mm"))
                            }
                            Controls.Label {
                                text: qsTr("分辨率：%1 × %2").arg(model.resolution.width).arg(model.resolution.height)
                            }
                        }

                        Controls.Button {
                            text: qsTr("打开")
                            highlighted: true
                            onClicked: root.openRequested(model.id)
                        }
                        Controls.Button {
                            text: qsTr("重命名")
                            onClicked: root.openRename(model.id, model.name)
                        }
                        Controls.Button {
                            text: qsTr("删除")
                            onClicked: root.openDelete(model.id, model.name)
                        }
                    }

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onDoubleTapped: root.openRequested(model.id)
                    }
                }
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: Math.min(parent.width - 32, 480)
                visible: drafts.count === 0
                text: qsTr("还没有草稿")
                explanation: qsTr("新建一个草稿，即可导入素材并开始剪辑。")
                helpfulAction: Kirigami.Action {
                    text: qsTr("新建草稿")
                    onTriggered: newDialog.open()
                }
            }
        }
    }

    Controls.Dialog {
        id: newDialog
        title: qsTr("新建草稿")
        modal: true
        width: 480
        parent: Controls.Overlay.overlay
        anchors.centerIn: parent
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel

        onOpened: {
            nameField.text = ""
            widthSpin.value = settings.defaultWidth
            heightSpin.value = settings.defaultHeight
            presetCombo.currentIndex = 0
            standardButton(Controls.Dialog.Ok).enabled = false
            nameField.forceActiveFocus()
        }
        onAccepted: root.createRequested(nameField.text.trim(), widthSpin.value, heightSpin.value)

        ColumnLayout {
            width: parent.width
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.smallSpacing

                Controls.Label { text: qsTr("名称") }
                Controls.TextField {
                    id: nameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("输入草稿名称")
                    onTextChanged: newDialog.standardButton(Controls.Dialog.Ok).enabled = text.trim().length > 0
                }

                Controls.Label { text: qsTr("常见预设") }
                Controls.ComboBox {
                    id: presetCombo
                    Layout.fillWidth: true
                    textRole: "text"
                    model: [
                        { text: qsTr("自定义"), width: 0, height: 0 },
                        { text: qsTr("1920 × 1080"), width: 1920, height: 1080 },
                        { text: qsTr("1280 × 720"), width: 1280, height: 720 },
                        { text: qsTr("3840 × 2160"), width: 3840, height: 2160 },
                        { text: qsTr("1080 × 1920"), width: 1080, height: 1920 },
                        { text: qsTr("1080 × 1080"), width: 1080, height: 1080 }
                    ]
                    onActivated: function(index) {
                        const preset = model[index]
                        if (preset.width > 0) {
                            widthSpin.value = preset.width
                            heightSpin.value = preset.height
                        }
                    }
                }

                Controls.Label { text: qsTr("宽度（像素）") }
                Controls.SpinBox {
                    id: widthSpin
                    from: 16
                    to: 16384
                    editable: true
                    Layout.fillWidth: true
                }

                Controls.Label { text: qsTr("高度（像素）") }
                Controls.SpinBox {
                    id: heightSpin
                    from: 16
                    to: 16384
                    editable: true
                    Layout.fillWidth: true
                }
            }
        }
    }

    Controls.Dialog {
        id: renameDialog
        title: qsTr("重命名草稿")
        modal: true
        width: 440
        parent: Controls.Overlay.overlay
        anchors.centerIn: parent
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        onOpened: {
            standardButton(Controls.Dialog.Ok).enabled = renameField.text.trim().length > 0
            renameField.forceActiveFocus()
            renameField.selectAll()
        }
        onAccepted: {
            if (drafts.rename(root.renameDraftId, renameField.text.trim()))
                drafts.refresh()
        }

        ColumnLayout {
            width: parent.width
            Controls.TextField {
                id: renameField
                Layout.fillWidth: true
                placeholderText: qsTr("草稿名称")
                onTextChanged: renameDialog.standardButton(Controls.Dialog.Ok).enabled = text.trim().length > 0
            }
        }
    }

    Controls.Dialog {
        id: bulkDeleteDialog
        title: qsTr("批量删除草稿")
        modal: true
        width: 460
        parent: Controls.Overlay.overlay
        anchors.centerIn: parent
        standardButtons: Controls.Dialog.Yes | Controls.Dialog.Cancel
        onAccepted: root.deleteSelected()

        ColumnLayout {
            width: parent.width
            Controls.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("确定永久删除选中的 %1 个草稿吗？此操作无法撤销。").arg(root.selectedIds.length)
            }
        }
    }

    Controls.Dialog {
        id: deleteDialog
        title: qsTr("删除草稿")
        modal: true
        width: 440
        parent: Controls.Overlay.overlay
        anchors.centerIn: parent
        standardButtons: Controls.Dialog.Yes | Controls.Dialog.Cancel
        onAccepted: drafts.deleteDraft(root.deleteDraftId)

        ColumnLayout {
            width: parent.width
            Controls.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("确定永久删除“%1”吗？此操作无法撤销。").arg(root.deleteDraftName)
            }
        }
    }
}
