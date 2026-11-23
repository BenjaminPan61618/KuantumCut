#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QProcess;

class MediaProbe : public QObject
{
    Q_OBJECT

public:
    explicit MediaProbe(QObject *parent = nullptr);
    void probe(const QString &assetId, const QString &absolutePath);

signals:
    void finished(const QString &assetId, qint64 durationMs);

private:
    void finishProcess(QProcess *process);

    QHash<QProcess *, QString> m_jobs;
};
