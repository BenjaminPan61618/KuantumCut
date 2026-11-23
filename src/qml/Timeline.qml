pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: root

    property string operationError: ""
    property var clipboardClip: null
    property string dragSourceTrackId: ""
    property string dragTargetTrackId: ""
    // 拖拽片段与已有片段重叠时的处理：true 吸附到最近空位，false 自动移到新轨道
    property bool snapOnOverlap: true

    function trackLocked(trackId) {
        for (let i = 0; i < timeline.tracks.length; ++i) {
            if (timeline.tracks[i].id === trackId)
                return timeline.tracks[i].locked === true
        }
        return false
    }

    function findSnapStart(trackId, clipId, desired, duration) {
        if (duration <= 0)
            return -1
        const clips = timeline.clipsForTrack(trackId)
        const candidates = [0]
        for (let i = 0; i < clips.length; ++i) {
            if (clips[i].id === clipId)
                continue
            candidates.push(clips[i].start + clips[i].duration)
            candidates.push(clips[i].start - duration)
        }
        let best = -1
        let bestDistance = 1e18
        for (let i = 0; i < candidates.length; ++i) {
            const candidate = candidates[i]
            if (candidate < 0)
                continue
            let fits = true
            for (let j = 0; j < clips.length; ++j) {
                if (clips[j].id === clipId)
                    continue
                if (candidate < clips[j].start + clips[j].duration
                        && candidate + duration > clips[j].start) {
                    fits = false
                    break
                }
            }
            if (!fits)
                continue
            const distance = Math.abs(candidate - desired)
            if (distance < bestDistance) {
                bestDistance = distance
                best = candidate
            }
        }
        return best
    }

    function compatibleTrackIndex(index, audioClip, filterClip) {
        if (timeline.tracks.length === 0)
            return -1
        const clamped = Math.max(0, Math.min(timeline.tracks.length - 1, index))
        const matchKind = function(track) {
            if (filterClip)
                return track.kind === "filter"
            return track.kind === "audio" ? audioClip : !audioClip
        }
        if (matchKind(timeline.tracks[clamped]))
            return clamped
        let best = -1
        let bestDistance = 1e9
        for (let i = 0; i < timeline.tracks.length; ++i) {
            if (!matchKind(timeline.tracks[i]))
                continue
            const distance = Math.abs(i - clamped)
            if (distance < bestDistance) {
                bestDistance = distance
                best = i
            }
        }
        return best
    }
    readonly property int headerWidth: 92
    readonly property int rulerHeight: 32
    readonly property int trackHeight: 64
    readonly property real timeWidth: Math.max(1800, (timeline.duration + 15000) * timeline.pixelsPerSecond / 1000)
    readonly property real totalWidth: Math.max(flick.width, headerWidth + timeWidth)

    function formatTime(milliseconds) {
        const totalSeconds = Math.max(0, milliseconds) / 1000
        if (totalSeconds >= 60)
            return qsTr("%1:%2").arg(Math.floor(totalSeconds / 60)).arg(Math.floor(totalSeconds % 60).toString().padStart(2, "0"))
        return qsTr("%1 秒").arg(totalSeconds.toFixed(totalSeconds < 10 ? 1 : 0))
    }

    function tickInterval() {
        const choices = [100, 200, 500, 1000, 2000, 5000, 10000, 15000, 30000, 60000, 120000, 300000]
        for (let i = 0; i < choices.length; ++i) {
            if (choices[i] * timeline.pixelsPerSecond / 1000 >= 80)
                return choices[i]
        }
        return 600000
    }

    function moveClip(id, trackId, start) {
        const data = timeline.getClip(id)
        if (data.id === undefined) {
            operationError = qsTr("移动失败：片段不存在。")
            return
        }
        if (data.locked || trackLocked(data.trackId) || trackLocked(trackId)) {
            operationError = qsTr("轨道或片段已锁定，请先解锁后再移动。")
            return
        }
        if (editor.moveClip(id, trackId, start)) {
            operationError = ""
            return
        }
        if (snapOnOverlap) {
            const snapped = findSnapStart(trackId, id, start, data.duration)
            if (snapped >= 0 && editor.moveClip(id, trackId, snapped)) {
                operationError = ""
                return
            }
        } else {
            const newTrackId = data.trackKind === "audio" ? editor.addAudioTrack() : editor.addTrack()
            if (newTrackId.length > 0 && editor.moveClip(id, newTrackId, start)) {
                operationError = ""
                return
            }
        }
        operationError = qsTr("移动失败：目标位置可能与其他片段重叠。")
    }

    function trimClip(id, start, sourceIn, duration) {
        if (editor.trimClip(id, start, sourceIn, duration)) {
            operationError = ""
            return
        }
        const data = timeline.getClip(id)
        if (data.id !== undefined && (data.locked || trackLocked(data.trackId)))
            operationError = qsTr("轨道或片段已锁定，请先解锁后再调整时长。")
        else
            operationError = qsTr("无法调整时长：目标区间可能与其他片段重叠。")
    }

    // Ctrl+滚轮缩放时间线，以鼠标所指时间为锚点
    function handleZoomWheel(wheel, point) {
        if (!(wheel.modifiers & Qt.ControlModifier)) {
            wheel.accepted = false
            return
        }
        const oldScale = timeline.pixelsPerSecond
        const factor = wheel.angleDelta.y > 0 ? 1.15 : 1 / 1.15
        const newScale = Math.max(10, Math.min(500, oldScale * factor))
        wheel.accepted = true
        if (newScale === oldScale)
            return
        const timeAtCursor = Math.max(0, (flick.contentX + point.x - root.headerWidth) / oldScale)
        timeline.pixelsPerSecond = newScale
        Qt.callLater(function() {
            flick.contentX = Math.max(0, root.headerWidth + timeAtCursor * newScale - point.x)
        })
    }

    function copyClip(id) {
        const data = timeline.getClip(id)
        if (data.id === undefined)
            return
        clipboardClip = data
        operationError = ""
    }

    function cutClip(id) {
        const data = timeline.getClip(id)
        if (data.id === undefined)
            return
        clipboardClip = data
        if (!editor.removeClip(id))
            operationError = qsTr("剪切失败：片段不存在。")
        else
            operationError = ""
    }

    function pasteClip() {
        if (!clipboardClip || clipboardClip.id === undefined) {
            operationError = qsTr("剪贴板为空。")
            return
        }
        const sourceKind = clipboardClip.trackKind
        const start = Math.max(0, Math.round(playback.position / 100) * 100)
        const candidates = []
        if (clipboardClip.trackId)
            candidates.push(clipboardClip.trackId)
        for (let i = 0; i < timeline.tracks.length; ++i) {
            const track = timeline.tracks[i]
            if (candidates.indexOf(track.id) >= 0)
                continue
            const sameKind = sourceKind === "audio" ? track.kind === "audio"
                           : sourceKind === "filter" ? track.kind === "filter"
                           : (track.kind !== "audio" && track.kind !== "filter")
            if (sameKind)
                candidates.push(track.id)
        }
        for (let i = 0; i < candidates.length; ++i) {
            if (editor.insertClip(clipboardClip, candidates[i], start).length > 0) {
                operationError = ""
                return
            }
        }
        operationError = qsTr("粘贴失败：播放头位置可能与现有片段重叠。")
    }

    function razorSplit() {
        const id = timeline.selectedClipId
        if (id.length === 0) {
            operationError = qsTr("请先在时间轴上选中一个片段。")
            return
        }
        if (editor.splitClip(id, playback.position).length === 0)
            operationError = qsTr("无法拆分：播放头需要位于所选片段内部。")
        else
            operationError = ""
    }

    function razorTrim(keepLeft) {
        const id = timeline.selectedClipId
        if (id.length === 0) {
            operationError = qsTr("请先在时间轴上选中一个片段。")
            return
        }
        if (!editor.trimAt(id, playback.position, keepLeft))
            operationError = qsTr("无法裁剪：播放头需要位于所选片段内部。")
        else
            operationError = ""
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Controls.ToolBar {
            Layout.fillWidth: true
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Controls.ToolButton {
                    id: addTrackButton
                    icon.name: "list-add"
                    icon.width: 20
                    icon.height: 20
                    text: qsTr("添加轨道")
                    display: Controls.AbstractButton.IconOnly
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: text
                    onClicked: addTrackMenu.open()
                    Controls.Menu {
                        id: addTrackMenu
                        y: parent.height
                        Controls.MenuItem {
                            text: qsTr("添加叠加轨")
                            onTriggered: editor.addTrack()
                        }
                        Controls.MenuItem {
                            text: qsTr("添加滤镜轨")
                            onTriggered: editor.addFilterTrack()
                        }
                    }
                }
                Controls.ToolButton {
                    icon.name: "edit-cut"
                    text: qsTr("拆分")
                    display: Controls.AbstractButton.TextBesideIcon
                    onClicked: root.razorSplit()
                }
                Controls.ToolButton {
                    text: qsTr("保留左侧")
                    onClicked: root.razorTrim(true)
                }
                Controls.ToolButton {
                    text: qsTr("保留右侧")
                    onClicked: root.razorTrim(false)
                }
                Controls.ToolButton {
                    text: root.snapOnOverlap ? qsTr("重叠：吸附") : qsTr("重叠：新轨道")
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: qsTr("拖拽片段与已有片段重叠时的处理方式，点击切换")
                    onClicked: root.snapOnOverlap = !root.snapOnOverlap
                }
                Controls.ToolButton {
                    text: qsTr("−")
                    onClicked: timeline.pixelsPerSecond = Math.max(10, timeline.pixelsPerSecond / 1.25)
                }
                Controls.Slider {
                    from: 10
                    to: 500
                    value: timeline.pixelsPerSecond
                    Layout.preferredWidth: 180
                    onMoved: timeline.pixelsPerSecond = value
                }
                Controls.ToolButton {
                    text: qsTr("+")
                    onClicked: timeline.pixelsPerSecond = Math.min(500, timeline.pixelsPerSecond * 1.25)
                }
                Controls.Label {
                    text: qsTr("%1 像素/秒").arg(Math.round(timeline.pixelsPerSecond))
                }
                Item { Layout.fillWidth: true }
                Controls.Label {
                    text: qsTr("Space 播放/暂停；←/→ ±10 秒；上下拖动切换轨道")
                    elide: Text.ElideRight
                    Layout.maximumWidth: 360
                }
            }
        }

        Controls.Label {
            Layout.fillWidth: true
            visible: root.operationError.length > 0
            text: root.operationError
            color: Kirigami.Theme.negativeTextColor
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            leftPadding: 8
            rightPadding: 8
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: root.totalWidth
            contentHeight: Math.max(height, timelineContent.implicitHeight)
            boundsBehavior: Flickable.StopAtBounds
            focus: true

            Keys.onDeletePressed: {
                if (timeline.selectedClipId.length > 0) {
                    editor.removeClip(timeline.selectedClipId)
                    accepted = true
                }
            }

            Keys.onPressed: function(event) {
                if (!(event.modifiers & Qt.ControlModifier))
                    return
                if (event.key === Qt.Key_C) {
                    root.copyClip(timeline.selectedClipId)
                    event.accepted = true
                } else if (event.key === Qt.Key_X) {
                    root.cutClip(timeline.selectedClipId)
                    event.accepted = true
                } else if (event.key === Qt.Key_V) {
                    root.pasteClip()
                    event.accepted = true
                }
            }

            Item {
                id: timelineContent
                width: root.totalWidth
                implicitHeight: root.rulerHeight + timeline.tracks.length * root.trackHeight
                height: Math.max(flick.height, implicitHeight)

                // 放在内容项内，确保 wheel.x/y 与内容坐标一致，空白区域也能触发缩放
                WheelHandler {
                    acceptedModifiers: Qt.ControlModifier
                    onWheel: function(wheel) {
                        root.handleZoomWheel(wheel,
                                             Qt.point(wheel.x - flick.contentX,
                                                      wheel.y - flick.contentY))
                    }
                }

                Rectangle {
                    x: root.headerWidth
                    width: root.timeWidth
                    height: root.rulerHeight
                    color: Kirigami.Theme.alternateBackgroundColor
                }

                Repeater {
                    model: Math.ceil(root.timeWidth / (root.tickInterval() * timeline.pixelsPerSecond / 1000)) + 1
                    delegate: Item {
                        required property int index
                        readonly property real tickPixels: root.tickInterval() * timeline.pixelsPerSecond / 1000
                        x: root.headerWidth + index * tickPixels
                        width: 1
                        height: root.rulerHeight
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: 1
                            height: index % 2 === 0 ? 12 : 7
                            color: Kirigami.Theme.disabledTextColor
                        }
                        Controls.Label {
                            anchors.left: parent.left
                            anchors.leftMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.formatTime(index * root.tickInterval())
                            font.pixelSize: 10
                        }
                    }
                }

                MouseArea {
                    x: root.headerWidth
                    y: 0
                    width: root.timeWidth
                    height: root.rulerHeight
                    onWheel: function(wheel) {
                        root.handleZoomWheel(wheel, mapToItem(flick, wheel.x, wheel.y))
                    }
                    onClicked: function(mouse) {
                        flick.forceActiveFocus()
                        playback.seek(mouse.x * 1000 / timeline.pixelsPerSecond)
                    }
                }

                DropArea {
                    id: dropArea
                    x: root.headerWidth
                    y: root.rulerHeight
                    width: root.timeWidth
                    height: Math.max(root.trackHeight * (timeline.tracks.length + 1),
                                     timelineContent.height - root.rulerHeight)
                    keys: ["text/plain"]
                    z: 50

                    property int hoverTrack: -1

                    function trackIndexAt(yPos) {
                        return Math.floor(yPos / root.trackHeight)
                    }

                    function trackAt(yPos) {
                        const index = trackIndexAt(yPos)
                        if (index < 0 || index >= timeline.tracks.length)
                            return null
                        return timeline.tracks[index]
                    }

                    onPositionChanged: function(drag) {
                        const index = dropArea.trackIndexAt(drag.y)
                        dropArea.hoverTrack = index >= 0 && index < timeline.tracks.length ? index : -1
                    }
                    onExited: dropArea.hoverTrack = -1
                    onDropped: function(drop) {
                        dropArea.hoverTrack = -1
                        if (!drop.hasText)
                            return
                        const asset = assets.get(drop.text)
                        if (asset.id === undefined || asset.missing) {
                            root.operationError = qsTr("素材不存在或文件缺失，请先重新定位。")
                            return
                        }
                        const audioAsset = asset.kind === "audio"
                        const filterAsset = asset.kind === "filter"
                        let track = dropArea.trackAt(drop.y)
                        const typeMismatch = filterAsset ? track.kind !== "filter"
                                            : (track.kind === "audio" ? !audioAsset : audioAsset)
                        if (!track || typeMismatch) {
                            // 拖到现有轨道之外或类型不匹配时，自动新建对应类型的轨道
                            const id = filterAsset ? editor.addFilterTrack()
                                       : audioAsset ? editor.addAudioTrack()
                                       : editor.addTrack()
                            for (let i = 0; id.length > 0 && i < timeline.tracks.length; ++i) {
                                if (timeline.tracks[i].id === id) {
                                    track = timeline.tracks[i]
                                    break
                                }
                            }
                        }
                        if (!track) {
                            root.operationError = qsTr("无法新建轨道。")
                            return
                        }
                        if (track.locked) {
                            root.operationError = qsTr("目标轨道已锁定，无法添加素材。")
                            return
                        }
                        let start = Math.max(0, Math.round(drop.x * 1000 / timeline.pixelsPerSecond / 100) * 100)
                        if (root.snapOnOverlap) {
                            const snapped = root.findSnapStart(track.id, "", start, asset.duration)
                            if (snapped >= 0)
                                start = snapped
                        }
                        if (editor.addClip(drop.text, track.id, start).length === 0)
                            root.operationError = qsTr("无法添加片段：目标位置可能与现有片段重叠。")
                        else
                            root.operationError = ""
                    }
                }

                Repeater {
                    id: trackRepeater
                    model: timeline.tracks

                    Item {
                        id: trackRoot
                        required property var modelData
                        required property int index
                        property var trackData: modelData
                        property string trackId: trackData.id
                        property var clipList: []
                        x: root.headerWidth
                        y: root.rulerHeight + index * root.trackHeight
                        width: root.timeWidth
                        height: root.trackHeight

                        function refreshClips() {
                            clipList = timeline.clipsForTrack(trackId)
                        }

                        Component.onCompleted: refreshClips()
                        Connections {
                            target: timeline
                            function onTimelineChanged() { trackRoot.refreshClips() }
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: {
                                const base = index % 2 === 0 ? Kirigami.Theme.backgroundColor : Kirigami.Theme.alternateBackgroundColor
                                return trackRoot.trackData.locked ? Qt.darker(base, 1.12) : base
                            }
                            border.color: Kirigami.Theme.disabledTextColor
                            border.width: 0
                        }

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 2
                            radius: 4
                            z: 3
                            color: Kirigami.Theme.highlightColor
                            opacity: 0.2
                            visible: dropArea.hoverTrack === index
                        }

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 1
                            radius: 4
                            z: 3
                            color: "transparent"
                            border.color: Kirigami.Theme.highlightColor
                            border.width: 2
                            visible: root.dragTargetTrackId.length > 0
                                     && root.dragTargetTrackId !== root.dragSourceTrackId
                                     && root.dragTargetTrackId === trackRoot.trackId
                        }

                        Repeater {
                            model: trackRoot.clipList

                            Rectangle {
                                id: clipRect
                                required property var modelData
                                property var clipData: modelData
                                property string clipId: clipData.id
                                property string gesture: ""
                                property string targetTrackId: ""
                                property real pressSceneX: 0
                                property real gestureDelta: 0
                                property real originalStart: 0
                                property real originalSourceIn: 0
                                property real originalDuration: 0
                                readonly property real baseX: clipData.start * timeline.pixelsPerSecond / 1000
                                readonly property real baseWidth: clipData.duration * timeline.pixelsPerSecond / 1000
                                x: baseX + ((gesture === "move" || gesture === "left") ? gestureDelta : 0)
                                y: 7
                                width: Math.max(6, baseWidth + (gesture === "right" ? gestureDelta : gesture === "left" ? -gestureDelta : 0))
                                height: root.trackHeight - 14
                                radius: 4
                                z: timeline.selectedClipId === clipId ? 4 : 2
                                opacity: (clipData.locked || trackRoot.trackData.locked) ? 0.5 : 1
                                color: timeline.selectedClipId === clipId
                                       ? Kirigami.Theme.highlightColor
                                       : clipData.trackKind === "audio" ? "#0f766e"
                                       : clipData.trackKind === "filter" ? "#7c3aed" : "#52525b"
                                border.color: timeline.selectedClipId === clipId ? "#ffffff" : "#a1a1aa"
                                border.width: timeline.selectedClipId === clipId ? 2 : 1

                                function beginGesture(mode, sceneX) {
                                    timeline.selectedClipId = clipId
                                    flick.forceActiveFocus()
                                    if (trackRoot.trackData.locked) {
                                        root.operationError = qsTr("轨道已锁定，请先解锁后再编辑。")
                                        return
                                    }
                                    if (clipData.locked)
                                        return
                                    gesture = mode
                                    pressSceneX = sceneX
                                    gestureDelta = 0
                                    originalStart = clipData.start
                                    originalSourceIn = clipData.sourceIn
                                    originalDuration = clipData.duration
                                    targetTrackId = clipData.trackId
                                    root.dragSourceTrackId = clipData.trackId
                                    root.dragTargetTrackId = ""
                                }

                                function updateGesture(sceneX) {
                                    if (gesture.length === 0)
                                        return
                                    let milliseconds = (sceneX - pressSceneX) * 1000 / timeline.pixelsPerSecond
                                    if (gesture === "move") {
                                        milliseconds = Math.max(-originalStart, milliseconds)
                                    } else if (gesture === "left") {
                                        const sourceInLimit = originalSourceIn / Math.max(0.01, clipData.speed)
                                        milliseconds = Math.max(-Math.min(originalStart, sourceInLimit),
                                                               Math.min(originalDuration - 100, milliseconds))
                                    } else {
                                        milliseconds = Math.max(100 - originalDuration, milliseconds)
                                    }
                                    gestureDelta = milliseconds * timeline.pixelsPerSecond / 1000
                                }

                                function finishGesture() {
                                    if (gesture.length === 0)
                                        return
                                    const mode = gesture
                                    const delta = Math.round(gestureDelta * 1000 / timeline.pixelsPerSecond / 100) * 100
                                    const target = targetTrackId.length > 0 ? targetTrackId : clipData.trackId
                                    gesture = ""
                                    gestureDelta = 0
                                    targetTrackId = ""
                                    root.dragSourceTrackId = ""
                                    root.dragTargetTrackId = ""
                                    if (mode === "move") {
                                        const start = Math.max(0, originalStart + delta)
                                        if (start !== originalStart || target !== clipData.trackId)
                                            root.moveClip(clipId, target, start)
                                        trackRoot.refreshClips()
                                        return
                                    }
                                    if (delta === 0)
                                        return
                                    if (mode === "left") {
                                        root.trimClip(clipId, originalStart + delta,
                                                      originalSourceIn + Math.round(delta * clipData.speed),
                                                      originalDuration - delta)
                                    } else {
                                        root.trimClip(clipId, originalStart, originalSourceIn,
                                                      originalDuration + delta)
                                    }
                                    trackRoot.refreshClips()
                                }

                                function cancelGesture() {
                                    gesture = ""
                                    gestureDelta = 0
                                    targetTrackId = ""
                                    root.dragSourceTrackId = ""
                                    root.dragTargetTrackId = ""
                                }

                                Controls.Label {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: clipData.locked ? 26 : 10
                                    text: clipData.name
                                    color: "#ffffff"
                                    elide: Text.ElideMiddle
                                    verticalAlignment: Text.AlignVCenter
                                }

                                Kirigami.Icon {
                                    visible: clipData.locked === true
                                    source: "object-locked-symbolic"
                                    width: 14
                                    height: 14
                                    anchors.right: parent.right
                                    anchors.rightMargin: 6
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: "#ffffff"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: (clipData.locked || trackRoot.trackData.locked) ? Qt.ArrowCursor : Qt.SizeAllCursor
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    preventStealing: true
                                    onWheel: function(wheel) {
                                        root.handleZoomWheel(wheel, mapToItem(flick, wheel.x, wheel.y))
                                    }
                                    onPressed: function(mouse) {
                                        if (mouse.button === Qt.LeftButton)
                                            clipRect.beginGesture("move", mapToItem(trackRoot, mouse.x, mouse.y).x)
                                        else
                                            timeline.selectedClipId = clipId
                                    }
                                    onPositionChanged: function(mouse) {
                                        if (!(mouse.buttons & Qt.LeftButton))
                                            return
                                        clipRect.updateGesture(mapToItem(trackRoot, mouse.x, mouse.y).x)
                                        if (clipRect.gesture !== "move")
                                            return
                                        const point = mapToItem(timelineContent, mouse.x, mouse.y)
                                        const rawIndex = Math.floor((point.y - root.rulerHeight) / root.trackHeight)
                                        const index = root.compatibleTrackIndex(
                                                          rawIndex,
                                                          clipRect.clipData.trackKind === "audio",
                                                          clipRect.clipData.trackKind === "filter")
                                        if (index >= 0) {
                                            clipRect.targetTrackId = timeline.tracks[index].id
                                            root.dragTargetTrackId = clipRect.targetTrackId
                                        }
                                    }
                                    onReleased: clipRect.finishGesture()
                                    onCanceled: clipRect.cancelGesture()
                                    onClicked: function(mouse) {
                                        if (mouse.button !== Qt.RightButton)
                                            return
                                        const data = timeline.getClip(clipId)
                                        clipMenu.targetId = clipId
                                        clipMenu.hasAudio = data.kind === "video" || data.kind === "audio"
                                        clipMenu.isVideo = data.kind === "video"
                                        clipMenu.muted = data.volume <= 0.001
                                        clipMenu.detached = data.audioDetached === true
                                        clipMenu.reversed = data.reverse === true
                                        clipMenu.flippedH = data.flipHorizontal === true
                                        clipMenu.flippedV = data.flipVertical === true
                                        clipMenu.locked = data.locked === true
                                        clipMenu.parent = clipRect
                                        clipMenu.x = mouse.x
                                        clipMenu.y = mouse.y
                                        clipMenu.open()
                                    }
                                }

                                MouseArea {
                                    anchors.left: parent.left
                                    width: 9
                                    height: parent.height
                                    z: 5
                                    enabled: !clipData.locked && !trackRoot.trackData.locked
                                    cursorShape: Qt.SizeHorCursor
                                    preventStealing: true
                                    onWheel: function(wheel) {
                                        root.handleZoomWheel(wheel, mapToItem(flick, wheel.x, wheel.y))
                                    }
                                    onPressed: function(mouse) {
                                        clipRect.beginGesture("left", mapToItem(trackRoot, mouse.x, mouse.y).x)
                                    }
                                    onPositionChanged: function(mouse) {
                                        clipRect.updateGesture(mapToItem(trackRoot, mouse.x, mouse.y).x)
                                    }
                                    onReleased: clipRect.finishGesture()
                                    onCanceled: clipRect.cancelGesture()
                                }

                                MouseArea {
                                    anchors.right: parent.right
                                    width: 9
                                    height: parent.height
                                    z: 5
                                    enabled: !clipData.locked && !trackRoot.trackData.locked
                                    cursorShape: Qt.SizeHorCursor
                                    preventStealing: true
                                    onWheel: function(wheel) {
                                        root.handleZoomWheel(wheel, mapToItem(flick, wheel.x, wheel.y))
                                    }
                                    onPressed: function(mouse) {
                                        clipRect.beginGesture("right", mapToItem(trackRoot, mouse.x, mouse.y).x)
                                    }
                                    onPositionChanged: function(mouse) {
                                        clipRect.updateGesture(mapToItem(trackRoot, mouse.x, mouse.y).x)
                                    }
                                    onReleased: clipRect.finishGesture()
                                    onCanceled: clipRect.cancelGesture()
                                }
                            }
                        }
                    }
                }

                Repeater {
                    model: timeline.tracks
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        x: flick.contentX
                        y: root.rulerHeight + index * root.trackHeight
                        width: root.headerWidth
                        height: root.trackHeight
                        z: 30
                        color: modelData.locked
                               ? Qt.darker(Kirigami.Theme.alternateBackgroundColor, 1.18)
                               : modelData.muted
                               ? Qt.darker(Kirigami.Theme.alternateBackgroundColor, 1.08)
                               : Kirigami.Theme.alternateBackgroundColor
                        border.color: Kirigami.Theme.disabledTextColor
                        border.width: 0

                        Controls.Label {
                            anchors.top: parent.top
                            anchors.topMargin: 5
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 8
                            horizontalAlignment: Text.AlignHCenter
                            text: modelData.name
                            font.bold: true
                            elide: Text.ElideRight
                            color: modelData.locked ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onWheel: function(wheel) {
                                root.handleZoomWheel(wheel, mapToItem(flick, wheel.x, wheel.y))
                            }
                            onClicked: trackMenu.openFor(modelData)
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 3
                            spacing: 0

                            Controls.ToolButton {
                                width: 24
                                height: 24
                                padding: 0
                                display: Controls.AbstractButton.IconOnly
                                icon.name: modelData.muted
                                           ? "audio-volume-muted-symbolic"
                                           : "audio-volume-high-symbolic"
                                icon.width: 14
                                icon.height: 14
                                Controls.ToolTip.visible: hovered
                                Controls.ToolTip.text: modelData.muted
                                                        ? qsTr("取消静音轨道")
                                                        : qsTr("静音轨道")
                                onClicked: editor.setTrackMuted(modelData.id, !modelData.muted)
                            }

                            Controls.ToolButton {
                                width: 24
                                height: 24
                                padding: 0
                                display: Controls.AbstractButton.IconOnly
                                icon.name: modelData.locked
                                           ? "object-locked-symbolic"
                                           : "object-unlocked-symbolic"
                                icon.width: 14
                                icon.height: 14
                                Controls.ToolTip.visible: hovered
                                Controls.ToolTip.text: modelData.locked
                                                        ? qsTr("解锁轨道")
                                                        : qsTr("锁定轨道")
                                onClicked: editor.setTrackLocked(modelData.id, !modelData.locked)
                            }

                            Controls.ToolButton {
                                width: 24
                                height: 24
                                padding: 0
                                display: Controls.AbstractButton.IconOnly
                                icon.name: "overflow-menu"
                                icon.width: 14
                                icon.height: 14
                                Controls.ToolTip.visible: hovered
                                Controls.ToolTip.text: qsTr("轨道操作")
                                onClicked: trackMenu.openFor(modelData)
                            }
                        }
                    }
                }

                Rectangle {
                    x: flick.contentX
                    y: 0
                    width: root.headerWidth
                    height: root.rulerHeight
                    z: 31
                    color: Kirigami.Theme.alternateBackgroundColor
                    Controls.Label {
                        anchors.centerIn: parent
                        text: qsTr("轨道")
                        font.bold: true
                    }
                }

                Rectangle {
                    id: playhead
                    x: root.headerWidth + playback.position * timeline.pixelsPerSecond / 1000 - 1
                    y: 0
                    width: 2
                    height: timelineContent.height
                    z: 40
                    color: "#ef4444"

                    MouseArea {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 14
                        height: parent.height
                        cursorShape: Qt.SizeHorCursor
                        preventStealing: true
                        onWheel: function(wheel) {
                            root.handleZoomWheel(wheel, mapToItem(flick, wheel.x, wheel.y))
                        }
                        onPressed: flick.forceActiveFocus()
                        onPositionChanged: function(mouse) {
                            const point = mapToItem(timelineContent, mouse.x, mouse.y)
                            playback.seek((point.x - root.headerWidth) * 1000 / timeline.pixelsPerSecond)
                        }
                    }
                }
            }
        }
    }

    Controls.Menu {
        id: clipMenu
        property string targetId: ""
        property bool hasAudio: false
        property bool isVideo: false
        property bool muted: false
        property bool detached: false
        property bool reversed: false
        property bool flippedH: false
        property bool flippedV: false
        property bool locked: false

        Controls.MenuItem {
            text: qsTr("剪切") + "  (Ctrl+X)"
            enabled: clipMenu.targetId.length > 0 && !clipMenu.locked
            onTriggered: root.cutClip(clipMenu.targetId)
        }
        Controls.MenuItem {
            text: qsTr("复制") + "  (Ctrl+C)"
            enabled: clipMenu.targetId.length > 0
            onTriggered: root.copyClip(clipMenu.targetId)
        }
        Controls.MenuItem {
            text: qsTr("粘贴") + "  (Ctrl+V)"
            enabled: root.clipboardClip !== null
            onTriggered: root.pasteClip()
        }
        Controls.MenuSeparator {}
        Controls.MenuItem {
            text: clipMenu.locked ? qsTr("解锁素材") : qsTr("锁定素材")
            checkable: true
            checked: clipMenu.locked
            enabled: clipMenu.targetId.length > 0
            onTriggered: editor.setClipProperty(clipMenu.targetId, "locked", !clipMenu.locked)
        }
        Controls.MenuSeparator {}
        Controls.MenuItem {
            text: qsTr("删除") + "  (Del)"
            enabled: clipMenu.targetId.length > 0 && !clipMenu.locked
            onTriggered: editor.removeClip(clipMenu.targetId)
        }
        Controls.MenuSeparator {}
        Controls.MenuItem {
            text: clipMenu.muted ? qsTr("取消静音") : qsTr("静音")
            visible: clipMenu.hasAudio
            enabled: clipMenu.targetId.length > 0 && !clipMenu.locked
            onTriggered: editor.setClipProperty(clipMenu.targetId, "volume",
                                                clipMenu.muted ? 1.0 : 0.0)
        }
        Controls.MenuItem {
            text: qsTr("分离音频")
            visible: clipMenu.isVideo && !clipMenu.detached
            enabled: clipMenu.targetId.length > 0 && !clipMenu.locked
            onTriggered: {
                if (editor.detachAudio(clipMenu.targetId).length === 0)
                    root.operationError = qsTr("分离音频失败：音频轨可能没有空位。")
                else
                    root.operationError = ""
            }
        }
        Controls.MenuItem {
            text: qsTr("恢复音频")
            visible: clipMenu.detached
            enabled: clipMenu.targetId.length > 0 && !clipMenu.locked
            onTriggered: {
                if (!editor.reattachAudio(clipMenu.targetId))
                    root.operationError = qsTr("恢复音频失败。")
                else
                    root.operationError = ""
            }
        }
        Controls.MenuSeparator {}
        Controls.MenuItem {
            text: qsTr("左右反转")
            checkable: true
            checked: clipMenu.flippedH
            enabled: clipMenu.targetId.length > 0 && !clipMenu.locked
            onTriggered: editor.setClipProperty(clipMenu.targetId, "flipHorizontal", !clipMenu.flippedH)
        }
        Controls.MenuItem {
            text: qsTr("上下反转")
            checkable: true
            checked: clipMenu.flippedV
            enabled: clipMenu.targetId.length > 0 && !clipMenu.locked
            onTriggered: editor.setClipProperty(clipMenu.targetId, "flipVertical", !clipMenu.flippedV)
        }
        Controls.MenuItem {
            text: qsTr("倒放")
            checkable: true
            checked: clipMenu.reversed
            enabled: clipMenu.targetId.length > 0 && clipMenu.isVideo && !editor.reverseProcessing
                      && !clipMenu.locked
            onTriggered: editor.toggleReverse(clipMenu.targetId)
        }
    }

    Controls.Menu {
        id: trackMenu
        property string trackId: ""
        property string trackName: ""
        property bool muted: false
        property bool locked: false
        property int clipCount: 0
        property bool removable: false

        function openFor(track) {
            if (!track)
                return
            trackId = track.id
            trackName = track.name
            muted = track.muted === true
            locked = track.locked === true
            clipCount = track.clipCount || 0
            removable = track.id !== "V1" && track.id !== "A1"
            popup()
        }

        Controls.MenuItem {
            text: qsTr("静音轨道")
            checkable: true
            checked: trackMenu.muted
            onTriggered: editor.setTrackMuted(trackMenu.trackId, !trackMenu.muted)
        }
        Controls.MenuItem {
            text: qsTr("锁定轨道")
            checkable: true
            checked: trackMenu.locked
            onTriggered: editor.setTrackLocked(trackMenu.trackId, !trackMenu.locked)
        }
        Controls.MenuSeparator {}
        Controls.MenuItem {
            text: qsTr("删除轨道内所有素材")
            enabled: trackMenu.clipCount > 0 && !trackMenu.locked
            onTriggered: {
                if (!editor.clearTrack(trackMenu.trackId))
                    root.operationError = qsTr("无法清空轨道素材。")
                else
                    root.operationError = ""
            }
        }
        Controls.MenuItem {
            text: qsTr("删除空轨道")
            enabled: trackMenu.removable && trackMenu.clipCount === 0 && !trackMenu.locked
            onTriggered: {
                if (!editor.removeTrack(trackMenu.trackId))
                    root.operationError = qsTr("只能删除空轨道。")
                else
                    root.operationError = ""
            }
        }
    }
}
