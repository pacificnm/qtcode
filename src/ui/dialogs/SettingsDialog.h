#pragma once

#include "settings/AppConfig.h"

#include <QDialog>
#include <QString>

class QCheckBox;
class QDialogButtonBox;

namespace qtcode::core {
class AppConfigService;
} // namespace qtcode::core

namespace qtcode::ui {

class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(qtcode::core::AppConfigService *appConfigService, QWidget *parent = nullptr);

private slots:
    void saveSettings();

private:
    void configureLayout();
    void loadCurrentValues();
    [[nodiscard]] qtcode::settings::AppConfig currentConfig() const;
    void setStatusMessage(const QString &message);

    qtcode::core::AppConfigService *m_appConfigService = nullptr;
    QCheckBox *m_restoreLastProjectCheckbox = nullptr;
    QCheckBox *m_startMaximizedCheckbox = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

} // namespace qtcode::ui
