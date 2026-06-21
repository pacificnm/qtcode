#pragma once

#include <QHash>
#include <QWidget>

#include "memory/McpHealthResult.h"

class QFormLayout;
class QGroupBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace qtcode::core {
class McpServerService;
class ProjectManager;
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
        QWidget *parent = nullptr);

private slots:
    void refreshServerList();
    void onServerSelectionChanged();
    void createNewServer();
    void saveCurrentServer();
    void deleteCurrentServer();
    void testSelectedServerHealth();
    void onServerHealthUpdated(const QString &serverId, const qtcode::memory::McpHealthResult &result);

private:
    void configureLayout();
    void loadSelectedServerIntoForm();
    void refreshHealthDisplay();
    [[nodiscard]] QString healthWorkingDirectory() const;
    [[nodiscard]] QString serverListLabel(const qtcode::settings::McpServerRecord &server) const;
    [[nodiscard]] qtcode::settings::McpServerRecord currentFormRecord() const;
    void setStatusMessage(const QString &message);
    void clearForm();
    void refreshSecretValueInputs();
    void storeSecretValuesFromForm(const qtcode::settings::McpServerRecord &server);
    void clearSecretValuesFromWallet(const qtcode::settings::McpServerRecord &server);

    qtcode::core::McpServerService *m_mcpServerService = nullptr;
    qtcode::memory::MemoryService *m_memoryService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    QListWidget *m_serverList = nullptr;
    QLineEdit *m_nameInput = nullptr;
    QLineEdit *m_endpointInput = nullptr;
    QComboBox *m_transportSelector = nullptr;
    QPlainTextEdit *m_argsInput = nullptr;
    QPlainTextEdit *m_secretKeysInput = nullptr;
    QGroupBox *m_secretValuesGroup = nullptr;
    QFormLayout *m_secretValuesLayout = nullptr;
    QHash<QString, QLineEdit *> m_secretValueInputs;
    QCheckBox *m_enabledCheckbox = nullptr;
    QPushButton *m_newButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_testHealthButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_healthStateLabel = nullptr;
    QString m_editingServerId;
};

} // namespace qtcode::ui
