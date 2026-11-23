pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtMultimedia
import org.kde.kirigami as Kirigami

Item {
    id: root

    signal fullScreenRequested(bool enter)
    property bool fullScreen: false

    function formatTime(milliseconds) {
        const totalSeconds = Math.floor(Math.max(0, milliseconds) / 1000)
        const hours = Math.floor(totalSeconds / 3600)
        const minutes = Math.floor((totalSeconds % 3600) / 60)
        const seconds = totalSeconds % 60
        return hours > 0
                ? qsTr("%1:%2:%3").arg(hours).arg(minutes.toString().padStart(2, "0")).arg(seconds.toString().padStart(2, "0"))
                : qsTr("%1:%2").arg(minutes).arg(seconds.toString().padStart(2, "0"))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: viewport
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 220
            clip: true

            Rectangle {
                anchors.fill: parent
                color: "#09090b"
            }

            Item {
                id: canvas
                width: Math.max(1, editor.canvasWidth)
                height: Math.max(1, editor.canvasHeight)
                anchors.centerIn: parent
                scale: Math.min(viewport.width / width, viewport.height / height) * 0.96
                clip: true

                Rectangle {
                    anchors.fill: parent
                    color: "#000000"
                }

                Repeater {
                    id: visualRepeater
                    model: activeVisuals

                    Item {
                        id: visualLayer
                        required property var model
                        property var sourceSink: model.videoSink
                        width: parent.width
                        height: parent.height
                        z: model.z
                        x: model.x + previewOffsetX
                        y: model.y + previewOffsetY
                        scale: model.scale * previewScaleFactor
                        rotation: model.rotation + previewRotationDelta
                        opacity: model.opacity
                        transformOrigin: Item.Center
                        transform: Scale {
                            xScale: model.flipHorizontal ? -1 : 1
                            yScale: model.flipVertical ? -1 : 1
                            origin.x: visualLayer.width / 2
                            origin.y: visualLayer.height / 2
                        }

                        property real previewOffsetX: 0
                        property real previewOffsetY: 0
                        property real previewScaleFactor: 1
                        property real previewRotationDelta: 0
                        property real moveStartX: 0
                        property real moveStartY: 0
                        property real transformCenterX: 0
                        property real transformCenterY: 0
                        property real scaleStartDistance: 1
                        property real rotateStartAngle: 0
                        readonly property real handleSize: 12 / Math.max(0.05, scale)

                        function canvasPoint(item, x, y) {
                            return canvas.mapFromItem(item, x, y)
                        }

                        function beginMove(point) {
                            timeline.selectedClipId = model.clipId
                            moveStartX = point.x
                            moveStartY = point.y
                        }

                        function updateMove(point) {
                            previewOffsetX = point.x - moveStartX
                            previewOffsetY = point.y - moveStartY
                        }

                        function commitMove() {
                            const dx = previewOffsetX
                            const dy = previewOffsetY
                            previewOffsetX = 0
                            previewOffsetY = 0
                            if (Math.abs(dx) >= 0.5 || Math.abs(dy) >= 0.5)
                                editor.setClipPosition(model.clipId, Math.round(model.x + dx),
                                                       Math.round(model.y + dy))
                        }

                        function cancelMove() {
                            previewOffsetX = 0
                            previewOffsetY = 0
                        }

                        function beginScale(point) {
                            timeline.selectedClipId = model.clipId
                            const center = canvasPoint(visualLayer, width / 2, height / 2)
                            transformCenterX = center.x
                            transformCenterY = center.y
                            scaleStartDistance = Math.max(1, Math.hypot(point.x - center.x,
                                                                        point.y - center.y))
                        }

                        function updateScale(point) {
                            const distance = Math.max(1, Math.hypot(point.x - transformCenterX,
                                                                    point.y - transformCenterY))
                            previewScaleFactor = distance / scaleStartDistance
                        }

                        function commitScale() {
                            const factor = previewScaleFactor
                            previewScaleFactor = 1
                            if (Math.abs(factor - 1) > 0.001)
                                editor.setClipProperty(model.clipId, "scale",
                                                       Math.max(0.01, model.scale * factor))
                        }

                        function beginRotate(point) {
                            timeline.selectedClipId = model.clipId
                            const center = canvasPoint(visualLayer, width / 2, height / 2)
                            transformCenterX = center.x
                            transformCenterY = center.y
                            rotateStartAngle = Math.atan2(point.y - center.y,
                                                          point.x - center.x) * 180 / Math.PI
                        }

                        function updateRotate(point) {
                            const angle = Math.atan2(point.y - transformCenterY,
                                                     point.x - transformCenterX) * 180 / Math.PI
                            previewRotationDelta = angle - rotateStartAngle
                        }

                        function commitRotate() {
                            const delta = previewRotationDelta
                            previewRotationDelta = 0
                            if (Math.abs(delta) > 0.1)
                                editor.setClipProperty(model.clipId, "rotation", model.rotation + delta)
                        }

                        function bindSink() {
                            if (sourceSink && videoOutput.videoSink)
                                playback.bindVideoSink(sourceSink, videoOutput.videoSink)
                        }

                        onSourceSinkChanged: bindSink()
                        Component.onCompleted: bindSink()

                        Item {
                            id: cropArea
                            x: canvas.width * model.cropLeft
                            y: canvas.height * model.cropTop
                            width: Math.max(0, canvas.width * (1 - model.cropLeft - model.cropRight))
                            height: Math.max(0, canvas.height * (1 - model.cropTop - model.cropBottom))
                            clip: true

                            property bool hasFilter: playback.activeColorFilter && playback.activeColorFilter.length > 0
                            property var filterParams: playback.colorFilterParams(playback.activeColorFilter || "")
                            readonly property url filterShader: "qrc:/shaders/color_filter.frag.qsb"

                            VideoOutput {
                                id: videoOutput
                                visible: model.kind === "video" && !model.missing
                                x: -cropArea.x
                                y: -cropArea.y
                                width: canvas.width
                                height: canvas.height
                                fillMode: VideoOutput.PreserveAspectFit
                                layer.enabled: cropArea.hasFilter
                                layer.effect: ShaderEffect {
                                    property real brightness: cropArea.filterParams ? cropArea.filterParams.brightness : 0
                                    property real contrast: cropArea.filterParams ? cropArea.filterParams.contrast : 1
                                    property real saturation: cropArea.filterParams ? cropArea.filterParams.saturation : 1
                                    property real m00: cropArea.filterParams ? cropArea.filterParams.m00 : 1
                                    property real m01: cropArea.filterParams ? cropArea.filterParams.m01 : 0
                                    property real m02: cropArea.filterParams ? cropArea.filterParams.m02 : 0
                                    property real m10: cropArea.filterParams ? cropArea.filterParams.m10 : 0
                                    property real m11: cropArea.filterParams ? cropArea.filterParams.m11 : 1
                                    property real m12: cropArea.filterParams ? cropArea.filterParams.m12 : 0
                                    property real m20: cropArea.filterParams ? cropArea.filterParams.m20 : 0
                                    property real m21: cropArea.filterParams ? cropArea.filterParams.m21 : 0
                                    property real m22: cropArea.filterParams ? cropArea.filterParams.m22 : 1
                                    property real keepHue: cropArea.filterParams ? cropArea.filterParams.keepHue : -1
                                    property real keepRange: cropArea.filterParams ? cropArea.filterParams.keepRange : 0
                                    property real strength: playback.activeFilterStrength
                                    fragmentShader: cropArea.filterShader
                                }
                            }

                            Image {
                                visible: model.kind === "image" && !model.missing
                                x: -cropArea.x
                                y: -cropArea.y
                                width: canvas.width
                                height: canvas.height
                                source: model.kind === "image" && !model.missing ? model.source : ""
                                asynchronous: true
                                fillMode: Image.PreserveAspectFit
                                layer.enabled: cropArea.hasFilter
                                layer.effect: ShaderEffect {
                                    property real brightness: cropArea.filterParams ? cropArea.filterParams.brightness : 0
                                    property real contrast: cropArea.filterParams ? cropArea.filterParams.contrast : 1
                                    property real saturation: cropArea.filterParams ? cropArea.filterParams.saturation : 1
                                    property real m00: cropArea.filterParams ? cropArea.filterParams.m00 : 1
                                    property real m01: cropArea.filterParams ? cropArea.filterParams.m01 : 0
                                    property real m02: cropArea.filterParams ? cropArea.filterParams.m02 : 0
                                    property real m10: cropArea.filterParams ? cropArea.filterParams.m10 : 0
                                    property real m11: cropArea.filterParams ? cropArea.filterParams.m11 : 1
                                    property real m12: cropArea.filterParams ? cropArea.filterParams.m12 : 0
                                    property real m20: cropArea.filterParams ? cropArea.filterParams.m20 : 0
                                    property real m21: cropArea.filterParams ? cropArea.filterParams.m21 : 0
                                    property real m22: cropArea.filterParams ? cropArea.filterParams.m22 : 1
                                    property real keepHue: cropArea.filterParams ? cropArea.filterParams.keepHue : -1
                                    property real keepRange: cropArea.filterParams ? cropArea.filterParams.keepRange : 0
                                    property real strength: playback.activeFilterStrength
                                    fragmentShader: cropArea.filterShader
                                }
                            }

                            Rectangle {
                                visible: model.kind === "color"
                                x: -cropArea.x
                                y: -cropArea.y
                                width: canvas.width
                                height: canvas.height
                                color: {
                                    if (!cropArea.hasFilter)
                                        return model.color.length > 0 ? model.color : "#000000"
                                    const c = Qt.color(model.color.length > 0 ? model.color : "#000000")
                                    const p = cropArea.filterParams
                                    const r = c.r, g = c.g, b = c.b
                                    const s = Math.max(0, Math.min(1, playback.activeFilterStrength))
                                    // 单色滤镜：保留指定色相，其余转为灰度
                                    if (p.keepRange > 0) {
                                        const maximum = Math.max(r, Math.max(g, b))
                                        const minimum = Math.min(r, Math.min(g, b))
                                        const chroma = maximum - minimum
                                        let hue = 0
                                        if (chroma > 0.00001) {
                                            if (maximum === r)
                                                hue = ((((g - b) / chroma) % 6) + 6) % 6 / 6
                                            else if (maximum === g)
                                                hue = ((b - r) / chroma + 2) / 6
                                            else
                                                hue = ((r - g) / chroma + 4) / 6
                                        }
                                        let hueDistance = Math.abs(hue - p.keepHue)
                                        hueDistance = Math.min(hueDistance, 1 - hueDistance)
                                        const t1 = Math.max(0, Math.min(1,
                                                (hueDistance - p.keepRange * 0.6) / (p.keepRange * 0.4)))
                                        const t2 = Math.max(0, Math.min(1,
                                                (chroma / Math.max(maximum, 0.00001) - 0.02) / 0.2))
                                        const keep = (1 - t1 * t1 * (3 - 2 * t1))
                                                   * t2 * t2 * (3 - 2 * t2)
                                        const gray = (r + g + b) / 3
                                        const kr = gray + (r - gray) * keep
                                        const kg = gray + (g - gray) * keep
                                        const kb = gray + (b - gray) * keep
                                        return Qt.rgba(r + (kr - r) * s,
                                                       g + (kg - g) * s,
                                                       b + (kb - b) * s, c.a)
                                    }
                                    const luma = 0.299*r + 0.587*g + 0.114*b
                                    const sat = p.saturation
                                    let nr = luma + (r - luma) * sat
                                    let ng = luma + (g - luma) * sat
                                    let nb = luma + (b - luma) * sat
                                    nr = (nr - 0.5) * p.contrast + 0.5 + p.brightness
                                    ng = (ng - 0.5) * p.contrast + 0.5 + p.brightness
                                    nb = (nb - 0.5) * p.contrast + 0.5 + p.brightness
                                    const or = Math.max(0, Math.min(1, p.m00*nr + p.m01*ng + p.m02*nb))
                                    const og = Math.max(0, Math.min(1, p.m10*nr + p.m11*ng + p.m12*nb))
                                    const ob = Math.max(0, Math.min(1, p.m20*nr + p.m21*ng + p.m22*nb))
                                    return Qt.rgba(r + (or - r) * s,
                                                   g + (og - g) * s,
                                                   b + (ob - b) * s, c.a)
                                }
                            }

                            readonly property bool hasTextStroke: model.kind === "text"
                                && model.textStrokeColor !== undefined
                                && model.textStrokeColor.length > 0
                                && Qt.color(model.textStrokeColor).a > 0
                            readonly property bool hasTextShadow: model.kind === "text"
                                && model.textShadowColor !== undefined
                                && model.textShadowColor.length > 0
                                && Qt.color(model.textShadowColor).a > 0

                            Text {
                                visible: cropArea.hasTextShadow
                                x: -cropArea.x + canvas.width * 0.06 + 3
                                y: -cropArea.y + canvas.height * 0.06 + 3
                                width: canvas.width * 0.88
                                height: canvas.height * 0.88
                                text: model.text
                                color: model.textShadowColor
                                font.family: model.textFont && model.textFont.length > 0
                                             ? model.textFont : Qt.application.font.family
                                font.pixelSize: Math.max(18, canvas.height * 0.07)
                                font.bold: model.textBold
                                font.italic: model.textItalic
                                font.underline: model.textUnderline
                                font.strikeout: model.textStrikeout
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.Wrap
                            }

                            Text {
                                id: textFill
                                visible: model.kind === "text"
                                x: -cropArea.x + canvas.width * 0.06
                                y: -cropArea.y + canvas.height * 0.06
                                width: canvas.width * 0.88
                                height: canvas.height * 0.88
                                text: model.text
                                color: model.textFillColor && model.textFillColor.length > 0
                                       ? model.textFillColor : "#ffffff"
                                style: cropArea.hasTextStroke ? Text.Outline : Text.Normal
                                styleColor: model.textStrokeColor
                                font.family: model.textFont && model.textFont.length > 0
                                             ? model.textFont : Qt.application.font.family
                                font.pixelSize: Math.max(18, canvas.height * 0.07)
                                font.bold: model.textBold
                                font.italic: model.textItalic
                                font.underline: model.textUnderline
                                font.strikeout: model.textStrikeout
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.Wrap
                                layer.enabled: cropArea.hasFilter
                                layer.effect: ShaderEffect {
                                    property real brightness: cropArea.filterParams ? cropArea.filterParams.brightness : 0
                                    property real contrast: cropArea.filterParams ? cropArea.filterParams.contrast : 1
                                    property real saturation: cropArea.filterParams ? cropArea.filterParams.saturation : 1
                                    property real m00: cropArea.filterParams ? cropArea.filterParams.m00 : 1
                                    property real m01: cropArea.filterParams ? cropArea.filterParams.m01 : 0
                                    property real m02: cropArea.filterParams ? cropArea.filterParams.m02 : 0
                                    property real m10: cropArea.filterParams ? cropArea.filterParams.m10 : 0
                                    property real m11: cropArea.filterParams ? cropArea.filterParams.m11 : 1
                                    property real m12: cropArea.filterParams ? cropArea.filterParams.m12 : 0
                                    property real m20: cropArea.filterParams ? cropArea.filterParams.m20 : 0
                                    property real m21: cropArea.filterParams ? cropArea.filterParams.m21 : 0
                                    property real m22: cropArea.filterParams ? cropArea.filterParams.m22 : 1
                                    property real keepHue: cropArea.filterParams ? cropArea.filterParams.keepHue : -1
                                    property real keepRange: cropArea.filterParams ? cropArea.filterParams.keepRange : 0
                                    property real strength: playback.activeFilterStrength
                                    fragmentShader: cropArea.filterShader
                                }
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: model.missing
                            color: "#66111111"
                            border.color: "#ef4444"
                            border.width: Math.max(2, canvas.width * 0.002)
                            Controls.Label {
                                anchors.centerIn: parent
                                text: qsTr("素材文件缺失")
                                color: "#fecaca"
                                font.pixelSize: Math.max(16, canvas.height * 0.04)
                            }
                        }

                        // 点击图层可见内容即选中该图层
                        MouseArea {
                            id: selectArea
                            readonly property bool textLayer: model.kind === "text"
                            visible: timeline.selectedClipId !== model.clipId
                            enabled: visible
                            z: 50
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            x: textLayer ? canvas.width * 0.06 : 0
                            y: textLayer
                               ? canvas.height * 0.06
                                 + Math.max(0, (textFill.height - textFill.contentHeight) / 2)
                               : 0
                            width: textLayer ? textFill.width : parent.width
                            height: textLayer
                                    ? Math.min(textFill.height, Math.max(1, textFill.contentHeight))
                                    : parent.height
                            onClicked: timeline.selectedClipId = model.clipId
                        }

                        Item {
                            id: selectionOverlay
                            anchors.fill: parent
                            visible: timeline.selectedClipId === model.clipId && !model.missing
                            z: 100

                            Rectangle {
                                anchors.fill: parent
                                color: "transparent"
                                border.color: "#3b82f6"
                                border.width: Math.max(1, 2 / Math.max(0.05, visualLayer.scale))
                            }

                            MouseArea {
                                id: moveArea
                                anchors.fill: parent
                                enabled: !model.locked
                                cursorShape: Qt.SizeAllCursor
                                preventStealing: true
                                onPressed: function(mouse) {
                                    visualLayer.beginMove(visualLayer.canvasPoint(moveArea, mouse.x, mouse.y))
                                }
                                onPositionChanged: function(mouse) {
                                    visualLayer.updateMove(visualLayer.canvasPoint(moveArea, mouse.x, mouse.y))
                                }
                                onReleased: visualLayer.commitMove()
                                onCanceled: visualLayer.cancelMove()
                            }

                            Repeater {
                                model: [
                                    { hx: 0, hy: 0, cursor: Qt.SizeFDiagCursor },
                                    { hx: 1, hy: 0, cursor: Qt.SizeBDiagCursor },
                                    { hx: 0, hy: 1, cursor: Qt.SizeBDiagCursor },
                                    { hx: 1, hy: 1, cursor: Qt.SizeFDiagCursor }
                                ]

                                delegate: Rectangle {
                                    id: scaleHandle
                                    required property var modelData
                                    width: visualLayer.handleSize
                                    height: visualLayer.handleSize
                                    radius: 2
                                    x: modelData.hx * (selectionOverlay.width - width)
                                    y: modelData.hy * (selectionOverlay.height - height)
                                    color: "#ffffff"
                                    border.color: "#3b82f6"
                                    border.width: 1

                                    MouseArea {
                                        id: scaleArea
                                        anchors.fill: parent
                                        enabled: !model.locked
                                        anchors.margins: -6
                                        cursorShape: scaleHandle.modelData.cursor
                                        preventStealing: true
                                        onPressed: function(mouse) {
                                            visualLayer.beginScale(visualLayer.canvasPoint(scaleArea, mouse.x, mouse.y))
                                        }
                                        onPositionChanged: function(mouse) {
                                            visualLayer.updateScale(visualLayer.canvasPoint(scaleArea, mouse.x, mouse.y))
                                        }
                                        onReleased: visualLayer.commitScale()
                                        onCanceled: visualLayer.previewScaleFactor = 1
                                    }
                                }
                            }

                            Rectangle {
                                id: rotateHandle
                                width: visualLayer.handleSize * 1.4
                                height: width
                                radius: width / 2
                                x: (selectionOverlay.width - width) / 2
                                y: visualLayer.handleSize * 0.6
                                color: "#3b82f6"
                                border.color: "#ffffff"
                                border.width: 1

                                MouseArea {
                                    id: rotateArea
                                    anchors.fill: parent
                                    enabled: !model.locked
                                    anchors.margins: -6
                                    cursorShape: Qt.CrossCursor
                                    preventStealing: true
                                    onPressed: function(mouse) {
                                        visualLayer.beginRotate(visualLayer.canvasPoint(rotateArea, mouse.x, mouse.y))
                                    }
                                    onPositionChanged: function(mouse) {
                                        visualLayer.updateRotate(visualLayer.canvasPoint(rotateArea, mouse.x, mouse.y))
                                    }
                                    onReleased: visualLayer.commitRotate()
                                    onCanceled: visualLayer.previewRotationDelta = 0
                                }
                            }
                        }
                    }
                }

                Controls.Label {
                    anchors.centerIn: parent
                    // 主轨留空时显示纯黑占位帧，仅空草稿给出提示
                    visible: visualRepeater.count === 0 && timeline.duration <= 0
                    text: qsTr("当前位置没有可视片段")
                    color: "#a1a1aa"
                    font.pixelSize: Math.max(14, canvas.height * 0.025)
                }
            }
        }

        Controls.Frame {
            Layout.fillWidth: true
            padding: Kirigami.Units.smallSpacing

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    Controls.Button {
                        icon.name: playback.playing ? "media-playback-pause" : "media-playback-start"
                        display: Controls.AbstractButton.IconOnly
                        enabled: playback.duration > 0
                        onClicked: playback.togglePlayback()
                    }
                    Controls.Button {
                        text: qsTr("后退")
                        enabled: playback.duration > 0
                        hoverEnabled: true
                        onClicked: playback.seekBackward()
                        Controls.ToolTip.text: qsTr("后退 10 秒")
                    }
                    Controls.Button {
                        text: qsTr("前进")
                        enabled: playback.duration > 0
                        hoverEnabled: true
                        onClicked: playback.seekForward()
                        Controls.ToolTip.text: qsTr("前进 10 秒")
                    }
                    Controls.Label {
                        text: qsTr("%1 / %2").arg(root.formatTime(playback.position)).arg(root.formatTime(playback.duration))
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Controls.Label { text: qsTr("主音量") }
                    Controls.Slider {
                        from: 0
                        to: 1
                        value: playback.masterVolume
                        Layout.preferredWidth: 110
                        onMoved: playback.masterVolume = value
                    }
                    Controls.Label {
                        text: qsTr("%1%").arg(Math.round(playback.masterVolume * 100))
                    }
                    Controls.Button {
                        text: root.fullScreen ? qsTr("退出全屏") : qsTr("全屏")
                        onClicked: root.fullScreenRequested(!root.fullScreen)
                    }
                }

                Controls.Slider {
                    id: positionSlider
                    Layout.fillWidth: true
                    from: 0
                    to: Math.max(0, playback.duration)
                    stepSize: 100
                    value: playback.position
                    enabled: playback.duration > 0
                    onMoved: playback.seek(value)
                }
            }
        }
    }
}
