#include "github/GitHubIssueBranchNaming.h"

#include <QtTest>

class GitHubIssueBranchNamingTest final : public QObject
{
    Q_OBJECT

private slots:
    void suggestedIssueBranchNameUsesIssueNumberAndSlug();
    void matchingIssueBranchesFindsLocalAndRemoteNames();
    void resolveIssueBranchPrefersLinkedBranch();
};

void GitHubIssueBranchNamingTest::suggestedIssueBranchNameUsesIssueNumberAndSlug()
{
    QCOMPARE(
        qtcode::github::suggestedIssueBranchName(
            125,
            QStringLiteral("Define QTCommands models and CommandLibraryService discovery")),
        QStringLiteral("125-define-qtcommands-models-and-commandlibraryservice-discovery"));
}

void GitHubIssueBranchNamingTest::matchingIssueBranchesFindsLocalAndRemoteNames()
{
    const QStringList branches {
        QStringLiteral("main"),
        QStringLiteral("113-add-shared-repositoryfiles-left-project-space"),
        QStringLiteral("origin/113-add-shared-repositoryfiles-left-project-space"),
        QStringLiteral("feature/other")};

    const QStringList matches = qtcode::github::matchingIssueBranches(113, branches);
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.first(), QStringLiteral("113-add-shared-repositoryfiles-left-project-space"));
}

void GitHubIssueBranchNamingTest::resolveIssueBranchPrefersLinkedBranch()
{
    const QStringList linked {QStringLiteral("125-custom-linked-branch")};
    const QStringList repositoryBranches {
        QStringLiteral("main"),
        QStringLiteral("125-define-qtcommands-models")};

    QCOMPARE(
        qtcode::github::resolveIssueBranchName(125, linked, repositoryBranches),
        QStringLiteral("125-custom-linked-branch"));
}

QTEST_MAIN(GitHubIssueBranchNamingTest)
#include "GitHubIssueBranchNamingTest.moc"
