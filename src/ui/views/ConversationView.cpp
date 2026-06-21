#include "ui/views/ConversationView.h"

#include "ui/views/ConversationFormatting.h"
#include "ui/views/ConversationTurnWidget.h"
#include "ui/views/ConversationTurns.h"

#include <KLocalizedString>

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace qtcode::ui {

namespace {

TurnPresentation presentationForTurn(int turnIndex, int turnCount, bool generating)
{
    TurnPresentation presentation;
    presentation.generating = generating;
    presentation.isActiveTurn = turnCount > 0 && turnIndex == turnCount - 1;
    return presentation;
}

} // namespace

ConversationView::ConversationView(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(120);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->viewport()->installEventFilter(this);

    m_contentWidget = new QWidget(m_scrollArea);
    m_contentWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_turnsLayout = new QVBoxLayout(m_contentWidget);
    m_turnsLayout->setContentsMargins(10, 10, 10, 10);
    m_turnsLayout->setSpacing(12);
    m_turnsLayout->addStretch(1);
    m_scrollArea->setWidget(m_contentWidget);

    m_stickyPrompt = new QFrame(this);
    m_stickyPrompt->setObjectName(QStringLiteral("conversationStickyPrompt"));
    m_stickyPrompt->setVisible(false);
    m_stickyPrompt->raise();

    auto *stickyLayout = new QVBoxLayout(m_stickyPrompt);
    stickyLayout->setContentsMargins(10, 6, 10, 6);
    stickyLayout->setSpacing(0);

    m_stickyPromptLabel = new QLabel(m_stickyPrompt);
    m_stickyPromptLabel->setWordWrap(true);
    m_stickyPromptLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_stickyPromptLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    stickyLayout->addWidget(m_stickyPromptLabel);

    m_emptyLabel = new QLabel(i18n("Start a conversation with the agent…"), m_scrollArea->viewport());
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    QFont emptyFont = m_emptyLabel->font();
    emptyFont.setItalic(true);
    m_emptyLabel->setFont(emptyFont);
    m_emptyLabel->setVisible(true);

    outerLayout->addWidget(m_scrollArea, 1);

    connect(
        m_scrollArea->verticalScrollBar(),
        &QScrollBar::valueChanged,
        this,
        [this]() {
            const QScrollBar *scrollBar = m_scrollArea->verticalScrollBar();
            m_cachedStickToBottom = scrollBar == nullptr
                || scrollBar->value() >= scrollBar->maximum() - 24;
            updateStickyPrompt();
        });

    applyChromeStyles();
}

ConversationView::~ConversationView()
{
    m_shuttingDown = true;

    if (m_scrollArea != nullptr && m_scrollArea->viewport() != nullptr) {
        m_scrollArea->viewport()->removeEventFilter(this);
    }

    if (m_scrollArea != nullptr && m_scrollArea->verticalScrollBar() != nullptr) {
        disconnect(m_scrollArea->verticalScrollBar(), nullptr, this, nullptr);
    }

    destroyTurnWidgets();
}

void ConversationView::destroyTurnWidgets()
{
    for (ConversationTurnWidget *turnWidget : m_turnWidgets) {
        if (turnWidget == nullptr) {
            continue;
        }

        if (m_turnsLayout != nullptr) {
            m_turnsLayout->removeWidget(turnWidget);
        }

        delete turnWidget;
    }

    m_turnWidgets.clear();
    m_turnSignatures.clear();
}

void ConversationView::clearConversation()
{
    m_messages.clear();
    m_turnSignatures.clear();
    m_generating = false;

    destroyTurnWidgets();

    if (m_stickyPrompt != nullptr) {
        m_stickyPrompt->hide();
    }
    m_stickyPromptVisible = false;
    updateEmptyState();
    updateOverlayGeometry();
}

void ConversationView::setMessages(const QList<qtcode::agents::AgentMessage> &messages)
{
    syncMessages(messages, m_generating);
}

void ConversationView::syncMessages(
    const QList<qtcode::agents::AgentMessage> &messages,
    bool generating)
{
    m_generating = generating;

    if (messages.isEmpty()) {
        clearConversation();
        return;
    }

    const QScrollBar *scrollBar = m_scrollArea->verticalScrollBar();
    const bool stickToBottom = scrollBar == nullptr
        || scrollBar->value() >= scrollBar->maximum() - 24;

    const QList<ConversationTurn> turns = groupMessagesIntoTurns(messages);
    QStringList nextSignatures;
    nextSignatures.reserve(turns.size());
    for (const ConversationTurn &turn : turns) {
        nextSignatures.append(turn.signature());
    }

    const bool canIncrementalUpdate = turns.size() >= m_turnWidgets.size()
        && turns.size() > 0
        && messages.size() >= m_messages.size();

    if (!canIncrementalUpdate || turns.size() < m_turnWidgets.size()) {
        rebuildTurns(messages);
    } else {
        while (m_turnWidgets.size() < turns.size()) {
            auto *turnWidget = new ConversationTurnWidget(m_contentWidget);
            m_turnsLayout->insertWidget(m_turnsLayout->count() - 1, turnWidget);
            m_turnWidgets.append(turnWidget);
        }

        for (int index = 0; index < turns.size(); ++index) {
            if (index >= m_turnSignatures.size()
                || m_turnSignatures.at(index) != nextSignatures.at(index)) {
                m_turnWidgets.at(index)->setTurn(turns.at(index));
            }
            m_turnWidgets.at(index)->setPresentation(
                presentationForTurn(index, turns.size(), m_generating));
        }
        m_turnSignatures = nextSignatures;
    }

    m_messages = messages;
    applyGeneratingState();
    updateEmptyState();
    m_contentWidget->adjustSize();

    if (stickToBottom || m_cachedStickToBottom) {
        scrollViewToBottom();
    }

    updateStickyPrompt();
    updateOverlayGeometry();
}

void ConversationView::setGenerating(bool generating)
{
    if (m_generating == generating) {
        return;
    }

    m_generating = generating;
    applyGeneratingState();
}

QScrollBar *ConversationView::verticalScrollBar() const
{
    return m_scrollArea != nullptr ? m_scrollArea->verticalScrollBar() : nullptr;
}

void ConversationView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (m_shuttingDown) {
        return;
    }

    updateOverlayGeometry();
    updateStickyPrompt();
}

bool ConversationView::eventFilter(QObject *watched, QEvent *event)
{
    if (m_shuttingDown) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == m_scrollArea->viewport() && event->type() == QEvent::Resize) {
        updateOverlayGeometry();
        QTimer::singleShot(0, this, &ConversationView::updateStickyPrompt);
    }

    return QWidget::eventFilter(watched, event);
}

void ConversationView::rebuildTurns(const QList<qtcode::agents::AgentMessage> &messages)
{
    destroyTurnWidgets();

    const QList<ConversationTurn> turns = groupMessagesIntoTurns(messages);
    for (int index = 0; index < turns.size(); ++index) {
        auto *turnWidget = new ConversationTurnWidget(m_contentWidget);
        turnWidget->setTurn(turns.at(index));
        turnWidget->setPresentation(presentationForTurn(index, turns.size(), m_generating));
        m_turnsLayout->insertWidget(m_turnsLayout->count() - 1, turnWidget);
        m_turnWidgets.append(turnWidget);
        m_turnSignatures.append(turns.at(index).signature());
    }
}

void ConversationView::applyGeneratingState()
{
    for (int index = 0; index < m_turnWidgets.size(); ++index) {
        m_turnWidgets.at(index)->setPresentation(
            presentationForTurn(index, m_turnWidgets.size(), m_generating));
    }
}

void ConversationView::scrollViewToBottom()
{
    if (m_shuttingDown || m_scrollArea == nullptr) {
        return;
    }

    const auto applyScroll = [this]() {
        if (m_shuttingDown || m_scrollArea == nullptr) {
            return;
        }

        if (QScrollBar *scrollBar = m_scrollArea->verticalScrollBar()) {
            scrollBar->setValue(scrollBar->maximum());
        }
        m_cachedStickToBottom = true;
    };

    applyScroll();
    QTimer::singleShot(0, this, applyScroll);
}

void ConversationView::updateStickyPrompt()
{
    if (m_shuttingDown || m_scrollArea == nullptr || m_scrollArea->viewport() == nullptr) {
        return;
    }
    if (m_turnWidgets.isEmpty()) {
        if (m_stickyPromptVisible) {
            m_stickyPrompt->hide();
            m_stickyPromptVisible = false;
        }
        return;
    }

    ConversationTurnWidget *activeTurn = m_turnWidgets.constLast();
    QWidget *humanBubble = activeTurn->humanBubbleWidget();
    if (humanBubble == nullptr || !humanBubble->isVisible()) {
        if (m_stickyPromptVisible) {
            m_stickyPrompt->hide();
            m_stickyPromptVisible = false;
        }
        return;
    }

    if (m_cachedStickToBottom) {
        if (m_stickyPromptVisible) {
            m_stickyPrompt->hide();
            m_stickyPromptVisible = false;
        }
        return;
    }

    const QPoint bubbleTopInViewport =
        humanBubble->mapTo(m_scrollArea->viewport(), QPoint(0, 0));
    const bool humanBubbleAboveViewport = bubbleTopInViewport.y() < -4;
    const bool shouldStick = humanBubbleAboveViewport && m_turnWidgets.size() > 0;

    if (!shouldStick) {
        if (m_stickyPromptVisible) {
            m_stickyPrompt->hide();
            m_stickyPromptVisible = false;
        }
        return;
    }

    const QList<ConversationTurn> turns = groupMessagesIntoTurns(m_messages);
    if (turns.isEmpty()) {
        if (m_stickyPromptVisible) {
            m_stickyPrompt->hide();
            m_stickyPromptVisible = false;
        }
        return;
    }

    const QString promptText = turns.constLast().userMessage.content.trimmed();
    if (promptText.isEmpty()) {
        if (m_stickyPromptVisible) {
            m_stickyPrompt->hide();
            m_stickyPromptVisible = false;
        }
        return;
    }

    m_stickyPromptLabel->setText(promptText);
    if (!m_stickyPromptVisible) {
        m_stickyPrompt->show();
        m_stickyPromptVisible = true;
    }
    m_stickyPrompt->raise();
    updateOverlayGeometry();
}

void ConversationView::updateEmptyState()
{
    const bool hasMessages = !m_messages.isEmpty();
    m_emptyLabel->setVisible(!hasMessages);

    if (!hasMessages) {
        if (m_stickyPromptVisible) {
            m_stickyPrompt->hide();
            m_stickyPromptVisible = false;
        }
    }

    updateOverlayGeometry();
}

void ConversationView::updateOverlayGeometry()
{
    if (m_shuttingDown || m_scrollArea == nullptr || m_scrollArea->viewport() == nullptr) {
        return;
    }

    QWidget *viewport = m_scrollArea->viewport();
    const QRect area = viewport->rect();

    if (m_emptyLabel != nullptr) {
        const int horizontalPadding = 24;
        const int labelWidth = qMax(120, area.width() - horizontalPadding * 2);
        const int labelHeight = m_emptyLabel->heightForWidth(labelWidth);
        m_emptyLabel->setGeometry(
            (area.width() - labelWidth) / 2,
            qMax(24, (area.height() - labelHeight) / 3),
            labelWidth,
            labelHeight);
        if (m_emptyLabel->isVisible()) {
            m_emptyLabel->raise();
        }
    }

    if (m_stickyPrompt != nullptr && m_stickyPrompt->isVisible()) {
        m_stickyPrompt->setFixedWidth(width());
        m_stickyPrompt->move(0, 0);
    }
}

void ConversationView::applyChromeStyles()
{
    m_stickyPrompt->setStyleSheet(QStringLiteral(
        "QFrame#conversationStickyPrompt {"
        "  background-color: palette(window);"
        "  border-bottom: 1px solid palette(mid);"
        "}"
        "QFrame#conversationStickyPrompt QLabel {"
        "  background-color: rgba(%1, %2, %3, 0.14);"
        "  border: 1px solid palette(mid);"
        "  border-left: 3px solid palette(highlight);"
        "  border-radius: 10px;"
        "  padding: 8px 10px;"
        "}")
        .arg(palette().color(QPalette::Highlight).red())
        .arg(palette().color(QPalette::Highlight).green())
        .arg(palette().color(QPalette::Highlight).blue()));

    setStyleSheet(QStringLiteral(
        "QFrame#conversationToolCard,"
        "QFrame#conversationToolCardShell,"
        "QFrame#conversationToolCardFile,"
        "QFrame#conversationToolCardSearch {"
        "  background-color: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "}"
        "QFrame#conversationToolCardShell {"
        "  border-left: 3px solid palette(highlight);"
        "}"
        "QFrame#conversationToolCardFile {"
        "  border-left: 3px solid palette(link);"
        "}"
        "QFrame#conversationToolCardSearch {"
        "  border-left: 3px solid palette(midlight);"
        "}"
        "QFrame#conversationToolCardHeader {"
        "  border-bottom: 1px solid palette(mid);"
        "  color: palette(mid);"
        "}"
        "QLabel#conversationToolCardBody {"
        "  color: palette(text);"
        "}"
        "QPushButton#conversationTurnCollapsedSummary {"
        "  color: palette(mid);"
        "  text-align: left;"
        "  padding: 2px 0;"
        "}"
        "QLabel#conversationTurnFooter {"
        "  color: palette(mid);"
        "  font-size: 11px;"
        "  padding-top: 2px;"
        "}"));
}

} // namespace qtcode::ui
