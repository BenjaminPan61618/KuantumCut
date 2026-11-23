#include "models/TimelineModel.h"

#include "models/AssetModel.h"

#include <QCoreApplication>
#include <QSet>
#include <QUuid>
#include <QtGlobal>

QJsonObject TimelineClip::toJson() const
{
    return {
        {QStringLiteral("id"), id}, {QStringLiteral("assetId"), assetId},
        {QStringLiteral("trackId"), trackId}, {QStringLiteral("start"), start},
        {QStringLiteral("sourceIn"), sourceIn}, {QStringLiteral("duration"), duration},
        {QStringLiteral("scale"), scale}, {QStringLiteral("x"), x},
        {QStringLiteral("y"), y}, {QStringLiteral("rotation"), rotation},
        {QStringLiteral("volume"), volume}, {QStringLiteral("opacity"), opacity},
        {QStringLiteral("cropLeft"), cropLeft}, {QStringLiteral("cropTop"), cropTop},
        {QStringLiteral("cropRight"), cropRight}, {QStringLiteral("cropBottom"), cropBottom},
        {QStringLiteral("flipHorizontal"), flipHorizontal},
        {QStringLiteral("flipVertical"), flipVertical},
        {QStringLiteral("speed"), speed}, {QStringLiteral("reverse"), reverse},
        {QStringLiteral("audioDetached"), audioDetached},
        {QStringLiteral("linkedClipId"), linkedClipId},
        {QStringLiteral("originalAssetId"), originalAssetId},
        {QStringLiteral("colorFilter"), colorFilter},
        {QStringLiteral("filterStrength"), filterStrength},
        {QStringLiteral("locked"), locked},
        {QStringLiteral("textFont"), textFont},
        {QStringLiteral("textBold"), textBold},
        {QStringLiteral("textItalic"), textItalic},
        {QStringLiteral("textUnderline"), textUnderline},
        {QStringLiteral("textStrikeout"), textStrikeout},
        {QStringLiteral("textFillColor"), textFillColor},
        {QStringLiteral("textStrokeColor"), textStrokeColor},
        {QStringLiteral("textShadowColor"), textShadowColor}
    };
}

TimelineClip TimelineClip::fromJson(const QJsonObject &object)
{
    TimelineClip clip;
    clip.id = object.value(QStringLiteral("id")).toString();
    clip.assetId = object.value(QStringLiteral("assetId")).toString();
    clip.trackId = object.value(QStringLiteral("trackId")).toString();
    clip.start = object.value(QStringLiteral("start")).toInteger();
    clip.sourceIn = object.value(QStringLiteral("sourceIn")).toInteger();
    clip.duration = object.value(QStringLiteral("duration")).toInteger();
    clip.scale = object.value(QStringLiteral("scale")).toDouble(1.0);
    clip.x = object.value(QStringLiteral("x")).toDouble();
    clip.y = object.value(QStringLiteral("y")).toDouble();
    clip.rotation = object.value(QStringLiteral("rotation")).toDouble();
    clip.volume = object.value(QStringLiteral("volume")).toDouble(1.0);
    clip.opacity = object.value(QStringLiteral("opacity")).toDouble(1.0);
    clip.cropLeft = object.value(QStringLiteral("cropLeft")).toDouble();
    clip.cropTop = object.value(QStringLiteral("cropTop")).toDouble();
    clip.cropRight = object.value(QStringLiteral("cropRight")).toDouble();
    clip.cropBottom = object.value(QStringLiteral("cropBottom")).toDouble();
    clip.flipHorizontal = object.value(QStringLiteral("flipHorizontal")).toBool();
    clip.flipVertical = object.value(QStringLiteral("flipVertical")).toBool();
    clip.speed = object.value(QStringLiteral("speed")).toDouble(1.0);
    clip.reverse = object.value(QStringLiteral("reverse")).toBool();
    clip.audioDetached = object.value(QStringLiteral("audioDetached")).toBool();
    clip.linkedClipId = object.value(QStringLiteral("linkedClipId")).toString();
    clip.originalAssetId = object.value(QStringLiteral("originalAssetId")).toString();
    clip.colorFilter = object.value(QStringLiteral("colorFilter")).toString();
    clip.filterStrength = qBound(0.0, object.value(QStringLiteral("filterStrength")).toDouble(1.0), 1.0);
    clip.locked = object.value(QStringLiteral("locked")).toBool();
    clip.textFont = object.value(QStringLiteral("textFont")).toString();
    clip.textBold = object.value(QStringLiteral("textBold")).toBool();
    clip.textItalic = object.value(QStringLiteral("textItalic")).toBool();
    clip.textUnderline = object.value(QStringLiteral("textUnderline")).toBool();
    clip.textStrikeout = object.value(QStringLiteral("textStrikeout")).toBool();
    clip.textFillColor = object.value(QStringLiteral("textFillColor")).toString(QStringLiteral("#ffffff"));
    clip.textStrokeColor = object.value(QStringLiteral("textStrokeColor")).toString(QStringLiteral("#00000000"));
    clip.textShadowColor = object.value(QStringLiteral("textShadowColor")).toString(QStringLiteral("#00000000"));
    return clip;
}

TimelineModel::TimelineModel(AssetModel *assets, QObject *parent)
    : QAbstractListModel(parent)
    , m_assets(assets)
{
    m_tracks = {
        {QStringLiteral("V1"), QStringLiteral("main"), QStringLiteral("V1"), 0, {}},
        {QStringLiteral("A1"), QStringLiteral("audio"), QStringLiteral("A1"), -1, {}}
    };
    if (m_assets) {
        connect(m_assets, &AssetModel::assetsChanged, this, [this]() {
            if (!m_tracks.isEmpty()) {
                beginResetModel();
                endResetModel();
                emit timelineChanged();
            }
        });
    }
}

int TimelineModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    int result = 0;
    for (const TimelineTrack &track : m_tracks)
        result += int(track.clips.size());
    return result;
}

QVariant TimelineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    const QList<TimelineClip> flatClips = clips();
    if (index.row() < 0 || index.row() >= flatClips.size())
        return {};
    const TimelineClip &clip = flatClips.at(index.row());
    if (role == Qt::DisplayRole)
        return m_assets ? m_assets->name(clip.assetId) : clip.assetId;
    const QVariantMap values = clipMap(clip);
    const QByteArray roleName = roleNames().value(role);
    return roleName.isEmpty() ? QVariant{} : values.value(QString::fromLatin1(roleName));
}

QHash<int, QByteArray> TimelineModel::roleNames() const
{
    return {
        {IdRole, "id"}, {AssetIdRole, "assetId"}, {TrackIdRole, "trackId"},
        {TrackKindRole, "trackKind"}, {KindRole, "kind"}, {NameRole, "name"},
        {SourceRole, "source"}, {TextRole, "text"}, {StartRole, "start"},
        {SourceInRole, "sourceIn"}, {DurationRole, "duration"}, {ScaleRole, "scale"},
        {XRole, "x"}, {YRole, "y"}, {RotationRole, "rotation"},
        {VolumeRole, "volume"}, {OpacityRole, "opacity"},
        {CropLeftRole, "cropLeft"}, {CropTopRole, "cropTop"},
        {CropRightRole, "cropRight"}, {CropBottomRole, "cropBottom"},
        {FlipHorizontalRole, "flipHorizontal"}, {FlipVerticalRole, "flipVertical"},
        {SpeedRole, "speed"}, {ReverseRole, "reverse"},
        {AudioDetachedRole, "audioDetached"}, {ZRole, "z"},
        {ColorFilterRole, "colorFilter"}, {FilterStrengthRole, "filterStrength"},
        {LockedRole, "locked"},
        {TextFontRole, "textFont"}, {TextBoldRole, "textBold"},
        {TextItalicRole, "textItalic"}, {TextUnderlineRole, "textUnderline"},
        {TextStrikeoutRole, "textStrikeout"}, {TextFillColorRole, "textFillColor"},
        {TextStrokeColorRole, "textStrokeColor"}, {TextShadowColorRole, "textShadowColor"}
    };
}

QVariantMap TimelineModel::selectedClip() const
{
    const TimelineClip *selected = clipById(m_selectedClipId);
    return selected ? clipMap(*selected) : QVariantMap{};
}

qint64 TimelineModel::duration() const
{
    qint64 result = 0;
    for (const TimelineTrack &track : m_tracks) {
        for (const TimelineClip &clip : track.clips)
            result = qMax(result, clip.start + clip.duration);
    }
    return result;
}

QVariantList TimelineModel::tracksVariant() const
{
    QVariantList result;
    for (const TimelineTrack &track : m_tracks) {
        result.append(QVariantMap{{QStringLiteral("id"), track.id},
                                  {QStringLiteral("kind"), track.kind},
                                  {QStringLiteral("name"), track.name},
                                  {QStringLiteral("z"), track.z},
                                  {QStringLiteral("muted"), track.muted},
                                  {QStringLiteral("locked"), track.locked},
                                  {QStringLiteral("clipCount"), int(track.clips.size())}});
    }
    return result;
}

void TimelineModel::setSelectedClipId(const QString &id)
{
    if (m_selectedClipId == id || (!id.isEmpty() && !clipById(id)))
        return;
    m_selectedClipId = id;
    emit selectedClipIdChanged();
    emit selectedClipChanged();
}

void TimelineModel::setPixelsPerSecond(double value)
{
    value = qBound(1.0, value, 1000.0);
    if (qFuzzyCompare(m_pixelsPerSecond, value))
        return;
    m_pixelsPerSecond = value;
    emit pixelsPerSecondChanged();
}

QString TimelineModel::addAudioTrack()
{
    int number = 2;
    while (trackById(QStringLiteral("A%1").arg(number)))
        ++number;
    TimelineTrack track{QStringLiteral("A%1").arg(number), QStringLiteral("audio"),
                        QStringLiteral("A%1").arg(number), -1, {}};
    beginResetModel();
    m_tracks.append(track);
    endResetModel();
    emit tracksChanged();
    emit timelineChanged();
    return track.id;
}

QString TimelineModel::addFilterTrack()
{
    int number = 1;
    int z = 100;
    for (const TimelineTrack &track : m_tracks) {
        if (track.kind == QStringLiteral("filter")) {
            ++number;
            z = qMax(z, track.z);
        }
    }
    while (trackById(QStringLiteral("F%1").arg(number)))
        ++number;

    TimelineTrack track{QStringLiteral("F%1").arg(number), QStringLiteral("filter"),
                        QStringLiteral("F%1").arg(number), z + 1, {}};
    int insertAt = m_tracks.size();
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks.at(i).kind == QStringLiteral("audio")) {
            insertAt = i;
            break;
        }
    }
    beginResetModel();
    m_tracks.insert(insertAt, track);
    endResetModel();
    emit tracksChanged();
    emit timelineChanged();
    return track.id;
}

QString TimelineModel::addTrack()
{
    int number = 2;
    int z = 0;
    for (const TimelineTrack &track : m_tracks) {
        if (track.kind == QStringLiteral("overlay")) {
            ++number;
            z = qMax(z, track.z);
        }
    }
    while (trackById(QStringLiteral("V%1").arg(number)))
        ++number;

    TimelineTrack track{QStringLiteral("V%1").arg(number), QStringLiteral("overlay"),
                        QStringLiteral("V%1").arg(number), z + 1, {}};
    int insertAt = m_tracks.size();
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks.at(i).kind == QStringLiteral("audio")) {
            insertAt = i;
            break;
        }
    }
    beginResetModel();
    m_tracks.insert(insertAt, track);
    endResetModel();
    emit tracksChanged();
    emit timelineChanged();
    return track.id;
}

QString TimelineModel::addClip(const QString &assetId, const QString &trackId,
                               qint64 start, qint64 requestedDuration)
{
    TimelineTrack *track = trackById(trackId);
    if (!track || track->locked || !m_assets || !m_assets->contains(assetId) || start < 0
        || !compatible(*track, m_assets->kind(assetId)))
        return {};

    const QString assetKind = m_assets->kind(assetId);
    const qint64 assetDuration = m_assets->duration(assetId);
    qint64 clipDuration = requestedDuration;
    if (clipDuration <= 0)
        clipDuration = assetDuration > 0 ? assetDuration : 5000;
    if ((assetKind == QStringLiteral("video") || assetKind == QStringLiteral("audio"))
        && assetDuration > 0)
        clipDuration = qMin(clipDuration, assetDuration);
    if (clipDuration <= 0)
        return {};

    TimelineClip clip;
    clip.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    clip.assetId = assetId;
    clip.trackId = trackId;
    clip.start = start;
    clip.duration = clipDuration;
    if (assetKind == QStringLiteral("filter"))
        clip.colorFilter = m_assets->filterName(assetId);

    const QString targetId = placeableTrackId(track, clip, assetKind);
    if (targetId.isEmpty())
        return {};
    clip.trackId = targetId;
    track = trackById(targetId);
    if (!track)
        return {};

    beginResetModel();
    track->clips.append(clip);
    endResetModel();
    setSelectedClipId(clip.id);
    emitStructureSignals();
    return clip.id;
}

bool TimelineModel::moveClip(const QString &clipId, const QString &trackId, qint64 start)
{
    if (start < 0)
        return false;
    TimelineClip *sourceClip = clipById(clipId);
    TimelineTrack *destination = trackById(trackId);
    const TimelineTrack *sourceTrack = sourceClip ? trackById(sourceClip->trackId) : nullptr;
    if (!sourceClip || !destination || !m_assets || sourceClip->locked || destination->locked
        || (sourceTrack && sourceTrack->locked)
        || !compatible(*destination, m_assets->kind(sourceClip->assetId)))
        return false;

    TimelineClip moved = *sourceClip;
    moved.trackId = trackId;
    moved.start = start;
    if (!canPlace(*destination, moved))
        return false;

    TimelineTrack *source = trackById(sourceClip->trackId);
    beginResetModel();
    if (source == destination) {
        *sourceClip = moved;
    } else {
        for (auto it = source->clips.begin(); it != source->clips.end(); ++it) {
            if (it->id == clipId) {
                source->clips.erase(it);
                break;
            }
        }
        destination->clips.append(moved);
    }
    endResetModel();
    emitStructureSignals();
    emit clipChanged(clipId);
    return true;
}

bool TimelineModel::trimClip(const QString &clipId, qint64 start,
                             qint64 sourceIn, qint64 newDuration)
{
    TimelineClip *current = clipById(clipId);
    if (!current || start < 0 || sourceIn < 0 || newDuration <= 0)
        return false;
    const TimelineTrack *lockedTrack = trackById(current->trackId);
    if (current->locked || (lockedTrack && lockedTrack->locked))
        return false;
    const QString assetKind = m_assets ? m_assets->kind(current->assetId) : QString{};
    const qint64 assetDuration = m_assets ? m_assets->duration(current->assetId) : 0;
    if ((assetKind == QStringLiteral("video") || assetKind == QStringLiteral("audio"))
        && assetDuration > 0
        && sourceIn + qCeil(newDuration * current->speed) > assetDuration)
        return false;

    TimelineClip trimmed = *current;
    trimmed.start = start;
    trimmed.sourceIn = sourceIn;
    trimmed.duration = newDuration;
    const TimelineTrack *track = trackById(current->trackId);
    if (!track || !canPlace(*track, trimmed))
        return false;

    *current = trimmed;
    const QList<TimelineClip> flatClips = clips();
    for (int row = 0; row < flatClips.size(); ++row) {
        if (flatClips.at(row).id == clipId) {
            emit dataChanged(index(row), index(row),
                             {StartRole, SourceInRole, DurationRole});
            break;
        }
    }
    emit durationChanged();
    emit selectedClipChanged();
    emit timelineChanged();
    emit clipChanged(clipId);
    return true;
}

bool TimelineModel::removeClip(const QString &clipId)
{
    TimelineClip *current = clipById(clipId);
    if (!current)
        return false;
    TimelineTrack *track = trackById(current->trackId);
    if (current->locked || (track && track->locked))
        return false;
    beginResetModel();
    for (auto it = track->clips.begin(); it != track->clips.end(); ++it) {
        if (it->id == clipId) {
            track->clips.erase(it);
            break;
        }
    }
    endResetModel();
    if (m_selectedClipId == clipId)
        setSelectedClipId({});
    emitStructureSignals();
    return true;
}

QString TimelineModel::splitClip(const QString &clipId, qint64 atPosition)
{
    TimelineClip *current = clipById(clipId);
    if (!current)
        return {};
    const TimelineTrack *lockedTrack = trackById(current->trackId);
    if (current->locked || (lockedTrack && lockedTrack->locked))
        return {};
    const qint64 offset = atPosition - current->start;
    if (offset <= 0 || offset >= current->duration)
        return {};
    const qint64 originalStart = current->start;
    const qint64 originalDuration = current->duration;

    TimelineClip left = *current;
    TimelineClip right = *current;
    right.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    right.start = originalStart + offset;
    right.sourceIn = current->sourceIn + qRound64(offset * current->speed);
    right.duration = originalDuration - offset;
    left.duration = offset;

    TimelineTrack *track = trackById(current->trackId);
    if (!track)
        return {};
    // 拆分后原片段只保留左半段，用缩短后的轨道副本校验右半段是否放得下
    TimelineTrack shortened = *track;
    for (TimelineClip &item : shortened.clips) {
        if (item.id == clipId)
            item = left;
    }
    if (!canPlace(shortened, right))
        return {};

    // 已分离音频时，同步切分关联的音频片段，避免声音与画面对不上
    TimelineClip linkedRight;
    TimelineTrack *linkedTrack = nullptr;
    QString linkedId;
    if (current->audioDetached) {
        for (TimelineTrack &candidate : m_tracks) {
            if (candidate.kind != QStringLiteral("audio"))
                continue;
            for (const TimelineClip &audio : candidate.clips) {
                if (audio.linkedClipId != clipId || audio.start != originalStart
                    || audio.duration != originalDuration)
                    continue;
                linkedRight = audio;
                linkedRight.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                linkedRight.start = audio.start + offset;
                linkedRight.sourceIn = audio.sourceIn + qRound64(offset * audio.speed);
                linkedRight.duration = audio.duration - offset;
                linkedRight.linkedClipId = right.id;
                linkedId = audio.id;
                linkedTrack = &candidate;
                break;
            }
            if (linkedTrack)
                break;
        }
        if (linkedTrack) {
            TimelineTrack shortenedLinked = *linkedTrack;
            for (TimelineClip &item : shortenedLinked.clips) {
                if (item.id == linkedId)
                    item.duration = offset;
            }
            if (!canPlace(shortenedLinked, linkedRight))
                linkedTrack = nullptr;
        }
        if (!linkedTrack)
            right.audioDetached = false;
    }

    beginResetModel();
    *current = left;
    track->clips.append(right);
    if (linkedTrack) {
        for (TimelineClip &audio : linkedTrack->clips) {
            if (audio.id == linkedId) {
                audio.duration = offset;
                break;
            }
        }
        linkedTrack->clips.append(linkedRight);
    }
    endResetModel();
    setSelectedClipId(right.id);
    emitStructureSignals();
    emit clipChanged(left.id);
    emit clipChanged(right.id);
    if (linkedTrack) {
        emit clipChanged(linkedId);
        emit clipChanged(linkedRight.id);
    }
    return right.id;
}

bool TimelineModel::trimAt(const QString &clipId, qint64 atPosition, bool keepLeft)
{
    TimelineClip *current = clipById(clipId);
    if (!current || m_assets == nullptr)
        return false;
    const TimelineTrack *lockedTrack = trackById(current->trackId);
    if (current->locked || (lockedTrack && lockedTrack->locked))
        return false;
    const qint64 offset = atPosition - current->start;
    if (offset <= 0 || offset >= current->duration)
        return false;
    if (keepLeft)
        return trimClip(clipId, current->start, current->sourceIn, offset);
    return trimClip(clipId, atPosition,
                    current->sourceIn + qRound64(offset * current->speed),
                    current->duration - offset);
}

QString TimelineModel::insertClip(const QVariantMap &data, const QString &trackId, qint64 start)
{
    TimelineTrack *track = trackById(trackId);
    if (!track || track->locked || !m_assets || start < 0)
        return {};
    const QString assetId = data.value(QStringLiteral("assetId")).toString();
    if (!m_assets->contains(assetId)
        || !compatible(*track, m_assets->kind(assetId)))
        return {};

    TimelineClip clip;
    clip.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    clip.assetId = assetId;
    clip.trackId = trackId;
    clip.start = start;
    clip.sourceIn = qMax<qint64>(0, data.value(QStringLiteral("sourceIn")).toLongLong());
    clip.duration = data.value(QStringLiteral("duration")).toLongLong();
    clip.scale = data.value(QStringLiteral("scale"), 1.0).toDouble();
    clip.x = data.value(QStringLiteral("x")).toDouble();
    clip.y = data.value(QStringLiteral("y")).toDouble();
    clip.rotation = data.value(QStringLiteral("rotation")).toDouble();
    clip.volume = data.value(QStringLiteral("volume"), 1.0).toDouble();
    clip.opacity = data.value(QStringLiteral("opacity"), 1.0).toDouble();
    clip.cropLeft = data.value(QStringLiteral("cropLeft")).toDouble();
    clip.cropTop = data.value(QStringLiteral("cropTop")).toDouble();
    clip.cropRight = data.value(QStringLiteral("cropRight")).toDouble();
    clip.cropBottom = data.value(QStringLiteral("cropBottom")).toDouble();
    clip.flipHorizontal = data.value(QStringLiteral("flipHorizontal")).toBool();
    clip.flipVertical = data.value(QStringLiteral("flipVertical")).toBool();
    clip.speed = data.value(QStringLiteral("speed"), 1.0).toDouble();
    clip.reverse = data.value(QStringLiteral("reverse")).toBool();
    clip.originalAssetId = data.value(QStringLiteral("originalAssetId")).toString();
    clip.colorFilter = data.value(QStringLiteral("colorFilter")).toString();
    clip.filterStrength =
        qBound(0.0, data.value(QStringLiteral("filterStrength"), 1.0).toDouble(), 1.0);
    clip.textFont = data.value(QStringLiteral("textFont")).toString();
    clip.textBold = data.value(QStringLiteral("textBold")).toBool();
    clip.textItalic = data.value(QStringLiteral("textItalic")).toBool();
    clip.textUnderline = data.value(QStringLiteral("textUnderline")).toBool();
    clip.textStrikeout = data.value(QStringLiteral("textStrikeout")).toBool();
    clip.textFillColor = data.value(QStringLiteral("textFillColor"), QStringLiteral("#ffffff")).toString();
    clip.textStrokeColor = data.value(QStringLiteral("textStrokeColor"), QStringLiteral("#00000000")).toString();
    clip.textShadowColor = data.value(QStringLiteral("textShadowColor"), QStringLiteral("#00000000")).toString();
    const QString insertAssetKind = m_assets->kind(assetId);
    if (insertAssetKind == QStringLiteral("filter") && clip.colorFilter.isEmpty())
        clip.colorFilter = m_assets->filterName(assetId);
    if (clip.duration <= 0)
        return {};

    const QString assetKind = m_assets->kind(assetId);
    const qint64 assetDuration = m_assets->duration(assetId);
    if ((assetKind == QStringLiteral("video") || assetKind == QStringLiteral("audio"))
        && assetDuration > 0
        && clip.sourceIn + qCeil(clip.duration * clip.speed) > assetDuration)
        return {};

    const QString targetId = placeableTrackId(track, clip, assetKind);
    if (targetId.isEmpty())
        return {};
    clip.trackId = targetId;
    track = trackById(targetId);
    if (!track)
        return {};

    beginResetModel();
    track->clips.append(clip);
    endResetModel();
    setSelectedClipId(clip.id);
    emitStructureSignals();
    return clip.id;
}

QString TimelineModel::detachAudio(const QString &clipId)
{
    TimelineClip *source = clipById(clipId);
    if (!source || !m_assets || source->audioDetached)
        return {};
    if (source->locked || trackLocked(source->trackId))
        return {};
    if (m_assets->kind(source->assetId) != QStringLiteral("video"))
        return {};

    TimelineClip audio;
    audio.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    audio.assetId = source->assetId;
    audio.start = source->start;
    audio.sourceIn = source->sourceIn;
    audio.duration = source->duration;
    audio.speed = source->speed;
    audio.volume = source->volume;
    audio.linkedClipId = source->id;

    TimelineTrack *audioTrack = nullptr;
    for (TimelineTrack &track : m_tracks) {
        if (track.kind != QStringLiteral("audio") || track.locked)
            continue;
        audio.trackId = track.id;
        if (canPlace(track, audio)) {
            audioTrack = &track;
            break;
        }
    }
    if (!audioTrack)
        return {};

    beginResetModel();
    source->audioDetached = true;
    audioTrack->clips.append(audio);
    endResetModel();
    emitStructureSignals();
    emit clipChanged(source->id);
    emit clipChanged(audio.id);
    return audio.id;
}

bool TimelineModel::reattachAudio(const QString &clipId)
{
    TimelineClip *source = clipById(clipId);
    if (!source || !source->audioDetached)
        return false;
    if (source->locked || trackLocked(source->trackId))
        return false;
    // 关联音频所在的轨道被锁定时不允许改动
    for (const TimelineTrack &track : m_tracks) {
        if (track.kind != QStringLiteral("audio") || !track.locked)
            continue;
        for (const TimelineClip &audio : track.clips)
            if (audio.linkedClipId == clipId)
                return false;
    }

    beginResetModel();
    for (TimelineTrack &track : m_tracks) {
        if (track.kind != QStringLiteral("audio"))
            continue;
        for (auto it = track.clips.begin(); it != track.clips.end();) {
            if (it->linkedClipId == clipId)
                it = track.clips.erase(it);
            else
                ++it;
        }
    }
    source = clipById(clipId);
    if (source)
        source->audioDetached = false;
    endResetModel();
    emitStructureSignals();
    emit clipChanged(clipId);
    return true;
}

bool TimelineModel::setTrackMuted(const QString &trackId, bool muted)
{
    TimelineTrack *track = trackById(trackId);
    if (!track || track->muted == muted)
        return false;
    track->muted = muted;
    emit tracksChanged();
    // 静音滤镜轨会改变当前生效滤镜，需要让播放器重新计算
    emit timelineChanged();
    return true;
}

bool TimelineModel::setTrackLocked(const QString &trackId, bool locked)
{
    TimelineTrack *track = trackById(trackId);
    if (!track || track->locked == locked)
        return false;
    track->locked = locked;
    emit tracksChanged();
    return true;
}

bool TimelineModel::clearTrack(const QString &trackId)
{
    TimelineTrack *track = trackById(trackId);
    if (!track || track->locked)
        return false;
    bool removed = false;
    beginResetModel();
    for (auto it = track->clips.begin(); it != track->clips.end();) {
        // 单个素材被锁定时保留，避免清空操作绕过素材锁
        if (it->locked) {
            ++it;
        } else {
            it = track->clips.erase(it);
            removed = true;
        }
    }
    endResetModel();
    if (!removed)
        return false;
    if (!clipById(m_selectedClipId))
        setSelectedClipId({});
    emit tracksChanged();
    emitStructureSignals();
    return true;
}

bool TimelineModel::removeTrack(const QString &trackId)
{
    TimelineTrack *track = trackById(trackId);
    // V1/A1 是草稿必需的基础轨道，不允许删除
    if (!track || track->locked || !track->clips.isEmpty()
        || trackId == QStringLiteral("V1") || trackId == QStringLiteral("A1"))
        return false;
    beginResetModel();
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks.at(i).id == trackId) {
            m_tracks.removeAt(i);
            break;
        }
    }
    endResetModel();
    emit tracksChanged();
    emitStructureSignals();
    return true;
}

bool TimelineModel::setClipProperty(const QString &clipId, const QString &property,
                                    const QVariant &value)
{
    TimelineClip *clip = clipById(clipId);
    if (!clip)
        return false;
    if (property != QStringLiteral("locked")
        && (clip->locked || trackLocked(clip->trackId)))
        return false;

    int role = 0;
    bool durationAdjusted = false;
    if (property == QStringLiteral("audioDetached")) {
        clip->audioDetached = value.toBool();
        role = AudioDetachedRole;
    } else if (property == QStringLiteral("locked")) {
        clip->locked = value.toBool();
        role = LockedRole;
    } else if (property == QStringLiteral("flipHorizontal")) {
        clip->flipHorizontal = value.toBool();
        role = FlipHorizontalRole;
    } else if (property == QStringLiteral("flipVertical")) {
        clip->flipVertical = value.toBool();
        role = FlipVerticalRole;
    } else if (property == QStringLiteral("reverse")) {
        clip->reverse = value.toBool();
        role = ReverseRole;
    } else if (property == QStringLiteral("colorFilter")) {
        clip->colorFilter = value.toString();
        role = ColorFilterRole;
    } else if (property == QStringLiteral("textBold")) {
        clip->textBold = value.toBool();
        role = TextBoldRole;
    } else if (property == QStringLiteral("textItalic")) {
        clip->textItalic = value.toBool();
        role = TextItalicRole;
    } else if (property == QStringLiteral("textUnderline")) {
        clip->textUnderline = value.toBool();
        role = TextUnderlineRole;
    } else if (property == QStringLiteral("textStrikeout")) {
        clip->textStrikeout = value.toBool();
        role = TextStrikeoutRole;
    } else if (property == QStringLiteral("textFont")) {
        clip->textFont = value.toString();
        role = TextFontRole;
    } else if (property == QStringLiteral("textFillColor")) {
        clip->textFillColor = value.toString();
        role = TextFillColorRole;
    } else if (property == QStringLiteral("textStrokeColor")) {
        clip->textStrokeColor = value.toString();
        role = TextStrokeColorRole;
    } else if (property == QStringLiteral("textShadowColor")) {
        clip->textShadowColor = value.toString();
        role = TextShadowColorRole;
    } else {
        bool ok = false;
        const double number = value.toDouble(&ok);
        if (!ok)
            return false;
        if (property == QStringLiteral("scale")) {
            clip->scale = qBound(0.01, number, 100.0);
            role = ScaleRole;
        } else if (property == QStringLiteral("x")) {
            clip->x = number;
            role = XRole;
        } else if (property == QStringLiteral("y")) {
            clip->y = number;
            role = YRole;
        } else if (property == QStringLiteral("rotation")) {
            clip->rotation = number;
            role = RotationRole;
        } else if (property == QStringLiteral("volume")) {
            clip->volume = qBound(0.0, number, 1.0);
            role = VolumeRole;
        } else if (property == QStringLiteral("opacity")) {
            clip->opacity = qBound(0.0, number, 1.0);
            role = OpacityRole;
        } else if (property == QStringLiteral("cropLeft")) {
            clip->cropLeft = qBound(0.0, number, 0.99);
            role = CropLeftRole;
        } else if (property == QStringLiteral("cropTop")) {
            clip->cropTop = qBound(0.0, number, 0.99);
            role = CropTopRole;
        } else if (property == QStringLiteral("cropRight")) {
            clip->cropRight = qBound(0.0, number, 0.99);
            role = CropRightRole;
        } else if (property == QStringLiteral("cropBottom")) {
            clip->cropBottom = qBound(0.0, number, 0.99);
            role = CropBottomRole;
        } else if (property == QStringLiteral("speed")) {
            const double newSpeed = qBound(0.25, number, 4.0);
            TimelineClip adjusted = *clip;
            adjusted.speed = newSpeed;
            adjusted.duration = qMax<qint64>(1, qRound64(clip->duration * clip->speed / newSpeed));
            const TimelineTrack *track = trackById(clip->trackId);
            if (!track || !canPlace(*track, adjusted))
                return false;
            *clip = adjusted;
            role = SpeedRole;
            durationAdjusted = true;
        } else if (property == QStringLiteral("filterStrength")) {
            clip->filterStrength = qBound(0.0, number, 1.0);
            role = FilterStrengthRole;
        } else {
            return false;
        }
    }

    const QList<TimelineClip> flatClips = clips();
    const QList<int> roles = durationAdjusted ? QList<int>{role, DurationRole} : QList<int>{role};
    for (int row = 0; row < flatClips.size(); ++row) {
        if (flatClips.at(row).id == clipId) {
            emit dataChanged(index(row), index(row), roles);
            break;
        }
    }
    if (durationAdjusted)
        emit durationChanged();
    emit selectedClipChanged();
    emit timelineChanged();
    emit clipChanged(clipId);
    return true;
}

bool TimelineModel::replaceClipAsset(const QString &clipId, const QString &newAssetId)
{
    TimelineClip *clip = clipById(clipId);
    if (!clip || newAssetId.isEmpty())
        return false;
    if (clip->originalAssetId.isEmpty())
        clip->originalAssetId = clip->assetId;
    clip->assetId = newAssetId;
    clip->reverse = true;
    const QList<TimelineClip> flatClips = clips();
    for (int row = 0; row < flatClips.size(); ++row) {
        if (flatClips.at(row).id == clipId) {
            emit dataChanged(index(row), index(row), {AssetIdRole, ReverseRole, SourceRole, NameRole});
            break;
        }
    }
    emit selectedClipChanged();
    emit timelineChanged();
    emit clipChanged(clipId);
    return true;
}

bool TimelineModel::restoreClipAsset(const QString &clipId)
{
    TimelineClip *clip = clipById(clipId);
    if (!clip || clip->originalAssetId.isEmpty())
        return false;
    clip->assetId = clip->originalAssetId;
    clip->originalAssetId.clear();
    clip->reverse = false;
    const QList<TimelineClip> flatClips = clips();
    for (int row = 0; row < flatClips.size(); ++row) {
        if (flatClips.at(row).id == clipId) {
            emit dataChanged(index(row), index(row), {AssetIdRole, ReverseRole, SourceRole, NameRole});
            break;
        }
    }
    emit selectedClipChanged();
    emit timelineChanged();
    emit clipChanged(clipId);
    return true;
}

QString TimelineModel::linkedClipId(const QString &clipId) const
{
    const TimelineClip *clip = clipById(clipId);
    return clip ? clip->linkedClipId : QString{};
}

QVariantMap TimelineModel::getClip(const QString &clipId) const
{
    const TimelineClip *current = clipById(clipId);
    return current ? clipMap(*current) : QVariantMap{};
}

QVariantList TimelineModel::clipsForTrack(const QString &trackId) const
{
    QVariantList result;
    const TimelineTrack *track = trackById(trackId);
    if (!track)
        return result;
    for (const TimelineClip &clip : track->clips)
        result.append(clipMap(clip));
    return result;
}

QList<TimelineClip> TimelineModel::clips() const
{
    QList<TimelineClip> result;
    for (const TimelineTrack &track : m_tracks) {
        for (const TimelineClip &clip : track.clips)
            result.append(clip);
    }
    return result;
}

bool TimelineModel::clip(const QString &clipId, TimelineClip *result) const
{
    const TimelineClip *current = clipById(clipId);
    if (!current)
        return false;
    if (result)
        *result = *current;
    return true;
}

int TimelineModel::trackZ(const QString &trackId) const
{
    const TimelineTrack *track = trackById(trackId);
    return track ? track->z : 0;
}

QString TimelineModel::trackKind(const QString &trackId) const
{
    const TimelineTrack *track = trackById(trackId);
    return track ? track->kind : QString{};
}

bool TimelineModel::trackMuted(const QString &trackId) const
{
    const TimelineTrack *track = trackById(trackId);
    return track && track->muted;
}

bool TimelineModel::trackLocked(const QString &trackId) const
{
    const TimelineTrack *track = trackById(trackId);
    return track && track->locked;
}

QString TimelineModel::activeFilterNameAt(qint64 position) const
{
    QString result;
    int topZ = -1;
    for (const TimelineTrack &track : m_tracks) {
        if (track.kind != QStringLiteral("filter") || track.muted)
            continue;
        for (const TimelineClip &clip : track.clips) {
            if (position < clip.start || position >= clip.start + clip.duration)
                continue;
            if (clip.colorFilter.isEmpty())
                continue;
            if (track.z >= topZ) {
                topZ = track.z;
                result = clip.colorFilter;
            }
        }
    }
    return result;
}

double TimelineModel::activeFilterStrengthAt(qint64 position) const
{
    double result = 1.0;
    int topZ = -1;
    for (const TimelineTrack &track : m_tracks) {
        if (track.kind != QStringLiteral("filter") || track.muted)
            continue;
        for (const TimelineClip &clip : track.clips) {
            if (position < clip.start || position >= clip.start + clip.duration)
                continue;
            if (clip.colorFilter.isEmpty())
                continue;
            if (track.z >= topZ) {
                topZ = track.z;
                result = clip.filterStrength;
            }
        }
    }
    return result;
}

QJsonObject TimelineModel::toJson() const
{
    QJsonArray trackArray;
    for (const TimelineTrack &track : m_tracks) {
        QJsonArray clipArray;
        for (const TimelineClip &clip : track.clips)
            clipArray.append(clip.toJson());
        trackArray.append(QJsonObject{{QStringLiteral("id"), track.id},
                                      {QStringLiteral("kind"), track.kind},
                                      {QStringLiteral("name"), track.name},
                                      {QStringLiteral("z"), track.z},
                                      {QStringLiteral("muted"), track.muted},
                                      {QStringLiteral("locked"), track.locked},
                                      {QStringLiteral("clips"), clipArray}});
    }
    return {{QStringLiteral("version"), 2}, {QStringLiteral("tracks"), trackArray}};
}

bool TimelineModel::fromJson(const QJsonObject &object, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    const int timelineVersion = object.value(QStringLiteral("version")).toInt(1);
    const QJsonArray trackArray = object.value(QStringLiteral("tracks")).toArray();
    QList<TimelineTrack> parsed;
    QSet<QString> trackIds;
    QSet<QString> clipIds;
    for (const QJsonValue &trackValue : trackArray) {
        const QJsonObject trackObject = trackValue.toObject();
        TimelineTrack track;
        track.id = trackObject.value(QStringLiteral("id")).toString();
        track.kind = trackObject.value(QStringLiteral("kind")).toString();
        track.name = trackObject.value(QStringLiteral("name")).toString(track.id);
        track.z = trackObject.value(QStringLiteral("z")).toInt();
        track.muted = trackObject.value(QStringLiteral("muted")).toBool();
        track.locked = trackObject.value(QStringLiteral("locked")).toBool();
        if (track.id.isEmpty() || trackIds.contains(track.id)
            || (track.kind != QStringLiteral("main") && track.kind != QStringLiteral("overlay")
                && track.kind != QStringLiteral("audio") && track.kind != QStringLiteral("filter")))
            return fail(QStringLiteral("The draft contains an invalid track."));
        trackIds.insert(track.id);

        const QJsonArray clipArray = trackObject.value(QStringLiteral("clips")).toArray();
        for (const QJsonValue &clipValue : clipArray) {
            TimelineClip clip = TimelineClip::fromJson(clipValue.toObject());
            clip.trackId = track.id;
            // 旧版本保存的是源素材时长；从版本 2 起保存随速度变化后的时间线时长。
            if (timelineVersion < 2 && clip.speed > 0.0 && !qFuzzyCompare(clip.speed, 1.0))
                clip.duration = qMax<qint64>(1, qRound64(clip.duration / clip.speed));
            const QString assetKind = m_assets ? m_assets->kind(clip.assetId) : QString{};
            const qint64 assetDuration = m_assets ? m_assets->duration(clip.assetId) : 0;
            const bool outsideSource = (assetKind == QStringLiteral("video")
                                        || assetKind == QStringLiteral("audio"))
                && assetDuration > 0
                && clip.sourceIn + qCeil(clip.duration * clip.speed) > assetDuration;
            if (clip.id.isEmpty() || clipIds.contains(clip.id) || !m_assets
                || !m_assets->contains(clip.assetId) || clip.start < 0 || clip.sourceIn < 0
                || clip.duration <= 0 || outsideSource || !compatible(track, assetKind)
                || !canPlace(track, clip))
                return fail(QStringLiteral("The draft contains an invalid or overlapping clip."));
            clipIds.insert(clip.id);
            track.clips.append(clip);
        }
        parsed.append(track);
    }
    bool hasMain = false;
    bool hasAudio = false;
    for (const TimelineTrack &track : parsed) {
        hasMain |= track.id == QStringLiteral("V1") && track.kind == QStringLiteral("main");
        hasAudio |= track.id == QStringLiteral("A1") && track.kind == QStringLiteral("audio");
    }
    if (!hasMain || !hasAudio)
        return fail(QStringLiteral("The draft must contain V1 and A1 tracks."));

    beginResetModel();
    m_tracks = parsed;
    m_selectedClipId.clear();
    endResetModel();
    emit tracksChanged();
    emit selectedClipIdChanged();
    emitStructureSignals();
    return true;
}

TimelineTrack *TimelineModel::trackById(const QString &id)
{
    for (TimelineTrack &track : m_tracks) {
        if (track.id == id)
            return &track;
    }
    return nullptr;
}

const TimelineTrack *TimelineModel::trackById(const QString &id) const
{
    for (const TimelineTrack &track : m_tracks) {
        if (track.id == id)
            return &track;
    }
    return nullptr;
}

TimelineClip *TimelineModel::clipById(const QString &id)
{
    for (TimelineTrack &track : m_tracks) {
        for (TimelineClip &clip : track.clips) {
            if (clip.id == id)
                return &clip;
        }
    }
    return nullptr;
}

const TimelineClip *TimelineModel::clipById(const QString &id) const
{
    for (const TimelineTrack &track : m_tracks) {
        for (const TimelineClip &clip : track.clips) {
            if (clip.id == id)
                return &clip;
        }
    }
    return nullptr;
}

bool TimelineModel::canPlace(const TimelineTrack &track, const TimelineClip &clip) const
{
    for (const TimelineClip &other : track.clips) {
        if (other.id != clip.id && clip.start < other.start + other.duration
            && other.start < clip.start + clip.duration)
            return false;
    }
    return true;
}

QString TimelineModel::placeableTrackId(TimelineTrack *preferred, const TimelineClip &clip,
                                        const QString &assetKind)
{
    if (preferred && canPlace(*preferred, clip))
        return preferred->id;

    const bool audioAsset = assetKind == QStringLiteral("audio");
    const bool filterAsset = assetKind == QStringLiteral("filter");
    for (const TimelineTrack &candidate : m_tracks) {
        if (candidate.locked)
            continue;
        if (filterAsset) {
            if (candidate.kind != QStringLiteral("filter"))
                continue;
        } else if ((candidate.kind == QStringLiteral("audio")) != audioAsset) {
            continue;
        }
        if (canPlace(candidate, clip))
            return candidate.id;
    }
    // 所有同类轨道都被占用时，自动新建一条并放到新轨道上
    if (filterAsset)
        return addFilterTrack();
    return audioAsset ? addAudioTrack() : addTrack();
}

bool TimelineModel::compatible(const TimelineTrack &track, const QString &assetKind) const
{
    if (track.kind == QStringLiteral("audio")) {
        // 音频轨也接受视频素材，用于“分离音频”后仅播放声音。
        return assetKind == QStringLiteral("audio") || assetKind == QStringLiteral("video");
    }
    if (track.kind == QStringLiteral("filter"))
        return assetKind == QStringLiteral("filter");
    if (assetKind == QStringLiteral("filter"))
        return false;
    return assetKind == QStringLiteral("video") || assetKind == QStringLiteral("image")
        || assetKind == QStringLiteral("text") || assetKind == QStringLiteral("color");
}

QVariantMap TimelineModel::clipMap(const TimelineClip &clip) const
{
    return {
        {QStringLiteral("id"), clip.id}, {QStringLiteral("assetId"), clip.assetId},
        {QStringLiteral("trackId"), clip.trackId},
        {QStringLiteral("trackKind"), trackKind(clip.trackId)},
        {QStringLiteral("kind"), m_assets ? m_assets->kind(clip.assetId) : QString{}},
        {QStringLiteral("name"), m_assets ? m_assets->name(clip.assetId) : QString{}},
        {QStringLiteral("source"), m_assets ? m_assets->sourceUrl(clip.assetId) : QUrl{}},
        {QStringLiteral("text"), m_assets ? m_assets->text(clip.assetId) : QString{}},
        {QStringLiteral("start"), clip.start}, {QStringLiteral("sourceIn"), clip.sourceIn},
        {QStringLiteral("duration"), clip.duration}, {QStringLiteral("scale"), clip.scale},
        {QStringLiteral("x"), clip.x}, {QStringLiteral("y"), clip.y},
        {QStringLiteral("rotation"), clip.rotation}, {QStringLiteral("volume"), clip.volume},
        {QStringLiteral("opacity"), clip.opacity}, {QStringLiteral("cropLeft"), clip.cropLeft},
        {QStringLiteral("cropTop"), clip.cropTop}, {QStringLiteral("cropRight"), clip.cropRight},
        {QStringLiteral("cropBottom"), clip.cropBottom},
        {QStringLiteral("flipHorizontal"), clip.flipHorizontal},
        {QStringLiteral("flipVertical"), clip.flipVertical},
        {QStringLiteral("speed"), clip.speed},
        {QStringLiteral("reverse"), clip.reverse},
        {QStringLiteral("audioDetached"), clip.audioDetached},
        {QStringLiteral("linkedClipId"), clip.linkedClipId},
        {QStringLiteral("originalAssetId"), clip.originalAssetId},
        {QStringLiteral("colorFilter"), clip.colorFilter},
        {QStringLiteral("filterStrength"), clip.filterStrength},
        {QStringLiteral("locked"), clip.locked},
        {QStringLiteral("textFont"), clip.textFont},
        {QStringLiteral("textBold"), clip.textBold},
        {QStringLiteral("textItalic"), clip.textItalic},
        {QStringLiteral("textUnderline"), clip.textUnderline},
        {QStringLiteral("textStrikeout"), clip.textStrikeout},
        {QStringLiteral("textFillColor"), clip.textFillColor},
        {QStringLiteral("textStrokeColor"), clip.textStrokeColor},
        {QStringLiteral("textShadowColor"), clip.textShadowColor},
        {QStringLiteral("z"), trackZ(clip.trackId)}
    };
}

namespace {

struct FilterDef
{
    const char *name;
    const char *displayName;
    ColorFilterParams params;
};

// 颜色滤镜预设：亮度(-1..1)、对比度(0..3)、饱和度(0..3) + 3x3 颜色矩阵
const FilterDef kFilters[] = {
    {"", QT_TRANSLATE_NOOP("Filters", "无"), {0.0, 1.0, 1.0, {1,0,0, 0,1,0, 0,0,1}}},
    {"grayscale", QT_TRANSLATE_NOOP("Filters", "灰度"), {0.0, 1.0, 0.0, {1,0,0, 0,1,0, 0,0,1}}},
    {"oldfilm", QT_TRANSLATE_NOOP("Filters", "老电影"), {-0.05, 1.1, 0.4, {0.393,0.769,0.189, 0.349,0.686,0.168, 0.272,0.534,0.131}}},
    {"mono_red", QT_TRANSLATE_NOOP("Filters", "纯色红"), {0.0, 1.0, 0.0, {0.299,0.587,0.114, 0,0,0, 0,0,0}}},
    {"mono_green", QT_TRANSLATE_NOOP("Filters", "纯色绿"), {0.0, 1.0, 0.0, {0,0,0, 0.299,0.587,0.114, 0,0,0}}},
    {"mono_blue", QT_TRANSLATE_NOOP("Filters", "纯色蓝"), {0.0, 1.0, 0.0, {0,0,0, 0,0,0, 0.299,0.587,0.114}}},
    // 单色：保留指定色相，其余转为灰度
    {"keep_red", QT_TRANSLATE_NOOP("Filters", "单色红"), {0.0, 1.0, 1.0, {1,0,0, 0,1,0, 0,0,1}, 0.0, 0.35}},
    {"keep_green", QT_TRANSLATE_NOOP("Filters", "单色绿"), {0.0, 1.0, 1.0, {1,0,0, 0,1,0, 0,0,1}, 1.0 / 3.0, 0.35}},
    {"keep_blue", QT_TRANSLATE_NOOP("Filters", "单色蓝"), {0.0, 1.0, 1.0, {1,0,0, 0,1,0, 0,0,1}, 2.0 / 3.0, 0.35}},
    {"sunny", QT_TRANSLATE_NOOP("Filters", "晴朗"), {0.05, 1.2, 1.6, {1.05,0,0, 0,1,0, 0,0,0.9}}},
    {"hazy", QT_TRANSLATE_NOOP("Filters", "阴霾"), {0.05, 0.8, 0.7, {0.95,0,0, 0,1,0, 0,0,1.1}}},
    {"vintage", QT_TRANSLATE_NOOP("Filters", "复古"), {0.0, 0.9, 0.6, {0.45,0.7,0.15, 0.3,0.6,0.1, 0.2,0.4,0.15}}},
    {"autumn", QT_TRANSLATE_NOOP("Filters", "秋天"), {0.0, 1.1, 1.5, {1.1,0,0, 0,0.95,0, 0,0,0.7}}},
    {"wongkarwai", QT_TRANSLATE_NOOP("Filters", "王家卫港风"), {0.0, 1.3, 1.4, {1.1,0.05,0, 0,1.0,0, 0.05,0,0.85}}},
    {"sunset", QT_TRANSLATE_NOOP("Filters", "夕阳"), {0.05, 0.85, 1.2, {1.15,0,0, 0,0.85,0, 0,0,0.6}}},
    {"vibrant", QT_TRANSLATE_NOOP("Filters", "元气"), {0.02, 1.25, 1.8, {1,0,0, 0,1,0, 0,0,1}}}
};

const FilterDef *findFilter(const QString &name)
{
    for (const FilterDef &f : kFilters)
        if (name == QLatin1String(f.name))
            return &f;
    return nullptr;
}

} // namespace

ColorFilterParams TimelineModel::colorFilterParams(const QString &name)
{
    const FilterDef *f = findFilter(name);
    return f ? f->params : ColorFilterParams{};
}

QString TimelineModel::colorFilterFfmpeg(const QString &name, double strength)
{
    const FilterDef *f = findFilter(name);
    if (!f)
        return {};
    ColorFilterParams p = f->params;
    const double s = qBound(0.0, strength, 1.0);
    // 强度通过把滤镜参数向“原始画面”插值实现，保证预览与导出一致
    if (s < 0.999) {
        p.brightness *= s;
        p.contrast = 1.0 + (p.contrast - 1.0) * s;
        p.saturation = 1.0 + (p.saturation - 1.0) * s;
        const double identity[9] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
        for (int i = 0; i < 9; ++i)
            p.matrix[i] = identity[i] + (p.matrix[i] - identity[i]) * s;
    }
    QStringList parts;
    const bool hasEq = !qFuzzyIsNull(p.brightness) || !qFuzzyCompare(p.contrast, 1.0)
        || !qFuzzyCompare(p.saturation, 1.0);
    if (hasEq) {
        parts << QStringLiteral("eq=brightness=%1:contrast=%2:saturation=%3")
                         .arg(p.brightness, 0, 'f', 3)
                         .arg(p.contrast, 0, 'f', 3)
                         .arg(p.saturation, 0, 'f', 3);
    }
    const bool identity = qFuzzyCompare(p.matrix[0], 1.0) && qFuzzyIsNull(p.matrix[1])
        && qFuzzyIsNull(p.matrix[2]) && qFuzzyIsNull(p.matrix[3])
        && qFuzzyCompare(p.matrix[4], 1.0) && qFuzzyIsNull(p.matrix[5])
        && qFuzzyIsNull(p.matrix[6]) && qFuzzyIsNull(p.matrix[7])
        && qFuzzyCompare(p.matrix[8], 1.0);
    if (!identity) {
        parts << QStringLiteral("colorchannelmixer=rr=%1:rg=%2:rb=%3:gr=%4:gg=%5:gb=%6:br=%7:bg=%8:bb=%9")
                         .arg(p.matrix[0], 0, 'f', 3).arg(p.matrix[1], 0, 'f', 3).arg(p.matrix[2], 0, 'f', 3)
                         .arg(p.matrix[3], 0, 'f', 3).arg(p.matrix[4], 0, 'f', 3).arg(p.matrix[5], 0, 'f', 3)
                         .arg(p.matrix[6], 0, 'f', 3).arg(p.matrix[7], 0, 'f', 3).arg(p.matrix[8], 0, 'f', 3);
    }
    if (p.keepHue >= 0.0) {
        const char *color = p.keepHue < 1.0 / 6.0 ? "red"
                          : p.keepHue < 0.5 ? "green" : "blue";
        parts << QStringLiteral("colorhold=color=%1:similarity=%2:blend=%3")
                         .arg(QLatin1String(color))
                         .arg(p.keepSimilarity, 0, 'f', 3)
                         .arg(1.0 - s, 0, 'f', 3);
    }
    return parts.join(',');
}

QStringList TimelineModel::colorFilterNames()
{
    QStringList result;
    for (const FilterDef &f : kFilters)
        result << QString::fromLatin1(f.name);
    return result;
}

QString TimelineModel::colorFilterDisplayName(const QString &name)
{
    const FilterDef *f = findFilter(name);
    if (!f)
        return TimelineModel::tr("无");
    return QCoreApplication::translate("Filters", f->displayName);
}

void TimelineModel::emitStructureSignals()
{
    emit countChanged();
    emit durationChanged();
    emit selectedClipChanged();
    emit timelineChanged();
}
