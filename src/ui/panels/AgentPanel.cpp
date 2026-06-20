#include "ui/panels/AgentPanel.h"

#include <KLocalizedString>

#include <QLabel>
#include <QVBoxLayout>

namespace qtcode::ui {

AgentPanel::AgentPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel(i18n("AI Agent"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *emptyStateLabel = new QLabel(
        i18n("Choose an agent and start a conversation once a repository is active."),
        this);
    emptyStateLabel->setWordWrap(true);
    emptyStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    layout->addWidget(titleLabel);
    layout->addWidget(emptyStateLabel);
    layout->addStretch();
}

} // namespace qtcode::ui
