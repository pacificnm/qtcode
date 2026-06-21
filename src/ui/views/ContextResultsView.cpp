#include "ui/views/ContextResultsView.h"

#include "core/ContextManager.h"
#include "core/ProjectManager.h"
#include "settings/ProjectModels.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace qtcode::ui {

ContextResultsView::ContextResultsView(qtcode::core::ProjectManager *projectManager, QWidget *parent)
    : QWidget(parent)
    , m_projectManager(projectManager)
{
    configureLayout();
    clearResults();
}

void ContextResultsView::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setVisible(false);

    m_resultList = new QListWidget(this);
    m_resultList->setSelectionMode(QAbstractItemView::SingleSelection);

    m_attachAllButton = new QPushButton(i18n("Attach all"), this);
    m_detachAllButton = new QPushButton(i18n("Detach all"), this);
    m_attachSelectedButton = new QPushButton(i18n("Attach selected"), this);
    m_detachSelectedButton = new QPushButton(i18n("Detach selected"), this);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_attachAllButton);
    buttonLayout->addWidget(m_detachAllButton);
    buttonLayout->addWidget(m_attachSelectedButton);
    buttonLayout->addWidget(m_detachSelectedButton);
    buttonLayout->addStretch();

    connect(m_resultList, &QListWidget::itemClicked, this, &ContextResultsView::onResultItemClicked);
    connect(m_resultList, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (m_auditMode || item == nullptr || m_resultList == nullptr) {
            return;
        }

        const int row = m_resultList->row(item);
        if (row < 0 || row >= m_results.size()) {
            return;
        }

        const bool attached = item->checkState() == Qt::Checked;
        if (row >= m_attachedStates.size()) {
            m_attachedStates.resize(m_results.size(), attached);
        }
        m_attachedStates[row] = attached;
        item->setText(resultListLabel(m_results.at(row), attached));
        emit attachmentChanged();
    });
    connect(m_attachAllButton, &QPushButton::clicked, this, &ContextResultsView::attachAllResults);
    connect(m_detachAllButton, &QPushButton::clicked, this, &ContextResultsView::detachAllResults);
    connect(m_attachSelectedButton, &QPushButton::clicked, this, &ContextResultsView::attachSelectedResult);
    connect(m_detachSelectedButton, &QPushButton::clicked, this, &ContextResultsView::detachSelectedResult);

    layout->addWidget(m_statusLabel);
    layout->addWidget(m_resultList, 1);
    layout->addLayout(buttonLayout);
}

void ContextResultsView::setRetrievalOutcome(const qtcode::core::ContextRetrievalOutcome &outcome)
{
    m_auditMode = false;
    setControlsEnabled(true);
    m_statusMessage = outcome.statusMessage;
    m_memoryUnavailable = outcome.memoryUnavailable;
    m_results = outcome.results;
    m_attachedStates = QList<bool>(m_results.size(), true);

    const QSignalBlocker blocker(m_resultList);
    m_resultList->clear();

    if (m_results.isEmpty()) {
        if (outcome.memoryUnavailable) {
            setStatusMessage(i18n("Project memory is unavailable: %1", outcome.statusMessage));
        } else if (outcome.memorySearchAttempted) {
            setStatusMessage(i18n("No matching project memory found."));
        } else {
            setStatusMessage({});
        }
        setEnabled(!outcome.results.isEmpty());
        emit attachmentChanged();
        return;
    }

    setStatusMessage({});
    setEnabled(true);
    for (int index = 0; index < m_results.size(); ++index) {
        auto *item = new QListWidgetItem(resultListLabel(m_results.at(index), true), m_resultList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }

    if (m_resultList->count() > 0) {
        m_resultList->setCurrentRow(0);
    }
    emit attachmentChanged();
}

void ContextResultsView::setPersistedRetrieval(
    const storage::PersistedContextRetrieval &retrieval,
    const QList<storage::PersistedContextResult> &results)
{
    m_auditMode = true;
    setControlsEnabled(false);
    m_statusMessage = retrieval.metadataJson.value(QStringLiteral("status_message")).toString();
    m_memoryUnavailable = retrieval.metadataJson.value(QStringLiteral("memory_unavailable")).toBool();

    m_results.clear();
    m_attachedStates.clear();
    m_results.reserve(results.size());
    m_attachedStates.reserve(results.size());

    for (const storage::PersistedContextResult &persisted : results) {
        memory::ContextResult result;
        result.sourceType = memory::ContextSourceType::ProjectMemory;
        result.sourceUri = persisted.sourceUri;
        result.title = persisted.title;
        result.excerpt = persisted.excerpt;
        result.score = persisted.score;
        result.retrievedAt = persisted.metadataJson.value(QStringLiteral("retrieved_at")).toString();
        m_results.append(result);
        m_attachedStates.append(true);
    }

    const QSignalBlocker blocker(m_resultList);
    m_resultList->clear();

    if (m_results.isEmpty()) {
        QStringList detailLines;
        detailLines.append(i18n("Query: %1", retrieval.query));
        detailLines.append(i18n("Provider: %1", retrieval.providerKey));
        detailLines.append(i18n("Saved at: %1", retrieval.createdAt));
        setStatusMessage(detailLines.join(QStringLiteral("\n")));
        setEnabled(true);
        emit attachmentChanged();
        return;
    }

    setStatusMessage({});
    setEnabled(true);
    for (int index = 0; index < m_results.size(); ++index) {
        auto *item = new QListWidgetItem(resultListLabel(m_results.at(index), true), m_resultList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }

    if (m_resultList->count() > 0) {
        m_resultList->setCurrentRow(0);
    }
    emit attachmentChanged();
}

void ContextResultsView::clearResults()
{
    m_auditMode = false;
    setControlsEnabled(true);
    m_results.clear();
    m_attachedStates.clear();
    m_statusMessage.clear();
    m_memoryUnavailable = false;

    if (m_resultList != nullptr) {
        m_resultList->clear();
    }
    setStatusMessage({});
    setEnabled(false);
    emit attachmentChanged();
}

QList<memory::ContextResult> ContextResultsView::attachedResults() const
{
    QList<memory::ContextResult> attached;
    attached.reserve(m_results.size());

    for (int index = 0; index < m_results.size(); ++index) {
        if (index < m_attachedStates.size() && m_attachedStates.at(index)) {
            attached.append(m_results.at(index));
        }
    }

    return attached;
}

QStringList ContextResultsView::attachedContextExcerpts() const
{
    return qtcode::core::ContextManager::contextExcerptsFromResults(attachedResults());
}

void ContextResultsView::setControlsEnabled(bool enabled)
{
    if (m_attachAllButton != nullptr) {
        m_attachAllButton->setEnabled(enabled);
    }
    if (m_detachAllButton != nullptr) {
        m_detachAllButton->setEnabled(enabled);
    }
    if (m_attachSelectedButton != nullptr) {
        m_attachSelectedButton->setEnabled(enabled);
    }
    if (m_detachSelectedButton != nullptr) {
        m_detachSelectedButton->setEnabled(enabled);
    }
}

void ContextResultsView::onResultItemClicked(QListWidgetItem *item)
{
    if (item == nullptr || m_resultList == nullptr) {
        return;
    }

    const int row = m_resultList->row(item);
    if (row < 0 || row >= m_results.size()) {
        return;
    }

    const QString path = resolveSourcePath(m_results.at(row));
    if (!path.isEmpty()) {
        emit fileOpenRequested(path);
    }
}

void ContextResultsView::attachAllResults()
{
    for (int index = 0; index < m_resultList->count(); ++index) {
        setResultAttached(index, true);
    }
    emit attachmentChanged();
}

void ContextResultsView::detachAllResults()
{
    for (int index = 0; index < m_resultList->count(); ++index) {
        setResultAttached(index, false);
    }
    emit attachmentChanged();
}

void ContextResultsView::attachSelectedResult()
{
    const int row = m_resultList != nullptr ? m_resultList->currentRow() : -1;
    if (row < 0) {
        return;
    }
    setResultAttached(row, true);
    emit attachmentChanged();
}

void ContextResultsView::detachSelectedResult()
{
    const int row = m_resultList != nullptr ? m_resultList->currentRow() : -1;
    if (row < 0) {
        return;
    }
    setResultAttached(row, false);
    emit attachmentChanged();
}

void ContextResultsView::setResultAttached(int row, bool attached)
{
    if (m_resultList == nullptr || row < 0 || row >= m_resultList->count() || row >= m_results.size()) {
        return;
    }

    if (row >= m_attachedStates.size()) {
        m_attachedStates.resize(m_results.size(), attached);
    }

    m_attachedStates[row] = attached;
    if (QListWidgetItem *item = m_resultList->item(row)) {
        item->setCheckState(attached ? Qt::Checked : Qt::Unchecked);
        item->setText(resultListLabel(m_results.at(row), attached));
    }
}

QString ContextResultsView::resolveSourcePath(const memory::ContextResult &result) const
{
    const QString source = result.sourceUri.trimmed().isEmpty() ? result.title.trimmed() : result.sourceUri.trimmed();
    if (source.isEmpty()) {
        return {};
    }

    const QFileInfo directInfo(source);
    if (directInfo.isAbsolute() && directInfo.isFile()) {
        return directInfo.absoluteFilePath();
    }

    if (m_projectManager == nullptr) {
        return {};
    }

    qtcode::settings::ProjectRecord project;
    if (!m_projectManager->activeProject(&project) || project.rootPath.isEmpty()) {
        return {};
    }

    const QString candidatePath = QDir(project.rootPath).absoluteFilePath(source);
    const QFileInfo candidateInfo(candidatePath);
    if (candidateInfo.isFile()) {
        return candidateInfo.absoluteFilePath();
    }

    return {};
}

void ContextResultsView::setStatusMessage(const QString &message)
{
    if (m_statusLabel == nullptr) {
        return;
    }

    const QString trimmed = message.trimmed();
    m_statusLabel->setText(trimmed);
    m_statusLabel->setVisible(!trimmed.isEmpty());
}

QString ContextResultsView::resultListLabel(const memory::ContextResult &result, bool attached)
{
    const QString title = result.title.trimmed().isEmpty() ? result.sourceUri : result.title;
    QString label = QStringLiteral("[%1] %2")
                        .arg(memory::contextSourceTypeLabel(result.sourceType), title);
    if (result.score > 0.0) {
        label.append(QStringLiteral(" (%1)").arg(QString::number(result.score, 'f', 3)));
    }
    if (!attached) {
        label.prepend(QStringLiteral("(-) "));
    }
    return label;
}

} // namespace qtcode::ui
