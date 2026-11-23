#include "editor/UndoCommands.h"

#include "models/TimelineModel.h"

#include <utility>

TimelineCommand::TimelineCommand(TimelineModel *timeline, QJsonObject before, QJsonObject after,
                                 QString selectionBefore, QString selectionAfter,
                                 const QString &text, bool mergeable, QString clipId,
                                 QString property)
    : QUndoCommand(text)
    , m_timeline(timeline)
    , m_before(std::move(before))
    , m_after(std::move(after))
    , m_selectionBefore(std::move(selectionBefore))
    , m_selectionAfter(std::move(selectionAfter))
    , m_mergeable(mergeable)
    , m_clipId(std::move(clipId))
    , m_property(std::move(property))
{
}

void TimelineCommand::undo()
{
    apply(m_before, m_selectionBefore);
}

void TimelineCommand::redo()
{
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    apply(m_after, m_selectionAfter);
}

int TimelineCommand::id() const
{
    return m_mergeable ? 1 : -1;
}

bool TimelineCommand::mergeWith(const QUndoCommand *other)
{
    const auto *command = dynamic_cast<const TimelineCommand *>(other);
    if (!m_mergeable || !command || !command->m_mergeable || m_timeline != command->m_timeline
        || m_clipId != command->m_clipId || m_property != command->m_property)
        return false;
    m_after = command->m_after;
    m_selectionAfter = command->m_selectionAfter;
    return true;
}

void TimelineCommand::apply(const QJsonObject &snapshot, const QString &selection)
{
    m_timeline->fromJson(snapshot);
    m_timeline->setSelectedClipId(selection);
}
