#include "editor/EditorController.h"

#include "core/DraftRepository.h"
#include "editor/UndoCommands.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUuid>

namespace {

QString systemExecutable(const QString &name)
{
    const QStringList candidates = {
        QStringLiteral("/usr/bin/") + name,
        QStringLiteral("/usr/local/bin/") + name,
        QStringLiteral("/bin/") + name
    };
    for (const QString &path : candidates)
        if (QFileInfo::exists(path))
            return path;
    return QStandardPaths::findExecutable(name);
}

QString videoEncoder()
{
    static QString cached;
    if (!cached.isEmpty())
        return cached;
    const QString ffmpeg = systemExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty())
        return QStringLiteral("mpeg4");
    QProcess process;
    process.start(ffmpeg, {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")});
    if (!process.waitForFinished(5000)) {
        cached = QStringLiteral("mpeg4");
        return cached;
    }
    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    if (output.contains(QStringLiteral(" libx264 ")))
        cached = QStringLiteral("libx264");
    else if (output.contains(QStringLiteral(" libopenh264 ")))
        cached = QStringLiteral("libopenh264");
    else
        cached = QStringLiteral("mpeg4");
    return cached;
}

} // namespace

EditorController::EditorController(DraftRepository *repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
    , m_timeline(&m_assets)
    , m_playback(&m_timeline, &m_assets)
{
    connect(&m_assets, &AssetModel::assetsChanged, this, [this]() {
        if (!m_loading && !m_currentDraftPath.isEmpty()) {
            m_nonUndoDirty = true;
            updateDirty();
        }
    });
    connect(&m_undoStack, &QUndoStack::canUndoChanged, this, &EditorController::canUndoChanged);
    connect(&m_undoStack, &QUndoStack::canRedoChanged, this, &EditorController::canRedoChanged);
    connect(&m_undoStack, &QUndoStack::cleanChanged, this, [this]() { updateDirty(); });

    m_autosaveTimer.setSingleShot(true);
    m_autosaveTimer.setInterval(3000);
    connect(&m_autosaveTimer, &QTimer::timeout, this, [this]() {
        if (m_autosaveEnabled && m_dirty)
            save();
    });
}

void EditorController::setAutosaveEnabled(bool enabled)
{
    if (m_autosaveEnabled == enabled)
        return;
    m_autosaveEnabled = enabled;
    if (!m_autosaveEnabled)
        m_autosaveTimer.stop();
    else if (m_dirty)
        m_autosaveTimer.start();
}

bool EditorController::newDraft(const QString &name, int width, int height)
{
    if (!m_repository)
        return false;
    const QVariantMap created = m_repository->create(name, width, height);
    if (created.isEmpty()) {
        setLastError(m_repository->lastError());
        return false;
    }
    return openDraft(created.value(QStringLiteral("id")).toString());
}

bool EditorController::openDraft(const QString &id)
{
    if (!m_repository)
        return false;
    m_loading = true;
    m_playback.pause();
    const QString previousDirectory = m_assets.draftDirectory();
    const QJsonArray previousAssets = m_assets.toJson();
    const QJsonObject previousTimeline = m_timeline.toJson();
    const QString previousSelection = m_timeline.selectedClipId();
    Draft draft;
    QString error;
    if (!m_repository->load(id, draft, m_assets, m_timeline, &error)) {
        QString restoreError;
        m_assets.setDraftDirectory(previousDirectory);
        m_assets.fromJson(previousAssets, &restoreError);
        m_timeline.fromJson(previousTimeline, &restoreError);
        m_timeline.setSelectedClipId(previousSelection);
        m_loading = false;
        setLastError(error);
        return false;
    }
    const bool result = finishLoad(draft, m_repository->draftFilePath(id));
    m_loading = false;
    return result;
}

bool EditorController::save()
{
    if (!m_repository || m_currentDraftPath.isEmpty()) {
        setLastError(QStringLiteral("There is no open draft to save."));
        return false;
    }
    QString error;
    if (!m_repository->save(m_draft, m_assets, m_timeline, &error)) {
        setLastError(error);
        return false;
    }
    m_nonUndoDirty = false;
    m_undoStack.setClean();
    updateDirty();
    setLastError({});
    return true;
}

QString EditorController::addAudioTrack()
{
    const QString id = m_timeline.addAudioTrack();
    if (!id.isEmpty()) {
        m_nonUndoDirty = true;
        updateDirty();
    }
    return id;
}

QString EditorController::addFilterTrack()
{
    const QString id = m_timeline.addFilterTrack();
    if (!id.isEmpty()) {
        m_nonUndoDirty = true;
        updateDirty();
    }
    return id;
}

QString EditorController::addTrack()
{
    const QString id = m_timeline.addTrack();
    if (!id.isEmpty()) {
        m_nonUndoDirty = true;
        updateDirty();
    }
    return id;
}

QString EditorController::addClip(const QString &assetId, const QString &trackId,
                                  qint64 start, qint64 duration)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    const QString id = m_timeline.addClip(assetId, trackId, start, duration);
    if (!id.isEmpty())
        pushTimelineCommand(before, selection, tr("Add clip"));
    return id;
}

bool EditorController::moveClip(const QString &clipId, const QString &trackId, qint64 start)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    if (!m_timeline.moveClip(clipId, trackId, start))
        return false;
    pushTimelineCommand(before, selection, tr("Move clip"));
    return true;
}

bool EditorController::trimClip(const QString &clipId, qint64 start,
                                qint64 sourceIn, qint64 duration)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    if (!m_timeline.trimClip(clipId, start, sourceIn, duration))
        return false;
    pushTimelineCommand(before, selection, tr("Trim clip"));
    return true;
}

bool EditorController::removeClip(const QString &clipId)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    if (!m_timeline.removeClip(clipId))
        return false;
    pushTimelineCommand(before, selection, tr("Remove clip"));
    return true;
}

QString EditorController::splitClip(const QString &clipId, qint64 atPosition)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    const QString id = m_timeline.splitClip(clipId, atPosition);
    if (!id.isEmpty())
        pushTimelineCommand(before, selection, tr("Split clip"));
    return id;
}

bool EditorController::trimAt(const QString &clipId, qint64 atPosition, bool keepLeft)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    if (!m_timeline.trimAt(clipId, atPosition, keepLeft))
        return false;
    pushTimelineCommand(before, selection, tr("Trim clip at playhead"));
    return true;
}

QString EditorController::insertClip(const QVariantMap &data, const QString &trackId, qint64 start)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    const QString id = m_timeline.insertClip(data, trackId, start);
    if (!id.isEmpty())
        pushTimelineCommand(before, selection, tr("Paste clip"));
    return id;
}

QString EditorController::detachAudio(const QString &clipId)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    const QString id = m_timeline.detachAudio(clipId);
    if (!id.isEmpty())
        pushTimelineCommand(before, selection, tr("Detach audio"));
    return id;
}

bool EditorController::reattachAudio(const QString &clipId)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    if (!m_timeline.reattachAudio(clipId))
        return false;
    pushTimelineCommand(before, selection, tr("Reattach audio"));
    return true;
}

bool EditorController::setTrackMuted(const QString &trackId, bool muted)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    if (!m_timeline.setTrackMuted(trackId, muted))
        return false;
    pushTimelineCommand(before, selection, muted ? tr("Mute track") : tr("Unmute track"));
    return true;
}

bool EditorController::setTrackLocked(const QString &trackId, bool locked)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    if (!m_timeline.setTrackLocked(trackId, locked))
        return false;
    pushTimelineCommand(before, selection, locked ? tr("Lock track") : tr("Unlock track"));
    return true;
}

bool EditorController::clearTrack(const QString &trackId)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    if (!m_timeline.clearTrack(trackId))
        return false;
    pushTimelineCommand(before, selection, tr("Clear track"));
    return true;
}

bool EditorController::removeTrack(const QString &trackId)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    if (!m_timeline.removeTrack(trackId))
        return false;
    pushTimelineCommand(before, selection, tr("Remove track"));
    return true;
}

bool EditorController::setClipPosition(const QString &clipId, double x, double y)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    const bool moved = m_timeline.setClipProperty(clipId, QStringLiteral("x"), x);
    if (!m_timeline.setClipProperty(clipId, QStringLiteral("y"), y) && !moved)
        return false;
    pushTimelineCommand(before, selection, tr("Move clip"));
    return true;
}

bool EditorController::setClipProperty(const QString &clipId, const QString &property,
                                       const QVariant &value, bool merge)
{
    const QJsonObject before = m_timeline.toJson();
    const QString selection = m_timeline.selectedClipId();
    if (!m_timeline.setClipProperty(clipId, property, value))
        return false;
    pushTimelineCommand(before, selection, tr("Change clip property"), merge, clipId, property);
    return true;
}

void EditorController::undo()
{
    m_undoStack.undo();
}

void EditorController::redo()
{
    m_undoStack.redo();
}

void EditorController::toggleReverse(const QString &clipId)
{
    if (m_reverseProcessing || clipId.isEmpty())
        return;
    TimelineClip clip;
    if (!m_timeline.clip(clipId, &clip))
        return;
    if (m_assets.kind(clip.assetId) != QStringLiteral("video"))
        return;

    // 查找分离出来的音频片段（其 linkedClipId 指向本视频片段）
    QString linkedAudioId;
    for (const TimelineClip &c : m_timeline.clips()) {
        if (c.linkedClipId == clipId) {
            linkedAudioId = c.id;
            break;
        }
    }

    if (clip.reverse) {
        // 取消倒放：恢复原素材
        if (clip.originalAssetId.isEmpty()) {
            // 旧草稿兼容：无预渲染素材，仅清除倒放标记
            m_timeline.setClipProperty(clipId, QStringLiteral("reverse"), false);
            return;
        }
        const QString reversedAssetId = clip.assetId;
        m_timeline.restoreClipAsset(clipId);
        if (!linkedAudioId.isEmpty())
            m_timeline.restoreClipAsset(linkedAudioId);
        // 清理倒放产生的素材
        if (!reversedAssetId.isEmpty())
            m_assets.removeAsset(reversedAssetId);
        m_nonUndoDirty = true;
        updateDirty();
        return;
    }

    // 开启倒放：预渲染倒放视频
    m_reverseClipId = clipId;
    m_reverseLinkedClipId = linkedAudioId;
    m_reverseOriginalAssetId = clip.assetId;
    startReverseProcess(clipId);
}

void EditorController::startReverseProcess(const QString &clipId)
{
    const QString sourcePath = m_assets.absoluteSource(m_reverseOriginalAssetId);
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        setLastError(tr("无法读取原始视频文件，倒放失败。"));
        return;
    }
    const QString ffmpeg = systemExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) {
        setLastError(tr("未找到 ffmpeg，无法生成倒放视频。"));
        return;
    }

    // 输出到临时目录，成功后再导入素材库
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString outputName = QStringLiteral("kuantumcut_reverse_")
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".mp4");
    m_reverseOutputPath = QDir(tempDir).absoluteFilePath(outputName);

    const QString encoder = videoEncoder();
    const QStringList arguments = {
        QStringLiteral("-y"),
        QStringLiteral("-i"), sourcePath,
        QStringLiteral("-vf"), QStringLiteral("reverse"),
        QStringLiteral("-af"), QStringLiteral("areverse"),
        QStringLiteral("-c:v"), encoder,
        QStringLiteral("-preset"), QStringLiteral("fast"),
        QStringLiteral("-c:a"), QStringLiteral("aac"),
        QStringLiteral("-b:a"), QStringLiteral("192k"),
        m_reverseOutputPath
    };

    setReverseStatusText(tr("正在生成倒放视频，请稍候..."));
    setReverseProcessing(true);

    m_reverseProcess = new QProcess(this);
    m_reverseProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_reverseProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart)
                    onReverseFinished(-1, QProcess::CrashExit);
            });
    connect(m_reverseProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &EditorController::onReverseFinished);
    m_reverseProcess->start(ffmpeg, arguments);
}

void EditorController::onReverseFinished(int exitCode, QProcess::ExitStatus status)
{
    const bool success = status == QProcess::NormalExit && exitCode == 0
        && QFileInfo::exists(m_reverseOutputPath);

    if (success) {
        const QString newAssetId = m_assets.importMedia(QUrl::fromLocalFile(m_reverseOutputPath));
        if (!newAssetId.isEmpty()) {
            m_timeline.replaceClipAsset(m_reverseClipId, newAssetId);
            if (!m_reverseLinkedClipId.isEmpty())
                m_timeline.replaceClipAsset(m_reverseLinkedClipId, newAssetId);
            m_nonUndoDirty = true;
            updateDirty();
        } else {
            setLastError(m_assets.lastError().isEmpty()
                         ? tr("导入倒放视频失败。") : m_assets.lastError());
        }
    } else {
        setLastError(tr("生成倒放视频失败（ffmpeg 退出码 %1）。").arg(exitCode));
    }

    // 清理临时文件
    if (!m_reverseOutputPath.isEmpty() && QFile::exists(m_reverseOutputPath))
        QFile::remove(m_reverseOutputPath);

    m_reverseProcess->deleteLater();
    m_reverseProcess = nullptr;
    m_reverseClipId.clear();
    m_reverseLinkedClipId.clear();
    m_reverseOriginalAssetId.clear();
    m_reverseOutputPath.clear();
    setReverseProcessing(false);
    setReverseStatusText(QString());
}

void EditorController::setReverseProcessing(bool value)
{
    if (m_reverseProcessing == value)
        return;
    m_reverseProcessing = value;
    emit reverseProcessingChanged();
}

void EditorController::setReverseStatusText(const QString &text)
{
    if (m_reverseStatusText == text)
        return;
    m_reverseStatusText = text;
    emit reverseStatusTextChanged();
}

void EditorController::pushTimelineCommand(const QJsonObject &before,
                                           const QString &selectionBefore, const QString &text,
                                           bool mergeable, const QString &clipId,
                                           const QString &property)
{
    m_undoStack.push(new TimelineCommand(&m_timeline, before, m_timeline.toJson(),
                                         selectionBefore, m_timeline.selectedClipId(), text,
                                         mergeable, clipId, property));
    updateDirty();
}

void EditorController::updateDirty()
{
    const bool value = m_nonUndoDirty || !m_undoStack.isClean();
    if (m_dirty == value)
        return;
    m_dirty = value;
    // 自动保存：停止编辑 3 秒后写盘，避免频繁操作期间反复写文件。
    if (m_dirty && m_autosaveEnabled)
        m_autosaveTimer.start();
    else
        m_autosaveTimer.stop();
    emit dirtyChanged();
}

void EditorController::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

bool EditorController::finishLoad(const Draft &draft, const QString &path)
{
    m_draft = draft;
    m_currentDraftPath = path;
    m_nonUndoDirty = false;
    m_undoStack.clear();
    m_undoStack.setClean();
    m_playback.seek(0);
    updateDirty();
    setLastError({});
    emit currentDraftChanged();
    return true;
}
