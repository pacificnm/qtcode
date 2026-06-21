#include "ui/panels/WorkspaceTabs.h"

#include "core/AppConfigService.h"
#include "core/ProjectManager.h"
#include "core/RepoConfigLoader.h"
#include "core/StatusModels.h"
#include "core/StatusService.h"
#include "shared/Logging.h"
#include "ui/panels/EditorTab.h"
#include "ui/views/GitHubDetailView.h"
#include "ui/views/RepoHelpView.h"
#include "settings/AppConfig.h"
#include "settings/ProjectModels.h"
#include "settings/RepoConfig.h"

#include <KLocalizedString>

#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QMenu>
#include <QDir>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace qtcode::ui {

namespace {

constexpr int kBinarySampleSize = 8192;

} // namespace

WorkspaceTabs::WorkspaceTabs(
    qtcode::core::StatusService *statusService,
    qtcode::core::ProjectManager *projectManager,
    qtcode::core::AppConfigService *appConfigService,
    QWidget *parent)
    : QWidget(parent)
    , m_statusService(statusService)
    , m_projectManager(projectManager)
    , m_appConfigService(appConfigService)
{
    configureLayout();

    if (m_projectManager != nullptr) {
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::activeProjectChanged,
            this,
            &WorkspaceTabs::onActiveProjectChanged);
    }
}

void WorkspaceTabs::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setTabsClosable(true);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &WorkspaceTabs::onTabCloseRequested);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &WorkspaceTabs::onCurrentTabChanged);
    m_tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        m_tabWidget->tabBar(),
        &QTabBar::customContextMenuRequested,
        this,
        &WorkspaceTabs::showTabContextMenu);

    layout->addWidget(m_tabWidget, 1);
}

void WorkspaceTabs::setPermanentAiChatTab(QWidget *conversationPanel)
{
    if (m_tabWidget == nullptr || conversationPanel == nullptr) {
        return;
    }

    m_aiChatPanel = conversationPanel;

    if (m_aiChatTabIndex >= 0) {
        QWidget *existingWidget = m_tabWidget->widget(m_aiChatTabIndex);
        if (existingWidget == conversationPanel) {
            refreshPermanentTabCloseButton();
            return;
        }

        m_tabWidget->removeTab(m_aiChatTabIndex);
        m_aiChatTabIndex = -1;
    }

    m_aiChatTabIndex = m_tabWidget->addTab(conversationPanel, i18n("AI Chat"));
    m_tabWidget->setCurrentIndex(m_aiChatTabIndex);
    refreshPermanentTabCloseButton();
}

int WorkspaceTabs::aiChatTabIndex() const
{
    return m_aiChatTabIndex;
}

int WorkspaceTabs::tabCount() const
{
    return m_tabWidget != nullptr ? m_tabWidget->count() : 0;
}

void WorkspaceTabs::requestOpenFile(const QString &absolutePath)
{
    if (m_tabWidget == nullptr || absolutePath.isEmpty()) {
        return;
    }

    const QString pathKey = normalizedPath(absolutePath);
    if (pathKey.isEmpty()) {
        return;
    }

    const auto existingIndex = m_fileTabIndices.constFind(pathKey);
    if (existingIndex != m_fileTabIndices.constEnd() && *existingIndex >= 0
        && *existingIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(*existingIndex);
        qCInfo(qtcodeUi) << "Focused existing workspace tab for" << pathKey;
        return;
    }

    const QFileInfo fileInfo(pathKey);
    if (!fileInfo.isFile() || !fileInfo.isReadable()) {
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                i18n("Cannot open %1 in the editor.", fileInfo.fileName()),
                qtcode::core::StatusSeverity::Warning);
        }
        return;
    }

    if (looksBinaryFile(pathKey)) {
        const QString message = i18n("Cannot open %1 in the editor.", fileInfo.fileName());
        if (m_statusService != nullptr) {
            m_statusService->showMessage(message, qtcode::core::StatusSeverity::Warning);
        }
        QMessageBox::information(
            this,
            i18n("Unsupported File"),
            i18n("%1 does not look like a text file.", fileInfo.fileName()));
        return;
    }

    auto *editorTab = new EditorTab(pathKey, m_statusService, m_tabWidget);
    if (!editorTab->isLoadSuccessful()) {
        const QString message = editorTab->loadErrorMessage().isEmpty()
            ? i18n("Could not open %1 in the editor.", fileInfo.fileName())
            : editorTab->loadErrorMessage();
        if (m_statusService != nullptr) {
            m_statusService->showMessage(message, qtcode::core::StatusSeverity::Error);
        }
        QMessageBox::warning(this, i18n("Open Failed"), message);
        editorTab->deleteLater();
        return;
    }

    connect(editorTab, &EditorTab::modificationChanged, this, [this, editorTab](bool) {
        updateEditorTabTitle(editorTab);
    });

    const int tabIndex = m_tabWidget->addTab(editorTab, editorTab->displayName());
    m_fileTabIndices.insert(pathKey, tabIndex);
    updateEditorTabTitle(editorTab);
    m_tabWidget->setCurrentIndex(tabIndex);
    emit activeEditorStateChanged(hasActiveEditorTab(), currentEditorTabIsModified());
    qCInfo(qtcodeUi) << "Opened editor tab for" << pathKey;
}

void WorkspaceTabs::requestOpenIssue(
    const qtcode::github::GitHubIssueDetail &detail,
    const qtcode::github::GitHubCacheMetadata &cacheMetadata)
{
    if (m_tabWidget == nullptr || detail.number <= 0) {
        return;
    }

    const QString tabKey = githubTabKeyForIssue(detail.number);
    const auto existingIndex = m_githubTabIndices.constFind(tabKey);
    if (existingIndex != m_githubTabIndices.constEnd() && *existingIndex >= 0
        && *existingIndex < m_tabWidget->count()) {
        GitHubDetailView *detailView = githubDetailViewAt(*existingIndex);
        if (detailView != nullptr) {
            detailView->showIssue(detail, cacheMetadata);
        }
        m_tabWidget->setCurrentIndex(*existingIndex);
        qCInfo(qtcodeUi) << "Focused existing issue tab for" << detail.number;
        return;
    }

    auto *detailView = new GitHubDetailView(m_tabWidget);
    detailView->setProperty("githubTabKey", tabKey);
    detailView->showIssue(detail, cacheMetadata);
    connect(
        detailView,
        &GitHubDetailView::attachIssueRequested,
        this,
        &WorkspaceTabs::issueContextSelected);
    connect(
        detailView,
        &GitHubDetailView::attachPullRequestRequested,
        this,
        &WorkspaceTabs::pullRequestContextSelected);

    const int tabIndex =
        m_tabWidget->addTab(detailView, githubTabTitle(i18n("Issue"), detail.number, detail.title));
    m_githubTabIndices.insert(tabKey, tabIndex);
    m_tabWidget->setCurrentIndex(tabIndex);
    qCInfo(qtcodeUi) << "Opened issue tab for" << detail.number;
}

void WorkspaceTabs::requestOpenRepoHelp()
{
    if (m_tabWidget == nullptr) {
        return;
    }

    const QString entryPath = repoHelpEntryPathForActiveProject();
    if (entryPath.isEmpty()) {
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                i18n("Open a project before viewing repository help."),
                qtcode::core::StatusSeverity::Warning);
        }
        return;
    }

    const QString tabKey = repoHelpTabKey();
    const auto existingIndex = m_repoHelpTabIndices.constFind(tabKey);
    if (existingIndex != m_repoHelpTabIndices.constEnd() && *existingIndex >= 0
        && *existingIndex < m_tabWidget->count()) {
        RepoHelpView *helpView = repoHelpViewAt(*existingIndex);
        if (helpView != nullptr) {
            helpView->loadHelpEntry(entryPath);
        }
        m_tabWidget->setCurrentIndex(*existingIndex);
        qCInfo(qtcodeUi) << "Focused existing repository help tab";
        return;
    }

    auto *helpView = new RepoHelpView(m_statusService, m_tabWidget);
    helpView->setProperty("repoHelpTabKey", tabKey);
    helpView->loadHelpEntry(entryPath);

    const int tabIndex = m_tabWidget->addTab(helpView, i18n("Repo Help"));
    m_repoHelpTabIndices.insert(tabKey, tabIndex);
    m_tabWidget->setCurrentIndex(tabIndex);
    qCInfo(qtcodeUi) << "Opened repository help tab";
}

void WorkspaceTabs::requestOpenPullRequest(
    const qtcode::github::GitHubPullRequestDetail &detail,
    const qtcode::github::GitHubCacheMetadata &cacheMetadata)
{
    if (m_tabWidget == nullptr || detail.number <= 0) {
        return;
    }

    const QString tabKey = githubTabKeyForPullRequest(detail.number);
    const auto existingIndex = m_githubTabIndices.constFind(tabKey);
    if (existingIndex != m_githubTabIndices.constEnd() && *existingIndex >= 0
        && *existingIndex < m_tabWidget->count()) {
        GitHubDetailView *detailView = githubDetailViewAt(*existingIndex);
        if (detailView != nullptr) {
            detailView->showPullRequest(detail, cacheMetadata);
        }
        m_tabWidget->setCurrentIndex(*existingIndex);
        qCInfo(qtcodeUi) << "Focused existing pull request tab for" << detail.number;
        return;
    }

    auto *detailView = new GitHubDetailView(m_tabWidget);
    detailView->setProperty("githubTabKey", tabKey);
    detailView->showPullRequest(detail, cacheMetadata);
    connect(
        detailView,
        &GitHubDetailView::attachIssueRequested,
        this,
        &WorkspaceTabs::issueContextSelected);
    connect(
        detailView,
        &GitHubDetailView::attachPullRequestRequested,
        this,
        &WorkspaceTabs::pullRequestContextSelected);

    const int tabIndex = m_tabWidget->addTab(
        detailView,
        githubTabTitle(i18n("PR"), detail.number, detail.title));
    m_githubTabIndices.insert(tabKey, tabIndex);
    m_tabWidget->setCurrentIndex(tabIndex);
    qCInfo(qtcodeUi) << "Opened pull request tab for" << detail.number;
}

QString WorkspaceTabs::normalizedPath(const QString &absolutePath) const
{
    return QFileInfo(absolutePath).absoluteFilePath();
}

void WorkspaceTabs::onTabCloseRequested(int index)
{
    if (isEditorTabIndex(index)) {
        (void) closeEditorTabAt(index, true);
        return;
    }

    if (isGitHubTabIndex(index)) {
        (void) closeGitHubTabAt(index);
        return;
    }

    if (isRepoHelpTabIndex(index)) {
        (void) closeRepoHelpTabAt(index);
    }
}

void WorkspaceTabs::closeCurrentEditorTab()
{
    if (m_tabWidget == nullptr) {
        return;
    }

    (void) closeEditorTabAt(m_tabWidget->currentIndex(), true);
}

bool WorkspaceTabs::saveCurrentEditorTab()
{
    EditorTab *editorTab = currentEditorTab();
    if (editorTab == nullptr) {
        return false;
    }

    QString saveError;
    if (!editorTab->save(&saveError)) {
        if (!saveError.isEmpty()) {
            QMessageBox::warning(this, i18n("Save Failed"), saveError);
        }
        return false;
    }

    updateEditorTabTitle(editorTab);
    return true;
}

bool WorkspaceTabs::hasActiveEditorTab() const
{
    return currentEditorTab() != nullptr;
}

bool WorkspaceTabs::currentEditorTabIsModified() const
{
    EditorTab *editorTab = currentEditorTab();
    return editorTab != nullptr && editorTab->isModified();
}

bool WorkspaceTabs::closeAllEditorTabs(bool promptForDirty)
{
    if (m_tabWidget == nullptr) {
        return true;
    }

    for (int index = m_tabWidget->count() - 1; index >= 0; --index) {
        if (index == m_aiChatTabIndex) {
            continue;
        }

        if (isEditorTabIndex(index)) {
            if (!closeEditorTabAt(index, promptForDirty)) {
                return false;
            }
            continue;
        }

        if (isGitHubTabIndex(index)) {
            if (!closeGitHubTabAt(index)) {
                return false;
            }
            continue;
        }

        if (isRepoHelpTabIndex(index)) {
            if (!closeRepoHelpTabAt(index)) {
                return false;
            }
        }
    }

    return true;
}

bool WorkspaceTabs::closeEditorTabAt(int index, bool promptForDirty)
{
    if (m_tabWidget == nullptr || index < 0 || index >= m_tabWidget->count()
        || index == m_aiChatTabIndex) {
        return true;
    }

    EditorTab *editorTab = editorTabAt(index);
    if (editorTab == nullptr) {
        return true;
    }

    if (promptForDirty) {
        const EditorCloseChoice choice = editorTab->promptClose(this);
        if (choice == EditorCloseChoice::Cancel) {
            return false;
        }
    }

    const QString pathKey = normalizedPath(editorTab->filePath());
    m_fileTabIndices.remove(pathKey);

    QWidget *widget = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);

    if (m_aiChatTabIndex > index) {
        m_aiChatTabIndex -= 1;
    }

    reindexWorkspaceTabs();

    if (widget != nullptr) {
        delete widget;
    }

    return true;
}

bool WorkspaceTabs::closeRepoHelpTabAt(int index)
{
    if (m_tabWidget == nullptr || index < 0 || index >= m_tabWidget->count()
        || index == m_aiChatTabIndex) {
        return true;
    }

    RepoHelpView *helpView = repoHelpViewAt(index);
    if (helpView == nullptr) {
        return true;
    }

    const QString tabKey = m_repoHelpTabIndices.key(index);
    if (!tabKey.isEmpty()) {
        m_repoHelpTabIndices.remove(tabKey);
    }

    QWidget *widget = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);

    if (m_aiChatTabIndex > index) {
        m_aiChatTabIndex -= 1;
    }

    reindexWorkspaceTabs();

    if (widget != nullptr) {
        delete widget;
    }

    return true;
}

bool WorkspaceTabs::closeGitHubTabAt(int index)
{
    if (m_tabWidget == nullptr || index < 0 || index >= m_tabWidget->count()
        || index == m_aiChatTabIndex) {
        return true;
    }

    GitHubDetailView *detailView = githubDetailViewAt(index);
    if (detailView == nullptr) {
        return true;
    }

    const QString tabKey = m_githubTabIndices.key(index);
    if (!tabKey.isEmpty()) {
        m_githubTabIndices.remove(tabKey);
    }

    QWidget *widget = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);

    if (m_aiChatTabIndex > index) {
        m_aiChatTabIndex -= 1;
    }

    reindexWorkspaceTabs();

    if (widget != nullptr) {
        delete widget;
    }

    return true;
}

bool WorkspaceTabs::closeEditorTabForPath(const QString &absolutePath, bool promptForDirty)
{
    const QString pathKey = normalizedPath(absolutePath);
    const auto tabIndex = m_fileTabIndices.constFind(pathKey);
    if (tabIndex == m_fileTabIndices.constEnd()) {
        return true;
    }

    return closeEditorTabAt(*tabIndex, promptForDirty);
}

void WorkspaceTabs::handleFileRenamed(const QString &oldPath, const QString &newPath)
{
    const QString oldKey = normalizedPath(oldPath);
    const QString newKey = normalizedPath(newPath);

    if (m_fileTabIndices.contains(oldKey)) {
        EditorTab *editorTab = editorTabAt(m_fileTabIndices.value(oldKey));
        if (editorTab != nullptr) {
            editorTab->repathTo(newKey);
            m_fileTabIndices.remove(oldKey);
            m_fileTabIndices.insert(newKey, m_tabWidget->indexOf(editorTab));
            updateEditorTabTitle(editorTab);
        }
    }

    const QString oldPrefix = oldKey + QDir::separator();
    const QString newPrefix = newKey + QDir::separator();
    QStringList nestedPaths;
    for (auto it = m_fileTabIndices.constBegin(); it != m_fileTabIndices.constEnd(); ++it) {
        if (it.key().startsWith(oldPrefix)) {
            nestedPaths.append(it.key());
        }
    }

    for (const QString &nestedOldPath : nestedPaths) {
        const QString nestedNewPath = newPrefix + nestedOldPath.mid(oldPrefix.length());
        EditorTab *editorTab = editorTabAt(m_fileTabIndices.value(nestedOldPath));
        if (editorTab == nullptr) {
            continue;
        }

        editorTab->repathTo(nestedNewPath);
        const int tabIndex = m_tabWidget->indexOf(editorTab);
        m_fileTabIndices.remove(nestedOldPath);
        m_fileTabIndices.insert(nestedNewPath, tabIndex);
        updateEditorTabTitle(editorTab);
    }
}

void WorkspaceTabs::handleFileDeleted(const QString &path)
{
    (void) closeEditorTabForPath(path, true);
}

void WorkspaceTabs::handleDirectoryDeleted(const QString &directoryPath)
{
    const QString dirKey = normalizedPath(directoryPath);
    const QString dirPrefix = dirKey + QDir::separator();
    QStringList pathsToClose;
    for (auto it = m_fileTabIndices.constBegin(); it != m_fileTabIndices.constEnd(); ++it) {
        if (it.key() == dirKey || it.key().startsWith(dirPrefix)) {
            pathsToClose.append(it.key());
        }
    }

    for (const QString &path : pathsToClose) {
        if (!closeEditorTabForPath(path, true)) {
            return;
        }
    }
}

void WorkspaceTabs::handlePathDeleted(const QString &path, bool isDirectory)
{
    if (isDirectory) {
        handleDirectoryDeleted(path);
    } else {
        handleFileDeleted(path);
    }
}

void WorkspaceTabs::onActiveProjectChanged()
{
    if (!closeAllEditorTabs(true)) {
        return;
    }

    if (m_tabWidget == nullptr) {
        return;
    }

    for (int index = m_tabWidget->count() - 1; index >= 0; --index) {
        if (index == m_aiChatTabIndex || !isRepoHelpTabIndex(index)) {
            continue;
        }

        if (!closeRepoHelpTabAt(index)) {
            return;
        }
    }
}

void WorkspaceTabs::onCurrentTabChanged(int index)
{
    Q_UNUSED(index)
    emit activeEditorStateChanged(hasActiveEditorTab(), currentEditorTabIsModified());
}

EditorTab *WorkspaceTabs::editorTabAt(int index) const
{
    if (m_tabWidget == nullptr || index < 0 || index >= m_tabWidget->count()) {
        return nullptr;
    }

    return qobject_cast<EditorTab *>(m_tabWidget->widget(index));
}

EditorTab *WorkspaceTabs::currentEditorTab() const
{
    if (m_tabWidget == nullptr) {
        return nullptr;
    }

    return editorTabAt(m_tabWidget->currentIndex());
}

bool WorkspaceTabs::isEditorTabIndex(int index) const
{
    return editorTabAt(index) != nullptr;
}

bool WorkspaceTabs::looksBinaryFile(const QString &absolutePath) const
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    return file.read(kBinarySampleSize).contains('\0');
}

void WorkspaceTabs::updateEditorTabTitle(EditorTab *editorTab)
{
    if (m_tabWidget == nullptr || editorTab == nullptr) {
        return;
    }

    const int index = m_tabWidget->indexOf(editorTab);
    if (index < 0) {
        return;
    }

    QString title = editorTab->displayName();
    if (editorTab->isModified()) {
        title = QStringLiteral("* %1").arg(title);
    }

    m_tabWidget->setTabText(index, title);
    emit activeEditorStateChanged(hasActiveEditorTab(), currentEditorTabIsModified());
}

void WorkspaceTabs::reindexWorkspaceTabs()
{
    m_fileTabIndices.clear();
    m_githubTabIndices.clear();
    m_repoHelpTabIndices.clear();
    if (m_tabWidget == nullptr) {
        m_aiChatTabIndex = -1;
        return;
    }

    for (int index = 0; index < m_tabWidget->count(); ++index) {
        if (m_aiChatPanel != nullptr && m_tabWidget->widget(index) == m_aiChatPanel) {
            m_aiChatTabIndex = index;
            continue;
        }

        EditorTab *editorTab = editorTabAt(index);
        if (editorTab != nullptr) {
            m_fileTabIndices.insert(normalizedPath(editorTab->filePath()), index);
            continue;
        }

        GitHubDetailView *detailView = githubDetailViewAt(index);
        if (detailView == nullptr) {
            continue;
        }

        const QString tabKey = detailView->property("githubTabKey").toString();
        if (!tabKey.isEmpty()) {
            m_githubTabIndices.insert(tabKey, index);
            continue;
        }

        RepoHelpView *helpView = repoHelpViewAt(index);
        if (helpView == nullptr) {
            continue;
        }

        const QString helpTabKey = helpView->property("repoHelpTabKey").toString();
        if (!helpTabKey.isEmpty()) {
            m_repoHelpTabIndices.insert(helpTabKey, index);
        }
    }
}

void WorkspaceTabs::refreshPermanentTabCloseButton()
{
    if (m_tabWidget == nullptr || m_aiChatTabIndex < 0) {
        return;
    }

    m_tabWidget->tabBar()->setTabButton(m_aiChatTabIndex, QTabBar::RightSide, nullptr);
}

GitHubDetailView *WorkspaceTabs::githubDetailViewAt(int index) const
{
    if (m_tabWidget == nullptr || index < 0 || index >= m_tabWidget->count()) {
        return nullptr;
    }

    return qobject_cast<GitHubDetailView *>(m_tabWidget->widget(index));
}

bool WorkspaceTabs::isGitHubTabIndex(int index) const
{
    return githubDetailViewAt(index) != nullptr;
}

RepoHelpView *WorkspaceTabs::repoHelpViewAt(int index) const
{
    if (m_tabWidget == nullptr || index < 0 || index >= m_tabWidget->count()) {
        return nullptr;
    }

    return qobject_cast<RepoHelpView *>(m_tabWidget->widget(index));
}

bool WorkspaceTabs::isRepoHelpTabIndex(int index) const
{
    return repoHelpViewAt(index) != nullptr;
}

QString WorkspaceTabs::repoHelpTabKey() const
{
    return QStringLiteral("repo-help");
}

QString WorkspaceTabs::repoHelpEntryPathForActiveProject() const
{
    if (m_projectManager == nullptr) {
        return {};
    }

    qtcode::settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject) || activeProject.rootPath.isEmpty()) {
        return {};
    }

    qtcode::settings::AppConfig appConfig = qtcode::settings::AppConfig::defaults();
    if (m_appConfigService != nullptr) {
        appConfig = m_appConfigService->config();
    }

    const qtcode::settings::RepoConfig repoConfig =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(activeProject.rootPath);
    const QString repoHelpPath = qtcode::settings::effectiveRepoHelpPath(appConfig, repoConfig);

    return QDir(activeProject.rootPath).absoluteFilePath(repoHelpPath);
}

QString WorkspaceTabs::githubTabKeyForIssue(int number) const
{
    return QStringLiteral("issue:%1").arg(number);
}

QString WorkspaceTabs::githubTabKeyForPullRequest(int number) const
{
    return QStringLiteral("pr:%1").arg(number);
}

QString WorkspaceTabs::githubTabTitle(const QString &prefix, int number, const QString &title) const
{
    QString elidedTitle = title;
    if (elidedTitle.length() > 48) {
        elidedTitle = elidedTitle.left(45).trimmed() + QStringLiteral("…");
    }

    return i18n("%1 #%2 — %3", prefix, number, elidedTitle);
}

void WorkspaceTabs::showTabContextMenu(const QPoint &position)
{
    if (m_tabWidget == nullptr) {
        return;
    }

    const int tabIndex = m_tabWidget->tabBar()->tabAt(position);
    if (tabIndex < 0 || tabIndex == m_aiChatTabIndex) {
        return;
    }

    EditorTab *editorTab = editorTabAt(tabIndex);
    if (editorTab == nullptr) {
        return;
    }

    QMenu menu(this);
    menu.addAction(
        QIcon::fromTheme(QStringLiteral("bookmark-new")),
        i18n("Add to Context"),
        this,
        [this, editorTab]() {
            const QString contentOverride = editorTab->isModified() ? editorTab->documentText() : QString();
            emit fileContextSelected(editorTab->filePath(), contentOverride);
        });
    menu.exec(m_tabWidget->tabBar()->mapToGlobal(position));
}

} // namespace qtcode::ui
