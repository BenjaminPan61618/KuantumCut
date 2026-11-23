#include "core/Draft.h"

#include "models/AssetModel.h"
#include "models/TimelineModel.h"

#include <QJsonArray>

QJsonObject Draft::toJson(const AssetModel &assets, const TimelineModel &timeline) const
{
    return {
        {QStringLiteral("format"), QStringLiteral("KuantumCutDraft")},
        {QStringLiteral("schemaVersion"), SchemaVersion},
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("canvas"), QJsonObject{{QStringLiteral("width"), width},
                                               {QStringLiteral("height"), height}}},
        {QStringLiteral("assets"), assets.toJson()},
        {QStringLiteral("timeline"), timeline.toJson()}
    };
}

bool Draft::fromJson(const QJsonObject &object, AssetModel &assets,
                     TimelineModel &timeline, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    if (object.value(QStringLiteral("format")).toString() != QStringLiteral("KuantumCutDraft"))
        return fail(QStringLiteral("Not a KuantumCut draft."));
    if (object.value(QStringLiteral("schemaVersion")).toInt() != SchemaVersion)
        return fail(QStringLiteral("Unsupported draft schema version."));

    const QString parsedId = object.value(QStringLiteral("id")).toString();
    const QString parsedName = object.value(QStringLiteral("name")).toString();
    const QJsonObject canvas = object.value(QStringLiteral("canvas")).toObject();
    const int parsedWidth = canvas.value(QStringLiteral("width")).toInt();
    const int parsedHeight = canvas.value(QStringLiteral("height")).toInt();
    if (parsedId.isEmpty() || parsedName.isEmpty() || parsedWidth <= 0 || parsedHeight <= 0)
        return fail(QStringLiteral("The draft metadata is incomplete."));
    if (!assets.fromJson(object.value(QStringLiteral("assets")).toArray(), error))
        return false;
    if (!timeline.fromJson(object.value(QStringLiteral("timeline")).toObject(), error))
        return false;

    id = parsedId;
    name = parsedName;
    width = parsedWidth;
    height = parsedHeight;
    return true;
}
