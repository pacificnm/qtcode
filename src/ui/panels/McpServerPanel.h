#pragma once

#include <QWidget>

#include "memory/McpHealthResult.h"

class QLabel;
class QListWidget;
class QGroupBox;
class QPushButton;
class QToolButton;

namespace qtcode::core {
class McpServerService;
class ProjectManager;
class WorkspaceInstaller;
} // namespace qtcode::core

namespace qtcode::memory {
class MemoryService;
} // namespace qtcode::memory

namespace qtcode::settings {
struct McpServerRecord;
} // namespace qtcode::settings

namespace qtcode::ui {

class McpServerPanel final : public QWidget
{
    Q_OBJECT

public:
    McpServerPanel(
        qtcode::core::McpServerService *mcpServerService,
        qtcode::memory::MemoryService *memoryService,
        qtcode::core::ProjectManager *projectManager,
        qtcode::core::WorkspaceInstaller *workspaceInstaller,
        QWidget *parent = nullptr);

private slots:
    void refreshServerList();
    void openAddServerDialog();
    void openSelectedServerDialog();
    void onServerHealthUpdated(const QString &serverId, const qtcode::memory::McpHealthResult &result);
    void refreshWorkspaceHealth();

private:
    void configureLayout();
    void openServerDialog(const QString &serverId);
    [[nodiscard]] QString serverListLabel(const qtcode::settings::McpServerRecord &server) const;
    void updateCountLabel(int serverCount);

    qtcode::core::McpServerService *m_mcpServerService = nullptr;
    qtcode::memory::MemoryService *m_memoryService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::core::WorkspaceInstaller *m_workspaceInstaller = nullptr;
    QLabel *m_countLabel = nullptr;
    QToolButton *m_addServerButton = nullptr;
    QListWidget *m_serverList = nullptr;
    QGroupBox *m_workspaceHealthGroup = nullptr;
    QLabel *m_workspaceHealthSummaryLabel = nullptr;
    QPushButton *m_refreshWorkspaceHealthButton = nullptr;
};

} // namespace qtcode::ui
