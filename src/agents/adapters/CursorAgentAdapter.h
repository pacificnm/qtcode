#pragma once

#include "agents/AgentAdapter.h"

#include <QJsonObject>
#include <QProcess>

namespace qtcode::agents {

class CursorAgentAdapter final : public AgentAdapter
{
    Q_OBJECT

public:
    explicit CursorAgentAdapter(QObject *parent = nullptr);
    ~CursorAgentAdapter() override;

    [[nodiscard]] QString agentKey() const override;
    [[nodiscard]] QString displayName() const override;
    [[nodiscard]] AgentCapabilities capabilities() const override;
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] QString executablePath() const override;

    [[nodiscard]] bool validateConfiguration(QString *errorMessage = nullptr) const override;
    [[nodiscard]] bool startRequest(
        const AgentRequest &request,
        QString *errorMessage = nullptr) override;
    void cancelRequest() override;

    void setExecutablePath(const QString &executablePath);

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessErrorOccurred(QProcess::ProcessError processError);

private:
    void emitNormalizedEvent(const QJsonObject &eventObject);
    void emitAssistantStreamText(const QString &text);
    void emitActivityText(const QString &text);
    void emitStatusText(const QString &text);
    void emitOutputText(const QString &text, bool startNewMessage = false);
    void emitAssistantMessageObject(const QJsonObject &messageObject);
    void emitToolCallEvent(const QJsonObject &eventObject);
    [[nodiscard]] static QString extractShellCommand(const QJsonObject &eventObject);
    void finishRequest(AgentRequestStatus status, const QString &errorMessage);
    [[nodiscard]] static AgentError errorFromProcess(
        QProcess::ProcessError processError,
        const QString &details);

    QString m_executablePath;
    QProcess *m_process = nullptr;
    bool m_requestInFlight = false;
    bool m_receivedAssistantOutput = false;
    QString m_pendingOutput;
    QString m_accumulatedAssistantText;
};

} // namespace qtcode::agents
