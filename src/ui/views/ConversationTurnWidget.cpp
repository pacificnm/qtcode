#include "ui/views/ConversationTurnWidget.h"

#include "ui/views/ConversationFormatting.h"
#include "ui/widgets/CollapsibleSection.h"

#include <KLocalizedString>

#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace qtcode::ui {

namespace {

constexpr int kPromptBubbleMaxWidthPercent = 86;

class ConversationDocumentBlock final : public QLabel
{
public:
    explicit ConversationDocumentBlock(
        const QString &objectName,
        QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setObjectName(objectName);
        setWordWrap(true);
        setTextFormat(Qt::RichText);
        setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        setOpenExternalLinks(false);
        setAlignment(Qt::AlignTop | Qt::AlignLeft);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        setStyleSheet(QStringLiteral("background: transparent;"));
    }

    [[nodiscard]] bool hasHeightForWidth() const override
    {
        return true;
    }

    [[nodiscard]] int heightForWidth(int width) const override
    {
        return QLabel::heightForWidth(width);
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return QLabel::sizeHint();
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return QLabel::minimumSizeHint();
    }

    void setHtml(const QString &html)
    {
        if (m_html == html) {
            return;
        }

        m_html = html;
        setText(html);
        setProperty("conversationHtml", html);
        updateGeometry();
    }

private:
    QString m_html;
};

QString displayActivityDetail(const ParsedActivity &parsed)
{
    if (parsed.detail.isEmpty()) {
        return {};
    }

    if (parsed.kind == ActivityKind::Shell) {
        return truncateForDisplay(parsed.detail, 160);
    }

    if (parsed.kind == ActivityKind::Search) {
        return truncateForDisplay(parsed.detail, 120);
    }

    return shortPath(parsed.detail);
}

} // namespace

ConversationTurnWidget::ConversationTurnWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 8);
    layout->setSpacing(6);

    m_humanBubble = new QFrame(this);
    m_humanBubble->setObjectName(QStringLiteral("conversationHumanBubble"));
    m_humanBubble->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *bubbleLayout = new QVBoxLayout(m_humanBubble);
    bubbleLayout->setContentsMargins(10, 8, 10, 8);
    bubbleLayout->setSpacing(0);

    m_humanContent = new ConversationDocumentBlock(
        QStringLiteral("conversationUserPromptBlock"),
        m_humanBubble);
    bubbleLayout->addWidget(m_humanContent);

    m_collapsedSummary = new QPushButton(this);
    m_collapsedSummary->setObjectName(QStringLiteral("conversationTurnCollapsedSummary"));
    m_collapsedSummary->setFlat(true);
    m_collapsedSummary->setCursor(Qt::PointingHandCursor);
    m_collapsedSummary->setVisible(false);
    connect(m_collapsedSummary, &QPushButton::clicked, this, &ConversationTurnWidget::onCollapsedSummaryClicked);

    m_aiContainer = new QWidget(this);
    m_aiContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_aiLayout = new QVBoxLayout(m_aiContainer);
    m_aiLayout->setContentsMargins(0, 0, 0, 0);
    m_aiLayout->setSpacing(6);

    m_footerLabel = new QLabel(this);
    m_footerLabel->setObjectName(QStringLiteral("conversationTurnFooter"));
    m_footerLabel->setVisible(false);
    m_footerLabel->setCursor(Qt::PointingHandCursor);
    m_footerLabel->installEventFilter(this);

    auto *bubbleRow = new QWidget(this);
    bubbleRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *bubbleRowLayout = new QHBoxLayout(bubbleRow);
    bubbleRowLayout->setContentsMargins(0, 0, 0, 0);
    bubbleRowLayout->setSpacing(0);
    bubbleRowLayout->addStretch(1);
    bubbleRowLayout->addWidget(m_humanBubble);

    layout->addWidget(bubbleRow);
    layout->addWidget(m_collapsedSummary, 0, Qt::AlignLeft);
    layout->addWidget(m_aiContainer, 0);
    layout->addWidget(m_footerLabel, 0, Qt::AlignLeft);

    applyHumanBubbleStyle();
}

ConversationTurnWidget::~ConversationTurnWidget()
{
    m_shuttingDown = true;

    if (m_footerLabel != nullptr) {
        m_footerLabel->removeEventFilter(this);
    }

    setGraphicsEffect(nullptr);
    m_assistantBlocks.clear();
}

void ConversationTurnWidget::setTurn(const ConversationTurn &turn)
{
    m_turn = turn;
    m_signature = turn.signature();

    if (auto *humanContent = static_cast<ConversationDocumentBlock *>(m_humanContent)) {
        humanContent->setHtml(formatContentHtml(turn.userMessage.content, palette()));
    }
    m_humanBubble->setVisible(!turn.userMessage.content.trimmed().isEmpty()
        || turn.userMessage.id != QStringLiteral("orphan-user"));
    updateHumanBubbleWidth();

    rebuildAiContent(turn);
    updateFooter(turn);
    applyPresentation();
}

void ConversationTurnWidget::setPresentation(const TurnPresentation &presentation)
{
    const bool activeChanged = m_presentation.isActiveTurn != presentation.isActiveTurn;
    const bool generatingChanged = m_presentation.generating != presentation.generating;
    m_presentation = presentation;

    if (!presentation.generating) {
        m_userExpanded = false;
    }

    if ((activeChanged || generatingChanged) && !m_turn.responses.isEmpty()) {
        rebuildAiContent(m_turn);
    }

    applyPresentation();
}

void ConversationTurnWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (!m_shuttingDown) {
        updateHumanBubbleWidth();
    }
}

bool ConversationTurnWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_footerLabel && event->type() == QEvent::MouseButtonRelease && m_collapsed) {
        onCollapsedSummaryClicked();
        return true;
    }

    return QWidget::eventFilter(watched, event);
}

QString ConversationTurnWidget::signature() const
{
    return m_signature;
}

QWidget *ConversationTurnWidget::humanBubbleWidget() const
{
    return m_humanBubble;
}

void ConversationTurnWidget::onCollapsedSummaryClicked()
{
    m_userExpanded = true;
    setCollapsed(false);
}

void ConversationTurnWidget::rebuildAiContent(const ConversationTurn &turn)
{
    m_assistantBlocks.clear();

    while (QLayoutItem *item = m_aiLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            delete widget;
        }
        delete item;
    }

    const QList<TurnContentSegment> segments = segmentTurnResponses(turn.responses);

    for (const TurnContentSegment &segment : segments) {
        if (segment.kind == TurnContentSegmentKind::Assistant) {
            for (const qtcode::agents::AgentMessage &message : segment.messages) {
                m_aiLayout->addWidget(createAssistantBlock(message));
            }
            continue;
        }

        if (segment.messages.size() == 1) {
            m_aiLayout->addWidget(createToolCallCard(segment.messages.constFirst()));
            continue;
        }

        auto *group = new CollapsibleSection(
            activityGroupTitle(segment.messages),
            false,
            m_aiContainer);
        for (const qtcode::agents::AgentMessage &activity : segment.messages) {
            group->contentLayout()->addWidget(createToolCallCard(activity));
        }
        m_aiLayout->addWidget(group);
    }

    updateGeometry();
}

void ConversationTurnWidget::applyPresentation()
{
    const bool hasAiContent = m_turn.activityCount() + m_turn.assistantReplyCount() > 0;
    const bool autoCollapsed = m_presentation.generating
        && !m_presentation.isActiveTurn
        && hasAiContent
        && !m_userExpanded;
    setCollapsed(autoCollapsed);
    setDimmed(m_presentation.generating && !m_presentation.isActiveTurn);

    const bool showFooter = hasAiContent
        && (!m_presentation.isActiveTurn || !m_presentation.generating)
        && !m_collapsed;
    m_footerLabel->setVisible(showFooter);
}

void ConversationTurnWidget::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    m_collapsedSummary->setVisible(collapsed);
    m_collapsedSummary->setText(m_turn.collapsedSummaryText());
    m_aiContainer->setVisible(!collapsed);
    m_humanBubble->setVisible(!collapsed
        && (!m_turn.userMessage.content.trimmed().isEmpty()
            || m_turn.userMessage.id != QStringLiteral("orphan-user")));
}

void ConversationTurnWidget::setDimmed(bool dimmed)
{
    if (dimmed) {
        auto *effect = qobject_cast<QGraphicsOpacityEffect *>(graphicsEffect());
        if (effect == nullptr) {
            effect = new QGraphicsOpacityEffect(this);
            setGraphicsEffect(effect);
        }
        effect->setOpacity(0.55);
        return;
    }

    setGraphicsEffect(nullptr);
}

void ConversationTurnWidget::updateFooter(const ConversationTurn &turn)
{
    m_footerLabel->setText(turn.footerSummaryText());
}

void ConversationTurnWidget::updateHumanBubbleWidth()
{
    if (m_humanBubble == nullptr) {
        return;
    }

    const int availableWidth = width();
    if (availableWidth <= 0) {
        return;
    }

    const int bubbleWidth = qMax(260, availableWidth * kPromptBubbleMaxWidthPercent / 100);
    const int targetWidth = qMin(availableWidth, bubbleWidth);
    if (m_humanBubble->minimumWidth() == targetWidth
        && m_humanBubble->maximumWidth() == targetWidth) {
        return;
    }

    m_humanBubble->setFixedWidth(targetWidth);
}

QWidget *ConversationTurnWidget::createToolCallCard(
    const qtcode::agents::AgentMessage &message) const
{
    const ParsedActivity parsed = parseActivityText(message.content);
    const QString displayDetail = displayActivityDetail(parsed);
    const QString headerLabel = activityCardHeaderLabel(parsed);

    auto *card = new QFrame();
    card->setObjectName(activityCardObjectName(parsed.kind));
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    auto *header = new QFrame(card);
    header->setObjectName(QStringLiteral("conversationToolCardHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLayout->setSpacing(6);

    auto *verbLabel = new QLabel(headerLabel, header);
    verbLabel->setObjectName(QStringLiteral("conversationToolCardHeaderLabel"));
    QFont headerFont = verbLabel->font();
    headerFont.setPointSize(std::max(headerFont.pointSize() - 1, 8));
    verbLabel->setFont(headerFont);
    headerLayout->addWidget(verbLabel, 1);

    cardLayout->addWidget(header);

    if (!displayDetail.isEmpty()) {
        auto *body = new QLabel(displayDetail, card);
        body->setObjectName(QStringLiteral("conversationToolCardBody"));
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QFont bodyFont = body->font();
        bodyFont.setFamily(QStringLiteral("monospace"));
        bodyFont.setPointSize(std::max(bodyFont.pointSize() - 1, 8));
        bodyFont.setStyleHint(QFont::Monospace);
        body->setFont(bodyFont);
        body->setContentsMargins(8, 4, 8, 6);
        cardLayout->addWidget(body);
    }

    return card;
}

QWidget *ConversationTurnWidget::createAssistantBlock(
    const qtcode::agents::AgentMessage &message)
{
    auto *block = new ConversationDocumentBlock(
        QStringLiteral("conversationAssistantMessageBlock"),
        m_aiContainer);
    block->setHtml(formatContentHtml(message.content, palette()));
    m_assistantBlocks.append(block);
    return block;
}

void ConversationTurnWidget::applyHumanBubbleStyle()
{
    const QColor highlight = palette().color(QPalette::Highlight);
    m_humanBubble->setStyleSheet(QStringLiteral(
        "QFrame#conversationHumanBubble {"
        "  background-color: rgba(%1, %2, %3, 0.14);"
        "  border: 1px solid palette(mid);"
        "  border-left: 3px solid palette(highlight);"
        "  border-radius: 10px;"
        "}")
        .arg(highlight.red())
        .arg(highlight.green())
        .arg(highlight.blue()));
}

} // namespace qtcode::ui
