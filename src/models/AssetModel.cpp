#include "models/AssetModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QSet>
#include <QUuid>
#include <QCoreApplication>

AssetModel::AssetModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_probe(this)
{
    connect(&m_probe, &MediaProbe::finished, this,
            [this](const QString &id, qint64 durationMs) {
                const int row = indexOf(id);
                if (row < 0 || durationMs <= 0)
                    return;
                m_assets[row].duration = durationMs;
                const QModelIndex modelIndex = index(row);
                emit dataChanged(modelIndex, modelIndex, {DurationRole});
                emit assetsChanged();
            });
}

int AssetModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_assets.size());
}

QVariant AssetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_assets.size())
        return {};
    const Asset &asset = m_assets.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return asset.name;
    case IdRole: return asset.id;
    case KindRole: return asset.kind;
    case SourceRole: return sourceUrl(asset.id);
    case RelativeSourceRole: return asset.source;
    case DurationRole: return asset.duration;
    case MissingRole: return asset.missing;
    case TextRole: return asset.text;
    case ColorRole: return asset.color;
    case FavoriteRole: return asset.favorite;
    case FilterNameRole: return asset.filterName;
    default: return {};
    }
}

QHash<int, QByteArray> AssetModel::roleNames() const
{
    return {
        {IdRole, "id"}, {KindRole, "kind"}, {NameRole, "name"},
        {SourceRole, "source"}, {RelativeSourceRole, "relativeSource"},
        {DurationRole, "duration"}, {MissingRole, "missing"}, {TextRole, "text"},
        {ColorRole, "color"}, {FavoriteRole, "favorite"},
        {FilterNameRole, "filterName"}
    };
}

void AssetModel::setDraftDirectory(const QString &directory)
{
    const QString cleanDirectory = directory.isEmpty() ? QString{} : QDir::cleanPath(directory);
    if (m_draftDirectory == cleanDirectory)
        return;
    m_draftDirectory = cleanDirectory;
    if (!m_draftDirectory.isEmpty())
        QDir(m_draftDirectory).mkpath(QStringLiteral("media"));
    for (Asset &asset : m_assets)
        asset.missing = !asset.preset && asset.kind != QStringLiteral("text")
            && asset.kind != QStringLiteral("color")
            && asset.kind != QStringLiteral("filter")
            && !QFileInfo::exists(absoluteSource(asset.id));
    emit draftDirectoryChanged();
    if (!m_assets.isEmpty()) {
        emit dataChanged(index(0), index(m_assets.size() - 1),
                         {SourceRole, RelativeSourceRole, MissingRole});
    }
    emit assetsChanged();
}

QString AssetModel::importMedia(const QUrl &source)
{
    if (m_draftDirectory.isEmpty()) {
        setLastError(QStringLiteral("Create or open a draft before importing media."));
        return {};
    }
    const QString path = source.toLocalFile();
    const QFileInfo file(path);
    if (!source.isLocalFile() || !file.isFile()) {
        setLastError(QStringLiteral("The selected media file does not exist."));
        return {};
    }
    const QString mediaKind = detectKind(file.fileName());
    if (mediaKind.isEmpty()) {
        setLastError(QStringLiteral("Unsupported media type."));
        return {};
    }

    Asset asset;
    asset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    asset.kind = mediaKind;
    asset.name = file.completeBaseName();
    asset.duration = mediaKind == QStringLiteral("image") ? 5000 : 0;
    if (!copyMedia(source, asset.id, &asset.source))
        return {};

    beginInsertRows(QModelIndex(), m_assets.size(), m_assets.size());
    m_assets.append(asset);
    endInsertRows();
    emit countChanged();
    emit assetsChanged();
    setLastError({});
    if (mediaKind == QStringLiteral("video") || mediaKind == QStringLiteral("audio"))
        m_probe.probe(asset.id, absoluteSource(asset.id));
    return asset.id;
}

QString AssetModel::addText(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Text cannot be empty."));
        return {};
    }
    Asset asset;
    asset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    asset.kind = QStringLiteral("text");
    asset.name = text.trimmed().left(40);
    asset.duration = 5000;
    asset.text = text;

    beginInsertRows(QModelIndex(), m_assets.size(), m_assets.size());
    m_assets.append(asset);
    endInsertRows();
    emit countChanged();
    emit assetsChanged();
    setLastError({});
    return asset.id;
}

bool AssetModel::relocate(const QString &id, const QUrl &source)
{
    const int row = indexOf(id);
    if (row < 0 || !source.isLocalFile() || !QFileInfo(source.toLocalFile()).isFile()) {
        setLastError(QStringLiteral("The replacement file does not exist."));
        return false;
    }
    Asset &asset = m_assets[row];
    if (asset.kind == QStringLiteral("text") || detectKind(source.toLocalFile()) != asset.kind) {
        setLastError(QStringLiteral("The replacement must have the same media type."));
        return false;
    }
    QString relativePath;
    if (!copyMedia(source, id, &relativePath))
        return false;
    asset.source = relativePath;
    asset.missing = false;
    emit dataChanged(index(row), index(row),
                     {SourceRole, RelativeSourceRole, MissingRole, DurationRole});
    emit assetsChanged();
    setLastError({});
    if (asset.kind == QStringLiteral("video") || asset.kind == QStringLiteral("audio"))
        m_probe.probe(asset.id, absoluteSource(asset.id));
    return true;
}

bool AssetModel::removeAsset(const QString &id)
{
    const int row = indexOf(id);
    if (row < 0)
        return false;
    const QString path = absoluteSource(id);
    beginRemoveRows(QModelIndex(), row, row);
    m_assets.removeAt(row);
    endRemoveRows();
    if (!path.isEmpty() && QFile::exists(path))
        QFile::remove(path);
    emit countChanged();
    emit assetsChanged();
    return true;
}

void AssetModel::refreshMissing()
{
    bool changed = false;
    for (int row = 0; row < m_assets.size(); ++row) {
        Asset &asset = m_assets[row];
        const bool isMissing = !asset.preset && asset.kind != QStringLiteral("text")
            && asset.kind != QStringLiteral("color")
            && asset.kind != QStringLiteral("filter")
            && !QFileInfo(absoluteSource(asset.id)).isFile();
        if (asset.missing != isMissing) {
            asset.missing = isMissing;
            emit dataChanged(index(row), index(row), {MissingRole});
            changed = true;
        }
    }
    if (changed)
        emit assetsChanged();
}

QVariantMap AssetModel::get(const QString &id) const
{
    const int row = indexOf(id);
    return row < 0 ? QVariantMap{} : assetMap(m_assets.at(row));
}

QString AssetModel::kind(const QString &id) const
{
    const int row = indexOf(id);
    return row < 0 ? QString{} : m_assets.at(row).kind;
}

QString AssetModel::name(const QString &id) const
{
    const int row = indexOf(id);
    return row < 0 ? QString{} : m_assets.at(row).name;
}

QString AssetModel::text(const QString &id) const
{
    const int row = indexOf(id);
    return row < 0 ? QString{} : m_assets.at(row).text;
}

QString AssetModel::color(const QString &id) const
{
    const int row = indexOf(id);
    return row < 0 ? QString{} : m_assets.at(row).color;
}

QString AssetModel::filterName(const QString &id) const
{
    const int row = indexOf(id);
    return row < 0 ? QString{} : m_assets.at(row).filterName;
}

void AssetModel::toggleFavorite(const QString &id)
{
    const int row = indexOf(id);
    if (row < 0)
        return;
    m_assets[row].favorite = !m_assets[row].favorite;
    emit dataChanged(index(row), index(row), {FavoriteRole});
    emit assetsChanged();
}

void AssetModel::ensurePresets()
{
    struct Preset {
        const char *id;
        const char *name;
        const char *color;
        const char *filterName;
    };
    // 纯色背景预设
    static const Preset colorPresets[] = {
        {"preset-black", "黑色背景", "#000000", nullptr},
        {"preset-white", "白色背景", "#FFFFFF", nullptr}
    };
    // 颜色滤镜预设（与 TimelineModel::kFilters 保持一致）
    static const Preset filterPresets[] = {
        {"preset-filter-none", QT_TRANSLATE_NOOP("Filters", "无"), nullptr, ""},
        {"preset-filter-grayscale", QT_TRANSLATE_NOOP("Filters", "灰度"), nullptr, "grayscale"},
        {"preset-filter-oldfilm", QT_TRANSLATE_NOOP("Filters", "老电影"), nullptr, "oldfilm"},
        {"preset-filter-mono_red", QT_TRANSLATE_NOOP("Filters", "纯色红"), nullptr, "mono_red"},
        {"preset-filter-mono_green", QT_TRANSLATE_NOOP("Filters", "纯色绿"), nullptr, "mono_green"},
        {"preset-filter-mono_blue", QT_TRANSLATE_NOOP("Filters", "纯色蓝"), nullptr, "mono_blue"},
        {"preset-filter-keep-red", QT_TRANSLATE_NOOP("Filters", "单色红"), nullptr, "keep_red"},
        {"preset-filter-keep-green", QT_TRANSLATE_NOOP("Filters", "单色绿"), nullptr, "keep_green"},
        {"preset-filter-keep-blue", QT_TRANSLATE_NOOP("Filters", "单色蓝"), nullptr, "keep_blue"},
        {"preset-filter-sunny", QT_TRANSLATE_NOOP("Filters", "晴朗"), nullptr, "sunny"},
        {"preset-filter-hazy", QT_TRANSLATE_NOOP("Filters", "阴霾"), nullptr, "hazy"},
        {"preset-filter-vintage", QT_TRANSLATE_NOOP("Filters", "复古"), nullptr, "vintage"},
        {"preset-filter-autumn", QT_TRANSLATE_NOOP("Filters", "秋天"), nullptr, "autumn"},
        {"preset-filter-wongkarwai", QT_TRANSLATE_NOOP("Filters", "王家卫港风"), nullptr, "wongkarwai"},
        {"preset-filter-sunset", QT_TRANSLATE_NOOP("Filters", "夕阳"), nullptr, "sunset"},
        {"preset-filter-vibrant", QT_TRANSLATE_NOOP("Filters", "元气"), nullptr, "vibrant"}
    };

    QList<Asset> additions;
    auto addPreset = [this, &additions](const Preset &preset, const char *kind) {
        const QString id = QString::fromLatin1(preset.id);
        if (indexOf(id) >= 0)
            return;
        Asset asset;
        asset.id = id;
        asset.kind = QString::fromLatin1(kind);
        asset.name = QCoreApplication::translate("Filters", preset.name);
        asset.duration = 5000;
        asset.preset = true;
        if (preset.color)
            asset.color = QString::fromLatin1(preset.color);
        if (preset.filterName)
            asset.filterName = QString::fromLatin1(preset.filterName);
        additions.append(asset);
    };
    for (const Preset &preset : colorPresets)
        addPreset(preset, "color");
    for (const Preset &preset : filterPresets)
        addPreset(preset, "filter");

    if (additions.isEmpty())
        return;

    beginInsertRows(QModelIndex(), m_assets.size(), m_assets.size() + additions.size() - 1);
    m_assets.append(additions);
    endInsertRows();
    emit countChanged();
    emit assetsChanged();
}

QString AssetModel::relativeSource(const QString &id) const
{
    const int row = indexOf(id);
    return row < 0 ? QString{} : m_assets.at(row).source;
}

QString AssetModel::absoluteSource(const QString &id) const
{
    const int row = indexOf(id);
    if (row < 0 || m_assets.at(row).source.isEmpty())
        return {};
    const QString source = m_assets.at(row).source;
    return QDir::isAbsolutePath(source) ? QDir::cleanPath(source)
                                       : QDir(m_draftDirectory).absoluteFilePath(source);
}

QUrl AssetModel::sourceUrl(const QString &id) const
{
    const QString path = absoluteSource(id);
    return path.isEmpty() ? QUrl{} : QUrl::fromLocalFile(path);
}

qint64 AssetModel::duration(const QString &id) const
{
    const int row = indexOf(id);
    return row < 0 ? 0 : m_assets.at(row).duration;
}

bool AssetModel::missing(const QString &id) const
{
    const int row = indexOf(id);
    return row < 0 || m_assets.at(row).missing;
}

QJsonArray AssetModel::toJson() const
{
    QJsonArray result;
    for (const Asset &asset : m_assets) {
        if (asset.preset)
            continue;
        result.append(QJsonObject{
            {QStringLiteral("id"), asset.id},
            {QStringLiteral("kind"), asset.kind},
            {QStringLiteral("name"), asset.name},
            {QStringLiteral("source"), asset.source},
            {QStringLiteral("duration"), asset.duration},
            {QStringLiteral("text"), asset.text},
            {QStringLiteral("color"), asset.color},
            {QStringLiteral("favorite"), asset.favorite},
            {QStringLiteral("filterName"), asset.filterName}
        });
    }
    return result;
}

bool AssetModel::fromJson(const QJsonArray &array, QString *error)
{
    QList<Asset> parsed;
    QSet<QString> ids;
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        Asset asset;
        asset.id = object.value(QStringLiteral("id")).toString();
        asset.kind = object.value(QStringLiteral("kind")).toString();
        asset.name = object.value(QStringLiteral("name")).toString();
        asset.source = object.value(QStringLiteral("source")).toString();
        asset.duration = qMax<qint64>(0, object.value(QStringLiteral("duration")).toInteger());
        asset.text = object.value(QStringLiteral("text")).toString();
        asset.color = object.value(QStringLiteral("color")).toString();
        asset.favorite = object.value(QStringLiteral("favorite")).toBool();
        asset.filterName = object.value(QStringLiteral("filterName")).toString();
        const bool validKind = asset.kind == QStringLiteral("image")
            || asset.kind == QStringLiteral("video") || asset.kind == QStringLiteral("audio")
            || asset.kind == QStringLiteral("text") || asset.kind == QStringLiteral("color")
            || asset.kind == QStringLiteral("filter");
        const QString cleanSource = QDir::cleanPath(asset.source);
        const bool invalidSource = asset.kind != QStringLiteral("text")
            && asset.kind != QStringLiteral("color")
            && asset.kind != QStringLiteral("filter")
            && (asset.source.isEmpty() || QDir::isAbsolutePath(asset.source)
                || cleanSource == QStringLiteral("..") || cleanSource.startsWith(QStringLiteral("../")));
        if (asset.id.isEmpty() || ids.contains(asset.id) || !validKind || invalidSource) {
            if (error)
                *error = QStringLiteral("The draft contains an invalid asset.");
            return false;
        }
        ids.insert(asset.id);
        asset.missing = asset.kind != QStringLiteral("text")
            && asset.kind != QStringLiteral("color")
            && asset.kind != QStringLiteral("filter")
            && !QFileInfo(QDir(m_draftDirectory).absoluteFilePath(asset.source)).isFile();
        parsed.append(asset);
    }

    beginResetModel();
    m_assets = parsed;
    endResetModel();
    emit countChanged();
    emit assetsChanged();
    ensurePresets();
    return true;
}

int AssetModel::indexOf(const QString &id) const
{
    for (int i = 0; i < m_assets.size(); ++i) {
        if (m_assets.at(i).id == id)
            return i;
    }
    return -1;
}

QVariantMap AssetModel::assetMap(const Asset &asset) const
{
    return {
        {QStringLiteral("id"), asset.id}, {QStringLiteral("kind"), asset.kind},
        {QStringLiteral("name"), asset.name}, {QStringLiteral("source"), sourceUrl(asset.id)},
        {QStringLiteral("relativeSource"), asset.source},
        {QStringLiteral("duration"), asset.duration}, {QStringLiteral("missing"), asset.missing},
        {QStringLiteral("text"), asset.text}, {QStringLiteral("color"), asset.color},
        {QStringLiteral("favorite"), asset.favorite},
        {QStringLiteral("filterName"), asset.filterName}
    };
}

bool AssetModel::copyMedia(const QUrl &source, const QString &id, QString *relativePath)
{
    const QFileInfo input(source.toLocalFile());
    if (!QDir(m_draftDirectory).mkpath(QStringLiteral("media"))) {
        setLastError(QStringLiteral("Cannot create the draft media directory."));
        return false;
    }
    const QString fileName = id + (input.suffix().isEmpty()
        ? QString{} : QStringLiteral(".") + input.suffix().toLower());
    const QString destination = QDir(m_draftDirectory).absoluteFilePath(QStringLiteral("media/") + fileName);
    if (QFileInfo::exists(destination) && !QFile::remove(destination)) {
        setLastError(QStringLiteral("Cannot replace the existing draft media file."));
        return false;
    }
    if (!QFile::copy(input.absoluteFilePath(), destination)) {
        setLastError(QStringLiteral("Cannot copy media into the draft."));
        return false;
    }
    *relativePath = QStringLiteral("media/") + fileName;
    return true;
}

void AssetModel::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

QString AssetModel::detectKind(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    static const QSet<QString> images = {QStringLiteral("png"), QStringLiteral("jpg"),
        QStringLiteral("jpeg"), QStringLiteral("webp"), QStringLiteral("bmp"), QStringLiteral("gif")};
    static const QSet<QString> videos = {QStringLiteral("mp4"), QStringLiteral("mov"),
        QStringLiteral("mkv"), QStringLiteral("webm"), QStringLiteral("avi"), QStringLiteral("m4v")};
    static const QSet<QString> audio = {QStringLiteral("mp3"), QStringLiteral("wav"),
        QStringLiteral("flac"), QStringLiteral("ogg"), QStringLiteral("m4a"),
        QStringLiteral("aac"), QStringLiteral("opus")};
    if (images.contains(suffix))
        return QStringLiteral("image");
    if (videos.contains(suffix))
        return QStringLiteral("video");
    if (audio.contains(suffix))
        return QStringLiteral("audio");
    return {};
}
