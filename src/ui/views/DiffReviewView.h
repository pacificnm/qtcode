#pragma once

#include "agents/AgentModels.h"

#include <QList>
#include <QString>
#include <QWidget>

class QListWidget;
class QPushButton;
class QTextEdit;

namespace qtcode::agents {
class AgentSession;
} // namespace qtcode::agents

namespace qtcode::ui {

class DiffReviewView final : public QWidget
{
    Q_OBJECT

public:
    explicit DiffReviewView(QWidget *parent = nullptr);

    void setSession(qtcode::agents::AgentSession *session);
    void clearSession();

signals:
    void approveRequested(const QString &artifactId);
    void rejectRequested(const QString &artifactId);

private slots:
    void onArtifactSelectionChanged();

private:
    void configureLayout();
    void refreshArtifacts();
    void showArtifact(const qtcode::agents::AgentArtifact *artifact);
    void setReviewButtonsEnabled(bool enabled);

    qtcode::agents::AgentSession *m_session = nullptr;
    QListWidget *m_artifactList = nullptr;
    QTextEdit *m_diffView = nullptr;
    QPushButton *m_approveButton = nullptr;
    QPushButton *m_rejectButton = nullptr;
    QString m_selectedArtifactId;
};

} // namespace qtcode::ui
