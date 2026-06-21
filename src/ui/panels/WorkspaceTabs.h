#pragma once

#include <QWidget>

class QTabWidget;

namespace qtcode::ui {

class WorkspaceTabs final : public QWidget
{
    Q_OBJECT

public:
    explicit WorkspaceTabs(QWidget *parent = nullptr);

    void setPermanentAiChatTab(QWidget *conversationPanel);

    [[nodiscard]] int aiChatTabIndex() const;

private slots:
    void onTabCloseRequested(int index);

private:
    void configureLayout();
    void refreshPermanentTabCloseButton();

    QTabWidget *m_tabWidget = nullptr;
    int m_aiChatTabIndex = -1;
};

} // namespace qtcode::ui
