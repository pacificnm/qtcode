#pragma once

#include <QString>
#include <memory>

namespace qtcode::storage {
class StorageService;
} // namespace qtcode::storage

namespace qtcode::core {

class SettingsService;
class ProjectManager;
class CliCapabilityService;
class McpServerService;

} // namespace qtcode::core

namespace qtcode::git {
class GitService;
} // namespace qtcode::git

namespace qtcode::terminal {
class TerminalManager;
} // namespace qtcode::terminal

namespace qtcode::agents {
class AgentManager;
} // namespace qtcode::agents

namespace qtcode::core {

class ApplicationController
{
public:
    ApplicationController();
    ~ApplicationController();

    ApplicationController(const ApplicationController &) = delete;
    ApplicationController &operator=(const ApplicationController &) = delete;

    [[nodiscard]] bool initialize(QString *errorMessage = nullptr);
    void shutdown();

    [[nodiscard]] storage::StorageService *storageService() const;
    [[nodiscard]] SettingsService *settingsService() const;
    [[nodiscard]] ProjectManager *projectManager() const;
    [[nodiscard]] git::GitService *gitService() const;
    [[nodiscard]] terminal::TerminalManager *terminalManager() const;
    [[nodiscard]] CliCapabilityService *cliCapabilityService() const;
    [[nodiscard]] agents::AgentManager *agentManager() const;
    [[nodiscard]] McpServerService *mcpServerService() const;
    [[nodiscard]] bool runSmokeTestAgentPromptIfRequested(
        QString *errorMessage = nullptr,
        QString *sessionId = nullptr);
    [[nodiscard]] bool runSmokeTestDiffArtifactIfRequested(QString *errorMessage = nullptr);

private:
    std::unique_ptr<storage::StorageService> m_storageService;
    std::unique_ptr<SettingsService> m_settingsService;
    std::unique_ptr<git::GitService> m_gitService;
    std::unique_ptr<ProjectManager> m_projectManager;
    std::unique_ptr<terminal::TerminalManager> m_terminalManager;
    std::unique_ptr<CliCapabilityService> m_cliCapabilityService;
    std::unique_ptr<agents::AgentManager> m_agentManager;
    std::unique_ptr<McpServerService> m_mcpServerService;
};

} // namespace qtcode::core
