# KuantumCut

KuantumCut 是一个基于 Qt 6 / Qt Quick / Kirigami 的轻量级桌面视频剪辑器，使用 FFmpeg 完成视频合成与导出。

## 功能

- 草稿管理：新建、打开、重命名、批量删除草稿，支持自定义分辨率。
- 自动保存：默认开启，停止编辑 3 秒后自动写盘；可在设置中关闭（不建议）。
- 素材库：导入图片、视频、音频，添加文字素材，内置纯色与滤镜预设。
- 多轨时间线：主轨、叠加轨、音频轨、滤镜轨；支持拖拽移动、裁边、拆分、复制/粘贴、重叠吸附、Ctrl + 滚轮缩放。
- 片段属性：缩放、X/Y 位置、旋转、不透明度、音量、四向裁剪、翻转、播放速度。
- 颜色滤镜：灰度、老电影、纯色、单色、场景风格等预设，并可调节滤镜强度。
- 轨道与素材管理：静音/取消静音轨道、锁定/解锁轨道、删除空轨道、清空轨道素材、锁定素材。
- 实时预览：播放/暂停、前进/后退 10 秒、进度拖动、主音量、全屏预览。
- 导出：MP4 / MOV / AVI / WMV，可选帧率、视频比特率与音频质量，支持后台渲染。
- 界面支持简体中文与英文，可在设置中切换。

## 依赖

- CMake ≥ 3.22，C++20 编译器
- Qt 6.10（Core、Gui、Qml、Quick、QuickControls2、Multimedia）
- Qt LinguistTools（Qt 6.10，用于生成翻译）
- KDE Kirigami
- FFmpeg（运行时需要，导出与倒放功能会从 `PATH` 中查找 `ffmpeg`）

## 构建与运行

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
./build/kuantumcut
```

## 测试

```bash
ctest --test-dir build --output-on-failure
```

## 目录结构

```text
src/app/      应用设置、翻译文件（.ts）
src/core/     草稿数据、草稿仓库、媒体探测
src/models/   素材库与时间线模型
src/editor/   编辑、播放与导出控制器
src/qml/      界面（草稿管理、编辑器、预览、时间线、属性面板、设置）
src/shaders/  颜色滤镜着色器
tests/        单元测试
```

## 快捷键

- `Space`：播放 / 暂停
- `←` / `→`：后退 / 前进 10 秒
- `Ctrl + 滚轮`：缩放时间线
- `Ctrl + X` / `Ctrl + C` / `Ctrl + V`：剪切 / 复制 / 粘贴片段
- `Delete`：删除选中片段

## 数据位置

草稿与设置保存在系统应用数据目录（`QStandardPaths::AppDataLocation`）下的 `KuantumCut` 目录中。

## 翻译

界面文本通过 `qsTr()` 标记。修改或新增文本后，用 `lupdate` 更新翻译源：

```bash
lupdate src -ts src/app/kuantumcut_en_US.ts
```

补全 `<translation>` 后重新构建，`lrelease` 会把 `.qm` 编译进资源路径 `:/i18n`，运行时由 `SettingsManager` 按所选语言加载。

## 说明

导出为当前版本的基础实现：合成视频、图片、纯色片段与音频混合；预览中的位移、缩放、旋转、不透明度与文字图层暂未参与导出。
