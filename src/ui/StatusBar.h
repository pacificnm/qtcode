#pragma once

#include <QWidget>

class QLabel;

namespace qtcode::core {
class StatusService;
enum class StatusSeverity;
} // namespace qtcode::core

namespace qtcode::ui {

class StatusBar final : public QWidget
{
    Q_OBJECT

public:
    explicit StatusBar(qtcode::core::StatusService *statusService, QWidget *parent = nullptr);

private slots:
    void onMessageChanged(const QString &text, qtcode::core::StatusSeverity severity);
    void onCleared();

private:
    void applySeverity(qtcode::core::StatusSeverity severity);

    qtcode::core::StatusService *m_statusService = nullptr;
    QLabel *m_messageLabel = nullptr;
};

} // namespace qtcode::ui
