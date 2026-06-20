#pragma once

#include <QWidget>

namespace qtcode::ui {

class RepositoryPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit RepositoryPanel(QWidget *parent = nullptr);
};

} // namespace qtcode::ui
