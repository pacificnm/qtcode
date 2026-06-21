#include "ui/panels/WorkspaceTabs.h"

#include "core/ProjectManager.h"
#include "core/StatusModels.h"
#include "core/StatusService.h"
#include "shared/Logging.h"
#include "ui/panels/EditorTab.h"

#include <KLocalizedString>

#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
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
    QWidget *parent)
    : QWidget(parent)
    , m_statusService(statusService)
    , m_projectManager(projectManager)
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

QString WorkspaceTabs::normalizedPath(const QString &absolutePath) const
{
    return QFileInfo(absolutePath).absoluteFilePath();
}

void WorkspaceTabs::onTabCloseRequested(int index)
{
    (void) closeEditorTabAt(index, true);
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
        if (!isEditorTabIndex(index)) {
            continue;
        }

        if (!closeEditorTabAt(index, promptForDirty)) {
            return false;
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

    reindexFileTabs();

    if (widget != nullptr) {
        widget->deleteLater();
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

void WorkspaceTabs::reindexFileTabs()
{
    m_fileTabIndices.clear();
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
        if (editorTab == nullptr) {
            continue;
        }

        m_fileTabIndices.insert(normalizedPath(editorTab->filePath()), index);
    }
}

void WorkspaceTabs::refreshPermanentTabCloseButton()
{
    if (m_tabWidget == nullptr || m_aiChatTabIndex < 0) {
        return;
    }

    m_tabWidget->tabBar()->setTabButton(m_aiChatTabIndex, QTabBar::RightSide, nullptr);
}

} // namespace qtcode::ui
