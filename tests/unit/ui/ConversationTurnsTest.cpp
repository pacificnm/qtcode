#include "ui/views/ConversationFormatting.h"
#include "ui/views/ConversationTurns.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QtTest>

namespace {

qtcode::agents::AgentMessage makeMessage(
    const QString &id,
    const QString &role,
    const QString &content)
{
    qtcode::agents::AgentMessage message;
    message.id = id;
    message.role = role;
    message.content = content;
    return message;
}

} // namespace

class ConversationTurnsTest final : public QObject
{
    Q_OBJECT

private slots:
    void groupsUserPromptWithFollowingResponses();
    void startsNewTurnOnEachUserMessage();
    void groupsConsecutiveActivitiesIntoOneSegment();
    void splitsActivityGroupsAroundAssistantReplies();
    void classifiesActivityKinds();
    void buildsFooterSummaryText();
};

void ConversationTurnsTest::groupsUserPromptWithFollowingResponses()
{
    QList<qtcode::agents::AgentMessage> messages = {
        makeMessage(QStringLiteral("u1"), QStringLiteral("user"), QStringLiteral("Fix the bug")),
        makeMessage(QStringLiteral("a1"), QStringLiteral("activity"), QStringLiteral("Running: cmake --build")),
        makeMessage(QStringLiteral("a2"), QStringLiteral("assistant"), QStringLiteral("Found the issue.")),
    };

    const QList<qtcode::ui::ConversationTurn> turns =
        qtcode::ui::groupMessagesIntoTurns(messages);

    QCOMPARE(turns.size(), 1);
    QCOMPARE(turns.constFirst().userMessage.content, QStringLiteral("Fix the bug"));
    QCOMPARE(turns.constFirst().responses.size(), 2);
    QCOMPARE(turns.constFirst().responses.at(0).role, QStringLiteral("activity"));
    QCOMPARE(turns.constFirst().responses.at(1).role, QStringLiteral("assistant"));
}

void ConversationTurnsTest::startsNewTurnOnEachUserMessage()
{
    QList<qtcode::agents::AgentMessage> messages = {
        makeMessage(QStringLiteral("u1"), QStringLiteral("user"), QStringLiteral("First prompt")),
        makeMessage(QStringLiteral("a1"), QStringLiteral("assistant"), QStringLiteral("First reply")),
        makeMessage(QStringLiteral("u2"), QStringLiteral("user"), QStringLiteral("Second prompt")),
        makeMessage(QStringLiteral("a2"), QStringLiteral("activity"), QStringLiteral("Read: src/main.cpp")),
    };

    const QList<qtcode::ui::ConversationTurn> turns =
        qtcode::ui::groupMessagesIntoTurns(messages);

    QCOMPARE(turns.size(), 2);
    QCOMPARE(turns.at(0).userMessage.content, QStringLiteral("First prompt"));
    QCOMPARE(turns.at(0).responses.size(), 1);
    QCOMPARE(turns.at(1).userMessage.content, QStringLiteral("Second prompt"));
    QCOMPARE(turns.at(1).responses.size(), 1);
}

void ConversationTurnsTest::groupsConsecutiveActivitiesIntoOneSegment()
{
    const QList<qtcode::agents::AgentMessage> responses = {
        makeMessage(QStringLiteral("a1"), QStringLiteral("activity"), QStringLiteral("Running: make")),
        makeMessage(QStringLiteral("a2"), QStringLiteral("activity"), QStringLiteral("Read: src/a.cpp")),
        makeMessage(QStringLiteral("a3"), QStringLiteral("assistant"), QStringLiteral("Done.")),
    };

    const QList<qtcode::ui::TurnContentSegment> segments =
        qtcode::ui::segmentTurnResponses(responses);

    QCOMPARE(segments.size(), 2);
    QCOMPARE(segments.at(0).kind, qtcode::ui::TurnContentSegmentKind::ActivityGroup);
    QCOMPARE(segments.at(0).messages.size(), 2);
    QCOMPARE(segments.at(1).kind, qtcode::ui::TurnContentSegmentKind::Assistant);
}

void ConversationTurnsTest::splitsActivityGroupsAroundAssistantReplies()
{
    const QList<qtcode::agents::AgentMessage> responses = {
        makeMessage(QStringLiteral("a1"), QStringLiteral("activity"), QStringLiteral("Grep: pattern")),
        makeMessage(QStringLiteral("a2"), QStringLiteral("assistant"), QStringLiteral("Checking.")),
        makeMessage(QStringLiteral("a3"), QStringLiteral("activity"), QStringLiteral("Read: src/b.cpp")),
    };

    const QList<qtcode::ui::TurnContentSegment> segments =
        qtcode::ui::segmentTurnResponses(responses);

    QCOMPARE(segments.size(), 3);
    QCOMPARE(segments.at(0).kind, qtcode::ui::TurnContentSegmentKind::ActivityGroup);
    QCOMPARE(segments.at(0).messages.size(), 1);
    QCOMPARE(segments.at(1).kind, qtcode::ui::TurnContentSegmentKind::Assistant);
    QCOMPARE(segments.at(2).kind, qtcode::ui::TurnContentSegmentKind::ActivityGroup);
}

void ConversationTurnsTest::classifiesActivityKinds()
{
    using qtcode::ui::ActivityKind;
    using qtcode::ui::activityKindForVerb;
    using qtcode::ui::activityCardObjectName;

    QCOMPARE(activityKindForVerb(QStringLiteral("Running")), ActivityKind::Shell);
    QCOMPARE(activityKindForVerb(QStringLiteral("Read")), ActivityKind::File);
    QCOMPARE(activityKindForVerb(QStringLiteral("Grep")), ActivityKind::Search);
    QCOMPARE(
        activityCardObjectName(ActivityKind::Shell),
        QStringLiteral("conversationToolCardShell"));
    QCOMPARE(
        activityCardObjectName(ActivityKind::File),
        QStringLiteral("conversationToolCardFile"));
}

void ConversationTurnsTest::buildsFooterSummaryText()
{
    qtcode::ui::ConversationTurn turn;
    turn.userMessage = makeMessage(QStringLiteral("u1"), QStringLiteral("user"), QStringLiteral("Prompt"));
    turn.responses = {
        makeMessage(QStringLiteral("a1"), QStringLiteral("activity"), QStringLiteral("Running: make")),
        makeMessage(QStringLiteral("a2"), QStringLiteral("activity"), QStringLiteral("Read: a.cpp")),
        makeMessage(QStringLiteral("a3"), QStringLiteral("assistant"), QStringLiteral("Done")),
    };

    QCOMPARE(turn.activityCount(), 2);
    QCOMPARE(turn.assistantReplyCount(), 1);
    QVERIFY(turn.footerSummaryText().contains(QStringLiteral("2")));
    QVERIFY(turn.footerSummaryText().contains(QStringLiteral("1")));
}

QObject *buildConversationTurnsTest()
{
    return new ConversationTurnsTest();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("qtcode");

    KAboutData aboutData(
        QStringLiteral("qtcode"),
        QStringLiteral("QTCode"),
        QStringLiteral("0.1.0"),
        QStringLiteral("Test"),
        KAboutLicense::MIT);
    KAboutData::setApplicationData(aboutData);

    return QTest::qExec(buildConversationTurnsTest(), argc, argv);
}

#include "ConversationTurnsTest.moc"
