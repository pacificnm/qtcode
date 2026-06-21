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

private:
    [[nodiscard]] static QString iconToHtml(const QIcon &icon, int size);
    [[nodiscard]] static QString formatMessageHtml(
        const qtcode::agents::AgentMessage &message,
        const QPalette &palette);

    QTextEdit *m_view = nullptr;
};

} // namespace qtcode::ui
