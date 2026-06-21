#include "agents/AgentModels.h"
#include "ui/views/ConversationView.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QtTest>

namespace {

[[nodiscard]] qtcode::agents::AgentMessage makeMessage(
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

[[nodiscard]] QList<qtcode::agents::AgentMessage> sampleMessages(int count)
{
    QList<qtcode::agents::AgentMessage> messages;
    messages.reserve(count);
    for (int index = 0; index < count; ++index) {
        messages.append(makeMessage(
            QStringLiteral("message-%1").arg(index),
            index % 2 == 0 ? QStringLiteral("user") : QStringLiteral("assistant"),
            QStringLiteral(
                "Message %1 with enough text to make the conversation view scroll vertically.")
                .arg(index)));
    }
    return messages;
}

} // namespace

class ConversationViewTest final : public QObject
{
    Q_OBJECT

private slots:
    void incrementalUpdateKeepsScrollAtBottom();
    void appendedMessageScrollsToBottom();
    void transcriptUsesSingleScrollArea();
    void teardownWithLoadedConversationDoesNotCrash();
};

void ConversationViewTest::incrementalUpdateKeepsScrollAtBottom()
{
    qtcode::ui::ConversationView view;
    QList<qtcode::agents::AgentMessage> messages;
    messages.append(makeMessage(
        QStringLiteral("message-0"),
        QStringLiteral("user"),
        QStringLiteral("Investigate the failing test.")));
    messages.append(makeMessage(
        QStringLiteral("message-1"),
        QStringLiteral("activity"),
        QStringLiteral("Running: ctest --output-on-failure")));
    messages.append(makeMessage(
        QStringLiteral("message-2"),
        QStringLiteral("assistant"),
        QStringLiteral("The failure comes from a stale cache.")));

    for (int index = 3; index < 24; ++index) {
        messages.append(makeMessage(
            QStringLiteral("message-%1").arg(index),
            index % 2 == 0 ? QStringLiteral("user") : QStringLiteral("assistant"),
            QStringLiteral(
                "Message %1 with enough text to make the conversation view scroll vertically.")
                .arg(index)));
    }

    view.setMessages(messages);

    QScrollBar *scrollBar = view.verticalScrollBar();
    QVERIFY2(scrollBar != nullptr, "ConversationView should expose its scroll bar");
    scrollBar->setValue(scrollBar->maximum());
    const int bottomBefore = scrollBar->value();

    qtcode::agents::AgentMessage &lastMessage = messages.last();
    lastMessage.content.append(QStringLiteral(" Updated while running."));

    view.syncMessages(messages);
    QCoreApplication::processEvents();

    QCOMPARE(scrollBar->value(), scrollBar->maximum());
    QVERIFY(scrollBar->value() >= bottomBefore);
}

void ConversationViewTest::appendedMessageScrollsToBottom()
{
    qtcode::ui::ConversationView view;
    QList<qtcode::agents::AgentMessage> messages = sampleMessages(24);
    view.setMessages(messages);

    QScrollBar *scrollBar = view.verticalScrollBar();
    QVERIFY2(scrollBar != nullptr, "ConversationView should expose its scroll bar");
    scrollBar->setValue(scrollBar->maximum());

    messages.append(makeMessage(
        QStringLiteral("new-user-message"),
        QStringLiteral("user"),
        QStringLiteral("A newly submitted prompt.")));

    view.syncMessages(messages);
    QCoreApplication::processEvents();

    QCOMPARE(scrollBar->value(), scrollBar->maximum());
}

void ConversationViewTest::transcriptUsesSingleScrollArea()
{
    qtcode::ui::ConversationView view;
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();

    QList<qtcode::agents::AgentMessage> messages;
    messages.append(makeMessage(
        QStringLiteral("message-0"),
        QStringLiteral("user"),
        QStringLiteral("Investigate the scrolling behavior.")));
    messages.append(makeMessage(
        QStringLiteral("message-1"),
        QStringLiteral("assistant"),
        QStringLiteral(
            "First paragraph of the reply.\n"
            "Second paragraph of the reply.\n"
            "Third paragraph of the reply.\n"
            "Fourth paragraph of the reply.\n"
            "Fifth paragraph of the reply.")));

    view.setMessages(messages);
    QCoreApplication::processEvents();

    const QList<QAbstractScrollArea *> nestedScrollAreas =
        view.findChildren<QAbstractScrollArea *>();
    QCOMPARE(nestedScrollAreas.size(), 1);
    QVERIFY(view.verticalScrollBar() != nullptr);
    QCOMPARE(view.verticalScrollBar(), nestedScrollAreas.constFirst()->verticalScrollBar());
}

void ConversationViewTest::teardownWithLoadedConversationDoesNotCrash()
{
    QWidget parent;
    parent.resize(800, 600);

    auto *view = new qtcode::ui::ConversationView(&parent);
    view->setMessages(sampleMessages(12));
    view->show();
    QCoreApplication::processEvents();

    delete view;
    QCoreApplication::processEvents();
}

QObject *buildConversationViewTest()
{
    return new ConversationViewTest();
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

    return QTest::qExec(buildConversationViewTest(), argc, argv);
}

#include "ConversationViewTest.moc"
