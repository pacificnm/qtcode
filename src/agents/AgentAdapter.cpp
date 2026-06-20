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

} // namespace qtcode::agents
