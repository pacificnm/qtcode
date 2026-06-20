#include "ui/panels/AgentPanel.h"

#include "core/CliCapabilityService.h"
#include "core/CliCapabilityModels.h"

#include <KLocalizedString>

#include <QLabel>
#include <QVBoxLayout>

namespace qtcode::ui {

AgentPanel::AgentPanel(
    qtcode::core::CliCapabilityService *cliCapabilityService,
    QWidget *parent)
    : QWidget(parent)
    , m_cliCapabilityService(cliCapabilityService)
{
    configureLayout();
    refreshCapabilityState();

    if (m_cliCapabilityService != nullptr) {
        connect(
            m_cliCapabilityService,
            &qtcode::core::CliCapabilityService::capabilitiesDetected,
            this,
            &AgentPanel::refreshCapabilityState);
    }
}

void AgentPanel::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel(i18n("AI Agent"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    m_stateLabel = new QLabel(this);
    m_stateLabel->setWordWrap(true);
    m_stateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    layout->addWidget(titleLabel);
    layout->addWidget(m_stateLabel);
    layout->addStretch();
}

void AgentPanel::refreshCapabilityState()
{
    if (m_stateLabel == nullptr) {
        return;
    }

    if (m_cliCapabilityService == nullptr) {
        m_stateLabel->setText(i18n("Agent services are unavailable."));
        return;
    }

    const qtcode::core::CliCapabilitiesSnapshot snapshot = m_cliCapabilityService->snapshot();
    if (snapshot.agentCli.available) {
        m_stateLabel->setText(
            i18n("Detected %1 (%2). Choose an agent and start a conversation once a repository is active.",
                 snapshot.agentCli.displayName,
                 snapshot.agentCli.version));
        return;
    }

    m_stateLabel->setText(snapshot.agentCli.unavailableMessage);
}

} // namespace qtcode::ui
