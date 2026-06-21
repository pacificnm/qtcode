#include "ui/dialogs/SettingsDialog.h"

#include "core/AppConfigService.h"
#include "shared/Logging.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace qtcode::ui {

SettingsDialog::SettingsDialog(qtcode::core::AppConfigService *appConfigService, QWidget *parent)
    : QDialog(parent)
    , m_appConfigService(appConfigService)
{
    setWindowTitle(i18n("Settings"));
    setModal(true);
    setMinimumWidth(420);

    configureLayout();
    loadCurrentValues();
}

void SettingsDialog::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *headerLabel = new QLabel(
        i18n("These options are stored in QTCode's KDE configuration file and are loaded before SQLite."),
        this);
    headerLabel->setWordWrap(true);
    layout->addWidget(headerLabel);

    auto *formLayout = new QFormLayout();
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_restoreLastProjectCheckbox = new QCheckBox(i18n("Restore last active project on startup"), this);
    m_startMaximizedCheckbox = new QCheckBox(i18n("Start main window maximized"), this);

    formLayout->addRow(QString(), m_restoreLastProjectCheckbox);
    formLayout->addRow(QString(), m_startMaximizedCheckbox);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    m_buttonBox->button(QDialogButtonBox::Save)->setText(i18n("Save"));

    layout->addLayout(formLayout);
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
}

qtcode::settings::AppConfig SettingsDialog::currentConfig() const
{
    qtcode::settings::AppConfig config;
    config.restoreLastProjectOnStartup =
        m_restoreLastProjectCheckbox != nullptr ? m_restoreLastProjectCheckbox->isChecked() : true;
    config.startMaximized =
        m_startMaximizedCheckbox != nullptr ? m_startMaximizedCheckbox->isChecked() : false;
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
