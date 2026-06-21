#pragma once

#include "core/StatusModels.h"

#include <QObject>
#include <QString>

class QTimer;

namespace qtcode::core {

class StatusService final : public QObject
{
    Q_OBJECT

public:
    explicit StatusService(QObject *parent = nullptr);

    void showMessage(const QString &text, StatusSeverity severity = StatusSeverity::Info);
    void showProgress(const QString &text);
    void clear();

signals:
    void messageChanged(const QString &text, qtcode::core::StatusSeverity severity);
    void cleared();

private:
    void scheduleAutoClear(int milliseconds);

    QTimer *m_autoClearTimer = nullptr;
};

} // namespace qtcode::core
