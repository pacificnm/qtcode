#include "ui/StatusBar.h"

#include "core/StatusModels.h"
#include "core/StatusService.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QPalette>

#include <algorithm>

namespace qtcode::ui {

StatusBar::StatusBar(qtcode::core::StatusService *statusService, QWidget *parent)
    : QWidget(parent)
    , m_statusService(statusService)
{
    setObjectName(QStringLiteral("GlobalStatusBar"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(4);

    m_messageLabel = new QLabel(this);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QFont messageFont = m_messageLabel->font();
    messageFont.setPointSize(std::max(8, messageFont.pointSize() - 1));
    m_messageLabel->setFont(messageFont);

    layout->addWidget(m_messageLabel, 1);

    setMinimumHeight(24);
    setMaximumHeight(48);
    applySeverity(qtcode::core::StatusSeverity::Info);

    if (m_statusService != nullptr) {
        connect(
            m_statusService,
            &qtcode::core::StatusService::messageChanged,
            this,
            &StatusBar::onMessageChanged);
        connect(m_statusService, &qtcode::core::StatusService::cleared, this, &StatusBar::onCleared);
    }
}

void StatusBar::onMessageChanged(const QString &text, qtcode::core::StatusSeverity severity)
{
    m_messageLabel->setText(text);
    applySeverity(severity);
}

void StatusBar::onCleared()
{
    m_messageLabel->clear();
    applySeverity(qtcode::core::StatusSeverity::Info);
}

void StatusBar::applySeverity(qtcode::core::StatusSeverity severity)
{
    if (m_messageLabel == nullptr) {
        return;
    }

    QColor textColor = palette().color(QPalette::Text);

    switch (severity) {
    case qtcode::core::StatusSeverity::Warning:
        textColor = palette().color(QPalette::LinkVisited);
        break;
    case qtcode::core::StatusSeverity::Error:
        textColor = palette().color(QPalette::BrightText);
        break;
    case qtcode::core::StatusSeverity::Progress:
    case qtcode::core::StatusSeverity::Info:
    default:
        break;
    }

    QPalette labelPalette = m_messageLabel->palette();
    labelPalette.setColor(QPalette::WindowText, textColor);
    m_messageLabel->setPalette(labelPalette);
}

} // namespace qtcode::ui
