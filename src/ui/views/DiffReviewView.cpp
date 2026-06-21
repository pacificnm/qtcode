#include "ui/views/DiffReviewView.h"

#include "agents/AgentSession.h"

#include <KLocalizedString>

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace qtcode::ui {

DiffReviewView::DiffReviewView(QWidget *parent)
    : QWidget(parent)
{
    configureLayout();
    clearSession();
}

void DiffReviewView::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_artifactList = new QListWidget(this);
    m_artifactList->setSelectionMode(QAbstractItemView::SingleSelection);

    m_diffView = new QTextEdit(this);
    m_diffView->setReadOnly(true);
    m_diffView->setPlaceholderText(i18n("Select a generated change to review its diff."));

    auto *buttonLayout = new QHBoxLayout();
    m_approveButton = new QPushButton(i18n("Approve"), this);
    m_rejectButton = new QPushButton(i18n("Reject"), this);
    buttonLayout->addWidget(m_approveButton);
    buttonLayout->addWidget(m_rejectButton);
    buttonLayout->addStretch();

    connect(m_artifactList, &QListWidget::currentRowChanged, this, [this]() {
        onArtifactSelectionChanged();
    });
    connect(m_approveButton, &QPushButton::clicked, this, [this]() {
        if (!m_selectedArtifactId.isEmpty()) {
            emit approveRequested(m_selectedArtifactId);
        }
    });
    connect(m_rejectButton, &QPushButton::clicked, this, [this]() {
        if (!m_selectedArtifactId.isEmpty()) {
            emit rejectRequested(m_selectedArtifactId);
        }
    });

    layout->addWidget(m_artifactList);
    layout->addWidget(m_diffView, 1);
    layout->addLayout(buttonLayout);
}

void DiffReviewView::setSession(qtcode::agents::AgentSession *session)
{
    m_session = session;
    refreshArtifacts();
}

void DiffReviewView::clearSession()
{
    m_session = nullptr;
    m_selectedArtifactId.clear();
    if (m_artifactList != nullptr) {
        m_artifactList->clear();
    }
    if (m_diffView != nullptr) {
        m_diffView->clear();
    }
    setReviewButtonsEnabled(false);
}

void DiffReviewView::refreshArtifacts()
{
    if (m_artifactList == nullptr) {
        return;
    }

    m_artifactList->clear();
    m_selectedArtifactId.clear();
    if (m_diffView != nullptr) {
        m_diffView->clear();
    }

    if (m_session == nullptr) {
        setReviewButtonsEnabled(false);
        return;
    }

    for (const qtcode::agents::AgentArtifact &artifact : m_session->artifacts()) {
        const QString stateLabel = qtcode::agents::artifactReviewStateLabel(artifact.reviewState);
        const QString title = artifact.title.trimmed().isEmpty() ? artifact.filePath : artifact.title;
        auto *item = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(title, stateLabel), m_artifactList);
        item->setData(Qt::UserRole, artifact.id);
    }

    if (m_artifactList->count() > 0) {
        m_artifactList->setCurrentRow(0);
    } else {
        setReviewButtonsEnabled(false);
    }
}

void DiffReviewView::onArtifactSelectionChanged()
{
    if (m_artifactList == nullptr || m_session == nullptr) {
        return;
    }

    QListWidgetItem *currentItem = m_artifactList->currentItem();
    if (currentItem == nullptr) {
        m_selectedArtifactId.clear();
        if (m_diffView != nullptr) {
            m_diffView->clear();
        }
        setReviewButtonsEnabled(false);
        return;
    }

    m_selectedArtifactId = currentItem->data(Qt::UserRole).toString();
    showArtifact(m_session->artifactById(m_selectedArtifactId));
}

void DiffReviewView::showArtifact(const qtcode::agents::AgentArtifact *artifact)
{
    if (m_diffView == nullptr) {
        return;
    }

    if (artifact == nullptr) {
        m_diffView->clear();
        setReviewButtonsEnabled(false);
        return;
    }

    QStringList lines;
    if (!artifact->filePath.trimmed().isEmpty()) {
        lines.append(QStringLiteral("File: %1").arg(artifact->filePath));
    }
    if (!artifact->kind.trimmed().isEmpty()) {
        lines.append(QStringLiteral("Kind: %1").arg(artifact->kind));
    }
    lines.append(QStringLiteral("Status: %1")
                     .arg(qtcode::agents::artifactReviewStateLabel(artifact->reviewState)));
    lines.append(QString());
    lines.append(artifact->content);

    m_diffView->setPlainText(lines.join(QStringLiteral("\n")));
    setReviewButtonsEnabled(artifact->reviewState == qtcode::agents::ArtifactReviewState::Pending);
}

void DiffReviewView::setReviewButtonsEnabled(bool enabled)
{
    if (m_approveButton != nullptr) {
        m_approveButton->setEnabled(enabled);
    }
    if (m_rejectButton != nullptr) {
        m_rejectButton->setEnabled(enabled);
    }
}

} // namespace qtcode::ui
