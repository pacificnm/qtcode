#pragma once

#include <QWidget>

class QTermWidget;

namespace qtcode::ui {

class TerminalPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void configureLayout();
    void configureTerminal();
    [[nodiscard]] static QString defaultShellProgram();

    QTermWidget *m_terminal = nullptr;
};

} // namespace qtcode::ui
