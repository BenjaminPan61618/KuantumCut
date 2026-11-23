#include "core/DraftRepository.h"

#include "models/AssetModel.h"
#include "models/TimelineModel.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMediaPlayer>
#include <QSaveFile>
#include <QSize>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVideoFrame>
#include <QVideoSink>
#include <algorithm>
#include <functional>

// 用 Qt Multimedia 抓取草稿首个视频素材的一帧，缓存为草稿目录下的 thumbnail.png
class ThumbnailService : public QObject
{
public:
    explicit ThumbnailService(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_player.setVideoSink(&m_sink);
        connect(&m_sink, &QVideoSink::videoFrameChanged, this,
                [this](const QVideoFrame &frame) { capture(frame); });
        connect(&m_player, &QMediaPlayer::mediaStatusChanged, this,
                [this](QMediaPlayer::MediaStatus status) {
                    if (!m_running)
                        return;
                    if (status == QMediaPlayer::LoadedMedia)
                        m_player.play();
                    else if (status == QMediaPlayer::InvalidMedia || status == QMediaPlayer::NoMedia)
                        finish(false);
                });
        m_timeout.setSingleShot(true);
        m_timeout.setInterval(5000);
        connect(&m_timeout, &QTimer::timeout, this, [this]() { finish(false); });
    }

    void enqueue(const QString &id, const QString &sourcePath, const QString &targetPath)
    {
        if (!QFileInfo::exists(sourcePath))
            return;
        m_queue.append({id, sourcePath, targetPath});
        if (!m_running)
            process();
    }

    std::function<void(const QString &)> ready;

private:
    struct Job {
        QString id;
        QString source;
        QString target;
    };

    void process()
    {
        while (!m_queue.isEmpty()) {
            const Job job = m_queue.takeFirst();
            if (QFileInfo::exists(job.target))
                continue;
            m_running = true;
            m_currentId = job.id;
            m_currentTarget = job.target;
            m_timeout.start();
            m_player.setSource(QUrl::fromLocalFile(job.source));
            return;
        }
    }

    void capture(const QVideoFrame &frame)
    {
        if (!m_running || !frame.isValid())
            return;
        const QImage image = frame.toImage();
        if (image.isNull())
            return;
        const QImage scaled = image.scaledToWidth(320, Qt::SmoothTransformation);
        finish(scaled.save(m_currentTarget, "PNG"));
    }

    void finish(bool saved)
    {
        if (!m_running)
            return;
        m_running = false;
        m_timeout.stop();
        m_player.stop();
        m_player.setSource(QUrl());
        const QString id = m_currentId;
        m_currentId.clear();
        m_currentTarget.clear();
        if (saved && ready)
            ready(id);
        QTimer::singleShot(0, this, [this]() { process(); });
    }

    QMediaPlayer m_player;
    QVideoSink m_sink;
    QTimer m_timeout;
    QList<Job> m_queue;
    bool m_running = false;
    QString m_currentId;
    QString m_currentTarget;
};

DraftRepository::DraftRepository(QObject *parent)
    : DraftRepository(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                          + QStringLiteral("/drafts"), parent)
{
}

DraftRepository::DraftRepository(const QString &rootPath, QObject *parent)
    : QAbstractListModel(parent)
    , m_rootPath(QDir::cleanPath(rootPath))
{
    QDir().mkpath(m_rootPath);
    m_scanPool.setMaxThreadCount(1);
    refresh();
}

DraftRepository::~DraftRepository()
{
    m_closing = true;
    m_scanPool.waitForDone();
}

int DraftRepository::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant DraftRepository::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return entry.name;
    case IdRole: return entry.id;
    case PathRole: return entry.path;
    case ModifiedRole: return entry.modified;
    case ResolutionRole: return QSize(entry.width, entry.height);
    case ThumbnailSourceRole: {
        if (entry.thumbnailKind == QStringLiteral("image") && !entry.thumbnailSource.isEmpty())
            return QUrl::fromLocalFile(entry.thumbnailSource);
        if (entry.thumbnailKind == QStringLiteral("video")) {
            const QString poster = QDir(draftDirectory(entry.id))
                                       .absoluteFilePath(QStringLiteral("thumbnail.png"));
            if (QFileInfo::exists(poster))
                return QUrl::fromLocalFile(poster);
        }
        return QUrl();
    }
    case ThumbnailKindRole: return entry.thumbnailKind;
    case ThumbnailColorRole: return entry.thumbnailColor;
    default: return {};
    }
}

QHash<int, QByteArray> DraftRepository::roleNames() const
{
    return {{IdRole, "id"}, {NameRole, "name"}, {PathRole, "path"},
            {ModifiedRole, "modified"}, {ResolutionRole, "resolution"},
            {ThumbnailSourceRole, "thumbnailSource"}, {ThumbnailKindRole, "thumbnailKind"},
            {ThumbnailColorRole, "thumbnailColor"}};
}

QVariantMap DraftRepository::create(const QString &name, int width, int height)
{
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty() || width <= 0 || height <= 0) {
        setLastError(QStringLiteral("A draft name and valid resolution are required."));
        return {};
    }

    Draft draft;
    draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    draft.name = cleanName;
    draft.width = width;
    draft.height = height;
    AssetModel assets;
    TimelineModel timeline(&assets);
    assets.setDraftDirectory(draftDirectory(draft.id));
    QString error;
    if (!save(draft, assets, timeline, &error)) {
        setLastError(error);
        return {};
    }

    const QString path = draftFilePath(draft.id);
    return {{QStringLiteral("id"), draft.id}, {QStringLiteral("name"), draft.name},
            {QStringLiteral("path"), path}, {QStringLiteral("modified"), QFileInfo(path).lastModified()},
            {QStringLiteral("resolution"), QSize(width, height)}};
}

QJsonObject DraftRepository::open(const QString &id)
{
    if (!validId(id)) {
        setLastError(QStringLiteral("Invalid draft id."));
        return {};
    }
    QFile file(draftFilePath(id));
    if (!file.open(QIODevice::ReadOnly)) {
        setLastError(QStringLiteral("Cannot open the draft file."));
        return {};
    }
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setLastError(QStringLiteral("The draft JSON is invalid: ") + parseError.errorString());
        return {};
    }
    setLastError({});
    return document.object();
}

bool DraftRepository::rename(const QString &id, const QString &name)
{
    const QString cleanName = name.trimmed();
    if (!validId(id) || cleanName.isEmpty()) {
        setLastError(QStringLiteral("Invalid draft id or empty name."));
        return false;
    }
    QJsonObject object = open(id);
    if (object.isEmpty())
        return false;
    object[QStringLiteral("name")] = cleanName;
    QString error;
    if (!writeJson(id, object, &error)) {
        setLastError(error);
        return false;
    }
    setLastError({});
    refresh();
    return true;
}

bool DraftRepository::deleteDraft(const QString &id)
{
    if (!validId(id)) {
        setLastError(QStringLiteral("Invalid draft id."));
        return false;
    }
    if (!QDir(draftDirectory(id)).removeRecursively()) {
        setLastError(QStringLiteral("Cannot delete the draft directory."));
        return false;
    }
    setLastError({});
    refresh();
    return true;
}

QStringList DraftRepository::draftIds() const
{
    QStringList ids;
    ids.reserve(m_entries.size());
    for (const Entry &entry : m_entries)
        ids.append(entry.id);
    return ids;
}

int DraftRepository::deleteDrafts(const QStringList &ids)
{
    int removed = 0;
    for (const QString &id : ids) {
        if (!validId(id))
            continue;
        if (QDir(draftDirectory(id)).removeRecursively())
            ++removed;
    }
    if (removed > 0)
        setLastError({});
    else
        setLastError(QStringLiteral("Cannot delete the draft directory."));
    refresh();
    return removed;
}

void DraftRepository::refresh()
{
    const quint64 generation = ++m_refreshGeneration;
    const QString rootPath = m_rootPath;
    m_scanPool.start([this, rootPath, generation]() {
        QList<Entry> entries;
        const QDir root(rootPath);
        const QFileInfoList directories = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                              QDir::Name);
        for (const QFileInfo &directory : directories) {
            const QString id = directory.fileName();
            const QString path = QDir(rootPath).absoluteFilePath(
                id + QStringLiteral("/draft.kcdraft"));
            if (!validId(id) || !QFileInfo(path).isFile())
                continue;
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                continue;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
            const QJsonObject object = document.object();
            if (object.value(QStringLiteral("format")).toString() != QStringLiteral("KuantumCutDraft"))
                continue;
            const QJsonObject canvas = object.value(QStringLiteral("canvas")).toObject();
            Entry entry;
            entry.id = id;
            entry.name = object.value(QStringLiteral("name")).toString(id);
            entry.path = path;
            entry.modified = QFileInfo(path).lastModified();
            entry.width = canvas.value(QStringLiteral("width")).toInt();
            entry.height = canvas.value(QStringLiteral("height")).toInt();
            const QJsonArray assets = object.value(QStringLiteral("assets")).toArray();
            for (const QJsonValue &value : assets) {
                const QJsonObject asset = value.toObject();
                const QString kind = asset.value(QStringLiteral("kind")).toString();
                if (kind == QStringLiteral("audio"))
                    continue;
                entry.thumbnailKind = kind;
                if (kind == QStringLiteral("color")) {
                    entry.thumbnailColor = asset.value(QStringLiteral("color")).toString();
                } else if (kind == QStringLiteral("image") || kind == QStringLiteral("video")) {
                    const QString source = asset.value(QStringLiteral("source")).toString();
                    if (!source.isEmpty())
                        entry.thumbnailSource = QDir(directory.absoluteFilePath())
                                                    .absoluteFilePath(source);
                }
                break;
            }
            entries.append(entry);
        }
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
            return a.modified > b.modified;
        });
        if (m_closing.load())
            return;
        QMetaObject::invokeMethod(this, [this, generation, entries = std::move(entries)]() mutable {
            if (m_closing.load() || generation != m_refreshGeneration)
                return;
            beginResetModel();
            m_entries = std::move(entries);
            endResetModel();
            emit countChanged();
            requestThumbnails();
        }, Qt::QueuedConnection);
    });
}

bool DraftRepository::load(const QString &id, Draft &draft, AssetModel &assets,
                           TimelineModel &timeline, QString *error)
{
    const QJsonObject object = open(id);
    if (object.isEmpty()) {
        if (error)
            *error = m_lastError;
        return false;
    }
    if (object.value(QStringLiteral("id")).toString() != id) {
        if (error)
            *error = QStringLiteral("Draft id does not match its directory.");
        return false;
    }
    assets.setDraftDirectory(draftDirectory(id));
    if (!draft.fromJson(object, assets, timeline, error))
        return false;
    assets.refreshMissing();
    return true;
}

bool DraftRepository::save(const Draft &draft, const AssetModel &assets,
                           const TimelineModel &timeline, QString *error)
{
    if (!validId(draft.id) || draft.name.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("Cannot save an invalid draft.");
        return false;
    }
    const QJsonObject object = draft.toJson(assets, timeline);
    if (!writeJson(draft.id, object, error))
        return false;
    refresh();
    return true;
}

QString DraftRepository::draftDirectory(const QString &id) const
{
    return QDir(m_rootPath).absoluteFilePath(id);
}

QString DraftRepository::draftFilePath(const QString &id) const
{
    return QDir(draftDirectory(id)).absoluteFilePath(QStringLiteral("draft.kcdraft"));
}

bool DraftRepository::validId(const QString &id)
{
    const QUuid uuid = QUuid::fromString(id);
    return !uuid.isNull() && uuid.toString(QUuid::WithoutBraces) == id.toLower();
}

bool DraftRepository::writeJson(const QString &id, const QJsonObject &object, QString *error)
{
    if (!QDir().mkpath(draftDirectory(id))
        || !QDir(draftDirectory(id)).mkpath(QStringLiteral("media"))) {
        if (error)
            *error = QStringLiteral("Cannot create the draft directory.");
        return false;
    }
    QSaveFile file(draftFilePath(id));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

void DraftRepository::setLastError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void DraftRepository::requestThumbnails()
{
    for (int row = 0; row < m_entries.size(); ++row) {
        const Entry &entry = m_entries.at(row);
        if (entry.thumbnailKind != QStringLiteral("video") || entry.thumbnailSource.isEmpty())
            continue;
        const QString poster = QDir(draftDirectory(entry.id))
                                   .absoluteFilePath(QStringLiteral("thumbnail.png"));
        if (QFileInfo::exists(poster))
            continue;
        if (!m_thumbnailService) {
            m_thumbnailService = new ThumbnailService(this);
            m_thumbnailService->ready = [this](const QString &id) {
                for (int i = 0; i < m_entries.size(); ++i) {
                    if (m_entries.at(i).id != id)
                        continue;
                    const QModelIndex changed = index(i);
                    emit dataChanged(changed, changed, {ThumbnailSourceRole});
                    break;
                }
            };
        }
        m_thumbnailService->enqueue(entry.id, entry.thumbnailSource, poster);
    }
}

QVariantMap DraftRepository::entryMap(const Entry &entry) const
{
    return {{QStringLiteral("id"), entry.id}, {QStringLiteral("name"), entry.name},
            {QStringLiteral("path"), entry.path}, {QStringLiteral("modified"), entry.modified},
            {QStringLiteral("resolution"), QSize(entry.width, entry.height)}};
}
