#include "ui/panels/TerminalPanel.h"

#include "core/ProjectManager.h"
#include "shared/Logging.h"
#include "terminal/TerminalManager.h"
#include "terminal/TerminalSession.h"

#include <KLocalizedString>

#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QTabWidget>
#include <QVBoxLayout>

namespace qtcode::ui {

TerminalPanel::TerminalPanel(
    qtcode::terminal::TerminalManager *terminalManager,
    qtcode::core::ProjectManager *projectManager,
    QWidget *parent)
    : QWidget(parent)
    , m_terminalManager(terminalManager)
    , m_projectManager(projectManager)
{
    setFocusPolicy(Qt::StrongFocus);
    configureLayout();
    restoreOrCreateInitialTabs();
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

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(false);

    m_newTerminalButton = new QPushButton(i18n("New terminal tab"), this);
    connect(m_newTerminalButton, &QPushButton::clicked, this, &TerminalPanel::addTerminalTab);

    layout->addWidget(titleLabel);
    layout->addWidget(m_tabWidget, 1);
    layout->addWidget(m_newTerminalButton);
}

void TerminalPanel::restoreOrCreateInitialTabs()
{
    if (m_terminalManager == nullptr) {
        return;
    }

    const QList<qtcode::terminal::TerminalSession> sessions = m_terminalManager->sessions();
    if (sessions.isEmpty()) {
        addTerminalTabForActiveProject();
        return;
    }

    for (const qtcode::terminal::TerminalSession &session : sessions) {
        addTerminalTabFromSession(session, true);
    }
}

void TerminalPanel::addTerminalTab()
{
    addTerminalTabForActiveProject();
}

void TerminalPanel::addTerminalTabForActiveProject()
{
    if (m_terminalManager == nullptr || m_tabWidget == nullptr) {
        return;
    }

    const QString projectId =
        m_projectManager != nullptr ? m_projectManager->activeProjectId() : QString();

    qtcode::terminal::TerminalSession session;
    QWidget *terminalWidget = nullptr;
    QString errorMessage;
    if (!m_terminalManager->createTerminal(projectId, this, &session, &terminalWidget, &errorMessage)) {
        qCWarning(qtcodeTerminal) << "Failed to create terminal tab:" << errorMessage;
        return;
    }

    m_tabWidget->addTab(terminalWidget, session.title);
    m_tabWidget->setCurrentWidget(terminalWidget);
    focusCurrentTerminal();
}

void TerminalPanel::addTerminalTabFromSession(
    const qtcode::terminal::TerminalSession &session,
    bool restored)
{
    if (m_terminalManager == nullptr || m_tabWidget == nullptr) {
        return;
    }

    QWidget *terminalWidget = nullptr;
    QString errorMessage;
    if (!m_terminalManager->restoreTerminal(session, this, &terminalWidget, &errorMessage)) {
        qCWarning(qtcodeTerminal) << "Failed to restore terminal tab:" << errorMessage;
        return;
    }

    const QString tabTitle = restored
        ? i18n("%1 (restored)", session.title)
        : session.title;
    m_tabWidget->addTab(terminalWidget, tabTitle);
    m_tabWidget->setCurrentWidget(terminalWidget);
}

void TerminalPanel::focusCurrentTerminal()
{
    if (QWidget *terminalWidget = currentTerminalWidget(); terminalWidget != nullptr) {
        terminalWidget->setFocus(Qt::OtherFocusReason);
    }
}

QWidget *TerminalPanel::currentTerminalWidget() const
{
    if (m_tabWidget == nullptr) {
        return nullptr;
    }

    return m_tabWidget->currentWidget();
}

void TerminalPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    focusCurrentTerminal();
}

void TerminalPanel::mousePressEvent(QMouseEvent *event)
{
    focusCurrentTerminal();
    QWidget::mousePressEvent(event);
}

} // namespace qtcode::ui
