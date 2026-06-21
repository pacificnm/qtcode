#pragma once

#include "settings/AppConfig.h"

#include "settings/SettingsModels.h"

#include <QDialog>
#include <QString>

class QCheckBox;
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

namespace qtcode::ui {

class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(
        qtcode::core::AppConfigService *appConfigService,
        qtcode::core::ProjectManager *projectManager = nullptr,
        QWidget *parent = nullptr);

private slots:
    void saveSettings();

private:
    void configureLayout();
    void loadCurrentValues();
    void refreshRepositorySection();
    [[nodiscard]] qtcode::settings::AppConfig currentConfig() const;
    [[nodiscard]] bool saveRepositorySettings(QString *errorMessage) const;
    void setStatusMessage(const QString &message);

    qtcode::core::AppConfigService *m_appConfigService = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    QGroupBox *m_globalGroupBox = nullptr;
    QGroupBox *m_repositoryGroupBox = nullptr;
    QCheckBox *m_restoreLastProjectCheckbox = nullptr;
    QCheckBox *m_startMaximizedCheckbox = nullptr;
    QSpinBox *m_leftPanelWidthSpinBox = nullptr;
    QSpinBox *m_rightPanelWidthSpinBox = nullptr;
    QLabel *m_noActiveRepositoryLabel = nullptr;
    QLabel *m_activeRepositoryLabel = nullptr;
    QLineEdit *m_repoHelpEntryLineEdit = nullptr;
    QWidget *m_repositoryDetailsWidget = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

} // namespace qtcode::ui
