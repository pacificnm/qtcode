#include "agents/AgentModels.h"
#include "ui/views/ConversationView.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QScrollBar>
#include <QTextEdit>
#include <QtTest>

namespace {

[[nodiscard]] QList<qtcode::agents::AgentMessage> sampleMessages(int count)
{
    QList<qtcode::agents::AgentMessage> messages;
    messages.reserve(count);
    for (int index = 0; index < count; ++index) {
        qtcode::agents::AgentMessage message;
        message.id = QStringLiteral("message-%1").arg(index);
        message.role = index % 2 == 0 ? QStringLiteral("user") : QStringLiteral("assistant");
        message.content = QStringLiteral(
                              "Message %1 with enough text to make the conversation view scroll vertically.")
                              .arg(index);
        messages.append(message);
    }
    return messages;
}

} // namespace

class ConversationViewTest final : public QObject
{
    Q_OBJECT

private slots:
    void activityIndicatorUpdateKeepsScrollAtBottom();
    void appendedMessageScrollsToBottom();
};

void ConversationViewTest::activityIndicatorUpdateKeepsScrollAtBottom()
{
    qtcode::ui::ConversationView view;
    auto *textEdit = view.findChild<QTextEdit *>();
    QVERIFY(textEdit != nullptr);

    const QList<qtcode::agents::AgentMessage> messages = sampleMessages(24);
    view.setMessages(messages);

    QScrollBar *scrollBar = textEdit->verticalScrollBar();
    QVERIFY(scrollBar != nullptr);
    scrollBar->setValue(scrollBar->maximum());
    const int bottomBefore = scrollBar->value();

    view.syncMessages(messages, true, QStringLiteral("Agent is working…"));
    QCoreApplication::processEvents();

    QCOMPARE(scrollBar->value(), scrollBar->maximum());
    QVERIFY(scrollBar->value() >= bottomBefore);
}

void ConversationViewTest::appendedMessageScrollsToBottom()
{
    qtcode::ui::ConversationView view;
    auto *textEdit = view.findChild<QTextEdit *>();
    QVERIFY(textEdit != nullptr);

    QList<qtcode::agents::AgentMessage> messages = sampleMessages(24);
    view.setMessages(messages);

    QScrollBar *scrollBar = textEdit->verticalScrollBar();
    QVERIFY(scrollBar != nullptr);
    scrollBar->setValue(scrollBar->maximum());

    qtcode::agents::AgentMessage userMessage;
    userMessage.id = QStringLiteral("new-user-message");
    userMessage.role = QStringLiteral("user");
    userMessage.content = QStringLiteral("A newly submitted prompt.");
    messages.append(userMessage);

    view.syncMessages(messages, true, QStringLiteral("Agent is working…"));
    QCoreApplication::processEvents();

    QCOMPARE(scrollBar->value(), scrollBar->maximum());
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
