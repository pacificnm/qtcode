#include "core/StatusService.h"

#include <QTimer>

namespace qtcode::core {

namespace {

constexpr int kInfoAutoClearMs = 8000;
constexpr int kWarningAutoClearMs = 12000;

} // namespace

StatusService::StatusService(QObject *parent)
    : QObject(parent)
    , m_autoClearTimer(new QTimer(this))
{
    m_autoClearTimer->setSingleShot(true);
    connect(m_autoClearTimer, &QTimer::timeout, this, &StatusService::clear);
}

void StatusService::showMessage(const QString &text, StatusSeverity severity)
{
    m_autoClearTimer->stop();
    emit messageChanged(text, severity);

    switch (severity) {
    case StatusSeverity::Info:
        scheduleAutoClear(kInfoAutoClearMs);
        break;
    case StatusSeverity::Warning:
        scheduleAutoClear(kWarningAutoClearMs);
        break;
    case StatusSeverity::Error:
    case StatusSeverity::Progress:
        break;
    }
}

void StatusService::showProgress(const QString &text)
{
    showMessage(text, StatusSeverity::Progress);
}

void StatusService::clear()
{
    m_autoClearTimer->stop();
    emit cleared();
}

void StatusService::scheduleAutoClear(int milliseconds)
{
    if (milliseconds <= 0) {
        return;
    }

    m_autoClearTimer->start(milliseconds);
}

} // namespace qtcode::core
