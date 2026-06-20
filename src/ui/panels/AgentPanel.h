#pragma once

#include <QWidget>

namespace qtcode::ui {

class AgentPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit AgentPanel(QWidget *parent = nullptr);
};

} // namespace qtcode::ui
