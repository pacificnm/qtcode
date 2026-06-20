#pragma once

#include <QWidget>

class QLabel;

namespace qtcode::core {
class CliCapabilityService;
} // namespace qtcode::core

namespace qtcode::ui {

class AgentPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit AgentPanel(
        qtcode::core::CliCapabilityService *cliCapabilityService,
        QWidget *parent = nullptr);

private:
    void configureLayout();
    void refreshCapabilityState();

    qtcode::core::CliCapabilityService *m_cliCapabilityService = nullptr;
    QLabel *m_stateLabel = nullptr;
};

} // namespace qtcode::ui
