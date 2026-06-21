#include "ui/dialogs/McpServerDialog.h"

#include "core/McpServerService.h"
#include "core/ProjectManager.h"
#include "memory/McpHealthResult.h"
#include "memory/MemoryService.h"
#include "settings/McpServerModels.h"
#include "settings/ProjectModels.h"
#include "shared/Logging.h"
#include "shared/SecretStore.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
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

McpServerDialog::McpServerDialog(
    qtcode::core::McpServerService *mcpServerService,
    qtcode::memory::MemoryService *memoryService,
    qtcode::core::ProjectManager *projectManager,
    const QString &serverId,
    QWidget *parent)
    : QDialog(parent)
    , m_mcpServerService(mcpServerService)
    , m_memoryService(memoryService)
    , m_projectManager(projectManager)
{
    setWindowTitle(serverId.isEmpty() ? i18n("Add MCP Server") : i18n("Edit MCP Server"));
    setModal(true);
    setMinimumWidth(480);

    configureLayout();

    if (m_memoryService != nullptr) {
        connect(
            m_memoryService,
            &qtcode::memory::MemoryService::serverHealthUpdated,
            this,
            &McpServerDialog::onServerHealthUpdated);
    }

    if (serverId.isEmpty()) {
        clearForm();
        setStatusMessage(i18n("Configure a new MCP server."));
    } else {
        loadServer(serverId);
    }
}

void McpServerDialog::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

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

    m_healthStateLabel = new QLabel(this);
    m_healthStateLabel->setWordWrap(true);
    m_healthStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_deleteButton = new QPushButton(i18n("Delete"), this);
    m_testHealthButton = new QPushButton(i18n("Test health"), this);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Save)->setText(i18n("Save"));

    auto *actionRow = new QHBoxLayout();
    actionRow->addWidget(m_deleteButton);
    actionRow->addWidget(m_testHealthButton);
    actionRow->addStretch(1);
    actionRow->addWidget(buttonBox);

    layout->addWidget(m_statusLabel);
    layout->addLayout(formLayout);
    layout->addWidget(m_healthStateLabel);
    layout->addLayout(actionRow);

    connect(m_secretKeysInput, &QPlainTextEdit::textChanged, this, &McpServerDialog::refreshSecretValueInputs);
    connect(m_deleteButton, &QPushButton::clicked, this, &McpServerDialog::deleteServer);
    connect(m_testHealthButton, &QPushButton::clicked, this, &McpServerDialog::testServerHealth);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &McpServerDialog::saveServer);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_deleteButton->setVisible(!m_editingServerId.isEmpty());
    m_testHealthButton->setEnabled(!m_editingServerId.isEmpty());
}

void McpServerDialog::loadServer(const QString &serverId)
{
    if (m_mcpServerService == nullptr) {
        clearForm();
        return;
    }

    const QList<qtcode::settings::McpServerRecord> servers = m_mcpServerService->servers();
    const auto match = std::find_if(
        servers.cbegin(),
        servers.cend(),
        [&serverId](const qtcode::settings::McpServerRecord &server) {
            return server.id == serverId;
        });

    if (match == servers.cend()) {
        clearForm();
        setStatusMessage(i18n("The selected MCP server could not be loaded."));
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

    m_deleteButton->setVisible(true);
    m_testHealthButton->setEnabled(true);
    setStatusMessage(i18n("Editing MCP server \"%1\".", match->name));
}

void McpServerDialog::saveServer()
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
    if (m_memoryService != nullptr && server.enabled) {
        m_memoryService->checkServerHealth(server, healthWorkingDirectory());
    }
    accept();
}

void McpServerDialog::deleteServer()
{
    if (m_mcpServerService == nullptr || m_editingServerId.isEmpty()) {
        setStatusMessage(i18n("No MCP server selected to delete."));
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

    accept();
}

void McpServerDialog::testServerHealth()
{
    if (m_memoryService == nullptr || m_mcpServerService == nullptr || m_editingServerId.isEmpty()) {
        setStatusMessage(i18n("Save the MCP server before testing health."));
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
        setStatusMessage(i18n("Save the MCP server before testing health."));
        return;
    }

    m_memoryService->checkServerHealth(*match, healthWorkingDirectory());
    setStatusMessage(i18n("Testing MCP server \"%1\"...", match->name));
}

void McpServerDialog::onServerHealthUpdated(
    const QString &serverId,
    const qtcode::memory::McpHealthResult &result)
{
    if (serverId != m_editingServerId) {
        return;
    }

    refreshHealthDisplay();
    if (result.state == qtcode::memory::McpHealthState::Healthy
        || result.state == qtcode::memory::McpHealthState::Unhealthy) {
        setStatusMessage(result.message);
    }
}

void McpServerDialog::refreshHealthDisplay()
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

QString McpServerDialog::healthWorkingDirectory() const
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

qtcode::settings::McpServerRecord McpServerDialog::currentFormRecord() const
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

void McpServerDialog::setStatusMessage(const QString &message)
{
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(message);
        m_statusLabel->setVisible(!message.isEmpty());
    }
}

void McpServerDialog::clearForm()
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
    if (m_deleteButton != nullptr) {
        m_deleteButton->setVisible(false);
    }
    if (m_testHealthButton != nullptr) {
        m_testHealthButton->setEnabled(false);
    }
}

void McpServerDialog::refreshSecretValueInputs()
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

void McpServerDialog::storeSecretValuesFromForm(const qtcode::settings::McpServerRecord &server)
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
        }
    }
}

void McpServerDialog::clearSecretValuesFromWallet(const qtcode::settings::McpServerRecord &server)
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
