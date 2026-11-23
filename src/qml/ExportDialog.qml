import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: root

    modal: true
    title: qsTr("导出视频")
    width: 560
    parent: Overlay.overlay
    anchors.centerIn: parent

    property bool succeeded: false
    property string pendingOverwritePath: ""

    function pathFromUrl(url) {
        let text = url.toString()
        if (text.startsWith("file://"))
            text = decodeURIComponent(text.substring(7))
        return text
    }

    onOpened: {
        // 重新打开对话框时，恢复已有状态：正在渲染则显示进度，已完成则显示结果
        if (exporter.running) {
            succeeded = false
        } else if (exporter.outputPath.length > 0) {
            succeeded = true
        } else {
            succeeded = false
            if (outputField.text.length === 0)
                outputField.text = exporter.suggestFileName()
        }
    }

    onClosed: {
        // 关闭对话框后重置结果状态（渲染中的任务不受影响，仍在后台运行）
        if (!exporter.running) {
            succeeded = false
            exporter.clearResult()
        }
    }

    Connections {
        target: exporter
        function onFinished(success) {
            root.succeeded = success
        }
        function onOverwriteConfirmationRequested(path) {
            root.pendingOverwritePath = path
            overwriteDialog.open()
        }
    }

    Dialog {
        id: overwriteDialog
        title: qsTr("文件已存在")
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Yes | Dialog.No

        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("输出文件已存在：\n%1\n是否覆盖？").arg(root.pendingOverwritePath)
            }
        }

        onAccepted: {
            if (root.pendingOverwritePath.length > 0) {
                const path = root.pendingOverwritePath
                root.pendingOverwritePath = ""
                exporter.frameRate = Number(fpsCombo.currentText)
                exporter.videoBitrate = videoCombo.currentValue
                exporter.audioBitrate = audioCombo.currentValue
                exporter.start(path, true)
            }
        }
        onRejected: root.pendingOverwritePath = ""
    }

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing

            Label {
                text: qsTr("输出文件")
            }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: outputField
                    Layout.fillWidth: true
                    enabled: !exporter.running
                    placeholderText: qsTr("保存位置")
                }
                Button {
                    text: qsTr("浏览…")
                    enabled: !exporter.running
                    onClicked: saveDialog.open()
                }
            }

            Label {
                text: qsTr("帧率")
            }
            ComboBox {
                id: fpsCombo
                Layout.fillWidth: true
                model: ["24", "25", "30", "50", "60"]
                currentIndex: 2
                enabled: !exporter.running
            }

            Label {
                text: qsTr("输出格式")
            }
            ComboBox {
                id: formatCombo
                Layout.fillWidth: true
                model: ["MP4", "MOV", "AVI", "WMV"]
                currentIndex: 0
                enabled: !exporter.running
                onActivated: {
                    const ext = "." + model[currentIndex].toLowerCase()
                    let text = outputField.text
                    const dot = text.lastIndexOf(".")
                    if (dot > text.lastIndexOf("/") && dot > text.lastIndexOf("\\"))
                        text = text.substring(0, dot)
                    outputField.text = text + ext
                }
            }

            Label {
                text: qsTr("视频比特率")
            }
            ComboBox {
                id: videoCombo
                Layout.fillWidth: true
                textRole: "label"
                valueRole: "value"
                currentIndex: 3
                enabled: !exporter.running
                model: [
                    { label: qsTr("2000 kbps（轻量）"), value: 2000 },
                    { label: qsTr("4000 kbps"), value: 4000 },
                    { label: qsTr("6000 kbps"), value: 6000 },
                    { label: qsTr("8000 kbps（推荐）"), value: 8000 },
                    { label: qsTr("12000 kbps（高清）"), value: 12000 },
                    { label: qsTr("16000 kbps"), value: 16000 },
                    { label: qsTr("20000 kbps（高码率）"), value: 20000 }
                ]
            }

            Label {
                text: qsTr("音频质量")
            }
            ComboBox {
                id: audioCombo
                Layout.fillWidth: true
                textRole: "label"
                valueRole: "value"
                currentIndex: 2
                enabled: !exporter.running
                model: [
                    { label: qsTr("96 kbps（低）"), value: 96 },
                    { label: qsTr("128 kbps"), value: 128 },
                    { label: qsTr("192 kbps（推荐）"), value: 192 },
                    { label: qsTr("256 kbps"), value: 256 },
                    { label: qsTr("320 kbps（高）"), value: 320 }
                ]
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 1
            value: exporter.progress
            visible: exporter.running || root.succeeded
        }

        Label {
            Layout.fillWidth: true
            visible: exporter.error.length > 0
            text: exporter.error
            color: Kirigami.Theme.negativeTextColor
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            visible: root.succeeded
            text: qsTr("导出完成：%1").arg(exporter.outputPath)
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            // 导出中：后台渲染（关闭对话框，继续导出）
            Button {
                text: qsTr("后台渲染")
                visible: exporter.running
                onClicked: root.close()
            }

            // 导出中：取消
            Button {
                text: qsTr("取消")
                visible: exporter.running
                onClicked: exporter.cancel()
            }

            // 导出完成：打开文件夹
            Button {
                text: qsTr("打开文件夹")
                visible: root.succeeded
                onClicked: exporter.openOutputFolder()
            }

            // 导出完成：打开文件
            Button {
                text: qsTr("打开文件")
                visible: root.succeeded
                onClicked: exporter.openOutputFile()
            }

            // 空闲：开始导出
            Button {
                text: qsTr("开始导出")
                visible: !exporter.running && !root.succeeded
                onClicked: {
                    root.succeeded = false
                    exporter.frameRate = Number(fpsCombo.currentText)
                    exporter.videoBitrate = videoCombo.currentValue
                    exporter.audioBitrate = audioCombo.currentValue
                    exporter.start(outputField.text)
                }
            }

            // 空闲 / 完成：关闭
            Button {
                text: qsTr("关闭")
                visible: !exporter.running
                onClicked: root.close()
            }
        }
    }

    Dialogs.FileDialog {
        id: saveDialog
        title: qsTr("导出到")
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: [qsTr("MP4 视频 (*.mp4)"), qsTr("MOV 视频 (*.mov)"),
                      qsTr("AVI 视频 (*.avi)"), qsTr("WMV 视频 (*.wmv)")]
        defaultSuffix: "mp4"
        onAccepted: outputField.text = root.pathFromUrl(selectedFile)
    }
}
