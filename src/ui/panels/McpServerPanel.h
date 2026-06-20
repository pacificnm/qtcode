#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace qtcode::core {
class McpServerService;
} // namespace qtcode::core

namespace qtcode::settings {
struct McpServerRecord;
} // namespace qtcode::settings

namespace qtcode::ui {

class McpServerPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit McpServerPanel(qtcode::core::McpServerService *mcpServerService, QWidget *parent = nullptr);

private slots:
    void refreshServerList();
    void onServerSelectionChanged();
    void createNewServer();
    void saveCurrentServer();
    void deleteCurrentServer();

private:
    void configureLayout();
    void loadSelectedServerIntoForm();
    [[nodiscard]] qtcode::settings::McpServerRecord currentFormRecord() const;
    void setStatusMessage(const QString &message);
    void clearForm();

    qtcode::core::McpServerService *m_mcpServerService = nullptr;
    QListWidget *m_serverList = nullptr;
    QLineEdit *m_nameInput = nullptr;
    QLineEdit *m_endpointInput = nullptr;
    QComboBox *m_transportSelector = nullptr;
    QPlainTextEdit *m_argsInput = nullptr;
    QPlainTextEdit *m_secretKeysInput = nullptr;
    QCheckBox *m_enabledCheckbox = nullptr;
    QPushButton *m_newButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QString m_editingServerId;
};

} // namespace qtcode::ui
