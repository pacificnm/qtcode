#pragma once

#include "github/GitHubCachePolicy.h"
#include "github/GitHubModels.h"

#include <QHash>
#include <QWidget>

class QTabWidget;

namespace qtcode::core {
class AppConfigService;
class ProjectManager;
class StatusService;
} // namespace qtcode::core

namespace qtcode::ui {

class EditorTab;
class GitHubDetailView;
class RepoHelpView;

class WorkspaceTabs final : public QWidget
{
    Q_OBJECT

public:
    explicit WorkspaceTabs(
        qtcode::core::StatusService *statusService,
        qtcode::core::ProjectManager *projectManager,
        qtcode::core::AppConfigService *appConfigService,
        QWidget *parent = nullptr);

    void setPermanentAiChatTab(QWidget *conversationPanel);

    [[nodiscard]] int aiChatTabIndex() const;
    [[nodiscard]] int tabCount() const;
    [[nodiscard]] bool closeAllEditorTabs(bool promptForDirty);
    [[nodiscard]] bool saveCurrentEditorTab();
    [[nodiscard]] bool hasActiveEditorTab() const;
    [[nodiscard]] bool currentEditorTabIsModified() const;

signals:
    void activeEditorStateChanged(bool hasActiveEditor, bool isModified);
    void issueContextSelected(const qtcode::github::GitHubIssueDetail &detail);
    void pullRequestContextSelected(const qtcode::github::GitHubPullRequestDetail &detail);
    void fileContextSelected(const QString &absolutePath, const QString &contentOverride);

public slots:
    void requestOpenFile(const QString &absolutePath);
    void requestOpenIssue(
        const qtcode::github::GitHubIssueDetail &detail,
        const qtcode::github::GitHubCacheMetadata &cacheMetadata);
    void requestOpenPullRequest(
        const qtcode::github::GitHubPullRequestDetail &detail,
        const qtcode::github::GitHubCacheMetadata &cacheMetadata);
    void requestOpenRepoHelp();
    void closeCurrentEditorTab();
    void handleFileRenamed(const QString &oldPath, const QString &newPath);
    void handleFileDeleted(const QString &path);
    void handleDirectoryDeleted(const QString &directoryPath);
    void handlePathDeleted(const QString &path, bool isDirectory);

private slots:
    void onTabCloseRequested(int index);
    void onActiveProjectChanged();
    void onCurrentTabChanged(int index);
    void showTabContextMenu(const QPoint &position);

private:
    void configureLayout();
    void refreshPermanentTabCloseButton();
    [[nodiscard]] QString normalizedPath(const QString &absolutePath) const;
    [[nodiscard]] EditorTab *editorTabAt(int index) const;
    [[nodiscard]] EditorTab *currentEditorTab() const;
    [[nodiscard]] bool isEditorTabIndex(int index) const;
    [[nodiscard]] bool looksBinaryFile(const QString &absolutePath) const;
    [[nodiscard]] bool closeEditorTabAt(int index, bool promptForDirty);
    [[nodiscard]] bool closeEditorTabForPath(const QString &absolutePath, bool promptForDirty);
    [[nodiscard]] bool closeGitHubTabAt(int index);
    [[nodiscard]] bool closeRepoHelpTabAt(int index);
    [[nodiscard]] GitHubDetailView *githubDetailViewAt(int index) const;
    [[nodiscard]] RepoHelpView *repoHelpViewAt(int index) const;
    [[nodiscard]] bool isGitHubTabIndex(int index) const;
    [[nodiscard]] bool isRepoHelpTabIndex(int index) const;
    [[nodiscard]] QString repoHelpTabKey() const;
    [[nodiscard]] QString repoHelpEntryPathForActiveProject() const;
    [[nodiscard]] QString githubTabKeyForIssue(int number) const;
    [[nodiscard]] QString githubTabKeyForPullRequest(int number) const;
    [[nodiscard]] QString githubTabTitle(const QString &prefix, int number, const QString &title) const;
    void updateEditorTabTitle(EditorTab *editorTab);
    void reindexWorkspaceTabs();

    qtcode::core::StatusService *m_statusService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::core::AppConfigService *m_appConfigService = nullptr;
    QTabWidget *m_tabWidget = nullptr;
    QHash<QString, int> m_fileTabIndices;
    QHash<QString, int> m_githubTabIndices;
    QHash<QString, int> m_repoHelpTabIndices;
    QWidget *m_aiChatPanel = nullptr;
    int m_aiChatTabIndex = -1;
};

} // namespace qtcode::ui
