#include "ui/views/ConversationTurns.h"

#include "ui/views/ConversationFormatting.h"

#include <KLocalizedString>

namespace qtcode::ui {

QString ConversationTurn::signature() const
{
    QStringList parts;
    parts.append(messageSignature(userMessage.id, userMessage.role, userMessage.content));
    for (const qtcode::agents::AgentMessage &response : responses) {
        parts.append(messageSignature(response.id, response.role, response.content));
    }
    return parts.join(QLatin1Char('|'));
}

int ConversationTurn::activityCount() const
{
    int count = 0;
    for (const qtcode::agents::AgentMessage &response : responses) {
        if (response.role == QStringLiteral("activity")) {
            ++count;
        }
    }
    return count;
}

int ConversationTurn::assistantReplyCount() const
{
    int count = 0;
    for (const qtcode::agents::AgentMessage &response : responses) {
        if (response.role == QStringLiteral("assistant")) {
            ++count;
        }
    }
    return count;
}

QString ConversationTurn::collapsedSummaryText() const
{
    const int steps = activityCount();
    const int replies = assistantReplyCount();
    const QString prompt = userMessage.content.trimmed();

    if (steps == 0 && replies == 0) {
        return prompt.isEmpty() ? i18n("Prompt") : truncateForDisplay(prompt, 96);
    }

    return footerSummaryText();
}

QString ConversationTurn::footerSummaryText() const
{
    const int steps = activityCount();
    const int replies = assistantReplyCount();

    if (steps == 0 && replies == 0) {
        return i18n("Worked");
    }

    if (steps > 0 && replies > 0) {
        return i18n("Worked · %1 steps · %2 replies", steps, replies);
    }

    if (steps > 0) {
        return i18np("Worked · 1 step", "Worked · %1 steps", steps);
    }

    return i18np("Worked · 1 reply", "Worked · %1 replies", replies);
}

QList<ConversationTurn> groupMessagesIntoTurns(
    const QList<qtcode::agents::AgentMessage> &messages)
{
    QList<ConversationTurn> turns;
    ConversationTurn *currentTurn = nullptr;

    for (const qtcode::agents::AgentMessage &message : messages) {
        if (message.role == QStringLiteral("user")) {
            turns.append(ConversationTurn{message, {}});
            currentTurn = &turns.last();
            continue;
        }

        if (currentTurn == nullptr) {
            qtcode::agents::AgentMessage placeholderUser;
            placeholderUser.id = QStringLiteral("orphan-user");
            placeholderUser.role = QStringLiteral("user");
            placeholderUser.content = QString();
            turns.append(ConversationTurn{placeholderUser, {message}});
            currentTurn = &turns.last();
            continue;
        }

        currentTurn->responses.append(message);
    }

    return turns;
}

QList<TurnContentSegment> segmentTurnResponses(
    const QList<qtcode::agents::AgentMessage> &responses)
{
    QList<TurnContentSegment> segments;
    QList<qtcode::agents::AgentMessage> pendingActivities;

    const auto flushActivities = [&segments, &pendingActivities]() {
        if (pendingActivities.isEmpty()) {
            return;
        }

        TurnContentSegment segment;
        segment.kind = TurnContentSegmentKind::ActivityGroup;
        segment.messages = pendingActivities;
        segments.append(segment);
        pendingActivities.clear();
    };

    for (const qtcode::agents::AgentMessage &response : responses) {
        if (response.role == QStringLiteral("activity")) {
            pendingActivities.append(response);
            continue;
        }

        flushActivities();

        TurnContentSegment segment;
        segment.kind = TurnContentSegmentKind::Assistant;
        segment.messages = {response};
        segments.append(segment);
    }

    flushActivities();
    return segments;
}

QString activityGroupTitle(const QList<qtcode::agents::AgentMessage> &activities)
{
    if (activities.isEmpty()) {
        return i18n("Steps");
    }

    if (activities.size() == 1) {
        const ParsedActivity parsed = parseActivityText(activities.constFirst().content);
        return activityCardHeaderLabel(parsed);
    }

    bool allShell = true;
    for (const qtcode::agents::AgentMessage &activity : activities) {
        if (activityKindForVerb(parseActivityText(activity.content).verb) != ActivityKind::Shell) {
            allShell = false;
            break;
        }
    }

    if (allShell) {
        return i18np("1 command", "%1 commands", activities.size());
    }

    return i18np("1 step", "%1 steps", activities.size());
}

} // namespace qtcode::ui
