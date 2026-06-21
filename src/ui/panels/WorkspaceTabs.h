#pragma once

#include <QHash>
#include <QWidget>

class QTabWidget;

namespace qtcode::core {
class ProjectManager;
class StatusService;
} // namespace qtcode::core

namespace qtcode::ui {

class EditorTab;

class WorkspaceTabs final : public QWidget
{
    Q_OBJECT

public:
    explicit WorkspaceTabs(
        qtcode::core::StatusService *statusService,
        qtcode::core::ProjectManager *projectManager,
        QWidget *parent = nullptr);

    void setPermanentAiChatTab(QWidget *conversationPanel);

    [[nodiscard]] int aiChatTabIndex() const;
    [[nodiscard]] bool closeAllEditorTabs(bool promptForDirty);
    [[nodiscard]] bool saveCurrentEditorTab();
    [[nodiscard]] bool hasActiveEditorTab() const;
    [[nodiscard]] bool currentEditorTabIsModified() const;

signals:
    void activeEditorStateChanged(bool hasActiveEditor, bool isModified);

public slots:
    void requestOpenFile(const QString &absolutePath);
    void closeCurrentEditorTab();

private slots:
    void onTabCloseRequested(int index);
    void onActiveProjectChanged();
    void onCurrentTabChanged(int index);

private:
    void configureLayout();
    void refreshPermanentTabCloseButton();
    [[nodiscard]] QString normalizedPath(const QString &absolutePath) const;
    [[nodiscard]] EditorTab *editorTabAt(int index) const;
    [[nodiscard]] EditorTab *currentEditorTab() const;
    [[nodiscard]] bool isEditorTabIndex(int index) const;
    [[nodiscard]] bool looksBinaryFile(const QString &absolutePath) const;
    [[nodiscard]] bool closeEditorTabAt(int index, bool promptForDirty);
    void updateEditorTabTitle(EditorTab *editorTab);
    void reindexFileTabs();

    qtcode::core::StatusService *m_statusService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    QTabWidget *m_tabWidget = nullptr;
    QHash<QString, int> m_fileTabIndices;
    QWidget *m_aiChatPanel = nullptr;
    int m_aiChatTabIndex = -1;
};

} // namespace qtcode::ui
