#pragma once

#include "agents/AgentModels.h"

#include <QList>
#include <QString>

namespace qtcode::ui {

struct ConversationTurn
{
    qtcode::agents::AgentMessage userMessage;
    QList<qtcode::agents::AgentMessage> responses;
    [[nodiscard]] QString signature() const;
    [[nodiscard]] int activityCount() const;
    [[nodiscard]] int assistantReplyCount() const;
    [[nodiscard]] QString collapsedSummaryText() const;
    [[nodiscard]] QString footerSummaryText() const;
};

enum class TurnContentSegmentKind
{
    Assistant,
    ActivityGroup,
};

struct TurnContentSegment
{
    TurnContentSegmentKind kind = TurnContentSegmentKind::Assistant;
    QList<qtcode::agents::AgentMessage> messages;
};

[[nodiscard]] QList<ConversationTurn> groupMessagesIntoTurns(
    const QList<qtcode::agents::AgentMessage> &messages);
[[nodiscard]] QList<TurnContentSegment> segmentTurnResponses(
    const QList<qtcode::agents::AgentMessage> &responses);
[[nodiscard]] QString activityGroupTitle(const QList<qtcode::agents::AgentMessage> &activities);

} // namespace qtcode::ui
