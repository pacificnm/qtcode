#include "ui/panels/McpServerPanel.h"

#include "ui/dialogs/McpServerDialog.h"

#include "memory/McpHealthResult.h"
#include "core/McpServerService.h"
#include "core/ProjectManager.h"
#include "core/WorkspaceInstaller.h"
#include "core/WorkspaceModels.h"
#include "memory/MemoryService.h"
#include "settings/McpServerModels.h"
#include "settings/ProjectModels.h"

#include <KLocalizedString>

#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace qtcode::ui {

McpServerPanel::McpServerPanel(
    qtcode::core::McpServerService *mcpServerService,
    qtcode::memory::MemoryService *memoryService,
    qtcode::core::ProjectManager *projectManager,
    qtcode::core::WorkspaceInstaller *workspaceInstaller,
    QWidget *parent)
    : QWidget(parent)
    , m_mcpServerService(mcpServerService)
    , m_memoryService(memoryService)
    , m_projectManager(projectManager)
    , m_workspaceInstaller(workspaceInstaller)
{
    configureLayout();

    if (m_projectManager != nullptr) {
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::activeProjectChanged,
            this,
            &McpServerPanel::refreshWorkspaceHealth);
    }

    if (m_workspaceInstaller != nullptr) {
        connect(
            m_workspaceInstaller,
            &qtcode::core::WorkspaceInstaller::workspaceInstalled,
            this,
            &McpServerPanel::refreshWorkspaceHealth);
    }

    if (m_mcpServerService != nullptr) {
        connect(
            m_mcpServerService,
            &qtcode::core::McpServerService::serversChanged,
            this,
            &McpServerPanel::refreshServerList);
        refreshServerList();
    }

    if (m_memoryService != nullptr) {
        connect(
            m_memoryService,
            &qtcode::memory::MemoryService::serverHealthUpdated,
            this,
            &McpServerPanel::onServerHealthUpdated);
    }
}

void McpServerPanel::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel(i18n("MCP Servers"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *countHeaderLayout = new QHBoxLayout();
    countHeaderLayout->setContentsMargins(0, 0, 0, 0);

    m_countLabel = new QLabel(this);
    m_countLabel->setWordWrap(true);

    m_addServerButton = new QToolButton(this);
    m_addServerButton->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_addServerButton->setToolTip(i18n("Add MCP server"));
    m_addServerButton->setAccessibleName(i18n("Add MCP server"));
    m_addServerButton->setAutoRaise(true);

    countHeaderLayout->addWidget(m_countLabel, 1);
    countHeaderLayout->addWidget(m_addServerButton);

    m_serverList = new QListWidget(this);
    m_serverList->setMinimumHeight(120);

    m_workspaceHealthGroup = new QGroupBox(i18n("Workspace health"), this);
    auto *workspaceHealthLayout = new QVBoxLayout(m_workspaceHealthGroup);
    workspaceHealthLayout->setContentsMargins(8, 8, 8, 8);
    workspaceHealthLayout->setSpacing(6);

    m_workspaceHealthSummaryLabel = new QLabel(m_workspaceHealthGroup);
    m_workspaceHealthSummaryLabel->setWordWrap(true);
    m_workspaceHealthSummaryLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_refreshWorkspaceHealthButton = new QPushButton(i18n("Re-check workspace"), m_workspaceHealthGroup);
    connect(
        m_refreshWorkspaceHealthButton,
        &QPushButton::clicked,
        this,
        &McpServerPanel::refreshWorkspaceHealth);

    workspaceHealthLayout->addWidget(m_workspaceHealthSummaryLabel);
    workspaceHealthLayout->addWidget(m_refreshWorkspaceHealthButton);

    layout->addWidget(titleLabel);
    layout->addLayout(countHeaderLayout);
    layout->addWidget(m_serverList, 1);
    layout->addWidget(m_workspaceHealthGroup);

    connect(m_addServerButton, &QToolButton::clicked, this, &McpServerPanel::openAddServerDialog);
    connect(m_serverList, &QListWidget::itemClicked, this, &McpServerPanel::openSelectedServerDialog);

    refreshWorkspaceHealth();
    updateCountLabel(0);
}

void McpServerPanel::refreshServerList()
{
    if (m_mcpServerService == nullptr || m_serverList == nullptr) {
        return;
    }

    const QSignalBlocker blocker(m_serverList);
    m_serverList->clear();

    const QList<qtcode::settings::McpServerRecord> servers = m_mcpServerService->servers();
    for (const qtcode::settings::McpServerRecord &server : servers) {
        auto *item = new QListWidgetItem(serverListLabel(server), m_serverList);
        item->setData(Qt::UserRole, server.id);
        if (!server.enabled) {
            item->setForeground(palette().color(QPalette::PlaceholderText));
        }
    }

    updateCountLabel(servers.size());
}

void McpServerPanel::updateCountLabel(int serverCount)
{
    if (m_countLabel == nullptr) {
        return;
    }

    m_countLabel->setText(
        serverCount == 0 ? i18n("No MCP servers configured yet.")
                         : i18n("%1 MCP server(s) configured.", serverCount));
}

void McpServerPanel::openAddServerDialog()
{
    openServerDialog({});
}

void McpServerPanel::openSelectedServerDialog()
{
    if (m_serverList == nullptr) {
        return;
    }

    const QListWidgetItem *item = m_serverList->currentItem();
    if (item == nullptr) {
        return;
    }

    openServerDialog(item->data(Qt::UserRole).toString());
}

void McpServerPanel::openServerDialog(const QString &serverId)
{
    McpServerDialog dialog(
        m_mcpServerService,
        m_memoryService,
        m_projectManager,
        serverId,
        this);
    dialog.exec();
}

void McpServerPanel::onServerHealthUpdated(
    const QString &serverId,
    const qtcode::memory::McpHealthResult &result)
{
    Q_UNUSED(result);

    if (m_mcpServerService == nullptr || m_serverList == nullptr) {
        return;
    }

    const QList<qtcode::settings::McpServerRecord> servers = m_mcpServerService->servers();
    for (int index = 0; index < servers.size(); ++index) {
        if (servers.at(index).id != serverId) {
            continue;
        }

        if (QListWidgetItem *item = m_serverList->item(index)) {
            item->setText(serverListLabel(servers.at(index)));
        }
        break;
    }
}

void McpServerPanel::refreshWorkspaceHealth()
{
    if (m_workspaceHealthSummaryLabel == nullptr || m_workspaceInstaller == nullptr) {
        return;
    }

    if (m_projectManager == nullptr || !m_projectManager->hasActiveProject()) {
        m_workspaceHealthSummaryLabel->setText(i18n("Open a repository to inspect workspace health."));
        return;
    }

    qtcode::settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        m_workspaceHealthSummaryLabel->setText(i18n("The active project could not be loaded."));
        return;
    }

    const QList<qtcode::core::WorkspaceHealthCheck> checks =
        m_workspaceInstaller->evaluateHealth(activeProject.rootPath);
    QStringList lines;
    for (const qtcode::core::WorkspaceHealthCheck &check : checks) {
        lines.append(
            i18n("%1 (%2): %3",
                 check.label,
                 qtcode::core::workspaceHealthStateLabel(check.state),
                 check.message));
        if (!check.fixHint.isEmpty()
            && check.state != qtcode::core::WorkspaceHealthState::Ok) {
            lines.append(i18n("Fix: %1", check.fixHint));
        }
    }

    m_workspaceHealthSummaryLabel->setText(lines.join(QStringLiteral("\n\n")));
}

QString McpServerPanel::serverListLabel(const qtcode::settings::McpServerRecord &server) const
{
    QString label = server.name;
    if (m_memoryService == nullptr) {
        return label;
    }

    const qtcode::memory::McpHealthResult health = m_memoryService->healthForServer(server.id);
    switch (health.state) {
    case qtcode::memory::McpHealthState::Healthy:
        label.append(QStringLiteral(" \u2713"));
        break;
    case qtcode::memory::McpHealthState::Unhealthy:
        label.append(QStringLiteral(" \u2717"));
        break;
    case qtcode::memory::McpHealthState::Checking:
        label.append(QStringLiteral(" ..."));
        break;
    case qtcode::memory::McpHealthState::Unknown:
        break;
    }

    return label;
}

} // namespace qtcode::ui
