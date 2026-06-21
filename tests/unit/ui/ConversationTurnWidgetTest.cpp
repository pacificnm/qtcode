#include "agents/AgentModels.h"
#include "ui/views/ConversationTurnWidget.h"
#include "ui/views/ConversationTurns.h"
#include "ui/widgets/CollapsibleSection.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QAbstractScrollArea>
#include <QPushButton>
#include <QLayout>
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

class ConversationTurnWidgetTest final : public QObject
{
    Q_OBJECT

private slots:
    void assistantReplyUsesFullViewportWidth();
    void humanPromptUsesRightAlignedBubbleWidth();
    void pastTurnWithAiContentStartsCollapsed();
    void activityGroupsStartCollapsed();
    void richTextBlocksAvoidNestedScrolling();
    void longAssistantReplyWithCodeBlockIsNotClipped();
    void narrowChatWrapsLongUnbrokenAssistantText();
};

void ConversationTurnWidgetTest::assistantReplyUsesFullViewportWidth()
{
    qtcode::ui::ConversationTurnWidget turnWidget;
    turnWidget.resize(480, 640);
    turnWidget.show();
    QCoreApplication::processEvents();

    qtcode::ui::ConversationTurn turn;
    turn.userMessage = makeMessage(
        QStringLiteral("u1"),
        QStringLiteral("user"),
        QStringLiteral("Investigate the failing test."));
    turn.responses = {
        makeMessage(
            QStringLiteral("a1"),
            QStringLiteral("activity"),
            QStringLiteral("Running: ctest --output-on-failure")),
        makeMessage(
            QStringLiteral("a2"),
            QStringLiteral("assistant"),
            QStringLiteral(
                "I'll inspect the failing test output and trace the regression through the "
                "recent cache changes before proposing a fix.")),
    };

    turnWidget.setTurn(turn);
    qtcode::ui::TurnPresentation presentation;
    presentation.isActiveTurn = true;
    presentation.generating = false;
    turnWidget.setPresentation(presentation);
    for (int attempt = 0; attempt < 8; ++attempt) {
        QCoreApplication::processEvents();
    }

    QWidget *assistantBlock =
        turnWidget.findChild<QWidget *>(QStringLiteral("conversationAssistantMessageBlock"));
    QVERIFY2(assistantBlock != nullptr, "Assistant reply block should exist");

    const QString html = assistantBlock->property("conversationHtml").toString();
    QVERIFY(html.startsWith(QStringLiteral("I'll inspect")));
    QVERIFY(html.contains(QStringLiteral("proposing a fix")));

    const int expectedWidth = qMax(
        turnWidget.width(),
        qMax(assistantBlock->parentWidget()->width(), turnWidget.width()));
    QVERIFY2(expectedWidth > 100, "Assistant reply should have a meaningful layout width");
    QVERIFY2(
        assistantBlock->width() >= expectedWidth - 8,
        qPrintable(QStringLiteral(
            "Assistant reply should use the full chat column width "
            "(blockWidth=%1 expectedWidth=%2 turnWidth=%3)")
            .arg(assistantBlock->width())
            .arg(expectedWidth)
            .arg(turnWidget.width())));

    const int contentHeight = assistantBlock->heightForWidth(assistantBlock->width());
    QVERIFY2(
        contentHeight <= assistantBlock->height() + 2,
        "Assistant reply should show its full height without clipping");
    QVERIFY2(
        contentHeight > assistantBlock->fontMetrics().height() * 1.5,
        "Assistant reply should lay out more than a single clipped line");
}

void ConversationTurnWidgetTest::humanPromptUsesRightAlignedBubbleWidth()
{
    qtcode::ui::ConversationTurnWidget turnWidget;
    turnWidget.resize(480, 640);
    turnWidget.show();
    QCoreApplication::processEvents();

    qtcode::ui::ConversationTurn turn;
    turn.userMessage = makeMessage(
        QStringLiteral("u1"),
        QStringLiteral("user"),
        QStringLiteral(
            "Please investigate why the conversation transcript wraps user prompts at a "
            "narrow fixed width instead of using the full chat column available to the bubble."));
    turnWidget.setTurn(turn);
    qtcode::ui::TurnPresentation presentation;
    presentation.isActiveTurn = true;
    presentation.generating = false;
    turnWidget.setPresentation(presentation);
    for (int attempt = 0; attempt < 8; ++attempt) {
        QCoreApplication::processEvents();
    }

    QWidget *humanBlock =
        turnWidget.findChild<QWidget *>(QStringLiteral("conversationUserPromptBlock"));
    QVERIFY2(humanBlock != nullptr, "Human prompt block should exist");

    const QWidget *humanBubble = humanBlock->parentWidget();
    QVERIFY2(
        humanBubble != nullptr
            && humanBubble->objectName() == QStringLiteral("conversationHumanBubble"),
        "Human block should live inside the bubble frame");

    const QMargins bubbleMargins = humanBubble->layout()->contentsMargins();
    const int maxBubbleWidth = turnWidget.width() * 86 / 100 + 2;
    QVERIFY2(
        humanBubble->width() <= maxBubbleWidth,
        qPrintable(QStringLiteral(
            "Human prompt bubble should be bounded like a Cursor-style chat bubble "
            "(bubbleWidth=%1 maxBubbleWidth=%2 turnWidth=%3)")
            .arg(humanBubble->width())
            .arg(maxBubbleWidth)
            .arg(turnWidget.width())));
    QVERIFY2(
        humanBubble->x() > 0,
        qPrintable(QStringLiteral(
            "Human prompt bubble should be right aligned with left breathing room "
            "(bubbleX=%1 bubbleWidth=%2 turnWidth=%3)")
            .arg(humanBubble->x())
            .arg(humanBubble->width())
            .arg(turnWidget.width())));

    const int bubbleContentWidth =
        humanBubble->width() - bubbleMargins.left() - bubbleMargins.right();
    QVERIFY2(bubbleContentWidth > 100, "Human bubble should have a meaningful layout width");

    QVERIFY2(
        humanBlock->width() >= bubbleContentWidth - 8,
        qPrintable(QStringLiteral(
            "Human prompt text should fill the prompt bubble content width "
            "(blockWidth=%1 bubbleContentWidth=%2 bubbleWidth=%3 turnWidth=%4)")
            .arg(humanBlock->width())
            .arg(bubbleContentWidth)
            .arg(humanBubble->width())
            .arg(turnWidget.width())));

    const int contentHeight = humanBlock->heightForWidth(humanBlock->width());
    QVERIFY2(
        contentHeight <= humanBlock->height() + 2,
        "Human prompt should show its full height without clipping");
}

void ConversationTurnWidgetTest::pastTurnWithAiContentStartsCollapsed()
{
    qtcode::ui::ConversationTurnWidget turnWidget;
    turnWidget.resize(480, 640);
    turnWidget.show();
    QCoreApplication::processEvents();

    qtcode::ui::ConversationTurn turn;
    turn.userMessage = makeMessage(
        QStringLiteral("u1"),
        QStringLiteral("user"),
        QStringLiteral("First prompt"));
    turn.responses = {
        makeMessage(
            QStringLiteral("a1"),
            QStringLiteral("activity"),
            QStringLiteral("Running: make")),
        makeMessage(
            QStringLiteral("a2"),
            QStringLiteral("assistant"),
            QStringLiteral("Done.")),
    };

    turnWidget.setTurn(turn);
    qtcode::ui::TurnPresentation presentation;
    presentation.isActiveTurn = false;
    presentation.generating = true;
    turnWidget.setPresentation(presentation);
    QCoreApplication::processEvents();

    const QPushButton *collapsedSummary =
        turnWidget.findChild<QPushButton *>(QStringLiteral("conversationTurnCollapsedSummary"));
    QVERIFY2(collapsedSummary != nullptr, "Collapsed summary control should exist");
    QVERIFY2(collapsedSummary->isVisible(), "Past turns with AI content should start collapsed");
}

void ConversationTurnWidgetTest::activityGroupsStartCollapsed()
{
    qtcode::ui::ConversationTurnWidget turnWidget;
    turnWidget.resize(480, 640);
    turnWidget.show();
    QCoreApplication::processEvents();

    qtcode::ui::ConversationTurn turn;
    turn.userMessage = makeMessage(
        QStringLiteral("u1"),
        QStringLiteral("user"),
        QStringLiteral("Investigate the failure"));
    turn.responses = {
        makeMessage(
            QStringLiteral("a1"),
            QStringLiteral("activity"),
            QStringLiteral("Running: ctest")),
        makeMessage(
            QStringLiteral("a2"),
            QStringLiteral("activity"),
            QStringLiteral("Read: src/main.cpp")),
        makeMessage(
            QStringLiteral("a3"),
            QStringLiteral("assistant"),
            QStringLiteral("Found the issue.")),
    };

    turnWidget.setTurn(turn);
    qtcode::ui::TurnPresentation presentation;
    presentation.isActiveTurn = true;
    presentation.generating = true;
    turnWidget.setPresentation(presentation);
    QCoreApplication::processEvents();

    const QList<qtcode::ui::CollapsibleSection *> groups =
        turnWidget.findChildren<qtcode::ui::CollapsibleSection *>();
    QVERIFY2(!groups.isEmpty(), "Activity groups should render as collapsible sections");
    for (const qtcode::ui::CollapsibleSection *group : groups) {
        QVERIFY2(!group->isExpanded(), "Activity groups should start collapsed");
    }
}

void ConversationTurnWidgetTest::richTextBlocksAvoidNestedScrolling()
{
    qtcode::ui::ConversationTurnWidget turnWidget;
    turnWidget.resize(480, 640);
    turnWidget.show();
    QCoreApplication::processEvents();

    qtcode::ui::ConversationTurn turn;
    turn.userMessage = makeMessage(
        QStringLiteral("u1"),
        QStringLiteral("user"),
        QStringLiteral("Fix the scrolling bug."));
    turn.responses = {
        makeMessage(
            QStringLiteral("a1"),
            QStringLiteral("assistant"),
            QStringLiteral(
                "Line 1 of a long reply.\n"
                "Line 2 of a long reply.\n"
                "Line 3 of a long reply.\n"
                "Line 4 of a long reply.\n"
                "Line 5 of a long reply.\n"
                "Line 6 of a long reply.")),
    };

    turnWidget.setTurn(turn);
    QCoreApplication::processEvents();

    const QList<QAbstractScrollArea *> nestedScrollAreas =
        turnWidget.findChildren<QAbstractScrollArea *>();
    QVERIFY2(
        nestedScrollAreas.isEmpty(),
        "Turn widgets should not embed nested scroll areas");
}

void ConversationTurnWidgetTest::longAssistantReplyWithCodeBlockIsNotClipped()
{
    qtcode::ui::ConversationTurnWidget turnWidget;
    turnWidget.resize(480, 640);
    turnWidget.show();
    QCoreApplication::processEvents();

    qtcode::ui::ConversationTurn turn;
    turn.userMessage = makeMessage(
        QStringLiteral("u1"),
        QStringLiteral("user"),
        QStringLiteral("Explain the fix."));
    turn.responses = {
        makeMessage(
            QStringLiteral("a1"),
            QStringLiteral("assistant"),
            QStringLiteral(
                "The regression came from locking label heights during layout.\n"
                "\n"
                "Here is the corrected approach:\n"
                "```cpp\n"
                "block->setMaximumWidth(contentWidth);\n"
                "block->setMaximumHeight(QWIDGETSIZE_MAX);\n"
                "```\n"
                "\n"
                "That lets rich text grow naturally while the outer scroll area handles overflow.")),
    };

    turnWidget.setTurn(turn);
    for (int attempt = 0; attempt < 8; ++attempt) {
        QCoreApplication::processEvents();
    }

    QWidget *assistantBlock =
        turnWidget.findChild<QWidget *>(QStringLiteral("conversationAssistantMessageBlock"));
    QVERIFY2(assistantBlock != nullptr, "Assistant reply block should exist");
    QVERIFY(assistantBlock->property("conversationHtml").toString().contains(QStringLiteral("setMaximumHeight")));

    const int contentHeight = assistantBlock->heightForWidth(assistantBlock->width());
    QVERIFY2(
        assistantBlock->height() + 2 >= contentHeight,
        "Long assistant replies with code blocks should not be clipped");
    QVERIFY2(
        contentHeight > assistantBlock->fontMetrics().height() * 4,
        "Assistant reply should occupy multiple lines of height");
}

void ConversationTurnWidgetTest::narrowChatWrapsLongUnbrokenAssistantText()
{
    qtcode::ui::ConversationTurnWidget turnWidget;
    turnWidget.resize(320, 520);
    turnWidget.show();
    QCoreApplication::processEvents();

    qtcode::ui::ConversationTurn turn;
    turn.userMessage = makeMessage(
        QStringLiteral("u1"),
        QStringLiteral("user"),
        QStringLiteral("Open the generated file path."));
    turn.responses = {
        makeMessage(
            QStringLiteral("a1"),
            QStringLiteral("assistant"),
            QStringLiteral(
                "The relevant path is "
                "/home/jaimie/qtcode/src/ui/views/"
                "conversation_renderer_with_a_very_long_generated_file_name_that_must_wrap.cpp "
                "and it should remain visible in a narrow chat tab.")),
    };

    turnWidget.setTurn(turn);
    for (int attempt = 0; attempt < 8; ++attempt) {
        QCoreApplication::processEvents();
    }

    QWidget *assistantBlock =
        turnWidget.findChild<QWidget *>(QStringLiteral("conversationAssistantMessageBlock"));
    QVERIFY2(assistantBlock != nullptr, "Assistant reply block should exist");
    QVERIFY2(
        assistantBlock->width() <= turnWidget.width(),
        "Assistant reply should stay inside the narrow chat column");

    const int contentHeight = assistantBlock->heightForWidth(assistantBlock->width());
    QVERIFY2(
        assistantBlock->height() + 2 >= contentHeight,
        "Narrow long-path assistant reply should not be vertically clipped");
    QVERIFY2(
        contentHeight > assistantBlock->fontMetrics().height() * 2,
        "Long unbroken path should wrap to multiple visual lines");
}

QObject *buildConversationTurnWidgetTest()
{
    return new ConversationTurnWidgetTest();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("qtcode");

    KAboutData aboutData(
        QStringLiteral("qtcode"),
        QStringLiteral("QTCode"),
        QStringLiteral("0.1.0"),
        QStringLiteral("Test"),
        KAboutLicense::MIT);
    KAboutData::setApplicationData(aboutData);

    return QTest::qExec(buildConversationTurnWidgetTest(), argc, argv);
}

#include "ConversationTurnWidgetTest.moc"
