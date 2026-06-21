#pragma once

#include <QDialog>
#include <QHash>

#include "memory/McpHealthResult.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
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

class McpServerDialog final : public QDialog
{
    Q_OBJECT

public:
    McpServerDialog(
        qtcode::core::McpServerService *mcpServerService,
        qtcode::memory::MemoryService *memoryService,
        qtcode::core::ProjectManager *projectManager,
        const QString &serverId = {},
        QWidget *parent = nullptr);

private slots:
    void saveServer();
    void deleteServer();
    void testServerHealth();
    void onServerHealthUpdated(const QString &serverId, const qtcode::memory::McpHealthResult &result);
    void refreshSecretValueInputs();

private:
    void configureLayout();
    void loadServer(const QString &serverId);
    void clearForm();
    void refreshHealthDisplay();
    [[nodiscard]] QString healthWorkingDirectory() const;
    [[nodiscard]] qtcode::settings::McpServerRecord currentFormRecord() const;
    void setStatusMessage(const QString &message);
    void storeSecretValuesFromForm(const qtcode::settings::McpServerRecord &server);
    void clearSecretValuesFromWallet(const qtcode::settings::McpServerRecord &server);

    qtcode::core::McpServerService *m_mcpServerService = nullptr;
    qtcode::memory::MemoryService *m_memoryService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    QString m_editingServerId;
    QLineEdit *m_nameInput = nullptr;
    QLineEdit *m_endpointInput = nullptr;
    QComboBox *m_transportSelector = nullptr;
    QPlainTextEdit *m_argsInput = nullptr;
    QPlainTextEdit *m_secretKeysInput = nullptr;
    QGroupBox *m_secretValuesGroup = nullptr;
    QFormLayout *m_secretValuesLayout = nullptr;
    QHash<QString, QLineEdit *> m_secretValueInputs;
    QCheckBox *m_enabledCheckbox = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_testHealthButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_healthStateLabel = nullptr;
};

} // namespace qtcode::ui
