#include "github/GitHubRemoteResolver.h"

#include <QRegularExpression>

namespace qtcode::github {

namespace {

GitHubRepositoryIdentity identityFromMatch(const QRegularExpressionMatch &match)
{
    GitHubRepositoryIdentity identity;
    identity.owner = match.captured(QStringLiteral("owner")).trimmed();
    identity.name = match.captured(QStringLiteral("name")).trimmed();
    identity.isGitHub = !identity.owner.isEmpty() && !identity.name.isEmpty();
    return identity;
}

} // namespace

GitHubRepositoryIdentity parseGitHubRemoteUrl(const QString &remoteUrl)
{
    const QString trimmed = remoteUrl.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    static const QRegularExpression httpsPattern(
        QStringLiteral(R"(^https?://github\.com/(?<owner>[^/]+)/(?<name>[^/]+?)(?:\.git)?/?$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (const QRegularExpressionMatch match = httpsPattern.match(trimmed); match.hasMatch()) {
        return identityFromMatch(match);
    }

    static const QRegularExpression scpPattern(
        QStringLiteral(R"(^git@github\.com:(?<owner>[^/]+)/(?<name>[^/]+?)(?:\.git)?/?$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (const QRegularExpressionMatch match = scpPattern.match(trimmed); match.hasMatch()) {
        return identityFromMatch(match);
    }

    static const QRegularExpression sshUrlPattern(
        QStringLiteral(R"(^ssh://git@github\.com/(?<owner>[^/]+)/(?<name>[^/]+?)(?:\.git)?/?$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (const QRegularExpressionMatch match = sshUrlPattern.match(trimmed); match.hasMatch()) {
        return identityFromMatch(match);
    }

    static const QRegularExpression gitPattern(
        QStringLiteral(R"(^git://github\.com/(?<owner>[^/]+)/(?<name>[^/]+?)(?:\.git)?/?$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (const QRegularExpressionMatch match = gitPattern.match(trimmed); match.hasMatch()) {
        return identityFromMatch(match);
    }

    return {};
}

} // namespace qtcode::github
