#pragma once

#include "agents/AgentModels.h"

namespace qtcode::agents {

[[nodiscard]] QList<AgentOption> executionModesForAgentKey(const QString &agentKey);

} // namespace qtcode::agents
