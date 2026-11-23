#pragma once

#include "models/TimelineModel.h"

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QHash>
#include <QTimer>
#include <QUrl>

class AssetModel;
class QAudioOutput;
class QMediaPlayer;
class QVideoSink;

class PlaybackController : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(double masterVolume READ masterVolume WRITE setMasterVolume NOTIFY masterVolumeChanged)
    Q_PROPERTY(QAbstractListModel *activeVisuals READ activeVisuals CONSTANT)
    Q_PROPERTY(QString audioOutput READ audioOutput WRITE setAudioOutput NOTIFY audioOutputChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString activeColorFilter READ activeColorFilter NOTIFY activeColorFilterChanged)
    Q_PROPERTY(double activeFilterStrength READ activeFilterStrength NOTIFY activeFilterStrengthChanged)

public:
    enum Role {
        ClipIdRole = Qt::UserRole + 1,
        KindRole,
        SourceRole,
        TextRole,
        VideoSinkRole,
        ScaleRole,
        XRole,
        YRole,
        RotationRole,
        OpacityRole,
        CropLeftRole,
        CropTopRole,
        CropRightRole,
        CropBottomRole,
        ZRole,
        TrackIdRole,
        MissingRole,
        ColorRole,
        FlipHorizontalRole,
        FlipVerticalRole,
        ReverseRole,
        SpeedRole,
        ColorFilterRole,
        LockedRole,
        TextFontRole,
        TextBoldRole,
        TextItalicRole,
        TextUnderlineRole,
        TextStrikeoutRole,
        TextFillColorRole,
        TextStrokeColorRole,
        TextShadowColorRole
    };
    Q_ENUM(Role)

    PlaybackController(TimelineModel *timeline, AssetModel *assets, QObject *parent = nullptr);
    ~PlaybackController() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    qint64 position() const { return m_position; }
    qint64 duration() const { return m_duration; }
    bool playing() const { return m_playing; }
    double masterVolume() const { return m_masterVolume; }
    QAbstractListModel *activeVisuals() { return this; }
    QString audioOutput() const { return m_audioOutput; }
    QString statusText() const { return m_statusText; }
    QString activeColorFilter() const { return m_activeColorFilter; }
    double activeFilterStrength() const { return m_activeFilterStrength; }

    void setMasterVolume(double value);
    void setAudioOutput(const QString &deviceId);

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void seek(qint64 positionMs);
    Q_INVOKABLE void seekForward();
    Q_INVOKABLE void seekBackward();
    Q_INVOKABLE void bindVideoSink(QVideoSink *source, QVideoSink *target);
    Q_INVOKABLE QVariantMap colorFilterParams(const QString &name) const;

signals:
    void positionChanged();
    void durationChanged();
    void playingChanged();
    void masterVolumeChanged();
    void audioOutputChanged();
    void statusTextChanged();
    void activeColorFilterChanged();
    void activeFilterStrengthChanged();

private:
    struct MediaSlot;
    struct VisualLayer {
        TimelineClip clip;
        int z = 0;
    };

    MediaSlot *createSlot(const TimelineClip &clip);
    void rebuildActive(bool forceSeek = false);
    void syncSlot(MediaSlot *slot, bool forceSeek);
    qint64 targetPosition(const MediaSlot &slot) const;
    void updateClock();
    void setPlaying(bool playing);
    void setPosition(qint64 position);
    void setDuration(qint64 duration);
    void setStatusText(const QString &text);
    void applyAudioDevice(QAudioOutput *output) const;

    TimelineModel *m_timeline;
    AssetModel *m_assets;
    QList<VisualLayer> m_visuals;
    QHash<QString, MediaSlot *> m_media;
    QTimer m_clockTimer;
    QElapsedTimer m_clock;
    qint64 m_clockBase = 0;
    qint64 m_position = 0;
    qint64 m_duration = 0;
    bool m_playing = false;
    double m_masterVolume = 1.0;
    QString m_audioOutput;
    QString m_statusText;
    QString m_activeColorFilter;
    double m_activeFilterStrength = 1.0;
};
