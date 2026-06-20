#include "core/ApplicationController.h"

#include "core/CliCapabilityService.h"
#include "core/McpServerService.h"
#include "core/ProjectManager.h"
#include "core/SettingsService.h"
#include "git/GitService.h"
#include "memory/MemoryService.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "storage/MigrationRunner.h"
#include "storage/StorageService.h"
#include "terminal/TerminalManager.h"
#include "agents/AgentManager.h"
#include "agents/adapters/CodexAgentAdapter.h"
#include "agents/AgentModels.h"
#include "agents/AgentSession.h"
#include "terminal/TerminalProfile.h"

#include <QUuid>

#include <QTimer>

#include <algorithm>

#include <QByteArray>

namespace qtcode::core {

ApplicationController::ApplicationController() = default;

ApplicationController::~ApplicationController()
{
    shutdown();
}

bool ApplicationController::initialize(QString *errorMessage)
{
    if (m_storageService != nullptr && m_storageService->isOpen()) {
        return true;
    }

    m_storageService = std::make_unique<storage::StorageService>();
    if (!m_storageService->open(errorMessage)) {
        qCWarning(qtcodeCore) << "StorageService failed to open";
        m_storageService.reset();
        return false;
    }

    storage::MigrationRunner migrationRunner(*m_storageService);
    if (!migrationRunner.runPendingMigrations(errorMessage)) {
        qCWarning(qtcodeCore) << "Database migrations failed";
        m_storageService->close();
        m_storageService.reset();
        return false;
    }

    m_settingsService = std::make_unique<SettingsService>(*m_storageService);
    m_gitService = std::make_unique<git::GitService>();
    m_projectManager = std::make_unique<ProjectManager>(*m_storageService, *m_gitService);
    m_terminalManager = std::make_unique<terminal::TerminalManager>(*m_storageService);
    m_cliCapabilityService = std::make_unique<CliCapabilityService>();

    if (!m_cliCapabilityService->detectCapabilities()) {
        qCWarning(qtcodeCore) << "CLI capability detection failed";
    }

    m_agentManager = std::make_unique<agents::AgentManager>(*m_storageService);
    if (!m_agentManager->registerBuiltInAdapters(errorMessage)) {
        qCWarning(qtcodeCore) << "Failed to register built-in agent adapters:"
                              << (errorMessage != nullptr ? *errorMessage : QString());
        return false;
    }

    if (!m_agentManager->restoreState(errorMessage)) {
        qCWarning(qtcodeCore) << "Failed to restore agent sessions:"
                              << (errorMessage != nullptr ? *errorMessage : QString());
        return false;
    }

    m_mcpServerService = std::make_unique<McpServerService>(*m_storageService);
    if (!m_mcpServerService->ensureDefaultConfiguration(errorMessage)) {
        qCWarning(qtcodeCore) << "Failed to initialize MCP server configuration:"
                              << (errorMessage != nullptr ? *errorMessage : QString());
        return false;
    }

    m_memoryService = std::make_unique<memory::MemoryService>();

    if (agents::AgentAdapter *registeredAdapter = m_agentManager->adapter(QStringLiteral("codex"));
        registeredAdapter != nullptr) {
        if (auto *codexAdapter = qobject_cast<agents::CodexAgentAdapter *>(registeredAdapter)) {
            const CliToolCapability &agentCli = m_cliCapabilityService->snapshot().agentCli;
            if (agentCli.toolId == QStringLiteral("codex") && !agentCli.executablePath.isEmpty()) {
                codexAdapter->setExecutablePath(agentCli.executablePath);
            }
        }
    }

    if (!m_projectManager->restoreState(errorMessage)) {
        qCWarning(qtcodeCore) << "Failed to restore project state:"
                              << (errorMessage != nullptr ? *errorMessage : QString());
        return false;
    }

    if (!m_terminalManager->restoreState(errorMessage)) {
        qCWarning(qtcodeCore) << "Failed to restore terminal state:"
                              << (errorMessage != nullptr ? *errorMessage : QString());
        return false;
    }

    if (const QByteArray addRepositoryPath = qgetenv("QTCODE_ADD_REPOSITORY"); !addRepositoryPath.isEmpty()) {
        settings::ProjectRecord project;
        if (!m_projectManager->addLocalRepository(QString::fromUtf8(addRepositoryPath), &project, errorMessage)) {
            qCWarning(qtcodeCore) << "Failed to add repository from QTCODE_ADD_REPOSITORY:"
                                  << (errorMessage != nullptr ? *errorMessage : QString());
            return false;
        }
    }

    if (m_terminalManager != nullptr) {
        if (const QByteArray shellPath = qgetenv("QTCODE_TERMINAL_SHELL"); !shellPath.isEmpty()) {
            terminal::TerminalProfile profile = m_terminalManager->globalProfile();
            profile.shellPath = QString::fromUtf8(shellPath);
            if (!m_terminalManager->setGlobalProfile(profile, errorMessage)) {
                qCWarning(qtcodeCore) << "Failed to apply QTCODE_TERMINAL_SHELL:"
                                      << (errorMessage != nullptr ? *errorMessage : QString());
                return false;
            }
        }

        if (const QByteArray workingDirectoryMode = qgetenv("QTCODE_TERMINAL_WD_MODE");
            !workingDirectoryMode.isEmpty()) {
            terminal::TerminalProfile profile = m_terminalManager->globalProfile();
            bool modeOk = false;
            profile.workingDirectoryMode = terminal::workingDirectoryModeFromString(
                QString::fromUtf8(workingDirectoryMode),
                &modeOk);
            if (!modeOk) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Invalid QTCODE_TERMINAL_WD_MODE value.");
                }
                qCWarning(qtcodeCore) << "Invalid QTCODE_TERMINAL_WD_MODE:" << workingDirectoryMode;
                return false;
            }

            if (const QByteArray customPath = qgetenv("QTCODE_TERMINAL_WD_PATH"); !customPath.isEmpty()) {
                profile.customWorkingDirectory = QString::fromUtf8(customPath);
            }

            if (!m_terminalManager->setGlobalProfile(profile, errorMessage)) {
                qCWarning(qtcodeCore) << "Failed to apply QTCODE_TERMINAL_WD_MODE:"
                                      << (errorMessage != nullptr ? *errorMessage : QString());
                return false;
            }
        }

        if (m_projectManager != nullptr && m_projectManager->hasActiveProject()) {
            const QString activeProjectId = m_projectManager->activeProjectId();
            if (const QByteArray projectWorkingDirectoryMode = qgetenv("QTCODE_PROJECT_TERMINAL_WD_MODE");
                !projectWorkingDirectoryMode.isEmpty()) {
                terminal::TerminalProfile profile;
                bool found = false;
                if (!m_terminalManager->projectProfile(activeProjectId, &profile, &found, errorMessage)) {
                    qCWarning(qtcodeCore) << "Failed to load project terminal profile:"
                                          << (errorMessage != nullptr ? *errorMessage : QString());
                    return false;
                }

                bool modeOk = false;
                profile.workingDirectoryMode = terminal::workingDirectoryModeFromString(
                    QString::fromUtf8(projectWorkingDirectoryMode),
                    &modeOk);
                if (!modeOk) {
                    if (errorMessage != nullptr) {
                        *errorMessage = QStringLiteral("Invalid QTCODE_PROJECT_TERMINAL_WD_MODE value.");
                    }
                    qCWarning(qtcodeCore) << "Invalid QTCODE_PROJECT_TERMINAL_WD_MODE:"
                                          << projectWorkingDirectoryMode;
                    return false;
                }

                if (const QByteArray customPath = qgetenv("QTCODE_PROJECT_TERMINAL_WD_PATH");
                    !customPath.isEmpty()) {
                    profile.customWorkingDirectory = QString::fromUtf8(customPath);
                }

                if (!m_terminalManager->setProjectProfile(activeProjectId, profile, errorMessage)) {
                    qCWarning(qtcodeCore) << "Failed to apply QTCODE_PROJECT_TERMINAL_WD_MODE:"
                                          << (errorMessage != nullptr ? *errorMessage : QString());
                    return false;
                }
            }
        }
    }

    scheduleStartupMcpHealthChecks();

    qCInfo(qtcodeCore) << "Application services initialized";
    return true;
}

void ApplicationController::shutdown()
{
    m_memoryService.reset();
    m_mcpServerService.reset();
    m_agentManager.reset();
    m_cliCapabilityService.reset();
    m_terminalManager.reset();
    m_projectManager.reset();
    m_gitService.reset();
    m_settingsService.reset();

    if (m_storageService == nullptr) {
        return;
    }

    m_storageService->close();
    m_storageService.reset();
    qCInfo(qtcodeCore) << "Application services shut down";
}

storage::StorageService *ApplicationController::storageService() const
{
    return m_storageService.get();
}

SettingsService *ApplicationController::settingsService() const
{
    return m_settingsService.get();
}

ProjectManager *ApplicationController::projectManager() const
{
    return m_projectManager.get();
}

git::GitService *ApplicationController::gitService() const
{
    return m_gitService.get();
}

terminal::TerminalManager *ApplicationController::terminalManager() const
{
    return m_terminalManager.get();
}

CliCapabilityService *ApplicationController::cliCapabilityService() const
{
    return m_cliCapabilityService.get();
}

agents::AgentManager *ApplicationController::agentManager() const
{
    return m_agentManager.get();
}

McpServerService *ApplicationController::mcpServerService() const
{
    return m_mcpServerService.get();
}

memory::MemoryService *ApplicationController::memoryService() const
{
    return m_memoryService.get();
}

bool ApplicationController::runSmokeTestAgentPromptIfRequested(
    QString *errorMessage,
    QString *sessionId)
{
    const QByteArray promptBytes = qgetenv("QTCODE_AGENT_PROMPT");
    if (promptBytes.isEmpty()) {
        return true;
    }

    if (m_agentManager == nullptr || m_projectManager == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent services are unavailable.");
        }
        return false;
    }

    if (!m_projectManager->hasActiveProject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("QTCODE_AGENT_PROMPT requires an active project.");
        }
        return false;
    }

    settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The active project could not be loaded.");
        }
        return false;
    }

    agents::AgentSession *session = m_agentManager->createSession(
        activeProject.id,
        QStringLiteral("codex"),
        activeProject.name,
        errorMessage);
    if (session == nullptr) {
        return false;
    }

    agents::AgentRequest request;
    request.sessionId = session->id();
    request.projectId = activeProject.id;
    request.prompt = QString::fromUtf8(promptBytes);
    request.workingDirectory = activeProject.rootPath;
    request.nonInteractive = true;

    if (!m_agentManager->dispatchRequest(session->id(), request, errorMessage)) {
        qCWarning(qtcodeCore) << "Failed to run QTCODE_AGENT_PROMPT:"
                            << (errorMessage != nullptr ? *errorMessage : QString());
        return false;
    }

    if (sessionId != nullptr) {
        *sessionId = session->id();
    }

    qCInfo(qtcodeCore) << "Dispatched smoke-test agent prompt for project" << activeProject.name;
    return true;
}

bool ApplicationController::runSmokeTestDiffArtifactIfRequested(QString *errorMessage)
{
    const QByteArray relativePathBytes = qgetenv("QTCODE_AGENT_DIFF_FILE");
    if (relativePathBytes.isEmpty()) {
        return true;
    }

    if (m_agentManager == nullptr || m_projectManager == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Agent services are unavailable.");
        }
        return false;
    }

    if (!m_projectManager->hasActiveProject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("QTCODE_AGENT_DIFF_FILE requires an active project.");
        }
        return false;
    }

    settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The active project could not be loaded.");
        }
        return false;
    }

    qtcode::agents::AgentSession *targetSession = nullptr;
    QList<qtcode::agents::AgentSession *> projectSessions =
        m_agentManager->sessionsForProject(activeProject.id);
    std::sort(
        projectSessions.begin(),
        projectSessions.end(),
        [](const qtcode::agents::AgentSession *left, const qtcode::agents::AgentSession *right) {
            if (left == nullptr || right == nullptr) {
                return left != nullptr;
            }
            return left->updatedAt() > right->updatedAt();
        });
    if (!projectSessions.isEmpty()) {
        targetSession = projectSessions.first();
    }

    if (targetSession == nullptr) {
        targetSession = m_agentManager->createSession(
            activeProject.id,
            QStringLiteral("codex"),
            {},
            errorMessage);
    }

    if (targetSession == nullptr) {
        return false;
    }

    const QString kind = qEnvironmentVariableIsSet("QTCODE_AGENT_DIFF_KIND")
        ? QString::fromUtf8(qgetenv("QTCODE_AGENT_DIFF_KIND"))
        : QStringLiteral("file_write");

    qtcode::agents::AgentArtifact artifact;
    artifact.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    artifact.kind = kind;
    artifact.title = QString::fromUtf8(relativePathBytes);
    artifact.filePath = QString::fromUtf8(relativePathBytes);
    artifact.content = QString::fromUtf8(qgetenv("QTCODE_AGENT_DIFF_CONTENT"));
    artifact.reviewState = qtcode::agents::ArtifactReviewState::Pending;

    if (!m_agentManager->addArtifact(targetSession->id(), artifact, errorMessage)) {
        return false;
    }

    qCInfo(qtcodeCore) << "Registered smoke-test diff artifact for" << artifact.filePath;
    return true;
}

void ApplicationController::scheduleStartupMcpHealthChecks()
{
    if (m_memoryService == nullptr || m_mcpServerService == nullptr) {
        return;
    }

    const QList<settings::McpServerRecord> servers = m_mcpServerService->servers();
    if (servers.isEmpty()) {
        return;
    }

    QString workingDirectory;
    if (m_projectManager != nullptr && m_projectManager->hasActiveProject()) {
        settings::ProjectRecord activeProject;
        if (m_projectManager->activeProject(&activeProject)) {
            workingDirectory = activeProject.rootPath;
        }
    }

    QTimer::singleShot(0, m_memoryService.get(), [this, servers, workingDirectory]() {
        if (m_memoryService == nullptr) {
            return;
        }
        m_memoryService->checkEnabledServers(servers, workingDirectory);
    });
}

} // namespace qtcode::core
