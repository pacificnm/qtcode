#pragma once

#include "settings/AppConfig.h"

#include "settings/SettingsModels.h"

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QWidget;

namespace qtcode::core {
class AppConfigService;
class ProjectManager;
} // namespace qtcode::core

namespace qtcode::agents {
class AgentManager;
} // namespace qtcode::agents

namespace qtcode::ui {

class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(
        qtcode::core::AppConfigService *appConfigService,
        qtcode::core::ProjectManager *projectManager = nullptr,
        qtcode::agents::AgentManager *agentManager = nullptr,
        QWidget *parent = nullptr);

private slots:
    void saveSettings();

private:
    void configureLayout();
    void loadCurrentValues();
    void refreshRepositorySection();
    void refreshDefaultAgentSelector();
    [[nodiscard]] qtcode::settings::AppConfig currentConfig() const;
    [[nodiscard]] bool saveRepositorySettings(QString *errorMessage) const;
    void setStatusMessage(const QString &message);

    qtcode::core::AppConfigService *m_appConfigService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    qtcode::agents::AgentManager *m_agentManager = nullptr;
    QGroupBox *m_globalGroupBox = nullptr;
    QGroupBox *m_repositoryGroupBox = nullptr;
    QCheckBox *m_restoreLastProjectCheckbox = nullptr;
    QCheckBox *m_startMaximizedCheckbox = nullptr;
    QSpinBox *m_leftPanelWidthSpinBox = nullptr;
    QSpinBox *m_rightPanelWidthSpinBox = nullptr;
    QLabel *m_noActiveRepositoryLabel = nullptr;
    QLabel *m_activeRepositoryLabel = nullptr;
    QLineEdit *m_repoHelpEntryLineEdit = nullptr;
    QComboBox *m_defaultAgentComboBox = nullptr;
    QWidget *m_repositoryDetailsWidget = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

} // namespace qtcode::ui
