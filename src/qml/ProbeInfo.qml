import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Controls.Frame {
    id: root

    signal retestRequested()

    property bool expanded: true

    function levelText() {
        if (probe.level === "smooth")
            return qsTr("流畅")
        if (probe.level === "basic")
            return qsTr("较卡顿")
        if (probe.level === "compatibility")
            return qsTr("卡顿")
        return qsTr("尚未检测")
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        // 可点击的标题行：箭头旋转指示展开/收起状态
        RowLayout {
            id: header
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: "go-next-symbolic"
                width: 16
                height: 16
                rotation: root.expanded ? 90 : 0
                Behavior on rotation { NumberAnimation { duration: 150 } }
            }

            Controls.Label {
                Layout.fillWidth: true
                text: probe.busy ? qsTr("系统渲染检测（检测中…）")
                                 : qsTr("系统渲染检测（%1）").arg(root.levelText())
                font.bold: true
                wrapMode: Text.Wrap
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.expanded
            spacing: Kirigami.Units.smallSpacing

            Behavior on implicitHeight {
                NumberAnimation { duration: 150 }
            }

            GridLayout {
                columns: 2
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.smallSpacing
                Layout.fillWidth: true

                Controls.Label { text: qsTr("等级") }
                Controls.Label {
                    text: qsTr("%1（%2 分）").arg(root.levelText()).arg(probe.score)
                    font.bold: true
                    Layout.fillWidth: true
                }

                Controls.Label { text: qsTr("渲染 API") }
                Controls.Label { text: probe.renderApi; Layout.fillWidth: true; wrapMode: Text.Wrap }

                Controls.Label { text: qsTr("渲染器") }
                Controls.Label { text: probe.renderer; Layout.fillWidth: true; wrapMode: Text.Wrap }

                Controls.Label { text: qsTr("预览帧率") }
                Controls.Label {
                    text: probe.busy ? qsTr("检测中…") : qsTr("%1 FPS").arg(probe.framesPerSecond.toFixed(1))
                    Layout.fillWidth: true
                }
            }

            Controls.Label {
                text: probe.details
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: probe.softwareRendering || probe.level === "compatibility"
                type: Kirigami.MessageType.Warning
                text: probe.softwareRendering
                      ? qsTr("检测到软件光栅渲染，当前需要兼容模式；检测分数不代表硬件解码能力。")
                      : qsTr("当前检测结果建议兼容模式；该结果只是短时预览估计，不代表硬件解码能力。")
            }

            RowLayout {
                Layout.fillWidth: true
                Controls.BusyIndicator {
                    running: probe.busy
                    visible: probe.busy
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                }
                Controls.Label {
                    visible: probe.busy
                    text: qsTr("正在采集渲染帧，请稍候…")
                    Layout.fillWidth: true
                }
                Item { Layout.fillWidth: true }
                Controls.Button {
                    text: qsTr("重新检测")
                    enabled: !probe.busy
                    onClicked: root.retestRequested()
                }
            }
        }
    }
}
