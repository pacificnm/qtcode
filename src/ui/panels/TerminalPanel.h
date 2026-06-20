#pragma once

#include <QWidget>

class QPushButton;
class QTabWidget;
class QWidget;

namespace qtcode::core {
class ProjectManager;
} // namespace qtcode::core

namespace qtcode::terminal {
class TerminalManager;
class TerminalSession;
} // namespace qtcode::terminal

namespace qtcode::ui {

class TerminalPanel final : public QWidget
{
    Q_OBJECT

public:
    TerminalPanel(
        qtcode::terminal::TerminalManager *terminalManager,
        qtcode::core::ProjectManager *projectManager,
        QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void addTerminalTab();
    void focusCurrentTerminal();

private:
    void configureLayout();
    void restoreOrCreateInitialTabs();
    void addTerminalTabFromSession(const qtcode::terminal::TerminalSession &session, bool restored);
    void addTerminalTabForActiveProject();
    [[nodiscard]] QWidget *currentTerminalWidget() const;

    qtcode::terminal::TerminalManager *m_terminalManager = nullptr;
    qtcode::core::ProjectManager *m_projectManager = nullptr;
    QTabWidget *m_tabWidget = nullptr;
    QPushButton *m_newTerminalButton = nullptr;
};

} // namespace qtcode::ui
