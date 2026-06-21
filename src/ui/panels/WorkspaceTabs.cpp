#include "ui/panels/WorkspaceTabs.h"

#include "shared/Logging.h"

#include <KLocalizedString>

#include <QFileInfo>
#include <QLabel>
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

void WorkspaceTabs::requestOpenFile(const QString &absolutePath)
{
    if (m_tabWidget == nullptr || absolutePath.isEmpty()) {
        return;
    }

    const QString pathKey = normalizedPath(absolutePath);
    if (pathKey.isEmpty()) {
        return;
    }

    const auto existingIndex = m_fileTabIndices.constFind(pathKey);
    if (existingIndex != m_fileTabIndices.constEnd() && *existingIndex >= 0
        && *existingIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(*existingIndex);
        qCInfo(qtcodeUi) << "Focused existing workspace tab for" << pathKey;
        return;
    }

    const QFileInfo fileInfo(pathKey);
    auto *placeholder = new QLabel(
        i18n("Editor tab for %1\n\nKTextEditor integration arrives in a later milestone.", fileInfo.fileName()),
        m_tabWidget);
    placeholder->setWordWrap(true);
    placeholder->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    placeholder->setContentsMargins(12, 12, 12, 12);

    const int tabIndex = m_tabWidget->addTab(placeholder, fileInfo.fileName());
    m_fileTabIndices.insert(pathKey, tabIndex);
    m_tabWidget->setCurrentIndex(tabIndex);
    qCInfo(qtcodeUi) << "Opened workspace tab for" << pathKey;
}

QString WorkspaceTabs::normalizedPath(const QString &absolutePath) const
{
    return QFileInfo(absolutePath).absoluteFilePath();
}

void WorkspaceTabs::onTabCloseRequested(int index)
{
    if (m_tabWidget == nullptr || index == m_aiChatTabIndex) {
        return;
    }

    const QString pathToRemove = m_fileTabIndices.key(index, QString());
    if (!pathToRemove.isEmpty()) {
        m_fileTabIndices.remove(pathToRemove);
    }

    QWidget *widget = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);

    for (auto it = m_fileTabIndices.begin(); it != m_fileTabIndices.end(); ++it) {
        if (it.value() > index) {
            it.value() -= 1;
        }
    }

    if (m_aiChatTabIndex > index) {
        m_aiChatTabIndex -= 1;
    }

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
