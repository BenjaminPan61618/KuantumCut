# KuantumCut 可用 MVP 实施计划

## Context

当前 `/home/benjamin/KuantumCut` 是空目录。本次从零搭建 Qt 6 + C++ + Kirigami 桌面视频剪辑器，交付可真实导入媒体、编辑、多轨预览和保存/重开草稿的 MVP；不包含最终成片导出、专业特效或帧级无缝合成。

已确认用户选择 **Qt Multimedia** 后端。环境有 Qt 6.10.0、Kirigami 6.22、FFmpeg 7.1.5、CMake 和 GCC，但缺少 Qt/Kirigami 开发包；当前图形渲染器为 llvmpipe，必须显示软件渲染/兼容模式，不能虚报硬件加速。

## 1. 建立可构建工程

- 前置安装 Mageia 开发包：`qt6-base-devel`、`qt6-declarative-devel`、`qt6-multimedia-devel`、`kf6-kirigami-devel`；ECM 已安装。此步骤需要系统软件安装权限，若软件源未配置则先明确反馈。
- 新建 `CMakeLists.txt`、`src/main.cpp` 和 QML 资源模块，使用 C++20、Qt Quick Controls、Kirigami、Qt Multimedia、`qt_add_translations` 和 CTest。
- C++ 负责状态、模型、文件和播放控制，QML 只负责呈现与交互。所有 GUI 对象和媒体播放器留在 GUI 线程，草稿扫描、媒体信息读取采用异步任务/进程，结果回到 GUI 线程更新模型。

## 2. 启动性能检测与草稿管理

- 在 `src/core/SystemProbe.*` 中读取 Qt 实际渲染 API、渲染器、CPU 和内存，并短时采样 `QQuickWindow::frameSwapped` 帧间隔。
- 按“软件渲染、采样帧率/卡顿、系统资源”给出流畅、基础可用、兼容模式三档**预估**，展示检测依据，允许重新检测；不根据 FFmpeg hwaccels 列表推断硬解可用。
- `src/core/DraftRepository.*` 使用 `QStandardPaths` 下的草稿库，每个草稿为 UUID 目录，包含 `draft.kcdraft` 与 `media/`。草稿页支持按修改时间列表、新建、打开、重命名、确认删除和未保存关闭保护。

## 3. 专有草稿与持久设置

- `.kcdraft` 定义为带格式标识、`schemaVersion` 的 UTF-8 JSON：保存画布、素材、轨道、毫秒时间、源素材入出点、文字、变换、裁剪和音量。
- 导入媒体复制到草稿 `media/` 并以 UUID 重命名，草稿只记录相对路径；使用 `QSaveFile` 原子写入。缺素材时保留记录并允许重新定位，重命名草稿不移动媒体。
- `src/app/SettingsManager.*` 用 `QSettings` 保存中英语言、跟随系统/浅色/深色、新草稿默认分辨率、音频输出设备、后端与解码偏好。
- 后端明确显示 Qt Multimedia，FFmpeg/自动选择仅提供 Qt 实际支持的选项；硬件解码“自动/禁用”在启动早期应用并注明重启生效。无法确认生效的硬件能力只展示状态或禁用，不提供虚假开关。

## 4. 素材、轨道与撤销模型

- `src/models/AssetModel.*`：图片、视频、音频导入，分类筛选、列表展示和文字片段创建；用异步 `ffprobe` 仅读取时长/流信息，实际解码仍由 Qt Multimedia 完成。
- `src/models/TimelineModel.*`：主视频轨 V1、可动态增加的叠加轨 V2…Vn、音频轨 A1；支持选中、添加、删除、移动、左右裁边、同轨防重叠、磁吸到播放头及动态像素/秒缩放。
- `src/editor/EditorController.*` 与 `QUndoStack`：素材/片段增删、移动、裁边和属性修改保存前后快照；连续拖动合并为一次命令，保存时设置 clean 点。设置、保存和草稿删除不进入撤销历史。

## 5. 编辑器界面与真实多轨预览

- `src/qml/Main.qml` 管理启动检测、草稿管理、设置和编辑页；`EditorPage.qml` 组合工具栏、素材区、预览、时间轴、右侧检查器。
- `qml/editor/AssetPanel.qml` 实现素材导入、分类及文字添加；`Timeline.qml` 实现多轨片段、播放头、滚动和缩放。
- `qml/editor/Inspector.qml` 按选中片段编辑缩放、X/Y、旋转、音量、透明度和四边裁剪；视觉属性即时映射到对应预览图层。
- `src/editor/PlaybackController.*` 使用中央毫秒时钟，为活动主视频、叠加媒体及音频片段管理若干 `QMediaPlayer`/`QAudioOutput`，QML `VideoOutput`/图片/文字按轨道叠放并应用变换、透明度与裁剪。
- `PreviewPane.qml` 实现播放/暂停、前后 10 秒、音量、全屏和时间显示。切换片段时异步加载并校准位置，播放中仅在漂移超过阈值时校正；MVP 不承诺帧级同步、无缝切换或最终统一编码导出。

## 6. 关键文件结构

```text
CMakeLists.txt
src/main.cpp
src/app/{AppController,SettingsManager}.*
src/core/{Draft,DraftRepository,SystemProbe,MediaProbe}.*
src/models/{AssetModel,TimelineModel}.*
src/editor/{EditorController,PlaybackController,UndoCommands}.*
src/qml/{Main,StartupCheckPage,DraftManagerPage,SettingsPage,EditorPage}.qml
src/qml/editor/{AssetPanel,PreviewPane,Timeline,Inspector}.qml
resources/i18n/
tests/{test_draft,test_timeline_undo}.cpp
tests/qml/tst_timeline.qml
```

## 7. 验证

1. 用 Unix Makefiles 执行 CMake 配置、完整编译、`ctest --output-on-failure` 和 QML 测试/静态检查。
2. 单元测试覆盖草稿往返序列化、原子保存、相对媒体路径、缺素材处理、同轨重叠约束及撤销/重做合并。
3. 用 FFmpeg 生成短视频、音频和测试图片，端到端验证：新建草稿 → 导入三类素材 → 添加文字及多层轨道 → 移动/裁边/修改检查器属性 → 撤销/重做 → 播放、±10 秒、音量和全屏 → 保存并重启重开。
4. 验证草稿重命名/删除确认、未保存提示、媒体丢失重定位、中英与明暗设置持久化。
5. 在当前 llvmpipe 环境确认启动结果显示“兼容模式”，检查界面无阻塞、QML 错误和持续媒体播放报错；明确记录当前后端实际能力及 MVP 限制。
