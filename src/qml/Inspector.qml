import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Controls.Frame {
    id: root

    padding: 0
    property var clipData: timeline.selectedClip
    property bool hasClip: clipData.id !== undefined && clipData.id.length > 0
    readonly property bool isFilterClip: hasClip && clipData.trackKind === "filter"
    readonly property bool isTextClip: hasClip && clipData.kind === "text"
    readonly property bool clipLocked: hasClip && clipData.locked === true
    property string operationError: ""
    readonly property var fontFamiliesModel: {
        const list = [{ name: "", label: qsTr("默认") }]
        const families = Qt.fontFamilies()
        for (let i = 0; i < families.length; ++i)
            list.push({ name: families[i], label: families[i] })
        return list
    }

    function colorToHex(c) {
        function hex(v) {
            const s = Math.round(Math.max(0, Math.min(1, v)) * 255).toString(16)
            return s.length < 2 ? "0" + s : s
        }
        return "#" + hex(c.a) + hex(c.r) + hex(c.g) + hex(c.b)
    }
    readonly property var colorFilterModel: {
        const names = timeline.colorFilterNames()
        const model = []
        for (let i = 0; i < names.length; ++i)
            model.push({ name: names[i], text: timeline.colorFilterDisplayName(names[i]) })
        return model
    }

    function setProperty(name, value) {
        if (!hasClip)
            return
        if (!editor.setClipProperty(clipData.id, name, value, true))
            operationError = qsTr("属性修改失败，请重新选择片段后重试。")
        else
            operationError = ""
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.hasClip ? 1 : 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: Math.min(parent.width - 2 * Kirigami.Units.largeSpacing,
                                Kirigami.Units.gridUnit * 20)
                text: qsTr("未选择片段")
                explanation: qsTr("在时间线中点击片段后，可在这里调整画面、声音和裁剪。")
            }
        }

        Controls.ScrollView {
            id: view
            contentWidth: availableWidth

            ColumnLayout {
                width: view.availableWidth
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Heading {
                    level: 3
                    text: qsTr("片段属性")
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.largeSpacing
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                }

                Controls.Label {
                    text: root.hasClip ? root.clipData.name : ""
                    elide: Text.ElideMiddle
                    font.bold: true
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                }

                Controls.Label {
                    text: root.hasClip
                          ? qsTr("轨道：%1　时长：%2 秒").arg(root.clipData.trackId).arg((root.clipData.duration / 1000).toFixed(1))
                          : ""
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                }

                Controls.CheckBox {
                    text: qsTr("锁定素材")
                    checked: root.clipLocked
                    enabled: root.hasClip
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                    onToggled: root.setProperty("locked", checked)
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing
                    wideMode: false
                    visible: !root.isFilterClip
                    enabled: !root.clipLocked

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("缩放（%）")
                        Layout.fillWidth: true
                        from: 1
                        to: 10000
                        stepSize: 1
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.scale * 100) : 100
                        onValueModified: root.setProperty("scale", value / 100)
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("X 位置（像素）")
                        Layout.fillWidth: true
                        from: -100000
                        to: 100000
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.x) : 0
                        onValueModified: root.setProperty("x", value)
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("Y 位置（像素）")
                        Layout.fillWidth: true
                        from: -100000
                        to: 100000
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.y) : 0
                        onValueModified: root.setProperty("y", value)
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("旋转（度）")
                        Layout.fillWidth: true
                        from: -3600
                        to: 3600
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.rotation) : 0
                        onValueModified: root.setProperty("rotation", value)
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("不透明度（%）")
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.opacity * 100) : 100
                        onValueModified: root.setProperty("opacity", value / 100)
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("音量（%）")
                        Layout.fillWidth: true
                        visible: !root.isTextClip
                        from: 0
                        to: 100
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.volume * 100) : 100
                        onValueModified: root.setProperty("volume", value / 100)
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("左裁剪（%）")
                        Layout.fillWidth: true
                        visible: !root.isTextClip
                        from: 0
                        to: 99
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.cropLeft * 100) : 0
                        onValueModified: root.setProperty("cropLeft", value / 100)
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("上裁剪（%）")
                        Layout.fillWidth: true
                        visible: !root.isTextClip
                        from: 0
                        to: 99
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.cropTop * 100) : 0
                        onValueModified: root.setProperty("cropTop", value / 100)
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("右裁剪（%）")
                        Layout.fillWidth: true
                        visible: !root.isTextClip
                        from: 0
                        to: 99
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.cropRight * 100) : 0
                        onValueModified: root.setProperty("cropRight", value / 100)
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("下裁剪（%）")
                        Layout.fillWidth: true
                        visible: !root.isTextClip
                        from: 0
                        to: 99
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.cropBottom * 100) : 0
                        onValueModified: root.setProperty("cropBottom", value / 100)
                    }

                    Controls.CheckBox {
                        Kirigami.FormData.label: qsTr("变换")
                        text: qsTr("左右反转")
                        checked: root.hasClip ? root.clipData.flipHorizontal : false
                        onToggled: root.setProperty("flipHorizontal", checked)
                    }

                    Controls.CheckBox {
                        text: qsTr("上下反转")
                        checked: root.hasClip ? root.clipData.flipVertical : false
                        onToggled: root.setProperty("flipVertical", checked)
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("播放速度（%）")
                        Layout.fillWidth: true
                        visible: !root.isTextClip
                        from: 25
                        to: 400
                        stepSize: 25
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.speed * 100) : 100
                        onValueModified: root.setProperty("speed", value / 100)
                    }
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing
                    wideMode: false
                    visible: root.isTextClip
                    enabled: !root.clipLocked

                    Controls.ComboBox {
                        Kirigami.FormData.label: qsTr("字体")
                        Layout.fillWidth: true
                        model: root.fontFamiliesModel
                        textRole: "label"
                        valueRole: "name"
                        currentIndex: root.hasClip
                            ? Math.max(0, root.fontFamiliesModel.findIndex(function(m) {
                                  return m.name === (root.clipData.textFont || "")
                              }))
                            : 0
                        onActivated: root.setProperty("textFont", root.fontFamiliesModel[currentIndex].name)
                    }

                    GridLayout {
                        Kirigami.FormData.label: qsTr("样式")
                        columns: 2
                        columnSpacing: Kirigami.Units.smallSpacing
                        rowSpacing: 0

                        Controls.CheckBox {
                            text: qsTr("加粗")
                            font.bold: true
                            checked: root.hasClip ? root.clipData.textBold === true : false
                            onToggled: root.setProperty("textBold", checked)
                        }
                        Controls.CheckBox {
                            text: qsTr("斜体")
                            font.italic: true
                            checked: root.hasClip ? root.clipData.textItalic === true : false
                            onToggled: root.setProperty("textItalic", checked)
                        }
                        Controls.CheckBox {
                            text: qsTr("下划线")
                            font.underline: true
                            checked: root.hasClip ? root.clipData.textUnderline === true : false
                            onToggled: root.setProperty("textUnderline", checked)
                        }
                        Controls.CheckBox {
                            text: qsTr("删除线")
                            font.strikeout: true
                            checked: root.hasClip ? root.clipData.textStrikeout === true : false
                            onToggled: root.setProperty("textStrikeout", checked)
                        }
                    }

                    RowLayout {
                        Kirigami.FormData.label: qsTr("填充颜色")
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 20
                            radius: 3
                            color: root.hasClip ? root.clipData.textFillColor : "#ffffff"
                            border.color: Kirigami.Theme.disabledTextColor
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            text: root.hasClip ? String(root.clipData.textFillColor || "").toUpperCase() : ""
                        }
                        Controls.Button {
                            text: qsTr("选择…")
                            onClicked: {
                                fillColorDialog.selectedColor = Qt.color(root.clipData.textFillColor || "#ffffff")
                                fillColorDialog.open()
                            }
                        }
                    }

                    RowLayout {
                        Kirigami.FormData.label: qsTr("描边颜色")
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 20
                            radius: 3
                            color: root.hasClip ? root.clipData.textStrokeColor : "#00000000"
                            border.color: Kirigami.Theme.disabledTextColor
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            text: root.hasClip ? String(root.clipData.textStrokeColor || "").toUpperCase() : ""
                        }
                        Controls.Button {
                            text: qsTr("选择…")
                            onClicked: {
                                strokeColorDialog.selectedColor = Qt.color(root.clipData.textStrokeColor || "#00000000")
                                strokeColorDialog.open()
                            }
                        }
                    }

                    RowLayout {
                        Kirigami.FormData.label: qsTr("阴影颜色")
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 20
                            radius: 3
                            color: root.hasClip ? root.clipData.textShadowColor : "#00000000"
                            border.color: Kirigami.Theme.disabledTextColor
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            text: root.hasClip ? String(root.clipData.textShadowColor || "").toUpperCase() : ""
                        }
                        Controls.Button {
                            text: qsTr("选择…")
                            onClicked: {
                                shadowColorDialog.selectedColor = Qt.color(root.clipData.textShadowColor || "#00000000")
                                shadowColorDialog.open()
                            }
                        }
                    }
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing
                    wideMode: false
                    visible: root.isFilterClip
                    enabled: !root.clipLocked

                    Controls.ComboBox {
                        Kirigami.FormData.label: qsTr("颜色滤镜")
                        Layout.fillWidth: true
                        model: root.colorFilterModel
                        textRole: "text"
                        valueRole: "name"
                        currentIndex: root.hasClip
                            ? Math.max(0, model.findIndex(function(m){ return m.name === root.clipData.colorFilter }))
                            : 0
                        onActivated: root.setProperty("colorFilter", model[currentIndex].name)
                    }

                    Controls.SpinBox {
                        Kirigami.FormData.label: qsTr("强度（%）")
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        stepSize: 5
                        editable: true
                        value: root.hasClip ? Math.round(root.clipData.filterStrength * 100) : 100
                        onValueModified: root.setProperty("filterStrength", value / 100)
                    }
                }

                Controls.Button {
                    text: qsTr("删除片段")
                    enabled: !root.clipLocked
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                    onClicked: {
                        if (!editor.removeClip(root.clipData.id))
                            root.operationError = qsTr("删除片段失败。")
                    }
                }

                Kirigami.InlineMessage {
                    visible: root.operationError.length > 0
                    text: root.operationError
                    type: Kirigami.MessageType.Error
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing
                }

                Item { Layout.preferredHeight: Kirigami.Units.largeSpacing }
            }
        }
    }

    Dialogs.ColorDialog {
        id: fillColorDialog
        title: qsTr("选择填充颜色")
        options: Dialogs.ColorDialog.ShowAlphaChannel
        onAccepted: root.setProperty("textFillColor", root.colorToHex(selectedColor))
    }

    Dialogs.ColorDialog {
        id: strokeColorDialog
        title: qsTr("选择描边颜色")
        options: Dialogs.ColorDialog.ShowAlphaChannel
        onAccepted: root.setProperty("textStrokeColor", root.colorToHex(selectedColor))
    }

    Dialogs.ColorDialog {
        id: shadowColorDialog
        title: qsTr("选择阴影颜色")
        options: Dialogs.ColorDialog.ShowAlphaChannel
        onAccepted: root.setProperty("textShadowColor", root.colorToHex(selectedColor))
    }
}
