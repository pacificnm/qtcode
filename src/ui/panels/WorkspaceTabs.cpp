#include "ui/panels/WorkspaceTabs.h"

#include <KLocalizedString>

#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace qtcode::ui {

WorkspaceTabs::WorkspaceTabs(QWidget *parent)
    : QWidget(parent)
{
    configureLayout();
}

void WorkspaceTabs::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setTabsClosable(true);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &WorkspaceTabs::onTabCloseRequested);

    layout->addWidget(m_tabWidget, 1);
}

void WorkspaceTabs::setPermanentAiChatTab(QWidget *conversationPanel)
{
    if (m_tabWidget == nullptr || conversationPanel == nullptr) {
        return;
    }

    if (m_aiChatTabIndex >= 0) {
        QWidget *existingWidget = m_tabWidget->widget(m_aiChatTabIndex);
        if (existingWidget == conversationPanel) {
            refreshPermanentTabCloseButton();
            return;
        }

        m_tabWidget->removeTab(m_aiChatTabIndex);
        m_aiChatTabIndex = -1;
    }

    m_aiChatTabIndex = m_tabWidget->addTab(conversationPanel, i18n("AI Chat"));
    m_tabWidget->setCurrentIndex(m_aiChatTabIndex);
    refreshPermanentTabCloseButton();
}

int WorkspaceTabs::aiChatTabIndex() const
{
    return m_aiChatTabIndex;
}

void WorkspaceTabs::onTabCloseRequested(int index)
{
    if (m_tabWidget == nullptr || index == m_aiChatTabIndex) {
        return;
    }

    QWidget *widget = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);
    if (widget != nullptr) {
        widget->deleteLater();
    }
}

void WorkspaceTabs::refreshPermanentTabCloseButton()
{
    if (m_tabWidget == nullptr || m_aiChatTabIndex < 0) {
        return;
    }

    m_tabWidget->tabBar()->setTabButton(m_aiChatTabIndex, QTabBar::RightSide, nullptr);
}

} // namespace qtcode::ui
