#include "ui/views/ConversationView.h"

#include "agents/AgentModels.h"

#include <KLocalizedString>

#include <QBuffer>
#include <QFrame>
#include <QIcon>
#include <QPalette>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

namespace qtcode::ui {

namespace {

[[nodiscard]] QString messageSignature(const qtcode::agents::AgentMessage &message)
{
    return QStringLiteral("%1:%2:%3")
        .arg(message.id, message.role, message.content);
}

} // namespace

ConversationView::ConversationView(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_view = new QTextEdit(this);
    m_view->setReadOnly(true);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setPlaceholderText(i18n("Start a conversation with the agent…"));
    layout->addWidget(m_view);
}

void ConversationView::clearConversation()
{
    m_cachedMessageCount = 0;
    m_cachedLastMessageSignature.clear();
    m_messageHtmlBlocks.clear();
    m_messageSignatures.clear();
    if (m_view != nullptr) {
        m_view->clear();
    }
}

void ConversationView::setMessages(const QList<qtcode::agents::AgentMessage> &messages)
{
    syncMessages(messages, false, {});
}

void ConversationView::syncMessages(
    const QList<qtcode::agents::AgentMessage> &messages,
    bool showActivityIndicator,
    const QString &activityText)
{
    if (m_view == nullptr) {
        return;
    }

    if (messages.isEmpty()) {
        clearConversation();
        if (showActivityIndicator) {
            renderDocument(messages, showActivityIndicator, activityText);
        }
        return;
    }

    const QString lastSignature =
        messageSignature(messages.constLast());
    const bool canIncrementalUpdate = messages.size() == m_cachedMessageCount
        && m_cachedMessageCount > 0
        && lastSignature != m_cachedLastMessageSignature;
    const bool canAppendMessage = messages.size() == m_cachedMessageCount + 1;

    if (canIncrementalUpdate || canAppendMessage) {
        m_cachedMessageCount = messages.size();
        m_cachedLastMessageSignature = lastSignature;
        renderDocument(messages, showActivityIndicator, activityText);
        return;
    }

    rebuildAllMessages(messages);
    renderDocument(messages, showActivityIndicator, activityText);
}

void ConversationView::rebuildAllMessages(const QList<qtcode::agents::AgentMessage> &messages)
{
    m_cachedMessageCount = messages.size();
    m_cachedLastMessageSignature = messages.isEmpty()
        ? QString()
        : messageSignature(messages.constLast());
    m_messageHtmlBlocks.clear();
    m_messageSignatures.clear();
    m_messageHtmlBlocks.reserve(messages.size());
    m_messageSignatures.reserve(messages.size());
    for (const qtcode::agents::AgentMessage &message : messages) {
        m_messageSignatures.append(messageSignature(message));
        m_messageHtmlBlocks.append(formatMessageHtml(message, palette()));
    }
}

void ConversationView::renderDocument(
    const QList<qtcode::agents::AgentMessage> &messages,
    bool showActivityIndicator,
    const QString &activityText)
{
    if (m_view == nullptr) {
        return;
    }

    while (m_messageHtmlBlocks.size() < messages.size()) {
        const qtcode::agents::AgentMessage &message = messages.at(m_messageHtmlBlocks.size());
        m_messageSignatures.append(messageSignature(message));
        m_messageHtmlBlocks.append(formatMessageHtml(message, palette()));
    }
    while (m_messageHtmlBlocks.size() > messages.size()) {
        m_messageHtmlBlocks.removeLast();
        m_messageSignatures.removeLast();
    }

    for (int index = 0; index < messages.size(); ++index) {
        const QString signature = messageSignature(messages.at(index));
        if (m_messageSignatures.at(index) == signature) {
            continue;
        }

        m_messageSignatures[index] = signature;
        m_messageHtmlBlocks[index] = formatMessageHtml(messages.at(index), palette());
    }

    const QScrollBar *scrollBar = m_view->verticalScrollBar();
    const bool stickToBottom = scrollBar == nullptr
        || scrollBar->value() >= scrollBar->maximum() - 24;

    QStringList blocks = m_messageHtmlBlocks;
    if (showActivityIndicator) {
        blocks.append(formatActivityIndicatorHtml(activityText, palette()));
    }

    m_view->setHtml(blocks.join(QStringLiteral("<hr style=\"border:none;height:8px;\"/>")));
    if (stickToBottom) {
        m_view->moveCursor(QTextCursor::End);
    }
}

QString ConversationView::iconToHtml(const QIcon &icon, int size)
{
    if (icon.isNull()) {
        return {};
    }

    const QPixmap pixmap = icon.pixmap(size, size);
    if (pixmap.isNull()) {
        return {};
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");

    return QStringLiteral(
               "<img src=\"data:image/png;base64,%1\" width=\"%2\" height=\"%2\" "
               "style=\"vertical-align:middle;\"/>")
        .arg(QString::fromLatin1(bytes.toBase64()), QString::number(size));
}

QString ConversationView::formatMessageHtml(
    const qtcode::agents::AgentMessage &message,
    const QPalette &palette)
{
    const bool isUser = message.role == QStringLiteral("user");
    const bool isActivity = message.role == QStringLiteral("activity");
    const QString senderLabel = isUser
        ? i18n("You")
        : (isActivity ? i18n("Activity") : i18n("Agent"));
    const QString iconName = isUser
        ? QStringLiteral("user-identity")
        : (isActivity ? QStringLiteral("system-run") : QStringLiteral("irc-bot"));
    const QIcon senderIcon = QIcon::fromTheme(iconName);

    const QColor bubbleBackground = isUser
        ? palette.color(QPalette::Highlight).lighter(175)
        : (isActivity ? palette.color(QPalette::AlternateBase).lighter(108)
                      : palette.color(QPalette::Base));
    const QColor senderColor = isUser
        ? palette.color(QPalette::Highlight).darker(130)
        : (isActivity ? palette.color(QPalette::Mid)
                      : palette.color(QPalette::Link));
    const QColor textColor = isActivity
        ? palette.color(QPalette::Mid)
        : palette.color(QPalette::Text);

    const QString iconHtml = iconToHtml(senderIcon, 20);
    const QString escapedContent = QString(message.content).toHtmlEscaped().replace(
        QLatin1Char('\n'),
        QStringLiteral("<br/>"));

    return QStringLiteral(
               "<div style=\"margin:4px 0;padding:10px 12px;border-radius:8px;"
               "background-color:%1;color:%2;\">"
               "<table cellspacing=\"0\" cellpadding=\"0\" style=\"border:none;width:100%%;\">"
               "<tr>"
               "<td style=\"vertical-align:top;padding-right:10px;width:24px;\">%3</td>"
               "<td style=\"vertical-align:top;\">"
               "<div style=\"font-weight:600;color:%4;margin-bottom:4px;\">%5</div>"
               "<div style=\"white-space:pre-wrap;%6\">%7</div>"
               "</td>"
               "</tr>"
               "</table>"
               "</div>")
        .arg(
            bubbleBackground.name(QColor::HexRgb),
            textColor.name(QColor::HexRgb),
            iconHtml,
            senderColor.name(QColor::HexRgb),
            senderLabel.toHtmlEscaped(),
            isActivity ? QStringLiteral("font-style:italic;") : QString(),
            escapedContent);
}

QString ConversationView::formatActivityIndicatorHtml(
    const QString &activityText,
    const QPalette &palette)
{
    const QString label = activityText.trimmed().isEmpty()
        ? i18n("Agent is working…")
        : activityText.trimmed();
    const QIcon indicatorIcon = QIcon::fromTheme(QStringLiteral("view-refresh"));
    const QColor bubbleBackground = palette.color(QPalette::AlternateBase).lighter(104);
    const QColor textColor = palette.color(QPalette::Mid);

    return QStringLiteral(
               "<div style=\"margin:4px 0;padding:8px 12px;border-radius:8px;"
               "background-color:%1;color:%2;font-style:italic;\">"
               "%3 %4"
               "</div>")
        .arg(
            bubbleBackground.name(QColor::HexRgb),
            textColor.name(QColor::HexRgb),
            iconToHtml(indicatorIcon, 16),
            label.toHtmlEscaped());
}

} // namespace qtcode::ui
