#include "editor/PlaybackController.h"

#include "models/AssetModel.h"
#include "models/TimelineModel.h"

#include <QAudioDevice>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QSet>
#include <QVideoSink>
#include <algorithm>
#include <utility>

struct PlaybackController::MediaSlot
{
    ~MediaSlot() { delete player; }

    TimelineClip clip;
    QMediaPlayer *player = nullptr;
    QAudioOutput *audio = nullptr;
    QVideoSink *videoSink = nullptr;
    bool pendingSeek = true;
    QElapsedTimer correctionCooldown;
};

PlaybackController::PlaybackController(TimelineModel *timeline, AssetModel *assets, QObject *parent)
    : QAbstractListModel(parent)
    , m_timeline(timeline)
    , m_assets(assets)
    , m_duration(timeline ? timeline->duration() : 0)
{
    qRegisterMetaType<QVideoSink *>("QVideoSink*");
    m_clockTimer.setInterval(33);
    connect(&m_clockTimer, &QTimer::timeout, this, [this]() {
        if (!m_playing)
            return;
        const qint64 next = m_clockBase + m_clock.elapsed();
        if (next >= m_duration) {
            setPosition(m_duration);
            setPlaying(false);
            m_clockTimer.stop();
            for (MediaSlot *slot : std::as_const(m_media))
                slot->player->pause();
            rebuildActive();
            return;
        }
        setPosition(next);
        rebuildActive();
    });
    if (m_timeline) {
        connect(m_timeline, &TimelineModel::timelineChanged, this, [this]() { rebuildActive(); });
        connect(m_timeline, &TimelineModel::durationChanged, this, [this]() {
            setDuration(m_timeline->duration());
            if (m_position > m_duration)
                seek(m_duration);
        });
    }
    if (m_assets)
        connect(m_assets, &AssetModel::assetsChanged, this, [this]() { rebuildActive(); });
}

PlaybackController::~PlaybackController()
{
    m_clockTimer.stop();
    qDeleteAll(m_media);
    m_media.clear();
}

int PlaybackController::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_visuals.size());
}

QVariant PlaybackController::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visuals.size())
        return {};
    const VisualLayer &layer = m_visuals.at(index.row());
    const TimelineClip &clip = layer.clip;
    switch (role) {
    case ClipIdRole: return clip.id;
    case KindRole: return m_assets->kind(clip.assetId);
    case SourceRole: return m_assets->sourceUrl(clip.assetId);
    case TextRole: return m_assets->text(clip.assetId);
    case VideoSinkRole: {
        MediaSlot *slot = m_media.value(clip.id, nullptr);
        return QVariant::fromValue<QVideoSink *>(slot ? slot->videoSink : nullptr);
    }
    case ScaleRole: return clip.scale;
    case XRole: return clip.x;
    case YRole: return clip.y;
    case RotationRole: return clip.rotation;
    case OpacityRole: return clip.opacity;
    case CropLeftRole: return clip.cropLeft;
    case CropTopRole: return clip.cropTop;
    case CropRightRole: return clip.cropRight;
    case CropBottomRole: return clip.cropBottom;
    case ZRole: return layer.z;
    case TrackIdRole: return clip.trackId;
    case MissingRole: return m_assets->missing(clip.assetId);
    case ColorRole: return m_assets->color(clip.assetId);
    case FlipHorizontalRole: return clip.flipHorizontal;
    case FlipVerticalRole: return clip.flipVertical;
    case ReverseRole: return clip.reverse;
    case SpeedRole: return clip.speed;
    case ColorFilterRole: return clip.colorFilter;
    case LockedRole: return clip.locked;
    case TextFontRole: return clip.textFont;
    case TextBoldRole: return clip.textBold;
    case TextItalicRole: return clip.textItalic;
    case TextUnderlineRole: return clip.textUnderline;
    case TextStrikeoutRole: return clip.textStrikeout;
    case TextFillColorRole: return clip.textFillColor;
    case TextStrokeColorRole: return clip.textStrokeColor;
    case TextShadowColorRole: return clip.textShadowColor;
    default: return {};
    }
}

QHash<int, QByteArray> PlaybackController::roleNames() const
{
    return {
        {ClipIdRole, "clipId"}, {KindRole, "kind"}, {SourceRole, "source"},
        {TextRole, "text"}, {VideoSinkRole, "videoSink"}, {ScaleRole, "scale"},
        {XRole, "x"}, {YRole, "y"}, {RotationRole, "rotation"},
        {OpacityRole, "opacity"}, {CropLeftRole, "cropLeft"},
        {CropTopRole, "cropTop"}, {CropRightRole, "cropRight"},
        {CropBottomRole, "cropBottom"}, {ZRole, "z"}, {TrackIdRole, "trackId"},
        {MissingRole, "missing"}, {ColorRole, "color"},
        {FlipHorizontalRole, "flipHorizontal"}, {FlipVerticalRole, "flipVertical"},
        {ReverseRole, "reverse"}, {SpeedRole, "speed"}, {ColorFilterRole, "colorFilter"},
        {LockedRole, "locked"},
        {TextFontRole, "textFont"}, {TextBoldRole, "textBold"},
        {TextItalicRole, "textItalic"}, {TextUnderlineRole, "textUnderline"},
        {TextStrikeoutRole, "textStrikeout"}, {TextFillColorRole, "textFillColor"},
        {TextStrokeColorRole, "textStrokeColor"}, {TextShadowColorRole, "textShadowColor"}
    };
}

void PlaybackController::setMasterVolume(double value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_masterVolume, value))
        return;
    m_masterVolume = value;
    for (MediaSlot *slot : std::as_const(m_media)) {
        const bool muted = m_timeline && m_timeline->trackMuted(slot->clip.trackId);
        slot->audio->setVolume((slot->clip.audioDetached || muted)
                               ? 0.0f : float(m_masterVolume * slot->clip.volume));
    }
    emit masterVolumeChanged();
}

void PlaybackController::setAudioOutput(const QString &deviceId)
{
    if (m_audioOutput == deviceId)
        return;
    m_audioOutput = deviceId;
    for (MediaSlot *slot : std::as_const(m_media))
        applyAudioDevice(slot->audio);
    emit audioOutputChanged();
}

void PlaybackController::play()
{
    if (m_duration <= 0)
        return;
    if (m_position >= m_duration)
        seek(0);
    m_clockBase = m_position;
    m_clock.start();
    m_clockTimer.start();
    setPlaying(true);
    rebuildActive(true);
}

void PlaybackController::pause()
{
    if (m_playing)
        updateClock();
    m_clockTimer.stop();
    setPlaying(false);
    for (MediaSlot *slot : std::as_const(m_media))
        slot->player->pause();
    rebuildActive();
}

void PlaybackController::togglePlayback()
{
    if (m_playing)
        pause();
    else
        play();
}

void PlaybackController::seek(qint64 positionMs)
{
    positionMs = qBound<qint64>(0, positionMs, m_duration);
    setPosition(positionMs);
    if (m_playing) {
        m_clockBase = m_position;
        m_clock.restart();
    }
    rebuildActive(true);
}

void PlaybackController::seekForward()
{
    seek(m_position + 10000);
}

void PlaybackController::seekBackward()
{
    seek(m_position - 10000);
}

void PlaybackController::bindVideoSink(QVideoSink *source, QVideoSink *target)
{
    if (!source || !target || source == target)
        return;
    disconnect(source, nullptr, target, nullptr);
    connect(source, &QVideoSink::videoFrameChanged, target,
            [target](const QVideoFrame &frame) { target->setVideoFrame(frame); });
}

QVariantMap PlaybackController::colorFilterParams(const QString &name) const
{
    const ColorFilterParams p = TimelineModel::colorFilterParams(name);
    return {
        {QStringLiteral("brightness"), p.brightness},
        {QStringLiteral("contrast"), p.contrast},
        {QStringLiteral("saturation"), p.saturation},
        {QStringLiteral("m00"), p.matrix[0]}, {QStringLiteral("m01"), p.matrix[1]}, {QStringLiteral("m02"), p.matrix[2]},
        {QStringLiteral("m10"), p.matrix[3]}, {QStringLiteral("m11"), p.matrix[4]}, {QStringLiteral("m12"), p.matrix[5]},
        {QStringLiteral("m20"), p.matrix[6]}, {QStringLiteral("m21"), p.matrix[7]}, {QStringLiteral("m22"), p.matrix[8]},
        {QStringLiteral("keepHue"), p.keepHue},
        {QStringLiteral("keepRange"), p.keepHue < 0.0 ? 0.0 : p.keepSimilarity * 0.25}
    };
}

PlaybackController::MediaSlot *PlaybackController::createSlot(const TimelineClip &clip)
{
    auto *slot = new MediaSlot;
    slot->clip = clip;
    slot->player = new QMediaPlayer(this);
    slot->audio = new QAudioOutput(slot->player);
    applyAudioDevice(slot->audio);
    const bool trackMuted = m_timeline && m_timeline->trackMuted(clip.trackId);
    slot->audio->setVolume((clip.audioDetached || trackMuted)
                           ? 0.0f : float(m_masterVolume * clip.volume));
    slot->player->setAudioOutput(slot->audio);
    const bool audioTrack = m_timeline
        && m_timeline->trackKind(clip.trackId) == QStringLiteral("audio");
    if (m_assets->kind(clip.assetId) == QStringLiteral("video") && !audioTrack) {
        slot->videoSink = new QVideoSink(slot->player);
        slot->player->setVideoOutput(slot->videoSink);
    }
    slot->correctionCooldown.start();
    connect(slot->player, &QMediaPlayer::mediaStatusChanged, this,
            [this, slot](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
                    syncSlot(slot, false);
            });
    connect(slot->player, &QMediaPlayer::errorOccurred, this,
            [this, slot](QMediaPlayer::Error, const QString &errorString) {
                setStatusText(QStringLiteral("%1: %2").arg(slot->clip.id, errorString));
            });
    m_media.insert(clip.id, slot);
    syncSlot(slot, true);
    return slot;
}

void PlaybackController::rebuildActive(bool forceSeek)
{
    QList<VisualLayer> desiredVisuals;
    QList<TimelineClip> desiredMedia;
    if (m_timeline && m_assets) {
        const QList<TimelineClip> allClips = m_timeline->clips();
        for (const TimelineClip &clip : allClips) {
            if (m_position < clip.start || m_position >= clip.start + clip.duration)
                continue;
            const QString kind = m_assets->kind(clip.assetId);
            const QString trackKind = m_timeline->trackKind(clip.trackId);
            // 滤镜叠加轨道的片段不产生画面/音频，仅用于颜色滤镜
            if (trackKind == QStringLiteral("filter"))
                continue;
            const bool audioTrack = trackKind == QStringLiteral("audio");
            if (kind == QStringLiteral("audio") || audioTrack) {
                desiredMedia.append(clip);
            } else {
                desiredVisuals.append({clip, m_timeline->trackZ(clip.trackId)});
                if (kind == QStringLiteral("video"))
                    desiredMedia.append(clip);
            }
        }
    }
    std::sort(desiredVisuals.begin(), desiredVisuals.end(), [](const VisualLayer &a,
                                                               const VisualLayer &b) {
        if (a.z != b.z)
            return a.z < b.z;
        return a.clip.id < b.clip.id;
    });

    // 计算当前时间点最顶层激活的颜色滤镜（来自滤镜叠加轨道）
    const QString activeFilter = m_timeline ? m_timeline->activeFilterNameAt(m_position) : QString{};
    if (activeFilter != m_activeColorFilter) {
        m_activeColorFilter = activeFilter;
        emit activeColorFilterChanged();
    }
    const double filterStrength = m_timeline
        ? m_timeline->activeFilterStrengthAt(m_position) : 1.0;
    if (!qFuzzyCompare(m_activeFilterStrength, filterStrength)) {
        m_activeFilterStrength = filterStrength;
        emit activeFilterStrengthChanged();
    }

    bool sameVisuals = desiredVisuals.size() == m_visuals.size();
    for (int i = 0; sameVisuals && i < desiredVisuals.size(); ++i)
        sameVisuals = desiredVisuals.at(i).clip.id == m_visuals.at(i).clip.id;
    if (!sameVisuals) {
        beginResetModel();
        m_visuals = desiredVisuals;
        endResetModel();
    } else {
        m_visuals = desiredVisuals;
        if (!m_visuals.isEmpty())
            emit dataChanged(index(0), index(m_visuals.size() - 1));
    }

    QSet<QString> desiredIds;
    for (const TimelineClip &clip : desiredMedia) {
        desiredIds.insert(clip.id);
        MediaSlot *slot = m_media.value(clip.id, nullptr);
        bool propChanged = false;
        if (!slot) {
            slot = createSlot(clip);
        } else {
            propChanged = !qFuzzyCompare(slot->clip.speed, clip.speed);
            slot->clip = clip;
        }
        syncSlot(slot, forceSeek || propChanged);
    }
    const auto existingIds = m_media.keys();
    for (const QString &id : existingIds) {
        if (!desiredIds.contains(id)) {
            MediaSlot *slot = m_media.take(id);
            disconnect(slot->player, nullptr, this, nullptr);
            slot->player->stop();
            delete slot;
        }
    }
    if (!m_visuals.isEmpty()) {
        emit dataChanged(index(0), index(m_visuals.size() - 1),
                         {VideoSinkRole, SourceRole, TextRole, MissingRole});
    }
}

void PlaybackController::syncSlot(MediaSlot *slot, bool forceSeek)
{
    if (!slot)
        return;
    const QUrl source = m_assets->sourceUrl(slot->clip.assetId);
    if (slot->player->source() != source) {
        slot->player->stop();
        slot->player->setSource(source);
        slot->pendingSeek = true;
    }
    if (forceSeek)
        slot->pendingSeek = true;
    slot->audio->setVolume((slot->clip.audioDetached
                            || (m_timeline && m_timeline->trackMuted(slot->clip.trackId)))
                           ? 0.0f : float(m_masterVolume * slot->clip.volume));

    // 变速通过 playbackRate 实现（倒放素材已预渲染，无需负速率）
    const double rate = slot->clip.speed;
    if (!qFuzzyCompare(slot->player->playbackRate(), rate))
        slot->player->setPlaybackRate(rate);

    const QMediaPlayer::MediaStatus status = slot->player->mediaStatus();
    const bool seekableStatus = status == QMediaPlayer::LoadedMedia
        || status == QMediaPlayer::BufferedMedia || status == QMediaPlayer::BufferingMedia
        || status == QMediaPlayer::StalledMedia || status == QMediaPlayer::EndOfMedia;
    if (slot->pendingSeek && seekableStatus) {
        slot->player->setPosition(targetPosition(*slot));
        slot->pendingSeek = false;
        slot->correctionCooldown.restart();
    }

    if (m_playing && !slot->pendingSeek && !m_assets->missing(slot->clip.assetId)) {
        if (slot->player->playbackState() != QMediaPlayer::PlayingState)
            slot->player->play();
        if (slot->correctionCooldown.elapsed() > 250
            && qAbs(slot->player->position() - targetPosition(*slot)) > 250) {
            slot->player->setPosition(targetPosition(*slot));
            slot->correctionCooldown.restart();
        }
    } else if (!m_playing) {
        slot->player->pause();
    }
}

qint64 PlaybackController::targetPosition(const MediaSlot &slot) const
{
    const qint64 offset = m_position - slot.clip.start;
    return qMax<qint64>(0, slot.clip.sourceIn + qint64(offset * slot.clip.speed));
}

void PlaybackController::updateClock()
{
    if (!m_playing || !m_clock.isValid())
        return;
    setPosition(qBound<qint64>(0, m_clockBase + m_clock.elapsed(), m_duration));
}

void PlaybackController::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    emit playingChanged();
}

void PlaybackController::setPosition(qint64 position)
{
    if (m_position == position)
        return;
    m_position = position;
    emit positionChanged();
}

void PlaybackController::setDuration(qint64 duration)
{
    if (m_duration == duration)
        return;
    m_duration = duration;
    emit durationChanged();
}

void PlaybackController::setStatusText(const QString &text)
{
    if (m_statusText == text)
        return;
    m_statusText = text;
    emit statusTextChanged();
}

void PlaybackController::applyAudioDevice(QAudioOutput *output) const
{
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!m_audioOutput.isEmpty()) {
        const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
        for (const QAudioDevice &candidate : devices) {
            if (QString::fromLatin1(candidate.id()) == m_audioOutput) {
                device = candidate;
                break;
            }
        }
    }
    output->setDevice(device);
}
