#include "github/GitHubIssueBranchNaming.h"

#include <QRegularExpression>

#include <algorithm>

namespace qtcode::github {

namespace {

QString slugifyTitle(const QString &title)
{
    QString slug = title.trimmed().toLower();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    slug.replace(QRegularExpression(QStringLiteral("-{2,}")), QStringLiteral("-"));
    slug.remove(QRegularExpression(QStringLiteral("^-|-$")));

    constexpr int kMaxSlugLength = 60;
    if (slug.length() > kMaxSlugLength) {
        slug.truncate(kMaxSlugLength);
        slug.remove(QRegularExpression(QStringLiteral("-+$")));
    }

    return slug;
}

QString normalizeBranchReference(const QString &branchReference)
{
    QString normalized = branchReference.trimmed();
    if (normalized.startsWith(QStringLiteral("remotes/origin/"))) {
        normalized.remove(0, QStringLiteral("remotes/origin/").size());
    } else if (normalized.startsWith(QStringLiteral("origin/"))) {
        normalized.remove(0, QStringLiteral("origin/").size());
    }

    if (normalized == QStringLiteral("HEAD")) {
        return {};
    }

    return normalized;
}

} // namespace

QString suggestedIssueBranchName(int issueNumber, const QString &title)
{
    if (issueNumber <= 0) {
        return {};
    }

    const QString slug = slugifyTitle(title);
    if (slug.isEmpty()) {
        return QString::number(issueNumber);
    }

    return QStringLiteral("%1-%2").arg(issueNumber).arg(slug);
}

QStringList matchingIssueBranches(int issueNumber, const QStringList &repositoryBranchNames)
{
    if (issueNumber <= 0 || repositoryBranchNames.isEmpty()) {
        return {};
    }

    const QString exactPrefix = QStringLiteral("%1-").arg(issueNumber);
    const QString exactNumber = QString::number(issueNumber);
    QStringList matches;

    for (const QString &branchReference : repositoryBranchNames) {
        const QString branchName = normalizeBranchReference(branchReference);
        if (branchName.isEmpty()) {
            continue;
        }

        if (branchName == exactNumber || branchName.startsWith(exactPrefix)) {
            matches.append(branchName);
        }
    }

    matches.removeDuplicates();
    std::sort(matches.begin(), matches.end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });
    return matches;
}

QString resolveIssueBranchName(
    int issueNumber,
    const QStringList &linkedBranches,
    const QStringList &repositoryBranchNames)
{
    const QStringList linkedMatches = matchingIssueBranches(issueNumber, linkedBranches);
    if (!linkedMatches.isEmpty()) {
        return linkedMatches.first();
    }

    for (const QString &linkedBranch : linkedBranches) {
        const QString trimmedBranch = linkedBranch.trimmed();
        if (!trimmedBranch.isEmpty()) {
            return trimmedBranch;
        }
    }

    const QStringList repositoryMatches = matchingIssueBranches(issueNumber, repositoryBranchNames);
    if (!repositoryMatches.isEmpty()) {
        return repositoryMatches.first();
    }

    return {};
}

} // namespace qtcode::github
