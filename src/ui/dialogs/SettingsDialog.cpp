#include "ui/dialogs/SettingsDialog.h"

#include "agents/AgentAdapter.h"
#include "agents/AgentManager.h"
#include "core/AppConfigService.h"
#include "core/ProjectManager.h"
#include "core/RepoConfigLoader.h"
#include "core/RepoConfigWriter.h"
#include "settings/RepoConfig.h"
#include "shared/Logging.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace qtcode::ui {

SettingsDialog::SettingsDialog(
    qtcode::core::AppConfigService *appConfigService,
    qtcode::core::ProjectManager *projectManager,
    qtcode::agents::AgentManager *agentManager,
    QWidget *parent)
    : QDialog(parent)
    , m_appConfigService(appConfigService)
    , m_projectManager(projectManager)
    , m_agentManager(agentManager)
{
    setWindowTitle(i18n("Settings"));
    setModal(true);
    setMinimumWidth(580);
    resize(620, sizeHint().height());

    configureLayout();
    loadCurrentValues();
}

void SettingsDialog::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    m_globalGroupBox = new QGroupBox(i18n("Global"), this);
    auto *globalFormLayout = new QFormLayout(m_globalGroupBox);
    globalFormLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_restoreLastProjectCheckbox = new QCheckBox(i18n("Restore last active project on startup"), m_globalGroupBox);
    m_startMaximizedCheckbox = new QCheckBox(i18n("Start main window maximized"), m_globalGroupBox);

    m_leftPanelWidthSpinBox = new QSpinBox(m_globalGroupBox);
    m_leftPanelWidthSpinBox->setRange(
        qtcode::settings::kLeftColumnMinWidth,
        qtcode::settings::kLeftColumnMaxWidth);
    m_leftPanelWidthSpinBox->setSuffix(i18n(" px"));

    m_rightPanelWidthSpinBox = new QSpinBox(m_globalGroupBox);
    m_rightPanelWidthSpinBox->setRange(
        qtcode::settings::kRightColumnMinWidth,
        qtcode::settings::kRightColumnMaxWidth);
    m_rightPanelWidthSpinBox->setSuffix(i18n(" px"));

    globalFormLayout->addRow(QString(), m_restoreLastProjectCheckbox);
    globalFormLayout->addRow(QString(), m_startMaximizedCheckbox);
    globalFormLayout->addRow(i18n("Left panel width"), m_leftPanelWidthSpinBox);
    globalFormLayout->addRow(i18n("Right panel width"), m_rightPanelWidthSpinBox);

    m_repositoryGroupBox = new QGroupBox(i18n("Repository"), this);
    auto *repositoryLayout = new QVBoxLayout(m_repositoryGroupBox);

    m_noActiveRepositoryLabel = new QLabel(
        i18n("No active repository. Select or open a project to edit repository settings."),
        m_repositoryGroupBox);
    m_noActiveRepositoryLabel->setWordWrap(true);
    repositoryLayout->addWidget(m_noActiveRepositoryLabel);

    auto *repositoryDetailsWidget = new QWidget(m_repositoryGroupBox);
    auto *repositoryFormLayout = new QFormLayout(repositoryDetailsWidget);
    repositoryFormLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    repositoryFormLayout->setContentsMargins(0, 0, 0, 0);

    m_activeRepositoryLabel = new QLabel(repositoryDetailsWidget);
    m_activeRepositoryLabel->setWordWrap(true);

    m_repoHelpEntryLineEdit = new QLineEdit(repositoryDetailsWidget);
    m_repoHelpEntryLineEdit->setPlaceholderText(
        QString::fromLatin1(qtcode::settings::kAppConfigDefaultRepoHelpPath));

    m_defaultAgentComboBox = new QComboBox(repositoryDetailsWidget);
    m_defaultAgentComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    repositoryFormLayout->addRow(i18n("Active repository"), m_activeRepositoryLabel);
    repositoryFormLayout->addRow(i18n("Default agent"), m_defaultAgentComboBox);
    repositoryFormLayout->addRow(i18n("Repo help entry"), m_repoHelpEntryLineEdit);

    m_repositoryDetailsWidget = repositoryDetailsWidget;
    repositoryLayout->addWidget(repositoryDetailsWidget);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    m_buttonBox->button(QDialogButtonBox::Save)->setText(i18n("Save"));

    layout->addWidget(m_globalGroupBox);
    layout->addWidget(m_repositoryGroupBox);
    layout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::loadCurrentValues()
{
    const qtcode::settings::AppConfig config =
        m_appConfigService != nullptr ? m_appConfigService->config()
                                      : qtcode::settings::AppConfig::defaults();

    if (m_restoreLastProjectCheckbox != nullptr) {
        m_restoreLastProjectCheckbox->setChecked(config.restoreLastProjectOnStartup);
    }
    if (m_startMaximizedCheckbox != nullptr) {
        m_startMaximizedCheckbox->setChecked(config.startMaximized);
    }
    if (m_leftPanelWidthSpinBox != nullptr) {
        m_leftPanelWidthSpinBox->setValue(
            qtcode::settings::clampLeftPanelWidth(config.leftPanelWidth));
    }
    if (m_rightPanelWidthSpinBox != nullptr) {
        m_rightPanelWidthSpinBox->setValue(
            qtcode::settings::clampRightPanelWidth(config.rightPanelWidth));
    }

    refreshRepositorySection();
}

void SettingsDialog::refreshDefaultAgentSelector()
{
    if (m_defaultAgentComboBox == nullptr) {
        return;
    }

    const QString previousAgentKey = m_defaultAgentComboBox->currentData().toString();
    m_defaultAgentComboBox->clear();

    if (m_agentManager != nullptr) {
        for (qtcode::agents::AgentAdapter *adapter : m_agentManager->adapters()) {
            if (adapter == nullptr) {
                continue;
            }

            m_defaultAgentComboBox->addItem(adapter->displayName(), adapter->agentKey());
        }
    }

    m_defaultAgentComboBox->setEnabled(m_defaultAgentComboBox->count() > 0);
    if (m_defaultAgentComboBox->count() == 0) {
        return;
    }

    int selectedIndex = 0;
    if (!previousAgentKey.isEmpty()) {
        selectedIndex = m_defaultAgentComboBox->findData(previousAgentKey);
        if (selectedIndex < 0) {
            selectedIndex = 0;
        }
    }

    m_defaultAgentComboBox->setCurrentIndex(selectedIndex);
}

void SettingsDialog::refreshRepositorySection()
{
    const bool hasActiveProject =
        m_projectManager != nullptr && m_projectManager->hasActiveProject();

    if (m_noActiveRepositoryLabel != nullptr) {
        m_noActiveRepositoryLabel->setVisible(!hasActiveProject);
    }
    if (m_repositoryDetailsWidget != nullptr) {
        m_repositoryDetailsWidget->setVisible(hasActiveProject);
    }

    if (!hasActiveProject) {
        if (m_defaultAgentComboBox != nullptr) {
            m_defaultAgentComboBox->clear();
            m_defaultAgentComboBox->setEnabled(false);
        }
        return;
    }

    refreshDefaultAgentSelector();

    qtcode::settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        return;
    }

    const qtcode::settings::AppConfig appConfig =
        m_appConfigService != nullptr ? m_appConfigService->config()
                                      : qtcode::settings::AppConfig::defaults();
    const qtcode::settings::RepoConfig repoConfig =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(activeProject.rootPath);

    if (m_activeRepositoryLabel != nullptr) {
        m_activeRepositoryLabel->setText(activeProject.name);
    }
    if (m_repoHelpEntryLineEdit != nullptr) {
        m_repoHelpEntryLineEdit->setText(
            repoConfig.hasRepoHelpPath() ? repoConfig.repoHelpPath : QString());
        m_repoHelpEntryLineEdit->setPlaceholderText(appConfig.repoHelpPath);
    }
    if (m_defaultAgentComboBox != nullptr && m_defaultAgentComboBox->count() > 0) {
        int selectedIndex = 0;
        if (repoConfig.hasDefaultAgentKey()) {
            selectedIndex = m_defaultAgentComboBox->findData(repoConfig.defaultAgentKey);
            if (selectedIndex < 0) {
                selectedIndex = 0;
            }
        }
        m_defaultAgentComboBox->setCurrentIndex(selectedIndex);
    }
}

qtcode::settings::AppConfig SettingsDialog::currentConfig() const
{
    const qtcode::settings::AppConfig existingConfig =
        m_appConfigService != nullptr ? m_appConfigService->config()
                                      : qtcode::settings::AppConfig::defaults();

    qtcode::settings::AppConfig config;
    config.restoreLastProjectOnStartup =
        m_restoreLastProjectCheckbox != nullptr ? m_restoreLastProjectCheckbox->isChecked() : true;
    config.startMaximized =
        m_startMaximizedCheckbox != nullptr ? m_startMaximizedCheckbox->isChecked() : false;
    config.repoHelpPath = existingConfig.repoHelpPath;
    config.leftPanelWidth = qtcode::settings::clampLeftPanelWidth(
        m_leftPanelWidthSpinBox != nullptr ? m_leftPanelWidthSpinBox->value()
                                           : qtcode::settings::kLeftColumnDefaultWidth);
    config.rightPanelWidth = qtcode::settings::clampRightPanelWidth(
        m_rightPanelWidthSpinBox != nullptr ? m_rightPanelWidthSpinBox->value()
                                            : qtcode::settings::kRightColumnDefaultWidth);
    return config;
}

bool SettingsDialog::saveRepositorySettings(QString *errorMessage) const
{
    if (m_projectManager == nullptr || !m_projectManager->hasActiveProject()) {
        return true;
    }

    qtcode::settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        return true;
    }

    const QString repoHelpPath =
        m_repoHelpEntryLineEdit != nullptr ? m_repoHelpEntryLineEdit->text() : QString();
    const QString defaultAgentKey =
        m_defaultAgentComboBox != nullptr && m_defaultAgentComboBox->currentIndex() >= 0
        ? m_defaultAgentComboBox->currentData().toString()
        : QString();

    qtcode::settings::RepoConfig config =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(activeProject.rootPath);
    config.repoHelpPath = repoHelpPath.trimmed().isEmpty() ? QString() : repoHelpPath.trimmed();
    config.defaultAgentKey =
        defaultAgentKey.trimmed().isEmpty() ? QString() : defaultAgentKey.trimmed();

    return qtcode::core::RepoConfigWriter::save(activeProject.rootPath, config, errorMessage);
}

void SettingsDialog::setStatusMessage(const QString &message)
{
    QMessageBox::warning(this, i18n("Save Settings Failed"), message);
    qCWarning(qtcodeUi) << message;
}

void SettingsDialog::saveSettings()
{
    if (m_appConfigService == nullptr) {
        setStatusMessage(i18n("Application config service is unavailable."));
        return;
    }

    QString errorMessage;
    if (!saveRepositorySettings(&errorMessage)) {
        setStatusMessage(errorMessage);
        return;
    }

    const qtcode::settings::AppConfig config = currentConfig();
    if (!m_appConfigService->save(config, &errorMessage)) {
        setStatusMessage(errorMessage);
        return;
    }

    accept();
}

} // namespace qtcode::ui
