#include "ui/views/ContextResultsView.h"

#include "core/ContextManager.h"

#include <KLocalizedString>

#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QVBoxLayout>

namespace qtcode::ui {

ContextResultsView::ContextResultsView(QWidget *parent)
    : QWidget(parent)
{
    configureLayout();
    clearResults();
}

void ContextResultsView::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_resultList = new QListWidget(this);
    m_resultList->setSelectionMode(QAbstractItemView::SingleSelection);

    m_detailView = new QTextEdit(this);
    m_detailView->setReadOnly(true);
    m_detailView->setPlaceholderText(i18n("Select a context result to inspect its excerpt."));
    m_detailView->setMaximumHeight(120);

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

    connect(m_resultList, &QListWidget::currentRowChanged, this, &ContextResultsView::onResultSelectionChanged);
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

    layout->addWidget(m_resultList);
    layout->addWidget(m_detailView, 1);
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
            m_detailView->setPlainText(
                i18n("Project memory is unavailable: %1", outcome.statusMessage));
        } else if (outcome.memorySearchAttempted) {
            m_detailView->setPlainText(i18n("No matching project memory found."));
        } else {
            m_detailView->clear();
        }
        setEnabled(!outcome.results.isEmpty());
        emit attachmentChanged();
        return;
    }

    setEnabled(true);
    for (int index = 0; index < m_results.size(); ++index) {
        auto *item = new QListWidgetItem(resultListLabel(m_results.at(index), true), m_resultList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }

    if (m_resultList->count() > 0) {
        m_resultList->setCurrentRow(0);
    }
    refreshDetailView();
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
        m_detailView->setPlainText(detailLines.join(QStringLiteral("\n")));
        setEnabled(true);
        emit attachmentChanged();
        return;
    }

    setEnabled(true);
    for (int index = 0; index < m_results.size(); ++index) {
        auto *item = new QListWidgetItem(resultListLabel(m_results.at(index), true), m_resultList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }

    if (m_resultList->count() > 0) {
        m_resultList->setCurrentRow(0);
    }
    refreshDetailView();
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
    if (m_detailView != nullptr) {
        m_detailView->clear();
    }
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

void ContextResultsView::onResultSelectionChanged()
{
    refreshDetailView();
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

void ContextResultsView::refreshDetailView()
{
    if (m_detailView == nullptr || m_resultList == nullptr) {
        return;
    }

    const int row = m_resultList->currentRow();
    if (row < 0 || row >= m_results.size()) {
        m_detailView->clear();
        return;
    }

    const memory::ContextResult &result = m_results.at(row);
    QStringList lines;
    lines.append(
        QStringLiteral("%1: %2")
            .arg(i18n("Source type"), memory::contextSourceTypeLabel(result.sourceType)));
    lines.append(QStringLiteral("%1: %2").arg(i18n("Title"), result.title));
    if (!result.sourceUri.isEmpty() && result.sourceUri != result.title) {
        lines.append(QStringLiteral("%1: %2").arg(i18n("Source URI"), result.sourceUri));
    }
    if (result.score > 0.0) {
        lines.append(QStringLiteral("%1: %2").arg(i18n("Score"), QString::number(result.score, 'f', 3)));
    }
    if (!result.retrievedAt.isEmpty()) {
        lines.append(QStringLiteral("%1: %2").arg(i18n("Retrieved at"), result.retrievedAt));
    }
    lines.append(QString());
    lines.append(result.excerpt);
    m_detailView->setPlainText(lines.join(QStringLiteral("\n")));
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
