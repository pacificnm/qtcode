#include "ui/panels/TerminalPanel.h"

#include "shared/Logging.h"

#include <KLocalizedString>

#include <qtermwidget.h>

#include <QLabel>
#include <QMouseEvent>
#include <QShowEvent>
#include <QVBoxLayout>

namespace qtcode::ui {

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    configureLayout();
    configureTerminal();
}

void TerminalPanel::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel(i18n("Terminal"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    m_terminal = new QTermWidget(0, this);
    m_terminal->setFocusPolicy(Qt::StrongFocus);

    layout->addWidget(titleLabel);
    layout->addWidget(m_terminal, 1);
}

void TerminalPanel::configureTerminal()
{
    if (m_terminal == nullptr) {
        return;
    }

    const QString shellProgram = defaultShellProgram();
    m_terminal->setShellProgram(shellProgram);
    m_terminal->setTerminalSizeHint(true);
    m_terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);
    m_terminal->startShellProgram();

    const int shellPid = m_terminal->getShellPID();
    if (shellPid <= 0) {
        qCWarning(qtcodeTerminal) << "Terminal shell did not start for" << shellProgram;
        return;
    }

    qCInfo(qtcodeTerminal) << "Started terminal shell" << shellProgram << "pid" << shellPid;
}

QString TerminalPanel::defaultShellProgram()
{
    const QByteArray shell = qgetenv("SHELL");
    if (!shell.isEmpty()) {
        return QString::fromLocal8Bit(shell);
    }

    return QStringLiteral("/bin/bash");
}

void TerminalPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (m_terminal != nullptr) {
        m_terminal->setFocus(Qt::OtherFocusReason);
    }
}

void TerminalPanel::mousePressEvent(QMouseEvent *event)
{
    if (m_terminal != nullptr) {
        m_terminal->setFocus(Qt::MouseFocusReason);
    }

    QWidget::mousePressEvent(event);
}

} // namespace qtcode::ui
