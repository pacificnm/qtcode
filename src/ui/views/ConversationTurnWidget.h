#pragma once

#include "ui/views/ConversationTurns.h"

#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QResizeEvent;
class QVBoxLayout;

namespace qtcode::ui {

struct TurnPresentation
{
    bool isActiveTurn = false;
    bool generating = false;
};

class ConversationTurnWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ConversationTurnWidget(QWidget *parent = nullptr);
    ~ConversationTurnWidget() override;

    void setTurn(const ConversationTurn &turn);
    void setPresentation(const TurnPresentation &presentation);
    [[nodiscard]] QString signature() const;
    [[nodiscard]] QWidget *humanBubbleWidget() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onCollapsedSummaryClicked();

private:
    void rebuildAiContent(const ConversationTurn &turn);
    void applyPresentation();
    void setCollapsed(bool collapsed);
    void setDimmed(bool dimmed);
    QWidget *createToolCallCard(const qtcode::agents::AgentMessage &message) const;
    QWidget *createAssistantBlock(const qtcode::agents::AgentMessage &message);
    void applyHumanBubbleStyle();
    void syncBlockWidths();
    void updateFooter(const ConversationTurn &turn);

    QFrame *m_humanBubble = nullptr;
    QLabel *m_humanContent = nullptr;
    QList<QLabel *> m_assistantBlocks;
    QWidget *m_aiContainer = nullptr;
    QVBoxLayout *m_aiLayout = nullptr;
    QPushButton *m_collapsedSummary = nullptr;
    QLabel *m_footerLabel = nullptr;
    ConversationTurn m_turn;
    TurnPresentation m_presentation;
    QString m_signature;
    bool m_userExpanded = false;
    bool m_collapsed = false;
    bool m_shuttingDown = false;
};

} // namespace qtcode::ui
