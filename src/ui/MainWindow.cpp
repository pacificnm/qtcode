#include "ui/MainWindow.h"

#include "ui/panels/AgentPanel.h"
#include "ui/panels/RepositoryPanel.h"
#include "ui/panels/TerminalPanel.h"

#include <KLocalizedString>

#include <QSplitter>

namespace qtcode::ui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(i18n("QTCode"));
    resize(1280, 800);
    configureLayout();
}

void MainWindow::configureLayout()
{
    m_repositoryPanel = new RepositoryPanel(this);
    m_agentPanel = new AgentPanel(this);
    m_terminalPanel = new TerminalPanel(this);

    m_repositoryPanel->setMinimumWidth(240);
    m_agentPanel->setMinimumWidth(320);
    m_terminalPanel->setMinimumHeight(120);

    m_horizontalSplitter = new QSplitter(Qt::Horizontal, this);
    m_horizontalSplitter->addWidget(m_repositoryPanel);
    m_horizontalSplitter->addWidget(m_agentPanel);
    m_horizontalSplitter->setStretchFactor(0, 1);
    m_horizontalSplitter->setStretchFactor(1, 2);
    m_horizontalSplitter->setCollapsible(0, false);
    m_horizontalSplitter->setCollapsible(1, false);

    m_verticalSplitter = new QSplitter(Qt::Vertical, this);
    m_verticalSplitter->addWidget(m_horizontalSplitter);
    m_verticalSplitter->addWidget(m_terminalPanel);
    m_verticalSplitter->setStretchFactor(0, 3);
    m_verticalSplitter->setStretchFactor(1, 1);
    m_verticalSplitter->setCollapsible(0, false);
    m_verticalSplitter->setCollapsible(1, false);

    setCentralWidget(m_verticalSplitter);

    m_horizontalSplitter->setSizes({360, 920});
    m_verticalSplitter->setSizes({560, 240});
}

} // namespace qtcode::ui
