#pragma once

#include "core/Draft.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QThreadPool>
#include <QVariantMap>
#include <atomic>

class AssetModel;
class TimelineModel;
class ThumbnailService;

class DraftRepository : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString rootPath READ rootPath CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        PathRole,
        ModifiedRole,
        ResolutionRole,
        ThumbnailSourceRole,
        ThumbnailKindRole,
        ThumbnailColorRole
    };
    Q_ENUM(Role)

    explicit DraftRepository(QObject *parent = nullptr);
    explicit DraftRepository(const QString &rootPath, QObject *parent = nullptr);
    ~DraftRepository() override;

    int count() const { return rowCount(); }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString rootPath() const { return m_rootPath; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE QVariantMap create(const QString &name, int width, int height);
    Q_INVOKABLE QJsonObject open(const QString &id);
    Q_INVOKABLE bool rename(const QString &id, const QString &name);
    Q_INVOKABLE bool deleteDraft(const QString &id);
    Q_INVOKABLE QStringList draftIds() const;
    Q_INVOKABLE int deleteDrafts(const QStringList &ids);
    Q_INVOKABLE void refresh();

    bool load(const QString &id, Draft &draft, AssetModel &assets,
              TimelineModel &timeline, QString *error = nullptr);
    bool save(const Draft &draft, const AssetModel &assets,
              const TimelineModel &timeline, QString *error = nullptr);
    QString draftDirectory(const QString &id) const;
    QString draftFilePath(const QString &id) const;

signals:
    void countChanged();
    void lastErrorChanged();

private:
    struct Entry {
        QString id;
        QString name;
        QString path;
        QDateTime modified;
        int width = 0;
        int height = 0;
        QString thumbnailSource;
        QString thumbnailKind;
        QString thumbnailColor;
    };

    static bool validId(const QString &id);
    bool writeJson(const QString &id, const QJsonObject &object, QString *error);
    void setLastError(const QString &message);
    void requestThumbnails();
    QVariantMap entryMap(const Entry &entry) const;

    QString m_rootPath;
    QList<Entry> m_entries;
    QString m_lastError;
    QThreadPool m_scanPool;
    ThumbnailService *m_thumbnailService = nullptr;
    std::atomic_bool m_closing = false;
    quint64 m_refreshGeneration = 0;
};
