#pragma once

#include <QWidget>

namespace qtcode::ui {

class TerminalPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(QWidget *parent = nullptr);
};

} // namespace qtcode::ui
