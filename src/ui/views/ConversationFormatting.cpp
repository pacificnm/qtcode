#include "ui/views/ConversationFormatting.h"

#include <QRegularExpression>

namespace qtcode::ui {

QString truncateForDisplay(const QString &text, int maxLength)
{
    const QString trimmed = text.trimmed();
    if (trimmed.size() <= maxLength) {
        return trimmed;
    }
    return trimmed.left(maxLength - 1) + QStringLiteral("…");
}

QString shortPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return trimmed;
    }

    const QStringList segments = trimmed.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.size() <= 2) {
        return trimmed;
    }

    return segments.mid(segments.size() - 2).join(QLatin1Char('/'));
}

ParsedActivity parseActivityText(const QString &activityText)
{
    ParsedActivity parsed;
    const QString trimmed = activityText.trimmed();
    if (trimmed.isEmpty()) {
        return parsed;
    }

    const int colonIndex = trimmed.indexOf(QLatin1Char(':'));
    parsed.verb = colonIndex > 0 ? trimmed.left(colonIndex).trimmed() : trimmed;
    parsed.detail = colonIndex > 0 ? trimmed.mid(colonIndex + 1).trimmed() : QString();
    parsed.kind = activityKindForVerb(parsed.verb);
    return parsed;
}

ActivityKind activityKindForVerb(const QString &verb)
{
    if (verb == QStringLiteral("Running") || verb == QStringLiteral("Ran")) {
        return ActivityKind::Shell;
    }

    if (verb == QStringLiteral("Read") || verb == QStringLiteral("Write")
        || verb == QStringLiteral("Edit") || verb == QStringLiteral("Delete")
        || verb == QStringLiteral("List")) {
        return ActivityKind::File;
    }

    if (verb == QStringLiteral("Grep") || verb == QStringLiteral("Glob")) {
        return ActivityKind::Search;
    }

    return ActivityKind::Generic;
}

QString activityCardObjectName(ActivityKind kind)
{
    switch (kind) {
    case ActivityKind::Shell:
        return QStringLiteral("conversationToolCardShell");
    case ActivityKind::File:
        return QStringLiteral("conversationToolCardFile");
    case ActivityKind::Search:
        return QStringLiteral("conversationToolCardSearch");
    case ActivityKind::Generic:
        break;
    }

    return QStringLiteral("conversationToolCard");
}

QString activityCardHeaderLabel(const ParsedActivity &parsed)
{
    switch (parsed.kind) {
    case ActivityKind::Shell:
        return QStringLiteral("Shell");
    case ActivityKind::File:
    case ActivityKind::Search:
    case ActivityKind::Generic:
        return parsed.verb;
    }

    return parsed.verb;
}

namespace {

QString formatInlineCodeSegments(const QString &text, const QPalette &palette)
{
    const QColor codeText = palette.color(QPalette::Text);

    QString html;
    html.reserve(text.size() + 32);

    int index = 0;
    while (index < text.size()) {
        const int tickIndex = text.indexOf(QLatin1Char('`'), index);
        if (tickIndex < 0) {
            html.append(QStringView(text).mid(index).toString().toHtmlEscaped());
            break;
        }

        html.append(QStringView(text).mid(index, tickIndex - index).toString().toHtmlEscaped());

        const int closingTickIndex = text.indexOf(QLatin1Char('`'), tickIndex + 1);
        if (closingTickIndex < 0) {
            html.append(QStringView(text).mid(tickIndex).toString().toHtmlEscaped());
            break;
        }

        const QString code = text.mid(tickIndex + 1, closingTickIndex - tickIndex - 1);
        html.append(QStringLiteral(
                        "<code style=\"font-family:monospace;font-size:12px;color:%1;\">%2</code>")
                        .arg(codeText.name(QColor::HexRgb), code.toHtmlEscaped()));
        index = closingTickIndex + 1;
    }

    return html;
}

QString formatProseHtml(const QString &text, const QPalette &palette)
{
    QString escaped = formatInlineCodeSegments(text, palette);
    return escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
}

QString formatCodeBlockHtml(const QString &language, const QString &code, const QPalette &palette)
{
    const QColor blockText = palette.color(QPalette::Text);
    const QColor labelColor = palette.color(QPalette::Text);
    const QString escapedCode =
        code.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br/>"));

    QString html = QStringLiteral(
                       "<div style=\"font-family:monospace;font-size:12px;"
                       "color:%1;margin:6px 0;\">%2</div>")
                       .arg(blockText.name(QColor::HexRgb), escapedCode);

    if (!language.trimmed().isEmpty()) {
        html = QStringLiteral(
                   "<div style=\"font-family:monospace;font-size:11px;color:%1;"
                   "margin:8px 0 2px 0;\">%2</div>")
                   .arg(labelColor.name(QColor::HexRgb), language.trimmed().toHtmlEscaped())
               + html;
    }

    return html;
}

} // namespace

QString formatContentHtml(const QString &content, const QPalette &palette)
{
    static const QRegularExpression fencePattern(
        QStringLiteral("```([\\w.-]*)\\n?([\\s\\S]*?)```"));

    QString html;
    int lastIndex = 0;
    QRegularExpressionMatchIterator iterator = fencePattern.globalMatch(content);
    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        const QString prose = content.mid(lastIndex, match.capturedStart() - lastIndex);
        if (!prose.trimmed().isEmpty()) {
            html.append(formatProseHtml(prose, palette));
        }

        html.append(formatCodeBlockHtml(match.captured(1), match.captured(2), palette));
        lastIndex = match.capturedEnd();
    }

    const QString trailing = content.mid(lastIndex);
    if (!trailing.isEmpty()) {
        html.append(formatProseHtml(trailing, palette));
    }

    return html;
}

QString messageSignature(const QString &id, const QString &role, const QString &content)
{
    return QStringLiteral("%1:%2:%3").arg(id, role, content);
}

} // namespace qtcode::ui
