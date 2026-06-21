#pragma once

#include <QHash>
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

public slots:
    void requestOpenFile(const QString &absolutePath);

private slots:
    void onTabCloseRequested(int index);

private:
    void configureLayout();
    void refreshPermanentTabCloseButton();
    [[nodiscard]] QString normalizedPath(const QString &absolutePath) const;

    QTabWidget *m_tabWidget = nullptr;
    QHash<QString, int> m_fileTabIndices;
    int m_aiChatTabIndex = -1;
};

} // namespace qtcode::ui
