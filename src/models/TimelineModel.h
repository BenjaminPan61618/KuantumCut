#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QVariantMap>

class AssetModel;

struct ColorFilterParams
{
    double brightness = 0.0;
    double contrast = 1.0;
    double saturation = 1.0;
    // 3x3 颜色矩阵（行优先，作用于 RGB）
    double matrix[9] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    // 单色滤镜：保留指定色相，其余转为灰度；keepHue < 0 表示不启用
    double keepHue = -1.0;
    // 色相容差（0..0.5），同时作为导出时 colorhold 的 similarity
    double keepSimilarity = 0.35;
};

struct TimelineClip
{
    QString id;
    QString assetId;
    QString trackId;
    qint64 start = 0;
    qint64 sourceIn = 0;
    qint64 duration = 0;
    double scale = 1.0;
    double x = 0.0;
    double y = 0.0;
    double rotation = 0.0;
    double volume = 1.0;
    double opacity = 1.0;
    double cropLeft = 0.0;
    double cropTop = 0.0;
    double cropRight = 0.0;
    double cropBottom = 0.0;
    bool flipHorizontal = false;
    bool flipVertical = false;
    double speed = 1.0;
    bool reverse = false;
    bool audioDetached = false;
    QString linkedClipId;
    QString originalAssetId;
    QString colorFilter;
    // 滤镜强度：0 表示原图，1 表示完整应用滤镜
    double filterStrength = 1.0;
    bool locked = false;
    // 文字样式（仅文字素材使用）
    QString textFont;
    bool textBold = false;
    bool textItalic = false;
    bool textUnderline = false;
    bool textStrikeout = false;
    QString textFillColor = QStringLiteral("#ffffff");
    QString textStrokeColor = QStringLiteral("#00000000");
    QString textShadowColor = QStringLiteral("#00000000");

    QJsonObject toJson() const;
    static TimelineClip fromJson(const QJsonObject &object);
};

struct TimelineTrack
{
    QString id;
    QString kind;
    QString name;
    int z = 0;
    QList<TimelineClip> clips;
    bool muted = false;
    bool locked = false;
};

class TimelineModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString selectedClipId READ selectedClipId WRITE setSelectedClipId NOTIFY selectedClipIdChanged)
    Q_PROPERTY(QVariantMap selectedClip READ selectedClip NOTIFY selectedClipChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double pixelsPerSecond READ pixelsPerSecond WRITE setPixelsPerSecond NOTIFY pixelsPerSecondChanged)
    Q_PROPERTY(QVariantList tracks READ tracksVariant NOTIFY tracksChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        AssetIdRole,
        TrackIdRole,
        TrackKindRole,
        KindRole,
        NameRole,
        SourceRole,
        TextRole,
        StartRole,
        SourceInRole,
        DurationRole,
        ScaleRole,
        XRole,
        YRole,
        RotationRole,
        VolumeRole,
        OpacityRole,
        CropLeftRole,
        CropTopRole,
        CropRightRole,
        CropBottomRole,
        FlipHorizontalRole,
        FlipVerticalRole,
        SpeedRole,
        ReverseRole,
        AudioDetachedRole,
        ZRole,
        ColorFilterRole,
        FilterStrengthRole,
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

    explicit TimelineModel(AssetModel *assets, QObject *parent = nullptr);

    int count() const { return rowCount(); }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString selectedClipId() const { return m_selectedClipId; }
    QVariantMap selectedClip() const;
    qint64 duration() const;
    double pixelsPerSecond() const { return m_pixelsPerSecond; }
    QVariantList tracksVariant() const;

    void setSelectedClipId(const QString &id);
    void setPixelsPerSecond(double value);

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
    Q_INVOKABLE bool setClipProperty(const QString &clipId, const QString &property,
                                     const QVariant &value);
    Q_INVOKABLE QVariantMap getClip(const QString &clipId) const;
    Q_INVOKABLE QVariantList clipsForTrack(const QString &trackId) const;
    Q_INVOKABLE bool replaceClipAsset(const QString &clipId, const QString &newAssetId);
    Q_INVOKABLE bool restoreClipAsset(const QString &clipId);
    Q_INVOKABLE QString linkedClipId(const QString &clipId) const;
    Q_INVOKABLE bool setTrackMuted(const QString &trackId, bool muted);
    Q_INVOKABLE bool setTrackLocked(const QString &trackId, bool locked);
    Q_INVOKABLE bool clearTrack(const QString &trackId);
    Q_INVOKABLE bool removeTrack(const QString &trackId);

    QList<TimelineClip> clips() const;
    QList<TimelineTrack> tracks() const { return m_tracks; }
    bool clip(const QString &clipId, TimelineClip *result) const;
    int trackZ(const QString &trackId) const;
    QString trackKind(const QString &trackId) const;
    bool trackMuted(const QString &trackId) const;
    bool trackLocked(const QString &trackId) const;
    QString activeFilterNameAt(qint64 position) const;
    double activeFilterStrengthAt(qint64 position) const;

    static ColorFilterParams colorFilterParams(const QString &name);
    static QString colorFilterFfmpeg(const QString &name, double strength = 1.0);
    Q_INVOKABLE static QStringList colorFilterNames();
    Q_INVOKABLE static QString colorFilterDisplayName(const QString &name);

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &object, QString *error = nullptr);

signals:
    void countChanged();
    void selectedClipIdChanged();
    void selectedClipChanged();
    void durationChanged();
    void pixelsPerSecondChanged();
    void tracksChanged();
    void timelineChanged();
    void clipChanged(const QString &clipId);

private:
    TimelineTrack *trackById(const QString &id);
    const TimelineTrack *trackById(const QString &id) const;
    TimelineClip *clipById(const QString &id);
    const TimelineClip *clipById(const QString &id) const;
    bool canPlace(const TimelineTrack &track, const TimelineClip &clip) const;
    bool compatible(const TimelineTrack &track, const QString &assetKind) const;
    // 返回可放置片段的目标轨道；都不行时自动新建同类轨道
    QString placeableTrackId(TimelineTrack *preferred, const TimelineClip &clip,
                             const QString &assetKind);
    QVariantMap clipMap(const TimelineClip &clip) const;
    void emitStructureSignals();

    AssetModel *m_assets;
    QList<TimelineTrack> m_tracks;
    QString m_selectedClipId;
    double m_pixelsPerSecond = 100.0;
};
