#include "shared/PerformanceTimer.h"

#include <QElapsedTimer>

namespace qtcode::shared {

ScopedPerformanceTimer::ScopedPerformanceTimer(const QLoggingCategory &category, QString label)
    : m_category(category)
    , m_label(std::move(label))
{
    m_timer.start();
}

ScopedPerformanceTimer::~ScopedPerformanceTimer()
{
    qCInfo(m_category) << m_label << "completed in" << m_timer.elapsed() << "ms";
}

} // namespace qtcode::shared
