#include "ui/views/ConversationView.h"

#include "agents/AgentModels.h"

#include <KLocalizedString>

#include <QBuffer>
#include <QFrame>
#include <QIcon>
#include <QPalette>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

namespace qtcode::ui {

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
    if (m_view != nullptr) {
        m_view->clear();
    }
}

void ConversationView::setMessages(const QList<qtcode::agents::AgentMessage> &messages)
{
    if (m_view == nullptr) {
        return;
    }

    if (messages.isEmpty()) {
        m_view->clear();
        return;
    }

    QStringList blocks;
    blocks.reserve(messages.size());
    for (const qtcode::agents::AgentMessage &message : messages) {
        blocks.append(formatMessageHtml(message, palette()));
    }

    m_view->setHtml(blocks.join(QStringLiteral("<hr style=\"border:none;height:8px;\"/>")));
    m_view->moveCursor(QTextCursor::End);
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
    const QString senderLabel = isUser ? i18n("You") : i18n("Agent");
    const QIcon senderIcon = QIcon::fromTheme(
        isUser ? QStringLiteral("user-identity") : QStringLiteral("irc-bot"));

    const QColor bubbleBackground = isUser
        ? palette.color(QPalette::Highlight).lighter(175)
        : palette.color(QPalette::Base);
    const QColor senderColor = isUser
        ? palette.color(QPalette::Highlight).darker(130)
        : palette.color(QPalette::Link);
    const QColor textColor = palette.color(QPalette::Text);

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
               "<div style=\"white-space:pre-wrap;\">%6</div>"
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
            escapedContent);
}

} // namespace qtcode::ui
