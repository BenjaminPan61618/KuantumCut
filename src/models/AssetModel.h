#pragma once

#include "core/MediaProbe.h"

#include <QAbstractListModel>
#include <QJsonArray>
#include <QList>
#include <QUrl>

class AssetModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString draftDirectory READ draftDirectory NOTIFY draftDirectoryChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        KindRole,
        NameRole,
        SourceRole,
        RelativeSourceRole,
        DurationRole,
        MissingRole,
        TextRole,
        ColorRole,
        FavoriteRole,
        FilterNameRole
    };
    Q_ENUM(Role)

    explicit AssetModel(QObject *parent = nullptr);

    int count() const { return rowCount(); }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString draftDirectory() const { return m_draftDirectory; }
    QString lastError() const { return m_lastError; }
    void setDraftDirectory(const QString &directory);

    Q_INVOKABLE QString importMedia(const QUrl &source);
    Q_INVOKABLE QString addText(const QString &text);
    Q_INVOKABLE bool relocate(const QString &id, const QUrl &source);
    Q_INVOKABLE bool removeAsset(const QString &id);
    Q_INVOKABLE void refreshMissing();
    Q_INVOKABLE QVariantMap get(const QString &id) const;
    Q_INVOKABLE void toggleFavorite(const QString &id);

    bool contains(const QString &id) const { return indexOf(id) >= 0; }
    QString kind(const QString &id) const;
    QString name(const QString &id) const;
    QString text(const QString &id) const;
    QString color(const QString &id) const;
    QString filterName(const QString &id) const;
    QString relativeSource(const QString &id) const;
    QString absoluteSource(const QString &id) const;
    QUrl sourceUrl(const QString &id) const;
    qint64 duration(const QString &id) const;
    bool missing(const QString &id) const;

    QJsonArray toJson() const;
    bool fromJson(const QJsonArray &array, QString *error = nullptr);

signals:
    void countChanged();
    void draftDirectoryChanged();
    void lastErrorChanged();
    void assetsChanged();

private:
    struct Asset {
        QString id;
        QString kind;
        QString name;
        QString source;
        qint64 duration = 0;
        bool missing = false;
        QString text;
        QString color;
        bool favorite = false;
        bool preset = false;
        QString filterName;
    };

    void ensurePresets();
    int indexOf(const QString &id) const;
    QVariantMap assetMap(const Asset &asset) const;
    bool copyMedia(const QUrl &source, const QString &id, QString *relativePath);
    void setLastError(const QString &message);
    static QString detectKind(const QString &path);

    QList<Asset> m_assets;
    QString m_draftDirectory;
    QString m_lastError;
    MediaProbe m_probe;
};
