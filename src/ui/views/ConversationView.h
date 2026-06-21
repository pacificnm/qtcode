#pragma once

#include "agents/AgentModels.h"

#include <QWidget>

class QFrame;
class QLabel;
class QResizeEvent;
class QScrollArea;
class QScrollBar;
class QVBoxLayout;

namespace qtcode::ui {

class ConversationTurnWidget;

class ConversationView final : public QWidget
{
    Q_OBJECT

public:
    explicit ConversationView(QWidget *parent = nullptr);
    ~ConversationView() override;

    void clearConversation();
    void setMessages(const QList<qtcode::agents::AgentMessage> &messages);
    void syncMessages(
        const QList<qtcode::agents::AgentMessage> &messages,
        bool generating = false);
    void setGenerating(bool generating);
    [[nodiscard]] QScrollBar *verticalScrollBar() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void destroyTurnWidgets();
    void rebuildTurns(const QList<qtcode::agents::AgentMessage> &messages);
    void applyGeneratingState();
    void scrollViewToBottom();
    void updateStickyPrompt();
    void updateEmptyState();
    void updateOverlayGeometry();
    void applyChromeStyles();

    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_contentWidget = nullptr;
    QVBoxLayout *m_turnsLayout = nullptr;
    QFrame *m_stickyPrompt = nullptr;
    QLabel *m_stickyPromptLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QList<ConversationTurnWidget *> m_turnWidgets;
    QList<qtcode::agents::AgentMessage> m_messages;
    QStringList m_turnSignatures;
    bool m_generating = false;
    bool m_cachedStickToBottom = true;
    bool m_stickyPromptVisible = false;
    bool m_shuttingDown = false;
};

} // namespace qtcode::ui
