#include "agents/AgentAdapter.h"

namespace qtcode::agents {

AgentAdapter::AgentAdapter(QObject *parent)
    : QObject(parent)
{
}

AgentAdapter::~AgentAdapter() = default;

QString AgentAdapter::executablePath() const
{
    return {};
}

QList<AgentOption> AgentAdapter::supportedModels() const
{
    return {};
}

bool AgentAdapter::refreshSupportedModels(QString *errorMessage)
{
    Q_UNUSED(errorMessage);
    return true;
}

} // namespace qtcode::agents
