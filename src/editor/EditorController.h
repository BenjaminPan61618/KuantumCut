#pragma once

#include "core/Draft.h"
#include "editor/PlaybackController.h"
#include "models/AssetModel.h"
#include "models/TimelineModel.h"

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QUndoStack>

class DraftRepository;

class EditorController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)
    Q_PROPERTY(QString currentDraftName READ currentDraftName NOTIFY currentDraftChanged)
    Q_PROPERTY(QString currentDraftPath READ currentDraftPath NOTIFY currentDraftChanged)
    Q_PROPERTY(int canvasWidth READ canvasWidth NOTIFY currentDraftChanged)
    Q_PROPERTY(int canvasHeight READ canvasHeight NOTIFY currentDraftChanged)
    Q_PROPERTY(AssetModel *assets READ assets CONSTANT)
    Q_PROPERTY(TimelineModel *timeline READ timeline CONSTANT)
    Q_PROPERTY(PlaybackController *playback READ playback CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool reverseProcessing READ reverseProcessing NOTIFY reverseProcessingChanged)
    Q_PROPERTY(QString reverseStatusText READ reverseStatusText NOTIFY reverseStatusTextChanged)

public:
    explicit EditorController(DraftRepository *repository, QObject *parent = nullptr);

    bool dirty() const { return m_dirty; }
    bool canUndo() const { return m_undoStack.canUndo(); }
    bool canRedo() const { return m_undoStack.canRedo(); }
    QString currentDraftName() const { return m_draft.name; }
    QString currentDraftPath() const { return m_currentDraftPath; }
    int canvasWidth() const { return m_draft.width; }
    int canvasHeight() const { return m_draft.height; }
    AssetModel *assets() { return &m_assets; }
    TimelineModel *timeline() { return &m_timeline; }
    PlaybackController *playback() { return &m_playback; }
    QString lastError() const { return m_lastError; }
    bool reverseProcessing() const { return m_reverseProcessing; }
    QString reverseStatusText() const { return m_reverseStatusText; }

    Q_INVOKABLE bool newDraft(const QString &name, int width, int height);
    Q_INVOKABLE bool openDraft(const QString &id);
    Q_INVOKABLE bool save();

    void setAutosaveEnabled(bool enabled);
    bool autosaveEnabled() const { return m_autosaveEnabled; }

    Q_INVOKABLE QString addTrack();
    Q_INVOKABLE QString addAudioTrack();
    Q_INVOKABLE QString addFilterTrack();
    Q_INVOKABLE QString addClip(const QString &assetId, const QString &trackId,
                                qint64 start, qint64 duration = -1);
    Q_INVOKABLE bool moveClip(const QString &clipId, const QString &trackId, qint64 start);
    Q_INVOKABLE bool trimClip(const QString &clipId, qint64 start,
                              qint64 sourceIn, qint64 duration);
    Q_INVOKABLE bool removeClip(const QString &clipId);
    Q_INVOKABLE QString splitClip(const QString &clipId, qint64 atPosition);
    Q_INVOKABLE bool trimAt(const QString &clipId, qint64 atPosition, bool keepLeft);
    Q_INVOKABLE QString insertClip(const QVariantMap &data, const QString &trackId, qint64 start);
    Q_INVOKABLE QString detachAudio(const QString &clipId);
    Q_INVOKABLE bool reattachAudio(const QString &clipId);
    Q_INVOKABLE bool setTrackMuted(const QString &trackId, bool muted);
    Q_INVOKABLE bool setTrackLocked(const QString &trackId, bool locked);
    Q_INVOKABLE bool clearTrack(const QString &trackId);
    Q_INVOKABLE bool removeTrack(const QString &trackId);
    Q_INVOKABLE bool setClipPosition(const QString &clipId, double x, double y);
    Q_INVOKABLE bool setClipProperty(const QString &clipId, const QString &property,
                                     const QVariant &value, bool merge = false);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void toggleReverse(const QString &clipId);

signals:
    void dirtyChanged();
    void canUndoChanged();
    void canRedoChanged();
    void currentDraftChanged();
    void lastErrorChanged();
    void reverseProcessingChanged();
    void reverseStatusTextChanged();

private:
    void pushTimelineCommand(const QJsonObject &before, const QString &selectionBefore,
                             const QString &text, bool mergeable = false,
                             const QString &clipId = {}, const QString &property = {});
    void updateDirty();
    void setLastError(const QString &message);
    bool finishLoad(const Draft &draft, const QString &path);
    void startReverseProcess(const QString &clipId);
    void onReverseFinished(int exitCode, QProcess::ExitStatus status);
    void setReverseProcessing(bool value);
    void setReverseStatusText(const QString &text);

    DraftRepository *m_repository;
    AssetModel m_assets;
    TimelineModel m_timeline;
    PlaybackController m_playback;
    QUndoStack m_undoStack;
    Draft m_draft;
    QString m_currentDraftPath;
    QString m_lastError;
    bool m_dirty = false;
    bool m_nonUndoDirty = false;
    bool m_loading = false;
    QProcess *m_reverseProcess = nullptr;
    QString m_reverseClipId;
    QString m_reverseLinkedClipId;
    QString m_reverseOriginalAssetId;
    QString m_reverseOutputPath;
    bool m_reverseProcessing = false;
    QString m_reverseStatusText;
    QTimer m_autosaveTimer;
    bool m_autosaveEnabled = true;
};
