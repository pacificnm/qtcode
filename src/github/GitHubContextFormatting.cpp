#include "github/GitHubContextFormatting.h"

#include "github/GitHubModels.h"

namespace qtcode::github {

QString formatPullRequestContextExcerpt(const GitHubPullRequestDetail &detail, int maxBodyLength)
{
    QStringList lines;
    lines.append(QStringLiteral("GitHub pull request #%1: %2")
                     .arg(QString::number(detail.number), detail.title));
    if (!detail.state.isEmpty()) {
        lines.append(QStringLiteral("State: %1").arg(detail.state));
    }
    if (!detail.author.isEmpty()) {
        lines.append(QStringLiteral("Author: %1").arg(detail.author));
    }
    if (!detail.baseRef.isEmpty() || !detail.headRef.isEmpty()) {
        lines.append(QStringLiteral("Branches: %1 <- %2").arg(detail.baseRef, detail.headRef));
    }
    if (!detail.url.isEmpty()) {
        lines.append(QStringLiteral("URL: %1").arg(detail.url));
    }
    if (!detail.body.isEmpty()) {
        QString body = detail.body;
        if (maxBodyLength > 0 && body.size() > maxBodyLength) {
            body = body.left(maxBodyLength).trimmed() + QStringLiteral("…");
        }
        lines.append(QStringLiteral("Body:"));
        lines.append(body);
    }

    return lines.join(QStringLiteral("\n"));
}

} // namespace qtcode::github
