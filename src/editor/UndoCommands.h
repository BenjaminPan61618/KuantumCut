#pragma once

#include <QJsonObject>
#include <QUndoCommand>

class TimelineModel;

class TimelineCommand : public QUndoCommand
{
public:
    TimelineCommand(TimelineModel *timeline, QJsonObject before, QJsonObject after,
                    QString selectionBefore, QString selectionAfter, const QString &text,
                    bool mergeable = false, QString clipId = {}, QString property = {});

    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

private:
    void apply(const QJsonObject &snapshot, const QString &selection);

    TimelineModel *m_timeline;
    QJsonObject m_before;
    QJsonObject m_after;
    QString m_selectionBefore;
    QString m_selectionAfter;
    bool m_mergeable;
    bool m_firstRedo = true;
    QString m_clipId;
    QString m_property;
};
