#include "ui/panels/TerminalPanel.h"

#include "core/ProjectManager.h"
#include "shared/Logging.h"
#include "terminal/TerminalManager.h"
#include "terminal/TerminalSession.h"

#include <KLocalizedString>

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr auto kTerminalSessionIdProperty = "qtcode_session_id";

} // namespace

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

    if (m_projectManager != nullptr) {
        connect(
            m_projectManager,
            &qtcode::core::ProjectManager::activeProjectChanged,
            this,
            &TerminalPanel::onActiveProjectChanged);
    }

    restoreOrCreateInitialTabs();

    if (m_projectManager != nullptr && m_projectManager->hasActiveProject()) {
        onActiveProjectChanged(m_projectManager->activeProjectId());
    }
}

void TerminalPanel::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    auto *titleLabel = new QLabel(i18n("Terminal"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    m_newTerminalButton = new QToolButton(this);
    m_newTerminalButton->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    m_newTerminalButton->setToolTip(i18n("New Terminal Tab"));
    m_newTerminalButton->setAccessibleName(i18n("New Terminal Tab"));
    m_newTerminalButton->setAutoRaise(true);
    connect(m_newTerminalButton, &QToolButton::clicked, this, &TerminalPanel::addTerminalTab);

    m_collapseButton = new QToolButton(this);
    m_collapseButton->setAutoRaise(true);
    m_collapseButton->setAccessibleName(i18n("Collapse Terminal"));
    connect(m_collapseButton, &QToolButton::clicked, this, &TerminalPanel::toggleCollapsed);

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_collapseButton);
    headerLayout->addWidget(m_newTerminalButton);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &TerminalPanel::closeTerminalTab);

    layout->addLayout(headerLayout);
    layout->addWidget(m_tabWidget, 1);

    m_tabWidget->hide();
    m_collapsedHeight = sizeHint().height();
    m_tabWidget->show();
    updateCollapseButton();
}

void TerminalPanel::restoreOrCreateInitialTabs()
{
    if (m_terminalManager == nullptr) {
        return;
    }

    const QList<qtcode::terminal::TerminalSession> sessions = m_terminalManager->sessions();
    if (sessions.isEmpty()) {
        if (m_projectManager != nullptr && m_projectManager->hasActiveProject()) {
            addTerminalTabForActiveProject();
        }
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

bool TerminalPanel::isCollapsed() const
{
    return m_collapsed;
}

int TerminalPanel::collapsedHeight() const
{
    return m_collapsedHeight > 0 ? m_collapsedHeight : sizeHint().height();
}

void TerminalPanel::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) {
        return;
    }

    m_collapsed = collapsed;

    if (m_tabWidget != nullptr) {
        m_tabWidget->setVisible(!collapsed);
    }

    updateCollapseButton();
    emit collapsedChanged(collapsed);
}

void TerminalPanel::toggleCollapsed()
{
    setCollapsed(!m_collapsed);
}

void TerminalPanel::updateCollapseButton()
{
    if (m_collapseButton == nullptr) {
        return;
    }

    if (m_collapsed) {
        m_collapseButton->setIcon(QIcon::fromTheme(QStringLiteral("arrow-up")));
        m_collapseButton->setToolTip(i18n("Expand Terminal"));
        m_collapseButton->setAccessibleName(i18n("Expand Terminal"));
    } else {
        m_collapseButton->setIcon(QIcon::fromTheme(QStringLiteral("arrow-down")));
        m_collapseButton->setToolTip(i18n("Collapse Terminal"));
        m_collapseButton->setAccessibleName(i18n("Collapse Terminal"));
    }
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

void TerminalPanel::onActiveProjectChanged(const QString &projectId)
{
    if (projectId.isEmpty() || m_terminalManager == nullptr || m_tabWidget == nullptr) {
        return;
    }

    QString errorMessage;
    if (!m_terminalManager->syncSessionsToActiveProject(projectId, &errorMessage)) {
        qCWarning(qtcodeTerminal) << "Failed to sync terminal session metadata:" << errorMessage;
        return;
    }

    if (m_tabWidget->count() == 0) {
        addTerminalTabForActiveProject();
    }
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

void TerminalPanel::closeTerminalTab(int index)
{
    if (m_tabWidget == nullptr || index < 0 || index >= m_tabWidget->count()) {
        return;
    }

    QWidget *terminalWidget = m_tabWidget->widget(index);
    const QString sessionId = sessionIdForWidget(terminalWidget);

    m_tabWidget->removeTab(index);
    delete terminalWidget;

    if (m_terminalManager != nullptr && !sessionId.isEmpty()) {
        QString errorMessage;
        if (!m_terminalManager->closeSession(sessionId, &errorMessage)) {
            qCWarning(qtcodeTerminal) << "Failed to close terminal session:" << errorMessage;
        }
    }

    if (m_tabWidget->count() > 0) {
        focusCurrentTerminal();
    }
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

QString TerminalPanel::sessionIdForWidget(QWidget *widget)
{
    if (widget == nullptr) {
        return {};
    }

    return widget->property(kTerminalSessionIdProperty).toString();
}

void TerminalPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_collapsed) {
        focusCurrentTerminal();
    }
}

void TerminalPanel::mousePressEvent(QMouseEvent *event)
{
    if (!m_collapsed) {
        focusCurrentTerminal();
    }
    QWidget::mousePressEvent(event);
}

} // namespace qtcode::ui
