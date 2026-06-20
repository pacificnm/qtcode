#include "ui/panels/TerminalPanel.h"

#include <KLocalizedString>

#include <QLabel>
#include <QVBoxLayout>

namespace qtcode::ui {

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel(i18n("Terminal"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *emptyStateLabel = new QLabel(
        i18n("Open a terminal tab to run shell commands in the active project."),
        this);
    emptyStateLabel->setWordWrap(true);
    emptyStateLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    layout->addWidget(titleLabel);
    layout->addWidget(emptyStateLabel);
    layout->addStretch();
}

} // namespace qtcode::ui
