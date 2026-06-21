#pragma once

#include "core/ContextManager.h"
#include "memory/ContextResult.h"
#include "storage/repositories/ContextRetrievalRepository.h"

#include <QList>
#include <QWidget>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace qtcode::core {
class ProjectManager;
} // namespace qtcode::core

namespace qtcode::ui {

class ContextResultsView final : public QWidget
{
    Q_OBJECT

public:
    explicit ContextResultsView(qtcode::core::ProjectManager *projectManager, QWidget *parent = nullptr);

    void setRetrievalOutcome(const qtcode::core::ContextRetrievalOutcome &outcome);
    void setPersistedRetrieval(
        const storage::PersistedContextRetrieval &retrieval,
        const QList<storage::PersistedContextResult> &results);
    void clearResults();

    [[nodiscard]] QList<memory::ContextResult> attachedResults() const;
    [[nodiscard]] QStringList attachedContextExcerpts() const;
    [[nodiscard]] bool addFileContext(
        const QString &absolutePath,
        const QString &contentOverride = {},
        QString *errorMessage = nullptr);

signals:
    void attachmentChanged();
    void fileOpenRequested(const QString &absolutePath);

private slots:
    void onResultItemClicked(QListWidgetItem *item);
    void attachAllResults();
    void detachAllResults();
    void attachSelectedResult();
    void detachSelectedResult();

private:
    void configureLayout();
    void setControlsEnabled(bool enabled);
    void setResultAttached(int row, bool attached);
    void setStatusMessage(const QString &message);
    void rebuildResultList();
    [[nodiscard]] QList<memory::ContextResult> manualFileResults() const;
    [[nodiscard]] QString projectRootPath() const;
    [[nodiscard]] QString resolveSourcePath(const memory::ContextResult &result) const;
    [[nodiscard]] static QString resultListLabel(const memory::ContextResult &result, bool attached);

    qtcode::core::ProjectManager *m_projectManager = nullptr;
    QList<memory::ContextResult> m_results;
    QList<bool> m_attachedStates;
    QListWidget *m_resultList = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_attachAllButton = nullptr;
    QPushButton *m_detachAllButton = nullptr;
    QPushButton *m_attachSelectedButton = nullptr;
    QPushButton *m_detachSelectedButton = nullptr;
    QString m_statusMessage;
    bool m_memoryUnavailable = false;
};

} // namespace qtcode::ui
