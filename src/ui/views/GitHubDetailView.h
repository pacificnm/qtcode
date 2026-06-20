#pragma once

#include "github/GitHubModels.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTextEdit;

namespace qtcode::ui {

class GitHubDetailView final : public QWidget
{
    Q_OBJECT

public:
    explicit GitHubDetailView(QWidget *parent = nullptr);

    void showIssue(const qtcode::github::GitHubIssueDetail &detail);
    void showPullRequest(const qtcode::github::GitHubPullRequestDetail &detail);
    void showLoadingMessage(const QString &message);
    void showErrorMessage(const QString &message);
    void clearDetail();

signals:
    void attachIssueRequested(const qtcode::github::GitHubIssueDetail &detail);
    void attachPullRequestRequested(const qtcode::github::GitHubPullRequestDetail &detail);

private slots:
    void onAttachClicked();

private:
    enum class DetailMode
    {
        None,
        Issue,
        PullRequest,
    };

    void configureLayout();
    void setDetailText(const QString &title, const QString &metadata, const QString &body);
    [[nodiscard]] static QString issueMetadata(const qtcode::github::GitHubIssueDetail &detail);
    [[nodiscard]] static QString pullRequestMetadata(const qtcode::github::GitHubPullRequestDetail &detail);

    DetailMode m_mode = DetailMode::None;
    qtcode::github::GitHubIssueDetail m_issueDetail;
    qtcode::github::GitHubPullRequestDetail m_pullRequestDetail;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_metadataLabel = nullptr;
    QTextEdit *m_bodyView = nullptr;
    QPushButton *m_attachButton = nullptr;
};

} // namespace qtcode::ui
