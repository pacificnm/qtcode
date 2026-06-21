#include "ui/panels/McpServerPanel.h"

#include "memory/McpHealthResult.h"
#include "core/McpServerService.h"
#include "core/ProjectManager.h"
#include "core/WorkspaceInstaller.h"
#include "core/WorkspaceModels.h"
#include "memory/MemoryService.h"
#include "settings/McpServerModels.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "shared/SecretStore.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QStringList linesFromPlainText(const QPlainTextEdit *editor)
{
    QStringList lines;
    if (editor == nullptr) {
        return lines;
    }

    const QStringList rawLines = editor->toPlainText().split(QLatin1Char('\n'));
    for (const QString &line : rawLines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            lines.append(trimmed);
        }
    }
    return lines;
}

void setPlainTextLines(QPlainTextEdit *editor, const QStringList &lines)
{
    if (editor == nullptr) {
        return;
    }

    editor->setPlainText(lines.join(QStringLiteral("\n")));
}

} // namespace

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

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_serverList = new QListWidget(this);
    m_serverList->setMinimumHeight(120);

    auto *formLayout = new QFormLayout();
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_nameInput = new QLineEdit(this);
    m_endpointInput = new QLineEdit(this);
    m_transportSelector = new QComboBox(this);
    m_transportSelector->addItem(QStringLiteral("stdio"), QStringLiteral("stdio"));
    m_transportSelector->addItem(QStringLiteral("sse"), QStringLiteral("sse"));
    m_transportSelector->addItem(QStringLiteral("http"), QStringLiteral("http"));

    m_argsInput = new QPlainTextEdit(this);
    m_argsInput->setPlaceholderText(i18n("One argument per line"));
    m_argsInput->setMaximumHeight(72);

    m_secretKeysInput = new QPlainTextEdit(this);
    m_secretKeysInput->setPlaceholderText(i18n("Environment variable names, one per line"));
    m_secretKeysInput->setMaximumHeight(72);

    m_secretValuesGroup = new QGroupBox(i18n("Secret values"), this);
    m_secretValuesLayout = new QFormLayout(m_secretValuesGroup);
    m_secretValuesLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_enabledCheckbox = new QCheckBox(i18n("Enabled"), this);
    m_enabledCheckbox->setChecked(true);

    formLayout->addRow(i18n("Name"), m_nameInput);
    formLayout->addRow(i18n("Endpoint"), m_endpointInput);
    formLayout->addRow(i18n("Transport"), m_transportSelector);
    formLayout->addRow(i18n("Arguments"), m_argsInput);
    formLayout->addRow(i18n("Secret env keys"), m_secretKeysInput);
    formLayout->addRow(m_secretValuesGroup);
    formLayout->addRow(QString(), m_enabledCheckbox);

    auto *walletNote = new QLabel(
        shared::SecretStore::isWalletIntegrationAvailable()
            ? i18n("Secret values are stored in KDE Wallet. Leave a field blank when editing to keep the existing value.")
            : i18n("KDE Wallet is unavailable. Secret keys are tracked, but values cannot be stored securely."),
        this);
    walletNote->setWordWrap(true);
    formLayout->addRow(QString(), walletNote);

    m_newButton = new QPushButton(i18n("New"), this);
    m_saveButton = new QPushButton(i18n("Save"), this);
    m_deleteButton = new QPushButton(i18n("Delete"), this);
    m_testHealthButton = new QPushButton(i18n("Test health"), this);

    m_healthStateLabel = new QLabel(this);
    m_healthStateLabel->setWordWrap(true);
    m_healthStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

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

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(m_newButton);
    buttonRow->addWidget(m_saveButton);
    buttonRow->addWidget(m_deleteButton);
    buttonRow->addWidget(m_testHealthButton);
    buttonRow->addStretch(1);

    layout->addWidget(titleLabel);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_serverList, 1);
    layout->addWidget(m_healthStateLabel);
    layout->addWidget(m_workspaceHealthGroup);
    layout->addLayout(formLayout);
    layout->addLayout(buttonRow);

    connect(m_secretKeysInput, &QPlainTextEdit::textChanged, this, &McpServerPanel::refreshSecretValueInputs);
    connect(m_serverList, &QListWidget::currentRowChanged, this, &McpServerPanel::onServerSelectionChanged);
    connect(m_newButton, &QPushButton::clicked, this, &McpServerPanel::createNewServer);
    connect(m_saveButton, &QPushButton::clicked, this, &McpServerPanel::saveCurrentServer);
    connect(m_deleteButton, &QPushButton::clicked, this, &McpServerPanel::deleteCurrentServer);
    connect(m_testHealthButton, &QPushButton::clicked, this, &McpServerPanel::testSelectedServerHealth);

    refreshWorkspaceHealth();
}

void McpServerPanel::refreshServerList()
{
    if (m_mcpServerService == nullptr || m_serverList == nullptr) {
        return;
    }

    const QString previousSelectionId = m_editingServerId;
    const QSignalBlocker blocker(m_serverList);
    m_serverList->clear();

    const QList<qtcode::settings::McpServerRecord> servers = m_mcpServerService->servers();
    int rowToSelect = -1;
    for (int index = 0; index < servers.size(); ++index) {
        const qtcode::settings::McpServerRecord &server = servers.at(index);
        auto *item = new QListWidgetItem(serverListLabel(server), m_serverList);
        item->setData(Qt::UserRole, server.id);
        if (!server.enabled) {
            item->setForeground(Qt::gray);
        }
        if (server.id == previousSelectionId) {
            rowToSelect = index;
        }
    }

    if (rowToSelect >= 0) {
        m_serverList->setCurrentRow(rowToSelect);
    } else if (!servers.isEmpty()) {
        m_serverList->setCurrentRow(0);
    } else {
        clearForm();
    }

    setStatusMessage(
        servers.isEmpty() ? i18n("No MCP servers configured yet.")
                          : i18n("%1 MCP server(s) configured.", servers.size()));
    refreshHealthDisplay();
}

void McpServerPanel::onServerSelectionChanged()
{
    loadSelectedServerIntoForm();
    refreshHealthDisplay();
}

void McpServerPanel::createNewServer()
{
    clearForm();
    m_nameInput->setFocus();
    setStatusMessage(i18n("Creating a new MCP server."));
}

void McpServerPanel::saveCurrentServer()
{
    if (m_mcpServerService == nullptr) {
        setStatusMessage(i18n("MCP server service is unavailable."));
        return;
    }

    qtcode::settings::McpServerRecord server = currentFormRecord();
    QString errorMessage;
    if (!m_mcpServerService->saveServer(&server, &errorMessage)) {
        setStatusMessage(errorMessage);
        qCWarning(qtcodeUi) << "Failed to save MCP server:" << errorMessage;
        return;
    }

    m_editingServerId = server.id;
    storeSecretValuesFromForm(server);
    setStatusMessage(i18n("Saved MCP server \"%1\".", server.name));
    refreshServerList();
    if (m_memoryService != nullptr && server.enabled) {
        m_memoryService->checkServerHealth(server, healthWorkingDirectory());
    }
}

void McpServerPanel::deleteCurrentServer()
{
    if (m_mcpServerService == nullptr || m_editingServerId.isEmpty()) {
        setStatusMessage(i18n("Select an MCP server to delete."));
        return;
    }

    const QList<qtcode::settings::McpServerRecord> servers = m_mcpServerService->servers();
    const auto match = std::find_if(
        servers.cbegin(),
        servers.cend(),
        [this](const qtcode::settings::McpServerRecord &server) {
            return server.id == m_editingServerId;
        });
    if (match != servers.cend()) {
        clearSecretValuesFromWallet(*match);
    }

    QString errorMessage;
    if (!m_mcpServerService->deleteServer(m_editingServerId, &errorMessage)) {
        setStatusMessage(errorMessage);
        qCWarning(qtcodeUi) << "Failed to delete MCP server:" << errorMessage;
        return;
    }

    m_editingServerId.clear();
    clearForm();
    setStatusMessage(i18n("Deleted MCP server."));
    refreshServerList();
}

void McpServerPanel::loadSelectedServerIntoForm()
{
    if (m_serverList == nullptr || m_mcpServerService == nullptr) {
        return;
    }

    const QListWidgetItem *item = m_serverList->currentItem();
    if (item == nullptr) {
        clearForm();
        return;
    }

    const QString serverId = item->data(Qt::UserRole).toString();
    const QList<qtcode::settings::McpServerRecord> servers = m_mcpServerService->servers();
    const auto match = std::find_if(
        servers.cbegin(),
        servers.cend(),
        [&serverId](const qtcode::settings::McpServerRecord &server) {
            return server.id == serverId;
        });

    if (match == servers.cend()) {
        clearForm();
        return;
    }

    m_editingServerId = match->id;
    m_nameInput->setText(match->name);
    m_endpointInput->setText(match->endpoint);

    const int transportIndex = m_transportSelector->findData(match->transport);
    m_transportSelector->setCurrentIndex(transportIndex >= 0 ? transportIndex : 0);

    setPlainTextLines(m_argsInput, match->args());
    setPlainTextLines(m_secretKeysInput, match->secretEnvKeys());
    m_enabledCheckbox->setChecked(match->enabled);
    refreshSecretValueInputs();
    refreshHealthDisplay();
}

void McpServerPanel::testSelectedServerHealth()
{
    if (m_memoryService == nullptr || m_mcpServerService == nullptr || m_editingServerId.isEmpty()) {
        setStatusMessage(i18n("Select an MCP server to test."));
        return;
    }

    const QList<qtcode::settings::McpServerRecord> servers = m_mcpServerService->servers();
    const auto match = std::find_if(
        servers.cbegin(),
        servers.cend(),
        [this](const qtcode::settings::McpServerRecord &server) {
            return server.id == m_editingServerId;
        });
    if (match == servers.cend()) {
        setStatusMessage(i18n("Select an MCP server to test."));
        return;
    }

    m_memoryService->checkServerHealth(*match, healthWorkingDirectory());
    setStatusMessage(i18n("Testing MCP server \"%1\"...", match->name));
}

void McpServerPanel::onServerHealthUpdated(
    const QString &serverId,
    const qtcode::memory::McpHealthResult &result)
{
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

    if (serverId == m_editingServerId) {
        refreshHealthDisplay();
        if (result.state == qtcode::memory::McpHealthState::Healthy) {
            setStatusMessage(result.message);
        } else if (result.state == qtcode::memory::McpHealthState::Unhealthy) {
            setStatusMessage(result.message);
        }
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

void McpServerPanel::refreshHealthDisplay()
{
    if (m_healthStateLabel == nullptr || m_memoryService == nullptr || m_editingServerId.isEmpty()) {
        if (m_healthStateLabel != nullptr) {
            m_healthStateLabel->clear();
        }
        return;
    }

    const qtcode::memory::McpHealthResult health = m_memoryService->healthForServer(m_editingServerId);
    if (health.state == qtcode::memory::McpHealthState::Unknown) {
        m_healthStateLabel->setText(i18n("Health has not been checked yet."));
        return;
    }

    m_healthStateLabel->setText(
        i18n("%1: %2", qtcode::memory::mcpHealthStateLabel(health.state), health.message));
}

QString McpServerPanel::healthWorkingDirectory() const
{
    if (m_projectManager == nullptr || !m_projectManager->hasActiveProject()) {
        return {};
    }

    qtcode::settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        return {};
    }

    return activeProject.rootPath;
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

qtcode::settings::McpServerRecord McpServerPanel::currentFormRecord() const
{
    qtcode::settings::McpServerRecord server;
    server.id = m_editingServerId;
    server.name = m_nameInput->text();
    server.endpoint = m_endpointInput->text();
    server.transport = m_transportSelector->currentData().toString();
    server.enabled = m_enabledCheckbox->isChecked();
    server.setArgs(linesFromPlainText(m_argsInput));
    server.setSecretEnvKeys(linesFromPlainText(m_secretKeysInput));
    return server;
}

void McpServerPanel::setStatusMessage(const QString &message)
{
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(message);
    }
}

void McpServerPanel::clearForm()
{
    m_editingServerId.clear();
    if (m_nameInput != nullptr) {
        m_nameInput->clear();
    }
    if (m_endpointInput != nullptr) {
        m_endpointInput->clear();
    }
    if (m_transportSelector != nullptr) {
        m_transportSelector->setCurrentIndex(0);
    }
    if (m_argsInput != nullptr) {
        m_argsInput->clear();
    }
    if (m_secretKeysInput != nullptr) {
        m_secretKeysInput->clear();
    }
    refreshSecretValueInputs();
    if (m_enabledCheckbox != nullptr) {
        m_enabledCheckbox->setChecked(true);
    }
    if (m_healthStateLabel != nullptr) {
        m_healthStateLabel->clear();
    }
}

void McpServerPanel::refreshSecretValueInputs()
{
    if (m_secretValuesLayout == nullptr || m_secretKeysInput == nullptr) {
        return;
    }

    while (QLayoutItem *item = m_secretValuesLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_secretValueInputs.clear();

    const QStringList secretKeys = linesFromPlainText(m_secretKeysInput);
    if (m_secretValuesGroup != nullptr) {
        m_secretValuesGroup->setVisible(!secretKeys.isEmpty());
    }

    for (const QString &secretKey : secretKeys) {
        auto *valueInput = new QLineEdit(m_secretValuesGroup);
        valueInput->setEchoMode(QLineEdit::Password);
        if (!m_editingServerId.isEmpty()
            && shared::SecretStore::hasServerSecret(m_editingServerId, secretKey)) {
            valueInput->setPlaceholderText(i18n("Stored in KDE Wallet (leave blank to keep)"));
        } else {
            valueInput->setPlaceholderText(i18n("Enter secret value"));
        }
        m_secretValuesLayout->addRow(secretKey, valueInput);
        m_secretValueInputs.insert(secretKey, valueInput);
    }
}

void McpServerPanel::storeSecretValuesFromForm(const qtcode::settings::McpServerRecord &server)
{
    if (!shared::SecretStore::isWalletIntegrationAvailable()) {
        return;
    }

    for (auto it = m_secretValueInputs.cbegin(); it != m_secretValueInputs.cend(); ++it) {
        const QString secretKey = it.key();
        QLineEdit *valueInput = it.value();
        if (valueInput == nullptr) {
            continue;
        }

        const QString secretValue = valueInput->text();
        if (secretValue.isEmpty()) {
            continue;
        }

        QString errorMessage;
        if (!shared::SecretStore::storeServerSecret(server.id, secretKey, secretValue, &errorMessage)) {
            setStatusMessage(errorMessage);
            qCWarning(qtcodeUi) << "Failed to store MCP secret:" << errorMessage;
            continue;
        }

        valueInput->clear();
        valueInput->setPlaceholderText(i18n("Stored in KDE Wallet (leave blank to keep)"));
    }
}

void McpServerPanel::clearSecretValuesFromWallet(const qtcode::settings::McpServerRecord &server)
{
    if (!shared::SecretStore::isWalletIntegrationAvailable()) {
        return;
    }

    for (const QString &secretKey : server.secretEnvKeys()) {
        QString errorMessage;
        if (!shared::SecretStore::deleteServerSecret(server.id, secretKey, &errorMessage)) {
            qCWarning(qtcodeUi) << "Failed to remove MCP secret from KDE Wallet:" << errorMessage;
        }
    }
}

} // namespace qtcode::ui
