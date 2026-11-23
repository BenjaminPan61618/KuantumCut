pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import QtMultimedia
import org.kde.kirigami as Kirigami

Controls.Frame {
    id: root

    padding: 0
    property string selectedAssetId: ""
    property string selectedAssetKind: ""
    property string localError: ""
    property string relocateAssetId: ""
    property string relocateKind: ""
    property var pendingFiles: []
    property url previewUrl: ""
    property string previewKind: ""

    function fileKind(url) {
        const path = url.toString().toLowerCase().split("?")[0]
        const imageExt = [".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif"]
        const audioExt = [".mp3", ".wav", ".flac", ".ogg", ".m4a", ".aac", ".opus"]
        for (let i = 0; i < imageExt.length; ++i) {
            if (path.endsWith(imageExt[i]))
                return "image"
        }
        for (let i = 0; i < audioExt.length; ++i) {
            if (path.endsWith(audioExt[i]))
                return "audio"
        }
        return "video"
    }

    function fileName(url) {
        const parts = url.toString().split("/")
        return decodeURIComponent(parts[parts.length - 1])
    }

    function previewFile(url) {
        previewUrl = url
        previewKind = fileKind(url)
        previewPlayer.stop()
        if (previewKind === "image") {
            previewPlayer.source = ""
        } else {
            previewPlayer.source = url
            previewPlayer.play()
        }
    }

    function commitImport() {
        for (let i = 0; i < pendingFiles.length; ++i)
            assets.importMedia(pendingFiles[i])
        assets.refreshMissing()
        pendingFiles = []
    }

    function kindLabel(kind) {
        if (kind === "image")
            return qsTr("图片")
        if (kind === "video")
            return qsTr("视频")
        if (kind === "audio")
            return qsTr("音频")
        if (kind === "color")
            return qsTr("纯色")
        if (kind === "filter")
            return qsTr("滤镜")
        return qsTr("文字")
    }

    function delegateVisible(asset) {
        if (libraryTabs.currentIndex === 1)
            return asset.kind === "color" || asset.kind === "filter"
        if (libraryTabs.currentIndex === 2)
            return asset.favorite === true
        if (asset.kind === "color" || asset.kind === "filter")
            return false
        return filterCombo.currentValue === "all" || filterCombo.currentValue === asset.kind
    }

    function formatDuration(milliseconds) {
        if (milliseconds <= 0)
            return qsTr("时长未知")
        const seconds = Math.round(milliseconds / 1000)
        const hours = Math.floor(seconds / 3600)
        const minutes = Math.floor((seconds % 3600) / 60)
        const remainder = seconds % 60
        return hours > 0
                ? qsTr("%1:%2:%3").arg(hours).arg(minutes.toString().padStart(2, "0")).arg(remainder.toString().padStart(2, "0"))
                : qsTr("%1:%2").arg(minutes).arg(remainder.toString().padStart(2, "0"))
    }

    function compatible(track, kind) {
        if (kind === "filter")
            return track.kind === "filter"
        return track.kind === "audio" ? kind === "audio" : kind !== "audio"
    }

    function selectAsset(id, kind) {
        selectedAssetId = id
        selectedAssetKind = kind
    }

    function addAsset(id, kind) {
        localError = ""
        const asset = assets.get(id)
        if (asset.id === undefined || asset.missing) {
            localError = qsTr("素材不存在或文件缺失，请先重新定位。")
            return
        }
        let trackId = ""
        for (let i = 0; i < timeline.tracks.length; ++i) {
            if (compatible(timeline.tracks[i], kind)) {
                trackId = timeline.tracks[i].id
                break
            }
        }
        if (trackId.length === 0)
            trackId = kind === "audio" ? editor.addAudioTrack()
                      : kind === "filter" ? editor.addFilterTrack()
                      : editor.addTrack()
        if (trackId.length === 0) {
            localError = qsTr("没有适合该素材的轨道。")
            return
        }
        const clipId = editor.addClip(id, trackId, playback.position)
        if (clipId.length === 0)
            localError = qsTr("无法添加片段：目标位置可能与现有片段重叠。")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        Controls.TabBar {
            id: libraryTabs
            Layout.fillWidth: true
            Controls.TabButton { text: qsTr("素材") }
            Controls.TabButton { text: qsTr("预设") }
            Controls.TabButton { text: qsTr("收藏") }
        }

        RowLayout {
            Layout.fillWidth: true
            Controls.Button {
                text: qsTr("导入")
                Layout.fillWidth: true
                onClicked: importDialog.open()
            }
            Controls.Button {
                text: qsTr("添加文字")
                Layout.fillWidth: true
                onClicked: textDialog.open()
            }
        }

        Controls.ComboBox {
            id: filterCombo
            Layout.fillWidth: true
            visible: libraryTabs.currentIndex === 0
            textRole: "text"
            valueRole: "value"
            model: [
                { text: qsTr("全部素材"), value: "all" },
                { text: qsTr("图片"), value: "image" },
                { text: qsTr("视频"), value: "video" },
                { text: qsTr("音频"), value: "audio" },
                { text: qsTr("文字"), value: "text" }
            ]
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: assetList
                anchors.fill: parent
                clip: true
                model: assets
                spacing: 4

                delegate: Controls.ItemDelegate {
                    id: delegateItem
                    required property var model
                    width: ListView.view.width
                    height: visible ? 86 : 0
                    visible: root.delegateVisible(model)
                    highlighted: root.selectedAssetId === model.id
                    onClicked: root.selectAsset(model.id, model.kind)
                    onDoubleClicked: root.addAsset(model.id, model.kind)

                    Drag.active: dragArea.drag.active
                    Drag.dragType: Drag.Automatic
                    Drag.mimeData: ({ "text/plain": model.id })
                    Drag.keys: ["text/plain"]
                    Drag.hotSpot: Qt.point(width / 2, height / 2)

                    Item {
                        id: dragOrigin
                        width: 1
                        height: 1
                        visible: false
                    }

                    MouseArea {
                        id: dragArea
                        anchors.fill: parent
                        enabled: !delegateItem.model.missing
                        cursorShape: Qt.OpenHandCursor
                        drag.target: dragOrigin
                        drag.axis: Drag.XAndYAxis
                        onReleased: {
                            if (!drag.active)
                                root.selectAsset(delegateItem.model.id, delegateItem.model.kind)
                        }
                        onDoubleClicked: root.addAsset(delegateItem.model.id, delegateItem.model.kind)
                    }

                    contentItem: ColumnLayout {
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            Rectangle {
                                visible: model.kind === "color"
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                                color: model.color
                                border.color: Kirigami.Theme.disabledTextColor
                                radius: 2
                            }
                            Controls.Label {
                                text: model.name
                                elide: Text.ElideMiddle
                                font.bold: true
                                Layout.fillWidth: true
                            }
                            Controls.Label {
                                text: root.kindLabel(model.kind)
                                font.pixelSize: 11
                            }
                            Controls.ToolButton {
                                text: model.favorite ? "★" : "☆"
                                font.pixelSize: 16
                                padding: 2
                                onClicked: assets.toggleFavorite(delegateItem.model.id)
                            }
                        }

                        Controls.Label {
                            text: model.missing ? qsTr("文件缺失")
                                                : model.kind === "color" ? qsTr("纯色背景")
                                                                         : root.formatDuration(model.duration)
                            color: model.missing ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor
                            Layout.fillWidth: true
                        }

                        Controls.Button {
                            visible: model.missing
                            text: qsTr("重新定位文件")
                            Layout.fillWidth: true
                            onClicked: {
                                root.relocateAssetId = model.id
                                root.relocateKind = model.kind
                                relocateDialog.open()
                            }
                        }
                    }
                }
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - 16
                visible: assets.count === 0
                text: qsTr("素材库为空")
                explanation: qsTr("导入图片、视频或音频，也可以添加文字。")
            }
        }

        Controls.Button {
            text: qsTr("添加到播放头位置")
            enabled: root.selectedAssetId.length > 0 && timeline.tracks.length > 0
            Layout.fillWidth: true
            onClicked: root.addAsset(root.selectedAssetId, root.selectedAssetKind)
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.localError.length > 0
            type: Kirigami.MessageType.Error
            text: root.localError
        }
    }

    Dialogs.FileDialog {
        id: importDialog
        title: qsTr("导入媒体")
        fileMode: Dialogs.FileDialog.OpenFiles
        nameFilters: [
            qsTr("所有支持的媒体 (*.png *.jpg *.jpeg *.webp *.bmp *.gif *.mp4 *.mov *.mkv *.webm *.avi *.m4v *.mp3 *.wav *.flac *.ogg *.m4a *.aac *.opus)"),
            qsTr("图片 (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"),
            qsTr("视频 (*.mp4 *.mov *.mkv *.webm *.avi *.m4v)"),
            qsTr("音频 (*.mp3 *.wav *.flac *.ogg *.m4a *.aac *.opus)")
        ]
        onAccepted: {
            root.pendingFiles = selectedFiles
            previewDialog.open()
        }
    }

    MediaPlayer {
        id: previewPlayer
        videoOutput: previewOutput
        audioOutput: AudioOutput {
            id: previewAudio
        }
    }

    Controls.Dialog {
        id: previewDialog
        title: qsTr("预览并导入（%1 个文件）").arg(root.pendingFiles.length)
        modal: true
        width: 820
        parent: Controls.Overlay.overlay
        anchors.centerIn: parent
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        onOpened: if (root.pendingFiles.length > 0) root.previewFile(root.pendingFiles[0])
        onClosed: previewPlayer.stop()
        onAccepted: root.commitImport()
        onRejected: {
            previewPlayer.stop()
            root.pendingFiles = []
        }

        ColumnLayout {
            width: parent.width
            spacing: Kirigami.Units.largeSpacing

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                spacing: Kirigami.Units.largeSpacing

                ListView {
                    Layout.preferredWidth: 240
                    Layout.fillHeight: true
                    clip: true
                    model: root.pendingFiles

                    delegate: Controls.ItemDelegate {
                        required property var modelData
                        width: ListView.view.width
                        text: root.fileName(modelData)
                        highlighted: root.previewUrl.toString() === modelData.toString()
                        onClicked: root.previewFile(modelData)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#101014"
                    radius: 4

                    VideoOutput {
                        id: previewOutput
                        anchors.fill: parent
                        anchors.margins: 4
                        visible: root.previewKind === "video"
                        fillMode: VideoOutput.PreserveAspectFit
                    }

                    Image {
                        anchors.fill: parent
                        anchors.margins: 4
                        visible: root.previewKind === "image"
                        source: root.previewKind === "image" ? root.previewUrl : ""
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        visible: root.previewKind === "audio"
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            Layout.alignment: Qt.AlignHCenter
                            width: 48
                            height: 48
                            source: "audio-x-generic"
                            color: "#f4f4f5"
                        }
                        Controls.Label {
                            Layout.alignment: Qt.AlignHCenter
                            color: "#f4f4f5"
                            text: root.fileName(root.previewUrl)
                        }
                    }

                    Controls.Label {
                        anchors.centerIn: parent
                        color: "#f4f4f5"
                        visible: root.pendingFiles.length === 0
                        text: qsTr("没有待导入的文件")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Controls.Button {
                    icon.name: previewPlayer.playbackState === MediaPlayer.PlayingState
                               ? "media-playback-pause" : "media-playback-start"
                    display: Controls.AbstractButton.IconOnly
                    enabled: root.previewKind !== "image" && root.previewUrl.toString().length > 0
                    onClicked: previewPlayer.playbackState === MediaPlayer.PlayingState
                               ? previewPlayer.pause() : previewPlayer.play()
                }
                Controls.Label {
                    text: root.formatDuration(previewPlayer.position) + " / "
                          + root.formatDuration(previewPlayer.duration)
                    visible: root.previewKind !== "image"
                }
                Item { Layout.fillWidth: true }
                Controls.Label {
                    text: qsTr("点击左侧文件可切换预览")
                    opacity: 0.7
                }
            }
        }
    }

    Dialogs.FileDialog {
        id: relocateDialog
        title: qsTr("重新定位素材")
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: root.relocateKind === "image"
                     ? [qsTr("图片 (*.png *.jpg *.jpeg *.webp *.bmp *.gif)")]
                     : root.relocateKind === "video"
                       ? [qsTr("视频 (*.mp4 *.mov *.mkv *.webm *.avi *.m4v)")]
                       : [qsTr("音频 (*.mp3 *.wav *.flac *.ogg *.m4a *.aac *.opus)")]
        onAccepted: assets.relocate(root.relocateAssetId, selectedFile)
    }

    Controls.Dialog {
        id: textDialog
        title: qsTr("添加文字素材")
        modal: true
        width: 460
        parent: Controls.Overlay.overlay
        anchors.centerIn: parent
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        onOpened: {
            textInput.text = ""
            standardButton(Controls.Dialog.Ok).enabled = false
            textInput.forceActiveFocus()
        }
        onAccepted: assets.addText(textInput.text)

        ColumnLayout {
            width: parent.width
            Controls.TextArea {
                id: textInput
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                wrapMode: TextEdit.Wrap
                placeholderText: qsTr("输入要显示的文字")
                onTextChanged: textDialog.standardButton(Controls.Dialog.Ok).enabled = text.trim().length > 0
            }
        }
    }

    Connections {
        target: editor
        function onCurrentDraftChanged() {
            root.selectedAssetId = ""
            root.selectedAssetKind = ""
            root.localError = ""
        }
    }
}
