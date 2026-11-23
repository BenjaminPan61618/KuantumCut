#pragma once

#include <QJsonObject>
#include <QString>

class AssetModel;
class TimelineModel;

class Draft
{
public:
    static constexpr int SchemaVersion = 1;

    QString id;
    QString name;
    int width = 1920;
    int height = 1080;

    QJsonObject toJson(const AssetModel &assets, const TimelineModel &timeline) const;
    bool fromJson(const QJsonObject &object, AssetModel &assets, TimelineModel &timeline,
                  QString *error = nullptr);
};
