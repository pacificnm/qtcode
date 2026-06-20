#include "ui/views/GitHubDetailView.h"

#include <KLocalizedString>

#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace qtcode::ui {

GitHubDetailView::GitHubDetailView(QWidget *parent)
    : QWidget(parent)
{
    configureLayout();
    clearDetail();
}

void GitHubDetailView::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setWordWrap(true);

    m_metadataLabel = new QLabel(this);
    m_metadataLabel->setWordWrap(true);
    m_metadataLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_bodyView = new QTextEdit(this);
    m_bodyView->setReadOnly(true);
    m_bodyView->setMinimumHeight(120);

    m_attachButton = new QPushButton(i18n("Attach to agent session"), this);
    connect(m_attachButton, &QPushButton::clicked, this, &GitHubDetailView::onAttachClicked);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_metadataLabel);
    layout->addWidget(m_bodyView, 1);
    layout->addWidget(m_attachButton);
}

void GitHubDetailView::showIssue(const qtcode::github::GitHubIssueDetail &detail)
{
    m_mode = DetailMode::Issue;
    m_issueDetail = detail;
    m_pullRequestDetail = {};

    setDetailText(
        i18n("Issue #%1: %2", detail.number, detail.title),
        issueMetadata(detail),
        detail.body);
}

void GitHubDetailView::showPullRequest(const qtcode::github::GitHubPullRequestDetail &detail)
{
    m_mode = DetailMode::PullRequest;
    m_pullRequestDetail = detail;
    m_issueDetail = {};

    setDetailText(
        i18n("Pull request #%1: %2", detail.number, detail.title),
        pullRequestMetadata(detail),
        detail.body);
}

void GitHubDetailView::showLoadingMessage(const QString &message)
{
    m_mode = DetailMode::None;
    m_issueDetail = {};
    m_pullRequestDetail = {};
    m_titleLabel->setText(message);
    m_metadataLabel->clear();
    m_bodyView->clear();
    m_attachButton->setEnabled(false);
}

void GitHubDetailView::showErrorMessage(const QString &message)
{
    showLoadingMessage(message);
}

void GitHubDetailView::clearDetail()
{
    m_mode = DetailMode::None;
    m_issueDetail = {};
    m_pullRequestDetail = {};
    m_titleLabel->setText(i18n("Select a GitHub issue or pull request to view details."));
    m_metadataLabel->clear();
    m_bodyView->clear();
    m_attachButton->setEnabled(false);
}

void GitHubDetailView::onAttachClicked()
{
    switch (m_mode) {
    case DetailMode::Issue:
        emit attachIssueRequested(m_issueDetail);
        break;
    case DetailMode::PullRequest:
        emit attachPullRequestRequested(m_pullRequestDetail);
        break;
    case DetailMode::None:
        break;
    }
}

void GitHubDetailView::setDetailText(const QString &title, const QString &metadata, const QString &body)
{
    m_titleLabel->setText(title);
    m_metadataLabel->setText(metadata);
    m_bodyView->setPlainText(body.isEmpty() ? i18n("No description provided.") : body);
    m_attachButton->setEnabled(true);
}

QString GitHubDetailView::issueMetadata(const qtcode::github::GitHubIssueDetail &detail)
{
    QStringList parts;
    if (!detail.state.isEmpty()) {
        parts.append(i18n("State: %1", detail.state));
    }
    if (!detail.author.isEmpty()) {
        parts.append(i18n("Author: %1", detail.author));
    }
    if (!detail.updatedAt.isEmpty()) {
        parts.append(i18n("Updated: %1", detail.updatedAt));
    }
    if (!detail.url.isEmpty()) {
        parts.append(detail.url);
    }
    return parts.join(QStringLiteral("\n"));
}

QString GitHubDetailView::pullRequestMetadata(const qtcode::github::GitHubPullRequestDetail &detail)
{
    QStringList parts;
    if (!detail.state.isEmpty()) {
        parts.append(i18n("State: %1", detail.state));
    }
    if (!detail.author.isEmpty()) {
        parts.append(i18n("Author: %1", detail.author));
    }
    if (!detail.baseRef.isEmpty() || !detail.headRef.isEmpty()) {
        parts.append(i18n("Branches: %1 <- %2", detail.baseRef, detail.headRef));
    }
    if (!detail.updatedAt.isEmpty()) {
        parts.append(i18n("Updated: %1", detail.updatedAt));
    }
    if (!detail.url.isEmpty()) {
        parts.append(detail.url);
    }
    return parts.join(QStringLiteral("\n"));
}

} // namespace qtcode::ui
