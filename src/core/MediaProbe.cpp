#include "core/MediaProbe.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtMath>

MediaProbe::MediaProbe(QObject *parent)
    : QObject(parent)
{
}

void MediaProbe::probe(const QString &assetId, const QString &absolutePath)
{
    auto *process = new QProcess(this);
    m_jobs.insert(process, assetId);
    process->setProgram(QStringLiteral("ffprobe"));
    process->setArguments({QStringLiteral("-v"), QStringLiteral("error"),
                           QStringLiteral("-show_entries"),
                           QStringLiteral("format=duration:stream=duration"),
                           QStringLiteral("-of"), QStringLiteral("json"), absolutePath});
    connect(process, &QProcess::finished, this, [this, process]() { finishProcess(process); });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finishProcess(process);
    });
    process->start(QIODevice::ReadOnly);
}

void MediaProbe::finishProcess(QProcess *process)
{
    const auto it = m_jobs.find(process);
    if (it == m_jobs.end())
        return;

    const QString assetId = it.value();
    m_jobs.erase(it);
    const QJsonDocument document = QJsonDocument::fromJson(process->readAllStandardOutput());
    double seconds = 0.0;
    const QJsonObject root = document.object();
    const QString formatDuration = root.value(QStringLiteral("format")).toObject()
                                       .value(QStringLiteral("duration")).toString();
    if (!formatDuration.isEmpty())
        seconds = formatDuration.toDouble();

    if (seconds <= 0.0) {
        const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
        for (const QJsonValue &value : streams) {
            const QString streamDuration = value.toObject().value(QStringLiteral("duration")).toString();
            seconds = qMax(seconds, streamDuration.toDouble());
        }
    }

    emit finished(assetId, seconds > 0.0 ? qRound64(seconds * 1000.0) : 0);
    process->deleteLater();
}
