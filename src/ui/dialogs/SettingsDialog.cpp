#include "ui/dialogs/SettingsDialog.h"

#include "core/AppConfigService.h"
#include "core/ProjectManager.h"
#include "core/RepoConfigLoader.h"
#include "settings/RepoConfig.h"
#include "shared/Logging.h"

#include <KLocalizedString>

#include <QCheckBox>
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
    QWidget *parent)
    : QDialog(parent)
    , m_appConfigService(appConfigService)
    , m_projectManager(projectManager)
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

    auto *globalDescription = new QLabel(
        i18n("These settings apply to all repositories and are stored in QTCode's system configuration."),
        m_globalGroupBox);
    globalDescription->setWordWrap(true);
    globalFormLayout->addRow(globalDescription);

    m_restoreLastProjectCheckbox = new QCheckBox(i18n("Restore last active project on startup"), m_globalGroupBox);
    m_startMaximizedCheckbox = new QCheckBox(i18n("Start main window maximized"), m_globalGroupBox);
    m_repoHelpPathLineEdit = new QLineEdit(m_globalGroupBox);
    m_repoHelpPathLineEdit->setPlaceholderText(
        QString::fromLatin1(qtcode::settings::kAppConfigDefaultRepoHelpPath));

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
    globalFormLayout->addRow(i18n("Default repo help entry"), m_repoHelpPathLineEdit);

    auto *defaultHelpHint = new QLabel(
        i18n("Used when a repository does not define its own help entry in .qtcode/config.yaml."),
        m_globalGroupBox);
    defaultHelpHint->setWordWrap(true);
    globalFormLayout->addRow(QString(), defaultHelpHint);

    m_repositoryGroupBox = new QGroupBox(i18n("Repository"), this);
    auto *repositoryLayout = new QVBoxLayout(m_repositoryGroupBox);

    auto *repositoryDescription = new QLabel(
        i18n("These settings apply only to the active repository and are stored in .qtcode/config.yaml inside the project."),
        m_repositoryGroupBox);
    repositoryDescription->setWordWrap(true);
    repositoryLayout->addWidget(repositoryDescription);

    m_noActiveRepositoryLabel = new QLabel(
        i18n("No active repository. Select or open a project to view repository-specific settings."),
        m_repositoryGroupBox);
    m_noActiveRepositoryLabel->setWordWrap(true);
    repositoryLayout->addWidget(m_noActiveRepositoryLabel);

    auto *repositoryDetailsWidget = new QWidget(m_repositoryGroupBox);
    auto *repositoryFormLayout = new QFormLayout(repositoryDetailsWidget);
    repositoryFormLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    repositoryFormLayout->setContentsMargins(0, 0, 0, 0);

    m_activeRepositoryLabel = new QLabel(repositoryDetailsWidget);
    m_activeRepositoryLabel->setWordWrap(true);

    m_repoHelpOverrideLabel = new QLabel(repositoryDetailsWidget);
    m_repoHelpOverrideLabel->setWordWrap(true);

    m_repoHelpEffectiveLabel = new QLabel(repositoryDetailsWidget);
    m_repoHelpEffectiveLabel->setWordWrap(true);

    m_repositoryConfigHintLabel = new QLabel(
        i18n("Edit .qtcode/config.yaml in the project to change repository settings."),
        repositoryDetailsWidget);
    m_repositoryConfigHintLabel->setWordWrap(true);

    repositoryFormLayout->addRow(i18n("Active repository"), m_activeRepositoryLabel);
    repositoryFormLayout->addRow(i18n("Help entry override"), m_repoHelpOverrideLabel);
    repositoryFormLayout->addRow(i18n("Effective help entry"), m_repoHelpEffectiveLabel);
    repositoryFormLayout->addRow(QString(), m_repositoryConfigHintLabel);

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
    if (m_repoHelpPathLineEdit != nullptr) {
        m_repoHelpPathLineEdit->setText(config.repoHelpPath);
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
        return;
    }

    qtcode::settings::ProjectRecord activeProject;
    if (!m_projectManager->activeProject(&activeProject)) {
        return;
    }

    const qtcode::settings::AppConfig appConfig =
        m_appConfigService != nullptr ? m_appConfigService->config()
                                      : qtcode::settings::AppConfig::defaults();
    const qtcode::settings::RepoConfig repoConfig =
        qtcode::core::RepoConfigLoader::loadFromProjectRoot(activeProject.rootPath);
    const QString effectiveHelpPath = qtcode::settings::effectiveRepoHelpPath(appConfig, repoConfig);

    if (m_activeRepositoryLabel != nullptr) {
        m_activeRepositoryLabel->setText(activeProject.name);
    }
    if (m_repoHelpOverrideLabel != nullptr) {
        m_repoHelpOverrideLabel->setText(
            repoConfig.hasRepoHelpPath()
                ? repoConfig.repoHelpPath
                : i18n("None (using global default)"));
    }
    if (m_repoHelpEffectiveLabel != nullptr) {
        m_repoHelpEffectiveLabel->setText(effectiveHelpPath);
    }
}

qtcode::settings::AppConfig SettingsDialog::currentConfig() const
{
    qtcode::settings::AppConfig config;
    config.restoreLastProjectOnStartup =
        m_restoreLastProjectCheckbox != nullptr ? m_restoreLastProjectCheckbox->isChecked() : true;
    config.startMaximized =
        m_startMaximizedCheckbox != nullptr ? m_startMaximizedCheckbox->isChecked() : false;
    config.repoHelpPath = qtcode::settings::normalizedRepoHelpPath(
        m_repoHelpPathLineEdit != nullptr ? m_repoHelpPathLineEdit->text()
                                          : QString::fromLatin1(qtcode::settings::kAppConfigDefaultRepoHelpPath));
    config.leftPanelWidth = qtcode::settings::clampLeftPanelWidth(
        m_leftPanelWidthSpinBox != nullptr ? m_leftPanelWidthSpinBox->value()
                                           : qtcode::settings::kLeftColumnDefaultWidth);
    config.rightPanelWidth = qtcode::settings::clampRightPanelWidth(
        m_rightPanelWidthSpinBox != nullptr ? m_rightPanelWidthSpinBox->value()
                                            : qtcode::settings::kRightColumnDefaultWidth);
    return config;
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

    const qtcode::settings::AppConfig config = currentConfig();
    QString errorMessage;
    if (!m_appConfigService->save(config, &errorMessage)) {
        setStatusMessage(errorMessage);
        return;
    }

    accept();
}

} // namespace qtcode::ui
