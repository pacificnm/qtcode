#include "agents/AgentOptions.h"

namespace qtcode::agents {

namespace {

QList<AgentOption> cursorExecutionModes()
{
    return {
        {QStringLiteral("agent"), QStringLiteral("Agent")},
        {QStringLiteral("plan"), QStringLiteral("Plan")},
        {QStringLiteral("debug"), QStringLiteral("Debug")},
        {QStringLiteral("multitask"), QStringLiteral("Multitask")},
        {QStringLiteral("ask"), QStringLiteral("Ask")},
    };
}

QList<AgentOption> codexExecutionModes()
{
    return {
        {QStringLiteral("agent"), QStringLiteral("Agent")},
        {QStringLiteral("plan"), QStringLiteral("Plan")},
        {QStringLiteral("debug"), QStringLiteral("Debug")},
        {QStringLiteral("ask"), QStringLiteral("Ask")},
    };
}

QList<AgentOption> defaultExecutionModes()
{
    return {
        {QStringLiteral("agent"), QStringLiteral("Agent")},
    };
}

} // namespace

QList<AgentOption> executionModesForAgentKey(const QString &agentKey)
{
    if (agentKey == QStringLiteral("cursor")) {
        return cursorExecutionModes();
    }
    if (agentKey == QStringLiteral("codex")) {
        return codexExecutionModes();
    }

    return defaultExecutionModes();
}

} // namespace qtcode::agents
