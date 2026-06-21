#pragma once

#include <QWidget>

class QTextEdit;

namespace qtcode::agents {
struct AgentMessage;
} // namespace qtcode::agents

namespace qtcode::ui {

class ConversationView final : public QWidget
{
    Q_OBJECT

public:
    explicit ConversationView(QWidget *parent = nullptr);

    void clearConversation();
    void setMessages(const QList<qtcode::agents::AgentMessage> &messages);
    void syncMessages(
        const QList<qtcode::agents::AgentMessage> &messages,
        bool showActivityIndicator,
        const QString &activityText);

private:
    void rebuildAllMessages(const QList<qtcode::agents::AgentMessage> &messages);
    void renderDocument(
        const QList<qtcode::agents::AgentMessage> &messages,
        bool showActivityIndicator,
        const QString &activityText);
    [[nodiscard]] static QString iconToHtml(const QIcon &icon, int size);
    [[nodiscard]] static QString formatMessageHtml(
        const qtcode::agents::AgentMessage &message,
        const QPalette &palette);
    [[nodiscard]] static QString formatActivityIndicatorHtml(
        const QString &activityText,
        const QPalette &palette);

    QTextEdit *m_view = nullptr;
    int m_cachedMessageCount = 0;
    QString m_cachedLastMessageSignature;
    QStringList m_messageHtmlBlocks;
    QStringList m_messageSignatures;
};

} // namespace qtcode::ui
