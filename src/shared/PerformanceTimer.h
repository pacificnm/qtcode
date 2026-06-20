#pragma once

#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QString>

namespace qtcode::shared {

class ScopedPerformanceTimer
{
public:
    ScopedPerformanceTimer(const QLoggingCategory &category, QString label);
    ~ScopedPerformanceTimer();

    ScopedPerformanceTimer(const ScopedPerformanceTimer &) = delete;
    ScopedPerformanceTimer &operator=(const ScopedPerformanceTimer &) = delete;

private:
    const QLoggingCategory &m_category;
    QString m_label;
    QElapsedTimer m_timer;
};

} // namespace qtcode::shared
