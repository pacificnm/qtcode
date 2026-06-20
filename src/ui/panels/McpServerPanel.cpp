#include "ui/panels/McpServerPanel.h"

#include "core/McpServerService.h"
#include "settings/McpServerModels.h"
#include "shared/Logging.h"
#include "shared/SecretStore.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
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
    QWidget *parent)
    : QWidget(parent)
    , m_mcpServerService(mcpServerService)
{
    configureLayout();

    if (m_mcpServerService != nullptr) {
        connect(
            m_mcpServerService,
            &qtcode::core::McpServerService::serversChanged,
            this,
            &McpServerPanel::refreshServerList);
        refreshServerList();
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

    m_enabledCheckbox = new QCheckBox(i18n("Enabled"), this);
    m_enabledCheckbox->setChecked(true);

    formLayout->addRow(i18n("Name"), m_nameInput);
    formLayout->addRow(i18n("Endpoint"), m_endpointInput);
    formLayout->addRow(i18n("Transport"), m_transportSelector);
    formLayout->addRow(i18n("Arguments"), m_argsInput);
    formLayout->addRow(i18n("Secret env keys"), m_secretKeysInput);
    formLayout->addRow(QString(), m_enabledCheckbox);

    auto *walletNote = new QLabel(
        shared::SecretStore::isWalletIntegrationAvailable()
            ? i18n("Secrets are stored in KDE Wallet.")
            : i18n("KDE Wallet integration is not available yet. Secret keys are tracked for future wallet storage."),
        this);
    walletNote->setWordWrap(true);
    formLayout->addRow(QString(), walletNote);

    m_newButton = new QPushButton(i18n("New"), this);
    m_saveButton = new QPushButton(i18n("Save"), this);
    m_deleteButton = new QPushButton(i18n("Delete"), this);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(m_newButton);
    buttonRow->addWidget(m_saveButton);
    buttonRow->addWidget(m_deleteButton);
    buttonRow->addStretch(1);

    layout->addWidget(titleLabel);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_serverList, 1);
    layout->addLayout(formLayout);
    layout->addLayout(buttonRow);

    connect(m_serverList, &QListWidget::currentRowChanged, this, &McpServerPanel::onServerSelectionChanged);
    connect(m_newButton, &QPushButton::clicked, this, &McpServerPanel::createNewServer);
    connect(m_saveButton, &QPushButton::clicked, this, &McpServerPanel::saveCurrentServer);
    connect(m_deleteButton, &QPushButton::clicked, this, &McpServerPanel::deleteCurrentServer);
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
        auto *item = new QListWidgetItem(server.name, m_serverList);
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
}

void McpServerPanel::onServerSelectionChanged()
{
    loadSelectedServerIntoForm();
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
    setStatusMessage(i18n("Saved MCP server \"%1\".", server.name));
    refreshServerList();
}

void McpServerPanel::deleteCurrentServer()
{
    if (m_mcpServerService == nullptr || m_editingServerId.isEmpty()) {
        setStatusMessage(i18n("Select an MCP server to delete."));
        return;
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
    if (m_enabledCheckbox != nullptr) {
        m_enabledCheckbox->setChecked(true);
    }
}

} // namespace qtcode::ui
